#!/usr/bin/env python3
"""Compacted initial recipes must validate native capacity before compact copy."""
import copy
import pathlib
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_ray_table_invalid import field, run
from metal_ray_initial_invalid import initial


def main():
    command, compact, ordinary = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-ray-compact-initial-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', compact, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        target = initial(tree.find('./chunks')); structure = field(target, 'id').text
        assert field(target, 'compacted').text == 'true'
        cases = [('lost-compact-flag', 'flag', 'false'), ('schema3-lost-compaction', 'schema', '3'),
                 ('zero-capacity', 'capacity', '0'), ('small-capacity256', 'capacity', '256'),
                 ('small-capacity512', 'capacity', '512'), ('small-capacity1024', 'capacity', '1024')]
        for tag, member, value in cases:
            variant = copy.deepcopy(tree); parent = variant.find('./chunks'); target = initial(parent)
            if member == 'flag': field(target, 'compacted').text = value
            elif member == 'schema':
                field(target, 'schema').text = value
                for member in ['compacted', 'sizeSource', 'sizeParameters']: target.remove(field(target, member))
            else:
                creation = next(c for c in parent if c.get('name') == 'MTLDevice::newAccelerationStructureWithSize'
                                and field(c, 'Structure').text == structure)
                field(creation, 'size').text = value
            xml = directory / f'{tag}.zip.xml'; variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4]); invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert 'Failed to process Metal chunk' in message or 'Invalid Metal initial CPU buffer data' in message, (tag, message)
            print(f'PASS rejected {tag}, no signal/hang')
        print(f'PASS {len(cases)} malformed compact AS initial captures')
        original = directory / 'ordinary.zip.xml'
        run(command, 'convert', '-f', ordinary, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        query = initial(tree.find('./chunks'))
        assert field(query, 'sizeSource').text != '0'
        query_cases = [('query-source-zero', 'sizeSource', '0'),
                       ('query-source-unknown', 'sizeSource', '99999999'),
                       ('query-source-AS', 'sizeSource', field(query, 'id').text),
                       ('query-offset-unaligned', 0, '1'), ('query-offset-range', 0, '8'),
                       ('query-type-invalid', 1, '999'), ('query-parameters-short', 'short', None),
                       ('query-parameters-long', 'long', None)]
        for tag, member, value in query_cases:
            variant = copy.deepcopy(tree); target = initial(variant.find('./chunks'))
            parameters = field(target, 'sizeParameters')
            if isinstance(member, int): parameters[member].text = value
            elif member == 'short': parameters.remove(parameters[-1])
            elif member == 'long': parameters.append(copy.deepcopy(parameters[-1]))
            else: field(target, member).text = value
            xml = directory / f'{tag}.zip.xml'; variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4]); capture = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', capture, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', capture, success=False)
            assert 'Failed to process Metal chunk' in message, (tag, message)
            print(f'PASS rejected {tag}, no signal/hang')
        print(f'PASS {len(query_cases)} malformed initial size-query captures')
        for schema in [1, 2, 3]:
            legacy = copy.deepcopy(tree); target = initial(legacy.find('./chunks'))
            field(target, 'schema').text = str(schema)
            for member in ['compacted', 'sizeSource', 'sizeParameters']: target.remove(field(target, member))
            if schema < 3:
                target.remove(field(target, 'indexSource')); target.remove(field(target, 'indices'))
            if schema < 2:
                target.remove(field(target, 'kind')); target.remove(field(target, 'children'))
            xml = directory / f'schema{schema}.zip.xml'; legacy.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4]); capture = directory / f'schema{schema}.rdc'
            run(command, 'convert', '-f', xml, '-o', capture, '-c', 'rdc')
            run(command, 'replay', '--loops', '3', capture)
            print(f'PASS schema{schema} noncompact initial compatibility, 3 loops')


if __name__ == '__main__':
    main()
