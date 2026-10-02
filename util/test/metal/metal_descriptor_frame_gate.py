#!/usr/bin/env python3
"""CPU preflight negatives for tiny frame-created placement descriptor inputs."""
import copy
import os
from pathlib import Path
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def main():
    if len(sys.argv) != 5:
        raise SystemExit('usage: frame_gate.py cli open_probe capture output_dir')
    cli, opener, capture, folder = map(Path, sys.argv[1:])
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
               RENDERDOC_METAL_TRACE_INITIAL_PRIVATE='1')

    def run(label, args, refuse=False):
        result = subprocess.run(list(map(str, args)), env=env, capture_output=True,
                                text=True, timeout=20)
        output = result.stdout + result.stderr
        (folder / f'{label}.log').write_text(output)
        if refuse:
            assert result.returncode in (1, 4), (label, result.returncode, output)
            assert 'failed' in output.lower() and 'descriptor' in output.lower(), (label, output)
            assert 'Metal replay wait begin' not in output, (label, output)
            assert 'Private initial contents upload' not in output, (label, output)
        else:
            assert result.returncode == 0, (label, result.returncode, output)

    def field(c, name):
        return next(n for n in c if n.get('name') == name)

    def first(chunks, name):
        return next(c for c in chunks if c.get('name') == name)

    run('positive-api', [opener, capture])
    run('positive-cli', [cli, 'replay', '--loops', '1', capture])
    xml = folder / 'source.zip.xml'
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs = {n: archive.read(n) for n in archive.namelist()}
    chunks = original.find('./chunks')
    creations = [c for c in chunks if c.get('name') == 'MTLHeap::newBuffer(offset)']
    assert len(creations) == 2
    scope = next(i for i, c in enumerate(chunks) if c.get('id') == '5')
    assert all(list(chunks).index(c) > scope for c in creations)
    ids = {field(c, 'Buffer').text for c in creations}
    identities = [c for c in chunks if c.get('name') == 'MTLResource::CaptureGPUIdentity'
                  and field(c, 'resource').text in ids]
    assert len(identities) == 2 and all(list(chunks).index(c) > scope for c in identities)
    layout = first(chunks, 'MTLBuffer::DeclareDescriptorTable')
    table = field(layout, 'buffer').text
    cases = ['old-coverage', 'query-before-creation', 'write-before-creation', 'wrong-heap',
             'late-invalid-pointer', 'unknown-identity', 'unaligned-offset', 'overlap',
             'wrong-identity-kind', 'oversized-buffer']
    for name in cases:
        tree, data = copy.deepcopy(original), dict(blobs)
        chunks = tree.find('./chunks')
        creates = [c for c in chunks if c.get('name') == 'MTLHeap::newBuffer(offset)']
        queries = [c for c in chunks if c.get('name') == 'MTLResource::CaptureGPUIdentity'
                   and field(c, 'resource').text in ids]
        updates = [c for c in chunks if c.get('name') == 'Internal_MTLBufferModifyCPUContents'
                   and field(c, 'Buffer').text == table]
        assert len(updates) == 2
        if name == 'old-coverage':
            field(first(chunks, 'MTLDevice::DeclareDescriptorCoverage'), 'version').text = '2'
        elif name == 'query-before-creation':
            chunks.remove(queries[0]); chunks.insert(list(chunks).index(creates[0]), queries[0])
        elif name == 'write-before-creation':
            chunks.remove(updates[0]); chunks.insert(list(chunks).index(creates[0]), updates[0])
        elif name == 'wrong-heap':
            field(creates[1], 'Heap').text = table
        elif name == 'late-invalid-pointer':
            field(updates[1], 'start').text = '8'; field(updates[1], 'size').text = '8'
            payload = field(updates[1], 'data'); payload.set('byteLength', '8')
            data[f'{int(payload.text):06d}'] = struct.pack('<Q', 0xfffffffffffffff0)
        elif name == 'unknown-identity':
            field(queries[1], 'resource').text = '999'
        elif name == 'unaligned-offset':
            field(creates[1], 'offset').text = '1'
        elif name == 'overlap':
            field(creates[1], 'offset').text = field(creates[0], 'offset').text
        elif name == 'wrong-identity-kind':
            field(queries[1], 'kind').text = '1'
        elif name == 'oversized-buffer':
            field(creates[1], 'length').text = str(128 * 1024 * 1024 + 1)
        else:
            raise AssertionError(name)
        for i, c in enumerate(chunks):
            c.set('chunkIndex', str(i))
        altered = folder / f'{name}.zip.xml'
        tree.write(altered, encoding='unicode', xml_declaration=True)
        with zipfile.ZipFile(altered.with_suffix(''), 'w', zipfile.ZIP_DEFLATED) as archive:
            for key, value in data.items():
                archive.writestr(key, value)
        rdc = folder / f'{name}.rdc'
        run(f'{name}-convert', [cli, 'convert', '-f', altered, '-o', rdc, '-c', 'rdc'])
        run(f'{name}-api', [opener, rdc], True)
        run(f'{name}-cli', [cli, 'replay', '--loops', '1', rdc], True)
    print(f'PASS frame placement descriptors: two frame creation/query pairs, API+CLI; {len(cases)} negatives before frame GPU submission')


if __name__ == '__main__':
    main()
