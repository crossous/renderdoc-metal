#!/usr/bin/env python3
"""Malformed fences, annotations and timed presents must fail without crashing or hanging."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET

from metal_compute_inline_invalid import child, run


def nodes(tree, chunk_id):
    return [n for n in tree.findall('./chunks/chunk') if int(n.get('id')) == chunk_id]


def make_cases(tree, number):
    cases = []
    def edit(tag, chunk_id, field, value, occurrence=0):
        variant = copy.deepcopy(tree)
        target = nodes(variant, chunk_id)[occurrence]
        for part in field.split('.'):
            target = child(target, part)
        target.text = str(value)
        cases.append((tag, variant))
    pipeline = child(nodes(tree, 1021)[0], 'RenderPipelineState').text
    if number == 44:
        for chunk_id, encoder in ((1216, 'BlitCommandEncoder'), (1217, 'BlitCommandEncoder'),
                                  (1262, 'ComputeCommandEncoder'), (1263, 'ComputeCommandEncoder'),
                                  (1151, 'RenderCommandEncoder'), (1152, 'RenderCommandEncoder')):
            for field in (encoder, 'fence'):
                for value in (0, 2**60, pipeline):
                    edit(f'{chunk_id}-{field}-{value}', chunk_id, field, value)
        for chunk_id in (1151, 1152):
            for value in (0, 4, 8, 2**63):
                edit(f'{chunk_id}-stage-{value}', chunk_id, 'stagesValue', value)
        for chunk_id, encoder in ((1216, 'BlitCommandEncoder'), (1262, 'ComputeCommandEncoder'),
                                  (1152, 'RenderCommandEncoder')):
            old = child(nodes(tree, chunk_id)[0], encoder).text
            edit(f'{chunk_id}-ended-encoder', chunk_id, encoder, old, occurrence=1)
        edit('new-fence-null', 1026, 'Fence', 0)
        edit('new-fence-type-collision', 1026, 'Fence', pipeline)
        edit('new-fence-duplicate', 1026, 'Fence', child(nodes(tree, 1026)[0], 'Fence').text, 1)
        # Remove each required producer independently; never submit an unsignalled GPU wait.
        for chunk_id, occurrence in ((1216, 0), (1262, 0), (1151, 0), (1216, 1)):
            variant = copy.deepcopy(tree)
            variant.find('./chunks').remove(nodes(variant, chunk_id)[occurrence])
            cases.append((f'missing-update-{chunk_id}-{occurrence}', variant))
        edit('wait-future-fence', 1263, 'fence', child(nodes(tree, 1026)[3], 'Fence').text)
        # A same-encoder update immediately before a wait is not a valid dependency.
        variant = copy.deepcopy(tree)
        wait = nodes(variant, 1263)[0]
        update = copy.deepcopy(nodes(variant, 1262)[0])
        child(update, 'ComputeCommandEncoder').text = child(wait, 'ComputeCommandEncoder').text
        child(update, 'fence').text = child(wait, 'fence').text
        chunks = variant.find('./chunks')
        chunks.insert(list(chunks).index(wait), update)
        cases.append(('same-encoder-dependency', variant))
    else:
        present_id = 1051 if number == 45 else 1052
        for value in (-1, 'nan', 'inf', '-inf'):
            edit(f'present-time-{value}', present_id, 'time', value)
        for field in ('CommandBuffer', 'presentedImage'):
            for value in (0, 2**60, pipeline):
                edit(f'present-{field}-{value}', present_id, field, value)
        if number == 45:
            # Last occurrence is a frame annotation, not only saved background metadata.
            for chunk_id in (1194, 1195):
                for value in (0, 2**60, pipeline):
                    edit(f'marker-{chunk_id}-{value}', chunk_id, 'Buffer', value, -1)
            for field, value in (('range.location', 173), ('range.location', 2**64 - 1),
                                 ('range.length', 173), ('range.length', 2**64 - 1),
                                 ('range.location', 171)):
                edit(f'marker-{field}-{value}', 1194, field, value, -1)
    return cases


def main():
    if len(sys.argv) != 5:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t44.rdc t45.rdc t46.rdc')
    command = pathlib.Path(sys.argv[1])
    total = 0
    with tempfile.TemporaryDirectory(prefix='metal-fence-present-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        for number, capture in enumerate(sys.argv[2:], 44):
            original = directory / f't{number}.zip.xml'
            run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
            cases = make_cases(ET.parse(original), number)
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
            print(f'T{number} malformed captures rejected without crash/hang: {len(cases)} cases', flush=True)
    print(f'T44-T46 malformed total: {total} cases')


if __name__ == '__main__':
    main()
