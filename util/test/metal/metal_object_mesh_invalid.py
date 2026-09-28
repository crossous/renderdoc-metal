#!/usr/bin/env python3
"""Reject unsupported object/mesh pipeline and object buffer capture mutations."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t83_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-object-mesh-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't83.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        pipeline_id = child(next(item for item in chunks if item.get('id') == '1301'),
                            'PipelineState').text
        cases = [
            ('pipeline-zero', 1301, 'PipelineState', '0'),
            ('object-function-zero', 1301, 'objectFunction', '0'),
            ('object-function-type', 1301, 'objectFunction', pipeline_id),
            ('mesh-function-zero', 1301, 'meshFunction', '0'),
            ('fragment-function-zero', 1301, 'fragmentFunction', '0'),
            ('color-format', 1301, 'colorFormats.0', '0'),
            ('sample-count', 1301, 'sampleCount', '2'),
            ('object-threads', 1301, 'maxObjectThreads', '0'),
            ('mesh-threads', 1301, 'maxMeshThreads', '0'),
            ('payload-length', 1301, 'payloadLength', '0'),
            ('mesh-grid', 1301, 'maxMeshGrid', '2'),
            ('unsupported', 1301, 'supported', 'false'),
            ('buffer-encoder-zero', 1302, 'RenderCommandEncoder', '0'),
            ('buffer-type', 1302, 'buffer', pipeline_id),
            ('buffer-offset', 1302, 'offset', '4096'),
            ('buffer-slot', 1302, 'index', '31'),
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
    print(f'T83 object/mesh malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
