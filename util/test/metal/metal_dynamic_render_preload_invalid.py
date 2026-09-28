#!/usr/bin/env python3
"""T107 reject malformed render descriptor dynamic-library preload IDs."""
import copy
import pathlib
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from metal_dynamic_library_invalid import child, run


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t107_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-dynamic-render-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        index = next(i for i, c in enumerate(chunks) if c.get('name') in (
            'MTLDevice::newRenderPipelineStateWithDescriptor',
            'MTLDevice::newRenderPipelineStateWithDescriptor(completionHandler)',
            'MTLDevice::newRenderPipelineStateWithDescriptor(options, completionHandler)'))
        source_chunk = next(c for c in chunks if c.get('name') ==
                            'MTLDevice::newLibraryWithSource')
        device = child(source_chunk,'Device').text
        source = child(source_chunk,'Library').text
        pipeline = child(chunks[index],'RenderPipelineState').text
        descriptor = child(chunks[index], 'descriptor')
        stage = ('vertexPreloadedLibraries' if len(child(descriptor,'vertexPreloadedLibraries'))
                 else 'fragmentPreloadedLibraries')
        cases = []
        for tag, value in [('zero','0'), ('device',device), ('source',source),
                           ('pipeline',pipeline), ('unknown','999999999')]:
            variant = copy.deepcopy(tree)
            descriptor = child(variant.findall('./chunks/chunk')[index], 'descriptor')
            child(descriptor,stage)[0].text = value
            cases.append((tag,variant))
        for tag, variant in cases:
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml,encoding='unicode',xml_declaration=True)
            shutil.copyfile(str(original)[:-4],str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command,'convert','-f',xml,'-o',invalid,'-c','rdc')
            message = run(command,'replay','--loops','1',invalid,success=False)
            assert 'Failed to process Metal chunk' in message or 'Failed to replay Metal chunk' in message, (tag,message)
    chunk_name = chunks[index].get('name')
    number = (113 if 'options, completionHandler' in chunk_name else
              115 if '(completionHandler)' in chunk_name and stage.startswith('vertex') else
              112 if '(completionHandler)' in chunk_name else
              109 if any(c.get('name') == 'supported' for c in chunks[index]) else
              108 if stage.startswith('vertex') else 107)
    print(f'T{number} invalid render preloaded-library references rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
