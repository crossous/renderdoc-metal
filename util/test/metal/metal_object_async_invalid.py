#!/usr/bin/env python3
"""Reject malformed async object/mesh pipeline snapshots."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t87_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-object-async-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't87.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunk = next(item for item in tree.findall('./chunks/chunk')
                     if item.get('id') == '1313')
        pipeline_id = child(chunk, 'PipelineState').text
        cases = [
            ('pipeline-zero', 'PipelineState', '0'),
            ('object-function-zero', 'objectFunction', '0'),
            ('object-function-type', 'objectFunction', pipeline_id),
            ('mesh-function-zero', 'meshFunction', '0'),
            ('fragment-function-zero', 'fragmentFunction', '0'),
            ('color-format', 'colorFormats.0', '0'),
            ('payload-length', 'payloadLength', '0'),
            ('unsupported', 'supported', 'false'),
        ]
        for tag, path, value in cases:
            variant = copy.deepcopy(tree)
            current = next(item for item in variant.findall('./chunks/chunk')
                           if item.get('id') == '1313')
            node = current
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
    print(f'T87 async object/mesh malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
