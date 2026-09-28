#!/usr/bin/env python3
"""Malformed plain/indexed formatted-triangle chunks must fail before Metal execution."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    base_name = 'MTLAccelerationStructureCommandEncoder::'
    with tempfile.TemporaryDirectory(prefix='metal-formatted-triangle-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        indexed = any(c.get('name') == base_name + 'buildIndexedFormattedTriangle'
                      for c in tree.findall('./chunks/chunk'))
        name = base_name + ('buildIndexedFormattedTriangle' if indexed
                            else 'buildFormattedTriangle')

        def target(source):
            return next(c for c in source.findall('./chunks/chunk') if c.get('name') == name)

        build = target(tree)
        expected = {
            't244_capture': (0, 16, 30, 0, 0, 0, 1),
            't245_capture': (0, 16, 31, 0, 0, 0, 1),
            't246_capture': (16, 16, 30, 256, 0, 0, 1),
            't247_capture': (0, 16, 31, 0, 0, 0, 0),
            't248_capture': (0, 16, 31, 0, 0, 0, 1),
            't249_capture': (16, 16, 31, 256, 0, 0, 1),
            't250_capture': (0, 16, 30, 0, 0, 0, 0),
        }
        members = ('vertexOffset', 'vertexStride', 'vertexFormat', 'scratchOffset',
                   'tableOffset', 'opaque', 'allowDuplicate')
        if capture.stem in expected:
            actual = tuple(int({'false': '0', 'true': '1'}.get(field(build, member).text,
                                                             field(build, member).text))
                           for member in members)
            assert actual == expected[capture.stem], (capture.stem, actual)
        if indexed:
            assert field(build, 'indexType').text == ('1' if capture.stem ==
                                                      't249_capture' else '0')
            assert field(build, 'indexOffset').text == ('8' if capture.stem ==
                                                        't249_capture' else '0')
            assert field(build, 'indices').text not in ('0', None)
        ids = {key: field(build, key).text for key in ('structure', 'vertices', 'scratch')}
        cases = [
            ('wrong-encoder', 'Encoder', ids['structure']),
            ('zero-encoder', 'Encoder', '0'),
            ('wrong-structure', 'structure', ids['vertices']),
            ('zero-structure', 'structure', '0'),
            ('wrong-vertices', 'vertices', ids['structure']),
            ('zero-vertices', 'vertices', '0'),
            ('misaligned-vertex-offset', 'vertexOffset', '1'),
            ('out-of-bounds-vertex-offset', 'vertexOffset', '65536'),
            ('zero-stride', 'vertexStride', '0'),
            ('too-short-stride', 'vertexStride', '8'),
            ('misaligned-stride', 'vertexStride', '15'),
            ('large-stride', 'vertexStride', '1048577'),
            ('invalid-format', 'vertexFormat', '99'),
            ('zero-count', 'triangleCount', '0'),
            ('huge-count', 'triangleCount', '1000001'),
            ('wrong-scratch', 'scratch', ids['vertices']),
            ('zero-scratch', 'scratch', '0'),
            ('misaligned-scratch-offset', 'scratchOffset', '1'),
            ('out-of-bounds-scratch-offset', 'scratchOffset', '65536'),
            ('large-table-offset', 'tableOffset', '32'),
        ]
        if indexed:
            cases += [
                ('wrong-indices', 'indices', ids['structure']),
                ('zero-indices', 'indices', '0'),
                ('invalid-index-type', 'indexType', '99'),
                ('misaligned-index-offset', 'indexOffset', '1'),
                ('out-of-bounds-index-offset', 'indexOffset', '65536'),
            ]
        for tag, member, value in cases:
            variant = copy.deepcopy(tree)
            field(target(variant), member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
        print(f'{capture.stem} malformed formatted triangles rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
