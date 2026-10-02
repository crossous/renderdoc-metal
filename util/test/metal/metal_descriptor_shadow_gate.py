#!/usr/bin/env python3
"""Sourced slot shadow mutations must fail before frame GPU submission."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:])
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
               RENDERDOC_METAL_TRACE_INITIAL_PRIVATE='1')
    def run(label, args, refuse=False):
        result = subprocess.run(list(map(str, args)), capture_output=True, text=True, env=env, timeout=20)
        output = result.stdout + result.stderr
        (folder / (label + '.log')).write_text(output)
        assert result.returncode in ((1, 4) if refuse else (0,)), (label, result.returncode, output)
        if refuse:
            assert 'failed' in output.lower(), (label, output)
            assert 'Metal replay wait begin' not in output, (label, output)
            assert 'Private initial contents upload' not in output, (label, output)
    def field(c, name):
        return next(n for n in c if n.get('name') == name)
    def first(chunks, name):
        return next(c for c in chunks if c.get('name') == name)
    run('positive-api', [opener, capture])
    run('positive-cli', [cli, 'replay', '--loops', '1', capture])
    xml = folder / 'source.zip.xml'
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs = {name: archive.read(name) for name in archive.namelist()}
    cases = ['missing-slot-source', 'unknown-slot-source', 'source-offset-bounds', 'wrong-generation',
        'overlapping-slot', 'slot-byte-address-mismatch', 'texture-field-unsupported',
        'missing-inline-source', 'duplicate-inline-source', 'inline-length', 'bytes-before-layout',
        'inline-after-end', 'unknown-encoder', 'render-stage-unsupported',
        'gpu-expected-unsupported', 'cpu-write-after-dispatch', 'oversized-heap', 'second-command',
        'missing-pipeline', 'missing-output', 'oversized-grid', 'oversized-drawable']
    for case in cases:
        tree, data = copy.deepcopy(original), dict(blobs)
        chunks = tree.find('./chunks')
        scope = next(i for i,c in enumerate(chunks) if c.get('id') == '5')
        frame = list(chunks)[scope+1:]
        slot_source = first(frame, 'MTLBuffer::DescriptorSlotBinding')
        slot_write = next(c for c in frame if c.get('name') == 'MTLBuffer::DescriptorSlotEvent' and field(c, 'event').text == '2')
        slot_allocate = next(c for c in frame if c.get('name') == 'MTLBuffer::DescriptorSlotEvent' and field(c, 'event').text == '0')
        inline_layout = first(frame, 'MTLCommandEncoder::DescriptorInlineLayout')
        inline_source = first(frame, 'MTLCommandEncoder::DescriptorInlineBinding')
        setter = first(frame, 'MTLComputeCommandEncoder::setBytes')
        if case == 'missing-slot-source': chunks.remove(slot_source)
        elif case == 'unknown-slot-source': field(slot_source, 'resource').text = '999999'
        elif case == 'source-offset-bounds': field(slot_source, 'memberOffset').text = '12'
        elif case == 'wrong-generation': field(slot_allocate, 'generation').text = '1'
        elif case == 'overlapping-slot': field(slot_allocate, 'offset').text = '8'
        elif case in ('slot-byte-address-mismatch', 'texture-field-unsupported'):
            key = f'{int(field(slot_write, "data").text):06d}'
            value = bytearray(data[key]); start = 0 if case == 'slot-byte-address-mismatch' else 8
            value[start:start+8] = b'\xff' * 8; data[key] = bytes(value)
        elif case == 'missing-inline-source': chunks.remove(inline_source)
        elif case == 'duplicate-inline-source': chunks.insert(list(chunks).index(inline_source), copy.deepcopy(inline_source))
        elif case == 'inline-length': field(inline_layout, 'stride').text = '24'
        elif case == 'bytes-before-layout':
            chunks.remove(setter); chunks.insert(list(chunks).index(inline_layout), setter)
        elif case == 'inline-after-end':
            end = first(frame, 'MTLComputeCommandEncoder::endEncoding')
            chunks.remove(setter); chunks.insert(list(chunks).index(end)+1, setter)
        elif case == 'unknown-encoder': field(inline_layout, 'encoder').text = '999999'
        elif case == 'render-stage-unsupported': field(inline_layout, 'stage').text = '1'
        elif case == 'gpu-expected-unsupported':
            field(slot_write, 'event').text = '3'; field(slot_write, 'generation').text = '0'
        elif case == 'cpu-write-after-dispatch':
            dispatch = first(frame, 'MTLComputeCommandEncoder::dispatchThreadgroups')
            chunks.remove(slot_write); chunks.insert(list(chunks).index(dispatch)+1, slot_write)
        elif case == 'oversized-heap': field(first(chunks, 'MTLDevice::newHeapWithDescriptor'), 'size').text = str(2*1024*1024)
        elif case == 'second-command':
            command = next(c for c in frame if c.get('name', '').startswith('MTLCommandQueue::commandBuffer'))
            chunks.insert(list(chunks).index(command)+1, copy.deepcopy(command))
        elif case == 'missing-pipeline': chunks.remove(first(frame, 'MTLComputeCommandEncoder::setComputePipelineState'))
        elif case == 'missing-output': chunks.remove(first(frame, 'MTLComputeCommandEncoder::setBuffer'))
        elif case == 'oversized-grid':
            field(field(first(frame, 'MTLComputeCommandEncoder::dispatchThreadgroups'), 'groups'), 'width').text = '2'
        elif case == 'oversized-drawable':
            field(field(first(chunks, '[CAMetalLayer nextDrawable]'), 'descriptor'), 'width').text = '4096'
        else: raise AssertionError(case)
        for i,c in enumerate(chunks): c.set('chunkIndex', str(i))
        altered = folder / (case + '.zip.xml')
        tree.write(altered, encoding='unicode', xml_declaration=True)
        with zipfile.ZipFile(altered.with_suffix(''), 'w', zipfile.ZIP_DEFLATED) as archive:
            for key,value in data.items(): archive.writestr(key,value)
        rdc = folder / (case + '.rdc')
        run(case+'-convert', [cli, 'convert', '-f', altered, '-o', rdc, '-c', 'rdc'])
        run(case+'-api', [opener, rdc], True)
        run(case+'-cli', [cli, 'replay', '--loops', '1', rdc], True)
    print(f'PASS sourced descriptor shadow: API+CLI, {len(cases)} negative groups before GPU submission')


if __name__ == '__main__': main()
