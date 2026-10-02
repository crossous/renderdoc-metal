#!/usr/bin/env python3
"""Check UE descriptor enum/resource associations and refuse bad uploads before GPU work."""
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
               RENDERDOC_METAL_TRACE_INITIAL_PRIVATE='1', RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    def run(label, args, refuse=False):
        result = subprocess.run(list(map(str, args)), capture_output=True, text=True, env=env, timeout=20)
        output = result.stdout + result.stderr
        (folder / (label + '.log')).write_text(output)
        assert result.returncode in ((1, 4) if refuse else (0,)), (label, result.returncode, output)
        if refuse:
            assert 'failed' in output.lower(), (label, output)
            assert 'Metal replay wait begin' not in output and 'Private initial contents upload' not in output, (label, output)
    def field(chunk, name): return next(n for n in chunk if n.get('name') == name)
    def first(chunks, name): return next(c for c in chunks if c.get('name') == name)
    run('positive-api', [opener, capture])
    run('positive-cli', [cli, 'replay', '--loops', '1', capture])
    xml = folder / 'source.zip.xml'
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs = {name: archive.read(name) for name in archive.namelist()}
    cases = ['legacy-v6', 'missing-texture-binding', 'duplicate-texture-binding', 'texture-kind',
        'texture-source-buffer', 'texture-source-sampler', 'texture-member', 'sampler-kind',
        'sampler-source-texture', 'sampler-member', 'sampler-no-argument-buffer', 'unsupported-type',
        'buffer-unused-word', 'texture-unused-word', 'buffer-va', 'texture-id', 'sampler-id',
        'initial-texture-id', 'oversized-texture', 'private-cpu-upload', 'upload-row-pitch',
        'upload-region', 'upload-level', 'upload-payload']
    for case in cases + ['buffer-uav', 'buffer-cbv', 'texture-uav', 'buffer-texture-srv', 'buffer-texture-uav']:
        tree, data = copy.deepcopy(original), dict(blobs)
        chunks = tree.find('./chunks')
        bindings = [c for c in chunks if c.get('name') == 'MTLBuffer::DescriptorSlotBinding']
        buffer, texture, sampler = bindings
        texture_id = field(texture, 'resource').text
        sampler_id = field(sampler, 'resource').text
        slots = [c for c in chunks if c.get('name') == 'MTLBuffer::DescriptorSlotEvent' and field(c, 'event').text == '2']
        bslot, tslot, sslot = slots
        tcreate = next(c for c in chunks if c.get('name') == 'MTLDevice::newTextureWithDescriptor' and field(c, 'Texture').text == texture_id)
        screate = first(chunks, 'MTLDevice::newSamplerStateWithDescriptor')
        upload = first(chunks, 'MTLTexture::replaceRegion')
        def alter_blob(node, offset, value=b'\xff'*8):
            key = f'{int(node.text):06d}'
            packet = bytearray(data[key]); packet[offset:offset+len(value)] = value; data[key] = bytes(packet)
        if case == 'legacy-v6': field(first(chunks, 'MTLDevice::DeclareDescriptorCoverage'), 'version').text = '6'
        elif case == 'missing-texture-binding': chunks.remove(texture)
        elif case == 'duplicate-texture-binding': chunks.insert(list(chunks).index(texture), copy.deepcopy(texture))
        elif case == 'texture-kind': field(texture, 'kind').text = '0'
        elif case == 'texture-source-buffer': field(texture, 'resource').text = field(buffer, 'resource').text
        elif case == 'texture-source-sampler': field(texture, 'resource').text = sampler_id
        elif case == 'texture-member': field(texture, 'memberOffset').text = '1'
        elif case == 'sampler-kind': field(sampler, 'kind').text = '1'
        elif case == 'sampler-source-texture': field(sampler, 'resource').text = texture_id
        elif case == 'sampler-member': field(sampler, 'memberOffset').text = '1'
        elif case == 'sampler-no-argument-buffer': field(field(screate, 'descriptor'), 'supportArgumentBuffers').text = 'false'
        elif case in ('unsupported-type', 'buffer-uav', 'buffer-cbv', 'texture-uav', 'buffer-texture-srv', 'buffer-texture-uav'):
            target = field(bslot if case != 'texture-uav' else tslot, 'buffer').text
            for c in chunks:
                if c.get('name') == 'MTLBuffer::DescriptorSlotEvent' and field(c, 'buffer').text == target:
                    field(c, 'descriptorType').text = {'unsupported-type': '8', 'buffer-uav': '1', 'buffer-cbv': '6', 'texture-uav': '5', 'buffer-texture-srv': '4', 'buffer-texture-uav': '5'}[case]
        elif case == 'buffer-unused-word': alter_blob(field(bslot, 'data'), 8)
        elif case == 'texture-unused-word': alter_blob(field(tslot, 'data'), 0)
        elif case == 'buffer-va': alter_blob(field(bslot, 'data'), 0)
        elif case == 'texture-id': alter_blob(field(tslot, 'data'), 8)
        elif case == 'sampler-id': alter_blob(field(sslot, 'data'), 0)
        elif case == 'initial-texture-id':
            table = field(tslot, 'buffer').text
            create = next(c for c in chunks if c.get('name') == 'MTLDevice::newBufferWithBytes' and field(c, 'Buffer').text == table)
            alter_blob(field(create, 'initialData'), 8)
        elif case == 'oversized-texture': field(field(tcreate, 'descriptor'), 'width').text = '3'
        elif case == 'private-cpu-upload':
            field(field(tcreate, 'descriptor'), 'storageMode').text = '2'
            field(field(tcreate, 'descriptor'), 'resourceOptions').text = '32'
        elif case == 'upload-row-pitch': field(upload, 'bytesPerRow').text = '3'
        elif case == 'upload-region': field(field(field(upload, 'region'), 'origin'), 'x').text = '1'
        elif case == 'upload-level': field(upload, 'level').text = '1'
        elif case == 'upload-payload':
            node = field(upload, 'contents'); data[f'{int(node.text):06d}'] = b'\x01\x02\x03'; node.set('byteLength', '3')
        target = folder / (case + '.zip.xml'); tree.write(target, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, value in data.items(): archive.writestr(name, value)
        rdc = folder / (case + '.rdc')
        run(case + '-convert', [cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        refused = case in cases
        run(case + '-api', [opener, rdc], refused)
        run(case + '-cli', [cli, 'replay', '--loops', '1', rdc], refused)
    print(f'PASS real UE types: {len(cases)} API+CLI negative groups; Buffer1/CBV6/Texture5 and UE Texture4/5 buffer-view positive variants')


if __name__ == '__main__': main()
