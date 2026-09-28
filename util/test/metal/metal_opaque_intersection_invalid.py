#!/usr/bin/env python3
"""Malformed opaque-triangle intersection-table chunks must fail cleanly."""
import copy
import os
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd opaque_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-opaque-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        names = {
            'table': 'MTLRenderPipelineState::newIntersectionFunctionTableWithDescriptor',
            'build': 'MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle',
            'bind': 'MTLRenderCommandEncoder::setFragmentIntersectionFunctionTable',
        }
        update = next(c for c in chunks if c.get('name') in (
            'MTLIntersectionFunctionTable::setOpaqueTriangleFunction',
            'MTLIntersectionFunctionTable::setOpaqueTriangleFunctions'))
        names['update'] = update.get('name')
        range_update = names['update'].endswith('Functions')

        def target(kind, source=tree):
            return next(c for c in source.findall('./chunks/chunk')
                        if c.get('name') == names[kind])

        ids = {
            'as': field(target('build'), 'structure').text,
            'vertices': field(target('build'), 'vertices').text,
        }
        cases = [
            ('table-zero-id', 'table', 'Table', '0'),
            ('table-wrong-pipeline', 'table', 'Pipeline', '{as}'),
            ('table-zero-count', 'table', 'count', '0'),
            ('table-large-count', 'table', 'count', '33'),
            ('table-wrong-stage', 'table', 'stageValue', '8'),
            ('update-wrong-table', 'update', 'Table', '{as}'),
            ('update-zero-signature', 'update', 'signatureValue', '0'),
            ('update-wrong-signature', 'update', 'signatureValue', '1'),
            ('update-extra-signature', 'update', 'signatureValue', '7'),
            ('bind-wrong-table', 'bind', 'table', '{as}'),
            ('bind-index-oob', 'bind', 'index', '31'),
            ('bind-wrong-encoder', 'bind', 'RenderCommandEncoder', '{as}'),
            ('build-zero-count', 'build', 'triangleCount', '0'),
            ('build-wrong-vertices', 'build', 'vertices', '{as}'),
            ('build-zero-scratch', 'build', 'scratch', '0'),
        ]
        if range_update:
            cases.extend([
                ('update-index-oob', 'update', 'range.location', '1'),
                ('update-zero-length', 'update', 'range.length', '0'),
                ('update-length-oob', 'update', 'range.length', '2'),
            ])
        else:
            cases.append(('update-index-oob', 'update', 'index', '1'))
        selected = os.environ.get('RENDERDOC_METAL_NEGATIVE_CASE')
        if selected:
            cases = [case for case in cases if case[0] == selected]
            if not cases:
                raise ValueError(f'unknown negative case: {selected}')
        for tag, kind, member, value in cases:
            variant = copy.deepcopy(tree)
            selected = target(kind, variant)
            if member.startswith('range.'):
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
        print(f'{capture.stem} opaque intersection malformed captures rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
