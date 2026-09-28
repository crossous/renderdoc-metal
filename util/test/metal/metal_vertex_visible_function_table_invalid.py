#!/usr/bin/env python3
"""Reject malformed vertex-stage visible function table resource links."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t122_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-vertex-visible-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't122.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        pipeline = chunk(tree, 'MTLDevice::newRenderPipelineStateWithDescriptor')
        descriptor = field(pipeline, 'descriptor')
        vertex = field(descriptor, 'vertexFunction').text
        linked = field(field(descriptor, 'vertexLinkedFunctions'), 'functions')
        visible = linked[0].text
        handle = chunk(tree, 'MTLRenderPipelineState::functionHandleWithFunction')
        table = chunk(tree, 'MTLRenderPipelineState::newVisibleFunctionTableWithDescriptor')
        update = chunk(tree, 'MTLVisibleFunctionTable::setFunction')
        binding = chunk(tree, 'MTLRenderCommandEncoder::setVertexVisibleFunctionTable')
        handle_id = field(handle, 'Handle').text
        table_id = field(table, 'Table').text
        edits = [
            ('link-vertex-type', 'link', '', vertex),
            ('handle-stage-fragment', 'handle', 'stageValue', '2'),
            ('handle-function-zero', 'handle', 'function', '0'),
            ('table-stage-fragment', 'table', 'stageValue', '2'),
            ('table-count-zero', 'table', 'count', '0'),
            ('update-function-wrong-type', 'update', 'function', table_id),
            ('binding-table-wrong-type', 'binding', 'table', handle_id),
            ('binding-index', 'binding', 'index', '31'),
        ]
        assert visible != vertex
        names = {
            'handle': 'MTLRenderPipelineState::functionHandleWithFunction',
            'table': 'MTLRenderPipelineState::newVisibleFunctionTableWithDescriptor',
            'update': 'MTLVisibleFunctionTable::setFunction',
            'binding': 'MTLRenderCommandEncoder::setVertexVisibleFunctionTable',
        }
        for tag, target, member, value in edits:
            variant = copy.deepcopy(tree)
            if target == 'link':
                field(field(field(chunk(variant,
                    'MTLDevice::newRenderPipelineStateWithDescriptor'), 'descriptor'),
                    'vertexLinkedFunctions'), 'functions')[0].text = value
            else:
                field(chunk(variant, names[target]), member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'T122 vertex visible-table malformed captures rejected without crash: {len(edits)} cases')


if __name__ == '__main__':
    main()
