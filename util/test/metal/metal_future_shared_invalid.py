#!/usr/bin/env python3
"""Ensure deferring future Shared snapshots does not accept invalid resources or ranges."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    r = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=30)
    if r.returncode < 0 or (r.returncode == 0) != success:
        raise RuntimeError(f'unexpected result {r.returncode}: {args}\n{r.stdout}')
    return r.stdout


def field(chunk, name):
    return next(n for n in chunk if n.get('name') == name)


def main():
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-future-shared-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        destination = next(n for n in chunks if n.get('name') == 'MTLBlitCommandEncoder::copyFromBuffer')
        destination_id = field(destination, 'destinationBuffer').text
        texture_id = field(next(n for n in chunks if n.get('name') == 'MTLDevice::newTextureWithDescriptor'), 'Texture').text
        cases = [
            ('unknown-resource', 'snapshot', 'Buffer', '999999'),
            ('zero-resource', 'snapshot', 'Buffer', '0'),
            ('wrong-resource-type', 'snapshot', 'Buffer', texture_id),
            ('offset-outside-buffer', 'snapshot', 'start', '256'),
            ('offset-overflow', 'snapshot', 'start', '18446744073709551615'),
            ('payload-size-mismatch', 'snapshot', 'size', '255'),
            ('private-snapshot-target', 'creation', 'options', '32'),
            ('undersized-creation', 'creation', 'length', '128'),
        ]
        for name, kind, key, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(n for n in variant.findall('./chunks/chunk')
                         if n.get('name') == ('Internal_MTLBufferModifyCPUContents' if kind == 'snapshot'
                                             else 'MTLDevice::newBufferWithLength')
                         and field(n, 'Buffer').text == destination_id)
            field(chunk, key).text = value
            xml = directory / f'{name}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{name}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert 'Failed to replay Metal chunk' in message, (name, message)
            print(f'PASS rejected {name}')


if __name__ == '__main__':
    main()
