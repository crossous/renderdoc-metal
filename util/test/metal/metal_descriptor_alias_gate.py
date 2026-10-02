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
    cases=['v12','missing-alias','double-alias','alias-before-wait','alias-before-retirement',
        'missing-retirement','stale-source','stale-compute-residency','stale-render-residency','stale-direct-buffer',
        'alias-unknown','alias-static','alias-replacement-before-birth','replacement-before-alias',
        'replacement-before-completion','unaligned-offset','outside-heap','oversized-buffer','unknown-heap',
        'duplicate-birth','missing-birth','source-before-birth','private-options','untracked-options',
        'nonplacement-heap','retire-wrong-generation']
    for case in cases:
        tree=copy.deepcopy(original);chunks=tree.find('./chunks');scope=next(i for i,c in enumerate(chunks) if c.get('id')=='5');frame=list(chunks)[scope+1:]
        births=[c for c in frame if c.get('name')=='MTLHeap::newBuffer(offset)'];first,birth=births
        aid=f(first,'Buffer').text;bid=f(birth,'Buffer').text
        alias=next(c for c in frame if c.get('name')=='MTLBuffer::makeAliasable')
        free=next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and f(c,'event').text=='1')
        binding=next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and f(c,'resource').text==bid)
        wait=next(c for c in frame if c.get('name')=='MTLCommandBuffer::waitUntilCompleted')
        heap=next(c for c in chunks if c.get('name')=='MTLDevice::newHeapWithDescriptor' and f(c,'Heap').text==f(birth,'Heap').text)
        def move(c,before):chunks.remove(c);chunks.insert(list(chunks).index(before),c)
        if case=='v12':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='12'
        elif case=='missing-alias':chunks.remove(alias)
        elif case=='double-alias':chunks.insert(list(chunks).index(alias),copy.deepcopy(alias))
        elif case=='alias-before-wait':move(alias,wait)
        elif case=='alias-before-retirement':move(alias,free)
        elif case=='missing-retirement':chunks.remove(free)
        elif case=='stale-source':f(binding,'resource').text=aid
        elif case in ('stale-compute-residency','stale-render-residency'):
            prefix='MTLCompute' if case=='stale-compute-residency' else 'MTLRender'
            usage=next(c for c in frame if c.get('name').startswith(prefix) and 'useResource' in c.get('name') and f(c,'resource').text==bid);f(usage,'resource').text=aid
        elif case=='stale-direct-buffer':
            setter=[c for c in frame if c.get('name')=='MTLComputeCommandEncoder::setBuffer'][-1];f(setter,'buffer').text=aid
        elif case=='alias-unknown':f(alias,'Buffer').text='999998'
        elif case=='alias-static':f(alias,'Buffer').text=next(f(c,'resource').text for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and f(c,'kind').text=='0')
        elif case=='alias-replacement-before-birth':f(alias,'Buffer').text=bid
        elif case=='replacement-before-alias':move(birth,alias)
        elif case=='replacement-before-completion':move(birth,wait)
        elif case=='unaligned-offset':f(birth,'offset').text='1'
        elif case=='outside-heap':f(birth,'offset').text='65536'
        elif case=='oversized-buffer':f(birth,'length').text='65537'
        elif case=='unknown-heap':f(birth,'Heap').text='0'
        elif case=='duplicate-birth':chunks.insert(list(chunks).index(birth),copy.deepcopy(birth))
        elif case=='missing-birth':chunks.remove(birth)
        elif case=='source-before-birth':move(binding,birth)
        elif case=='private-options':f(birth,'options').text='544'
        elif case=='untracked-options':f(birth,'options').text='256'
        elif case=='nonplacement-heap':f(heap,'type').text='0'
        elif case=='retire-wrong-generation':f(free,'generation').text='999'
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for n,v in blobs.items():archive.writestr(n,v)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc']);run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS frame heap alias: {len(cases)} API+CLI negative retirement/completion/equal-VA identity/residency/range groups')


if __name__=='__main__':main()
