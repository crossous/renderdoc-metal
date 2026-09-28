#!/usr/bin/env python3
"""T51/T52: reject malformed async results and event waits without crashes or GPU hangs."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_compute_inline_invalid import child, run


def make_cases(tree, fixture):
    chunks = tree.findall('./chunks/chunk')
    cases = []
    def node_id(chunk_id, field):
        return child(next(n for n in chunks if int(n.get('id')) == chunk_id), field).text
    def edit(index, field, value):
        variant = copy.deepcopy(tree)
        target = variant.findall('./chunks/chunk')[index]
        for part in field.split('.'):
            target = target[int(part)] if part.isdigit() else child(target, part)
        target.text = str(value)
        cases.append((f'{len(cases):03d}-{index}-{field}', variant))
        return variant
    if fixture == 51:
        functions = {child(n, 'FunctionName').text: child(n, 'Function').text
                     for n in chunks if int(n.get('id')) == 1039}
        library = node_id(1264, 'Library')
        index = next(i for i, n in enumerate(chunks) if int(n.get('id')) == 1264)
        edit(index, 'supported', 'false')
        edit(index, 'source', 'not valid Metal source!')
        for value in (0, 999999999, node_id(1001, 'CommandQueue')):
            edit(index, 'Device', value)
        for value in (0, node_id(1000, 'Device'), node_id(1001, 'CommandQueue')):
            edit(index, 'Library', value)
        for i, node in enumerate(chunks):
            chunk_id = int(node.get('id'))
            if chunk_id not in range(1265, 1270):
                continue
            render = chunk_id in (1265, 1266)
            descriptor = render or chunk_id == 1269
            resource = 'RenderPipelineState' if render else 'ComputePipelineState'
            for value in (0, library):
                edit(i, resource, value)
            for value in (4, 7, 2**64 - 1):
                edit(i, 'optionsValue', value)
            field = 'descriptor.vertexFunction' if render else (
                'descriptor.computeFunction' if descriptor else 'computeFunction')
            for value in (0, 999999999, library, node_id(1000, 'Device'),
                          functions['cs_async' if render else 'vs_async']):
                edit(i, field, value)
            if descriptor:
                edit(i, 'supported', 'false')
            if render:
                for value in (library, functions['cs_async'], functions['vs_async']):
                    edit(i, 'descriptor.fragmentFunction', value)
                for field in ('sampleCount', 'rasterSampleCount', 'maxVertexAmplificationCount'):
                    for value in (0, 3, 2**64 - 1):
                        edit(i, 'descriptor.' + field, value)
                for field in ('supportAddingVertexBinaryFunctions', 'supportAddingFragmentBinaryFunctions'):
                    edit(i, 'descriptor.' + field, 'true')
            if chunk_id == 1269:
                for value in (0, 2, 2**64 - 1):
                    edit(i, 'descriptor.maxCallStackDepth', value)
                for value in (16, 1025, 2**64 - 1):
                    edit(i, 'descriptor.maxTotalThreadsPerThreadgroup', value)
                for field in ('supportIndirectCommandBuffers', 'supportAddingBinaryFunctions'):
                    edit(i, 'descriptor.' + field, 'true')
                for field in ('indexBufferIndex', 'indexType'):
                    edit(i, 'descriptor.stageInputDescriptor.' + field, 99)
        dispatch = [i for i, n in enumerate(chunks)
                    if n.get('name') == 'MTLComputeCommandEncoder::dispatchThreadgroups'][-1]
        for value in (1, 16, 31, 33, 65):
            edit(dispatch, 'threadsPerGroup.width', value)
    else:
        creations = [i for i, n in enumerate(chunks) if int(n.get('id')) == 1032]
        commands = [i for i, n in enumerate(chunks) if int(n.get('id')) in (1062, 1063)]
        signals = [i for i in commands if int(chunks[i].get('id')) == 1063]
        waits = [i for i in commands if int(chunks[i].get('id')) == 1062]
        assert len(creations) == 2 and len(commands) == 6
        event = child(chunks[creations[0]], 'Event').text
        wrong = [0, 999999999, node_id(1000, 'Device'), node_id(1001, 'CommandQueue'),
                 node_id(1004, 'Buffer'), node_id(1056, 'BlitCommandEncoder'), node_id(1010, 'Texture')]
        cb = node_id(1044, 'CommandBuffer')
        for i in commands:
            for value in wrong + [cb]:
                edit(i, 'event', value)
            for value in wrong + [event]:
                edit(i, 'CommandBuffer', value)
        for i in waits:
            for value in (int(child(chunks[i], 'value').text) + 1, 2**64 - 1):
                edit(i, 'value', value)
        for i in signals:
            edit(i, 'value', 0)
        previous = child(chunks[signals[0]], 'value').text
        edit(signals[-1], 'value', previous)
        edit(signals[-1], 'value', int(previous) - 1)
        for i in creations:
            for value in (0, node_id(1000, 'Device'), node_id(1001, 'CommandQueue')):
                edit(i, 'Event', value)
        edit(creations[1], 'Event', event)
        # Wrong-current command buffer must fail even though it is a real, previously used object.
        for i in commands[1:]:
            edit(i, 'CommandBuffer', cb)
        variant = copy.deepcopy(tree)
        nodes = variant.find('./chunks')
        for n in list(nodes):
            if int(n.get('id')) == 1063: nodes.remove(n)
        cases.append(('wait-without-any-captured-signal', variant))
        for name, target_id, after in [('active-encoder', 1209, False), ('committed-buffer', 1048, True)]:
            # First fill ID is looked up by method, not assumed to match a particular old chunk ID.
            variant = copy.deepcopy(tree)
            nodes = variant.find('./chunks')
            signal = list(nodes)[signals[0]]
            nodes.remove(signal)
            target = next(n for n in nodes if (n.get('name') == 'MTLBlitCommandEncoder::fillBuffer'
                          if name == 'active-encoder' else int(n.get('id')) == target_id))
            nodes.insert(list(nodes).index(target) + int(after), signal)
            cases.append((name, variant))
    return cases


def main():
    if len(sys.argv) != 4:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t51_capture.rdc t52_capture.rdc')
    command = pathlib.Path(sys.argv[1])
    total = 0
    with tempfile.TemporaryDirectory(prefix='metal-async-event-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        for fixture, capture in zip((51, 52), map(pathlib.Path, sys.argv[2:])):
            original = directory / f't{fixture}.zip.xml'
            run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
            tree = ET.parse(original)
            cases = make_cases(tree, fixture)
            def write_variant(name, variant):
                xml = directory / f't{fixture}-{name}.zip.xml'
                variant.write(xml, encoding='unicode', xml_declaration=True)
                shutil.copyfile(str(original)[:-4], str(xml)[:-4])
                rdc = directory / f't{fixture}-{name}.rdc'
                run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
                return rdc
            for name, variant in cases:
                message = run(command, 'replay', '--loops', '1', write_variant(name, variant), success=False)
                if not re.search(r'failed|invalid|unsupported|missing|Couldn.t load', message, re.I):
                    raise RuntimeError(f'missing diagnostic: {fixture}-{name}: {message}')
            for option in (1, 2) if fixture == 51 else (0, 1):
                variant = copy.deepcopy(tree)
                for node in variant.findall('./chunks/chunk'):
                    chunk_id = int(node.get('id'))
                    if fixture == 51 and chunk_id in range(1265, 1270):
                        child(node, 'optionsValue').text = str(option)
                    elif fixture == 52 and chunk_id == 1062:
                        child(node, 'value').text = str(option)
                run(command, 'replay', '--loops', '3', write_variant(f'legal-{option}', variant))
            print(f'T{fixture} malformed captures rejected without crash/hang: {len(cases)} cases; 2 legal variants passed')
            total += len(cases)
    print(f'T51/T52 malformed total: {total}')


if __name__ == '__main__':
    main()
