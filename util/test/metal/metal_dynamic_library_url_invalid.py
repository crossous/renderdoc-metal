#!/usr/bin/env python3
"""Malformed URL-loaded Metal dynamic libraries must fail without using the captured path."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t216_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-dynamic-url-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        dynamic = next(c for c in chunks if c.get('name') ==
                       'MTLDevice::newDynamicLibraryWithURL')
        executable = next(c for c in chunks if c.get('name') ==
                          'MTLDevice::newLibraryWithSource' and
                          child(c, 'libraryType').text == '0')
        dynamic_index, executable_index = chunks.index(dynamic), chunks.index(executable)
        device = child(dynamic, 'Device').text
        dynamic_id = child(dynamic, 'DynamicLibrary').text
        install_name = child(dynamic, 'installName').text
        assert install_name and child(dynamic, 'data').get('byteLength')
        cases = []

        def edit(tag, index, member, value):
            variant = copy.deepcopy(tree)
            child(variant.findall('./chunks/chunk')[index], member).text = str(value)
            cases.append((tag, variant))

        edit('url-id-zero', dynamic_index, 'DynamicLibrary', '0')
        edit('url-id-device', dynamic_index, 'DynamicLibrary', device)
        edit('url-origin-long', dynamic_index, 'origin', 'x' * 4097)
        edit('url-install-empty', dynamic_index, 'installName', '')
        edit('url-install-short', dynamic_index, 'installName', '/tmp/a')
        edit('url-install-long', dynamic_index, 'installName', 'x' * 201)
        edit('url-install-not-in-bytes', dynamic_index, 'installName',
             'x' * len(install_name))
        edit('exe-unsupported', executable_index, 'supported', 'false')
        edit('exe-id-dynamic', executable_index, 'Library', dynamic_id)
        for tag, value in [('dependency-zero', '0'), ('dependency-device', device)]:
            variant = copy.deepcopy(tree)
            deps = child(variant.findall('./chunks/chunk')[executable_index], 'dependencies')
            deps[0].text = value
            cases.append((tag, variant))
        ordinary = next((c for c in chunks if c.get('name') ==
                         'MTLDevice::newDynamicLibrary'), None)
        if ordinary is not None:
            edit('second-dynamic-unsupported', chunks.index(ordinary), 'supported', 'false')
            variant = copy.deepcopy(tree)
            deps = child(variant.findall('./chunks/chunk')[executable_index], 'dependencies')
            deps[1].text = '0'
            cases.append(('second-dependency-zero', variant))

        for tag, variant in cases:
            for chunk in variant.findall('./chunks/chunk'): chunk.set('length', '0')
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)

        # The original URL may no longer exist; its spelling must not be used on replay.
        variant = copy.deepcopy(tree)
        child(variant.findall('./chunks/chunk')[dynamic_index], 'origin').text = \
            '/no/such/T216/library.metallib'
        for chunk in variant.findall('./chunks/chunk'): chunk.set('length', '0')
        xml = directory / 'remapped.zip.xml'
        variant.write(xml, encoding='unicode', xml_declaration=True)
        shutil.copyfile(str(original)[:-4], str(xml)[:-4])
        valid = directory / 'remapped.rdc'
        run(command, 'convert', '-f', xml, '-o', valid, '-c', 'rdc')
        run(command, 'replay', '--loops', '2', valid)
    print(f'{capture.stem} URL dynamic library malformed captures rejected: {len(cases)} cases; '
          'original URL ignored')


if __name__ == '__main__':
    main()
