#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Native six/eight color attachments, deferred stores, replay and malformed state."""
import argparse
import copy
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--fragmentless', action='store_true', help='Vertex-only pipelines with color/depth and unused fragment bindings')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    build = repo / 'build-macos-debug'
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=False)
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    backend = build / 'lib/librenderdoc.dylib'
    result = dict(status='RUNNING', backend_sha256=sha(backend), checks=[], fragmentless=args.fragmentless,
                  scope='Fresh MRT API development validation; not UE or final RT acceptance')
    environment = os.environ.copy()
    for key in tuple(environment):
        if key.startswith('RENDERDOC_') or key == 'DYLD_INSERT_LIBRARIES':
            environment.pop(key)
    environment.update(MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
                       RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')

    def run(tag, command, extra=None, expected=0, pre_gpu=False):
        env = dict(environment, **(extra or {}),
                   RENDERDOC_DEBUG_LOG_FILE=str(work / (tag + '-renderdoc.log')))
        with (work / (tag + '-renderdoc.log')).open('a') as keep, \
                (work / (tag + '.log')).open('w') as output:
            fcntl.flock(keep, fcntl.LOCK_SH)
            child = subprocess.Popen(list(map(str, command)), env=env, stdout=output,
                                     stderr=subprocess.STDOUT, start_new_session=True)
            try:
                code = child.wait(timeout=60)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
                code = 124
        debug = work / (tag + '-renderdoc.log')
        text = (work / (tag + '.log')).read_text(errors='replace') + \
            (debug.read_text(errors='replace') if debug.exists() else '')
        markers = ['Assertion failed', 'failed assertion', 'ForceCrash', 'OVERRUNNING CHUNK',
                   'm_ResourceMap.empty', 'm_ResourceRecords.empty']
        if pre_gpu:
            markers.append('Metal replay wait begin')
        hits = [marker for marker in markers if marker in text]
        result['checks'].append(dict(tag=tag, exit=code, expected=expected, hits=hits))
        print(tag, code, flush=True)
        assert code == expected and not hits, text[-3500:]
        return text

    field = lambda node, name: next(child for child in node if child.get('name') == name)
    try:
        with (Path(tempfile.gettempdir()) / 'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            for app in ('UnrealEditor', 'qrenderdoc'):
                assert subprocess.run(['pgrep', '-x', app], stdout=subprocess.DEVNULL).returncode != 0
            common = ['clang++', '-std=c++17', '-fobjc-arc', '-I' + str(repo)]
            run('native-build', common + [repo / 'util/test/metal/metal_descriptor_mrt_capture.mm',
                '-framework', 'Foundation', '-framework', 'Metal', '-framework', 'QuartzCore',
                '-o', work / 'native'])
            run('replay-build', common + ['-DRENDERDOC_PLATFORM_APPLE',
                repo / 'util/test/metal/metal_descriptor_mrt_replay.mm', '-L' + str(build / 'lib'),
                '-lrenderdoc', '-Wl,-rpath,' + str(build / 'lib'), '-framework', 'Foundation',
                '-framework', 'Metal', '-o', work / 'replay'])
            cases = [(2, 'd32'), (2, 'd32-deferred'), (2, 'd32s8-parallel')] if args.fragmentless else \
                [(6, 'direct'), (6, 'deferred'), (8, 'deferred'), (8, 'parallel')]
            for count, mode in cases:
                tag = str(count) + '-' + mode
                extra = dict(RENDERDOC_METAL_MRT_TARGETS=str(count))
                if args.fragmentless:
                    extra.update(RENDERDOC_METAL_MRT_FRAGMENTLESS_COLOR='1',
                                 RENDERDOC_METAL_MRT_DEPTH_ONLY='1',
                                 RENDERDOC_METAL_MRT_DEPTH_FORMAT='d32s8' if 'd32s8' in mode else 'd32')
                if 'deferred' in mode:
                    extra['RENDERDOC_METAL_MRT_DEFERRED_STORE'] = '1'
                if 'parallel' in mode:
                    extra['RENDERDOC_METAL_PARALLEL_MRT'] = '1'
                run(tag + '-native', [work / 'native'], extra)
                text = run(tag + '-capture', [work / 'native'], dict(extra,
                    DYLD_INSERT_LIBRARIES=str(backend),
                    RENDERDOC_METAL_CAPTURE_PATH=str(work / tag)))
                facts = dict(re.findall(r'(VA_A|VA_B|TEX|SAMP)=([0-9]+)', text))
                for second in (False, True):
                    capture = work / (tag + '_capture' + ('_2' if second else '') + '.rdc')
                    name = tag + ('-2' if second else '-1')
                    output = run(name + '-API', [work / 'replay', capture, facts['VA_A'],
                        facts['VA_B'], facts['TEX'], facts['SAMP'],
                        '186' if second else '122', '122' if second else '186'], extra)
                    assert 'targets=' + str(count) in output
                    run(name + '-CLI', [build / 'bin/renderdoccmd', 'replay', '--loops', '3', capture], extra)
                if (mode != 'd32-deferred' if args.fragmentless else count != 8 or mode != 'deferred'):
                    continue
                xml = work / 'source.zip.xml'
                run('export', [build / 'bin/renderdoccmd', 'convert', '-f',
                    work / (tag + '_capture.rdc'), '-o', xml, '-c', 'zip.xml'])
                original = ET.parse(xml)
                with zipfile.ZipFile(xml.with_suffix('')) as archive:
                    blobs = {name: archive.read(name) for name in archive.namelist()}
                damages = ('missing-fragmentless-colour', 'fragmentless-colour-as-depth',
                           'missing-fragmentless-store', 'unused-fragment-buffer-range') if args.fragmentless else \
                    ('missing-eighth', 'duplicate-eighth', 'eighth-format', 'missing-eighth-store', 'store-index-eight')
                for damage in damages:
                    tree = copy.deepcopy(original)
                    chunks = tree.find('./chunks')
                    render_pass = next(chunk for chunk in chunks if chunk.get('name') ==
                                       'MTLCommandBuffer::renderCommandEncoderWithDescriptor')
                    colors = list(field(field(render_pass, 'descriptor'), 'colorAttachments'))
                    stores = [chunk for chunk in chunks if chunk.get('name') ==
                              'MTLRenderCommandEncoder::setColorStoreAction' and
                              field(chunk, 'colorAttachmentIndex').text == ('0' if args.fragmentless else '7')]
                    assert stores
                    if damage == 'missing-fragmentless-colour':
                        field(colors[0], 'texture').text = '0'
                    elif damage == 'fragmentless-colour-as-depth':
                        field(colors[0], 'texture').text = field(field(field(render_pass, 'descriptor'), 'depthAttachment'), 'texture').text
                    elif damage == 'missing-fragmentless-store':
                        chunks.remove(stores[0])
                    elif damage == 'unused-fragment-buffer-range':
                        binding = next(chunk for chunk in chunks if chunk.get('name') ==
                                       'MTLRenderCommandEncoder::setFragmentBuffer' and field(chunk, 'index').text == '2')
                        field(binding, 'offset').text = '65536'
                    elif damage == 'missing-eighth':
                        field(colors[7], 'texture').text = '0'
                    elif damage == 'duplicate-eighth':
                        field(colors[7], 'texture').text = field(colors[6], 'texture').text
                    elif damage == 'eighth-format':
                        # BGRA first attachment cannot satisfy the RGBA pipeline slot.
                        first = field(colors[0], 'texture').text
                        field(colors[0], 'texture').text = field(colors[7], 'texture').text
                        field(colors[7], 'texture').text = first
                    elif damage == 'missing-eighth-store':
                        chunks.remove(stores[0])
                    else:
                        field(stores[0], 'colorAttachmentIndex').text = '8'
                    for chunk in chunks:
                        chunk.set('length', str(int(chunk.get('length', '0')) + 128))
                    damaged = work / (damage + '.zip.xml')
                    tree.write(damaged, encoding='utf-8', xml_declaration=True)
                    with zipfile.ZipFile(damaged.with_suffix(''), 'w', zipfile.ZIP_DEFLATED) as archive:
                        for name, data in blobs.items():
                            archive.writestr(name, data)
                    capture = work / (damage + '.rdc')
                    run(damage + '-import', [build / 'bin/renderdoccmd', 'convert', '-f',
                        damaged, '-o', capture, '-c', 'rdc'])
                    run(damage + '-API', [build / 'metal-ray-b534/final-short/opener', capture],
                        expected=4, pre_gpu=True)
                    run(damage + '-CLI', [build / 'bin/renderdoccmd', 'replay', '--loops', '1',
                        capture], expected=1, pre_gpu=True)
            assert sha(backend) == result['backend_sha256']
            result.update(status='PASS', fresh_captures=len(cases)*2, damaged_groups=len(damages),
                          production_RT_enabled=False)
    except Exception as error:
        result.update(status='FAIL', error=repr(error))
        raise
    finally:
        (work / 'manifest.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
