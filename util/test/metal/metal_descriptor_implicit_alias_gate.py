#!/usr/bin/env python3
"""Validate simultaneous logical placement aliases and captured completion before reuse."""
import copy
import os
from pathlib import Path
import subprocess
import sys
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
    cases=['old-contract','unknown-contract','missing-wait','birth-before-wait','missing-commit',
        'explicit-retirement-with-live-source','stale-source-id','unaligned-offset','outside-heap',
        'oversized-buffer','zero-length','unknown-heap','duplicate-birth','missing-birth',
        'source-before-birth','private-options','untracked-options','nonplacement-heap','untracked-heap']
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
        if case=='old-contract':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='17' if background else '16'
        elif case=='unknown-contract':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='65535'
        elif case=='missing-wait':chunks.remove(wait)
        elif case=='birth-before-wait':move(birth,wait)
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
        elif case=='outside-heap':f(birth,'offset').text='65536'
        elif case=='oversized-buffer':f(birth,'length').text='65537'
        elif case=='zero-length':f(birth,'length').text='0'
        elif case=='unknown-heap':f(birth,'Heap').text='0'
        elif case=='duplicate-birth':chunks.insert(list(chunks).index(birth),copy.deepcopy(birth))
        elif case=='missing-birth':chunks.remove(birth)
        elif case=='source-before-birth':move(binding,first)
        elif case=='private-options':f(birth,'options').text='544'
        elif case=='untracked-options':f(birth,'options').text='256'
        elif case=='nonplacement-heap':f(heap,'type').text='0'
        elif case=='untracked-heap':f(heap,'hazardMode').text='1'
        for c in chunks:c.set('length','0')
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for n,v in blobs.items():archive.writestr(n,v)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc']);run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS implicit placement alias: {len(cases)} API+CLI negative completion/live-source/contract/range groups')


if __name__=='__main__':main()
