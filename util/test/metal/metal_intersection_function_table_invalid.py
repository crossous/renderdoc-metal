#!/usr/bin/env python3
"""Malformed render intersection-table and non-opaque BLAS chunks must fail cleanly."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd intersection_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    names = {
        'table': 'MTLRenderPipelineState::newIntersectionFunctionTableWithDescriptor',
        'handle': 'MTLRenderPipelineState::functionHandleWithFunction',
        'update': 'MTLIntersectionFunctionTable::setFunction',
        'bind': 'MTLRenderCommandEncoder::setFragmentIntersectionFunctionTable',
        'build': 'MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle',
    }
    with tempfile.TemporaryDirectory(prefix='metal-intersection-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't148.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        if any(c.get('name') == 'MTLRenderCommandEncoder::setVertexIntersectionFunctionTable'
               for c in chunks):
            names['bind'] = 'MTLRenderCommandEncoder::setVertexIntersectionFunctionTable'
        if any(c.get('name') == 'MTLRenderCommandEncoder::setTileIntersectionFunctionTable'
               for c in chunks):
            names['bind'] = 'MTLRenderCommandEncoder::setTileIntersectionFunctionTable'
        for stage in ('Fragment', 'Vertex', 'Tile'):
            plural = f'MTLRenderCommandEncoder::set{stage}IntersectionFunctionTables'
            if any(c.get('name') == plural for c in chunks):
                names['bind'] = plural
        range_bind = names['bind'].endswith('Tables')

        def target(kind, source=tree):
            return next(c for c in source.findall('./chunks/chunk')
                        if c.get('name') == names[kind])

        ids = {
            'table': field(target('table'), 'Table').text,
            'handle': field(target('handle'), 'Handle').text,
            'pipeline': field(target('table'), 'Pipeline').text,
            'as': field(target('build'), 'structure').text,
            'vertices': field(target('build'), 'vertices').text,
        }
        cases = [
            ('table-zero-id', 'table', 'Table', '0'),
            ('table-duplicate-id', 'table', 'Table', '{handle}'),
            ('table-wrong-pipeline', 'table', 'Pipeline', '{as}'),
            ('table-zero-count', 'table', 'count', '0'),
            ('table-large-count', 'table', 'count', '33'),
            ('table-wrong-stage', 'table', 'stageValue', '8'),
            ('handle-zero-id', 'handle', 'Handle', '0'),
            ('handle-wrong-pipeline', 'handle', 'Pipeline', '{as}'),
            ('handle-zero-function', 'handle', 'function', '0'),
            ('handle-wrong-function', 'handle', 'function', '{vertices}'),
            ('handle-wrong-stage', 'handle', 'stageValue', '0'),
            ('update-wrong-table', 'update', 'Table', '{as}'),
            ('update-wrong-handle', 'update', 'function', '{vertices}'),
            ('update-index-oob', 'update', 'index', '1'),
            ('build-zero-count', 'build', 'triangleCount', '0'),
            ('build-wrong-vertices', 'build', 'vertices', '{as}'),
            ('build-zero-scratch', 'build', 'scratch', '0'),
        ]
        if range_bind:
            cases.extend([
                ('bind-wrong-table', 'bind', 'tables[0]', '{as}'),
                ('bind-index-oob', 'bind', 'range.location', '31'),
                ('bind-zero-length', 'bind', 'range.length', '0'),
                ('bind-length-mismatch', 'bind', 'range.length', '2'),
                ('bind-wrong-encoder', 'bind', 'RenderCommandEncoder', '{as}'),
            ])
        else:
            cases.extend([
                ('bind-wrong-table', 'bind', 'table', '{as}'),
                ('bind-index-oob', 'bind', 'index', '31'),
                ('bind-wrong-encoder', 'bind', 'RenderCommandEncoder', '{as}'),
            ])
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
        print(f'{capture.stem} intersection table malformed captures rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
