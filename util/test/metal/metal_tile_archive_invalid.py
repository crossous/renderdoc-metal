#!/usr/bin/env python3
"""T309: reject malformed tile-function archive records without native crashes."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t309_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-tile-archive-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        source_xml = directory / 'source.zip.xml'
        run(command, 'convert', '-f', capture, '-o', source_xml, '-c', 'zip.xml')
        tree = ET.parse(source_xml)
        chunks = tree.findall('./chunks/chunk')
        archive = next(n for n in chunks if n.get('name') ==
                       'MTLDevice::newBinaryArchiveWithDescriptor')
        tile = next(n for n in chunks if n.get('name') ==
                    'MTLBinaryArchive::addTileRenderPipelineFunctionsWithDescriptor')
        draw = next(n for n in chunks if n.get('name') ==
                    'MTLDevice::newRenderPipelineStateWithDescriptor')
        device = child(archive, 'Device').text
        archive_id = child(archive, 'Archive').text
        queue = next(child(n, 'CommandQueue').text for n in chunks if int(n.get('id')) == 1001)
        other_function = next(child(n, 'Function').text for n in chunks if
                              n.get('name') == 'MTLLibrary::newFunctionWithName' and
                              child(n, 'FunctionName').text == 'tile_vs')
        cases = []

        def edit(name, original, path, value):
            variant = copy.deepcopy(tree)
            node = variant.findall('./chunks/chunk')[chunks.index(original)]
            for part in path.split('.'):
                node = child(node, part)
            node.text = str(value)
            cases.append((name, variant))

        for value in (0, device, queue, 999999999):
            edit(f'archive-id-{value}', archive, 'Archive', value)
        for value in (0, queue):
            edit(f'archive-device-{value}', archive, 'Device', value)
        edit('archive-nonempty', archive, 'emptyArchive', 'false')
        for value in (0, device, queue, 999999999):
            edit(f'tile-archive-{value}', tile, 'BinaryArchive', value)
        for value in (0, device, queue, other_function, 999999999):
            edit(f'tile-function-{value}', tile, 'function', value)
        for value in (0, 2):
            edit(f'tile-samples-{value}', tile, 'sampleCount', value)
        edit('tile-threads-oversize', tile, 'maxThreads', 1025)
        for mode in ('first-invalid', 'second-active', 'short', 'long'):
            variant = copy.deepcopy(tree)
            colors = child(variant.findall('./chunks/chunk')[chunks.index(tile)], 'colors')
            if mode == 'first-invalid':
                colors[0].text = '0'
            elif mode == 'second-active':
                colors[1].text = '80'
            elif mode == 'short':
                colors.remove(colors[-1])
            else:
                ET.SubElement(colors, 'uint', typename='uint32_t', width='4').text = '0'
            cases.append((f'colors-{mode}', variant))
        edit('draw-options', draw, 'optionsValue', 8)
        tile_pipeline = next(n for n in chunks if n.get('name') ==
                             'MTLDevice::newRenderPipelineStateWithTileDescriptor')
        tile_archives = tile_pipeline.find("./array[@name='binaryArchives']")
        if tile_archives is not None and len(tile_archives):
            for value in (0, device, queue, 999999999):
                variant = copy.deepcopy(tree)
                pipeline = variant.findall('./chunks/chunk')[chunks.index(tile_pipeline)]
                child(pipeline, 'binaryArchives')[0].text = str(value)
                cases.append((f'tile-pipeline-archive-{value}', variant))
            edit('tile-pipeline-options', tile_pipeline, 'options', 8)
            edit('tile-pipeline-unsupported', tile_pipeline, 'supported', 'false')
        variant = copy.deepcopy(tree)
        descriptor = child(variant.findall('./chunks/chunk')[chunks.index(draw)], 'descriptor')
        child(descriptor, 'binaryArchives')[0].text = '0'
        cases.append(('draw-archive-missing', variant))

        with zipfile.ZipFile(str(source_xml)[:-4]) as source:
            entries = [(entry, source.read(entry.filename)) for entry in source.infolist()]
        for name, variant in cases:
            xml = directory / f'{name}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            with zipfile.ZipFile(str(xml)[:-4], 'w') as target:
                for entry, data in entries:
                    target.writestr(entry, data)
            invalid = directory / f'{name}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            if not re.search(r'failed|invalid|unsupported|missing|Couldn.t load', message, re.I):
                raise RuntimeError(f'missing diagnostic for {name}: {message}')
    print(f'Malformed tile archive capture rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
