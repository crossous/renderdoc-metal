#!/usr/bin/env python3
"""Reject malformed T42/T43 declarations and barriers before native Metal calls."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET

from metal_compute_inline_invalid import child, run


def make_cases(tree, compute):
    cases = []
    prefix = 'MTLComputeCommandEncoder::' if compute else 'MTLRenderCommandEncoder::'
    ids = (1255, 1256, 1257, 1258, 1259, 1260, 1261) if compute else (1177, 1178, 1179, 1186, 1187)
    encoder = 'ComputeCommandEncoder' if compute else 'RenderCommandEncoder'
    pipeline = next(child(node, 'RenderPipelineState').text
                    for node in tree.findall('./chunks/chunk')
                    if node.get('name') == 'MTLDevice::newRenderPipelineStateWithDescriptor')

    def edit(chunk_id, field, value, occurrence=0, index=None):
        variant = copy.deepcopy(tree)
        nodes = [node for node in variant.findall('./chunks/chunk')
                 if int(node.get('id')) == chunk_id]
        node = nodes[occurrence]
        assert node.get('name').startswith(prefix)
        target = child(node, field)
        if index is not None:
            target = target[index]
        target.text = str(value)
        tag = f'{chunk_id}-{field}-{value}-{occurrence}-{index}'
        cases.append((tag, variant))

    for chunk_id in ids:
        edit(chunk_id, encoder, 0)
    single = 1255 if compute else 1177
    batches = (1256,) if compute else (1178, 1179)
    for chunk_id in (single,) + batches:
        for value in (0, 8, 2**63):
            edit(chunk_id, 'usageValue', value)
    # Null, unresolved and existing non-resource IDs, including every element of mixed arrays.
    bad_ids = (0, 2**60, pipeline)
    for value in bad_ids:
        for occurrence in (0, 1):
            edit(single, 'resource', value, occurrence=occurrence)
        for chunk_id in batches:
            for index in range(3):
                edit(chunk_id, 'resources', value, index=index)
        edit(1258 if compute else 1187, 'resources', value, index=0)
    for value in ((0, 4, 8, 2**63) if compute else (0, 8, 2**63)):
        edit(1257 if compute else 1186, 'scopeValue', value)
    if not compute:
        for chunk_id in (1177, 1179):
            for value in (0, 4, 8, 2**63):
                edit(chunk_id, 'stagesValue', value)
        for chunk_id in (1186, 1187):
            for field in ('afterValue', 'beforeValue'):
                for value in (0, 4, 8, 2**63):
                    edit(chunk_id, field, value)
    return cases


def main():
    if len(sys.argv) != 4:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t42_capture.rdc t43_capture.rdc')
    command = pathlib.Path(sys.argv[1])
    total = 0
    with tempfile.TemporaryDirectory(prefix='metal-resource-barrier-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        for number, capture in enumerate(sys.argv[2:], 42):
            original = directory / f't{number}.zip.xml'
            run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
            cases = make_cases(ET.parse(original), number == 42)
            for tag, variant in cases:
                xml = directory / f't{number}-{tag}.zip.xml'
                variant.write(xml, encoding='unicode', xml_declaration=True)
                shutil.copyfile(str(original)[:-4], str(xml)[:-4])
                invalid = xml.with_suffix('.rdc')
                run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
                message = run(command, 'replay', '--loops', '1', invalid, success=False)
                if not re.search(r'failed|invalid|unsupported|missing|Couldn.t load', message, re.I):
                    raise RuntimeError(f'missing diagnostic: {tag}: {message}')
            total += len(cases)
            print(f'T{number} malformed captures rejected without crash: {len(cases)} cases', flush=True)
    print(f'T42/T43 malformed total: {total} cases')


if __name__ == '__main__':
    main()
