#!/usr/bin/env python3
"""Reject malformed object-stage buffer/bytes bindings."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t84_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-object-bind-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't84.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        pipeline = next(item for item in tree.findall('./chunks/chunk')
                        if item.get('id') == '1301')
        pipeline_id = child(pipeline, 'PipelineState').text
        cases = [
            ('bytes-encoder', 1303, 'RenderCommandEncoder', '0'),
            ('bytes-slot', 1303, 'range.location', '31'),
            ('bytes-range-zero', 1303, 'range.length', '0'),
            ('bytes-range-two', 1303, 'range.length', '2'),
            ('offset-encoder', 1304, 'RenderCommandEncoder', '0'),
            ('offset-past-end', 1304, 'offsets.0', '4096'),
            ('offset-slot', 1304, 'range.location', '31'),
            ('offset-range-two', 1304, 'range.length', '2'),
            ('batch-encoder', 1305, 'RenderCommandEncoder', '0'),
            ('batch-type', 1305, 'buffers.0', pipeline_id),
            ('batch-offset-past-end', 1305, 'offsets.0', '4096'),
            ('batch-slot', 1305, 'range.location', '31'),
            ('batch-range-two', 1305, 'range.length', '2'),
        ]
        for tag, chunk_id, path, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall('./chunks/chunk')
                         if item.get('id') == str(chunk_id))
            node = chunk
            for part in path.split('.'):
                node = node[int(part)] if part.isdigit() else child(node, part)
            node.text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'T84 object binding malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
