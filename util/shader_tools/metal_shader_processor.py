#!/usr/bin/python3
# SPDX-License-Identifier: MIT
"""Bundled Metal processors. Every tool runs out of process in private scratch space."""
import argparse
import json
import os
import re
import signal
import subprocess
import tempfile
import time
from pathlib import Path

from metal_air_processor import extract_module

ROOT = Path(__file__).resolve().parent
DEADLINE = None


def run_tool(args, scratch, timeout=20, env=None):
    """Bound the entire process group by elapsed time and aggregate resident memory."""
    if DEADLINE is not None:
        timeout = min(timeout, DEADLINE - time.monotonic())
        if timeout <= 0:
            raise RuntimeError('Shader processing exceeded its 24 second total deadline')
    with tempfile.TemporaryFile() as log:
        proc = subprocess.Popen([str(a) for a in args], cwd=scratch, env=env,
                                stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        deadline = time.monotonic() + timeout
        failure = None
        peak = 0
        try:
            while proc.poll() is None:
                if time.monotonic() >= deadline:
                    failure = 'Tool exceeded {} second limit'.format(timeout)
                snapshot = subprocess.run(['/bin/ps', '-axo', 'pgid=,rss='], capture_output=True,
                                          text=True, timeout=2)
                rss = sum(int(row[1]) for line in snapshot.stdout.splitlines()
                          if len(row := line.split()) == 2 and int(row[0]) == proc.pid)
                peak = max(peak, rss)
                if rss > 500 * 1024:
                    failure = 'Tool exceeded 500 MiB resident memory limit'
                if failure:
                    os.killpg(proc.pid, signal.SIGKILL)
                    proc.wait()
                    break
                time.sleep(0.05)
        finally:
            if proc.poll() is None:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait()
        if log.tell() > 128 * 1024 * 1024:
            raise RuntimeError('Tool output exceeded 128 MiB limit')
        log.seek(0)
        output = log.read().decode('utf-8', errors='replace')
        print('{}: exit={} sampledPeakRSS={} KiB'.format(Path(args[0]).name, proc.returncode, peak),
              file=__import__('sys').stderr)
        if failure or proc.returncode:
            raise RuntimeError((failure or 'Tool failed') + '\n' + output)
        return output


def disassemble(path, entry, scratch):
    text = run_tool(['/usr/bin/xcrun', 'metal-objdump', '--metallib', '--disassemble',
                     path.resolve()], scratch)
    return extract_module(text, entry)


def compile_flags(text):
    flags = []
    lang = re.search(r'!air.language_version = !\{!(\d+)\}', text)
    if lang:
        version = re.search(r'^!' + lang[1] + r' = !\{!"Metal", i32 ([1-4]), i32 ([0-3]),', text, re.M)
        if version:
            flags.append('-std=metal{}.{}'.format(*version.groups()))
    air = re.search(r'!air.version = !\{!(\d+)\}', text)
    if air:
        version = re.search(r'^!' + air[1] + r' = !\{i32 (2), i32 ([0-9]),', text, re.M)
        target = re.search(r'-apple-macosx(\d+(?:\.\d+){0,2})', text)
        if version:
            flags += ['-target', 'air64_v{}{}-apple-macosx{}'.format(
                *version.groups(), target[1] if target else '15.0.0')]
    return flags


def compile_shader(mode, source, output, scratch):
    text = source.read_text()
    inp = scratch / ('edited.ll' if mode == 'compile-air' else 'edited.metal')
    inp.write_text(text)
    flags = compile_flags(text) if mode == 'compile-air' else ['-std=metal3.2']
    air, lib = scratch / 'edited.air', scratch / 'edited.metallib'
    run_tool(['/usr/bin/xcrun', 'metal', '-c', *flags, inp, '-o', air], scratch, timeout=120)
    run_tool(['/usr/bin/xcrun', 'metallib', air, '-o', lib], scratch, timeout=120)
    data = lib.read_bytes()
    if data[:4] != b'MTLB':
        raise RuntimeError('Apple compiler did not produce a Metal library')
    output.write_bytes(data)


def translate(args, scratch):
    air_text = disassemble(args.input, args.entry, scratch)
    ir, spv, meta = scratch / 'input.ll', scratch / 'output.spv', scratch / 'reflection.json'
    ir.write_text(air_text)
    env = {key: value for key, value in os.environ.items()
           if not key.startswith('METAL2VULKAN_')}
    # Only the bundled validator is used; no developer Homebrew or PATH dependency.
    env['METAL2VULKAN_SPIRV_VAL'] = str(ROOT / 'bin/spirv-val')
    env['METAL2VULKAN_REPRO_DIR'] = str(scratch / 'repro')
    run_tool([ROOT / 'bin/metal2vulkan', ir, spv, '--emit-meta', meta], scratch, env=env)
    reflection = json.loads(meta.read_text())
    if reflection.get('reflection_version') != 55:
        raise RuntimeError('Unsupported Metal translator reflection schema')
    stage = {'Vertex': 'vert', 'Fragment': 'frag', 'Kernel': 'comp'}[reflection['stage']]
    opts = {'msl': ['--msl', '--msl-version', '30200', '--msl-decoration-binding'],
            'hlsl': ['--hlsl', '--shader-model', '60'],
            'glsl': ['--vulkan-semantics', '--version', '450']}[args.mode]
    if args.editable:
        # Vulkan translation can synthesize a different resource ABI. Never silently apply it.
        if (reflection.get('function_constants') or reflection.get('bindings')
                or reflection.get('varyings') or reflection.get('implicit_imageblock_attachments')
                or reflection.get('fragment_imageblock') or stage != 'frag'):
            raise RuntimeError('MSL editing currently requires a fragment shader without resource '
                               'bindings or function constants. Use editable AIR for this shader; '
                               'the MSL preview uses a translated Vulkan ABI.')
    cross_meta = scratch / 'cross-reflection.json'
    run_tool([ROOT / 'bin/spirv-cross', spv, '--reflect', '--output', cross_meta], scratch)
    entries = json.loads(cross_meta.read_text())['entryPoints']
    if len(entries) != 1 or entries[0]['mode'] != stage:
        raise RuntimeError('Translated SPIR-V entry/stage does not match AIR metadata')
    source = scratch / 'output.txt'
    # SPIRV-Cross exposes a native rename API; generated MSL must retain the original entry.
    run_tool([ROOT / 'bin/spirv-cross', spv, *opts, '--rename-entry-point',
              entries[0]['name'], args.entry, stage, '--output', source], scratch)
    banner = ('// RenderDoc Metal: AIR -> Vulkan SPIR-V -> SPIRV-Cross.\n'
              '// This is reconstructed source, not captured debug source.\n')
    if not args.editable:
        banner += ('// VIEW ONLY: translated bindings, function constants and dispatch ABI may differ\n'
                   '// from the original Metal pipeline. Use AIR or captured MSL for editing.\n')
    args.output.write_text(banner + source.read_text())


def main():
    global DEADLINE
    # Complete before the common RenderDoc external-tool 30 second wait expires,
    # so our finally blocks can reap children and remove scratch files normally.
    DEADLINE = time.monotonic() + 24
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=['air', 'msl', 'hlsl', 'glsl', 'compile-msl', 'compile-air'])
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('entry')
    parser.add_argument('--renderdoc-bundled', action='store_true')
    parser.add_argument('--view-only', action='store_true')
    parser.add_argument('--editable', action='store_true')
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        raise ValueError('Input and output paths must differ')
    args.output.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='renderdoc-metal-processor-') as temp:
        scratch = Path(temp)
        if args.mode == 'air':
            args.output.write_text(disassemble(args.input, args.entry, scratch))
        elif args.mode.startswith('compile-'):
            compile_shader(args.mode, args.input, args.output, scratch)
        else:
            translate(args, scratch)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        __import__('sys').exit(str(error))
