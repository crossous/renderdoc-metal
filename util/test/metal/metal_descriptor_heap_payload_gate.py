#!/usr/bin/env python3
"""Preflight frame-born heap descriptor payloads without allocating frame resources."""
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
    cases=['v10','missing-birth','duplicate-birth','unknown-heap','wrong-heap-type','private-options','untracked-options',
        'oversized-buffer','unaligned-offset','outside-heap','private-heap','static-overlap','frame-overlap',
        'table-before-birth','slot-before-table','missing-table','table-count','table-schema','identity-before-birth',
        'creation-before-completion','source-before-birth']
    for case in cases:
        tree=copy.deepcopy(original);chunks=tree.find('./chunks');scope=next(i for i,c in enumerate(chunks) if c.get('id')=='5');frame=list(chunks)[scope+1:]
        birth=next(c for c in frame if c.get('name')=='MTLHeap::newBuffer(offset)');bid=f(birth,'Buffer').text
        table=next(c for c in frame if c.get('name')=='MTLBuffer::DeclareDescriptorTable' and f(c,'buffer').text==bid)
        slot=next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and f(c,'buffer').text==bid and f(c,'event').text=='0')
        identity=next(c for c in frame if c.get('name')=='MTLResource::CaptureGPUIdentity' and f(c,'resource').text==bid)
        inline=next(c for c in frame if c.get('name')=='MTLCommandEncoder::DescriptorInlineBinding' and f(c,'resource').text==bid)
        heap=next(c for c in chunks if c.get('name')=='MTLDevice::newHeapWithDescriptor' and f(c,'Heap').text==f(birth,'Heap').text)
        wait=next(c for c in frame if c.get('name')=='MTLCommandBuffer::waitUntilCompleted')
        def move(c,before,after=False):chunks.remove(c);chunks.insert(list(chunks).index(before)+int(after),c)
        if case=='v10':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='10'
        elif case=='missing-birth':chunks.remove(birth)
        elif case=='duplicate-birth':chunks.insert(list(chunks).index(birth),copy.deepcopy(birth))
        elif case=='unknown-heap':f(birth,'Heap').text='0'
        elif case=='wrong-heap-type':f(birth,'Heap').text=next(f(c,'resource').text for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotBinding')
        elif case=='private-options':f(birth,'options').text='544'
        elif case=='untracked-options':f(birth,'options').text='256'
        elif case=='oversized-buffer':f(birth,'length').text='65537'
        elif case=='unaligned-offset':f(birth,'offset').text='1'
        elif case=='outside-heap':f(birth,'offset').text='65536'
        elif case=='private-heap':f(heap,'storageMode').text='2'
        elif case in ('static-overlap','frame-overlap'):
            fake=copy.deepcopy(birth);f(fake,'Buffer').text='999998'
            chunks.insert(scope if case=='static-overlap' else list(chunks).index(birth),fake)
        elif case=='table-before-birth':move(table,birth)
        elif case=='slot-before-table':move(slot,table)
        elif case=='missing-table':chunks.remove(table)
        elif case=='table-count':f(table,'count').text='2'
        elif case=='table-schema':f(table,'schema').text='0'
        elif case=='identity-before-birth':move(identity,birth)
        elif case=='creation-before-completion':move(birth,wait)
        elif case=='source-before-birth':move(inline,birth)
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for n,v in blobs.items():archive.writestr(n,v)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc']);run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS heap payload: {len(cases)} API+CLI negative creation/heap/range/order groups')


if __name__=='__main__':main()
