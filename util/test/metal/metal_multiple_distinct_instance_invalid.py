#!/usr/bin/env python3
"""Malformed multi-child TLAS chunks must fail cleanly during Metal replay."""
import copy
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


BUILDS = (
    'MTLAccelerationStructureCommandEncoder::buildMultipleDistinctInstances',
    'MTLAccelerationStructureCommandEncoder::buildRepeatedDistinctInstances',
)


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
    with tempfile.TemporaryDirectory(prefix='metal-multi-distinct-invalid-') as temporary:
        directory = pathlib.Path(temporary)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        build = next(chunk for chunk in chunks if chunk.get('name') in BUILDS)
        build_name = build.get('name')
        repeated = build_name == BUILDS[1]
        children = field(build, 'children')
        assert len(children) in (2, 3, 4)
        last_child = len(children) - 1
        top = field(build, 'structure').text
        instances = field(build, 'instances').text
        primitive = children[0].text
        cases = [
            ('zero-top', lambda b: setattr(field(b, 'structure'), 'text', '0')),
            ('wrong-top', lambda b: setattr(field(b, 'structure'), 'text', instances)),
            ('zero-child0', lambda b: setattr(field(b, 'children')[0], 'text', '0')),
            ('zero-child1', lambda b: setattr(field(b, 'children')[1], 'text', '0')),
            ('wrong-child', lambda b: setattr(field(b, 'children')[last_child], 'text', instances)),
            ('self-child', lambda b: setattr(field(b, 'children')[last_child], 'text', top)),
            ('duplicate-child', lambda b: setattr(field(b, 'children')[last_child], 'text', primitive)),
            ('zero-instances', lambda b: setattr(field(b, 'instances'), 'text', '0')),
            ('wrong-instances', lambda b: setattr(field(b, 'instances'), 'text', primitive)),
            ('zero-scratch', lambda b: setattr(field(b, 'scratch'), 'text', '0')),
            ('wrong-scratch', lambda b: setattr(field(b, 'scratch'), 'text', primitive)),
            ('zero-encoder', lambda b: setattr(field(b, 'Encoder'), 'text', '0')),
            ('short-children', lambda b: field(b, 'children').remove(field(b, 'children')[-1])),
            ('long-children', lambda b: field(b, 'children').append(
                copy.deepcopy(field(b, 'children')[0]))),
            ('short-descriptor', lambda b: field(b, 'descriptorBytes').remove(
                field(b, 'descriptorBytes')[-1])),
            ('changed-descriptor', lambda b: setattr(field(b, 'descriptorBytes')[0],
                                                     'text', '1')),
        ]
        if len(children) >= 3:
            cases.append(('zero-child2', lambda b: setattr(field(b, 'children')[2], 'text', '0')))
        if len(children) == 4:
            cases.extend([
                ('zero-child3', lambda b: setattr(field(b, 'children')[3], 'text', '0')),
                ('duplicate-child3', lambda b: setattr(field(b, 'children')[3],
                                                       'text', primitive)),
            ])
        if repeated:
            cases.extend([
                ('zero-count', lambda b: setattr(field(b, 'count'), 'text', '0')),
                ('equal-child-count', lambda b: setattr(field(b, 'count'),
                                                        'text', str(len(children)))),
                ('other-count', lambda b: setattr(field(b, 'count'), 'text',
                                                  str(int(field(build, 'count').text) + 1))),
                ('over-cap-count', lambda b: setattr(field(b, 'count'), 'text', '65537')),
            ])
        for label, mutate in cases:
            variant = copy.deepcopy(tree)
            target = next(chunk for chunk in variant.findall('./chunks/chunk')
                          if chunk.get('name') == build_name)
            mutate(target)
            xml = directory / f'{label}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{label}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            output = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in output or
                    'Failed to replay Metal chunk' in output), (label, output)
        print(f'{capture.stem} multiple distinct-child invalid: {len(cases)} cases rejected')


if __name__ == '__main__':
    main()
