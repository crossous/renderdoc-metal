#!/usr/bin/env python3
"""Reject malformed mesh texture and sampler binding chunks."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t80_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-mesh-res-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't80.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        pipeline_id = child(next(item for item in chunks if item.get('id') == '1287'),
                            'PipelineState').text
        cases = []
        for chunk_id, resource_field in [(1293, 'textures'), (1294, 'textures'),
                                         (1295, 'samplers'), (1296, 'samplers'),
                                         (1297, 'samplers'), (1298, 'samplers')]:
            cases.extend([
                (f'{chunk_id}-encoder', chunk_id, 'RenderCommandEncoder', '0'),
                (f'{chunk_id}-resource', chunk_id, resource_field + '.0', pipeline_id),
                (f'{chunk_id}-slot', chunk_id, 'range.location', '31'),
                (f'{chunk_id}-range', chunk_id, 'range.length', '2'),
            ])
        cases += [
            ('single-lod-negative', 1297, 'lodMinClamps.0', '-1'),
            ('single-lod-reversed', 1297, 'lodMinClamps.0', '2'),
            ('batch-lod-negative', 1298, 'lodMinClamps.0', '-1'),
            ('batch-lod-reversed', 1298, 'lodMinClamps.0', '2'),
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
    print(f'T80 mesh resource malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
