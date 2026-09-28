#!/usr/bin/env python3
"""T69: same-process shared-event handle imports reject invalid identities."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET

from metal_compute_inline_invalid import child, run


def main():
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-shared-event-handle-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't69.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        imports = [n for n in chunks if n.get('id') == '1034']
        assert len(imports) == 1
        source = int(child(imports[0], 'Source').text)
        imported = int(child(imports[0], 'Event').text)
        assert source and imported and source != imported
        cases = []

        def edit(tag, field, value):
            variant = copy.deepcopy(tree)
            node = next(n for n in variant.findall('./chunks/chunk') if n.get('id') == '1034')
            child(node, field).text = str(value)
            cases.append((tag, variant))

        for value in (0, 2**63):
            edit(f'event-{value}', 'Event', value)
            edit(f'source-{value}', 'Source', value)
        edit('duplicate-source-id', 'Event', source)
        edit('self-source', 'Source', imported)
        buffer_id = next(int(child(n, 'Buffer').text) for n in chunks if n.get('id') == '1004')
        edit('wrong-resource-type', 'Source', buffer_id)

        for tag, variant in cases:
            xml = directory / (tag + '.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', rdc, success=False)
            assert re.search(r'failed|invalid|unsupported|missing', message, re.I), (tag, message)
    print(f'T69: {len(cases)} malformed shared-event handle references rejected without crash')


if __name__ == '__main__':
    main()
