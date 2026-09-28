#!/usr/bin/env python3
"""Reject malformed empty-archive creation and function mutations (T305/T306)."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t305_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-archive-mutation-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        source_xml = directory / 'source.zip.xml'
        run(command, 'convert', '-f', capture, '-o', source_xml, '-c', 'zip.xml')
        tree = ET.parse(source_xml)
        chunks = tree.findall('./chunks/chunk')
        archive = next(n for n in chunks if n.get('name') ==
                       'MTLDevice::newBinaryArchiveWithDescriptor')
        compute = next(n for n in chunks if n.get('name') ==
                       'MTLBinaryArchive::addComputePipelineFunctionsWithDescriptor')
        render = next(n for n in chunks if n.get('name') ==
                      'MTLBinaryArchive::addRenderPipelineFunctionsWithDescriptor')
        device = child(archive, 'Device').text
        archive_id = child(archive, 'Archive').text
        queue = next(child(n, 'CommandQueue').text for n in chunks if int(n.get('id')) == 1001)
        functions = {child(n, 'FunctionName').text: child(n, 'Function').text
                     for n in chunks if n.get('name') == 'MTLLibrary::newFunctionWithName'}
        compute_function = child(child(compute, 'descriptor'), 'computeFunction').text
        vertex_function = child(child(render, 'descriptor'), 'vertexFunction').text
        fragment_function = child(child(render, 'descriptor'), 'fragmentFunction').text
        assert {compute_function, vertex_function, fragment_function} <= set(functions.values())
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
        edit('empty-flag-false', archive, 'emptyArchive', 'false')
        for target, tag in ((compute, 'compute'), (render, 'render')):
            for value in (0, device, queue, 999999999):
                edit(f'{tag}-archive-{value}', target, 'BinaryArchive', value)
            edit(f'{tag}-unrelated-library', target,
                 'descriptor.computeFunction' if tag == 'compute' else 'descriptor.vertexFunction',
                 device)
            edit(f'{tag}-missing-function', target,
                 'descriptor.computeFunction' if tag == 'compute' else 'descriptor.vertexFunction',
                 999999999)
            edit(f'{tag}-wrong-stage', target,
                 'descriptor.computeFunction' if tag == 'compute' else 'descriptor.vertexFunction',
                 vertex_function if tag == 'compute' else compute_function)
            edit(f'{tag}-binary-linking', target,
                 'descriptor.supportAddingBinaryFunctions' if tag == 'compute' else
                 'descriptor.supportAddingVertexBinaryFunctions', 'true')
            variant = copy.deepcopy(tree)
            descriptor = child(variant.findall('./chunks/chunk')[chunks.index(target)], 'descriptor')
            archives = child(descriptor, 'binaryArchives')
            ET.SubElement(archives, 'ResourceId', typename='MTLBinaryArchive', width='8').text = archive_id
            cases.append((f'{tag}-recursive-archive', variant))
        edit('render-fragment-wrong-stage', render, 'descriptor.fragmentFunction', compute_function)
        edit('render-fragment-missing', render, 'descriptor.fragmentFunction', 999999999)
        edit('compute-call-stack-zero', compute, 'descriptor.maxCallStackDepth', 0)
        edit('render-amplification-zero', render, 'descriptor.maxVertexAmplificationCount', 0)
        edit('render-samples-zero', render, 'descriptor.sampleCount', 0)
        add_function = next((n for n in chunks if n.get('name') ==
                             'MTLBinaryArchive::addFunctionWithDescriptor:library:'), None)
        if add_function is not None:
            for value in (0, device, queue, 999999999):
                edit(f'visible-archive-{value}', add_function, 'BinaryArchive', value)
            for value in (0, device, queue, compute_function, 999999999):
                edit(f'visible-library-{value}', add_function, 'library', value)
            for name in ('', 'missing_visible', 'archive_probe', 'x' * 129):
                edit(f'visible-name-{len(cases)}', add_function, 'functionName', name)
        add_library = next((n for n in chunks if n.get('name') ==
                            'MTLBinaryArchive::addLibraryWithDescriptor'), None)
        if add_library is not None:
            for value in (0, device, queue, 999999999):
                edit(f'stitched-archive-{value}', add_library, 'BinaryArchive', value)
            for value in (0, device, queue, compute_function, 999999999):
                edit(f'stitched-function-{value}', add_library, 'function', value)
            for name in ('', 'x' * 129):
                edit(f'stitched-graph-{len(cases)}', add_library, 'graphName', name)
            for name in ('', 'archive_probe', 'x' * 129):
                edit(f'stitched-name-{len(cases)}', add_library, 'functionName', name)

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
    print(f'Malformed binary archive mutation capture rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
