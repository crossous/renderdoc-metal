#!/usr/bin/env python3
"""Reject malformed AS markers and non-null intersection handles before GPU encoding."""
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
    command, capture = map(pathlib.Path, sys.argv[1:3])
    if len(sys.argv) == 4 and sys.argv[3] == '--background':
        message = run(command, 'replay', '--loops', '1', capture, success=False)
        assert 'Failed to replay Metal chunk MTLComputeCommandEncoder::setAccelerationStructure' in message
        print('PASS explicitly rejected missing pre-frame AS reconstruction without signal/hang')
        return
    with tempfile.TemporaryDirectory(prefix='metal-ray-table-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        build = next(c for c in chunks if c.get('name') ==
                     'MTLAccelerationStructureCommandEncoder::buildTriangleTableOffset')
        structure = field(build, 'structure').text
        vertices = field(build, 'vertices').text
        end_name = 'MTLAccelerationStructureCommandEncoder::endEncoding'
        cases = []
        for method in ('pushDebugGroup', 'insertDebugSignpost', 'popDebugGroup'):
            for tag, value in (('zero', '0'), ('unknown', '99999999'), ('wrong-type', structure)):
                cases.append((f'{method}-{tag}', method, 'Encoder', value))
            cases.append((f'{method}-after-end', method, 'after-end', None))
        for member, tag, value in (
            ('Table', 'zero-table', '0'), ('Table', 'unknown-table', '99999999'),
            ('Table', 'wrong-table', structure), ('function', 'unknown-function', '99999999'),
            ('function', 'wrong-function', vertices), ('index', 'index-oob', '2'),
            ('index', 'index-overflow', '4294967295')):
            cases.append((tag, 'setFunction', member, value))
        has_fences = any(c.get('name') == 'MTLAccelerationStructureCommandEncoder::waitForFence'
                         for c in chunks)
        if has_fences:
            for method in ('updateFence', 'waitForFence'):
                for member in ('Encoder', 'fence'):
                    for tag, value in (('zero', '0'), ('unknown', '99999999'), ('wrong-type', structure)):
                        cases.append((f'{method}-{member}-{tag}', method, member, value))
                cases.append((f'{method}-after-end', method, 'after-end', None))
            cases += [
                ('missing-upload-update', 'waitForFence', 'remove-producer',
                 'MTLBlitCommandEncoder::updateFence'),
                ('missing-as-update', 'updateFence', 'remove-producer',
                 'MTLAccelerationStructureCommandEncoder::updateFence'),
                ('same-encoder-wait', 'waitForFence', 'same-encoder', None),
            ]
        for tag, method, member, value in cases:
            variant = copy.deepcopy(tree)
            parent = variant.find('./chunks')
            prefix = ('MTLIntersectionFunctionTable::' if method == 'setFunction'
                      else 'MTLAccelerationStructureCommandEncoder::')
            target = next(c for c in parent if c.get('name') == prefix + method)
            if member == 'remove-producer':
                producer = next(c for c in parent if c.get('name') == value)
                parent.remove(producer)
            elif member == 'same-encoder':
                producer = copy.deepcopy(next(c for c in parent if c.get('name') ==
                    'MTLAccelerationStructureCommandEncoder::updateFence'))
                field(producer, 'Encoder').text = field(target, 'Encoder').text
                field(producer, 'fence').text = field(target, 'fence').text
                parent.insert(list(parent).index(target), producer)
            elif member == 'after-end':
                end = next(c for c in parent if c.get('name') == end_name)
                parent.remove(target)
                parent.insert(list(parent).index(end) + 1, target)
            else:
                field(target, member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
            print(f'PASS rejected {tag}')
        print(f'PASS {len(cases)} malformed ray table/AS marker captures')


if __name__ == '__main__':
    main()
