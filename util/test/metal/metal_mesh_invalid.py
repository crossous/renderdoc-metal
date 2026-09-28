#!/usr/bin/env python3
"""Reject malformed minimal mesh pipeline and direct mesh draw chunks."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t78_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-mesh-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't78.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        pipeline = next(item for item in chunks if item.get('id') == '1287')
        pipeline_id = child(pipeline, 'PipelineState').text
        cases = [
            ('pipeline-zero', 1287, 'PipelineState', '0'),
            ('mesh-function-zero', 1287, 'meshFunction', '0'),
            ('mesh-function-type', 1287, 'meshFunction', pipeline_id),
            ('fragment-function-zero', 1287, 'fragmentFunction', '0'),
            ('color-zero', 1287, 'colorFormats.0', '0'),
            ('second-color', 1287, 'colorFormats.1', '80'),
            ('sample-count', 1287, 'sampleCount', '2'),
            ('mesh-thread-count-zero', 1287, 'maxMeshThreads', '0'),
            ('mesh-thread-count-large', 1287, 'maxMeshThreads', '2048'),
            ('pipeline-options', 1287, 'options', '4'),
            ('unsupported-descriptor', 1287, 'supported', 'false'),
            ('draw-encoder-zero', 1288, 'RenderCommandEncoder', '0'),
            ('draw-grid-zero', 1288, 'threadgroupsPerGrid.width', '0'),
            ('draw-grid-overflow', 1288, 'threadgroupsPerGrid.width', '4294967296'),
            ('draw-object-threads', 1288, 'threadsPerObjectThreadgroup.width', '2'),
            ('draw-mesh-threads-zero', 1288, 'threadsPerMeshThreadgroup.width', '0'),
            ('draw-mesh-threads-large', 1288, 'threadsPerMeshThreadgroup.width', '64'),
        ]
        if any(item.get('name') == 'maxMeshGrid' for item in pipeline):
            cases += [
                ('mesh-grid-one', 1287, 'maxMeshGrid', '1'),
                ('mesh-grid-over-limit', 1287, 'maxMeshGrid', '1048576'),
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
    print(f'Mesh malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
