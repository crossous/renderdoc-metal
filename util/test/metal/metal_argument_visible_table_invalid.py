#!/usr/bin/env python3
"""Reject malformed argument-encoder visible-function-table identity and ranges."""
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


def chunk(tree, name):
    return next(c for c in tree.findall('./chunks/chunk') if c.get('name') == name)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t126/127_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-argument-visible-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'argument.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        encoder = chunk(tree, 'MTLFunction::newArgumentEncoderWithBufferIndex')
        selected = chunk(tree, 'MTLArgumentEncoder::setArgumentBuffer')
        table = chunk(tree, 'MTLArgumentEncoder::setVisibleFunctionTable')
        handle = chunk(tree, 'MTLComputePipelineState::functionHandleWithFunction')
        table_id = field(table, 'table').text
        handle_id = field(handle, 'Handle').text
        encoder_id = field(encoder, 'ArgumentEncoder').text
        assert field(table, 'ArgumentEncoder').text == encoder_id
        assert field(selected, 'ArgumentEncoder').text == encoder_id
        edits = [
            ('selected-encoder-wrong-type', 'selected', 'ArgumentEncoder', table_id),
            ('selected-buffer-wrong-type', 'selected', 'argumentBuffer', handle_id),
            ('selected-offset', 'selected', 'offset', '1'),
            ('table-encoder-zero', 'table', 'ArgumentEncoder', '0'),
            ('table-encoder-wrong-type', 'table', 'ArgumentEncoder', table_id),
            ('table-resource-wrong-type', 'table', 'table', handle_id),
            ('table-index', 'table', 'index', '1'),
        ]
        names = {
            'selected': 'MTLArgumentEncoder::setArgumentBuffer',
            'table': 'MTLArgumentEncoder::setVisibleFunctionTable',
        }
        for tag, target, member, value in edits:
            variant = copy.deepcopy(tree)
            field(chunk(variant, names[target]), member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'{capture.stem} argument visible table malformed captures rejected without crash: {len(edits)} cases')


if __name__ == '__main__':
    main()
