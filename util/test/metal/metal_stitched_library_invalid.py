#!/usr/bin/env python3
"""T303 stitched-library identity, graph and dependency rejection."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t303_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-stitched-library-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'source.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        stitched = next(n for n in chunks if n.get('name') in
                        ('MTLDevice::newLibraryWithStitchedDescriptor',
                         'MTLDevice::newLibraryWithStitchedDescriptor(completionHandler)'))
        index = chunks.index(stitched)
        device = child(stitched, 'Device').text
        queue = next(child(n, 'CommandQueue').text for n in chunks if int(n.get('id')) == 1001)
        compute_function = next(child(n, 'Function').text for n in chunks if
                                n.get('name') == 'MTLLibrary::newFunctionWithName' and
                                child(n, 'FunctionName').text == 'stitched_probe')
        cases = []

        def edit(name, field, value):
            variant = copy.deepcopy(tree)
            child(variant.findall('./chunks/chunk')[index], field).text = str(value)
            cases.append((name, variant))

        for value in (0, device, queue):
            edit(f'library-{value}', 'Library', value)
        for value in (0, queue):
            edit(f'device-{value}', 'Device', value)
        for value in (0, queue, 999999999, compute_function):
            edit(f'function-{value}', 'function', value)
        for value in ('', 'X' * 129):
            edit(f'graph-name-{len(value)}', 'graphName', value)
        for value in ('', 'missing_stitchable', 'X' * 129):
            edit(f'function-name-{len(value)}', 'functionName', value)
        for value in (1, 2**32 - 1):
            edit(f'input-index-{value}', 'inputIndex', value)
        variant = copy.deepcopy(tree)
        container = variant.find('./chunks')
        container.insert(index + 1, copy.deepcopy(container[index]))
        cases.append(('duplicate-library', variant))

        with zipfile.ZipFile(str(original)[:-4]) as source:
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
    print(f'Malformed stitched library capture rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
