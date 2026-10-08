#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Derive an AS-build/Header-only control; reject damaged headers and unqualified consumers."""
import argparse, copy, fcntl, hashlib, json, os, signal, struct, subprocess, tempfile
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    build = repo / 'build-macos-debug'
    work = args.work_dir.resolve(); work.mkdir(parents=True, exist_ok=True)
    original = ET.parse(args.source)
    with zipfile.ZipFile(args.source.with_suffix('')) as archive:
        blobs = {name: archive.read(name) for name in archive.namelist()}
    field = lambda c, name: c.find('./*[@name="' + name + '"]')
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    manifest = dict(status='RUNNING', scope='Derived Header-only replay, not fresh native capture or final acceptance',
                    backend_sha256=sha(build / 'lib/librenderdoc.dylib'), checks=[])
    def run(tag, command, expected, pre_gpu=False):
        env = os.environ.copy()
        for key in tuple(env):
            if key.startswith('RENDERDOC_') or key == 'DYLD_INSERT_LIBRARIES': env.pop(key)
        env.update(MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
                   RENDERDOC_DEBUG_LOG_FILE=str(work / (tag + '-renderdoc.log')))
        with (work / (tag + '.log')).open('w') as out, (work / (tag + '-renderdoc.log')).open('a') as keep:
            fcntl.flock(keep, fcntl.LOCK_SH)
            child = subprocess.Popen(list(map(str, command)), env=env, stdout=out,
                                     stderr=subprocess.STDOUT, start_new_session=True)
            try: code = child.wait(timeout=45)
            except subprocess.TimeoutExpired: os.killpg(child.pid, signal.SIGKILL); child.wait(); code = 124
        text = (work / (tag + '.log')).read_text(errors='replace') + (work / (tag + '-renderdoc.log')).read_text(errors='replace')
        markers = ['Assertion failed', 'failed assertion', 'ForceCrash', 'OVERRUNNING CHUNK',
                   'Unexpected Metal resource type', 'm_ResourceMap.empty', 'm_ResourceRecords.empty']
        if pre_gpu: markers.append('Metal replay wait begin')
        hits = [marker for marker in markers if marker in text]
        passed = code == expected and not hits
        manifest['checks'].append(dict(tag=tag, exit=code, passed=passed, diagnostic_hits=hits))
        (work / 'manifest.json').write_text(json.dumps(manifest, indent=2))
        print(tag, code, flush=True)
        if not passed: raise RuntimeError(tag + '\n' + text[-2500:])
    declarations = {'MTLComputePipelineState::DeclareRayQueryHeapDispatch', 'MTLComputePipelineState::DeclareRayQueryHeapCBVRoot',
                    'MTLComputePipelineState::DeclareIRComputeRoot', 'MTLComputePipelineState::DeclareIRComputeHeapEntry'}
    try:
        with (Path(tempfile.gettempdir()) / 'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            for name in ['UnrealEditor', 'qrenderdoc']:
                assert subprocess.run(['pgrep', '-x', name], stdout=subprocess.DEVNULL).returncode != 0
            run('helper-build', ['clang++', '-std=c++17', '-fobjc-arc', '-DRENDERDOC_PLATFORM_APPLE',
                                '-I' + str(repo), repo / 'util/test/metal/metal_ray_header_only_replay.mm',
                                '-framework', 'Foundation', '-framework', 'Metal', '-L' + str(build / 'lib'),
                                '-lrenderdoc', '-Wl,-rpath,' + str(build / 'lib'), '-o', work / 'replay'], 0)
            cases = ['legal-header-only', 'legal-large-contribution', 'reserved-word', 'AS-identity', 'contribution-identity', 'header-offset',
                     'header-length', 'header-private', 'contribution-offset', 'source-alias', 'birth-after-write',
                     'unqualified-consumer']
            for tag in cases:
                tree = copy.deepcopy(original); chunks = tree.find('./chunks')
                encoders = {field(c, 'ComputeCommandEncoder').text for c in chunks
                            if c.get('name', '').startswith('MTLCommandBuffer::computeCommandEncoder')}
                for chunk in list(chunks):
                    if chunk.get('name') in declarations or (tag != 'unqualified-consumer' and
                       any(value.text in encoders for value in chunk.iter('ResourceId'))):
                        chunks.remove(chunk)
                    elif chunk.get('name') == 'MTLDevice::CaptureComputeIndirectArgumentsCount' and tag != 'unqualified-consumer':
                        field(chunk, 'count').text = '0'
                header = next(c for c in chunks if c.get('name') == 'MTLBuffer::DeclareRayASHeader')
                birth = next(c for c in chunks if c.get('name') == 'MTLDevice::newBufferWithLength' and
                             field(c, 'Buffer').text == field(header, 'buffer').text)
                patches = {}
                if tag == 'legal-large-contribution':
                    owner = field(header, 'contributions').text
                    creation = next(c for c in chunks if c.get('name') == 'MTLDevice::newBufferWithLength' and
                                    field(c, 'Buffer').text == owner)
                    initial = next(c for c in chunks if c.get('name') == 'Internal::Initial Contents' and
                                   field(c, 'id').text == owner)
                    index = int(field(initial, 'Contents').text); raw = blobs[f'{index:06}']
                    patches[index] = raw + bytes([0xcd]) * (65536 - len(raw))
                    field(creation, 'length').text = '65536'; field(initial, 'Contents').set('byteLength', '65536')
                    initial.set('length', str(int(initial.get('length')) + 65536))
                elif tag in ('reserved-word', 'AS-identity', 'contribution-identity'):
                    index = int(field(header, 'bytes').text); data = bytearray(blobs[f'{index:06}'])
                    struct.pack_into('<Q', data, {'reserved-word':16, 'AS-identity':0, 'contribution-identity':8}[tag], 1)
                    patches[index] = bytes(data)
                elif tag == 'header-offset': field(header, 'offset').text = '8'
                elif tag == 'header-length': field(birth, 'length').text = '128'
                elif tag == 'header-private': field(birth, 'options').text = '32'
                elif tag == 'contribution-offset': field(header, 'contributionOffset').text = '4'
                elif tag == 'source-alias': field(header, 'contributions').text = field(header, 'buffer').text
                elif tag == 'birth-after-write':
                    chunks.remove(birth); chunks.insert(list(chunks).index(header) + 1, birth)
                for chunk in chunks: chunk.set('length', str(int(chunk.get('length', '0')) + 128))
                xml = work / (tag + '.zip.xml'); tree.write(xml, encoding='utf-8', xml_declaration=True)
                with zipfile.ZipFile(xml.with_suffix(''), 'w', zipfile.ZIP_DEFLATED) as archive:
                    for name, data in blobs.items(): archive.writestr(name, patches.get(int(name), data))
                capture = work / (tag + '.rdc')
                run(tag + '-import', [build / 'bin/renderdoccmd', 'convert', '-f', xml, '-o', capture, '-c', 'rdc'], 0)
                legal = tag.startswith('legal-')
                run(tag + '-API', [work / 'replay' if legal else build / 'metal-ray-b534/final-short/opener', capture],
                    0 if legal else 4, not legal)
                run(tag + '-CLI', [build / 'bin/renderdoccmd', 'replay', '--loops', '3' if legal else '1', capture],
                    0 if legal else 1, not legal)
            assert sha(build / 'lib/librenderdoc.dylib') == manifest['backend_sha256']
            manifest['status'] = 'PASS'
    except Exception as ex:
        manifest.update(status='FAIL', error=repr(ex)); raise
    finally: (work / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__': main()
