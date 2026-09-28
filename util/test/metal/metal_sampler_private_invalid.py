#!/usr/bin/env python3
"""T38/T39: reject malformed sampler clamps and buffer-output compute dispatches."""
import copy
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def run(*args, success=True):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=30)
    if result.returncode < 0 or (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected result {result.returncode}: {args}\n{result.stdout}")
    return result.stdout


def child(node, field):
    return next(item for item in node if item.get('name') == field)


def make_cases(tree, lod):
    cases = []
    def edit(tag, selector, path, value):
        variant = copy.deepcopy(tree)
        node = next(node for node in variant.findall('./chunks/chunk') if selector(node))
        for part in path.split('.'):
            node = node[int(part)] if part.isdigit() else child(node, part)
        if value is None:
            node.remove(node[-1])
        else:
            node.text = str(value)
        cases.append((tag, variant))
        return variant

    if lod:
        for stage in ('Vertex', 'Fragment', 'Compute'):
            prefix = 'MTLComputeCommandEncoder::setSampler' if stage == 'Compute' else \
                     f'MTLRenderCommandEncoder::set{stage}Sampler'
            def single(node):
                return node.get('name', '').startswith(prefix + 'State') and \
                       any(item.get('name') == 'lodMinClamp' for item in node)
            def batch(node):
                return node.get('name', '').startswith(prefix + 'States') and \
                       any(item.get('name') == 'lodMinClamps' for item in node)
            for field, value in [('index', 16), ('sampler', 0), ('bound', 'false'),
                                 ('lodMinClamp', -1), ('lodMaxClamp', 0),
                                 ('lodMinClamp', 'nan'), ('lodMaxClamp', 'inf')]:
                edit(f'{stage}-single-{field}-{value}', single, field, value)
            for field, value in [('range.location', 15), ('range.length', 2**64 - 1),
                                 ('samplers', None), ('bound', None), ('lodMinClamps', None),
                                 ('lodMaxClamps', None), ('samplers.0', 0), ('bound.0', 0),
                                 ('lodMinClamps.0', -1), ('lodMaxClamps.0', 0),
                                 ('lodMinClamps.0', 'nan')]:
                edit(f'{stage}-batch-{field}', batch, field, value)
    bind = lambda node: node.get('name') == 'MTLComputeCommandEncoder::setBuffer'
    edit('missing-output', bind, 'buffer', 0)
    edit('past-output', bind, 'offset', 48 if lod else 516)
    if lod:
        edit('short-float4-output', bind, 'offset', 40)
        edit('misaligned-float4-output', bind, 'offset', 4)
        edit('missing-sampled-texture',
             lambda node: node.get('name') == 'MTLComputeCommandEncoder::setTexture', 'texture', 0)
        variant = edit('nil-active-sampler',
                       lambda node: node.get('name') == 'MTLComputeCommandEncoder::setSamplerState_lodclamp',
                       'bound', 'false')
        node = next(node for node in variant.findall('./chunks/chunk')
                    if node.get('name') == 'MTLComputeCommandEncoder::setSamplerState_lodclamp')
        child(node, 'sampler').text = '0'
    name = 'dispatchThreadgroups' if lod else 'dispatchThreads'
    dispatch = lambda node: node.get('name') == 'MTLComputeCommandEncoder::' + name
    grid = 'groups' if lod else 'grid'
    for field, value in [(f'{grid}.width', 0), (f'{grid}.width', 2**64 - 1),
                         ('threadsPerGroup.width', 1025), ('threadsPerGroup.height', 0),
                         ('ComputeCommandEncoder', 0)]:
        edit(f'dispatch-{field}-{value}', dispatch, field, value)
    return cases


def main():
    if len(sys.argv) != 4:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t38_capture.rdc t39_capture.rdc')
    command, *captures = map(pathlib.Path, sys.argv[1:])
    total = 0
    with tempfile.TemporaryDirectory(prefix='metal-sampler-private-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        for index, capture in enumerate(captures):
            original = directory / f't{38 + index}.zip.xml'
            run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
            cases = make_cases(ET.parse(original), lod=index == 0)
            for tag, variant in cases:
                xml = directory / f't{38 + index}-{tag}.zip.xml'
                variant.write(xml, encoding='unicode', xml_declaration=True)
                shutil.copyfile(str(original)[:-4], str(xml)[:-4])
                invalid = directory / f't{38 + index}-{tag}.rdc'
                run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
                message = run(command, 'replay', '--loops', '1', invalid, success=False)
                if not re.search(r'failed|invalid|unsupported|missing|Couldn.t load', message, re.I):
                    raise RuntimeError(f'missing diagnostic: {tag}: {message}')
            total += len(cases)
    print(f'T38/T39 malformed captures rejected without crash: {total} cases')


if __name__ == '__main__':
    main()
