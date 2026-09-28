#!/usr/bin/env python3
"""T50: malformed CPU read metadata and synchronization must fail before native dispatch."""
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
    def identity(chunk_id, field):
        return child(next(n for n in chunks if int(n.get('id')) == chunk_id), field).text
    wrong_textures = [0, 999999999, identity(1000, 'Device'), identity(1001, 'CommandQueue'),
                      identity(1004, 'Buffer'), identity(1056, 'BlitCommandEncoder'),
                      identity(1044, 'CommandBuffer')]
    wrong_encoders = wrong_textures[:5] + [identity(1008, 'Texture'), identity(1044, 'CommandBuffer')]
    reads = [i for i, n in enumerate(chunks) if int(n.get('id')) in (1072, 1073)]
    syncs = [i for i, n in enumerate(chunks) if int(n.get('id')) == 1205]
    assert len(reads) == 4 and len(syncs) == 3
    cases = []
    def edit(index, field, value):
        variant = copy.deepcopy(tree)
        target = variant.findall('./chunks/chunk')[index]
        for part in field.split('.'):
            target = child(target, part)
        target.text = str(value)
        cases.append((f'case-{len(cases):03d}-{index}-{field}-{value}', variant))
        return variant
    maximum = 2**64 - 1
    dimensions = [(5, 3, 1), (9, 5, 1), (7, 5, 1), (8, 4, 2)]
    for index, dims in zip(reads, dimensions):
        for value in wrong_textures:
            edit(index, 'Texture', value)
        width = int(child(child(chunks[index], 'region'), 'size').find("./uint[@name='width']").text)
        for value in (2, maximum):
            edit(index, 'level', value)
        for value in (0, width * 4 - 4, width * 4 + 1, maximum - 3):
            edit(index, 'bytesPerRow', value)
        for axis in ('width', 'height', 'depth'):
            for value in (0, maximum):
                edit(index, 'region.size.' + axis, value)
        for axis, limit in zip(('x', 'y', 'z'), dims):
            for value in (limit, maximum):
                edit(index, 'region.origin.' + axis, value)
        for axis, limit in zip(('width', 'height'), dims):
            edit(index, 'region.size.' + axis, limit)
        if int(chunks[index].get('id')) == 1073:
            for value in (2, maximum):
                edit(index, 'slice', value)
    for value in (0, 32, 63, 65, maximum - 31):
        edit(reads[-1], 'bytesPerImage', value)
    for index in syncs:
        for value in wrong_textures:
            edit(index, 'texture', value)
        for value in wrong_encoders:
            edit(index, 'BlitCommandEncoder', value)
        for field in ('slice', 'level'):
            for value in (2, maximum):
                edit(index, field, value)
        # A real but CPU-shared texture is not a legal synchronizeTexture target.
        edit(index, 'texture', child(chunks[reads[-1]], 'Texture').text)
    variant = copy.deepcopy(tree)
    nodes = variant.find('./chunks')
    sync = list(nodes)[syncs[0]]
    nodes.remove(sync)
    end = next(n for n in nodes if int(n.get('id')) == 1200)
    nodes.insert(list(nodes).index(end) + 1, sync)
    cases.append(('synchronize-after-endEncoding', variant))
    # Remove CPU uploads before changing the read-only volume to Private. This avoids reaching
    # unrelated replaceRegion native calls with an intentionally incompatible storage mode.
    variant = copy.deepcopy(tree)
    nodes = variant.find('./chunks')
    volume = child(chunks[reads[-1]], 'Texture').text
    for n in list(nodes):
        if int(n.get('id')) in (1074, 1075) and child(n, 'Texture').text == volume:
            nodes.remove(n)
        if int(n.get('id')) == 1008 and child(n, 'Texture').text == volume:
            descriptor = child(n, 'descriptor')
            child(descriptor, 'storageMode').text = '2'
            child(descriptor, 'resourceOptions').text = '32'
    cases.append(('getBytes-private', variant))
    return cases


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t50_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-texture-readback-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't50.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        cases = make_cases(tree)
        def write_variant(name, variant):
            xml = directory / f'{name}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / f'{name}.rdc'
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            return rdc
        for name, variant in cases:
            invalid = write_variant(name, variant)
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            if not re.search(r'failed|invalid|unsupported|missing|Couldn.t load', message, re.I):
                raise RuntimeError(f'missing diagnostic: {name}: {message}')
        # Metadata is not the source of the consumer GPU data. Legal edits and removing CPU reads
        # entirely must remain replayable without any host pointers or output byte payloads.
        for name in ('no-cpu-reads', 'tight-layout'):
            variant = copy.deepcopy(tree)
            nodes = variant.find('./chunks')
            for n in list(nodes):
                if int(n.get('id')) not in (1072, 1073):
                    continue
                if name == 'no-cpu-reads':
                    nodes.remove(n)
                else:
                    size = child(child(n, 'region'), 'size')
                    row = int(child(size, 'width').text) * 4
                    child(n, 'bytesPerRow').text = str(row)
                    if int(n.get('id')) == 1073:
                        child(n, 'bytesPerImage').text = str(row * int(child(size, 'height').text))
            run(command, 'replay', '--loops', '3', write_variant(name, variant))
    print(f'T50 malformed captures rejected without crash: {len(cases)} cases; 2 legal variants passed')


if __name__ == '__main__':
    main()
