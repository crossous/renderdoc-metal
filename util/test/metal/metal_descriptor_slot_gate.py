#!/usr/bin/env python3
"""Verify diagnostic facts survive CPU export and never enable GPU replay."""
import os
from pathlib import Path
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:])
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
               RENDERDOC_METAL_TRACE_INITIAL_PRIVATE='1')
    def run(label, arguments, expected):
        result = subprocess.run(list(map(str, arguments)), env=env,
                                capture_output=True, text=True, timeout=20)
        output = result.stdout + result.stderr
        (folder / (label + '.log')).write_text(output)
        assert result.returncode == expected, (label, output)
        return output
    xml = folder / 'source.zip.xml'
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'], 0)
    chunks = ET.parse(xml).find('./chunks')
    records = [c for c in chunks if c.get('name') == 'MTLBuffer::DescriptorSlotEvent']
    def field(chunk, name):
        return next(n for n in chunk if n.get('name') == name)
    assert len(records) == 6, len(records)
    assert [int(field(c, 'event').text) for c in records] == [0, 2, 1, 0, 2, 3]
    assert [int(field(c, 'generation').text) for c in records] == [1, 1, 1, 2, 2, 0]
    assert [int(field(c, 'descriptorType').text) for c in records] == [5, 5, 5, 4, 4, 0]
    assert {field(c, 'buffer').text for c in records} == {field(records[0], 'buffer').text}
    assert all(int(field(c, 'offset').text) == 24 for c in records)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        payloads = [archive.read(f'{int(field(c, "data").text):06d}') for c in records]
    assert payloads == [b'', struct.pack('<QQQ', *[i * 0x0101010101010101 for i in (16, 32, 48)]),
                        b'', b'', struct.pack('<QQQ', *[i * 0x0101010101010101 for i in (65, 82, 99)]),
                        struct.pack('<QQQ', 11, 22, 33)]
    scope = next(i for i, c in enumerate(chunks) if c.get('id') == '5')
    assert all(list(chunks).index(c) < scope for c in records[:2])
    assert all(list(chunks).index(c) > scope for c in records[2:])
    bindings = [c for c in chunks if c.get('name') == 'MTLBuffer::DescriptorSlotBinding']
    assert len(bindings) == 3
    assert [int(field(c, 'memberOffset').text) for c in bindings] == [4, 8, 12]
    assert all(int(field(c, 'kind').text) == 0 and int(field(c, 'offset').text) == 24 for c in bindings)
    assert field(bindings[0], 'resource').text != field(bindings[1], 'resource').text
    assert field(bindings[1], 'resource').text == field(bindings[2], 'resource').text
    queried = {field(c, 'resource').text for c in chunks if c.get('name') == 'MTLResource::CaptureGPUIdentity'}
    assert all(field(c, 'resource').text in queried for c in bindings)
    layouts = [c for c in chunks if c.get('name') == 'MTLCommandEncoder::DescriptorInlineLayout']
    assert [(int(field(c, 'stage').text), int(field(c, 'index').text),
             int(field(c, 'count').text), int(field(c, 'stride').text)) for c in layouts] == [
        (0, 2, 1, 8), (0, 2, 2, 8), (1, 6, 2, 16), (2, 2, 1, 8)]
    inline_bindings = [c for c in chunks if c.get('name') == 'MTLCommandEncoder::DescriptorInlineBinding']
    assert len(inline_bindings) == 6
    assert [int(field(c, 'memberOffset').text) for c in inline_bindings] == [4, 4, 8, 4, 8, 4]
    assert all(field(c, 'resource').text in queried for c in inline_bindings)
    vertex = next(c for c in chunks if c.get('name') == 'MTLRenderCommandEncoder::setVertexBytes')
    data = bytes(int(n.text) for n in field(vertex, 'data'))
    assert len(data) == 32
    assert struct.unpack_from('<II', data, 8) == (12, 4)
    assert struct.unpack_from('<II', data, 24) == (16, 8)
    next_capture = capture.with_name(capture.stem + '_2.rdc')
    next_xml = folder / 'next.zip.xml'
    run('next-export', [cli, 'convert', '-f', next_capture, '-o', next_xml, '-c', 'zip.xml'], 0)
    next_chunks = ET.parse(next_xml).find('./chunks')
    next_records = [c for c in next_chunks if c.get('name') == 'MTLBuffer::DescriptorSlotEvent']
    assert [int(field(c, 'event').text) for c in next_records] == [0, 2, 1, 0, 2, 3, 1, 0, 2]
    assert [int(field(c, 'generation').text) for c in next_records] == [1, 1, 1, 2, 2, 0, 2, 3, 3]
    next_scope = next(i for i, c in enumerate(next_chunks) if c.get('id') == '5')
    assert all(list(next_chunks).index(c) < next_scope for c in next_records)
    for label, args, expected in [('api', [opener, capture], 4),
                                 ('cli', [cli, 'replay', '--loops', '1', capture], 1)]:
        output = run(label, args, expected)
        assert 'descriptor slot diagnostics' in output, output
        assert 'Metal replay wait begin' not in output, output
        assert 'Private initial contents upload' not in output, output
    print('PASS six slot records, two generations, complete payloads, exact source bindings, four changing compute/vertex/fragment inline layouts and ordinary vertex metadata; API+CLI refused before GPU work')


if __name__ == '__main__':
    main()
