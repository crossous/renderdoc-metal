#!/usr/bin/env python3
"""Validate typed backing retirement and submission ownership before native replay."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5])
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
               RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    def run(label, args, refuse=False):
        result = subprocess.run(list(map(str, args)), capture_output=True, text=True, env=env, timeout=20)
        output = result.stdout + result.stderr
        (folder / (label + '.log')).write_text(output)
        assert result.returncode in ((1, 4) if refuse else (0,)), (label, result.returncode, output)
        if refuse:
            assert 'failed' in output.lower() and 'Metal replay wait begin' not in output, (label, output)
    def f(chunk, name):
        return next(node for node in chunk if node.get('name') == name)
    run('positive-api', [opener, capture])
    run('positive-cli', [cli, 'replay', '--loops', '1', capture])
    xml = folder / 'source.zip.xml'
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs = {name: archive.read(name) for name in archive.namelist()}
    chunks = list(original.find('./chunks'))
    scope = next(i for i, chunk in enumerate(chunks) if chunk.get('id') == '5')
    frame = chunks[scope + 1:]
    new_declaration = next(c for c in frame if c.get('name') == 'MTLBuffer::DeclareDescriptorTable')
    bid = f(new_declaration, 'buffer').text
    birth = next(c for c in frame if c.get('name') == 'MTLHeap::newBuffer(offset)' and f(c, 'Buffer').text == bid)
    old = next(c for c in chunks[:scope] if c.get('name') == 'MTLHeap::newBuffer(offset)' and
               f(c, 'Heap').text == f(birth, 'Heap').text)
    aid = f(old, 'Buffer').text
    free = next(c for c in frame if c.get('name') == 'MTLBuffer::DescriptorSlotEvent' and
                f(c, 'buffer').text == aid and f(c, 'event').text == '1')
    commits = [c for c in frame if c.get('name') == 'MTLCommandBuffer::commit']
    assert len(commits) == 2 and frame.index(commits[0]) < frame.index(free) < frame.index(birth)
    assert f(old, 'offset').text == f(birth, 'offset').text == '0'
    assert f(old, 'length').text == f(birth, 'length').text == '24'
    snapshots = [c for c in frame if c.get('name') == 'Internal_MTLBufferModifyCPUContents' and
                 frame.index(c) > frame.index(birth)]
    assert any(f(c, 'Buffer').text == aid for c in snapshots), 'Missing duplicate retired backing snapshot'
    assert any(f(c, 'Buffer').text == bid for c in snapshots), 'Missing live replacement snapshot'
    print('PASS CPU capture proof: committed old use, matching slot free, same placement range, duplicate A/B snapshots')
    cases = ['v24', 'missing-free', 'stale-free-generation', 'late-free', 'uncommitted-old-use',
             'later-old-inline-source', 'missing-new-table', 'duplicate-birth', 'unaligned-offset',
             'outside-heap', 'private-table', 'untracked-buffer', 'untracked-heap',
             'old-gpu-written', 'new-gpu-written', 'missing-new-source', 'wrong-new-source-offset']
    for case in cases:
        tree = copy.deepcopy(original)
        target_chunks = tree.find('./chunks')
        def at(chunk):
            return target_chunks[chunks.index(chunk)]
        target_birth, target_free = at(birth), at(free)
        declaration = at(new_declaration)
        source = next(c for c in target_chunks if c.get('name') == 'MTLBuffer::DescriptorSlotBinding' and f(c, 'buffer').text == bid)
        heap = next(c for c in target_chunks if c.get('name') == 'MTLDevice::newHeapWithDescriptor' and f(c, 'Heap').text == f(birth, 'Heap').text)
        def move(chunk, anchor, after=False):
            target_chunks.remove(chunk)
            target_chunks.insert(list(target_chunks).index(anchor) + int(after), chunk)
        if case == 'v24':
            f(next(c for c in target_chunks if c.get('name') == 'MTLDevice::DeclareDescriptorCoverage'), 'version').text = '24'
        elif case == 'missing-free': target_chunks.remove(target_free)
        elif case == 'stale-free-generation': f(target_free, 'generation').text = '999'
        elif case == 'late-free': move(target_free, target_birth, True)
        elif case == 'uncommitted-old-use': move(at(commits[0]), target_birth, True)
        elif case == 'later-old-inline-source':
            binding = next(c for c in target_chunks if c.get('name') == 'MTLCommandEncoder::DescriptorInlineBinding' and f(c, 'resource').text == bid)
            f(binding, 'resource').text = aid
        elif case == 'missing-new-table': target_chunks.remove(declaration)
        elif case == 'duplicate-birth': target_chunks.insert(list(target_chunks).index(target_birth), copy.deepcopy(target_birth))
        elif case == 'unaligned-offset': f(target_birth, 'offset').text = '1'
        elif case == 'outside-heap': f(target_birth, 'offset').text = '65536'
        elif case == 'private-table': f(target_birth, 'options').text = '544'
        elif case == 'untracked-buffer': f(target_birth, 'options').text = '256'
        elif case == 'untracked-heap': f(heap, 'hazardMode').text = '1'
        elif case in ('old-gpu-written', 'new-gpu-written'):
            gpu = copy.deepcopy(next(c for c in target_chunks if c.get('name') == 'MTLBuffer::DeclareDescriptorGPUWrites'))
            f(gpu, 'buffer').text = aid if case == 'old-gpu-written' else bid
            target_chunks.insert(list(target_chunks).index(target_birth), gpu)
        elif case == 'missing-new-source': target_chunks.remove(source)
        elif case == 'wrong-new-source-offset': f(source, 'memberOffset').text = '4'
        for c in target_chunks: c.set('length', '0')
        target = folder / (case + '.zip.xml')
        tree.write(target, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, blob in blobs.items(): archive.writestr(name, blob)
        rdc = folder / (case + '.rdc')
        run(case + '-convert', [cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        run(case + '-api', [opener, rdc], True)
        run(case + '-cli', [cli, 'replay', '--loops', '1', rdc], True)
    print(f'PASS typed backing alias: {len(cases)} API+CLI negative groups')


if __name__ == '__main__':
    main()
