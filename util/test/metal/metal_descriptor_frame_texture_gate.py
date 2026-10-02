#!/usr/bin/env python3
"""Reject unsupported frame texture identities, birth order and attachments before GPU work."""
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
        result = subprocess.run(list(map(str, args)), capture_output=True, text=True, env=env, timeout=20)
        output = result.stdout + result.stderr
        (folder / (label + '.log')).write_text(output)
        assert result.returncode in ((1, 4) if refuse else (0,)), (label, result.returncode, output)
        if refuse:
            assert 'failed' in output.lower() and 'Metal replay wait begin' not in output, (label, output)

    def field(node, name):
        return next(child for child in node if child.get('name') == name)

    xml = folder / 'source.zip.xml'
    run('export', [cli, 'convert', '-f', capture, '-o', xml, '-c', 'zip.xml'])
    original = ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs = {name: archive.read(name) for name in archive.namelist()}
    source_chunks=list(original.find('./chunks'))
    source_birth=next(c for c in source_chunks if c.get('name')=='MTLHeap::newTexture(offset)')
    alias_buffers=[c for c in source_chunks if c.get('name')=='MTLHeap::newBuffer(offset)' and
        field(c,'Heap').text==field(source_birth,'Heap').text and field(c,'offset').text==field(source_birth,'offset').text]
    if alias_buffers:
        assert len(alias_buffers)==1
        old=field(alias_buffers[0],'Buffer').text
        scope=next(i for i,c in enumerate(source_chunks) if c.get('id')=='5')
        assert source_chunks.index(alias_buffers[0])<scope<source_chunks.index(source_birth)
        initial=next(c for c in source_chunks if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==old)
        import struct
        data=blobs[f'{int(field(initial,"Contents").text):06d}']
        assert len(data)==4096 and struct.unpack('<III',data[:12])==(0xbad,41,80)
        assert any(c.get('name')=='MTLComputeCommandEncoder::setBuffer' and
            field(c,'buffer').text==old and field(c,'index').text=='2' for c in source_chunks[scope:])
        assert not any(c.get('name')=='MTLBuffer::makeAliasable' for c in source_chunks[scope:])
        print('PASS cross-kind capture: background Private buffer initialized/read, same heap/offset frame texture, no logical retirement')
    cases = ['old-contract', 'missing-birth', 'duplicate-birth', 'late-birth',
             'texture-zero', 'texture-conflict', 'heap-zero', 'heap-unknown', 'offset-unaligned', 'offset-outside',
             'width', 'format', 'samples', 'array', 'storage', 'hazard', 'usage',
             'missing-identity', 'wrong-identity', 'source-offset', 'source-before-birth',
             'unknown-color', 'unknown-depth', 'unknown-stencil']
    view_cases=['old-view-contract','missing-view-birth','duplicate-view-birth','late-view-birth',
        'view-before-parent','view-source-zero','view-source-unknown','view-zero','view-conflict',
        'view-format','view-type','view-level-start','view-level-zero','view-level-count',
        'view-slice-start','view-slice-zero','view-swizzle']
    has_view=any(c.get('name','').startswith('MTLTexture::newTextureView') for c in source_chunks)
    if has_view:cases+=view_cases
    if alias_buffers:cases+=['old-cross-contract','missing-alias-initial','untracked-alias-buffer']
    for case in cases:
        tree = copy.deepcopy(original)
        chunks = tree.find('./chunks')
        birth = next(c for c in chunks if c.get('name') == 'MTLHeap::newTexture(offset)')
        view=next((c for c in chunks if c.get('name','').startswith('MTLTexture::newTextureView')),None)
        rid = field(view, 'View').text if view is not None else field(birth, 'Texture').text
        identity = next(c for c in chunks if c.get('name') == 'MTLResource::CaptureGPUIdentity'
                        and field(c, 'resource').text == rid)
        binding = next(c for c in chunks if c.get('name') == 'MTLBuffer::DescriptorSlotBinding'
                       and field(c, 'resource').text == rid)
        passes = [field(c, 'descriptor') for c in chunks if c.get('name') in
                  ('MTLCommandBuffer::renderCommandEncoderWithDescriptor',
                   'MTLCommandBuffer::parallelRenderCommandEncoderWithDescriptor')]
        if case in view_cases:
            def move(c, anchor, after=False):
                chunks.remove(c);chunks.insert(list(chunks).index(anchor)+int(after),c)
            if case=='old-view-contract':
                field(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='25'
            elif case=='missing-view-birth':chunks.remove(view)
            elif case=='duplicate-view-birth':chunks.insert(list(chunks).index(view),copy.deepcopy(view))
            elif case=='late-view-birth':chunks.remove(view);chunks.append(view)
            elif case=='view-before-parent':move(view,birth)
            elif case=='view-source-zero':field(view,'Source').text='0'
            elif case=='view-source-unknown':field(view,'Source').text='999999'
            elif case=='view-zero':field(view,'View').text='0'
            elif case=='view-conflict':field(view,'View').text=field(birth,'Texture').text
            elif case=='view-format':field(view,'format').text='0'
            elif case=='view-type':field(view,'type').text='7'
            elif case=='view-level-start':field(field(view,'levels'),'location').text='1'
            elif case=='view-level-zero':field(field(view,'levels'),'length').text='0'
            elif case=='view-level-count':field(field(view,'levels'),'length').text='2'
            elif case=='view-slice-start':field(field(view,'slices'),'location').text='1'
            elif case=='view-slice-zero':field(field(view,'slices'),'length').text='0'
            elif case=='view-swizzle':field(field(view,'swizzle'),'red').text='0'
        elif case == 'old-cross-contract':
            field(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='23'
        elif case == 'missing-alias-initial':
            chunks.remove(next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==old))
        elif case == 'untracked-alias-buffer':
            field(next(c for c in chunks if c.get('name')=='MTLHeap::newBuffer(offset)' and field(c,'Buffer').text==old),'options').text='288'
        elif case == 'old-contract':
            field(next(c for c in chunks if c.get('name') == 'MTLDevice::DeclareDescriptorCoverage'), 'version').text = '20'
        elif case == 'missing-birth':
            chunks.remove(birth)
        elif case == 'duplicate-birth':
            chunks.insert(list(chunks).index(birth) + 1, copy.deepcopy(birth))
        elif case == 'late-birth':
            chunks.remove(birth)
            chunks.append(birth)
        elif case in ('texture-zero', 'texture-conflict', 'heap-zero', 'heap-unknown', 'offset-unaligned', 'offset-outside'):
            key, value = {'texture-zero': ('Texture', '0'), 'texture-conflict': ('Texture', field(binding, 'buffer').text),
                          'heap-zero': ('Heap', '0'), 'heap-unknown': ('Heap', '999999'),
                          'offset-unaligned': ('offset', '1'), 'offset-outside': ('offset', str(2**63))}[case]
            field(birth, key).text = value
        elif case in ('width', 'format', 'samples', 'array', 'storage', 'hazard', 'usage'):
            key, value = {'width': ('width', '3'), 'format': ('pixelFormat', '0'), 'samples': ('sampleCount', '2'),
                          'array': ('arrayLength', '2'), 'storage': ('storageMode', '0'),
                          'hazard': ('hazardTrackingMode', '1'), 'usage': ('usage', '0')}[case]
            field(field(birth, 'descriptor'), key).text = value
        elif case == 'missing-identity':
            chunks.remove(identity)
        elif case == 'wrong-identity':
            field(identity, 'value').text = str(int(field(identity, 'value').text) + 1)
        elif case == 'source-offset':
            field(binding, 'memberOffset').text = '1'
        elif case == 'source-before-birth':
            chunks.remove(binding)
            chunks.insert(list(chunks).index(birth), binding)
        elif case == 'unknown-color':
            field(list(field(passes[0], 'colorAttachments'))[1], 'texture').text = '999999'
        elif case in ('unknown-depth', 'unknown-stencil'):
            field(field(passes[0], 'depthAttachment' if case == 'unknown-depth' else 'stencilAttachment'), 'texture').text = '999999'
        for chunk in chunks:
            chunk.set('length', '0')
        target = folder / (case + '.zip.xml')
        tree.write(target, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in blobs.items():
                archive.writestr(name, data)
        rdc = folder / (case + '.rdc')
        run(case + '-convert', [cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        run(case + '-api', [opener, rdc], True)
        run(case + '-cli', [cli, 'replay', '--loops', '1', rdc], True)
    print(f'PASS frame sourced texture: {len(cases)} API+CLI birth/source/layout/attachment negative groups')


if __name__ == '__main__':
    main()
