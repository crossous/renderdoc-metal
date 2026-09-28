#!/usr/bin/env python3
"""Reject malformed object-stage dynamic threadgroup-memory bindings."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t86_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-object-memory-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't86.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        pipeline = next(item for item in tree.findall('./chunks/chunk')
                        if item.get('id') == '1301')
        pipeline_id = child(pipeline, 'PipelineState').text
        cases = [
            ('encoder-zero', 'RenderCommandEncoder', '0'),
            ('encoder-type', 'RenderCommandEncoder', pipeline_id),
            ('index-outside', 'index', '31'),
            ('length-unaligned', 'length', '17'),
            ('length-too-large', 'length', '1048576'),
        ]
        for tag, path, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall('./chunks/chunk')
                         if item.get('id') == '1312')
            child(chunk, path).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'T86 object-memory malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
