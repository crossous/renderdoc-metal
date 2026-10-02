#!/usr/bin/env python3
"""Require a typed full-entry source, GPU destination, copy and expected-value order."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5])
    compute = len(sys.argv) == 6 and sys.argv[5] == 'compute'
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
               RENDERDOC_METAL_TRACE_INITIAL_PRIVATE='1', RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    def run(label, args, refuse=False):
        result = subprocess.run(list(map(str,args)), capture_output=True,text=True,env=env,timeout=20)
        text = result.stdout + result.stderr
        (folder/(label+'.log')).write_text(text)
        assert result.returncode in ((1,4) if refuse else (0,)), (label,result.returncode,text)
        if refuse:
            assert 'failed' in text.lower(), (label,text)
            assert 'Metal replay wait begin' not in text and 'Private initial contents upload' not in text, (label,text)
    def field(chunk,name): return next(n for n in chunk if n.get('name')==name)
    def first(chunks,name): return next(c for c in chunks if c.get('name')==name)
    run('positive-api',[opener,capture]); run('positive-cli',[cli,'replay','--loops','1',capture])
    xml=folder/'source.zip.xml'; run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
    original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs={name:archive.read(name) for name in archive.namelist()}
    cases=['missing-gpu-ownership','v4-gpu-copy','missing-copy','duplicate-copy','partial-copy',
        'source-offset','destination-offset','unknown-source','wrong-destination','copy-after-end',
        'missing-expected','expected-before-copy','expected-after-consumer','missing-source-binding',
        'wrong-source-binding','wrong-source-member','expected-va','expected-constant',
        'cpu-overwrite','initial-destination-mismatch','initial-payload-mismatch']
    if compute:
        cases = ['missing-gpu-ownership', 'v5-compute', 'missing-copy', 'duplicate-copy',
            'source-offset', 'destination-offset', 'unknown-source', 'wrong-destination',
            'producer-before-dispatch', 'producer-after-end', 'unknown-encoder', 'missing-dispatch',
            'oversized-grid', 'missing-expected', 'expected-before-copy', 'expected-after-consumer',
            'missing-source-binding', 'wrong-source-binding', 'wrong-source-member',
            'expected-va', 'expected-constant', 'cpu-overwrite',
            'initial-destination-mismatch', 'initial-payload-mismatch']
    for case in cases:
        tree,data=copy.deepcopy(original),dict(blobs); chunks=tree.find('./chunks')
        scope=next(i for i,c in enumerate(chunks) if c.get('id')=='5'); frame=list(chunks)[scope+1:]
        transfer=first(frame,'MTLBuffer::DescriptorSlotProducer' if compute else 'MTLBlitCommandEncoder::copyFromBuffer')
        expected=next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'event').text=='3')
        source=field(transfer,'source' if compute else 'sourceBuffer').text
        destination=field(transfer,'buffer' if compute else 'destinationBuffer').text
        binding=next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'buffer').text==destination and list(chunks).index(c)>list(chunks).index(expected))
        producer_dispatch = [c for c in frame if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups'][1] if compute else None
        if case=='missing-gpu-ownership': chunks.remove(first(chunks,'MTLBuffer::DeclareDescriptorGPUWrites'))
        elif case=='v5-compute': field(first(chunks,'MTLDevice::DeclareDescriptorCoverage'),'version').text='5'
        elif case=='v4-gpu-copy': field(first(chunks,'MTLDevice::DeclareDescriptorCoverage'),'version').text='4'
        elif case=='missing-copy': chunks.remove(transfer)
        elif case=='duplicate-copy': chunks.insert(list(chunks).index(transfer),copy.deepcopy(transfer))
        elif case=='partial-copy': field(transfer,'size').text='16'
        elif case=='source-offset': field(transfer,'sourceOffset').text='8'
        elif case=='destination-offset': field(transfer,'offset' if compute else 'destinationOffset').text='8'
        elif case=='unknown-source': field(transfer,'source' if compute else 'sourceBuffer').text='999999'
        elif case=='wrong-destination': field(transfer,'buffer' if compute else 'destinationBuffer').text=source
        elif case=='producer-before-dispatch':
            chunks.remove(transfer); chunks.insert(list(chunks).index(producer_dispatch), transfer)
        elif case=='producer-after-end':
            encoder=field(transfer,'encoder').text
            end=next(c for c in frame if c.get('name')=='MTLComputeCommandEncoder::endEncoding' and field(c,'ComputeCommandEncoder').text==encoder)
            chunks.remove(transfer); chunks.insert(list(chunks).index(end)+1, transfer)
        elif case=='unknown-encoder': field(transfer,'encoder').text='999999'
        elif case=='missing-dispatch': chunks.remove(producer_dispatch)
        elif case=='oversized-grid': field(producer_dispatch,'groups').find("uint[@name='width']").text='2'
        elif case=='copy-after-end':
            end=first(frame,'MTLBlitCommandEncoder::endEncoding'); chunks.remove(transfer); chunks.insert(list(chunks).index(end)+1,transfer)
        elif case=='missing-expected': chunks.remove(expected)
        elif case=='expected-before-copy': chunks.remove(expected); chunks.insert(list(chunks).index(transfer),expected)
        elif case=='expected-after-consumer':
            consumer=[c for c in frame if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups'][2 if compute else 1]
            chunks.remove(expected); chunks.remove(binding)
            index=list(chunks).index(consumer)+1; chunks.insert(index,expected); chunks.insert(index+1,binding)
        elif case=='missing-source-binding': chunks.remove(binding)
        elif case=='wrong-source-binding': field(binding,'resource').text=source
        elif case=='wrong-source-member': field(binding,'memberOffset').text='4'
        elif case in ('expected-va','expected-constant'):
            key=f'{int(field(expected,"data").text):06d}'; value=bytearray(data[key]); offset=0 if case=='expected-va' else 16
            value[offset:offset+8]=b'\xff'*8; data[key]=bytes(value)
        elif case=='cpu-overwrite':
            field(expected,'event').text='2'; field(expected,'generation').text='1'; field(expected,'descriptorType').text='5'
        elif case in ('initial-destination-mismatch','initial-payload-mismatch'):
            resource=destination if case=='initial-destination-mismatch' else source
            initial=next((c for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==resource), None)
            create = next((c for c in chunks if c.get('name')=='MTLDevice::newBufferWithBytes' and field(c,'Buffer').text==resource), None)
            if initial is not None: node = field(initial, 'Contents')
            elif create in frame and case=='initial-payload-mismatch':
                # A frame-born CPU payload can legitimately rewrite its creation bytes.
                # Corrupt the authoritative typed initializer after its declaration.
                node = field(next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'buffer').text==resource and field(c,'event').text=='2'), 'data')
            else: node = field(create, 'initialData')
            key=f'{int(node.text):06d}'; value=bytearray(data[key]); value[0:8]=b'\xff'*8; data[key]=bytes(value)
        target=folder/(case+'.zip.xml'); tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,value in data.items(): archive.writestr(name,value)
        rdc=folder/(case+'.rdc'); run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(case+'-api',[opener,rdc],True); run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS sourced GPU {"compute" if compute else "copy"}: {len(cases)} API+CLI negative groups rejected before GPU submission')
if __name__=='__main__': main()
