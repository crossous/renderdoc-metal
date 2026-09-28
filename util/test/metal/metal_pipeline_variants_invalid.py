#!/usr/bin/env python3
"""T47 pipeline IDs/options/descriptors fail cleanly, before invalid native/GPU operations."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_compute_inline_invalid import child, run


def make_cases(tree):
    cases = []
    chunks = tree.findall('./chunks/chunk')
    functions = {child(n, 'FunctionName').text: child(n, 'Function').text
                 for n in chunks if n.get('name') == 'MTLLibrary::newFunctionWithName'}
    library = next(child(n, 'Library').text for n in chunks
                   if n.get('name') == 'MTLLibrary::newFunctionWithName')

    def edit(chunk_id, field, value, occurrence=0):
        variant = copy.deepcopy(tree)
        nodes = [n for n in variant.findall('./chunks/chunk') if int(n.get('id')) == chunk_id]
        target = nodes[occurrence]
        for part in field.split('.'):
            target = target[int(part)] if part.isdigit() else child(target, part)
        if callable(value):
            value(target)
        else:
            target.text = str(value)
        tag = f'{len(cases):03d}-{chunk_id}-{field}'
        cases.append((tag, variant))
        return variant

    for chunk_id in (1022, 1024, 1025):
        resource = 'RenderPipelineState' if chunk_id == 1022 else 'ComputePipelineState'
        for value in (0, library):
            edit(chunk_id, resource, value)
        # A later creation cannot reuse an already-live pipeline ID.
        first = next(n for n in chunks if int(n.get('id')) == chunk_id)
        edit(chunk_id, resource, child(first, resource).text, occurrence=1)
        for value in (4, 7, 8, 2**63, 2**64 - 1):
            edit(chunk_id, 'optionsValue', value)
        function_field = ('descriptor.vertexFunction' if chunk_id == 1022 else
                          'descriptor.computeFunction' if chunk_id == 1025 else 'computeFunction')
        wrong_stage = functions['cs_main' if chunk_id == 1022 else 'vs_main']
        for value in (0, 999999999, library, wrong_stage):
            edit(chunk_id, function_field, value)
    for value in (library, functions['cs_main'], functions['vs_main']):
        edit(1022, 'descriptor.fragmentFunction', value)
    for chunk_id in (1022, 1025):
        edit(chunk_id, 'supported', 'false')
    for value in (0, 2, 2**64 - 1):
        edit(1025, 'descriptor.maxCallStackDepth', value)
    for value in (1025, 2**64 - 1, 16):
        # 16 is a legal pipeline limit but too small for this capture's 32-thread dispatch.
        edit(1025, 'descriptor.maxTotalThreadsPerThreadgroup', value)
    for field in ('supportIndirectCommandBuffers', 'supportAddingBinaryFunctions'):
        edit(1025, 'descriptor.' + field, 'true')
    for value in (3, 2**64 - 1):
        edit(1025, 'descriptor.buffers.0.mutability', value)
    for field in ('indexBufferIndex', 'indexType'):
        edit(1025, 'descriptor.stageInputDescriptor.' + field, 99)

    def resize(node, length):
        original = copy.deepcopy(node[0])
        for item in list(node):
            node.remove(item)
        for _ in range(length):
            node.append(copy.deepcopy(original))
    edit(1025, 'descriptor.buffers', lambda n: resize(n, 32))
    edit(1022, 'descriptor.colorAttachments', lambda n: resize(n, 9))
    for field in ('sampleCount', 'rasterSampleCount', 'maxVertexAmplificationCount'):
        for value in (0, 3, 2**64 - 1):
            edit(1022, 'descriptor.' + field, value)
    for field in ('supportAddingVertexBinaryFunctions', 'supportAddingFragmentBinaryFunctions'):
        edit(1022, 'descriptor.' + field, 'true')

    def insert_function(node):
        item = ET.SubElement(node, 'ResourceId', typename='MTLFunction', width='8')
        item.text = functions['cs_main']
    for chunk_id, links in ((1022, 'vertexLinkedFunctions'), (1022, 'fragmentLinkedFunctions'),
                            (1025, 'linkedFunctions')):
        for field in ('functions', 'binaryFunctions', 'privateFunctions'):
            edit(chunk_id, f'descriptor.{links}.{field}', insert_function)
    dispatch_id = int(next(n for n in chunks
                           if n.get('name') == 'MTLComputeCommandEncoder::dispatchThreadgroups').get('id'))
    for value in (1, 16, 31, 33, 65):
        edit(dispatch_id, 'threadsPerGroup.width', value, occurrence=2)
    threads_id = int(next(n for n in chunks
                          if n.get('name') == 'MTLComputeCommandEncoder::dispatchThreads').get('id'))
    for value in (1, 16, 31, 33):
        edit(threads_id, 'grid.width', value)
    return cases


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t47_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-pipeline-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't47.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        cases = make_cases(tree)
        for tag, variant in cases:
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            if not re.search(r'failed|invalid|unsupported|missing|Couldn.t load', message, re.I):
                raise RuntimeError(f'missing diagnostic: {tag}: {message}')
        # Exercise the two individual reflection bits as valid alternatives, not just 0 and 3.
        for option in (1, 2):
            variant = copy.deepcopy(tree)
            for node in variant.findall('./chunks/chunk'):
                if int(node.get('id')) in (1022, 1024, 1025):
                    child(node, 'optionsValue').text = str(option)
            xml = directory / f'valid-option-{option}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            valid = directory / f'valid-option-{option}.rdc'
            run(command, 'convert', '-f', xml, '-o', valid, '-c', 'rdc')
            run(command, 'replay', '--loops', '2', valid)
    print(f'T47 malformed captures rejected without crash: {len(cases)} cases; 2 valid option variants')


if __name__ == '__main__':
    main()
