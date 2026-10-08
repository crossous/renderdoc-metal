#!/usr/bin/env python3
"""Reject malformed TLAS dependencies and instance payloads before reconstruction."""
import copy
import pathlib
import struct
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

from metal_ray_table_invalid import field, run


def tlas(parent):
    return next(c for c in parent if c.get('name') == 'Internal::Initial Contents'
                and any(v.get('name') == 'kind' and v.text == '5' for v in c))


def main():
    command, capture = map(pathlib.Path, sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix='metal-ray-instance-initial-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        base = tlas(tree.find('./chunks'))
        cases = [('child-zero', 'child', '0'), ('child-unknown', 'child', '99999999'),
                 ('child-buffer', 'child', field(base, 'source').text),
                 ('child-self', 'child', field(base, 'id').text),
                 ('child-duplicate', 'duplicate', None), ('children-empty', 'empty', None),
                 ('children-limit', 'many', None), ('missing-child-initial', 'remove-child', None)]
        for tag, index, value in [('offset', 0, 4), ('stride', 1, 68), ('type', 2, 1),
                                  ('count-zero', 3, 0), ('count-limit', 3, 65537),
                                  ('payload-size', 3, 2), ('table', 4, 1), ('opaque', 5, 1),
                                  ('duplicate-flag', 6, 1), ('usage', 7, 1)]:
            cases.append((tag, index, str(value)))
        # Default Metal instance descriptor: 12 packed floats followed by options,
        # mask, table offset and the child-array index. Exercise execution-time data.
        cases += [('transform-nan', 'payload', (0, struct.pack('<I', 0x7fc00000))),
                  ('instance-options', 'payload', (48, struct.pack('<I', 16))),
                  ('instance-options-conflict', 'payload', (48, struct.pack('<I', 12))),
                  ('instance-mask', 'payload', (52, struct.pack('<I', 256))),
                  ('instance-table', 'payload', (56, struct.pack('<I', 32))),
                  ('instance-child-range', 'payload', (60, struct.pack('<I', 2)))]
        with zipfile.ZipFile(str(original)[:-4]) as archive:
            payloads = {n: archive.read(n) for n in archive.namelist()}
        for tag, member, value in cases:
            variant = copy.deepcopy(tree)
            parent = variant.find('./chunks')
            target = tlas(parent)
            children = field(target, 'children')
            changed = dict(payloads)
            if isinstance(member, int):
                field(target, 'parameters')[member].text = value
            elif member == 'child':
                children[0].text = value
            elif member == 'duplicate':
                children[1].text = children[0].text
            elif member == 'empty':
                children.clear()
            elif member == 'many':
                for _ in range(3): children.append(copy.deepcopy(children[0]))
            elif member == 'remove-child':
                child = next(c for c in parent if c.get('name') == 'Internal::Initial Contents'
                             and field(c, 'id').text == children[0].text)
                parent.remove(child)
            elif member == 'payload':
                index = field(target, 'vertices').text
                name = f'{int(index):06d}'
                data = bytearray(changed[name]); offset, replacement = value
                data[offset:offset+len(replacement)] = replacement
                changed[name] = bytes(data)
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            with zipfile.ZipFile(str(xml)[:-4], 'w') as archive:
                for name, data in changed.items(): archive.writestr(name, data)
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message or
                    'Invalid Metal initial CPU buffer data' in message), (tag, message)
            print(f'PASS rejected {tag}')
        print(f'PASS {len(cases)} malformed TLAS initial captures')
        legacy = copy.deepcopy(tree)
        for chunk in legacy.find('./chunks'):
            if chunk.get('name') == 'Internal::Initial Contents' and any(v.get('name') == 'kind' for v in chunk):
                field(chunk, 'schema').text = '2'
                chunk.remove(field(chunk, 'indexSource'))
                chunk.remove(field(chunk, 'indices'))
                for member in ['compacted', 'sizeSource', 'sizeParameters']:
                    node = next((v for v in chunk if v.get('name') == member), None)
                    if node is not None: chunk.remove(node)
        xml = directory / 'schema2.zip.xml'
        legacy.write(xml, encoding='unicode', xml_declaration=True)
        with zipfile.ZipFile(str(xml)[:-4], 'w') as archive:
            for name, data in payloads.items(): archive.writestr(name, data)
        capture2 = directory / 'schema2.rdc'
        run(command, 'convert', '-f', xml, '-o', capture2, '-c', 'rdc')
        run(command, 'replay', '--loops', '3', capture2)
        print('PASS schema2 BLAS/TLAS compatibility, 3 replay loops')


if __name__ == '__main__':
    main()
