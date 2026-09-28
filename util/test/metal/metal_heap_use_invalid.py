#!/usr/bin/env python3
"""Reject malformed render heap residency declarations without reaching Metal."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t73_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-heap-use-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't73.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        pipeline = next(item for item in chunks if item.get('name') ==
                        'MTLDevice::newRenderPipelineStateWithDescriptor')
        wrong_heap = child(pipeline, 'RenderPipelineState').text
        cases = []
        for chunk_id in (1180, 1181, 1182, 1183):
            cases.extend((
                (f'{chunk_id}-encoder-zero', chunk_id, 'encoder', '0'),
                (f'{chunk_id}-heap-zero', chunk_id, 'heap', '0'),
                (f'{chunk_id}-heap-wrong-type', chunk_id, 'heap', wrong_heap),
                (f'{chunk_id}-heap-count', chunk_id, 'count', '33'),
            ))
        cases.extend((
            ('single-empty', 1180, 'empty', None),
            ('single-staged-empty', 1181, 'empty', None),
            ('unstaged-invalid-stage', 1180, 'stages', '2'),
            ('unstaged-batch-invalid-stage', 1182, 'stages', '2'),
            ('staged-zero', 1181, 'stages', '0'),
            ('staged-unknown-bit', 1183, 'stages', '4'),
        ))
        for tag, chunk_id, field, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall('./chunks/chunk')
                         if int(item.get('id')) == chunk_id)
            if field == 'encoder':
                child(chunk, 'RenderCommandEncoder').text = value
            elif field == 'stages':
                child(chunk, 'stagesValue').text = value
            else:
                heaps = child(chunk, 'heaps')
                if field == 'heap':
                    heaps[0].text = value
                elif field == 'empty':
                    heaps.remove(heaps[0])
                else:
                    heaps.extend(copy.deepcopy(heaps[0]) for _ in range(32))
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'T73 heap declaration malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
