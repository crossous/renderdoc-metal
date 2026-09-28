#!/usr/bin/env python3
"""Reject malformed tile texture/sampler chunks before invoking Metal."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t75_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-tile-sample-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't75.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        wrong_type = child(target(chunks, 1160), 'buffer').text
        cases = [
            ('texture-encoder', 1163, 0, 'RenderCommandEncoder', '0'),
            ('texture-type', 1163, 0, 'textures.0', wrong_type),
            ('texture-slot', 1163, 0, 'range.location', '31'),
            ('texture-length', 1163, 0, 'range.length', '2'),
            ('texture-batch-type', 1164, 0, 'textures.0', wrong_type),
            ('texture-batch-slot', 1164, 0, 'range.location', '31'),
            ('texture-batch-length', 1164, 0, 'range.length', '2'),
            ('sampler-encoder', 1165, 0, 'RenderCommandEncoder', '0'),
            ('sampler-type', 1165, 0, 'samplers.0', wrong_type),
            ('sampler-slot', 1165, 0, 'range.location', '16'),
            ('sampler-length', 1165, 0, 'range.length', '2'),
            ('sampler-batch-type', 1167, 0, 'samplers.0', wrong_type),
            ('sampler-batch-slot', 1167, 0, 'range.location', '16'),
            ('sampler-batch-length', 1167, 0, 'range.length', '2'),
            ('lod-min-negative', 1166, 0, 'lodMinClamps.0', '-1'),
            ('lod-min-nan', 1166, 0, 'lodMinClamps.0', 'nan'),
            ('lod-max-reversed', 1166, 0, 'lodMinClamps.0', '1'),
            ('lod-max-infinite', 1166, 0, 'lodMaxClamps.0', 'inf'),
            ('lod-batch-min-negative', 1168, 0, 'lodMinClamps.0', '-1'),
            ('lod-batch-max-reversed', 1168, 0, 'lodMinClamps.0', '1'),
            ('lod-batch-shape', 1168, 0, 'lodMaxClamps.empty', None),
            ('plain-sampler-lod-shape', 1165, 0, 'lodMinClamps.add', '0'),
        ]
        for tag, chunk_id, ordinal, path, value in cases:
            variant = copy.deepcopy(tree)
            chunk = target(variant.findall('./chunks/chunk'), chunk_id, ordinal)
            node = chunk
            for part in path.split('.'):
                if part.isdigit():
                    node = node[int(part)]
                elif part == 'empty':
                    for member in list(node):
                        node.remove(member)
                elif part == 'add':
                    ET.SubElement(node, 'float', typename='float', width='4').text = value
                else:
                    node = child(node, part)
            if value is not None and not path.endswith('.add'):
                node.text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'T75 tile texture/sampler malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
