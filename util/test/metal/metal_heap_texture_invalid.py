#!/usr/bin/env python3
"""Reject malformed heap-backed Private 2D texture creation chunks."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t72_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-heap-texture-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't72.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        pipeline = next(item for item in tree.findall('./chunks/chunk')
                        if item.get('name') == 'MTLDevice::newRenderPipelineStateWithDescriptor')
        wrong_heap_id = child(pipeline, 'RenderPipelineState').text
        edits = [
            ('heap-zero', 'Heap', 0),
            ('heap-wrong-type', 'Heap', wrong_heap_id),
            ('texture-zero', 'Texture', 0),
            ('texture-type', 'descriptor.textureType', 1),
            ('pixel-format', 'descriptor.pixelFormat', 0),
            ('width-zero', 'descriptor.width', 0),
            ('width-huge', 'descriptor.width', 2**63),
            ('height-zero', 'descriptor.height', 0),
            ('mip-count', 'descriptor.mipmapLevelCount', 2),
            ('sample-count', 'descriptor.sampleCount', 2),
            ('array-length', 'descriptor.arrayLength', 2),
            ('storage-mode', 'descriptor.storageMode', 0),
            ('resource-options', 'descriptor.resourceOptions', 0),
            ('cache-mode', 'descriptor.cpuCacheMode', 1),
            ('hazard-mode', 'descriptor.hazardTrackingMode', 3),
            ('usage', 'descriptor.usage', 32),
            ('swizzle-red', 'descriptor.swizzle.red', 3),
        ]
        for tag, path, value in edits:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall('./chunks/chunk')
                         if item.get('name') == 'MTLHeap::newTexture')
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
    print(f'T72 heap texture malformed captures rejected without crash: {len(edits)} cases')


if __name__ == '__main__':
    main()
