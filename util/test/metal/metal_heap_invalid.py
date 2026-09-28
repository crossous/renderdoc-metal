#!/usr/bin/env python3
"""Reject malformed automatic Private Metal heaps and heap-buffer creation chunks."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t71_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-heap-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't71.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        edits = [
            ('heap-id-zero', 'MTLDevice::newHeapWithDescriptor', 'Heap', 0),
            ('heap-size-zero', 'MTLDevice::newHeapWithDescriptor', 'size', 0),
            ('heap-size-huge', 'MTLDevice::newHeapWithDescriptor', 'size', 2**63),
            ('heap-storage', 'MTLDevice::newHeapWithDescriptor', 'storageMode', 0),
            ('heap-cache', 'MTLDevice::newHeapWithDescriptor', 'cacheMode', 1),
            ('heap-hazard', 'MTLDevice::newHeapWithDescriptor', 'hazardMode', 0),
            ('heap-sparse', 'MTLDevice::newHeapWithDescriptor', 'type', 2),
            ('buffer-heap-zero', 'MTLHeap::newBuffer', 'Heap', 0),
            ('buffer-heap-wrong-type', 'MTLHeap::newBuffer', 'Heap', 20),
            ('buffer-id-zero', 'MTLHeap::newBuffer', 'Buffer', 0),
            ('buffer-size-zero', 'MTLHeap::newBuffer', 'length', 0),
            ('buffer-size-huge', 'MTLHeap::newBuffer', 'length', 2**63),
            ('buffer-storage', 'MTLHeap::newBuffer', 'options', 0),
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
    print(f'T71 heap malformed captures rejected without crash: {len(edits)} cases')


if __name__ == '__main__':
    main()
