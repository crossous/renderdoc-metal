#!/usr/bin/env python3
"""Malformed visible-table residency declarations must fail before native replay."""
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
    with tempfile.TemporaryDirectory(prefix='metal-table-residency-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 'original.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        chunks = tree.findall('./chunks/chunk')
        visible = next(c for c in chunks if c.get('name') ==
                       'MTLRenderPipelineState::newVisibleFunctionTableWithDescriptor')
        visible_id = field(visible, 'Table').text
        build = next(c for c in chunks if c.get('name') ==
                     'MTLAccelerationStructureCommandEncoder::buildNonOpaqueTriangle')
        structure_id = field(build, 'structure').text
        wrong_id = field(build, 'Encoder').text
        def target(source):
            return next(c for c in source.findall('./chunks/chunk')
                        if c.get('name') == 'MTLRenderCommandEncoder::useResource' and
                        field(c, 'resource').text == visible_id)
        # AS residency is legal after B478, just like VK/DX12 RT resource references.
        # Retain this former negative as a positive and use an encoder for wrong type.
        valid = copy.deepcopy(tree)
        field(target(valid), 'resource').text = structure_id
        valid_xml = directory / 'as-residency.zip.xml'
        valid.write(valid_xml, encoding='unicode', xml_declaration=True)
        shutil.copyfile(str(original)[:-4], str(valid_xml)[:-4])
        valid_capture = directory / 'as-residency.rdc'
        run(command, 'convert', '-f', valid_xml, '-o', valid_capture, '-c', 'rdc')
        run(command, 'replay', '--loops', '3', valid_capture)
        cases = [
            ('wrong-type', 'resource', wrong_id),
            ('missing', 'resource', '99999999'),
            ('zero-resource', 'resource', '0'),
            ('zero-usage', 'usageValue', '0'),
            ('invalid-usage', 'usageValue', '8'),
            ('zero-stage', 'stagesValue', '0'),
            ('invalid-stage', 'stagesValue', '8'),
            ('wrong-encoder', 'RenderCommandEncoder', wrong_id),
        ]
        for tag, member, value in cases:
            variant = copy.deepcopy(tree)
            field(target(variant), member).text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
        print(f'{capture.stem} AS residency valid: 3 loops; table residency malformed captures rejected: {len(cases)} cases')


if __name__ == '__main__':
    main()
