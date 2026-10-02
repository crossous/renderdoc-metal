#!/usr/bin/env python3
"""Validate simultaneous logical placement aliases and captured completion before reuse."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import struct
import xml.etree.ElementTree as ET
import zipfile


def main():
    cli,opener,capture,folder=map(Path,sys.argv[1:5]);folder.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    def run(label,args,refuse=False):
        r=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env,timeout=20);out=r.stdout+r.stderr;(folder/(label+'.log')).write_text(out)
        assert r.returncode in ((1,4) if refuse else (0,)),(label,r.returncode,out)
        if refuse:assert 'failed' in out.lower() and 'Metal replay wait begin' not in out,(label,out)
    def f(c,n):return next(x for x in c if x.get('name')==n)
    run('positive-api',[opener,capture]);run('positive-cli',[cli,'replay','--loops','1',capture])
    xml=folder/'source.zip.xml';run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml']);original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:blobs={n:archive.read(n) for n in archive.namelist()}
    # Verify the asynchronous fixture's actual capture chronology, independently of replay.
    chunks=list(original.find('./chunks')); scope=next(i for i,c in enumerate(chunks) if c.get('id')=='5')
    frame=chunks[scope+1:]
    commits=[i for i,c in enumerate(frame) if c.get('name')=='MTLCommandBuffer::commit']
    waits=[i for i,c in enumerate(frame) if c.get('name')=='MTLCommandBuffer::waitUntilCompleted']
    assert len(commits)==2 and len(waits)==1 and waits[0]>commits[-1]
    binding=next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and f(c,'kind').text=='0')
    aid=f(binding,'resource').text
    writes=[(i,c) for i,c in enumerate(frame) if c.get('name')=='Internal_MTLBufferModifyCPUContents' and f(c,'Buffer').text==aid]
    private=int(f(next(c for c in chunks if c.get('name')=='MTLHeap::newBuffer(offset)' and f(c,'Buffer').text==aid),'options').text)&0xf0==32
    frame_birth=any(c.get('name')=='MTLHeap::newBuffer(offset)' and f(c,'Buffer').text==aid for c in frame)
    if private:
        assert not writes, 'Private resources must not receive CPU snapshots'
        if frame_birth:
            dispatches=[c for c in frame if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups']
            assert len(dispatches)==4
            initializer=f(dispatches[0],'ComputeCommandEncoder').text
            assert any(c.get('name')=='MTLComputeCommandEncoder::setBuffer' and
                f(c,'ComputeCommandEncoder').text==initializer and f(c,'buffer').text==aid and f(c,'index').text=='2' for c in frame)
        else:
            initial=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and f(c,'id').text==aid)
            assert struct.unpack_from('<III',blobs[f'{int(f(initial,"Contents").text):06d}'])==(0xbad,41,80)
    else:
        saved_output=any(c.get('name')=='MTLDevice::newBufferWithLength' and f(c,'length').text=='16' for c in chunks)
        expected_input=(0xbad,103 if saved_output else 41,80)
        first_snapshot=any(i<commits[0] and int(f(c,'start').text)==0 and
            struct.unpack_from('<III',blobs[f'{int(f(c,"data").text):06d}'])==expected_input for i,c in writes)
        initial=next((c for c in chunks if c.get('name')=='Internal::Initial Contents' and f(c,'id').text==aid),None)
        background_initial=(not frame_birth and initial is not None and
            struct.unpack_from('<III',blobs[f'{int(f(initial,"Contents").text):06d}'])==(0xbad,41,80))
        assert first_snapshot or background_initial
    birth=next(c for c in reversed(frame) if c.get('name')=='MTLHeap::newBuffer(offset)')
    bid=f(birth,'Buffer').text
    delayed=frame.index(birth)<commits[0]
    if delayed:assert frame.index(birth)>next(i for i,c in enumerate(frame) if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups')
    writer=next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotProducer')
    encoder=f(writer,'encoder').text
    assert any(c.get('name')=='MTLComputeCommandEncoder::setBuffer' and
        f(c,'ComputeCommandEncoder').text==encoder and f(c,'buffer').text==bid and f(c,'index').text=='2' for c in frame)
    b_writes=[(i,c) for i,c in enumerate(frame) if c.get('name')=='Internal_MTLBufferModifyCPUContents' and f(c,'Buffer').text==bid]
    if private:assert not b_writes
    else:
        # A conservative mapped-heap snapshot of newly created B is legitimate.
        # It must retain A's pre-GPU-write value 41; the GPU producer supplies 80.
        for i,c in b_writes:
            start=int(f(c,'start').text); data=blobs[f'{int(f(c,"data").text):06d}']
            assert i<commits[-1]
            if start<=4 and len(data)>=8-start:assert struct.unpack('<I',data[4-start:8-start])[0]==expected_input[1]

    assert sum(c.get('name')=='MTLComputeCommandEncoder::useHeaps' for c in frame)==2
    assert sum(c.get('name')=='MTLRenderCommandEncoder::useHeaps' for c in frame)==1
    print('PASS capture chronology: one final wait, initial GPU/captured contents or owned Shared commit snapshot, GPU writer B at index 2, indirect heap declarations')
    cases=['old-contract','unknown-contract','old-async-contract','missing-commit',
        'explicit-retirement-with-live-source','stale-source-id','unaligned-offset','outside-heap',
        'oversized-buffer','zero-length','unknown-heap','duplicate-birth','missing-birth',
        'source-before-birth','private-options','untracked-options','nonplacement-heap','untracked-heap',
        'heap-unknown-resource','heap-wrong-type','heap-wrong-encoder','heap-wrong-array','heap-wrong-stages']
    large=int(f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text)>=42
    if large:cases+=['old-large-contract']
    if delayed:cases+=['old-encoding-contract']
    if private:cases+=['old-private-contract','missing-private-initial' if not frame_birth else 'missing-private-initializer-seed','wrong-private-heap-storage','private-backing-table']
    for case in cases:
        tree=copy.deepcopy(original);chunks=tree.find('./chunks');scope=next(i for i,c in enumerate(chunks) if c.get('id')=='5');frame=list(chunks)[scope+1:]
        births=[c for c in frame if c.get('name')=='MTLHeap::newBuffer(offset)']
        background=len(births)==1
        if background:
            birth=births[0]
            source=next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and f(c,'kind').text=='0')
            first=next(c for c in chunks if c.get('name')=='MTLHeap::newBuffer(offset)' and f(c,'Buffer').text==f(source,'resource').text)
        else:first,birth=births
        aid=f(first,'Buffer').text;bid=f(birth,'Buffer').text
        assert not any(c.get('name')=='MTLBuffer::makeAliasable' for c in frame)
        assert not any(c.get('name')=='MTLBuffer::DescriptorSlotEvent' and f(c,'event').text=='1' for c in frame)
        binding=next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and f(c,'resource').text==aid)
        wait=next(c for c in frame if c.get('name')=='MTLCommandBuffer::waitUntilCompleted')
        heap=next(c for c in chunks if c.get('name')=='MTLDevice::newHeapWithDescriptor' and f(c,'Heap').text==f(birth,'Heap').text)
        def move(c,before):chunks.remove(c);chunks.insert(list(chunks).index(before),c)
        if case=='old-large-contract':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='41'
        elif case=='old-contract':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='17' if background else '16'
        elif case=='unknown-contract':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='65535'
        elif case=='old-async-contract':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='21'
        elif case=='missing-commit':chunks.remove(next(c for c in frame if c.get('name')=='MTLCommandBuffer::commit'))
        elif case=='explicit-retirement-with-live-source':
            retire=ET.Element('chunk',{'id':'1344','name':'MTLBuffer::makeAliasable','length':'0'})
            # Get the actual chunk ID from the sibling explicit-retirement fixture's enum.
            enum_header=Path(__file__).resolve().parents[3]/'renderdoc/driver/metal/metal_common.h'
            import re
            names=re.search(r'enum class MetalChunk.*?\{(.*?)\n\};',enum_header.read_text(),re.S)
            assert names
            entries=[x.strip().split('=')[0].strip() for x in names.group(1).split(',')]
            # Enum values are sequential after the explicit FirstDriverChunk assignment.
            first=entries.index('MTLCreateSystemDefaultDevice')
            retire.set('id',str(1000+entries.index('MTLBuffer_makeAliasable')-first))
            ET.SubElement(retire,'ResourceId',{'name':'Buffer','typename':'MTLBuffer','width':'8'}).text=aid
            chunks.insert(list(chunks).index(birth),retire)
        elif case=='stale-source-id':f(binding,'resource').text='999998'
        elif case=='unaligned-offset':f(birth,'offset').text='1'
        elif case=='outside-heap':f(birth,'offset').text=f(heap,'size').text
        elif case=='oversized-buffer':f(birth,'length').text=str(131073 if int(f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text)>=42 else 65537)
        elif case=='zero-length':f(birth,'length').text='0'
        elif case=='unknown-heap':f(birth,'Heap').text='0'
        elif case=='duplicate-birth':chunks.insert(list(chunks).index(birth),copy.deepcopy(birth))
        elif case=='missing-birth':chunks.remove(birth)
        elif case=='source-before-birth':move(binding,first)
        elif case=='private-options':f(birth,'options').text='512' if private else '544'
        elif case=='untracked-options':f(birth,'options').text='256'
        elif case=='nonplacement-heap':f(heap,'type').text='0'
        elif case=='untracked-heap':f(heap,'hazardMode').text='1'
        elif case=='old-encoding-contract':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='23'
        elif case=='old-private-contract':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='22'
        elif case=='missing-private-initial':chunks.remove(next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and f(c,'id').text==aid))
        elif case=='missing-private-initializer-seed':
            initializer=f(next(c for c in frame if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups'),'ComputeCommandEncoder').text
            chunks.remove(next(c for c in frame if c.get('name')=='MTLComputeCommandEncoder::setBuffer' and f(c,'ComputeCommandEncoder').text==initializer and f(c,'index').text=='3'))
        elif case=='wrong-private-heap-storage':f(heap,'storageMode').text='0'
        elif case=='private-backing-table':
            table=f(binding,'buffer').text
            create=next(c for c in chunks if c.get('name') in ('MTLDevice::newBufferWithBytes','MTLDevice::newBufferWithLength') and f(c,'Buffer').text==table)
            f(create,'options').text='544'
        elif case.startswith('heap-'):
            declaration=next(c for c in frame if c.get('name')=='MTLComputeCommandEncoder::useHeaps')
            if case=='heap-unknown-resource':list(f(declaration,'heaps'))[0].text='999999'
            elif case=='heap-wrong-type':list(f(declaration,'heaps'))[0].text=aid
            elif case=='heap-wrong-encoder':f(declaration,'ComputeCommandEncoder').text='999999'
            elif case=='heap-wrong-array':f(declaration,'arrayVariant').text='false'
            elif case=='heap-wrong-stages':
                declaration=next(c for c in frame if c.get('name')=='MTLRenderCommandEncoder::useHeaps')
                f(declaration,'stagesValue').text='8'
        for c in chunks:c.set('length','0')
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for n,v in blobs.items():archive.writestr(n,v)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc']);run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS implicit placement alias: {len(cases)} API+CLI negative completion/live-source/contract/range groups')


if __name__=='__main__':main()
