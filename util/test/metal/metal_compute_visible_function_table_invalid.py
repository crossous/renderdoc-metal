#!/usr/bin/env python3
"""Reject malformed compute visible-function-table resource graphs."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t124_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-compute-visible-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't124.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        pipeline = chunk(tree, 'MTLDevice::newComputePipelineStateWithDescriptor')
        descriptor = field(pipeline, 'descriptor')
        kernel = field(descriptor, 'computeFunction').text
        linked = field(field(descriptor, 'linkedFunctions'), 'functions')
        handle = chunk(tree, 'MTLComputePipelineState::functionHandleWithFunction')
        table = chunk(tree, 'MTLComputePipelineState::newVisibleFunctionTableWithDescriptor')
        update = chunk(tree, 'MTLVisibleFunctionTable::setFunction')
        binding = chunk(tree, 'MTLComputeCommandEncoder::setVisibleFunctionTable')
        pipeline_id = field(handle, 'Pipeline').text
        handle_id = field(handle, 'Handle').text
        table_id = field(table, 'Table').text
        assert linked[0].text != kernel
        edits = [
            ('link-zero', 'link', '', '0'),
            ('link-kernel-type', 'link', '', kernel),
            ('handle-pipeline-zero', 'handle', 'Pipeline', '0'),
            ('handle-pipeline-wrong-type', 'handle', 'Pipeline', kernel),
            ('handle-id-duplicate', 'handle', 'Handle', pipeline_id),
            ('handle-function-zero', 'handle', 'function', '0'),
            ('handle-function-wrong-type', 'handle', 'function', pipeline_id),
            ('table-pipeline-zero', 'table', 'Pipeline', '0'),
            ('table-id-duplicate', 'table', 'Table', handle_id),
            ('table-count-zero', 'table', 'count', '0'),
            ('table-count-huge', 'table', 'count', '65537'),
            ('update-table-wrong-type', 'update', 'Table', handle_id),
            ('update-handle-wrong-type', 'update', 'function', table_id),
            ('update-index', 'update', 'index', '1'),
            ('binding-table-wrong-type', 'binding', 'table', handle_id),
            ('binding-index', 'binding', 'index', '31'),
        ]
        names = {
            'handle': 'MTLComputePipelineState::functionHandleWithFunction',
            'table': 'MTLComputePipelineState::newVisibleFunctionTableWithDescriptor',
            'update': 'MTLVisibleFunctionTable::setFunction',
            'binding': 'MTLComputeCommandEncoder::setVisibleFunctionTable',
        }
        for tag, target, member, value in edits:
            variant = copy.deepcopy(tree)
            if target == 'link':
                field(field(field(chunk(variant,
                    'MTLDevice::newComputePipelineStateWithDescriptor'), 'descriptor'),
                    'linkedFunctions'), 'functions')[0].text = value
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
    print(f'T124 compute visible-table malformed captures rejected without crash: {len(edits)} cases')


if __name__ == '__main__':
    main()
