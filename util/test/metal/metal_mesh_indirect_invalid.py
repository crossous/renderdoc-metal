#!/usr/bin/env python3
"""Reject malformed GPU-driven mesh draw arguments without reading GPU data."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t8x_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    cases = [
        ('encoder-zero', 'RenderCommandEncoder', '0'),
        ('buffer-zero', 'indirectBuffer', '0'),
        ('buffer-unknown', 'indirectBuffer', '999999'),
        ('offset-unaligned', 'indirectBufferOffset', '17'),
        ('offset-out-of-bounds', 'indirectBufferOffset', '20'),
        ('offset-overflow', 'indirectBufferOffset', '18446744073709551612'),
        ('object-width-zero', 'threadsPerObjectThreadgroup.width', '0'),
        ('object-width-too-large', 'threadsPerObjectThreadgroup.width', '65536'),
        ('mesh-width-zero', 'threadsPerMeshThreadgroup.width', '0'),
        ('mesh-width-too-large', 'threadsPerMeshThreadgroup.width', '64'),
        ('mesh-height-too-large', 'threadsPerMeshThreadgroup.height', '64'),
    ]
    with tempfile.TemporaryDirectory(prefix='metal-mesh-indirect-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't88.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        for tag, path, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall('./chunks/chunk')
                         if item.get('id') == '1314')
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
    print(f'{capture.stem.split("_")[0].upper()} mesh indirect malformed captures '
          f'rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
