#!/usr/bin/env python3
"""Reject malformed direct mesh thread-grid dispatches."""
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


def child(node, name):
    return next(item for item in node if item.get('name') == name)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t81_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    cases = [
        ('encoder-zero', 'RenderCommandEncoder', '0'),
        ('grid-width-zero', 'threadsPerGrid.width', '0'),
        ('grid-height-zero', 'threadsPerGrid.height', '0'),
        ('grid-overflow', 'threadsPerGrid.width', '4294967296'),
        ('grid-too-large', 'threadsPerGrid.width', '100000'),
        ('object-threads', 'threadsPerObjectThreadgroup.width', '2'),
        ('mesh-threads-zero', 'threadsPerMeshThreadgroup.width', '0'),
        ('mesh-threads-too-large', 'threadsPerMeshThreadgroup.width', '64'),
    ]
    with tempfile.TemporaryDirectory(prefix='metal-mesh-threads-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't81.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        for tag, path, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall('./chunks/chunk')
                         if item.get('id') == '1299')
            node = chunk
            for part in path.split('.'):
                node = child(node, part)
            node.text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'T81 mesh thread-grid malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
