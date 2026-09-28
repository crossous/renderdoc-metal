#!/usr/bin/env python3
"""Reject malformed async tile-pipeline creation chunks without crashing."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t77_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-tile-async-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't77.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = [item for item in tree.findall('./chunks/chunk') if item.get('id') == '1286']
        assert len(chunks) == 1
        cases = [
            ('pipeline-zero', 'PipelineState', '0'),
            ('function-zero', 'tileFunction', '0'),
            ('format-zero', 'colorFormats.0', '0'),
            ('second-format', 'colorFormats.1', '80'),
            ('sample-count', 'sampleCount', '2'),
            ('max-threads', 'maxThreads', '2048'),
            ('options', 'options', '4'),
            ('unsupported', 'supported', 'false'),
        ]
        for tag, path, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall('./chunks/chunk')
                         if item.get('id') == '1286')
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
    print(f'T77 async tile-pipeline malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
