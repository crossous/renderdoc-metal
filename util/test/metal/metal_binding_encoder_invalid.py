#!/usr/bin/env python3
"""Reject malformed reflection-binding argument encoder snapshots."""
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


def child(node, part):
    if part.isdigit():
        return node[int(part)]
    return next(item for item in node if item.get('name') == part)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t102_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    cases = [
        ('encoder-zero', 'Encoder', '0'),
        ('type-unsupported', 'descriptors.1', '0'),
        ('array-limit', 'descriptors.2', '33'),
        ('access-write', 'descriptors.3', '1'),
        ('texture-type-3d', 'descriptors.4', '7'),
        ('constant-alignment', 'descriptors.5', '16'),
        ('duplicate-index', 'descriptors.6', '0'),
        ('sampler-type-wrong', 'descriptors.7', '58'),
        ('length-mismatch', 'encodedLength', '24'),
        ('alignment-mismatch', 'alignment', '16'),
        ('length-zero', 'encodedLength', '0'),
        ('alignment-zero', 'alignment', '0'),
        ('unsupported', 'supported', 'false'),
    ]
    with tempfile.TemporaryDirectory(prefix='metal-binding-encoder-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't102.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        # Native Metal treats arrayLength0 as a scalar descriptor, including binding snapshots.
        scalar = copy.deepcopy(tree)
        binding = next(item for item in scalar.findall('./chunks/chunk')
                       if item.get('name') == 'MTLDevice::newArgumentEncoderWithBufferBinding')
        child(child(binding, 'descriptors'), '2').text = '0'
        xml = directory / 'scalar.zip.xml'
        scalar.write(xml, encoding='unicode', xml_declaration=True)
        shutil.copyfile(str(original)[:-4], str(xml)[:-4])
        valid = directory / 'scalar.rdc'
        run(command, 'convert', '-f', xml, '-o', valid, '-c', 'rdc')
        run(command, 'replay', '--loops', '3', valid)
        print('PASS buffer-binding scalar descriptor, 3 replay loops')
        for tag, path, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall('./chunks/chunk')
                         if item.get('name') ==
                         'MTLDevice::newArgumentEncoderWithBufferBinding')
            node = chunk
            for part in path.split('.'):
                node = child(node, part)
            node.text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'Buffer-binding argument encoder malformed captures rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
