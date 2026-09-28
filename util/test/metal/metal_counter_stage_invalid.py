#!/usr/bin/env python3
"""Reject malformed stage-boundary counter resources, pass bindings and resolves."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t101_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    names = {
        'create': 'MTLDevice::newCounterSampleBufferWithDescriptor',
        'pass': 'MTLCommandBuffer::renderCommandEncoderWithDescriptor',
        'resolve': 'MTLBlitCommandEncoder::resolveCounters',
    }
    cases = [
        ('create-id-zero', 'create', 'CounterSampleBuffer', '0'),
        ('create-set-name', 'create', 'counterSetName', 'not-timestamp'),
        ('create-count-zero', 'create', 'sampleCount', '0'),
        ('create-count-three', 'create', 'sampleCount', '3'),
        ('create-count-over-limit', 'create', 'sampleCount', '65'),
        ('create-storage-private', 'create', 'storageMode', '2'),
        ('create-unsupported', 'create', 'supported', 'false'),
        ('pass-sample-unknown', 'pass', 'descriptor.sampleBufferAttachments.0.sampleBuffer', '999999'),
        ('pass-sample-wrong-type', 'pass', 'descriptor.sampleBufferAttachments.0.sampleBuffer', '32'),
        ('pass-sample-id-zero', 'pass', 'descriptor.sampleBufferAttachments.0.sampleBufferId', '0'),
        ('pass-sample-id-unknown', 'pass', 'descriptor.sampleBufferAttachments.0.sampleBufferId', '999999'),
        ('pass-start-vertex-oob', 'pass', 'descriptor.sampleBufferAttachments.0.startOfVertexSampleIndex', '{count}'),
        ('pass-end-fragment-oob', 'pass', 'descriptor.sampleBufferAttachments.0.endOfFragmentSampleIndex', '{count}'),
        ('resolve-sample-zero', 'resolve', 'sampleBuffer', '0'),
        ('resolve-sample-wrong-type', 'resolve', 'sampleBuffer', '32'),
        ('resolve-range-empty', 'resolve', 'range.length', '0'),
        ('resolve-range-start-oob', 'resolve', 'range.location', '{count}'),
        ('resolve-range-length-oob', 'resolve', 'range.length', '{too_long}'),
        ('resolve-destination-zero', 'resolve', 'destinationBuffer', '0'),
        ('resolve-destination-wrong-type', 'resolve', 'destinationBuffer', '31'),
        ('resolve-offset-unaligned', 'resolve', 'destinationOffset', '3'),
        ('resolve-offset-oob', 'resolve', 'destinationOffset', '64'),
    ]
    with tempfile.TemporaryDirectory(prefix='metal-counter-stage-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't101.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        creation = next(item for item in tree.findall('./chunks/chunk')
                        if item.get('name') == names['create'])
        count = int(child(creation, 'sampleCount').text)
        for tag, kind, path, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall('./chunks/chunk')
                         if item.get('name') == names[kind])
            node = chunk
            for part in path.split('.'):
                node = child(node, part)
            node.text = value.format(count=count, too_long=count + 1)
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'Stage-boundary counter malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
