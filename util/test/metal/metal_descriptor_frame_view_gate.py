#!/usr/bin/env python3
"""CPU preflight negatives for tiny frame-created buffer-backed texture descriptors."""
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
    views = [c for c in chunks if c.get('name') == 'MTLBuffer::newTextureWithDescriptor']
    assert len(views) == 2 and all(list(chunks).index(c) > scope for c in views)
    ids = {field(c, 'Texture').text for c in views}
    identities = [c for c in chunks if c.get('name') == 'MTLResource::CaptureGPUIdentity'
                  and field(c, 'resource').text in ids]
    assert len(identities) == 2 and all(list(chunks).index(c) > scope for c in identities)
    layout = first(chunks, 'MTLBuffer::DeclareDescriptorTable')
    table = field(layout, 'buffer').text
    cases = ['old-coverage', 'view-before-buffer', 'query-before-view', 'write-before-view',
             'unknown-parent', 'unaligned-view-offset', 'unaligned-view-row', 'view-range',
             'unsupported-format', 'wrong-storage', 'resource-id-collision', 'wrong-identity-kind',
             'late-invalid-texture-id']
    for name in cases:
        tree, data = copy.deepcopy(original), dict(blobs)
        chunks = tree.find('./chunks')
        creates = [c for c in chunks if c.get('name') == 'MTLHeap::newBuffer(offset)']
        views = [c for c in chunks if c.get('name') == 'MTLBuffer::newTextureWithDescriptor']
        queries = [c for c in chunks if c.get('name') == 'MTLResource::CaptureGPUIdentity'
                   and field(c, 'resource').text in ids]
        updates = [c for c in chunks if c.get('name') == 'Internal_MTLBufferModifyCPUContents'
                   and field(c, 'Buffer').text == table]
        assert len(updates) == 2
        if name == 'old-coverage':
            field(first(chunks, 'MTLDevice::DeclareDescriptorCoverage'), 'version').text = '2'
        elif name == 'view-before-buffer':
            chunks.remove(views[0]); chunks.insert(list(chunks).index(creates[0]), views[0])
        elif name == 'query-before-view':
            chunks.remove(queries[0]); chunks.insert(list(chunks).index(views[0]), queries[0])
        elif name == 'write-before-view':
            chunks.remove(updates[0]); chunks.insert(list(chunks).index(views[0]), updates[0])
        elif name == 'unknown-parent':
            field(views[1], 'Buffer').text = '999'
        elif name == 'unaligned-view-offset':
            field(views[1], 'offset').text = '1'
        elif name == 'unaligned-view-row':
            field(views[1], 'bytesPerRow').text = '1'
        elif name == 'view-range':
            field(views[1], 'offset').text = '512'
        elif name == 'unsupported-format':
            field(field(views[1], 'descriptor'), 'pixelFormat').text = '0'
        elif name == 'wrong-storage':
            field(field(views[1], 'descriptor'), 'storageMode').text = '0'
        elif name == 'resource-id-collision':
            field(views[1], 'Texture').text = field(creates[1], 'Buffer').text
        elif name == 'wrong-identity-kind':
            field(queries[1], 'kind').text = '0'
        elif name == 'late-invalid-texture-id':
            field(updates[1], 'start').text = '16'; field(updates[1], 'size').text = '8'
            payload = field(updates[1], 'data'); payload.set('byteLength', '8')
            data[f'{int(payload.text):06d}'] = struct.pack('<Q', 0xfffffffffffffff0)
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
    print(f'PASS frame buffer/view descriptors: two frame creation/query pairs, API+CLI; {len(cases)} negatives before frame GPU submission')


if __name__ == '__main__':
    main()
