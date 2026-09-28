#!/usr/bin/env python3
"""T106 reject malformed compute descriptor dynamic-library preload IDs."""
import copy
import pathlib
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_dynamic_library_invalid import child, run


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t106_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-dynamic-preload-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        index = next(i for i, c in enumerate(chunks) if c.get('name') in (
            'MTLDevice::newComputePipelineStateWithDescriptor',
            'MTLDevice::newComputePipelineStateWithDescriptor(completionHandler)'))
        device = child(next(c for c in chunks if c.get('name') ==
                            'MTLDevice::newLibraryWithSource'), 'Device').text
        source = child(next(c for c in chunks if c.get('name') ==
                            'MTLDevice::newLibraryWithSource'), 'Library').text
        pipeline = child(chunks[index], 'ComputePipelineState').text
        cases = []
        for tag, value in [('zero','0'), ('device',device), ('source',source),
                           ('pipeline',pipeline), ('unknown','999999999')]:
            variant = copy.deepcopy(tree)
            descriptor = child(variant.findall('./chunks/chunk')[index], 'descriptor')
            child(descriptor, 'preloadedLibraries')[0].text = value
            cases.append((tag, variant))
        variant = copy.deepcopy(tree)
        child(variant.findall('./chunks/chunk')[index], 'supported').text = 'false'
        cases.append(('unsupported', variant))
        for tag, variant in cases:
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert 'Failed to process Metal chunk' in message or 'Failed to replay Metal chunk' in message, (tag, message)
    number = 114 if chunks[index].get('name').endswith('(completionHandler)') else 106
    print(f'T{number} invalid compute preloaded-library references rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
