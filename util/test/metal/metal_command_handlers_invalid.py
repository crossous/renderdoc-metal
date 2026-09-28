#!/usr/bin/env python3
"""T49 handler registrations: reject invalid identities, never execute application code."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t49_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-handlers-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't49.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        ids = [0, 999999999]
        for chunk_id, field in [(1000, 'Device'), (1001, 'CommandQueue'), (1004, 'Buffer'),
                                (1056, 'BlitCommandEncoder'), (1010, 'Texture')]:
            ids.append(child(next(n for n in chunks if int(n.get('id')) == chunk_id), field).text)
        handlers = [i for i, n in enumerate(chunks) if int(n.get('id')) in (1049, 1054)]
        assert len(handlers) == 8

        def write_variant(variant, name, payloads=None):
            xml = directory / f'{name}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            with zipfile.ZipFile(str(original)[:-4]) as source, zipfile.ZipFile(str(xml)[:-4], 'w') as target:
                for entry in source.infolist():
                    target.writestr(entry, (payloads or {}).get(entry.filename, source.read(entry.filename)))
            rdc = directory / f'{name}.rdc'
            run(command, 'convert', '-f', xml, '-o', rdc, '-c', 'rdc')
            return rdc

        count = 0
        for index in handlers:
            for value in ids:
                variant = copy.deepcopy(tree)
                child(variant.findall('./chunks/chunk')[index], 'CommandBuffer').text = str(value)
                invalid = write_variant(variant, f'invalid-{count:03d}')
                message = run(command, 'replay', '--loops', '1', invalid, success=False)
                if not re.search(r'failed|invalid|missing|Couldn.t load', message, re.I):
                    raise RuntimeError(f'missing diagnostic: {count}: {message}')
                count += 1

        parameters = child(next(n for n in chunks if int(n.get('id')) == 1004 and
                                child(n, 'length').text == '12'), 'Buffer').text
        update_index = next(i for i, n in enumerate(chunks) if int(n.get('id')) == 1198 and
                            child(n, 'Buffer').text == parameters)
        initial_index = next(i for i, n in enumerate(chunks) if int(n.get('id')) == 3 and
                             child(n, 'id').text == parameters)
        extra_cases = []

        def edit(index, field, value):
            variant = copy.deepcopy(tree)
            child(variant.findall('./chunks/chunk')[index], field).text = str(value)
            extra_cases.append((variant, {}))
            return variant

        wrong_buffer_ids = ids[:4] + ids[5:]
        for value in wrong_buffer_ids:
            edit(update_index, 'Buffer', value)
            edit(initial_index, 'id', value)
        for value in (4, 12, 13, 2**64 - 1):
            edit(update_index, 'start', value)
        for value in (0, 1, 8, 10, 2**64 - 1):
            edit(update_index, 'size', value)
        for index, field, lengths in [(update_index, 'data', (0, 8, 10)),
                                      (initial_index, 'Contents', (0, 11, 13))]:
            for length in lengths:
                variant = copy.deepcopy(tree)
                node = child(variant.findall('./chunks/chunk')[index], field)
                payload_index = int(node.text)
                node.set('byteLength', str(length))
                extra_cases.append((variant, {f'{payload_index:06d}': bytes(length)}))
        variant = copy.deepcopy(tree)
        container = variant.find('./chunks')
        container.insert(initial_index + 1, copy.deepcopy(container[initial_index]))
        extra_cases.append((variant, {}))
        # A CPU update needs an owning submission and CPU-accessible shared storage.
        variant = copy.deepcopy(tree)
        container = variant.find('./chunks')
        update = container[update_index]
        container.remove(update)
        first_command = next(i for i, n in enumerate(container) if int(n.get('id')) == 1044)
        container.insert(first_command, update)
        extra_cases.append((variant, {}))
        buffer_index = next(i for i, n in enumerate(chunks) if int(n.get('id')) == 1004 and
                            child(n, 'Buffer').text == parameters)
        edit(buffer_index, 'options', 32)
        edit(initial_index, 'type', 2)
        for variant, payloads in extra_cases:
            invalid = write_variant(variant, f'invalid-{count:03d}', payloads)
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            if not re.search(r'failed|invalid|unhandled|missing|Couldn.t load', message, re.I):
                raise RuntimeError(f'missing diagnostic: {count}: {message}')
            count += 1
        # Registering multiple callbacks is legal; offline replay neither invokes nor waits for them.
        variant = copy.deepcopy(tree)
        container = variant.find('./chunks')
        for index in reversed(handlers):
            container.insert(index + 1, copy.deepcopy(container[index]))
        run(command, 'replay', '--loops', '3', write_variant(variant, 'valid-duplicates'))
        # The captured CPU update is sufficient even when all callback metadata is removed.
        variant = copy.deepcopy(tree)
        container = variant.find('./chunks')
        for index in reversed(handlers):
            container.remove(container[index])
        run(command, 'replay', '--loops', '3', write_variant(variant, 'valid-no-handlers'))
    print(f'T49 malformed captures rejected without crash: {count} cases; duplicate/removed metadata replay passed')


if __name__ == '__main__':
    main()
