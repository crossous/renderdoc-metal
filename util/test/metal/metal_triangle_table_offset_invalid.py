#!/usr/bin/env python3
"""Reject malformed triangle geometry intersection-table-offset builds."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-triangle-table-offset-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        name = ('MTLAccelerationStructureCommandEncoder::buildTriangleNoDuplicate'
                if any(c.get('name') ==
                       'MTLAccelerationStructureCommandEncoder::buildTriangleNoDuplicate'
                       for c in tree.findall('./chunks/chunk'))
                else 'MTLAccelerationStructureCommandEncoder::buildTriangleTableOffset')
        target = next(c for c in tree.findall('./chunks/chunk') if c.get('name') == name)
        structure = field(target, 'structure').text
        vertices = field(target, 'vertices').text
        scratch = field(target, 'scratch').text
        cases = [
            ('wrong-encoder', 'Encoder', structure),
            ('zero-encoder', 'Encoder', '0'),
            ('wrong-structure', 'structure', vertices),
            ('zero-structure', 'structure', '0'),
            ('wrong-vertices', 'vertices', structure),
            ('zero-vertices', 'vertices', '0'),
            ('wrong-indices', 'indices', structure),
            ('invalid-index-type', 'indexType', '99'),
            ('misaligned-index-offset', 'indexOffset', '1'),
            ('misaligned-vertex-offset', 'vertexOffset', '1'),
            ('misaligned-scratch-offset', 'scratchOffset', '1'),
            ('zero-triangles', 'triangleCount', '0'),
            ('huge-triangles', 'triangleCount', '1000001'),
            ('wrong-scratch', 'scratch', structure),
            ('zero-scratch', 'scratch', '0'),
            ('large-table-offset', 'tableOffset', '32'),
        ]
        if name.endswith('TableOffset'):
            cases.append(('zero-table-offset', 'tableOffset', '0'))
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
        print(f'{capture.stem} triangle table offset malformed captures rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
