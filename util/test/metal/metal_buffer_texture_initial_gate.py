#!/usr/bin/env python3
"""Reject invalid parent-backed TextureBuffer data, view ranges and plain copies before GPU upload."""
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
    cases = ('legacy-v31', 'missing-initial', 'duplicate-initial', 'short-initial',
             'long-initial', 'wrong-initial-id', 'wrong-width', 'wrong-height',
             'wrong-mip-count', 'wrong-storage', 'texture-member-offset', 'unknown-source',
             'view-offset', 'view-parent', 'view-row-pitch', 'copy-source-range',
             'copy-destination-range', 'copy-descriptor-destination', 'copy-after-end', 'copy-after-commit')
    input_binding=next(c for c in original.find('./chunks') if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'kind').text=='0')
    input_id=field(input_binding,'resource').text
    input_initial=next((c for c in original.find('./chunks') if c.get('id')=='3' and field(c,'id').text==input_id),None)
    wide_input=input_initial is not None and int(field(input_initial,'Contents').get('byteLength'))>65536
    if wide_input:cases += ('legacy-v33-input','missing-input-initial','short-input-initial','long-input-initial','input-member-offset','input-length')
    for case in cases:
        tree, data = copy.deepcopy(original), dict(blobs)
        chunks = tree.find('./chunks')
        binding = next(c for c in chunks if c.get('name') == 'MTLBuffer::DescriptorSlotBinding'
                       and field(c, 'kind').text == '1')
        identity = field(binding, 'resource').text
        creation = next(c for c in chunks if c.get('name') == 'MTLBuffer::newTextureWithDescriptor'
                        and field(c,'Texture').text == identity)
        parent_id=field(creation,'Buffer').text
        initial=next(c for c in chunks if c.get('id')=='3' and field(c,'id').text==parent_id)
        descriptor=field(creation,'descriptor')
        if case in ('legacy-v31','legacy-v33-input'):
            coverage = next(c for c in chunks if c.get('name') == 'MTLDevice::DeclareDescriptorCoverage')
            field(coverage, 'version').text = '31' if case=='legacy-v31' else '33'
        elif case in ('missing-input-initial','short-input-initial','long-input-initial','input-member-offset','input-length'):
            input_binding=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'kind').text=='0')
            input_id=field(input_binding,'resource').text
            input_initial=next(c for c in chunks if c.get('id')=='3' and field(c,'id').text==input_id)
            if case=='missing-input-initial':chunks.remove(input_initial)
            elif case=='input-member-offset':field(input_binding,'memberOffset').text='65552'
            elif case=='input-length':
                input_creation=next(c for c in chunks if c.get('name')=='MTLDevice::newBufferWithLength' and field(c,'Buffer').text==input_id)
                field(input_creation,'length').text='65553'
            else:
                payload=field(input_initial,'Contents');key=f'{int(payload.text):06d}'
                data[key]=data[key][:-1] if case=='short-input-initial' else data[key]+b'\x01';payload.set('byteLength',str(len(data[key])))
        elif case == 'missing-initial': chunks.remove(initial)
        elif case == 'duplicate-initial': chunks.insert(list(chunks).index(initial), copy.deepcopy(initial))
        elif case in ('short-initial', 'long-initial'):
            node = field(initial, 'Contents')
            key = f'{int(node.text):06d}'
            data[key] = data[key][:-1] if case == 'short-initial' else data[key] + b'\x01'
            node.set('byteLength', str(len(data[key])))
        elif case == 'wrong-initial-id': field(initial, 'id').text = '9999999'
        elif case == 'wrong-width': field(descriptor, 'width').text = '1114113'
        elif case == 'wrong-height': field(descriptor, 'height').text = '2'
        elif case == 'wrong-mip-count': field(descriptor, 'mipmapLevelCount').text = '2'
        elif case == 'wrong-storage':field(descriptor,'storageMode').text='0'
        elif case == 'view-offset':field(creation,'offset').text='512'
        elif case == 'view-parent':field(creation,'Buffer').text='9999999'
        elif case == 'view-row-pitch':field(creation,'bytesPerRow').text='1'
        elif case.startswith('copy-'):
            operation=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer').text==parent_id)
            if case=='copy-source-range':field(operation,'sourceOffset').text='4'
            elif case=='copy-destination-range':field(operation,'destinationOffset').text='512'
            elif case=='copy-descriptor-destination':field(operation,'destinationBuffer').text=field(binding,'buffer').text
            else:
                target=next(c for c in chunks if c.get('name')==('MTLBlitCommandEncoder::endEncoding' if case=='copy-after-end' else 'MTLCommandBuffer::commit'))
                chunks.remove(operation);chunks.insert(list(chunks).index(target)+1,operation)
        elif case == 'texture-member-offset': field(binding, 'memberOffset').text = '1'
        elif case == 'unknown-source': field(binding, 'resource').text = '9999999'
        target = folder / (case + '.zip.xml')
        tree.write(target, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, value in data.items(): archive.writestr(name, value)
        rdc = folder / (case + '.rdc')
        run(case + '-convert', [cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        run(case + '-api', [opener, rdc], True)
        run(case + '-cli', [cli, 'replay', '--loops', '1', rdc], True)
    print(f'PASS Private TextureBuffer parent/view/copy: {len(cases)} API+CLI negative groups')


if __name__ == '__main__':
    main()
