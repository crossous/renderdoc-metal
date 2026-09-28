#!/usr/bin/env python3
"""Malformed no-duplicate triangle build/refit chunks must fail safely."""
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
    build_name = ('MTLAccelerationStructureCommandEncoder::'
                  'buildRefittableTriangleNoDuplicate')
    refit_name = 'MTLAccelerationStructureCommandEncoder::refitTriangleNoDuplicate'
    with tempfile.TemporaryDirectory(prefix='metal-refit-no-duplicate-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        def target(name, source=tree):
            return next(c for c in source.findall('./chunks/chunk')
                        if c.get('name') == name)
        build = target(build_name)
        ids = {
            'structure': field(build, 'structure').text,
            'vertices': field(build, 'vertices').text,
            'scratch': field(build, 'scratch').text,
        }
        cases = [
            ('build-wrong-encoder', build_name, 'Encoder', '{structure}'),
            ('build-zero-structure', build_name, 'structure', '0'),
            ('build-wrong-structure', build_name, 'structure', '{vertices}'),
            ('build-zero-vertices', build_name, 'vertices', '0'),
            ('build-wrong-vertices', build_name, 'vertices', '{structure}'),
            ('build-zero-count', build_name, 'triangleCount', '0'),
            ('build-huge-count', build_name, 'triangleCount', '1000001'),
            ('build-zero-scratch', build_name, 'scratch', '0'),
            ('build-wrong-scratch', build_name, 'scratch', '{vertices}'),
            ('refit-wrong-encoder', refit_name, 'Encoder', '{structure}'),
            ('refit-zero-source', refit_name, 'source', '0'),
            ('refit-wrong-source', refit_name, 'source', '{vertices}'),
            ('refit-zero-destination', refit_name, 'destination', '0'),
            ('refit-wrong-destination', refit_name, 'destination', '{scratch}'),
            ('refit-zero-vertices', refit_name, 'vertices', '0'),
            ('refit-wrong-vertices', refit_name, 'vertices', '{structure}'),
            ('refit-zero-count', refit_name, 'triangleCount', '0'),
            ('refit-zero-scratch', refit_name, 'scratch', '0'),
            ('refit-wrong-scratch', refit_name, 'scratch', '{vertices}'),
            ('refit-misaligned-scratch-offset', refit_name, 'scratchOffset', '1'),
            ('refit-out-of-bounds-scratch-offset', refit_name, 'scratchOffset', '65536'),
        ]
        for tag, chunk_name, member, value in cases:
            variant = copy.deepcopy(tree)
            field(target(chunk_name, variant), member).text = value.format(**ids)
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
        print(f'{capture.stem} no-duplicate refit malformed captures rejected: '
              f'{len(cases)} cases')


if __name__ == '__main__':
    main()
