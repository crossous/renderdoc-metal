#!/usr/bin/env python3
"""T64: reject malformed descriptor-backed shared textures before native allocation."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t64_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-shared-texture-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't64.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        creations = [n for n in tree.findall('./chunks/chunk') if n.get('id') == '1011']
        assert len(creations) == 1
        node = creations[0]
        assert child(child(node, 'descriptor'), 'storageMode').text == '2'
        cases = []

        def edit(field, value):
            variant = copy.deepcopy(tree)
            target = next(n for n in variant.findall('./chunks/chunk') if n.get('id') == '1011')
            for part in field.split('.'):
                target = child(target, part)
            target.text = str(value)
            cases.append((f'{field}-{value}', variant))

        for value in (0, 2, 2**63):
            edit('Texture', value)
        for field, values in (
                ('descriptor.textureType', (0, 3)),
                ('descriptor.pixelFormat', (0, 100)),
                ('descriptor.width', (0, 8193)),
                ('descriptor.height', (0, 8193)),
                ('descriptor.depth', (0, 2)),
                ('descriptor.mipmapLevelCount', (0, 2)),
                ('descriptor.sampleCount', (0, 2)),
                ('descriptor.arrayLength', (0, 2)),
                ('descriptor.storageMode', (0, 1)),
                ('descriptor.resourceOptions', (0, 16, 48)),
                ('descriptor.cpuCacheMode', (1,)),
                ('descriptor.hazardTrackingMode', (1,)),
                ('descriptor.usage', (8,)),
                ('descriptor.swizzle.red', (1,))):
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
    print(f'T64: {len(cases)} malformed shared textures rejected without crash')


if __name__ == '__main__':
    main()
