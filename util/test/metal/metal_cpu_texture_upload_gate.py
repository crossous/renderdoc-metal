#!/usr/bin/env python3
"""Reject invalid CPU upload footprints before native Metal calls or GPU submission."""
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
    def run(label, args, refuse=False):
        r = subprocess.run(list(map(str, args)), capture_output=True, text=True, env=env, timeout=20)
        output = r.stdout + r.stderr; (folder / (label + '.log')).write_text(output)
        assert r.returncode in ((1, 4) if refuse else (0,)), (label, r.returncode, output)
        if refuse:
            assert 'failed' in output.lower() and 'Metal replay wait begin' not in output, (label, output)
    def f(chunk, name): return next(n for n in chunk if n.get('name') == name)
    run('positive-api', [opener, capture]); run('positive-cli', [cli, 'replay', '--loops', '1', capture])
    xml = folder / 'source.zip.xml'; run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as a: blobs = {name: a.read(name) for name in a.namelist()}
    cases = ['volume-depth', 'volume-origin', 'volume-level', 'volume-slice', 'volume-row',
             'volume-image', 'volume-first-image-only', 'volume-private', 'bc-row',
             'bc-origin', 'bc-short-block', 'bc-wrong-storage']
    for case in cases:
        tree, data = copy.deepcopy(original), dict(blobs); chunks = tree.find('./chunks')
        volume = next(c for c in chunks if c.get('name') == 'MTLTexture::replaceRegion' and c.find("uint[@name='bytesPerImage']") is not None)
        bc = next(c for c in chunks if c.get('name') == 'MTLTexture::replaceRegion' and c.find("uint[@name='bytesPerImage']") is None)
        target = volume if case.startswith('volume') else bc
        region = f(target, 'region')
        if case == 'volume-depth': f(f(region, 'size'), 'depth').text = '3'
        elif case == 'volume-origin': f(f(region, 'origin'), 'z').text = '1'
        elif case == 'volume-level': f(target, 'level').text = '1'
        elif case == 'volume-slice': f(target, 'slice').text = '1'
        elif case == 'volume-row': f(target, 'bytesPerRow').text = '3'
        elif case == 'volume-image': f(target, 'bytesPerImage').text = '4'
        elif case == 'bc-row': f(target, 'bytesPerRow').text = '7'
        elif case == 'bc-origin': f(f(region, 'origin'), 'x').text = '1'
        elif case in ('volume-first-image-only', 'bc-short-block'):
            node = f(target, 'contents'); length = 16 if case.startswith('volume') else 7
            key = f'{int(node.text):06d}'; data[key] = data[key][:length]; node.set('byteLength', str(length))
        elif case in ('volume-private', 'bc-wrong-storage'):
            resource = f(target, 'Texture').text
            create = next(c for c in chunks if c.get('name') == 'MTLDevice::newTextureWithDescriptor' and f(c, 'Texture').text == resource)
            descriptor = f(create, 'descriptor'); f(descriptor, 'storageMode').text = '2'; f(descriptor, 'resourceOptions').text = '32'
        destination = folder / (case + '.zip.xml'); tree.write(destination, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(destination.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as a:
            for name, value in data.items(): a.writestr(name, value)
        rdc = folder / (case + '.rdc'); run(case + '-convert', [cli, 'convert', '-f', destination, '-o', rdc, '-c', 'rdc'])
        run(case + '-api', [opener, rdc], True); run(case + '-cli', [cli, 'replay', '--loops', '1', rdc], True)
    print(f'PASS CPU upload: 3D final image, BC1 block rows; {len(cases)} API+CLI negative footprint groups')


if __name__ == '__main__': main()
