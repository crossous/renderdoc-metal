#!/usr/bin/env python3
"""Malformed T142-T147 instance AS capture chunks must fail without crashing."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd instance_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    names = {
        'create': 'MTLDevice::newAccelerationStructureWithSize',
        'begin': 'MTLCommandBuffer::accelerationStructureCommandEncoder',
        'build': 'MTLAccelerationStructureCommandEncoder::buildInstance',
        'bind': 'MTLComputeCommandEncoder::setAccelerationStructure',
    }
    cases = [
        ('top-zero-size', 'create', 1, 'size', '0'),
        ('top-small-size', 'create', 1, 'size', '1'),
        ('top-zero-id', 'create', 1, 'Structure', '0'),
        ('build-zero-as', 'build', 0, 'structure', '0'),
        ('build-wrong-as', 'build', 0, 'structure', '{instances}'),
        ('build-zero-child', 'build', 0, 'child', '0'),
        ('build-wrong-child', 'build', 0, 'child', '{instances}'),
        ('build-self-child', 'build', 0, 'child', '{top}'),
        ('build-zero-instances', 'build', 0, 'instances', '0'),
        ('build-wrong-instances', 'build', 0, 'instances', '{primitive}'),
        ('build-short-instances', 'build', 0, 'instances', '{vertices}'),
        ('build-zero-scratch', 'build', 0, 'scratch', '0'),
        ('build-wrong-scratch', 'build', 0, 'scratch', '{vertices}'),
        ('build-zero-encoder', 'build', 0, 'Encoder', '0'),
        ('build-prior-encoder', 'build', 0, 'Encoder', '{first_encoder}'),
        ('bind-wrong-as', 'bind', 0, 'structure', '{instances}'),
        ('bind-index-oob', 'bind', 0, 'index', '31'),
        ('bind-zero-encoder', 'bind', 0, 'ComputeCommandEncoder', '0'),
    ]
    with tempfile.TemporaryDirectory(prefix='metal-instance-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't142.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        distinct = any(c.get('name') ==
                       'MTLAccelerationStructureCommandEncoder::buildDistinctInstances'
                       for c in chunks)
        copied = any(c.get('name') ==
                     'MTLAccelerationStructureCommandEncoder::copyAccelerationStructure'
                     for c in chunks)
        primitive_copied = copied and next(i for i, c in enumerate(chunks) if c.get('name') ==
            'MTLAccelerationStructureCommandEncoder::copyAccelerationStructure') < next(
            i for i, c in enumerate(chunks) if c.get('name') in (
                'MTLAccelerationStructureCommandEncoder::buildInstance',
                'MTLAccelerationStructureCommandEncoder::buildInstances',
                'MTLAccelerationStructureCommandEncoder::buildDistinctInstances'))
        compacted = any(c.get('name') ==
                        'MTLAccelerationStructureCommandEncoder::copyAndCompactAccelerationStructure'
                        for c in chunks)
        primitive_compacted = compacted and next(
            i for i, c in enumerate(chunks) if c.get('name') ==
            'MTLAccelerationStructureCommandEncoder::copyAndCompactAccelerationStructure') < next(
            i for i, c in enumerate(chunks) if c.get('name') in (
                'MTLAccelerationStructureCommandEncoder::buildInstance',
                'MTLAccelerationStructureCommandEncoder::buildInstances',
                'MTLAccelerationStructureCommandEncoder::buildDistinctInstances'))
        if distinct:
            names['build'] = 'MTLAccelerationStructureCommandEncoder::buildDistinctInstances'
            cases = [(tag, kind, index, 'child0' if member == 'child' else member, value)
                     for tag, kind, index, member, value in cases]
            cases = [(tag, kind, 2 if kind == 'create' and index == 1 else index, member, value)
                     for tag, kind, index, member, value in cases]
            cases.extend([
                ('build-zero-child1', 'build', 0, 'child1', '0'),
                ('build-wrong-child1', 'build', 0, 'child1', '{instances}'),
                ('build-same-child1', 'build', 0, 'child1', '{primitive}'),
            ])
        elif any(c.get('name') == 'MTLAccelerationStructureCommandEncoder::buildInstances'
               for c in chunks):
            names['build'] = 'MTLAccelerationStructureCommandEncoder::buildInstances'
            instance_count = int(field(next(c for c in chunks if c.get('name') == names['build']),
                                       'count').text)
            cases.extend([
                ('build-zero-count', 'build', 0, 'count', '0'),
                ('build-other-count', 'build', 0, 'count',
                 str(instance_count + 1 if instance_count < 8 else 7)),
                ('build-large-count', 'build', 0, 'count', '65537'),
            ])
        if copied:
            names['copy'] = 'MTLAccelerationStructureCommandEncoder::copyAccelerationStructure'
            if primitive_copied:
                cases = [(tag, kind, 2 if kind == 'create' and index == 1 else index,
                          member, value) for tag, kind, index, member, value in cases]
            cases.extend([
                ('copy-zero-source', 'copy', 0, 'source', '0'),
                ('copy-wrong-source', 'copy', 0, 'source', '{instances}'),
                ('copy-zero-destination', 'copy', 0, 'destination', '0'),
                ('copy-self', 'copy', 0, 'destination',
                 '{primitive}' if primitive_copied else '{top}'),
                ('copy-zero-encoder', 'copy', 0, 'Encoder', '0'),
            ])
            if not primitive_copied:
                cases.append(('copy-prior-encoder', 'copy', 0, 'Encoder', '{first_encoder}'))
        if compacted:
            names['compact'] = ('MTLAccelerationStructureCommandEncoder::'
                                'copyAndCompactAccelerationStructure')
            names['end'] = 'MTLAccelerationStructureCommandEncoder::endEncoding'
            if primitive_compacted:
                cases = [(tag, kind, 2 if kind == 'create' and index == 1 else index,
                          member, value) for tag, kind, index, member, value in cases]
            cases.extend([
                ('compact-zero-source', 'compact', 0, 'source', '0'),
                ('compact-wrong-source', 'compact', 0, 'source', '{instances}'),
                ('compact-zero-destination', 'compact', 0, 'destination', '0'),
                ('compact-self', 'compact', 0, 'destination',
                 '{primitive}' if primitive_compacted else '{top}'),
                ('compact-zero-expected', 'compact', 0, 'expectedSize', '0'),
                ('compact-wrong-expected', 'compact', 0, 'expectedSize',
                 '1279' if primitive_compacted else '1535'),
                ('compact-zero-encoder', 'compact', 0, 'Encoder', '0'),
                ('end-zero-encoder', 'end', 2, 'Encoder', '0'),
                ('end-prior-encoder', 'end', 2, 'Encoder', '{first_encoder}'),
            ])
            if any(c.get('name') ==
                   'MTLAccelerationStructureCommandEncoder::buildIndexedTriangle'
                   for c in chunks):
                names['primitive_build'] = ('MTLAccelerationStructureCommandEncoder::'
                                            'buildIndexedTriangle')
                cases.extend([
                    ('primitive-zero-indices', 'primitive_build', 0, 'indices', '0'),
                    ('primitive-invalid-index-type', 'primitive_build', 0, 'indexType', '99'),
                    ('primitive-zero-count', 'primitive_build', 0, 'triangleCount', '0'),
                ])
            elif any(c.get('name') ==
                     'MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle' and
                     field(c, 'triangleCount').text == '2' for c in chunks):
                names['primitive_build'] = ('MTLAccelerationStructureCommandEncoder::'
                                            'buildNonOpaqueTriangle')
                cases.append(('primitive-count-oob', 'primitive_build', 0,
                              'triangleCount', '3'))
        descriptor_top = any(c.get('name') ==
                             'MTLDevice::newAccelerationStructureWithDescriptor'
                             for c in chunks)
        if descriptor_top:
            names['create_top'] = 'MTLDevice::newAccelerationStructureWithDescriptor'
            top_index = 2 if distinct or primitive_copied or primitive_compacted else 1
            cases = [(tag, 'create_top' if kind == 'create' and index == top_index else kind,
                      0 if kind == 'create' and index == top_index else index, member, value)
                     for tag, kind, index, member, value in cases]
        stages = ('Fragment', 'Vertex', 'Tile')
        render_stage = next((stage for stage in stages if any(
            c.get('name') == f'MTLRenderCommandEncoder::set{stage}AccelerationStructure'
            for c in chunks)), None)
        if render_stage:
            names['render_stage'] = f'MTLRenderCommandEncoder::set{render_stage}AccelerationStructure'
            tag = render_stage.lower()
            cases.extend([
                (f'{tag}-wrong-as', 'render_stage', 0, 'structure', '{instances}'),
                (f'{tag}-index-oob', 'render_stage', 0, 'index', '31'),
                (f'{tag}-zero-encoder', 'render_stage', 0, 'RenderCommandEncoder', '0'),
                (f'{tag}-wrong-encoder', 'render_stage', 0, 'RenderCommandEncoder', '{first_encoder}'),
            ])
        matching = lambda kind: [c for c in chunks if c.get('name') == names[kind]]
        if len(matching('bind')) == 2:
            cases.extend([
                ('clear-compute-wrong-as', 'bind', 1, 'structure', '{instances}'),
                ('clear-compute-index-oob', 'bind', 1, 'index', '31'),
                ('clear-compute-zero-encoder', 'bind', 1, 'ComputeCommandEncoder', '0'),
            ])
        if render_stage and len(matching('render_stage')) == 2:
            cases.extend([
                ('clear-render-wrong-as', 'render_stage', 1, 'structure', '{instances}'),
                ('clear-render-index-oob', 'render_stage', 1, 'index', '31'),
                ('clear-render-zero-encoder', 'render_stage', 1, 'RenderCommandEncoder', '0'),
            ])
        ids = {
            'primitive': field(matching('create')[0], 'Structure').text,
            'top': field(matching('create_top')[0] if descriptor_top else
                         matching('create')[2 if distinct or primitive_copied or
                                            primitive_compacted else 1],
                         'Structure').text,
            'vertices': field(next(c for c in chunks if c.get('name') in (
                              'MTLAccelerationStructureCommandEncoder::buildAccelerationStructure',
                              'MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle',
                              'MTLAccelerationStructureCommandEncoder::buildIndexedTriangle')),
                              'vertices').text,
            'instances': field(matching('build')[0], 'instances').text,
            'first_encoder': field(matching('begin')[0], 'Encoder').text,
        }
        for tag, kind, index, member, value in cases:
            variant = copy.deepcopy(tree)
            target = [c for c in variant.findall('./chunks/chunk')
                      if c.get('name') == names[kind]][index]
            field(target, member).text = value.format(**ids)
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
        for tag, shorten in [('descriptor-mismatch', False),
                             ('descriptor-short', True)]:
            variant = copy.deepcopy(tree)
            build = next(c for c in variant.findall('./chunks/chunk')
                         if c.get('name') == names['build'])
            data = field(build, 'descriptorBytes')
            if shorten:
                data.remove(data[-1])
            else:
                data[0].text = '1'
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'{capture.stem} instance AS malformed captures rejected: {len(cases) + 2} cases')


if __name__ == '__main__':
    main()
