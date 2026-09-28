#!/usr/bin/env python3
"""T68: malformed same-process shared texture handle references fail cleanly."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t68_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-shared-texture-handle-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't68.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        assert sum(n.get('id') == '1079' for n in chunks) == 1
        assert sum(n.get('id') == '1012' for n in chunks) == 1
        cases = []

        def edit(tag, kind, field, value):
            variant = copy.deepcopy(tree)
            node = next(n for n in variant.findall('./chunks/chunk') if n.get('id') == kind)
            child(node, field).text = str(value)
            cases.append((tag, variant))

        for value in (0, 2**63):
            edit(f'export-source-{value}', '1079', 'Texture', value)
            edit(f'import-source-{value}', '1012', 'Source', value)
        edit('import-identity-zero', '1012', 'Texture', 0)
        edit('import-duplicate-source-identity', '1012', 'Texture', 16)
        edit('import-source-not-created-yet', '1012', 'Source', 17)

        for tag, variant in cases:
            xml = directory / (tag + '.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', rdc, success=False)
            assert re.search(r'failed|invalid|unsupported|missing', message, re.I), (tag, message)
    print(f'T68: {len(cases)} malformed shared texture handle references rejected without crash')


if __name__ == '__main__':
    main()
