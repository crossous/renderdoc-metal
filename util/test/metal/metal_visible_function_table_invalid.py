#!/usr/bin/env python3
"""Malformed render visible-function-table resource graph must fail without a crash."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t120_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-visible-table-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't120.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        pipeline = chunk(tree, 'MTLDevice::newRenderPipelineStateWithDescriptor')
        descriptor = field(pipeline, 'descriptor')
        fragment = field(descriptor, 'fragmentFunction').text
        linked = field(field(descriptor, 'fragmentLinkedFunctions'), 'functions')
        visible = linked[0].text
        handle = chunk(tree, 'MTLRenderPipelineState::functionHandleWithFunction')
        table = chunk(tree, 'MTLRenderPipelineState::newVisibleFunctionTableWithDescriptor')
        update = chunk(tree, 'MTLVisibleFunctionTable::setFunction')
        binding = chunk(tree, 'MTLRenderCommandEncoder::setFragmentVisibleFunctionTable')
        pipeline_id = field(handle, 'Pipeline').text
        handle_id = field(handle, 'Handle').text
        table_id = field(table, 'Table').text
        edits = [
            ('link-null', 'link', '', '0'),
            ('link-fragment-type', 'link', '', fragment),
            ('handle-pipeline-null', 'handle', 'Pipeline', '0'),
            ('handle-pipeline-wrong-type', 'handle', 'Pipeline', visible),
            ('handle-id-duplicate', 'handle', 'Handle', pipeline_id),
            ('handle-function-null', 'handle', 'function', '0'),
            ('handle-function-wrong-type', 'handle', 'function', pipeline_id),
            ('handle-stage', 'handle', 'stageValue', '1'),
            ('table-pipeline-null', 'table', 'Pipeline', '0'),
            ('table-id-duplicate', 'table', 'Table', handle_id),
            ('table-count-zero', 'table', 'count', '0'),
            ('table-count-huge', 'table', 'count', '33'),
            ('table-stage', 'table', 'stageValue', '1'),
            ('update-table-wrong-type', 'update', 'Table', handle_id),
            ('update-handle-wrong-type', 'update', 'function', table_id),
            ('update-handle-missing', 'update', 'function', '99999999'),
            ('update-index', 'update', 'index', '1'),
            ('binding-table-wrong-type', 'binding', 'table', handle_id),
            ('binding-index', 'binding', 'index', '31'),
        ]
        names = {
            'handle': 'MTLRenderPipelineState::functionHandleWithFunction',
            'table': 'MTLRenderPipelineState::newVisibleFunctionTableWithDescriptor',
            'update': 'MTLVisibleFunctionTable::setFunction',
            'binding': 'MTLRenderCommandEncoder::setFragmentVisibleFunctionTable',
        }
        for tag, target, member, value in edits:
            variant = copy.deepcopy(tree)
            if target == 'link':
                field(field(field(chunk(variant,
                    'MTLDevice::newRenderPipelineStateWithDescriptor'), 'descriptor'),
                    'fragmentLinkedFunctions'), 'functions')[0].text = value
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
    print(f'T120 visible function table malformed captures rejected without crash: {len(edits)} cases')


if __name__ == '__main__':
    main()
