#!/usr/bin/env python3
"""Reject malformed refittable indexed-triangle chunks before GPU execution."""
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
    build_name = 'MTLAccelerationStructureCommandEncoder::buildRefittableIndexedTriangle'
    refit_name = 'MTLAccelerationStructureCommandEncoder::refitIndexedTriangle'
    with tempfile.TemporaryDirectory(prefix='metal-indexed-refit-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)

        def target(name, source=tree):
            return next(c for c in source.findall('./chunks/chunk') if c.get('name') == name)

        build, refit = target(build_name), target(refit_name)
        index_type = '1' if capture.stem in (
            't267_capture', 't281_capture', 't283_capture', 't285_capture') else '0'
        vertex_format = '31' if capture.stem in (
            't272_capture', 't282_capture', 't283_capture') else '30'
        vertex_stride = '16' if vertex_format == '31' else '12'
        duplicate = 'false' if capture.stem in ('t273_capture', 't280_capture') else 'true'
        vertex_offset = '36' if capture.stem in (
            't276_capture', 't277_capture', 't280_capture') else \
            '16' if capture.stem in ('t282_capture', 't283_capture') else '0'
        index_offset = '8' if capture.stem in ('t281_capture', 't283_capture') else \
            '2' if capture.stem in ('t275_capture', 't280_capture') else '0'
        table_offset = '1' if capture.stem in ('t278_capture', 't280_capture') else '0'
        opaque = 'true' if capture.stem in ('t279_capture', 't280_capture') else 'false'
        count = '2' if capture.stem in (
            't284_capture', 't285_capture', 't286_capture', 't287_capture',
            't288_capture', 't289_capture') else '1'
        for node in (build, refit):
            assert field(node, 'indexType').text == index_type
            assert field(node, 'vertexFormat').text == vertex_format
            assert field(node, 'vertexStride').text == vertex_stride
            assert field(node, 'allowDuplicate').text == duplicate
            assert field(node, 'triangleCount').text == count
            assert field(node, 'vertexOffset').text == vertex_offset
            assert field(node, 'indexOffset').text == index_offset
            assert field(node, 'tableOffset').text == table_offset
            assert field(node, 'opaque').text == opaque
        assert field(refit, 'scratchOffset').text == \
            ('256' if capture.stem in ('t274_capture', 't280_capture') else '0')
        assert (field(build, 'indices').text != field(refit, 'indices').text) == \
            (capture.stem in ('t268_capture', 't287_capture', 't289_capture'))
        assert (field(build, 'vertices').text != field(refit, 'vertices').text) == \
            (capture.stem in ('t288_capture', 't289_capture'))
        ids = {key: field(build, key).text for key in
               ('structure', 'vertices', 'indices', 'scratch')}
        cases = [
            ('build-zero-encoder', build_name, 'Encoder', '0'),
            ('build-wrong-encoder', build_name, 'Encoder', ids['structure']),
            ('build-zero-structure', build_name, 'structure', '0'),
            ('build-wrong-structure', build_name, 'structure', ids['vertices']),
            ('build-zero-vertices', build_name, 'vertices', '0'),
            ('build-wrong-vertices', build_name, 'vertices', ids['structure']),
            ('build-misaligned-vertex-offset', build_name, 'vertexOffset', '3'),
            ('build-large-vertex-offset', build_name, 'vertexOffset', '65536'),
            ('build-zero-vertex-stride', build_name, 'vertexStride', '0'),
            ('build-short-vertex-stride', build_name, 'vertexStride', '8'),
            ('build-misaligned-vertex-stride', build_name, 'vertexStride', '13'),
            ('build-large-vertex-stride', build_name, 'vertexStride', '1048577'),
            ('build-invalid-vertex-format', build_name, 'vertexFormat', '99'),
            ('build-zero-indices', build_name, 'indices', '0'),
            ('build-wrong-indices', build_name, 'indices', ids['structure']),
            ('build-invalid-index-type', build_name, 'indexType', '99'),
            ('build-misaligned-index-offset', build_name, 'indexOffset', '1'),
            ('build-large-index-offset', build_name, 'indexOffset', '65536'),
            ('build-zero-count', build_name, 'triangleCount', '0'),
            ('build-other-count', build_name, 'triangleCount',
             '1' if count == '2' else '2'),
            ('build-huge-count', build_name, 'triangleCount', '1000001'),
            ('build-large-table-offset', build_name, 'tableOffset', '32'),
            ('build-zero-scratch', build_name, 'scratch', '0'),
            ('build-wrong-scratch', build_name, 'scratch', ids['vertices']),
            ('build-misaligned-scratch-offset', build_name, 'scratchOffset', '1'),
            ('build-large-scratch-offset', build_name, 'scratchOffset', '65536'),
            ('refit-zero-encoder', refit_name, 'Encoder', '0'),
            ('refit-wrong-encoder', refit_name, 'Encoder', ids['structure']),
            ('refit-zero-source', refit_name, 'source', '0'),
            ('refit-wrong-source', refit_name, 'source', ids['vertices']),
            ('refit-zero-destination', refit_name, 'destination', '0'),
            ('refit-wrong-destination', refit_name, 'destination', ids['scratch']),
            ('refit-zero-vertices', refit_name, 'vertices', '0'),
            ('refit-wrong-vertices', refit_name, 'vertices', ids['structure']),
            ('refit-other-vertex-offset', refit_name, 'vertexOffset',
             '0' if vertex_offset != '0' else '4'),
            ('refit-other-vertex-stride', refit_name, 'vertexStride',
             '12' if vertex_stride == '16' else '16'),
            ('refit-other-vertex-format', refit_name, 'vertexFormat',
             '30' if vertex_format == '31' else '31'),
            ('refit-zero-indices', refit_name, 'indices', '0'),
            ('refit-wrong-indices', refit_name, 'indices', ids['structure']),
            ('refit-other-index-type', refit_name, 'indexType',
             '1' if index_type == '0' else '0'),
            ('refit-other-index-offset', refit_name, 'indexOffset',
             '0' if index_offset != '0' else '2'),
            ('refit-zero-count', refit_name, 'triangleCount', '0'),
            ('refit-other-count', refit_name, 'triangleCount',
             '1' if count == '2' else '2'),
            ('refit-other-table-offset', refit_name, 'tableOffset',
             '0' if table_offset == '1' else '1'),
            ('refit-zero-scratch', refit_name, 'scratch', '0'),
            ('refit-wrong-scratch', refit_name, 'scratch', ids['vertices']),
            ('refit-misaligned-scratch-offset', refit_name, 'scratchOffset', '1'),
            ('refit-large-scratch-offset', refit_name, 'scratchOffset', '65536'),
            ('refit-other-opaque', refit_name, 'opaque',
             'false' if opaque == 'true' else 'true'),
            ('refit-other-duplicate', refit_name, 'allowDuplicate',
             'true' if duplicate == 'false' else 'false'),
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
        print(f'{capture.stem} malformed indexed refits rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
