#!/usr/bin/env python3
"""Malformed tile pipeline, binding and dispatch chunks must reject before Metal calls."""
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


def target(chunks, chunk_id, ordinal=0):
    return [item for item in chunks if int(item.get('id')) == chunk_id][ordinal]


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t74_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-tile-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't74.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        buffer_id = child(target(chunks, 1160), 'buffer').text
        tile_pipeline = child(target(chunks, 1027), 'PipelineState').text
        draw_pipeline = child(target(chunks, 1021), 'RenderPipelineState').text
        cases = [
            ('tile-pipeline-zero', 1027, 0, 'PipelineState', '0'),
            ('tile-function-zero', 1027, 0, 'tileFunction', '0'),
            ('tile-function-wrong-type', 1027, 0, 'tileFunction', buffer_id),
            ('tile-format-zero', 1027, 0, 'colorFormats.0', '0'),
            ('tile-format-second', 1027, 0, 'colorFormats.1', '80'),
            ('tile-samples', 1027, 0, 'sampleCount', '2'),
            ('tile-max-threads', 1027, 0, 'maxThreads', '2048'),
            ('tile-options', 1027, 0, 'options', '4'),
            ('tile-unsupported', 1027, 0, 'supported', 'false'),
            ('tile-binding-encoder', 1160, 0, 'RenderCommandEncoder', '0'),
            ('tile-binding-buffer', 1160, 0, 'buffer', tile_pipeline),
            ('tile-binding-offset', 1160, 0, 'offset', '12'),
            ('tile-binding-slot', 1160, 0, 'index', '31'),
            ('tile-bytes-empty', 1159, 0, 'data.empty', None),
            ('tile-bytes-slot', 1159, 0, 'range.location', '31'),
            ('tile-bytes-range', 1159, 0, 'range.length', '2'),
            ('tile-offset-overflow', 1161, 0, 'offsets.0', '12'),
            ('tile-offset-unbound', 1161, 0, 'range.location', '1'),
            ('tile-offset-range', 1161, 0, 'range.length', '2'),
            ('tile-batch-buffer', 1162, 0, 'buffers.0', tile_pipeline),
            ('tile-batch-offset', 1162, 0, 'offsets.0', '12'),
            ('tile-batch-length', 1162, 0, 'range.length', '2'),
            ('tile-batch-slot', 1162, 0, 'range.location', '31'),
            ('tile-dispatch-width', 1174, 0, 'threadsPerTile.width', '0'),
            ('tile-dispatch-height', 1174, 0, 'threadsPerTile.height', '17'),
            ('tile-dispatch-depth', 1174, 0, 'threadsPerTile.depth', '2'),
            ('tile-dispatch-encoder', 1174, 0, 'RenderCommandEncoder', '0'),
            ('tile-dispatch-wrong-pso', 1090, 0, 'pipelineState', draw_pipeline),
        ]
        for tag, chunk_id, ordinal, path, value in cases:
            variant = copy.deepcopy(tree)
            chunk = target(variant.findall('./chunks/chunk'), chunk_id, ordinal)
            parts = path.split('.')
            node = chunk
            for part in parts:
                if part.isdigit():
                    node = node[int(part)]
                elif part == 'empty':
                    for member in list(node):
                        node.remove(member)
                else:
                    node = child(node, part)
            if value is not None:
                node.text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'T74 tile malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
