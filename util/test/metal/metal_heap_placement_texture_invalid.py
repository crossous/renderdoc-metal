#!/usr/bin/env python3
"""Reject malformed explicit-offset placement heap texture captures."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t117_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-placement-texture-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't117.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        edits = [
            ('heap-automatic', 'MTLDevice::newHeapWithDescriptor', 'type', 0),
            ('heap-sparse', 'MTLDevice::newHeapWithDescriptor', 'type', 2),
            ('heap-size-small', 'MTLDevice::newHeapWithDescriptor', 'size', 512),
            ('texture-heap-zero', 'MTLHeap::newTexture(offset)', 'Heap', 0),
            ('texture-id-zero', 'MTLHeap::newTexture(offset)', 'Texture', 0),
            ('texture-format', 'MTLHeap::newTexture(offset)', 'descriptor.pixelFormat', 0),
            ('texture-width-zero', 'MTLHeap::newTexture(offset)', 'descriptor.width', 0),
            ('texture-width-huge', 'MTLHeap::newTexture(offset)', 'descriptor.width', 2**63),
            ('texture-storage', 'MTLHeap::newTexture(offset)', 'descriptor.storageMode', 0),
            ('texture-offset-unaligned', 'MTLHeap::newTexture(offset)', 'offset', 1),
            ('texture-offset-end', 'MTLHeap::newTexture(offset)', 'offset', 65536),
            ('texture-offset-huge', 'MTLHeap::newTexture(offset)', 'offset', 2**63),
        ]
        for tag, chunk_name, path, value in edits:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall('./chunks/chunk')
                         if item.get('name') == chunk_name)
            node = chunk
            for part in path.split('.'):
                node = child(node, part)
            node.text = str(value)
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
        texture_chunk = next(item for item in chunks
                             if item.get('name') == 'MTLHeap::newTexture(offset)')
        duplicate = copy.deepcopy(texture_chunk)
        child(duplicate, 'Texture').text = '999999'
        chunks.insert(list(chunks).index(texture_chunk) + 1, duplicate)
        xml = directory / 'texture-overlap.zip.xml'
        variant.write(xml, encoding='unicode', xml_declaration=True)
        shutil.copyfile(str(original)[:-4], str(xml)[:-4])
        invalid = directory / 'texture-overlap.rdc'
        run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
        message = run(command, 'replay', '--loops', '1', invalid, success=False)
        assert ('Failed to process Metal chunk MTLHeap::newTexture(offset)' in message or
                'Failed to replay Metal chunk MTLHeap::newTexture(offset)' in message), message
    print(f'T117 placement texture malformed captures rejected without crash: {len(edits) + 1} cases')


if __name__ == '__main__':
    main()
