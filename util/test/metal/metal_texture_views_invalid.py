#!/usr/bin/env python3
"""T58: reject malformed texture view parents, formats, ranges and swizzle before native use."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t58_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-texture-views-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't58.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        ids = (1076, 1077, 1078)
        nodes = [next(n for n in tree.findall('./chunks/chunk') if n.get('id') == str(cid))
                 for cid in ids]
        assert [child(n, 'Source').text for n in nodes] == ['16', '17', '17']
        cases = []

        def edit(tag, occurrence, field, value):
            variant = copy.deepcopy(tree)
            node = next(n for n in variant.findall('./chunks/chunk')
                        if n.get('id') == str(ids[occurrence]))
            target = node
            for part in field.split('.'):
                target = child(target, part)
            target.text = str(value)
            cases.append((tag, variant))

        for index in range(3):
            for value in (0, 2**63, child(nodes[0], 'View').text):
                # On the first call, the third identity is its own not-yet-created view.
                edit(f'source-{index}-{value}', index, 'Source', value)
            for value in (0, child(nodes[index], 'Source').text):
                edit(f'view-{index}-{value}', index, 'View', value)
            for value in (0, 80, 2**64 - 1):
                edit(f'format-{index}-{value}', index, 'format', value)
        for index in (1, 2):
            for field, values in (
                    ('type', (0, 99)),
                    ('levels.location', (3, 2**64 - 1)),
                    ('levels.length', (0, 3, 2**64 - 1)),
                    ('slices.location', (2, 2**64 - 1)),
                    ('slices.length', (0, 2, 2**64 - 1))):
                for value in values:
                    edit(f'{field}-{index}-{value}', index, field, value)
        for field in ('red', 'green', 'blue', 'alpha'):
            for value in (6, 255):
                edit(f'swizzle-{field}-{value}', 2, f'swizzle.{field}', value)

        for tag, variant in cases:
            xml = directory / (tag + '.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', rdc, success=False)
            assert re.search(r'failed|invalid|unsupported|missing', message, re.I), (tag, message)
    print(f'T58: {len(cases)} malformed texture views rejected without crash')


if __name__ == '__main__':
    main()
