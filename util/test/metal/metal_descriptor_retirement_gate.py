#!/usr/bin/env python3
"""Reject stale descriptors unless an exact leading free proves non-consumption."""
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
    cases = ['v15', 'missing-buffer-free', 'missing-texture-free', 'late-free',
             'free-after-dispatch', 'free-after-commit', 'duplicate-free', 'zero-generation',
             'wrong-generation', 'wrong-type', 'wrong-offset', 'unaligned-offset',
             'outside-buffer', 'unknown-buffer', 'unknown-source', 'free-with-payload',
             'initial-payload-mismatch', 'retired-payload-mismatch']
    declared_version = int(field(next(c for c in original.find('./chunks') if c.get('name') == 'MTLDevice::DeclareDescriptorCoverage'), 'version').text)
    if declared_version >= 29:
        cases += ['v28-mixed-prefix']
    if declared_version >= 30:
        cases += ['v29-blit-prefix','copy-source-range','copy-destination-range','copy-zero-size',
                  'copy-same-buffer','copy-unknown-source','copy-descriptor-destination',
                  'copy-unknown-encoder','copy-after-end','copy-after-commit']
    if declared_version >= 31:
        cases += ['v30-texture-prefix','view-unknown-buffer','view-range','view-storage',
                  'view-zero-width','view-before-buffer','heap-texture-offset','heap-texture-zero-width']
    for case in cases:
        tree = copy.deepcopy(original)
        chunks = tree.find('./chunks')
        scope = next(i for i, chunk in enumerate(chunks) if chunk.get('id') == '5')
        frame = list(chunks)[scope + 1:]
        frees = [c for c in frame if c.get('name') == 'MTLBuffer::DescriptorSlotEvent' and field(c, 'event').text == '1']
        assert len(frees) in (2,80)
        retired_id = field(frees[0], 'buffer').text
        bindings = [c for c in list(chunks)[:scope] if c.get('name') == 'MTLBuffer::DescriptorSlotBinding' and field(c, 'buffer').text == retired_id]
        assert len(bindings) == len(frees)
        # Exercise real expired resources, not a pointer that happens to differ.
        sources = {field(c, 'resource').text for c in bindings}
        created = {node.text for c in chunks if '::new' in c.get('name', '')
                   for node in c if node.get('name') in ('Buffer', 'Texture')}
        assert sources.isdisjoint(created), (sources, created)
        birth = next(c for c in frame if c.get('name', '').startswith('MTLCommandQueue::commandBuffer'))
        dispatch = next(c for c in frame if c.get('name') == 'MTLComputeCommandEncoder::dispatchThreadgroups')
        shader_birth = next(c for c in frame if c.get('name','').startswith('MTLCommandBuffer::computeCommandEncoder'))
        commit = next(c for c in frame if c.get('name') == 'MTLCommandBuffer::commit')
        update = next(c for c in list(chunks)[:scope] if c.get('name') == 'MTLBuffer::DescriptorSlotEvent' and field(c, 'buffer').text == retired_id and field(c, 'event').text == '2')
        initial = next((c for c in chunks if c.get('name') == 'Internal::Initial Contents' and field(c, 'id').text == retired_id), None)
        if initial is not None:
            initial_blob = field(initial, 'Contents').text
        else:
            creation = next(c for c in chunks if c.get('name') == 'MTLDevice::newBufferWithBytes' and field(c, 'Buffer').text == retired_id)
            initial_blob = field(creation, 'initialData').text
        mutated = dict(blobs)

        def move_after(chunk, target):
            chunks.remove(chunk)
            chunks.insert(list(chunks).index(target) + 1, chunk)

        if case in ('v15', 'v28-mixed-prefix', 'v29-blit-prefix', 'v30-texture-prefix'):
            field(next(c for c in chunks if c.get('name') == 'MTLDevice::DeclareDescriptorCoverage'), 'version').text = {'v15':'15','v28-mixed-prefix':'28','v29-blit-prefix':'29','v30-texture-prefix':'30'}[case]
        elif case.startswith('missing-'):
            chunks.remove(frees[0 if case == 'missing-buffer-free' else 1])
        elif case in ('late-free', 'free-after-dispatch', 'free-after-commit'):
            move_after(frees[0], {'late-free': shader_birth if declared_version>=30 else birth, 'free-after-dispatch': dispatch, 'free-after-commit': commit}[case])
        elif case == 'duplicate-free':
            chunks.insert(list(chunks).index(frees[0]), copy.deepcopy(frees[0]))
        elif case in ('zero-generation', 'wrong-generation', 'wrong-type', 'wrong-offset', 'unaligned-offset', 'outside-buffer', 'unknown-buffer'):
            name, value = {'zero-generation': ('generation', '0'), 'wrong-generation': ('generation', '2'),
                           'wrong-type': ('descriptorType', '4'), 'wrong-offset': ('offset', '24'),
                           'unaligned-offset': ('offset', '1'), 'outside-buffer': ('offset', '48'),
                           'unknown-buffer': ('buffer', '0')}[case]
            field(frees[0], name).text = value
        elif case == 'unknown-source':
            field(bindings[0], 'resource').text = '0'
        elif case == 'free-with-payload':
            free_data = field(frees[0], 'data')
            free_data.text = field(update, 'data').text
            free_data.set('byteLength', '24')
        elif case in ('initial-payload-mismatch', 'retired-payload-mismatch'):
            blob_id = initial_blob if case == 'initial-payload-mismatch' else field(update, 'data').text
            name = f'{int(blob_id):06d}'
            data = bytearray(mutated[name])
            data[16] ^= 1  # Ordinary metadata must still match the frozen initial bytes.
            mutated[name] = bytes(data)
        elif case.startswith('view-'):
            view=next(c for c in frame if c.get('name')=='MTLBuffer::newTextureWithDescriptor')
            if case=='view-unknown-buffer':field(view,'Buffer').text='9999999'
            elif case=='view-range':field(view,'offset').text='65536'
            elif case=='view-storage':field(field(view,'descriptor'),'storageMode').text='2'
            elif case=='view-zero-width':field(field(view,'descriptor'),'width').text='0'
            elif case=='view-before-buffer':
                buffer=next(c for c in frame if c.get('name')=='MTLDevice::newBufferWithLength' and field(c,'Buffer').text==field(view,'Buffer').text)
                chunks.remove(view);chunks.insert(list(chunks).index(buffer),view)
        elif case.startswith('heap-texture-'):
            texture=next(c for c in frame if c.get('name')=='MTLHeap::newTexture(offset)')
            if case=='heap-texture-offset':field(texture,'offset').text='1'
            else:field(field(texture,'descriptor'),'width').text='0'
        elif case.startswith('copy-'):
            blit_copy=next(c for c in frame if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer')
            if case in ('copy-source-range','copy-destination-range'):
                field(blit_copy,'sourceOffset' if case=='copy-source-range' else 'destinationOffset').text='16'
            elif case=='copy-zero-size':field(blit_copy,'size').text='0'
            elif case=='copy-same-buffer':field(blit_copy,'destinationBuffer').text=field(blit_copy,'sourceBuffer').text
            elif case=='copy-unknown-source':field(blit_copy,'sourceBuffer').text='9999999'
            elif case=='copy-descriptor-destination':field(blit_copy,'destinationBuffer').text=retired_id
            elif case=='copy-unknown-encoder':field(blit_copy,'BlitCommandEncoder').text='9999999'
            elif case=='copy-after-end':move_after(blit_copy,next(c for c in frame if c.get('name')=='MTLBlitCommandEncoder::endEncoding'))
            elif case=='copy-after-commit':move_after(blit_copy,commit)
        for chunk in chunks:
            chunk.set('length', '0')
        target = folder / (case + '.zip.xml')
        tree.write(target, encoding='utf-8', xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            for name, blob in mutated.items():
                archive.writestr(name, blob)
        rdc = folder / (case + '.rdc')
        run(case + '-convert', [cli, 'convert', '-f', target, '-o', rdc, '-c', 'rdc'])
        run(case + '-api', [opener, rdc], True)
        run(case + '-cli', [cli, 'replay', '--loops', '1', rdc], True)
    print(f'PASS leading retirement: {len(cases)} API+CLI prefix/generation/source/frozen-byte groups')


if __name__ == '__main__':
    main()
