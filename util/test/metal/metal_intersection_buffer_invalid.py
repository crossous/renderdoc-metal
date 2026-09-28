#!/usr/bin/env python3
"""Malformed intersection-function-table buffer bindings must fail cleanly."""
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
    with tempfile.TemporaryDirectory(prefix='metal-intersection-buffer-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        update = next(c for c in chunks if c.get('name') in (
            'MTLIntersectionFunctionTable::setBuffer',
            'MTLIntersectionFunctionTable::setBuffers'))
        range_update = update.get('name').endswith('setBuffers')
        build = next(c for c in chunks if c.get('name') ==
                     'MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle')
        wrong_id = field(build, 'structure').text
        cases = [
            ('wrong-table', 'Table', wrong_id),
            ('zero-table', 'Table', '0'),
            ('wrong-buffer-type', 'buffer', wrong_id),
            ('missing-buffer', 'buffer', '99999999'),
            ('offset-end', 'offset', '4'),
            ('offset-oob', 'offset', '18446744073709551615'),
        ]
        if range_update:
            cases += [
                ('zero-length', 'range.length', '0'),
                ('length-mismatch', 'range.length', '2'),
                ('index-oob', 'range.location', '31'),
                ('empty-buffers', 'buffers.empty', ''),
                ('empty-offsets', 'offsets.empty', ''),
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
            if member in ('buffer', 'offset') and range_update:
                node = field(target, member + 's')[0]
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
        print(f'{capture.stem} intersection buffer malformed captures rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
