#!/usr/bin/env python3
"""T63: four dynamic vertex-stride chunks and malformed replay inputs."""
import copy
import pathlib
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET

from metal_compute_inline_invalid import child, run


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t63_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-vertex-stride-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't63.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        ids = ('1278', '1279', '1280', '1281')
        counts = {chunk_id: sum(n.get('id') == chunk_id
                                for n in tree.findall('./chunks/chunk')) for chunk_id in ids}
        assert counts == {'1278': 2, '1279': 1, '1280': 1, '1281': 1}, counts
        batch = next(n for n in tree.findall('./chunks/chunk') if n.get('id') == '1279')
        assert [int(n.text) for n in child(batch, 'strides')] == [16, 2**64-1]
        amplification = [n for n in tree.findall('./chunks/chunk') if n.get('id') == '1109']
        assert len(amplification) == 1
        assert child(amplification[0], 'count').text == '1'
        cases = []

        def edit(chunk_id, tag, update):
            variant = copy.deepcopy(tree)
            node = next(n for n in variant.findall('./chunks/chunk')
                        if n.get('id') == chunk_id)
            update(node)
            cases.append((f'{chunk_id}-{tag}', variant))

        for chunk_id in ids:
            for identity in (0, 2**63):
                edit(chunk_id, f'encoder-{identity}',
                     lambda n, v=identity: setattr(child(n, 'RenderCommandEncoder'), 'text', str(v)))
            for stride in (0, 2049):
                edit(chunk_id, f'stride-{stride}',
                     lambda n, v=stride: setattr(child(n, 'strides')[0], 'text', str(v)))
            for location in (31, 2**63):
                edit(chunk_id, f'slot-{location}',
                     lambda n, v=location: setattr(child(child(n, 'range'), 'location'),
                                                  'text', str(v)))
            edit(chunk_id, 'range-length-32',
                 lambda n: setattr(child(child(n, 'range'), 'length'), 'text', '32'))
        for chunk_id in ('1278', '1279', '1280'):
            edit(chunk_id, 'offset-192',
                 lambda n: setattr(child(n, 'offsets')[0], 'text', '192'))
        edit('1279', 'batch-static-stride-2049',
             lambda n: setattr(child(n, 'strides')[1], 'text', '2049'))
        edit('1279', 'batch-static-on-dynamic-layout',
             lambda n: setattr(child(n, 'strides')[0], 'text', str(2**64-1)))
        edit('1279', 'batch-dynamic-on-static-layout',
             lambda n: setattr(child(n, 'strides')[1], 'text', '16'))
        for chunk_id in ('1278', '1280', '1281'):
            edit(chunk_id, 'dynamic-on-static-layout',
                 lambda n: setattr(child(child(n, 'range'), 'location'), 'text', '1'))
        edit('1279', 'batch-short-buffers',
             lambda n: child(n, 'buffers').remove(child(n, 'buffers')[1]))
        edit('1281', 'empty-inline', lambda n: child(n, 'data').clear())

        for identity in (0, 2**63):
            edit('1109', f'amplification-encoder-{identity}',
                 lambda n, v=identity: setattr(child(n, 'RenderCommandEncoder'), 'text', str(v)))
        for count in (0, 2):
            edit('1109', f'amplification-count-{count}',
                 lambda n, v=count: setattr(child(n, 'count'), 'text', str(v)))
        for field in ('viewportOffsets', 'targetOffsets'):
            edit('1109', f'amplification-{field}-1',
                 lambda n, f=field: setattr(child(n, f)[0], 'text', '1'))
            edit('1109', f'amplification-empty-{field}',
                 lambda n, f=field: child(n, f).clear())
        edit('1109', 'amplification-no-mappings-with-arrays',
             lambda n: setattr(child(n, 'hasMappings'), 'text', 'false'))

        for tag, variant in cases:
            xml = directory / (tag + '.zip.xml')
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            rdc = directory / (tag + '.rdc')
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', rdc, success=False)
            assert re.search(r'failed|invalid|unsupported|missing', message, re.I), (tag, message)
    print(f'T63: {len(cases)} malformed dynamic-stride bindings rejected without crash')


if __name__ == '__main__':
    main()
