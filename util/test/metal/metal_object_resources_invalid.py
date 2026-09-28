#!/usr/bin/env python3
"""Reject malformed object-stage texture and sampler bindings."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t85_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-object-res-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't85.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        pipeline_id = child(next(item for item in chunks if item.get('id') == '1301'),
                            'PipelineState').text
        cases = []
        for chunk_id, resource_field in [(1306, 'textures'), (1307, 'textures'),
                                         (1308, 'samplers'), (1309, 'samplers'),
                                         (1310, 'samplers'), (1311, 'samplers')]:
            cases.extend([
                (f'{chunk_id}-encoder', chunk_id, 'RenderCommandEncoder', '0'),
                (f'{chunk_id}-resource', chunk_id, resource_field + '.0', pipeline_id),
                (f'{chunk_id}-slot', chunk_id, 'range.location', '31'),
                (f'{chunk_id}-range', chunk_id, 'range.length', '2'),
            ])
        cases += [
            ('single-lod-negative', 1310, 'lodMinClamps.0', '-1'),
            ('single-lod-reversed', 1310, 'lodMinClamps.0', '2'),
            ('batch-lod-negative', 1311, 'lodMinClamps.0', '-1'),
            ('batch-lod-reversed', 1311, 'lodMinClamps.0', '2'),
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
    print(f'T85 object resource malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
