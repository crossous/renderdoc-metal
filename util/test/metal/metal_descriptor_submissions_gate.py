#!/usr/bin/env python3
"""Require completed submissions before CPU sourced descriptor reuse."""
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
    cases=['v9','missing-first-wait','missing-last-wait','duplicate-wait','unknown-wait','wait-before-commit',
        'missing-first-commit','missing-last-commit','duplicate-commit','unknown-commit','missing-first-birth',
        'missing-second-birth','duplicate-birth','conflicting-birth','unknown-queue','second-birth-before-wait',
        'cpu-update-before-wait','cpu-update-after-consumer','missing-encoder-end','wrong-first-encoder-command',
        'wrong-second-encoder-command','third-submission']
    if any(c.get('name')=='MTLCommandQueue::commandBufferWithDescriptor' for c in original.find('./chunks')):
        cases.append('descriptor-error-options')
    for case in cases:
        tree=copy.deepcopy(original);chunks=tree.find('./chunks');scope=next(i for i,c in enumerate(chunks) if c.get('id')=='5');frame=list(chunks)[scope+1:]
        births=[c for c in frame if c.get('name','').startswith('MTLCommandQueue::commandBuffer')];commits=[c for c in frame if c.get('name')=='MTLCommandBuffer::commit'];waits=[c for c in frame if c.get('name')=='MTLCommandBuffer::waitUntilCompleted']
        encoders=[c for c in frame if c.get('name')=='MTLCommandBuffer::computeCommandEncoder'];end=next(c for c in frame if c.get('name')=='MTLComputeCommandEncoder::endEncoding')
        event=next(c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and f(c,'event').text=='2')
        dispatches=[c for c in frame if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups']
        def move(c,before,after=False):chunks.remove(c);chunks.insert(list(chunks).index(before)+int(after),c)
        if case=='descriptor-error-options':f(next(c for c in births if c.get('name')=='MTLCommandQueue::commandBufferWithDescriptor'),'errorOptions').text='2'
        elif case=='v9':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='9'
        elif case.startswith('missing-'):chunks.remove({'missing-first-wait':waits[0],'missing-last-wait':waits[1],
            'missing-first-commit':commits[0],'missing-last-commit':commits[1],'missing-first-birth':births[0],
            'missing-second-birth':births[1],'missing-encoder-end':end}[case])
        elif case=='duplicate-wait':chunks.insert(list(chunks).index(waits[0]),copy.deepcopy(waits[0]))
        elif case=='unknown-wait':f(waits[0],'CommandBuffer').text='0'
        elif case=='wait-before-commit':move(waits[0],commits[0])
        elif case=='duplicate-commit':chunks.insert(list(chunks).index(commits[0]),copy.deepcopy(commits[0]))
        elif case=='unknown-commit':f(commits[0],'CommandBuffer').text='0'
        elif case=='duplicate-birth':chunks.insert(list(chunks).index(births[0]),copy.deepcopy(births[0]))
        elif case=='conflicting-birth':f(births[1],'CommandBuffer').text=f(births[0],'CommandBuffer').text
        elif case=='unknown-queue':f(births[1],'CommandQueue').text='0'
        elif case=='second-birth-before-wait':move(births[1],waits[0])
        elif case=='cpu-update-before-wait':move(event,waits[0])
        elif case=='cpu-update-after-consumer':move(event,dispatches[1],True)
        elif case=='wrong-first-encoder-command':f(encoders[0],'CommandBuffer').text=f(births[1],'CommandBuffer').text
        elif case=='wrong-second-encoder-command':f(encoders[1],'CommandBuffer').text=f(births[0],'CommandBuffer').text
        elif case=='third-submission':
            third=copy.deepcopy(births[1]);f(third,'CommandBuffer').text='999999';chunks.insert(list(chunks).index(waits[1])+1,third)
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for n,v in blobs.items():archive.writestr(n,v)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc']);run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS submissions: {len(cases)} API+CLI negative wait/commit/birth/CPU update ordering groups')


if __name__=='__main__':main()
