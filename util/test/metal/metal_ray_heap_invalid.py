#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Heap AS identity, storage, placement and overlap must be checked before Metal."""
import copy
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=45)
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f'exit {result.returncode}: {args}\n{result.stdout}')
    return result.stdout


def field(node, name):
    return next(child for child in node if child.get('name') == name)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd heap_ray_capture.rdc')
    command, capture = map(Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-ray-heap-invalid-') as temporary:
        directory = Path(temporary)
        original = directory/'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        allocation = next(chunk for chunk in chunks if chunk.get('name') ==
                          'MTLHeap::newAccelerationStructure')
        heap_id = field(allocation, 'Heap').text
        heap = next(chunk for chunk in chunks if chunk.get('name') ==
                    'MTLDevice::newHeapWithDescriptor' and field(chunk, 'Heap').text == heap_id)
        capacity = int(field(heap, 'size').text)
        device = next(chunk for chunk in chunks if chunk.get('name') == 'MTLCreateSystemDefaultDevice')
        device_id = field(device, 'Device').text
        placement = field(allocation, 'placement').text == 'true'
        cases = [('heap-zero', 'Heap', '0'), ('heap-unknown', 'Heap', '999999999'),
                 ('heap-wrong-type', 'Heap', device_id), ('structure-zero', 'Structure', '0'),
                 ('structure-duplicate', 'Structure', heap_id), ('size-zero', 'size', '0'),
                 ('size-huge', 'size', str(1024**3+1)), ('heap-size-zero', 'heap.size', '0'),
                 ('offset-misaligned', 'offset', '1'), ('offset-past-end', 'offset', str(capacity+1)),
                 ('variant-mismatch', 'placement', 'false' if placement else 'true')]
        if placement:
            # Requested heap size can be smaller than native size after driver rounding.
            # Use an aligned offset above the bounded allocation domain instead.
            cases.append(('offset-outside-native-domain', 'offset', str(1 << 63)))

        def replay(variant, tag):
            xml = directory/(tag+'.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory/(tag+'.rdc')
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            output = run(command, 'replay', '--loops', '1', invalid, success=False)
            if not ('Failed to process Metal chunk' in output or 'Failed to replay Metal chunk' in output):
                raise RuntimeError(tag+': missing clean Metal rejection: '+output)

        for tag, member, value in cases:
            variant = copy.deepcopy(tree)
            target = next(chunk for chunk in variant.findall('./chunks/chunk')
                          if chunk.get('name') == 'MTLHeap::newAccelerationStructure')
            if member == 'heap.size':
                target = next(chunk for chunk in variant.findall('./chunks/chunk')
                              if chunk.get('name') == 'MTLDevice::newHeapWithDescriptor' and
                              field(chunk, 'Heap').text == heap_id)
                field(target, 'size').text = value
            else:
                field(target, member).text = value
            replay(variant, tag)
        # A repeated allocation ID is illegal even if the placement and size are legal.
        duplicate = copy.deepcopy(tree)
        parent = duplicate.find('./chunks')
        index = next(i for i, chunk in enumerate(parent)
                     if chunk.get('name') == 'MTLHeap::newAccelerationStructure')
        parent.insert(index+1, copy.deepcopy(parent[index]))
        replay(duplicate, 'duplicate-allocation')
        if placement:
            overlap = copy.deepcopy(duplicate)
            field(overlap.find('./chunks')[index+1], 'Structure').text = '999999998'
            replay(overlap, 'overlap-allocation')
        count = len(cases)+1+int(placement)
        print(f'{capture.stem}: {count} malformed heap AS captures rejected without crash')


if __name__ == '__main__':
    main()
