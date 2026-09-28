#!/usr/bin/env python3
"""Malformed compute intersection table and box-ray chunks must fail cleanly."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd box_ray_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-compute-intersection-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'box.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        names = {
            'table': 'MTLComputePipelineState::newIntersectionFunctionTableWithDescriptor',
            'handle': 'MTLComputePipelineState::functionHandleWithFunction',
            'update': 'MTLIntersectionFunctionTable::setFunction',
            'build': next(name for name in (
                'MTLAccelerationStructureCommandEncoder::buildBoundingBoxNoDuplicate',
                'MTLAccelerationStructureCommandEncoder::buildBoundingBoxOpaque',
                'MTLAccelerationStructureCommandEncoder::buildBoundingBoxTableOffset',
                'MTLAccelerationStructureCommandEncoder::buildBoundingBoxStrided')
                if any(c.get('name') == name for c in chunks)),
            'bind': ('MTLComputeCommandEncoder::setIntersectionFunctionTables'
                     if any(c.get('name') == 'MTLComputeCommandEncoder::setIntersectionFunctionTables'
                            for c in chunks)
                     else 'MTLComputeCommandEncoder::setIntersectionFunctionTable'),
        }
        def target(kind, source=tree):
            return next(c for c in source.findall('./chunks/chunk')
                        if c.get('name') == names[kind])

        ids = {
            'table': field(target('table'), 'Table').text,
            'handle': field(target('handle'), 'Handle').text,
            'pipeline': field(target('table'), 'Pipeline').text,
            'as': field(target('build'), 'structure').text,
            'boxes': field(target('build'), 'boxes').text,
        }
        cases = [
            ('table-zero-id', 'table', 'Table', '0'),
            ('table-duplicate-id', 'table', 'Table', '{handle}'),
            ('table-wrong-pipeline', 'table', 'Pipeline', '{as}'),
            ('table-zero-count', 'table', 'count', '0'),
            ('table-large-count', 'table', 'count', '33'),
            ('handle-zero-id', 'handle', 'Handle', '0'),
            ('handle-wrong-pipeline', 'handle', 'Pipeline', '{as}'),
            ('handle-zero-function', 'handle', 'function', '0'),
            ('handle-wrong-function', 'handle', 'function', '{boxes}'),
            ('update-wrong-table', 'update', 'Table', '{as}'),
            ('update-wrong-handle', 'update', 'function', '{boxes}'),
            ('update-index-oob', 'update', 'index',
             '2' if field(target('update'), 'index').text == '1' else '1'),
            ('build-zero-count', 'build', 'boxCount', '0'),
            ('build-wrong-boxes', 'build', 'boxes', '{as}'),
            ('build-zero-scratch', 'build', 'scratch', '0'),
        ]
        if names['build'].endswith('TableOffset'):
            cases += [
                ('build-zero-table-offset', 'build', 'tableOffset', '0'),
                ('build-large-table-offset', 'build', 'tableOffset', '32'),
            ]
        elif names['build'].endswith('Opaque') or names['build'].endswith('NoDuplicate'):
            cases += [('build-large-table-offset', 'build', 'tableOffset', '32')]
        if names['build'].endswith('NoDuplicate'):
            cases += [
                ('build-unaligned-box-offset', 'build', 'boxOffset', '1'),
                ('build-unaligned-scratch-offset', 'build', 'scratchOffset', '1'),
                ('build-invalid-stride', 'build', 'boxStride', '16'),
            ]
        if names['bind'].endswith('Tables'):
            cases += [
                ('bind-wrong-table', 'bind', 'tables[0]', '{as}'),
                ('bind-index-oob', 'bind', 'range.location', '31'),
                ('bind-zero-length', 'bind', 'range.length', '0'),
                ('bind-length-mismatch', 'bind', 'range.length', '2'),
                ('bind-wrong-encoder', 'bind', 'ComputeCommandEncoder', '{as}'),
            ]
        else:
            cases += [
                ('bind-wrong-table', 'bind', 'table', '{as}'),
                ('bind-index-oob', 'bind', 'index', '31'),
                ('bind-wrong-encoder', 'bind', 'ComputeCommandEncoder', '{as}'),
            ]
        for tag, kind, member, value in cases:
            variant = copy.deepcopy(tree)
            selected = target(kind, variant)
            if member == 'tables[0]':
                selected = field(selected, 'tables')[0]
            elif member.startswith('range.'):
                selected = field(field(selected, 'range'), member[6:])
            else:
                selected = field(selected, member)
            selected.text = value.format(**ids)
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
        print(f'{capture.stem} compute intersection table malformed captures rejected: '
              f'{len(cases)} cases')


if __name__ == '__main__':
    main()
