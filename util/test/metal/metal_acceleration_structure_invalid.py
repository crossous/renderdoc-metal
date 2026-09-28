#!/usr/bin/env python3
"""Malformed T135-T141 AS build/copy/refit/binding chunks must fail safely."""
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
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t135_to_t141_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    names = {
        'create': 'MTLDevice::newAccelerationStructureWithSize',
        'begin': 'MTLCommandBuffer::accelerationStructureCommandEncoder',
        'build': 'MTLAccelerationStructureCommandEncoder::buildAccelerationStructure',
        'copy': 'MTLAccelerationStructureCommandEncoder::copyAccelerationStructure',
        'compact': 'MTLAccelerationStructureCommandEncoder::copyAndCompactAccelerationStructure',
        'write': 'MTLAccelerationStructureCommandEncoder::writeCompactedAccelerationStructureSize',
        'end': 'MTLAccelerationStructureCommandEncoder::endEncoding',
        'refit': 'MTLAccelerationStructureCommandEncoder::refitTriangle',
        'binding': 'MTLComputeCommandEncoder::setAccelerationStructure',
    }
    cases = [
        ('create-zero-id', 'create', 'Structure', '0'),
        ('create-zero-size', 'create', 'size', '0'),
        ('create-huge-size', 'create', 'size', '1073741825'),
        ('begin-zero-id', 'begin', 'Encoder', '0'),
        ('build-zero-as', 'build', 'structure', '0'),
        ('build-wrong-as', 'build', 'structure', '{geometry}'),
        ('build-zero-scratch', 'build', 'scratch', '0'),
        ('write-zero-as', 'write', 'structure', '0'),
        ('write-wrong-buffer', 'write', 'buffer', '{structure}'),
        ('write-offset-oob', 'write', 'offset', '8'),
        ('write-invalid-type', 'write', 'sizeDataType', '0'),
        ('end-zero-encoder', 'end', 'Encoder', '0'),
    ]
    with tempfile.TemporaryDirectory(prefix='metal-as-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't135.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        if any(c.get('name') == 'MTLDevice::newAccelerationStructureWithDescriptor'
               for c in chunks):
            names['create'] = 'MTLDevice::newAccelerationStructureWithDescriptor'
        indexed = any(c.get('name') ==
                      'MTLAccelerationStructureCommandEncoder::buildIndexedTriangle'
                      for c in chunks)
        nonopaque = any(c.get('name') ==
                        'MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle'
                        for c in chunks)
        strided_box = any(c.get('name') ==
                          'MTLAccelerationStructureCommandEncoder::buildBoundingBoxStrided'
                          for c in chunks)
        extended_box = any(c.get('name') ==
                           'MTLAccelerationStructureCommandEncoder::buildBoundingBoxExtended'
                           for c in chunks)
        boxed = strided_box or extended_box or any(c.get('name') ==
                                   'MTLAccelerationStructureCommandEncoder::buildBoundingBox'
                                   for c in chunks)
        copied = any(c.get('name') == names['copy'] for c in chunks)
        compacted = any(c.get('name') == names['compact'] for c in chunks)
        extended_refit = any(c.get('name') ==
                             'MTLAccelerationStructureCommandEncoder::refitTriangleExtended'
                             for c in chunks)
        if extended_refit:
            names['refit'] = 'MTLAccelerationStructureCommandEncoder::refitTriangleExtended'
        refittable = any(c.get('name') == names['refit'] for c in chunks)
        out_of_place_refit = extended_refit and any(
            field(c, 'source').text != field(c, 'destination').text
            for c in chunks if c.get('name') == names['refit'])
        if refittable:
            names['build'] = ('MTLAccelerationStructureCommandEncoder::'
                              'buildRefittableTriangle')
            if not compacted:
                cases = [case for case in cases if case[1] != 'write']
        if boxed:
            names['build'] = ('MTLAccelerationStructureCommandEncoder::' +
                              ('buildBoundingBoxStrided' if strided_box else
                               'buildBoundingBoxExtended' if extended_box else
                               'buildBoundingBox'))
        elif indexed:
            names['build'] = 'MTLAccelerationStructureCommandEncoder::buildIndexedTriangle'
        elif nonopaque:
            names['build'] = 'MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle'
        geometry = 'boxes' if boxed else 'vertices'
        count = 'boxCount' if boxed else 'triangleCount'
        cases.extend([
            ('build-zero-geometry', 'build', geometry, '0'),
            ('build-wrong-geometry', 'build', geometry, '{structure}'),
            ('build-zero-count', 'build', count, '0'),
            ('build-large-count', 'build', count, '1000001'),
        ])
        if indexed:
            cases.extend([
                ('build-zero-indices', 'build', 'indices', '0'),
                ('build-wrong-indices', 'build', 'indices', '{structure}'),
                ('build-invalid-index-type', 'build', 'indexType', '99'),
            ])
            build_chunk = next(c for c in chunks if c.get('name') == names['build'])
            if field(build_chunk, 'triangleCount').text == '2':
                cases.append(('build-index-range-oob', 'build', 'triangleCount', '3'))
        elif boxed:
            cases.extend([
                ('build-zero-encoder', 'build', 'Encoder', '0'),
                ('build-wrong-scratch', 'build', 'scratch', '{geometry}'),
            ])
            if extended_box or strided_box:
                cases.extend([
                    ('build-box-offset-misaligned', 'build', 'boxOffset', '1'),
                    ('build-box-offset-oob', 'build', 'boxOffset', '65536'),
                    ('build-scratch-offset-misaligned', 'build', 'scratchOffset', '1'),
                    ('build-scratch-offset-oob', 'build', 'scratchOffset', '65536'),
                ])
                build_chunk = next(c for c in chunks if c.get('name') == names['build'])
                if extended_box and field(build_chunk, 'boxOffset').text == '0':
                    cases.append(('build-zero-both-offsets', 'build', 'scratchOffset', '0'))
                elif extended_box and field(build_chunk, 'scratchOffset').text == '0':
                    cases.append(('build-zero-both-offsets', 'build', 'boxOffset', '0'))
                if strided_box:
                    cases.extend([
                        ('build-stride-zero', 'build', 'boxStride', '0'),
                        ('build-stride-small', 'build', 'boxStride', '16'),
                        ('build-stride-misaligned', 'build', 'boxStride', '31'),
                        ('build-stride-huge', 'build', 'boxStride', '1048584'),
                        ('build-stride-default', 'build', 'boxStride', '24'),
                    ])
                    if field(build_chunk, 'boxCount').text == '2':
                        cases.append(('build-stride-count-oob', 'build', 'boxCount', '3'))
        else:
            cases.extend([
                ('build-vertex-oob', 'build', 'vertexOffset', '36'),
                ('build-scratch-oob', 'build', 'scratchOffset', '6401'),
            ])
        if copied:
            cases.extend([
                ('copy-zero-source', 'copy', 'source', '0'),
                ('copy-wrong-source', 'copy', 'source', '{geometry}'),
                ('copy-zero-destination', 'copy', 'destination', '0'),
                ('copy-self', 'copy', 'destination', '{structure}'),
            ])
        if refittable:
            refit_source = 'source' if extended_refit else 'structure'
            cases.extend([
                ('refit-zero-as', 'refit', refit_source, '0'),
                ('refit-wrong-as', 'refit', refit_source, '{geometry}'),
                ('refit-zero-vertices', 'refit', 'vertices', '0'),
                ('refit-wrong-vertices', 'refit', 'vertices', '{structure}'),
                ('refit-zero-count', 'refit', 'triangleCount', '0'),
                ('refit-large-count', 'refit', 'triangleCount', '1000001'),
                ('refit-zero-scratch', 'refit', 'scratch', '0'),
                ('refit-wrong-scratch', 'refit', 'scratch', '{geometry}'),
                ('refit-zero-encoder', 'refit', 'Encoder', '0'),
                ('refit-prior-encoder', 'refit', 'Encoder', '{first_encoder}'),
                ('binding-wrong-as', 'binding', 'structure', '{geometry}'),
                ('binding-index-oob', 'binding', 'index', '31'),
                ('binding-zero-encoder', 'binding', 'ComputeCommandEncoder', '0'),
            ])
            if extended_refit:
                cases.extend([
                    ('refit-zero-destination', 'refit', 'destination', '0'),
                    ('refit-wrong-destination', 'refit', 'destination', '{geometry}'),
                    ('refit-scratch-offset-misaligned', 'refit', 'scratchOffset', '1'),
                    ('refit-scratch-offset-oob', 'refit', 'scratchOffset', '65536'),
                    ('binding-after-wrong-as', 'binding_after', 'structure', '{geometry}'),
                ])
                names['binding_after'] = names['binding']
                refit_chunk = next(c for c in chunks if c.get('name') == names['refit'])
                if field(refit_chunk, 'source').text != field(refit_chunk, 'destination').text:
                    cases.append(('refit-collapse-to-in-place', 'refit', 'destination',
                                  '{structure}'))
                else:
                    cases.append(('refit-zero-offset', 'refit', 'scratchOffset', '0'))
            build_count = field(next(c for c in chunks if c.get('name') == names['build']),
                                'triangleCount').text
            if build_count == '2':
                cases.extend([
                    ('build-refit-count-mismatch', 'build', 'triangleCount', '1'),
                    ('refit-build-count-mismatch', 'refit', 'triangleCount', '1'),
                ])
        if compacted:
            names['create_destination'] = 'MTLDevice::newAccelerationStructureWithSize'
            cases.extend([
                ('compact-zero-source', 'compact', 'source', '0'),
                ('compact-wrong-source', 'compact', 'source', '{geometry}'),
                ('compact-zero-destination', 'compact', 'destination', '0'),
                ('compact-self', 'compact', 'destination', '{refit_destination}'),
                ('compact-zero-size-buffer', 'compact', 'sizeBuffer', '0'),
                ('compact-wrong-size-buffer', 'compact', 'sizeBuffer', '{geometry}'),
                ('compact-zero-expected', 'compact', 'expectedSize', '0'),
                ('compact-huge-expected', 'compact', 'expectedSize', '65536'),
                ('compact-wrong-expected', 'compact', 'expectedSize',
                 '1791' if refittable else '1279'),
                ('compact-size-offset-oob', 'compact', 'sizeOffset', '8'),
                ('compact-invalid-size-type', 'compact', 'sizeDataType', '0'),
                ('compact-target-undersized', 'create_destination', 'size', '1'),
                ('compact-target-not-smaller', 'create_destination', 'size',
                 '2048' if refittable else '1536'),
                ('compact-prior-encoder', 'compact', 'Encoder', '{first_encoder}'),
            ])
            if out_of_place_refit:
                cases.append(('compact-prior-source', 'compact', 'source', '{structure}'))
        ids = {
            'structure': field(next(c for c in chunks if c.get('name') == names['create']),
                               'Structure').text,
            'geometry': field(next(c for c in chunks if c.get('name') == names['build']),
                              geometry).text,
            'first_encoder': field(next(c for c in chunks if c.get('name') == names['begin']),
                                   'Encoder').text,
            'refit_destination': field(next(c for c in chunks if c.get('name') == names['refit']),
                                       'destination').text if extended_refit else
                                 field(next(c for c in chunks if c.get('name') == names['create']),
                                       'Structure').text,
        }
        for tag, kind, member, value in cases:
            variant = copy.deepcopy(tree)
            matches = [c for c in variant.findall('./chunks/chunk')
                       if c.get('name') == names[kind]]
            destination_is_second = (kind == 'create_destination' and
                                     names['create'] == names['create_destination'])
            if destination_is_second:
                target = matches[2 if out_of_place_refit else 1]
            else:
                target = matches[1] if kind == 'binding_after' else matches[0]
            field(target, member).text = value.format(**ids)
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
        extra_cases = 0
        if extended_box or strided_box:
            scratch_id = field(next(c for c in chunks if c.get('name') == names['build']),
                               'scratch').text
            scratch_buffer = next((c for c in chunks if c.get('name') ==
                                   'MTLDevice::newBufferWithLength' and
                                   field(c, 'Buffer').text == scratch_id), None)
            if scratch_buffer is not None:
                original_length = int(field(scratch_buffer, 'length').text)
                variant = copy.deepcopy(tree)
                target = next(c for c in variant.findall('./chunks/chunk') if c.get('name') ==
                              'MTLDevice::newBufferWithLength' and
                              field(c, 'Buffer').text == scratch_id)
                field(target, 'length').text = str(original_length - 1)
                xml = directory / 'build-scratch-undersized.zip.xml'
                variant.write(xml, encoding='unicode', xml_declaration=True)
                shutil.copyfile(str(original)[:-4], str(xml)[:-4])
                invalid = directory / 'build-scratch-undersized.rdc'
                run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
                message = run(command, 'replay', '--loops', '1', invalid, success=False)
                assert ('Failed to process Metal chunk' in message or
                        'Failed to replay Metal chunk' in message), message
                extra_cases += 1
        if extended_refit:
            scratch_id = field(next(c for c in chunks if c.get('name') == names['refit']),
                               'scratch').text
            tight_scratch = next((c for c in chunks if c.get('name') ==
                                  'MTLDevice::newBufferWithLength' and
                                  field(c, 'Buffer').text == scratch_id and
                                  field(c, 'length').text == '512'), None)
            if tight_scratch is not None:
                variant = copy.deepcopy(tree)
                target = next(c for c in variant.findall('./chunks/chunk') if c.get('name') ==
                              'MTLDevice::newBufferWithLength' and
                              field(c, 'Buffer').text == scratch_id)
                field(target, 'length').text = '511'
                xml = directory / 'refit-scratch-undersized.zip.xml'
                variant.write(xml, encoding='unicode', xml_declaration=True)
                shutil.copyfile(str(original)[:-4], str(xml)[:-4])
                invalid = directory / 'refit-scratch-undersized.rdc'
                run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
                message = run(command, 'replay', '--loops', '1', invalid, success=False)
                assert ('Failed to process Metal chunk' in message or
                        'Failed to replay Metal chunk' in message), message
                extra_cases += 1
    print(f'{capture.stem} AS malformed captures rejected without crash: '
          f'{len(cases) + extra_cases} cases')


if __name__ == '__main__':
    main()
