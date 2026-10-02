#!/usr/bin/env python3
"""T104 malformed dynamic-library identities, options and dependency graph."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=45)
    if (result.returncode == 0) != success or result.returncode < 0:
        raise RuntimeError(f'unexpected exit {result.returncode}: {args}\n{result.stdout}')
    return result.stdout


def child(node, name):
    return next(item for item in node if item.get('name') == name)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t104_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-dynamic-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        sources = [c for c in chunks if c.get('name') in (
            'MTLDevice::newLibraryWithSource',
            'MTLDevice::newLibraryWithSource(completionHandler)')]
        dynamic = next(c for c in chunks if c.get('name') == 'MTLDevice::newDynamicLibrary')
        assert len(sources) == 2
        cases = []

        def edit(tag, index, field, value):
            variant = copy.deepcopy(tree)
            node = variant.findall('./chunks/chunk')[index]
            child(node, field).text = str(value)
            cases.append((tag, variant))

        source_index = chunks.index(sources[0])
        dynamic_index = chunks.index(dynamic)
        executable_index = chunks.index(sources[1])
        device = child(sources[0], 'Device').text
        library = child(sources[0], 'Library').text
        dynamic_id = child(dynamic, 'DynamicLibrary').text
        for name, value in [('type-invalid','99'),('type-executable','0')]:
            edit(name, source_index, 'libraryType', value)
        for name, value in [('install-empty',''),('install-long','a'*1025)]:
            edit(name, source_index, 'installName', value)
        edit('source-unsupported', source_index, 'supported', 'false')
        edit('source-id-zero', source_index, 'Library', '0')
        edit('source-id-device', source_index, 'Library', device)
        edit('dynamic-unsupported', dynamic_index, 'supported', 'false')
        edit('dynamic-id-zero', dynamic_index, 'DynamicLibrary', '0')
        edit('dynamic-id-source', dynamic_index, 'DynamicLibrary', library)
        edit('dynamic-source-zero', dynamic_index, 'library', '0')
        edit('dynamic-source-device', dynamic_index, 'library', device)
        edit('exe-type-dynamic', executable_index, 'libraryType', '1')
        edit('exe-install', executable_index, 'installName', '/tmp/invalid.metallib')
        edit('exe-unsupported', executable_index, 'supported', 'false')
        edit('exe-id-dynamic', executable_index, 'Library', dynamic_id)
        for tag, value in [('dep-zero','0'),('dep-source',library),('dep-device',device)]:
            variant = copy.deepcopy(tree)
            dependency = child(variant.findall('./chunks/chunk')[executable_index], 'dependencies')
            dependency[0].text = value
            cases.append((tag, variant))
        for tag, variant in cases:
            for chunk in variant.findall('./chunks/chunk'): chunk.set('length', '0')
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert 'Failed to process Metal chunk' in message or 'Failed to replay Metal chunk' in message, (tag, message)
        # Replay must ignore the captured absolute install path and materialize a private one.
        variant = copy.deepcopy(tree)
        child(variant.findall('./chunks/chunk')[source_index], 'installName').text = '/no/such/T104/path.metallib'
        for chunk in variant.findall('./chunks/chunk'): chunk.set('length', '0')
        xml = directory / 'remapped.zip.xml'
        variant.write(xml, encoding='unicode', xml_declaration=True)
        shutil.copyfile(str(original)[:-4], str(xml)[:-4])
        valid = directory / 'remapped.rdc'
        run(command, 'convert', '-f', xml, '-o', valid, '-c', 'rdc')
        run(command, 'replay', '--loops', '2', valid)
    number = (110 if sources[0].get('name').endswith('(completionHandler)') else
              111 if sources[1].get('name').endswith('(completionHandler)') else 104)
    print(f'T{number} malformed dynamic-library captures rejected: {len(cases)} cases; install path remapped')


if __name__ == '__main__':
    main()
