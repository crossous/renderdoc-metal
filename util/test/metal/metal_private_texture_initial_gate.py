#!/usr/bin/env python3
"""Refuse invalid Private texture initial state before native GPU uploads or replay."""
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
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_INITIAL_PRIVATE='1',
               RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')

    def run(label, args, refuse=False):
        result = subprocess.run(list(map(str, args)), capture_output=True, text=True,
                                timeout=30, env=env)
        output = result.stdout + result.stderr
        (folder / (label + '.log')).write_text(output)
        assert result.returncode in ((1, 4) if refuse else (0,)), (label, result.returncode, output)
        if refuse:
            assert 'failed' in output.lower(), (label, output)
            for marker in ('Metal replay wait begin', 'Metal Private initial contents:', 'Private initial contents upload',
                           'Private texture initial contents upload', 'Metal texture initial contents upload'):
                assert marker not in output, (label, output)

    def field(chunk, name):
        return next(n for n in chunk if n.get('name') == name)

    run('positive-api', [opener, capture])
    run('positive-cli', [cli, 'replay', '--loops', '1', capture])
    xml = folder / 'source.zip.xml'
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs = {name: archive.read(name) for name in archive.namelist()}
    cases = ('legacy-v26', 'missing-initial', 'duplicate-initial', 'short-initial',
             'long-initial', 'wrong-initial-id', 'wrong-width', 'wrong-height',
             'wrong-mip-count', 'short-shared-initial', 'array-initial', 'depth-initial',
             'texture-member-offset', 'unknown-source')
    has_view = any(c.get('name', '').startswith('MTLTexture::newTextureView') for c in original.find('./chunks'))
    if has_view:
        cases += ('missing-view', 'view-parent', 'view-level', 'view-format', 'binding-parent')
    wide_r16=any(c.get('name')=='MTLDevice::newTextureWithDescriptor' and
                 field(field(c,'descriptor'),'width').text=='192' for c in original.find('./chunks'))
    if wide_r16:cases += ('legacy-v34','width-limit','render-width-limit','render-format')
    drawable_source=any(c.get('name')=='MTLDevice::DeclareDescriptorCoverage' and field(c,'version').text=='37' for c in original.find('./chunks'))
    if drawable_source:cases += ('legacy-v36',)
    for case in cases:
        tree, data = copy.deepcopy(original), dict(blobs)
        chunks = tree.find('./chunks')
        binding = next(c for c in chunks if c.get('name') == 'MTLBuffer::DescriptorSlotBinding'
                       and field(c, 'kind').text == '1')
        identity = field(binding, 'resource').text
        view = next((c for c in chunks if c.get('name', '').startswith('MTLTexture::newTextureView')
                     and field(c, 'View').text == identity), None)
        parent_id = field(view, 'Source').text if view is not None else identity
        creation = next(c for c in chunks if c.get('name') in ('MTLDevice::newTextureWithDescriptor','[CAMetalLayer nextDrawable]')
                        and field(c, 'Texture').text == parent_id)
        initial = next(c for c in chunks if c.get('id') == '3' and field(c, 'id').text == parent_id)
        descriptor = field(creation, 'descriptor')
        if case in ('legacy-v26','legacy-v34','legacy-v36'):
            coverage = next(c for c in chunks if c.get('name') == 'MTLDevice::DeclareDescriptorCoverage')
            field(coverage, 'version').text = '36' if case=='legacy-v36' else '34' if case=='legacy-v34' else '27' if view is not None else '26'
        elif case == 'missing-initial': chunks.remove(initial)
        elif case == 'duplicate-initial': chunks.insert(list(chunks).index(initial), copy.deepcopy(initial))
        elif case in ('short-initial', 'long-initial'):
            node = field(initial, 'Contents')
            key = f'{int(node.text):06d}'
            data[key] = data[key][:-1] if case == 'short-initial' else data[key] + b'\x01'
            node.set('byteLength', str(len(data[key])))
        elif case == 'wrong-initial-id': field(initial, 'id').text = '9999999'
        elif case == 'wrong-width': field(descriptor, 'width').text = '2'
        elif case == 'wrong-height': field(descriptor, 'height').text = '2'
        elif case == 'wrong-mip-count': field(descriptor, 'mipmapLevelCount').text = '2'
        elif case == 'short-shared-initial':
            field(descriptor, 'storageMode').text = '0'
            field(descriptor, 'resourceOptions').text = '0'
            node = field(initial, 'Contents'); key = f'{int(node.text):06d}'
            data[key] = data[key][:-1]; node.set('byteLength', str(len(data[key])))
        elif case == 'array-initial':
            field(descriptor, 'textureType').text = '3'
            field(descriptor, 'arrayLength').text = '2'
        elif case == 'depth-initial': field(descriptor, 'pixelFormat').text = '252'
        elif case == 'texture-member-offset': field(binding, 'memberOffset').text = '1'
        elif case == 'unknown-source': field(binding, 'resource').text = '9999999'
        elif case == 'missing-view': chunks.remove(view)
        elif case == 'view-parent': field(view, 'Source').text = '9999999'
        elif case == 'view-level': field(field(view, 'levels'), 'location').text = '1'
        elif case == 'view-format': field(view, 'format').text = '55' if wide_r16 else '25'
        elif case == 'binding-parent': field(binding, 'resource').text = parent_id
        elif case == 'width-limit':field(descriptor,'width').text='8193'
        elif case in ('render-width-limit','render-format'):
            render=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::renderCommandEncoderWithDescriptor')
            if case=='render-width-limit':field(field(render,'descriptor'),'renderTargetWidth').text='193'
            else:field(descriptor,'pixelFormat').text='23'
        target = folder / (case + '.zip.xml')
        tree.write(target, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, value in data.items(): archive.writestr(name, value)
        rdc = folder / (case + '.rdc')
        run(case + '-convert', [cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        run(case + '-api', [opener, rdc], True)
        run(case + '-cli', [cli, 'replay', '--loops', '1', rdc], True)
    print(f'PASS Private texture initial contents: {len(cases)} API+CLI negative groups')


if __name__ == '__main__':
    main()
