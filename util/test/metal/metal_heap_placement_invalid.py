#!/usr/bin/env python3
"""Reject malformed explicit-offset placement heap buffer captures."""
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


def child(node, field):
    return next(item for item in node if item.get('name') == field)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t116_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-placement-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't116.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        edits = [
            ('heap-type-automatic', 'MTLDevice::newHeapWithDescriptor', 'type', 0),
            ('heap-type-sparse', 'MTLDevice::newHeapWithDescriptor', 'type', 2),
            ('heap-size-small', 'MTLDevice::newHeapWithDescriptor', 'size', 512),
            ('buffer-heap-zero', 'MTLHeap::newBuffer(offset)', 'Heap', 0),
            ('buffer-id-zero', 'MTLHeap::newBuffer(offset)', 'Buffer', 0),
            ('buffer-size-zero', 'MTLHeap::newBuffer(offset)', 'length', 0),
            ('buffer-size-huge', 'MTLHeap::newBuffer(offset)', 'length', 2**63),
            ('buffer-storage', 'MTLHeap::newBuffer(offset)', 'options', 0),
            ('buffer-offset-unaligned', 'MTLHeap::newBuffer(offset)', 'offset', 1),
            ('buffer-offset-end', 'MTLHeap::newBuffer(offset)', 'offset', 65536),
            ('buffer-offset-huge', 'MTLHeap::newBuffer(offset)', 'offset', 2**63),
        ]
        for tag, chunk_name, field, value in edits:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall('./chunks/chunk')
                         if item.get('name') == chunk_name)
            child(chunk, field).text = str(value)
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)

        variant = copy.deepcopy(tree)
        chunks = variant.find('./chunks')
        buffer_chunk = next(item for item in chunks
                            if item.get('name') == 'MTLHeap::newBuffer(offset)')
        duplicate = copy.deepcopy(buffer_chunk)
        child(duplicate, 'Buffer').text = '999999'
        chunks.insert(list(chunks).index(buffer_chunk) + 1, duplicate)
        xml = directory / 'buffer-overlap.zip.xml'
        variant.write(xml, encoding='unicode', xml_declaration=True)
        shutil.copyfile(str(original)[:-4], str(xml)[:-4])
        invalid = directory / 'buffer-overlap.rdc'
        run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
        message = run(command, 'replay', '--loops', '1', invalid, success=False)
        assert ('Failed to process Metal chunk MTLHeap::newBuffer(offset)' in message or
                'Failed to replay Metal chunk MTLHeap::newBuffer(offset)' in message), message
    print(f'T116 placement heap malformed captures rejected without crash: {len(edits) + 1} cases')


if __name__ == '__main__':
    main()
