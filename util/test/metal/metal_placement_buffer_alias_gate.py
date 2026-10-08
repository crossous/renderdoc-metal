#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Native/capture/replay physical placement-buffer aliases, plus pre-submit damaged captures."""
import argparse, copy, fcntl, hashlib, json, os, signal, subprocess, tempfile, zipfile
import xml.etree.ElementTree as ET
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--drop-owner', action='store_true', help='Release the app device reference while child proxies remain alive')
    args = parser.parse_args(); repo = Path(__file__).resolve().parents[3]
    build = repo / 'build-macos-debug'; work = args.work_dir.resolve(); work.mkdir(parents=True, exist_ok=True)
    backend = build / 'lib/librenderdoc.dylib'; sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    manifest = dict(status='RUNNING', scope='B544 placement API development checks, not final RT acceptance',
                    backend_sha256=sha(backend), dropped_device_owner=args.drop_owner, checks=[])
    def run(tag, cmd, expected=0, capture=False, pre_gpu=False):
        env = os.environ.copy()
        for key in tuple(env):
            if key.startswith('RENDERDOC_') or key == 'DYLD_INSERT_LIBRARIES': env.pop(key)
        env.update(MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
                   RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1', RENDERDOC_DEBUG_LOG_FILE=str(work / (tag + '-renderdoc.log')))
        if capture: env['DYLD_INSERT_LIBRARIES'] = str(backend)
        with (work / (tag + '.log')).open('w') as out, (work / (tag + '-renderdoc.log')).open('a') as keep:
            fcntl.flock(keep, fcntl.LOCK_SH)
            child = subprocess.Popen(list(map(str, cmd)), env=env, stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
            try: code = child.wait(timeout=45)
            except subprocess.TimeoutExpired: os.killpg(child.pid, signal.SIGKILL); child.wait(); code = 124
        text = (work / (tag + '.log')).read_text(errors='replace') + (work / (tag + '-renderdoc.log')).read_text(errors='replace')
        markers = ['Assertion failed', 'failed assertion', 'ForceCrash', 'OVERRUNNING CHUNK',
                   'Unexpected Metal resource type', 'm_ResourceMap.empty', 'm_ResourceRecords.empty']
        if pre_gpu: markers.append('Metal replay wait begin')
        hits = [v for v in markers if v in text]; passed = code == expected and not hits
        manifest['checks'].append(dict(tag=tag, exit=code, passed=passed, diagnostic_hits=hits))
        (work / 'manifest.json').write_text(json.dumps(manifest, indent=2)); print(tag, code, flush=True)
        if not passed: raise RuntimeError(tag + '\n' + text[-3000:])
    try:
        with (Path(tempfile.gettempdir()) / 'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            for name in ['UnrealEditor', 'qrenderdoc']:
                assert subprocess.run(['pgrep', '-x', name], stdout=subprocess.DEVNULL).returncode != 0
            run('native-build', ['clang++', '-std=c++17', '-fobjc-arc', '-I' + str(repo),
                                repo / 'util/test/metal/metal_placement_buffer_alias_capture.mm',
                                '-framework', 'Foundation', '-framework', 'Metal', '-o', work / 'native'])
            run('replay-build', ['clang++', '-std=c++17', '-DRENDERDOC_PLATFORM_APPLE', '-I' + str(repo),
                                repo / 'util/test/metal/metal_placement_buffer_alias_replay.mm',
                                '-L' + str(build / 'lib'), '-lrenderdoc', '-Wl,-rpath,' + str(build / 'lib'), '-o', work / 'replay'])
            for size in ([1048576] if args.drop_owner else [229376, 1048576]):
                folder = work / str(size); folder.mkdir(exist_ok=True)
                extra = ['drop-owner'] if args.drop_owner else []
                run(str(size) + '-native', [work / 'native', folder / 'native', size, *extra])
                run(str(size) + '-capture', [work / 'native', folder / 'capture', size, *extra], capture=True)
                captures = list(folder.glob('capture*.rdc')); assert len(captures) == 1
                cap = captures[0]
                run(str(size) + '-API', [work / 'replay', cap, size])
                run(str(size) + '-CLI', [build / 'bin/renderdoccmd', 'replay', '--loops', '3', cap])
                run(str(size) + '-export', [build / 'bin/renderdoccmd', 'convert', '-f', cap, '-o', folder / 'original.zip.xml', '-c', 'zip.xml'])
            if args.drop_owner:
                assert sha(backend) == manifest['backend_sha256']; manifest['status'] = 'PASS'
                return
            folder = work / '229376'; original = ET.parse(folder / 'original.zip.xml')
            with zipfile.ZipFile(folder / 'original.zip') as archive:
                blobs = {name: archive.read(name) for name in archive.namelist()}
            field = lambda c, name: c.find('./*[@name="' + name + '"]')
            for tag in ['frame-over-budget', 'old-over-budget', 'unaligned-offset', 'outside-heap', 'duplicate-birth',
                        'birth-after-write', 'untracked-frame', 'copy-outside']:
                tree = copy.deepcopy(original); chunks = tree.find('./chunks'); patches = {}
                alias = next(c for c in chunks if c.get('name') == 'MTLHeap::newBuffer(offset)' and field(c, 'offset').text == '131072')
                owner = field(alias, 'Buffer').text
                if tag == 'frame-over-budget': field(alias, 'length').text = str(1048576 + 256)
                elif tag == 'old-over-budget':
                    old = next(c for c in chunks if c.get('name') == 'MTLHeap::newBuffer(offset)' and field(c, 'offset').text == '0')
                    initial = next(c for c in chunks if c.get('name') == 'Internal::Initial Contents' and field(c, 'id').text == field(old, 'Buffer').text)
                    index = int(field(initial, 'Contents').text); patches[index] = blobs[f'{index:06}'] + bytes([0x11]) * 256
                    field(old, 'length').text = str(1048576 + 256); field(initial, 'Contents').set('byteLength', str(1048576 + 256))
                    initial.set('length', str(int(initial.get('length')) + 256))
                elif tag == 'unaligned-offset': field(alias, 'offset').text = '1'
                elif tag == 'outside-heap': field(alias, 'offset').text = str(2 * 1048576)
                elif tag == 'duplicate-birth': chunks.insert(list(chunks).index(alias) + 1, copy.deepcopy(alias))
                elif tag == 'birth-after-write':
                    writer = next(c for c in chunks if c.get('name') == 'MTLBlitCommandEncoder::copyFromBuffer' and field(c, 'destinationBuffer').text == owner)
                    chunks.remove(alias); chunks.insert(list(chunks).index(writer) + 1, alias)
                elif tag == 'untracked-frame': field(alias, 'options').text = '32'
                elif tag == 'copy-outside':
                    writer = next(c for c in chunks if c.get('name') == 'MTLBlitCommandEncoder::copyFromBuffer' and field(c, 'destinationBuffer').text == owner)
                    field(writer, 'size').text = '229377'
                for chunk in chunks: chunk.set('length', str(int(chunk.get('length', '0')) + 128))
                xml = work / (tag + '.zip.xml'); tree.write(xml, encoding='utf-8', xml_declaration=True)
                with zipfile.ZipFile(xml.with_suffix(''), 'w', zipfile.ZIP_DEFLATED) as archive:
                    for name, data in blobs.items(): archive.writestr(name, patches.get(int(name), data))
                cap = work / (tag + '.rdc')
                run(tag + '-import', [build / 'bin/renderdoccmd', 'convert', '-f', xml, '-o', cap, '-c', 'rdc'])
                run(tag + '-API', [build / 'metal-ray-b534/final-short/opener', cap], 4, pre_gpu=True)
                run(tag + '-CLI', [build / 'bin/renderdoccmd', 'replay', '--loops', '1', cap], 1, pre_gpu=True)
            assert sha(backend) == manifest['backend_sha256']; manifest['status'] = 'PASS'
    except Exception as ex: manifest.update(status='FAIL', error=repr(ex)); raise
    finally: (work / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__': main()
