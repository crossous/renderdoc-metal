#!/usr/bin/env python3
"""Malformed signal submissions must fail preflight, before any frame work."""
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
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')

    def run(label, args, reject=False):
        p = subprocess.run(list(map(str, args)), capture_output=True, text=True,
                           env=env, timeout=40)
        out = p.stdout + p.stderr
        (folder / (label + '.log')).write_text(out)
        assert p.returncode in ((1, 4) if reject else (0,)), (label, p.returncode, out)
        if reject:
            assert 'failed' in out.lower() and 'Metal replay wait begin' not in out, (label, out)

    def field(c, name):
        return next(n for n in c if n.get('name') == name)

    xml = folder / 'source.zip.xml'
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as z:
        blobs = {name: z.read(name) for name in z.namelist()}
    cases = ['event-zero', 'event-unknown', 'command-zero', 'command-unknown',
             'value-zero', 'duplicate-signal', 'after-commit', 'before-birth']
    for label in cases:
        tree = copy.deepcopy(original)
        chunks = tree.find('./chunks')
        signal = next(c for c in chunks if c.get('name') == 'MTLCommandBuffer::encodeSignalEvent')
        command = field(signal, 'CommandBuffer').text
        if label.startswith('event-'):
            field(signal, 'event').text = '0' if label.endswith('zero') else '999999999'
        elif label.startswith('command-'):
            field(signal, 'CommandBuffer').text = '0' if label.endswith('zero') else '999999999'
        elif label == 'value-zero':
            field(signal, 'value').text = '0'
        elif label == 'duplicate-signal':
            chunks.insert(list(chunks).index(signal) + 1, copy.deepcopy(signal))
        else:
            ref = next(c for c in chunks if c.get('name') ==
                       ('MTLCommandBuffer::commit' if label == 'after-commit' else
                        'MTLCommandQueue::commandBuffer') and
                       field(c, 'CommandBuffer').text == command)
            chunks.remove(signal)
            chunks.insert(list(chunks).index(ref) + (label == 'after-commit'), signal)
        for c in chunks:
            c.set('length', '0')
        target = folder / (label + '.zip.xml')
        tree.write(target, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''), 'w') as z:
            for name, data in blobs.items():
                z.writestr(name, data)
        rdc = folder / (label + '.rdc')
        run(label + '-convert', [cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        run(label + '-api', [opener, rdc], True)
        run(label + '-cli', [cli, 'replay', '--loops', '1', rdc], True)
    print('PASS malformed timeline signal submissions rejected before frame execution')


if __name__ == '__main__':
    main()
