#!/usr/bin/env python3
"""Reject malformed T40 inline bytes, offsets and threadgroup memory before GPU dispatch."""
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
        raise RuntimeError(f'unexpected result {result.returncode}: {args}\n{result.stdout}')
    return result.stdout


def child(node, field):
    return next(item for item in node if item.get('name') == field)


def make_cases(tree):
    cases = []
    def edit(tag, method, field, value, occurrence=0):
        variant = copy.deepcopy(tree)
        nodes = [node for node in variant.findall('./chunks/chunk')
                 if node.get('name') == 'MTLComputeCommandEncoder::' + method]
        node = nodes[occurrence]
        target = node
        for part in field.split('.'):
            target = child(target, part)
        if callable(value):
            value(target)
        else:
            target.text = str(value)
        cases.append((tag, variant))
        return variant

    for method in ('setBytes', 'setBufferOffset', 'setThreadgroupMemoryLength'):
        edit(method + '-slot', method, 'index', 31)
        edit(method + '-encoder', method, 'ComputeCommandEncoder', 0)
    # Preserve array metadata when modifying the payload length.
    def resize(node, length):
        original = copy.deepcopy(node[0])
        for item in list(node):
            node.remove(item)
        for _ in range(length):
            node.append(copy.deepcopy(original))
    edit('inline-empty-required', 'setBytes', 'data', lambda node: resize(node, 0))
    edit('inline-short-required', 'setBytes', 'data', lambda node: resize(node, 4))
    edit('inline-too-large', 'setBytes', 'data', lambda node: resize(node, 4097))
    # First offset update is on the 80-byte output buffer; slot 2 is currently inline.
    for field, value in [('offset', 80), ('offset', 2**64 - 1), ('offset', 79),
                         ('offset', 1), ('index', 2), ('index', 3)]:
        edit(f'offset-{field}-{value}', 'setBufferOffset', field, value)
    # Third offset call targets the real uint4 constant block after a rebind.
    for value in (256, 252, 4, 2**64 - 1):
        edit(f'constant-offset-{value}', 'setBufferOffset', 'offset', value, occurrence=2)
    for value in (1, 15, 17, 2**64 - 1, 2**64 - 16, 0):
        edit(f'threadgroup-length-{value}', 'setThreadgroupMemoryLength', 'length', value)
    edit('threadgroup-missing-slot', 'setThreadgroupMemoryLength', 'index', 3)
    # Two individually legal 32KiB allocations exceed Apple M2 Pro's shared memory budget.
    variant = edit('threadgroup-aggregate-limit', 'setThreadgroupMemoryLength', 'length', 32768)
    memory = [node for node in variant.findall('./chunks/chunk')
              if node.get('name') == 'MTLComputeCommandEncoder::setThreadgroupMemoryLength']
    child(memory[1], 'length').text = '32768'
    # Remove a required allocation/inline binding only in the second encoder. State must not leak.
    for method in ('setThreadgroupMemoryLength', 'setBytes'):
        variant = copy.deepcopy(tree)
        chunks = variant.find('./chunks')
        nodes = [node for node in chunks if node.get('name') == 'MTLComputeCommandEncoder::' + method]
        chunks.remove(nodes[-1])
        cases.append((method + '-new-encoder-reset', variant))
    for method in ('dispatchThreadgroups', 'dispatchThreads'):
        for field, value in [('threadsPerGroup.width', 0), ('threadsPerGroup.height', 0),
                             ('threadsPerGroup.width', 2**64 - 1),
                             ('threadsPerGroup.width', 1025),
                             # dispatchThreadgroups permits a 3D local size. Reject the
                             # actual total-thread limit, not a legal depth of two.
                             ('threadsPerGroup.depth', 1025),
                             ('ComputeCommandEncoder', 0)]:
            edit(f'{method}-{field}-{value}', method, field, value)
    return cases


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f'usage: {sys.argv[0]} renderdoccmd t40_capture.rdc')
    command, capture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix='metal-compute-inline-invalid-') as tmp:
        directory = pathlib.Path(tmp)
        original = directory / 't40.zip.xml'
        run(command, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml')
        cases = make_cases(ET.parse(original))
        for tag, variant in cases:
            xml = directory / f'{tag}.zip.xml'
            variant.write(xml, encoding='unicode', xml_declaration=True)
            shutil.copyfile(str(original)[:-4], str(xml)[:-4])
            invalid = directory / f'{tag}.rdc'
            run(command, 'convert', '-f', xml, '-o', invalid, '-c', 'rdc')
            message = run(command, 'replay', '--loops', '1', invalid, success=False)
            if not re.search(r'failed|invalid|unsupported|missing|Couldn.t load', message, re.I):
                raise RuntimeError(f'missing diagnostic: {tag}: {message}')
    print(f'T40 malformed captures rejected without crash: {len(cases)} cases')


if __name__ == '__main__':
    main()
