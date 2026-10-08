#!/usr/bin/env python3
"""Validate portable index input identity, offset, format and decoded vertex span."""
import copy
import pathlib
import struct
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

from metal_ray_table_invalid import field, run
from metal_ray_initial_invalid import initial


def main():
    command, capture = map(pathlib.Path, sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix='metal-ray-indexed-initial-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        base = initial(tree.find('./chunks'))
        with zipfile.ZipFile(str(original)[:-4]) as archive:
            payloads = {n: archive.read(n) for n in archive.namelist()}
        cases = [('index-zero', 'indexSource', '0'), ('index-unknown', 'indexSource', '99999999'),
                 ('index-AS', 'indexSource', field(base, 'id').text),
                 ('offset-unaligned', 8, '1'), ('offset-range', 8, '10'),
                 ('index-type', 9, '999'), ('index-type-payload-size', 9, '1'),
                 ('count-payload', 3, '2'), ('stride-payload', 1, '16'),
                 ('vertex-format-stride', 2, '31'), ('missing-parameter', 'short', None),
                 ('vertex-span-oob', 'payload', struct.pack('<HHH', 0, 1, 65535)),
                 ('vertex-span-mismatch', 'payload', struct.pack('<HHH', 0, 0, 0)),
                 ('unexpected-children', 'children', field(base, 'id').text),
                 ('wrong-kind', 'kind', '1')]
        for tag, member, value in cases:
            variant = copy.deepcopy(tree)
            target = initial(variant.find('./chunks'))
            changed = dict(payloads)
            if isinstance(member, int):
                field(target, 'parameters')[member].text = value
            elif member == 'short':
                parameters = field(target, 'parameters'); parameters.remove(parameters[-1])
            elif member == 'payload':
                name = f"{int(field(target, 'indices').text):06d}"
                changed[name] = value
            elif member == 'children':
                child = copy.deepcopy(field(target, 'id'))
                child.attrib.pop('name', None); child.text = value
                field(target, 'children').append(child)
            else:
                field(target, member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            with zipfile.ZipFile(str(xml)[:-4], 'w') as archive:
                for name, data in changed.items(): archive.writestr(name, data)
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert 'Failed to process Metal chunk' in message, (tag, message)
            print(f'PASS rejected {tag}')
        print(f'PASS {len(cases)} malformed indexed AS initial captures')


if __name__ == '__main__':
    main()
