#!/usr/bin/env python3
"""Reject malformed mesh resource bindings without invoking invalid Metal API calls."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t79_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-mesh-bind-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't79.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        pipeline = next(item for item in chunks if item.get('id') == '1287')
        pipeline_id = child(pipeline, 'PipelineState').text
        cases = [
            ('buffer-encoder-zero', 1289, 'RenderCommandEncoder', '0'),
            ('buffer-type', 1289, 'buffers.0', pipeline_id),
            ('buffer-offset-past-end', 1289, 'offsets.0', '4096'),
            ('buffer-slot-outside', 1289, 'range.location', '31'),
            ('buffer-range-zero', 1289, 'range.length', '0'),
            ('bytes-encoder-zero', 1290, 'RenderCommandEncoder', '0'),
            ('bytes-slot-outside', 1290, 'range.location', '31'),
            ('bytes-range-two', 1290, 'range.length', '2'),
            ('offset-encoder-zero', 1291, 'RenderCommandEncoder', '0'),
            ('offset-past-end', 1291, 'offsets.0', '4096'),
            ('offset-range-two', 1291, 'range.length', '2'),
            ('batch-encoder-zero', 1292, 'RenderCommandEncoder', '0'),
            ('batch-type', 1292, 'buffers.0', pipeline_id),
            ('batch-offset-past-end', 1292, 'offsets.0', '4096'),
            ('batch-range-two', 1292, 'range.length', '2'),
            ('batch-slot-outside', 1292, 'range.location', '31'),
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
    print(f'T79 mesh binding malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
