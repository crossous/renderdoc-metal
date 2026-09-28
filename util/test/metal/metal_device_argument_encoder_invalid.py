#!/usr/bin/env python3
"""T60 device argument encoder: reject invalid identities and descriptor layouts."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_compute_inline_invalid import child, run


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t60_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-device-arg-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't60.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = [n for n in tree.findall('./chunks/chunk') if n.get('id') == '1028']
        assert len(chunks) == 1
        original_fields = list(child(chunks[0], 'descriptors'))
        assert [int(n.text) for n in original_fields] == [0, 58, 1, 0, 2, 0,
                                                           1, 59, 1, 0, 2, 0]
        cases = []

        def edit(tag, position, value):
            variant = copy.deepcopy(tree)
            node = next(n for n in variant.findall('./chunks/chunk') if n.get('id') == '1028')
            target = child(node, 'Encoder') if position == -1 else list(child(node, 'descriptors'))[position]
            target.text = str(value)
            cases.append((tag, variant))

        for value in (0, 17, 2**63):
            edit(f'encoder-{value}', -1, value)
        for position, values in (
                (0, (1, 32, 2**63)),
                (1, (0, 57, 2**63)),
                (2, (0, 2, 33, 2**63)),
                (3, (1, 2)),
                (4, (0, 7)),
                (5, (1, 16)),
                (6, (0, 32)),
                (7, (0, 2**63)),
                (8, (0, 32)),
                (9, (1, 2)),
                (11, (1, 16))):
            for value in values:
                edit(f'field-{position}-{value}', position, value)
        for tag, variant in cases:
            xml = directory / (tag + '.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', rdc, success=False)
            assert re.search(r'failed|invalid|unsupported|missing', message, re.I), (tag, message)
    print(f'T60: {len(cases)} malformed device argument encoders rejected without crash')


if __name__ == '__main__':
    main()
