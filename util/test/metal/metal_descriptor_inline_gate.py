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
    declarations = [c for c in chunks if c.get('name') == 'MTLComputeCommandEncoder::DeclareDescriptorBytes']
    assert len(declarations) == 2
    cases = ['no-declaration', 'bytes-before-declaration', 'invalid-index', 'invalid-layout',
             'unknown-encoder', 'late-invalid-inline-va', 'inline-after-end', 'duplicate-declaration']
    for name in cases:
        tree, data = copy.deepcopy(original), dict(blobs)
        chunks = tree.find('./chunks')
        declarations = [c for c in chunks if c.get('name') == 'MTLComputeCommandEncoder::DeclareDescriptorBytes']
        setters = [c for c in chunks if c.get('name') == 'MTLComputeCommandEncoder::setBytes']
        assert len(setters) == 2
        if name == 'no-declaration':
            chunks.remove(declarations[1])
        elif name == 'bytes-before-declaration':
            chunks.remove(setters[1]); chunks.insert(list(chunks).index(declarations[1]), setters[1])
        elif name == 'invalid-index':
            field(declarations[1], 'index').text = '31'
        elif name == 'invalid-layout':
            field(declarations[1], 'count').text = '2'
        elif name == 'unknown-encoder':
            field(declarations[1], 'encoder').text = '999'
        elif name == 'late-invalid-inline-va':
            values = list(field(setters[1], 'data'))
            assert len(values) == 24
            for v in values[8:16]: v.text = '255'
        elif name == 'inline-after-end':
            encoder = field(setters[1], 'ComputeCommandEncoder').text
            end = next(c for c in chunks if c.get('name') == 'MTLComputeCommandEncoder::endEncoding'
                       and field(c, 'ComputeCommandEncoder').text == encoder)
            chunks.remove(setters[1]); chunks.insert(list(chunks).index(end) + 1, setters[1])
        elif name == 'duplicate-declaration':
            chunks.insert(list(chunks).index(declarations[1]), copy.deepcopy(declarations[1]))
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
    print(f'PASS inline descriptor bytes: two typed setBytes pairs, API+CLI; {len(cases)} negatives before frame GPU submission')


if __name__ == '__main__':
    main()
