#!/usr/bin/env python3
"""T105 reject invalid second dynamic source and executable dependency."""
import copy
import pathlib
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_dynamic_library_invalid import child, run


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t105_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-dynamic-multi-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        sources = [i for i, c in enumerate(chunks) if c.get('name') ==
                   'MTLDevice::newLibraryWithSource']
        dynamics = [i for i, c in enumerate(chunks) if c.get('name') ==
                    'MTLDevice::newDynamicLibrary']
        assert len(sources) == 3 and len(dynamics) == 2
        first_library = child(chunks[sources[0]], 'Library').text
        first_dynamic = child(chunks[dynamics[0]], 'DynamicLibrary').text
        device = child(chunks[sources[0]], 'Device').text
        cases = []

        def edit(tag, index, field, value):
            variant = copy.deepcopy(tree)
            child(variant.findall('./chunks/chunk')[index], field).text = str(value)
            cases.append((tag, variant))

        second_source, second_dynamic, executable = sources[1], dynamics[1], sources[2]
        for tag, field, value in [
            ('source-type','libraryType','99'),
            ('source-install','installName',''),
            ('source-unsupported','supported','false'),
            ('source-id-zero','Library','0'),
            ('source-id-duplicate','Library',first_library),
            ('dynamic-unsupported','supported','false'),
            ('dynamic-id-zero','DynamicLibrary','0'),
            ('dynamic-id-duplicate','DynamicLibrary',first_dynamic),
            ('dynamic-source-zero','library','0'),
            ('dynamic-source-device','library',device),
            ('executable-unsupported','supported','false'),
        ]:
            index = second_source if tag.startswith('source-') else (
                second_dynamic if tag.startswith('dynamic-') else executable)
            edit(tag,index,field,value)
        for tag, value in [('dependency-zero','0'),
                           ('dependency-source',first_library),
                           ('dependency-device',device)]:
            variant = copy.deepcopy(tree)
            child(variant.findall('./chunks/chunk')[executable], 'dependencies')[1].text = value
            cases.append((tag, variant))
        for tag, variant in cases:
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert 'Failed to process Metal chunk' in message or 'Failed to replay Metal chunk' in message, (tag, message)
    print(f'T105 invalid second dynamic-library dependency rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
