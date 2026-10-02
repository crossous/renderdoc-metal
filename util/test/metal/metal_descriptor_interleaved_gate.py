#!/usr/bin/env python3
"""Validate per-command ownership before submitting interleaved sourced work."""
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
    env = dict(os.environ, MTL_DEBUG_LAYER='1',
               RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
               RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')

    def run(label, args, refuse=False):
        result = subprocess.run(list(map(str, args)), capture_output=True, text=True,
                                env=env, timeout=25)
        output = result.stdout + result.stderr
        (folder / (label + '.log')).write_text(output)
        assert result.returncode in ((1, 4) if refuse else (0,)), (label, result.returncode, output)
        if refuse:
            assert 'failed' in output.lower() and 'Metal replay wait begin' not in output, (label, output)

    def field(chunk, name):
        return next(node for node in chunk if node.get('name') == name)

    run('positive-api', [opener, capture])
    run('positive-cli', [cli, 'replay', '--loops', '3', capture])
    xml = folder / 'source.zip.xml'
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs = {name: archive.read(name) for name in archive.namelist()}
    cases = ['v14', 'missing-later-birth', 'missing-empty-birth', 'missing-first-birth',
             'duplicate-birth', 'conflicting-birth', 'unknown-queue', 'cross-queue',
             'missing-first-commit', 'missing-empty-commit', 'missing-last-commit',
             'duplicate-commit', 'unknown-commit', 'commit-before-encoder-end',
             'unknown-encoder-command', 'encoder-before-birth', 'encoder-after-commit',
             'encoder-on-committed-command', 'missing-encoder-end', 'duplicate-encoder-end',
             'enqueue-before-birth', 'unknown-enqueue', 'duplicate-enqueue',
             'enqueue-after-commit', 'enqueue-with-live-encoder', 'uncommitted-reservation',
             'wait-uncommitted', 'wait-unknown', 'wait-before-commit', '257th-command']
    for case in cases:
        tree = copy.deepcopy(original)
        chunks = tree.find('./chunks')
        scope = next(i for i, chunk in enumerate(chunks) if chunk.get('id') == '5')
        frame = list(chunks)[scope + 1:]
        births = [c for c in frame if c.get('name', '').startswith('MTLCommandQueue::commandBuffer')]
        commits = [c for c in frame if c.get('name') == 'MTLCommandBuffer::commit']
        enqueues = [c for c in frame if c.get('name') == 'MTLCommandBuffer::enqueue']
        encoders = [c for c in frame if c.get('name') == 'MTLCommandBuffer::computeCommandEncoder']
        end = next(c for c in frame if c.get('name') == 'MTLComputeCommandEncoder::endEncoding')
        assert len(births) == len(commits) == 75 and len(enqueues) == 2
        first_birth = next(c for c in births if field(c, 'CommandBuffer').text == field(commits[0], 'CommandBuffer').text)
        assert field(births[0], 'CommandBuffer').text == field(commits[-1], 'CommandBuffer').text

        def move(chunk, before):
            chunks.remove(chunk)
            chunks.insert(list(chunks).index(before), chunk)

        if case == 'v14':
            field(next(c for c in chunks if c.get('name') == 'MTLDevice::DeclareDescriptorCoverage'), 'version').text = '14'
        elif case.startswith('missing-'):
            chunks.remove({'missing-later-birth': births[0], 'missing-empty-birth': births[1],
                           'missing-first-birth': first_birth, 'missing-first-commit': commits[0],
                           'missing-empty-commit': commits[1], 'missing-last-commit': commits[-1],
                           'missing-encoder-end': end}[case])
        elif case in ('duplicate-birth', 'duplicate-commit', 'duplicate-enqueue', 'duplicate-encoder-end'):
            chunk = {'duplicate-birth': births[0], 'duplicate-commit': commits[0],
                     'duplicate-enqueue': enqueues[0], 'duplicate-encoder-end': end}[case]
            chunks.insert(list(chunks).index(chunk), copy.deepcopy(chunk))
        elif case == 'conflicting-birth':
            field(births[1], 'CommandBuffer').text = field(births[0], 'CommandBuffer').text
        elif case in ('unknown-queue', 'cross-queue'):
            field(first_birth, 'CommandQueue').text = '0' if case == 'unknown-queue' else '999998'
        elif case in ('unknown-commit', 'unknown-enqueue', 'unknown-encoder-command'):
            field({'unknown-commit': commits[0], 'unknown-enqueue': enqueues[0],
                   'unknown-encoder-command': encoders[0]}[case], 'CommandBuffer').text = '0'
        elif case == 'commit-before-encoder-end':
            move(commits[0], end)
        elif case == 'encoder-before-birth':
            move(first_birth, commits[0])
        elif case == 'encoder-after-commit':
            move(commits[0], encoders[0])
        elif case == 'encoder-on-committed-command':
            field(encoders[1], 'CommandBuffer').text = field(commits[0], 'CommandBuffer').text
        elif case == 'enqueue-before-birth':
            move(enqueues[0], first_birth)
        elif case == 'enqueue-after-commit':
            move(enqueues[0], commits[1])
        elif case == 'enqueue-with-live-encoder':
            chunks.remove(enqueues[0])
            chunks.insert(list(chunks).index(encoders[0]) + 1, enqueues[0])
        elif case == 'uncommitted-reservation':
            reservation = copy.deepcopy(enqueues[0])
            field(reservation, 'CommandBuffer').text = field(births[0], 'CommandBuffer').text
            chunks.insert(list(chunks).index(enqueues[0]), reservation)
        elif case.startswith('wait-'):
            import re
            header = (Path(__file__).resolve().parents[3] / 'renderdoc/driver/metal/metal_common.h').read_text()
            names = re.findall(r'^  (\w+)(?:\s*=.*)?,', header.split('enum class MetalChunk : uint32_t')[1].split('};')[0], re.M)
            wait = ET.Element('chunk', id=str(1000 + names.index('MTLCommandBuffer_waitUntilCompleted')),
                              name='MTLCommandBuffer::waitUntilCompleted', length='0')
            identity = '0' if case == 'wait-unknown' else field(births[0] if case == 'wait-uncommitted' else first_birth, 'CommandBuffer').text
            ET.SubElement(wait, 'ResourceId', name='CommandBuffer', typename='ResourceId', width='8').text = identity
            chunks.insert(list(chunks).index(commits[0]), wait)
        elif case == '257th-command':
            for value in range(182):
                identity = str(999000 + value)
                birth = copy.deepcopy(births[0])
                field(birth, 'CommandBuffer').text = identity
                chunks.insert(list(chunks).index(first_birth), birth)
        for chunk in chunks:
            chunk.set('length', '0')
        target = folder / (case + '.zip.xml')
        tree.write(target, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, blob in blobs.items():
                archive.writestr(name, blob)
        rdc = folder / (case + '.rdc')
        run(case + '-convert', [cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        run(case + '-api', [opener, rdc], True)
        run(case + '-cli', [cli, 'replay', '--loops', '1', rdc], True)
    print(f'PASS interleaved: {len(cases)} API+CLI command identity/commit/encoder/reservation groups')


if __name__ == '__main__':
    main()
