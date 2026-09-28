#!/usr/bin/env python3
"""Reject malformed refittable bounding-box build/refit chunks before GPU execution."""
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
    build_name = 'MTLAccelerationStructureCommandEncoder::buildRefittableBoundingBox'
    refit_name = 'MTLAccelerationStructureCommandEncoder::refitBoundingBox'
    with tempfile.TemporaryDirectory(prefix='metal-box-refit-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)

        def target(name, source=tree, ordinal=0):
            return [c for c in source.findall('./chunks/chunk')
                    if c.get('name') == name][ordinal]

        build, refit = target(build_name), target(refit_name)
        expected = {
            't256_capture': ('0', '0', 'true', True),
            't257_capture': ('48', '256', 'false', True),
            't258_capture': ('0', '0', 'true', False),
            't259_capture': ('0', '0', 'true', True),
            't260_capture': ('0', '0', 'true', True),
            't261_capture': ('0', '0', 'true', True),
        }
        if capture.stem in expected:
            offset, scratch_offset, duplicate, in_place = expected[capture.stem]
            for node in (build, refit):
                assert field(node, 'boxOffset').text == offset
                assert field(node, 'scratchOffset').text == scratch_offset
                assert field(node, 'allowDuplicate').text == duplicate
                assert field(node, 'boxStride').text == '32'
                assert field(node, 'boxCount').text == '2'
            assert (field(refit, 'source').text == field(refit, 'destination').text) == in_place
            assert (field(refit, 'boxes').text != field(build, 'boxes').text) == \
                (capture.stem == 't261_capture')
        ids = {key: field(build, key).text for key in ('structure', 'boxes', 'scratch')}
        bad_duplicate = 'false' if field(build, 'allowDuplicate').text == 'true' else 'true'
        bad_offset = '0' if field(build, 'boxOffset').text == '48' else '48'
        bad_table = '0' if field(build, 'tableOffset').text == '1' else '1'
        cases = [
            ('build-wrong-encoder', build_name, 'Encoder', ids['structure']),
            ('build-zero-encoder', build_name, 'Encoder', '0'),
            ('build-wrong-structure', build_name, 'structure', ids['boxes']),
            ('build-zero-structure', build_name, 'structure', '0'),
            ('build-wrong-boxes', build_name, 'boxes', ids['structure']),
            ('build-zero-boxes', build_name, 'boxes', '0'),
            ('build-misaligned-offset', build_name, 'boxOffset', '1'),
            ('build-large-offset', build_name, 'boxOffset', '65536'),
            ('build-zero-stride', build_name, 'boxStride', '0'),
            ('build-short-stride', build_name, 'boxStride', '16'),
            ('build-misaligned-stride', build_name, 'boxStride', '25'),
            ('build-large-stride', build_name, 'boxStride', '1048577'),
            ('build-zero-count', build_name, 'boxCount', '0'),
            ('build-huge-count', build_name, 'boxCount', '1000001'),
            ('build-large-table-offset', build_name, 'tableOffset', '32'),
            ('build-zero-scratch', build_name, 'scratch', '0'),
            ('build-wrong-scratch', build_name, 'scratch', ids['boxes']),
            ('build-misaligned-scratch-offset', build_name, 'scratchOffset', '1'),
            ('build-large-scratch-offset', build_name, 'scratchOffset', '65536'),
            ('refit-wrong-encoder', refit_name, 'Encoder', ids['structure']),
            ('refit-zero-encoder', refit_name, 'Encoder', '0'),
            ('refit-zero-source', refit_name, 'source', '0'),
            ('refit-wrong-source', refit_name, 'source', ids['boxes']),
            ('refit-zero-destination', refit_name, 'destination', '0'),
            ('refit-wrong-destination', refit_name, 'destination', ids['scratch']),
            ('refit-zero-boxes', refit_name, 'boxes', '0'),
            ('refit-wrong-boxes', refit_name, 'boxes', ids['structure']),
            ('refit-other-offset', refit_name, 'boxOffset', bad_offset),
            ('refit-misaligned-offset', refit_name, 'boxOffset', '1'),
            ('refit-zero-stride', refit_name, 'boxStride', '0'),
            ('refit-other-stride', refit_name, 'boxStride', '24'),
            ('refit-zero-count', refit_name, 'boxCount', '0'),
            ('refit-other-count', refit_name, 'boxCount', '1'),
            ('refit-other-table-offset', refit_name, 'tableOffset', bad_table),
            ('refit-zero-scratch', refit_name, 'scratch', '0'),
            ('refit-wrong-scratch', refit_name, 'scratch', ids['boxes']),
            ('refit-misaligned-scratch-offset', refit_name, 'scratchOffset', '1'),
            ('refit-large-scratch-offset', refit_name, 'scratchOffset', '65536'),
            ('refit-other-opaque', refit_name, 'opaque', 'true'),
            ('refit-other-duplicate', refit_name, 'allowDuplicate', bad_duplicate),
        ]
        if capture.stem == 't260_capture':
            copied = target(refit_name, tree, 1)
            assert field(copied, 'source').text == field(copied, 'destination').text
            assert field(copied, 'source').text != field(refit, 'source').text
            cases.extend([
                ('copied-refit-zero-source', refit_name, 'source', '0', 1),
                ('copied-refit-wrong-source', refit_name, 'source', ids['boxes'], 1),
                ('copied-refit-wrong-destination', refit_name, 'destination', ids['structure'], 1),
                ('copied-refit-wrong-boxes', refit_name, 'boxes', ids['structure'], 1),
                ('copied-refit-other-count', refit_name, 'boxCount', '1', 1),
                ('copied-refit-other-stride', refit_name, 'boxStride', '24', 1),
                ('copied-refit-other-table-offset', refit_name, 'tableOffset', '1', 1),
                ('copied-refit-other-duplicate', refit_name, 'allowDuplicate', 'false', 1),
            ])
        for case in cases:
            tag, name, member, value = case[:4]
            ordinal = case[4] if len(case) == 5 else 0
            variant = copy.deepcopy(tree)
            field(target(name, variant, ordinal), member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
        print(f'{capture.stem} malformed box refits rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
