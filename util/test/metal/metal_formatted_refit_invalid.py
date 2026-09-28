#!/usr/bin/env python3
"""Reject malformed formatted-triangle build/refit chunks before GPU execution."""
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
    build_name = 'MTLAccelerationStructureCommandEncoder::buildRefittableFormattedTriangle'
    refit_name = 'MTLAccelerationStructureCommandEncoder::refitFormattedTriangle'
    with tempfile.TemporaryDirectory(prefix='metal-formatted-refit-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)

        def target(name, source=tree):
            return next(c for c in source.findall('./chunks/chunk') if c.get('name') == name)

        build, refit = target(build_name), target(refit_name)
        expected = {
            't251_capture': ('30', 'true', '0', True),
            't252_capture': ('31', 'true', '0', True),
            't253_capture': ('31', 'true', '0', False),
            't254_capture': ('30', 'false', '256', True),
            't255_capture': ('31', 'true', '0', True),
            't263_capture': ('31', 'true', '0', True),
        }
        if capture.stem in expected:
            format_value, duplicate, offset, in_place = expected[capture.stem]
            for node in (build, refit):
                assert field(node, 'vertexStride').text == '16'
                assert field(node, 'vertexFormat').text == format_value
                assert field(node, 'allowDuplicate').text == duplicate
            assert field(refit, 'scratchOffset').text == offset
            assert (field(refit, 'source').text == field(refit, 'destination').text) == in_place
            assert (field(build, 'vertices').text != field(refit, 'vertices').text) == \
                (capture.stem == 't263_capture')
        ids = {key: field(build, key).text for key in ('structure', 'vertices', 'scratch')}
        bad_format = '31' if field(build, 'vertexFormat').text == '30' else '30'
        bad_duplicate = ('false' if field(build, 'allowDuplicate').text == 'true'
                         else 'true')
        cases = [
            ('build-wrong-encoder', build_name, 'Encoder', ids['structure']),
            ('build-zero-encoder', build_name, 'Encoder', '0'),
            ('build-wrong-structure', build_name, 'structure', ids['vertices']),
            ('build-zero-structure', build_name, 'structure', '0'),
            ('build-wrong-vertices', build_name, 'vertices', ids['structure']),
            ('build-zero-vertices', build_name, 'vertices', '0'),
            ('build-zero-stride', build_name, 'vertexStride', '0'),
            ('build-short-stride', build_name, 'vertexStride', '8'),
            ('build-misaligned-stride', build_name, 'vertexStride', '15'),
            ('build-large-stride', build_name, 'vertexStride', '1048577'),
            ('build-invalid-format', build_name, 'vertexFormat', '99'),
            ('build-other-format', build_name, 'vertexFormat', bad_format),
            ('build-zero-count', build_name, 'triangleCount', '0'),
            ('build-huge-count', build_name, 'triangleCount', '1000001'),
            ('build-zero-scratch', build_name, 'scratch', '0'),
            ('build-wrong-scratch', build_name, 'scratch', ids['vertices']),
            ('build-other-duplicate', build_name, 'allowDuplicate', bad_duplicate),
            ('refit-wrong-encoder', refit_name, 'Encoder', ids['structure']),
            ('refit-zero-encoder', refit_name, 'Encoder', '0'),
            ('refit-zero-source', refit_name, 'source', '0'),
            ('refit-wrong-source', refit_name, 'source', ids['vertices']),
            ('refit-zero-destination', refit_name, 'destination', '0'),
            ('refit-wrong-destination', refit_name, 'destination', ids['scratch']),
            ('refit-zero-vertices', refit_name, 'vertices', '0'),
            ('refit-wrong-vertices', refit_name, 'vertices', ids['structure']),
            ('refit-zero-stride', refit_name, 'vertexStride', '0'),
            ('refit-misaligned-stride', refit_name, 'vertexStride', '15'),
            ('refit-other-format', refit_name, 'vertexFormat', bad_format),
            ('refit-zero-count', refit_name, 'triangleCount', '0'),
            ('refit-zero-scratch', refit_name, 'scratch', '0'),
            ('refit-wrong-scratch', refit_name, 'scratch', ids['vertices']),
            ('refit-misaligned-offset', refit_name, 'scratchOffset', '1'),
            ('refit-out-of-bounds-offset', refit_name, 'scratchOffset', '65536'),
            ('refit-other-duplicate', refit_name, 'allowDuplicate', bad_duplicate),
        ]
        for tag, name, member, value in cases:
            variant = copy.deepcopy(tree)
            field(target(name, variant), member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
        print(f'{capture.stem} malformed formatted refits rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
