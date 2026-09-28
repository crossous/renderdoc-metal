#!/usr/bin/env python3
"""T61: reject malformed no-copy buffer identity, payload and storage mode."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t61_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-no-copy-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't61.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        creations = [n for n in tree.findall('./chunks/chunk') if n.get('id') == '1006']
        assert len(creations) == 1
        assert child(creations[0], 'initialData').get('byteLength') == '4096'
        cases = []

        def edit(field, value):
            variant = copy.deepcopy(tree)
            node = next(n for n in variant.findall('./chunks/chunk') if n.get('id') == '1006')
            child(node, field).text = str(value)
            cases.append((f'{field}-{value}', variant))

        for value in (0, 2, 2**63):
            edit('Buffer', value)
        for value in (0, 1, 4095, 4097, 64*1024*1024+1, 2**63):
            edit('length', value)
        for value in (16, 32, 48, 2**63):
            edit('options', value)
        for tag, variant in cases:
            xml = directory / (tag + '.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', rdc, success=False)
            assert re.search(r'failed|invalid|unsupported|missing', message, re.I), (tag, message)
    print(f'T61: {len(cases)} malformed no-copy buffers rejected without crash')


if __name__ == '__main__':
    main()
