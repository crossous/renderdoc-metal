#!/usr/bin/env python3
"""T53: reject corrupt ICB GPU commands before native calls; preserve legal empty/no-op cases."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_compute_inline_invalid import child, run


def make_cases(tree):
    chunks = tree.findall('./chunks/chunk')
    cases = []
    def resource(chunk_id, field):
        return child(next(n for n in chunks if int(n.get('id')) == chunk_id), field).text
    def edit(index, field, value):
        variant = copy.deepcopy(tree)
        target = variant.findall('./chunks/chunk')[index]
        for name in field.split('.'):
            target = child(target, name)
        target.text = str(value)
        cases.append((f'{len(cases):03d}-{index}-{field}', variant))
        return variant
    wrong = [0, 999999999, resource(1000, 'Device'), resource(1001, 'CommandQueue'),
             resource(1005, 'Buffer'), resource(1010, 'Texture'), resource(1021, 'RenderPipelineState')]
    gpu = [i for i, n in enumerate(chunks) if int(n.get('id')) in (1224, 1225, 1226)]
    resets = [i for i, n in enumerate(chunks) if int(n.get('id')) == 1270]
    icb = resource(1031, 'IndirectCommandBuffer')
    encoder = child(chunks[gpu[0]], 'BlitCommandEncoder').text
    for i, node in enumerate(chunks):
        if int(node.get('id')) == 1031:
            for value in (16, 32, 48, 240): edit(i, 'options', value)
    for i in gpu:
        chunk_id = int(chunks[i].get('id'))
        for value in wrong + [icb]:
            edit(i, 'BlitCommandEncoder', value)
        fields = ['source', 'destination'] if chunk_id == 1225 else [
            'buffer' if chunk_id == 1224 else 'indirectCommandBuffer']
        for field in fields:
            for value in wrong + [encoder]:
                edit(i, field, value)
        prefix = 'sourceRange' if chunk_id == 1225 else 'range'
        for field, value in [('location', 6), ('location', 2**64 - 1),
                             ('length', 7), ('length', 2**64 - 1)]:
            edit(i, prefix + '.' + field, value)
        if chunk_id == 1225:
            for value in (6, 2**64 - 1): edit(i, 'destinationIndex', value)
        if child(chunks[i], 'BlitCommandEncoder').text != encoder:
            edit(i, 'BlitCommandEncoder', encoder)
    for i in resets:
        for value in wrong + [encoder, icb]: edit(i, 'IndirectRenderCommand', value)
    copies = [i for i in gpu if int(chunks[i].get('id')) == 1225]
    # Overlapping same-ICB copies and overlapping optimization ranges are not sent to Metal.
    edit(copies[1], 'destinationIndex', 2)
    optimizations = [i for i in gpu if int(chunks[i].get('id')) == 1226]
    edit(optimizations[1], 'range.location', 0)
    edit(optimizations[2], 'range.location', 0)
    # Move a command after endEncoding: the live object exists but is no longer the active encoder.
    variant = copy.deepcopy(tree)
    nodes = variant.find('./chunks')
    target = list(nodes)[gpu[0]]
    nodes.remove(target)
    end = next(n for n in nodes if n.get('name') == 'MTLBlitCommandEncoder::endEncoding')
    nodes.insert(list(nodes).index(end) + 1, target)
    cases.append(('ended-encoder', variant))
    return cases


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t53_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-icb-operations-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        def save(name, variant):
            xml = directory / f'{name}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / f'{name}.rdc'
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            return rdc
        cases = make_cases(tree)
        for name, variant in cases:
            message = run(command, 'replay', '--loops', '1', save(name, variant), success=False)
            if not re.search(r'failed|invalid|unsupported|Couldn.t load', message, re.I):
                raise RuntimeError(f'missing diagnostic: {name}: {message}')
        for mode in ('zero-length', 'remove-optimizations', 'reset-entire-destination'):
            variant = copy.deepcopy(tree)
            nodes = variant.find('./chunks')
            gpu = [n for n in nodes if int(n.get('id')) in (1224, 1225, 1226)]
            if mode == 'zero-length':
                for n in gpu:
                    child(child(n, 'sourceRange' if int(n.get('id')) == 1225 else 'range'), 'length').text = '0'
            elif mode == 'remove-optimizations':
                for n in gpu:
                    if int(n.get('id')) == 1226: nodes.remove(n)
            else:
                reset = next(n for n in gpu if int(n.get('id')) == 1224)
                child(child(reset, 'range'), 'length').text = '6'
            run(command, 'replay', '--loops', '3', save(mode, variant))
        print(f'T53 malformed captures rejected without crash/hang: {len(cases)} cases; 3 legal variants passed')


if __name__ == '__main__':
    main()
