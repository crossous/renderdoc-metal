#!/usr/bin/env python3
"""Reject malformed tile threadgroup-memory chunks before Metal calls."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t76_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-tile-memory-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't76.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = [item for item in tree.findall('./chunks/chunk')
                  if item.get('id') == '1175']
        assert len(chunks) == 4
        cases = [
            ('encoder-zero', 0, 'RenderCommandEncoder', '0'),
            ('length-unaligned', 0, 'length', '15'),
            ('length-too-large', 0, 'length', '48'),
            ('offset-unaligned', 0, 'offset', '1'),
            ('offset-overflow', 0, 'offset', '32'),
            ('index-overflow', 0, 'index', '31'),
            ('clear-with-offset', 1, 'offset', '16'),
            ('offset-phase-overflow', 2, 'length', '32'),
        ]
        for tag, ordinal, field, value in cases:
            variant = copy.deepcopy(tree)
            chunk = [item for item in variant.findall('./chunks/chunk')
                     if item.get('id') == '1175'][ordinal]
            child(chunk, field).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'T76 tile threadgroup-memory malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
