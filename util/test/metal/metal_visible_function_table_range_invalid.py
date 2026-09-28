#!/usr/bin/env python3
"""Reject malformed fragment visible-function-table range packets."""
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


def field(node, name):
    return next(child for child in node if child.get('name') == name)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t121_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-visible-table-range-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't121.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        binding_name = next(name for name in (
            'MTLRenderCommandEncoder::setFragmentVisibleFunctionTables',
            'MTLRenderCommandEncoder::setVertexVisibleFunctionTables',
            'MTLComputeCommandEncoder::setVisibleFunctionTables',
            'MTLRenderCommandEncoder::setTileVisibleFunctionTables')
            if any(c.get('name') == name for c in chunks))
        binding = next(c for c in chunks if c.get('name') == binding_name)
        handle = next(c for c in chunks if c.get('name') in (
            'MTLRenderPipelineState::functionHandleWithFunction',
            'MTLComputePipelineState::functionHandleWithFunction'))
        handle_id = field(handle, 'Handle').text
        table_id = field(binding, 'tables')[0].text
        cases = [
            ('member-wrong-type', 'member', handle_id),
            ('range-location', 'location', '31'),
            ('range-length-zero', 'length', '0'),
            ('range-length-mismatch', 'length', '2'),
            ('range-length-huge', 'length', '32'),
        ]
        assert table_id != handle_id
        for tag, member, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(c for c in variant.findall('./chunks/chunk') if c.get('name') ==
                         binding_name)
            if member == 'member':
                field(chunk, 'tables')[0].text = value
            else:
                field(field(chunk, 'range'), member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'{capture.stem} visible-table range malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
