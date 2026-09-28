#!/usr/bin/env python3
"""Validate non-unit object threadgroups for both direct mesh draw variants."""
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
    if len(sys.argv) != 4:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd first_capture.rdc second_capture.rdc')
    command = pathlib.Path(sys.argv[1])
    with tempfile.TemporaryDirectory(prefix='metal-object-threadgroup-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        total = 0
        for capture, chunk_id, grid in ((pathlib.Path(sys.argv[2]), '1288', 'threadgroupsPerGrid'),
                                        (pathlib.Path(sys.argv[3]), '1299', 'threadsPerGrid')):
            original = directory / f'{chunk_id}.zip.xml'
            run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
            tree = ET.parse(original)
            cases = [
                ('encoder-zero', 'RenderCommandEncoder', '0'),
                ('object-width-zero', 'threadsPerObjectThreadgroup.width', '0'),
                ('object-width-over-limit', 'threadsPerObjectThreadgroup.width', '33'),
                ('object-product-over-limit', 'threadsPerObjectThreadgroup.height', '9'),
                ('object-depth-zero', 'threadsPerObjectThreadgroup.depth', '0'),
                ('mesh-width-zero', 'threadsPerMeshThreadgroup.width', '0'),
                ('mesh-width-over-limit', 'threadsPerMeshThreadgroup.width', '64'),
                ('grid-width-zero', grid + '.width', '0'),
            ]
            for tag, path, value in cases:
                variant = copy.deepcopy(tree)
                chunk = next(item for item in variant.findall('./chunks/chunk')
                             if item.get('id') == chunk_id)
                node = chunk
                for part in path.split('.'):
                    node = child(node, part)
                node.text = value
                xml = directory / f'{chunk_id}-{tag}.zip.xml'
                variant.write(xml, encoding='unicode', xml_declaration=True)
                shutil.copyfile(str(original)[:-4], str(xml)[:-4])
                invalid = directory / f'{chunk_id}-{tag}.rdc'
                run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
                message = run(command, 'replay', '--loops', '1', invalid, success=False)
                assert ('Failed to process Metal chunk' in message or
                        'Failed to replay Metal chunk' in message), (tag, message)
                total += 1
    first = pathlib.Path(sys.argv[2]).stem.split('_')[0].upper()
    second = pathlib.Path(sys.argv[3]).stem.split('_')[0].upper()
    print(f'{first}–{second} object threadgroup malformed captures '
          f'rejected without crash: {total} cases')


if __name__ == '__main__':
    main()
