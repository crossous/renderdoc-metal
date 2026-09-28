#!/usr/bin/env python3
"""Reject malformed rasterization rate maps, parameter copies and pass references."""
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


def child(node, part):
    if part.isdigit():
        return node[int(part)]
    return next(item for item in node if item.get('name') == part)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd rate_map_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    cases = [
        ('map-zero', 'creation', 'RateMap', '0'),
        ('screen-width-zero', 'creation', 'screenSize.width', '0'),
        ('screen-height-too-large', 'creation', 'screenSize.height', '8193'),
        ('horizontal-negative', 'creation', 'horizontal.0', '-0.1'),
        ('horizontal-over-one', 'creation', 'horizontal.1', '1.1'),
        ('vertical-negative', 'creation', 'vertical.0', '-0.1'),
        ('vertical-over-one', 'creation', 'vertical.1', '1.1'),
        ('unsupported', 'creation', 'supported', 'false'),
        ('horizontal-short', 'creation', 'horizontal.1', None),
        ('vertical-short', 'creation', 'vertical.1', None),
        ('copy-map-zero', 'copy', 'RateMap', '0'),
        ('copy-buffer-zero', 'copy', 'buffer', '0'),
        ('copy-offset-unaligned', 'copy', 'offset', '3'),
        ('copy-offset-out-of-bounds', 'copy', 'offset', '24628'),
        ('pass-map-unknown', 'pass', 'descriptor.rasterizationRateMap', '999999'),
        ('pass-map-id-zero', 'pass', 'descriptor.rasterizationRateMapId', '0'),
        ('pass-map-id-unknown', 'pass', 'descriptor.rasterizationRateMapId', '999999'),
    ]
    names = {
        'creation': 'MTLDevice::newRasterizationRateMapWithDescriptor',
        'copy': 'MTLRasterizationRateMap::copyParameterDataToBuffer',
        'pass': 'MTLCommandBuffer::renderCommandEncoderWithDescriptor',
    }
    with tempfile.TemporaryDirectory(prefix='metal-rate-map-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't95.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        tree = ET.parse(original)
        creation = next(item for item in tree.findall('./chunks/chunk')
                        if item.get('name') == names['creation'])
        extras = next((item for item in creation if item.get('name') == 'extraHorizontal'), None)
        two_layers = extras is not None and len(extras) == 1
        pass_chunk = next(item for item in tree.findall('./chunks/chunk')
                          if item.get('name') == names['pass'])
        pass_map_id = child(child(pass_chunk, 'descriptor'), 'rasterizationRateMapId')
        bound_two_layers = two_layers and pass_map_id.text != '0'
        if bound_two_layers:
            cases += [
                ('pass-array-length-one', 'pass', 'descriptor.renderTargetArrayLength', '1'),
                ('pass-array-length-zero', 'pass', 'descriptor.renderTargetArrayLength', '0'),
            ]
        elif not two_layers:
            cases += [
                ('pass-array-length-two', 'pass', 'descriptor.renderTargetArrayLength', '2'),
            ]
        if two_layers:
            if not bound_two_layers:
                cases = [case for case in cases if case[0] not in
                         ('pass-map-unknown', 'pass-map-id-zero')]
            cases += [
                ('extra-horizontal-negative', 'creation', 'extraHorizontal.0.0', '-0.1'),
                ('extra-horizontal-over-one', 'creation', 'extraHorizontal.0.1', '1.1'),
                ('extra-vertical-negative', 'creation', 'extraVertical.0.0', '-0.1'),
                ('extra-vertical-over-one', 'creation', 'extraVertical.0.1', '1.1'),
                ('extra-horizontal-short', 'creation', 'extraHorizontal.0.1', None),
                ('extra-vertical-short', 'creation', 'extraVertical.0.1', None),
                ('extra-layer-mismatch', 'creation', 'extraVertical.0', None),
            ]
        for tag, kind, path, value in cases:
            variant = copy.deepcopy(tree)
            chunk = next(item for item in variant.findall('./chunks/chunk')
                         if item.get('name') == names[kind])
            node = chunk
            parts = path.split('.')
            for part in parts[:-1]:
                node = child(node, part)
            target = child(node, parts[-1])
            if value is None:
                node.remove(target)
            else:
                target.text = value
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            assert ('Failed to process Metal chunk' in message or
                    'Failed to replay Metal chunk' in message), (tag, message)
    print(f'Rasterization rate map malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
