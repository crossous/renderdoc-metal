#!/usr/bin/env python3
"""Reject malformed signal-only descriptor submissions before any captured GPU work."""
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
    cases=['old-contract','unknown-contract','zero-first','duplicate-second','zero-second',
        'unknown-event','wrong-event-type','unknown-command','signal-before-birth',
        'signal-after-commit','signal-with-active-encoder','duplicate-signal','signal-as-wait',
        'host-mutation','committed-command-signal']
    import re
    header=Path(__file__).resolve().parents[3]/'renderdoc/driver/metal/metal_common.h'
    body=re.search(r'enum class MetalChunk.*?\{(.*?)\n\};',header.read_text(),re.S).group(1)
    body=re.sub(r'//[^\n]*','',body)
    entries=[x.strip().split('=')[0].strip() for x in body.split(',')]
    def enum_id(name):return str(1000+entries.index(name))
    for case in cases:
        tree=copy.deepcopy(original);chunks=tree.find('./chunks');scope=next(i for i,c in enumerate(chunks) if c.get('id')=='5');frame=list(chunks)[scope+1:]
        signals=[c for c in frame if c.get('name')=='MTLCommandBuffer::encodeSignalEvent'];assert len(signals)==2
        first,second=signals
        command=f(first,'CommandBuffer').text
        birth=next(c for c in frame if c.get('name').startswith('MTLCommandQueue::commandBuffer') and f(c,'CommandBuffer').text==command)
        commit=next(c for c in frame if c.get('name')=='MTLCommandBuffer::commit' and f(c,'CommandBuffer').text==command)
        end=next(c for c in frame if c.get('name')=='MTLComputeCommandEncoder::endEncoding')
        def move(c,before):chunks.remove(c);chunks.insert(list(chunks).index(before),c)
        if case=='old-contract':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='18'
        elif case=='unknown-contract':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='65535'
        elif case=='zero-first':f(first,'value').text='0'
        elif case=='duplicate-second':f(second,'value').text=f(first,'value').text
        elif case=='zero-second':f(second,'value').text='0'
        elif case=='unknown-event':f(first,'event').text='0'
        elif case=='wrong-event-type':f(first,'event').text=next(f(c,'buffer').text for c in chunks if c.get('name')=='MTLBuffer::DeclareDescriptorTable')
        elif case=='unknown-command':f(first,'CommandBuffer').text='0'
        elif case=='signal-before-birth':move(first,birth)
        elif case=='signal-after-commit':
            chunks.remove(first);chunks.insert(list(chunks).index(commit)+1,first)
        elif case=='signal-with-active-encoder':move(first,end)
        elif case=='duplicate-signal':chunks.insert(list(chunks).index(first),copy.deepcopy(first))
        elif case=='signal-as-wait':first.set('name','MTLCommandBuffer::encodeWaitForEvent');first.set('id',enum_id('MTLCommandBuffer_encodeWaitForEvent'))
        elif case=='host-mutation':
            c=ET.Element('chunk',{'name':'MTLSharedEvent::unsupportedHostMutation','id':enum_id('MTLSharedEvent_unsupportedHostMutation')})
            chunks.insert(list(chunks).index(first),c)
        elif case=='committed-command-signal':f(second,'CommandBuffer').text=command
        for c in chunks:c.set('length','0')
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for n,v in blobs.items():archive.writestr(n,v)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc']);run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS signal-only descriptors: {len(cases)} API+CLI negative timeline/ownership/wait/host-mutation groups')


if __name__=='__main__':main()
