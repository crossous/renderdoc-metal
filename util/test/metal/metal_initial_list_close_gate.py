#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Finite canonical initial-list round trip and malformed payload close gate."""
import argparse
import copy
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', type=Path)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--oracle', type=Path)
    parser.add_argument('--mode', default='ue-indirect-tlas-private')
    args = parser.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    command = args.command.resolve(); capture = args.capture.resolve()
    library = command.parent.parent/'lib/librenderdoc.dylib'
    backend_hash = hashlib.sha256(library.read_bytes()).hexdigest()
    manifest = dict(status='RUNNING', backend_sha256=backend_hash, source=str(capture),
                    source_sha256=hashlib.sha256(capture.read_bytes()).hexdigest(), checks=[])
    env = os.environ.copy()
    for key in tuple(env):
        if key.startswith('RENDERDOC_METAL_') or key == 'DYLD_INSERT_LIBRARIES': env.pop(key)
    env['MTL_DEBUG_LAYER'] = '1'

    def run(tag, argv, rejected=False):
        log_path = out/(tag+'.log')
        current = env.copy(); current['RENDERDOC_DEBUG_LOG_FILE'] = str(log_path)
        with log_path.open('w') as log:
            # RenderDoc's logger may otherwise retire the redirected log on close.
            fcntl.flock(log.fileno(), fcntl.LOCK_SH)
            process = subprocess.Popen([str(x) for x in argv], env=current, stdout=log,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            try: code = process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL); process.wait(); code = 124
        text = log_path.read_text(errors='replace')
        if any(x in text for x in ('Unexpected Metal resource type 4', 'Assertion failed',
                                  'not handled in WrappedMTLDevice', 'm_ResourceMap.empty')):
            raise RuntimeError(tag+': shutdown or decode diagnostic: '+text[-1800:])
        # The outer loader can report APIReplayFailed or APIDataCorrupted.
        # Require the actual pre-allocation array boundary diagnostic instead.
        passed = code == 0 if not rejected else (0 < code < 124 and
            'Reading invalid array or byte buffer' in text and
            '18446744073709551615 larger than total stream size' in text and
            'Internal::List of Initial Contents Resources' in text)
        manifest['checks'].append(dict(name=tag, exit=code, passed=passed))
        (out/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
        if not passed: raise RuntimeError(f'{tag}: exit {code}: {text[-1800:]}')
        print('PASS '+tag, flush=True)

    def records(path):
        tree = ET.parse(path)
        chunks = [c for c in tree.findall('./chunks/chunk')
                  if c.get('name') == 'Internal::List of Initial Contents Resources']
        if len(chunks) != 1: raise RuntimeError('initial-list chunk count differs')
        entries = chunks[0].find('./array[@name="NeededInitials"]')
        if entries is None or not len(entries): raise RuntimeError('missing decoded initial-list entries')
        result = []
        for entry in entries:
            resource = entry.find('./ResourceId[@name="id"]')
            written = entry.find('./bool[@name="written"]')
            if resource is None or written is None: raise RuntimeError('incomplete WrittenRecord')
            result.append((int(resource.text), written.text))
        return tree, chunks[0], result

    try:
        original = out/'original.zip.xml'
        # Repeated CPU exports exercise dummy-device cleanup independently of native replay.
        previous = None
        for i in range(4):
            target = original if i == 0 else out/f'repeated-{i}.zip.xml'
            run(f'CPU-export-{i}', [command, 'convert', '-f', capture, '-o', target, '-c', 'zip.xml'])
            _, _, values = records(target)
            if previous is not None and values != previous: raise RuntimeError('CPU export changed initial-list metadata')
            previous = values
        roundtrip = out/'roundtrip.rdc'
        run('roundtrip-import', [command, 'convert', '-f', original, '-o', roundtrip, '-c', 'rdc'])
        exported = out/'roundtrip.zip.xml'
        run('roundtrip-export', [command, 'convert', '-f', roundtrip, '-o', exported, '-c', 'zip.xml'])
        if records(exported)[2] != previous: raise RuntimeError('round trip lost initial IDs or written flags')
        if args.oracle: run('roundtrip-API', [args.oracle.resolve(), roundtrip, args.mode])
        run('roundtrip-CLI', [command, 'replay', '--loops', '3', roundtrip])
        tree, chunk, _ = records(original)
        variant = copy.deepcopy(tree)
        damaged = next(c for c in variant.findall('./chunks/chunk')
                       if c.get('name') == chunk.get('name'))
        for child in list(damaged): damaged.remove(child)
        # Encode an invalid array count as an ordinary uint64 scalar. This tests
        # the reader without invoking the structured writer's zero-length path.
        damaged.set('length', '8')
        ET.SubElement(damaged, 'uint', name='NeededInitials', typename='uint64_t',
                      width='8').text = str(2**64-1)
        bad_xml = out/'oversized-list-count.zip.xml'
        variant.write(bad_xml, encoding='unicode', xml_declaration=True)
        shutil.copyfile(str(original)[:-4], str(bad_xml)[:-4])
        bad_capture = out/'oversized-list-count.rdc'
        run('oversized-count-import', [command, 'convert', '-f', bad_xml, '-o', bad_capture, '-c', 'rdc'])
        run('oversized-count-replay', [command, 'replay', '--loops', '1', bad_capture], rejected=True)
        if hashlib.sha256(library.read_bytes()).hexdigest() != backend_hash:
            raise RuntimeError('backend changed')
        manifest.update(status='PASS', CPU_exports=5, roundtrip_records=len(previous),
                        written_true=sum(w == 'true' for _, w in previous),
                        written_false=sum(w == 'false' for _, w in previous),
                        rejected=1, positive_controls=1,
                        malformed_capture=str(bad_capture))
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        manifest.update(status='FAIL', error=str(error)); print(error, flush=True); return 1
    finally: (out/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    return 0


if __name__ == '__main__':
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as gpu_lock:
        fcntl.flock(gpu_lock.fileno(), fcntl.LOCK_EX)
        raise SystemExit(main())
