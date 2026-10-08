#!/usr/bin/env python3
"""Reject malformed portable AS build inputs before native GPU reconstruction."""
import copy
import pathlib
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET

from metal_ray_table_invalid import field, run


def initial(parent):
    return next(c for c in parent if c.get('name') == 'Internal::Initial Contents'
                and any(v.get('name') == 'parameters' for v in c))


def main():
    command, capture = map(pathlib.Path, sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix='metal-ray-initial-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        base = initial(tree.find('./chunks'))
        source = field(base, 'source').text
        structure = field(base, 'id').text
        cases = []
        for member, wrong_type in [('id', source), ('source', structure)]:
            for tag, value in [('zero', '0'), ('unknown', '99999999'), ('wrong-type', wrong_type)]:
                cases.append((member+'-'+tag, member, value))
        cases += [('schema-zero', 'schema', '0'), ('schema-future', 'schema', '5')]
        for tag, index, value in [
            ('offset-unaligned', 0, 1), ('offset-range', 0, 36),
            ('stride-zero', 1, 0), ('stride-unaligned', 1, 13),
            ('stride-limit', 1, 1048580), ('format-invalid', 2, 0),
            ('float4-short-stride', 2, 31), ('count-zero', 3, 0),
            ('count-limit', 3, 1000001), ('count-payload-size', 3, 2),
            ('table-offset-range', 4, 32), ('opaque-invalid', 5, 2),
            ('duplicate-invalid', 6, 2), ('usage-unsupported', 7, 2),
        ]:
            cases.append((tag, index, str(value)))
        cases += [('invalid-kind', 'kind', '999'),
                  ('missing-parameter', 'short', None),
                  ('duplicate-initial', 'duplicate', None),
                  ('missing-initial', 'remove', None)]
        if len(sys.argv) == 4 and sys.argv[3] == '--formatted':
            # The raw Float4 bytes still fit at offset 20, but the complete final
            # stride does not. Reject before any Metal descriptor/sizes validation.
            cases = [('full-stride-source-bounds', 0, '20')]
        for tag, member, value in cases:
            variant = copy.deepcopy(tree)
            parent = variant.find('./chunks')
            target = initial(parent)
            if isinstance(member, int):
                field(target, 'parameters')[member].text = value
            elif member == 'short':
                params = field(target, 'parameters')
                params.remove(params[-1])
            elif member == 'duplicate':
                parent.insert(list(parent).index(target), copy.deepcopy(target))
            elif member == 'remove':
                parent.remove(target)
            else:
                field(target, member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
            print(f'PASS rejected {tag}')
        print(f'PASS {len(cases)} malformed AS initial captures')
        legacy = copy.deepcopy(tree)
        target = initial(legacy.find('./chunks'))
        field(target, 'schema').text = '1'
        target.remove(field(target, 'kind'))
        target.remove(field(target, 'children'))
        target.remove(field(target, 'indexSource'))
        target.remove(field(target, 'indices'))
        for member in ['compacted', 'sizeSource', 'sizeParameters']:
            node = next((v for v in target if v.get('name') == member), None)
            if node is not None: target.remove(node)
        xml = directory / 'schema1.zip.xml'
        legacy.write(xml, encoding='unicode', xml_declaration=True)
        shutil.copyfile(str(original)[:-4], str(xml)[:-4])
        capture1 = directory / 'schema1.rdc'
        run(command, 'convert', '-f', xml, '-o', capture1, '-c', 'rdc')
        run(command, 'replay', '--loops', '3', capture1)
        print('PASS schema1 BLAS initial compatibility, 3 replay loops')


if __name__ == '__main__':
    main()
