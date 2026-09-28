#!/usr/bin/env python3
"""Malformed descriptor-created opaque AS allocations must fail cleanly."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t166_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-opaque-descriptor-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        name = 'MTLDevice::newAccelerationStructureWithDescriptor'
        target = next(c for c in chunks if c.get('name') == name)
        vertex = next(c for c in chunks if c.get('name') in (
            'MTLAccelerationStructureCommandEncoder::buildOpaqueTriangle',
            'MTLAccelerationStructureCommandEncoder::buildIndexedOpaqueTriangle'))
        wrong_id = field(vertex, 'vertices').text
        cases = [
            ('zero-structure', 'Structure', '0'),
            ('wrong-structure-type', 'Structure', wrong_id),
            ('missing-structure', 'Structure', '99999999'),
            ('zero-size', 'size', '0'),
            ('huge-size', 'size', '1073741825'),
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
        print(f'{capture.stem} opaque descriptor malformed captures rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
