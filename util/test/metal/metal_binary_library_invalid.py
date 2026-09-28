#!/usr/bin/env python3
"""T48 library identity/payload and function failures; source-path metadata is never reopened."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t48_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-binary-library-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't48.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        queue = next(child(n, 'CommandQueue').text for n in chunks if int(n.get('id')) == 1001)
        device = next(child(n, 'Device').text for n in chunks if int(n.get('id')) == 1000)
        cases = []

        def edit(chunk_id, field, value):
            variant = copy.deepcopy(tree)
            node = next(n for n in variant.findall('./chunks/chunk') if int(n.get('id')) == chunk_id)
            child(node, field).text = str(value)
            cases.append((variant, {}))

        for chunk_id in range(1014, 1019):
            for value in (0, device, queue):
                edit(chunk_id, 'Library', value)
            for value in (0, 999999999, queue):
                edit(chunk_id, 'Device', value)
            # Alter the actual zipped bytes, not only XML's displayed length.
            for mode in ('empty', 'three-bytes', 'bad-magic', 'header-only'):
                variant = copy.deepcopy(tree)
                node = next(n for n in variant.findall('./chunks/chunk') if int(n.get('id')) == chunk_id)
                buffer = child(node, 'data')
                index = int(buffer.text)
                with zipfile.ZipFile(str(original)[:-4]) as archive:
                    payload = archive.read(f'{index:06d}')
                changed = (b'' if mode == 'empty' else payload[:3] if mode == 'three-bytes' else
                           b'BAD!' + payload[4:] if mode == 'bad-magic' else payload[:4])
                buffer.set('byteLength', str(len(changed)))
                cases.append((variant, {f'{index:06d}': changed}))
        function_id = int(next(n for n in chunks if n.get('name') == 'MTLLibrary::newFunctionWithName').get('id'))
        for value in (0, 999999999, queue):
            edit(function_id, 'Library', value)
        for value in (0, device, queue):
            edit(function_id, 'Function', value)
        for value in ('', 'missing_function'):
            edit(function_id, 'FunctionName', value)
        for chunk_id in [*range(1014, 1019), function_id]:
            variant = copy.deepcopy(tree)
            container = variant.find('./chunks')
            node = next(n for n in container if int(n.get('id')) == chunk_id)
            container.insert(list(container).index(node) + 1, copy.deepcopy(node))
            cases.append((variant, {}))

        def write_variant(variant, payloads, name):
            xml = directory / f'{name}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            with zipfile.ZipFile(str(original)[:-4]) as source, zipfile.ZipFile(str(xml)[:-4], 'w') as target:
                for entry in source.infolist():
                    target.writestr(entry, payloads.get(entry.filename, source.read(entry.filename)))
            rdc = directory / f'{name}.rdc'
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            return rdc

        for index, (variant, payloads) in enumerate(cases):
            invalid = write_variant(variant, payloads, f'invalid-{index:03d}')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            if not re.search(r'failed|invalid|unsupported|missing|Couldn.t load', message, re.I):
                raise RuntimeError(f'missing diagnostic: {index}: {message}')
        # Arbitrary metadata paths must have no effect on a self-contained capture.
        variant = copy.deepcopy(tree)
        for node in variant.findall('./chunks/chunk'):
            if 1015 <= int(node.get('id')) <= 1018:
                child(node, 'origin').text = '/nonexistent/T48/unused.metallib'
        valid = write_variant(variant, {}, 'valid-no-origin')
        run(command, 'replay', '--loops', '2', valid)
    print(f'T48 malformed captures rejected without crash: {len(cases)} cases; origin-independent replay passed')


if __name__ == '__main__':
    main()
