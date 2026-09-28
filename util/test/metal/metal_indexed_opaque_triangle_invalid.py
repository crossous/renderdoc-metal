#!/usr/bin/env python3
"""Malformed indexed AS builds must fail cleanly before GPU execution."""
import copy
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t167_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-indexed-opaque-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        name = next(c.get('name') for c in tree.findall('./chunks/chunk')
                    if c.get('name') in (
                        'MTLAccelerationStructureCommandEncoder::buildIndexedTriangleExtended',
                        'MTLAccelerationStructureCommandEncoder::buildIndexedTriangleOffset',
                        'MTLAccelerationStructureCommandEncoder::buildIndexedOpaqueTriangle',
                        'MTLAccelerationStructureCommandEncoder::buildIndexedTriangle'))
        target = next(c for c in tree.findall('./chunks/chunk') if c.get('name') == name)
        structure = field(target, 'structure').text
        vertices = field(target, 'vertices').text
        indices = field(target, 'indices').text
        cases = [
            ('wrong-encoder', 'Encoder', structure),
            ('zero-encoder', 'Encoder', '0'),
            ('wrong-structure', 'structure', vertices),
            ('zero-structure', 'structure', '0'),
            ('wrong-vertices', 'vertices', structure),
            ('missing-vertices', 'vertices', '99999999'),
            ('wrong-indices', 'indices', structure),
            ('zero-indices', 'indices', '0'),
            ('wrong-index-type', 'indexType', '99'),
            ('zero-count', 'triangleCount', '0'),
            ('huge-count', 'triangleCount', '1000001'),
            ('wrong-scratch', 'scratch', indices),
        ]
        if name.endswith('buildIndexedTriangleOffset'):
            cases += [
                ('zero-offset', 'indexOffset', '0'),
                ('misaligned-offset', 'indexOffset', '1'),
                ('past-end-offset', 'indexOffset', '1000000'),
            ]
        if name.endswith('buildIndexedTriangleExtended'):
            cases += [
                ('misaligned-index-offset', 'indexOffset', '1'),
                ('past-end-index-offset', 'indexOffset', '1000000'),
                ('misaligned-vertex-offset', 'vertexOffset', '1'),
                ('past-end-vertex-offset', 'vertexOffset', '1000000'),
                ('misaligned-scratch-offset', 'scratchOffset', '1'),
                ('past-end-scratch-offset', 'scratchOffset', '1000000'),
            ]
        for tag, member, value in cases:
            variant = copy.deepcopy(tree)
            selected = next(c for c in variant.findall('./chunks/chunk')
                            if c.get('name') == name)
            field(selected, member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
        print(f'{capture.stem} indexed malformed captures rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
