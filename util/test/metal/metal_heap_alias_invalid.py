#!/usr/bin/env python3
"""Reject invalid makeAliasable targets without calling Metal on bad resources."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=30)
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f'unexpected result {result.returncode}: {args}\n{result.stdout}')
    return result.stdout


def field(node, name):
    return next(child for child in node if child.get('name') == name)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t131_or_t132_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-heap-alias-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'alias.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        alias = next(c for c in chunks if c.get('name') in (
            'MTLBuffer::makeAliasable', 'MTLTexture::makeAliasable'))
        buffer = alias.get('name') == 'MTLBuffer::makeAliasable'
        member = 'Buffer' if buffer else 'Texture'
        wrong_type = field(next(c for c in chunks if c.get('name') ==
                                'MTLDevice::newHeapWithDescriptor'), 'Heap').text
        nonheap = field(next(c for c in chunks if c.get('name') ==
                             ('MTLDevice::newBufferWithLength' if buffer else
                              '[CAMetalLayer nextDrawable]')), member).text
        assert wrong_type != nonheap != field(alias, member).text
        cases = [('zero', '0'), ('wrong-type', wrong_type), ('nonheap', nonheap)]
        for tag, value in cases:
            variant = copy.deepcopy(tree)
            target = next(c for c in variant.findall('./chunks/chunk') if c.get('name') ==
                          alias.get('name'))
            field(target, member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'{capture.stem} heap alias malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
