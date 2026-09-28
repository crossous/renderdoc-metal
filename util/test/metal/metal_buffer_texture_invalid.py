#!/usr/bin/env python3
"""T59: reject malformed buffer-backed texture identities, shape and linear storage layout."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t59_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-buffer-texture-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't59.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        creation = [n for n in tree.findall('./chunks/chunk') if n.get('id') == '1193']
        updates = [n for n in tree.findall('./chunks/chunk') if n.get('id') == '1198']
        assert len(creation) == len(updates) == 1
        parent = child(creation[0], 'Buffer').text
        view = child(creation[0], 'Texture').text
        assert child(updates[0], 'Buffer').text == parent
        cases = []

        def edit(field, value):
            variant = copy.deepcopy(tree)
            node = next(n for n in variant.findall('./chunks/chunk') if n.get('id') == '1193')
            target = node
            for part in field.split('.'):
                target = child(target, part)
            target.text = str(value)
            cases.append((f'{field}-{value}', variant))

        for value in (0, 2**63, view):
            edit('Buffer', value)
        for value in (0, parent):
            edit('Texture', value)
        for field, values in (
                ('descriptor.textureType', (0, 3, 7)),
                ('descriptor.pixelFormat', (0, 2**64 - 1)),
                ('descriptor.width', (0, 5, 2**64 - 1)),
                ('descriptor.height', (0, 8, 2**64 - 1)),
                ('descriptor.depth', (0, 2)),
                ('descriptor.mipmapLevelCount', (0, 2)),
                ('descriptor.arrayLength', (0, 2)),
                ('descriptor.sampleCount', (0, 2)),
                ('descriptor.storageMode', (2, 3)),
                ('offset', (1, 33, 153, 2**64 - 1)),
                ('bytesPerRow', (0, 12, 17, 128, 2**64 - 1))):
            for value in values:
                edit(field, value)

        for tag, variant in cases:
            xml = directory / (tag + '.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', rdc, success=False)
            assert re.search(r'failed|invalid|unsupported|missing', message, re.I), (tag, message)
    print(f'T59: {len(cases)} malformed buffer-backed textures rejected without crash')


if __name__ == '__main__':
    main()
