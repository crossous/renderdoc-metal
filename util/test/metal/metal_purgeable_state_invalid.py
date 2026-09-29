#!/usr/bin/env python3
"""T62: reject malformed resource identities and content-discarding purgeable states."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t62_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-purgeable-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't62.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        cases = []
        for chunk_id, field in (('1189', 'Buffer'), ('1070', 'Texture')):
            found = [n for n in tree.findall('./chunks/chunk') if n.get('id') == chunk_id]
            assert len(found) >= 2, (chunk_id, len(found))
            for value in (0, 2**63):
                variant = copy.deepcopy(tree)
                node = next(n for n in variant.findall('./chunks/chunk')
                            if n.get('id') == chunk_id)
                child(node, field).text = str(value)
                cases.append((f'{field}-{value}', variant))
            for value in (0, 3, 5):
                variant = copy.deepcopy(tree)
                node = next(n for n in variant.findall('./chunks/chunk')
                            if n.get('id') == chunk_id)
                child(node, 'State').text = str(value)
                cases.append((f'{field}-state-{value}', variant))
        for tag, variant in cases:
            xml = directory / (tag + '.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', rdc, success=False)
            assert re.search(r'failed|invalid|unsupported|missing', message, re.I), (tag, message)
    print(f'T62: {len(cases)} malformed purgeable states rejected without crash')


if __name__ == '__main__':
    main()
