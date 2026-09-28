#!/usr/bin/env python3
"""Malformed opaque triangle AS builds must fail before GPU replay."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t165_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-opaque-triangle-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        name = 'MTLAccelerationStructureCommandEncoder::buildOpaqueTriangle'
        target = next(c for c in chunks if c.get('name') == name)
        wrong_id = field(target, 'structure').text
        cases = [
            ('wrong-encoder', 'Encoder', wrong_id),
            ('zero-encoder', 'Encoder', '0'),
            ('wrong-structure', 'structure', field(target, 'vertices').text),
            ('zero-structure', 'structure', '0'),
            ('wrong-vertices', 'vertices', wrong_id),
            ('missing-vertices', 'vertices', '99999999'),
            ('offset-oob', 'vertexOffset', '18446744073709551615'),
            ('offset-unaligned', 'vertexOffset', '2'),
            ('zero-count', 'triangleCount', '0'),
            ('huge-count', 'triangleCount', '1000001'),
            ('wrong-scratch', 'scratch', wrong_id),
            ('scratch-offset', 'scratchOffset', '18446744073709551615'),
            ('scratch-offset-unaligned', 'scratchOffset', '16'),
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
        print(f'{capture.stem} opaque triangle malformed captures rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
