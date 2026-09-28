#!/usr/bin/env python3
"""Malformed nested visible-function tables must fail replay without a GPU call."""
import copy
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=30)
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f'unexpected result {result.returncode}: {args}\n{result.stdout}')
    return result.stdout


def field(node, name):
    return next(child for child in node if child.get('name') == name)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-intersection-visible-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        update = next(c for c in chunks if c.get('name') in (
            'MTLIntersectionFunctionTable::setVisibleFunctionTable',
            'MTLIntersectionFunctionTable::setVisibleFunctionTables'))
        range_update = update.get('name').endswith('setVisibleFunctionTables')
        build = next(c for c in chunks if c.get('name') ==
                     'MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle')
        wrong_id = field(build, 'structure').text
        cases = [
            ('wrong-outer', 'Table', wrong_id),
            ('zero-outer', 'Table', '0'),
            ('wrong-inner-type', 'inner', wrong_id),
            ('missing-inner', 'inner', '99999999'),
            ('mismatched-id', 'identity', wrong_id),
        ]
        if range_update:
            cases += [
                ('zero-length', 'range.length', '0'),
                ('length-mismatch', 'range.length', '2'),
                ('index-oob', 'range.location', '31'),
                ('empty-tables', 'tables.empty', ''),
                ('empty-ids', 'tableIds.empty', ''),
            ]
        else:
            cases += [('index-oob', 'index', '31')]
        selected = os.environ.get('RENDERDOC_METAL_NEGATIVE_CASE')
        if selected:
            cases = [case for case in cases if case[0] == selected]
            if not cases:
                raise ValueError(f'unknown negative case: {selected}')
        for tag, member, value in cases:
            variant = copy.deepcopy(tree)
            target = next(c for c in variant.findall('./chunks/chunk')
                          if c.get('name') == update.get('name'))
            if member == 'inner':
                node = field(target, 'tables')[0] if range_update else field(target, 'visibleTable')
            elif member == 'identity':
                node = field(target, 'tableIds')[0] if range_update else field(target, 'visibleTableId')
            elif member.endswith('.empty'):
                field(target, member.split('.')[0])[:] = []
                node = None
            elif member.startswith('range.'):
                node = field(field(target, 'range'), member.split('.')[1])
            else:
                node = field(target, member)
            if node is not None:
                node.text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
        print(f'{capture.stem} nested visible table malformed captures rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
