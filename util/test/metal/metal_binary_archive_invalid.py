#!/usr/bin/env python3
"""T301 archive payload, identity and pipeline dependency rejection."""
import copy
import pathlib
import re
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from metal_compute_inline_invalid import child, run


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t301_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-binary-archive-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'source.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        archive = next(n for n in chunks if n.get('name') ==
                       'MTLDevice::newBinaryArchiveWithDescriptor')
        pipelines = [n for n in chunks if n.get('name') in
                     ('MTLDevice::newComputePipelineStateWithDescriptor',
                      'MTLDevice::newRenderPipelineStateWithDescriptor',
                      'MTLDevice::newComputePipelineStateWithDescriptor(completionHandler)',
                      'MTLDevice::newRenderPipelineStateWithDescriptor(options, completionHandler)')]
        if len(pipelines) != 2:
            raise RuntimeError('expected one compute and one render pipeline')
        device = child(archive, 'Device').text
        queue = next(child(n, 'CommandQueue').text for n in chunks if int(n.get('id')) == 1001)
        cases = []

        def edit(name, index, path, value):
            variant = copy.deepcopy(tree)
            node = variant.findall('./chunks/chunk')[index]
            for part in path.split('.'):
                node = child(node, part)
            node.text = str(value)
            cases.append((name, variant, {}))

        archive_index = chunks.index(archive)
        for value in (0, device, queue):
            edit(f'archive-id-{value}', archive_index, 'Archive', value)
        for value in (0, queue):
            edit(f'archive-device-{value}', archive_index, 'Device', value)
        for mode in ('empty', 'truncated', 'bad-magic'):
            variant = copy.deepcopy(tree)
            buffer = child(variant.findall('./chunks/chunk')[archive_index], 'data')
            entry = f'{int(buffer.text):06d}'
            with zipfile.ZipFile(str(original)[:-4]) as source:
                payload = source.read(entry)
            changed = b'' if mode == 'empty' else payload[:3] if mode == 'truncated' else b'BAD!' + payload[4:]
            buffer.set('byteLength', str(len(changed)))
            cases.append((f'payload-{mode}', variant, {entry: changed}))
        for pipeline in pipelines:
            index = chunks.index(pipeline)
            tag = 'compute' if 'Compute' in pipeline.get('name') else 'render'
            for value in (0, queue, 999999999):
                variant = copy.deepcopy(tree)
                descriptor = child(variant.findall('./chunks/chunk')[index], 'descriptor')
                array = child(descriptor, 'binaryArchives')
                array[0].text = str(value)
                cases.append((f'{tag}-archive-{value}', variant, {}))
            edit(f'{tag}-options', index, 'optionsValue', 8)
            edit(f'{tag}-unsupported', index, 'supported', 'false')
        variant = copy.deepcopy(tree)
        container = variant.find('./chunks')
        node = container[archive_index]
        container.insert(archive_index + 1, copy.deepcopy(node))
        cases.append(('duplicate-archive', variant, {}))

        with zipfile.ZipFile(str(original)[:-4]) as source:
            source_entries = [(entry, source.read(entry.filename)) for entry in source.infolist()]
        for name, variant, overrides in cases:
            xml = directory / f'{name}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            with zipfile.ZipFile(str(xml)[:-4], 'w') as target:
                for entry, data in source_entries:
                    target.writestr(entry, overrides.get(entry.filename, data))
            invalid = directory / f'{name}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            if not re.search(r'failed|invalid|unsupported|missing|Couldn.t load', message, re.I):
                raise RuntimeError(f'missing diagnostic for {name}: {message}')
    print(f'Malformed binary archive capture rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
