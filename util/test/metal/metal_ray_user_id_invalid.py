#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reject malformed user-ID TLAS layouts, payloads and typed dependencies."""
import copy
from pathlib import Path
import struct
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from metal_ray_table_invalid import field, run


def main():
    command, capture = map(Path, sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix='metal-ray-user-id-invalid-') as tmp:
        directory = Path(tmp)
        original = directory/'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        def target(tree):
            return next(c for c in tree.findall('./chunks/chunk') if
                c.get('name') == 'MTLAccelerationStructureCommandEncoder::buildUserIDInstances' or
                (c.get('name') == 'Internal::Initial Contents' and
                 any(v.get('name') == 'schema' and v.text == '6' for v in c)))
        base = target(tree)
        initial = base.get('name') == 'Internal::Initial Contents'
        source = 'source' if initial else 'instances'
        as_field = 'id' if initial else 'structure'
        cases = []
        for index, value in ((0, 1), (0, 18446744073709551612), (1, 64), (1, 70),
                             (1, 1048580), (2, 0), (2, 2), (2, 3), (3, 0), (3, 65537),
                             (3, int(field(base, 'parameters')[3].text)+1),
                             (4, 1), (5, 1), (6, 1), (7, 1)):
            cases.append((f'layout-{index}-{value}', index, str(value)))
        for name in (source, as_field):
            for tag, value in (('zero', '0'), ('unknown', '999999999')):
                cases.append((name+'-'+tag, name, value))
        cases += [('source-wrong-type', source, field(base, as_field).text),
                  ('child-zero', 'child', '0'), ('child-unknown', 'child', '999999999'),
                  ('child-wrong-type', 'child', field(base, source).text),
                  ('child-self', 'child', field(base, as_field).text),
                  ('children-empty', 'children-empty', None),
                  ('children-duplicate', 'children-duplicate', None),
                  ('children-limit', 'children-limit', None),
                  ('layout-short', 'layout-short', None), ('payload-short', 'payload-short', None)]
        for tag, offset, value in (('nan', 0, 0x7fc00000), ('options', 48, 16),
                                   ('option-conflict', 48, 12), ('mask', 52, 256),
                                   ('table', 56, 32), ('child-range', 60, 4)):
            cases.append((tag, 'payload', (offset, struct.pack('<I', value))))
        if initial:
            cases += [(f'schema-{v}', 'schema', str(v)) for v in (0, 4, 5, 7)]
            cases += [('kind-mismatch', 'kind', '8'), ('missing-child-initial', 'remove-child', None)]
        else:
            cases += [('scratch-zero', 'scratch', '0'),
                      ('scratch-wrong-type', 'scratch', field(base, as_field).text),
                      ('encoder-wrong-type', 'Encoder', field(base, as_field).text)]
        with zipfile.ZipFile(str(original)[:-4]) as archive:
            payloads = {n: archive.read(n) for n in archive.namelist()}
        for tag, member, value in cases:
            variant = copy.deepcopy(tree); node = target(variant); changed = dict(payloads)
            children = field(node, 'children')
            if isinstance(member, int): field(node, 'parameters')[member].text = value
            elif member == 'child': children[0].text = value
            elif member == 'children-empty': children.clear()
            elif member == 'children-duplicate': children.append(copy.deepcopy(children[0]))
            elif member == 'children-limit':
                while len(children) < 5: children.append(copy.deepcopy(children[0]))
            elif member == 'layout-short': field(node, 'parameters').remove(field(node, 'parameters')[-1])
            elif member == 'remove-child':
                parent = variant.find('./chunks')
                old = next(c for c in parent if c.get('name') == 'Internal::Initial Contents' and
                           field(c, 'id').text == children[0].text)
                parent.remove(old)
            elif member in ('payload', 'payload-short'):
                name = f"{int(field(node, 'vertices' if initial else 'descriptorBytes').text):06d}"
                data = bytearray(changed[name])
                if member == 'payload-short': data = data[:-1]
                else:
                    offset, replacement = value; data[offset:offset+len(replacement)] = replacement
                changed[name] = bytes(data)
            else: field(node, member).text = value
            xml = directory/(tag+'.zip.xml'); variant.write(xml, encoding='unicode', xml_declaration=True)
            with zipfile.ZipFile(str(xml)[:-4], 'w') as archive:
                for name, data in changed.items(): archive.writestr(name, data)
            invalid = directory/(tag+'.rdc')
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            if not ('Failed to process Metal chunk' in message or 'Failed to replay Metal chunk' in message or
                    'Invalid Metal initial CPU buffer data' in message):
                raise RuntimeError(tag+': missing clean rejection: '+message)
        print(f'PASS {len(cases)} malformed user-ID {"initial" if initial else "frame"} captures')


if __name__ == '__main__': main()
