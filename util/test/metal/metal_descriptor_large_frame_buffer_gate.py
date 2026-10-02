#!/usr/bin/env python3
"""Reject malformed large frame sources before upload/dispatch/waits."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

def main():
    cli, opener, capture, folder=map(Path,sys.argv[1:5]);folder.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    def run(label,args,reject=False):
        p=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env,timeout=40)
        out=p.stdout+p.stderr;(folder/(label+'.log')).write_text(out)
        assert p.returncode in ((1,4) if reject else (0,)),(label,p.returncode,out)
        if reject:assert 'failed' in out.lower() and 'Metal replay wait begin' not in out,(label,out)
    def field(c,name):return next(n for n in c if n.get('name')==name)
    run('positive-api',[opener,capture]);run('positive-cli',[cli,'replay','--loops','1',capture])
    xml=folder/'source.zip.xml';run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
    original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as z:blobs={n:z.read(n) for n in z.namelist()}
    cs=original.find('./chunks');scope=next(i for i,c in enumerate(cs) if c.get('id')=='5')
    birth=next(c for c in list(cs)[scope+1:] if c.get('name') in ('MTLHeap::newBuffer(offset)','MTLDevice::newBufferWithLength') and int(field(c,'length').text)>65536)
    heap=birth.get('name')=='MTLHeap::newBuffer(offset)';private=(int(field(birth,'options').text)&0xf0)==32
    version=int(field(next(c for c in cs if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text)
    cases=['legacy39','zero-length','length-limit','options','unknown-source','source-member','source-range','missing-identity','identity-zero','duplicate-birth','late-birth',
           'copy-source-range','copy-destination-range','copy-unknown-source','copy-after-end','dispatch-too-wide','missing-inline-layout','missing-inline-binding','missing-inline-bytes']
    if heap:cases+=['unknown-heap','heap-offset','heap-range']
    if not private:cases+=['cpu-range','cpu-size']
    if version>=47:
        cases+=['legacy46','copy-zero-size','copy-size-limit','copy-overflow','copy-count-limit']
        if not heap:cases+=['copy-byte-budget']
    if version>=65:
        cases+=['legacy64']
        if heap:cases+=['copy-byte-budget']
    for case in cases:
        tree=copy.deepcopy(original);chunks=tree.find('./chunks');data=dict(blobs)
        scope=next(i for i,c in enumerate(chunks) if c.get('id')=='5')
        birth=next(c for c in list(chunks)[scope+1:] if c.get('name') in ('MTLHeap::newBuffer(offset)','MTLDevice::newBufferWithLength') and int(field(c,'length').text)>65536)
        rid=field(birth,'Buffer').text;length=int(field(birth,'length').text)
        binding=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'resource').text==rid)
        identity=next(c for c in chunks if c.get('name')=='MTLResource::CaptureGPUIdentity' and field(c,'resource').text==rid)
        if case=='legacy64':field(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='64'
        elif case=='legacy46':field(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='46'
        elif case=='legacy39':field(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='39'
        elif case in ('zero-length','length-limit'):field(birth,'length').text='0' if case=='zero-length' else str(((1024*1024 if version>=65 else 131072) if heap else 8*1024*1024)+1)
        elif case=='options':field(birth,'options').text='16'
        elif case=='unknown-source':field(binding,'resource').text='9999999'
        elif case in ('source-member','source-range'):field(binding,'memberOffset').text='0' if case=='source-member' else str(length)
        elif case=='missing-identity':chunks.remove(identity)
        elif case=='identity-zero':field(identity,'value').text='0'
        elif case=='duplicate-birth':chunks.insert(list(chunks).index(birth),copy.deepcopy(birth))
        elif case=='late-birth':chunks.remove(birth);chunks.insert(list(chunks).index(binding)+1,birth)
        elif case in ('unknown-heap','heap-offset','heap-range'):field(birth,'Heap' if case=='unknown-heap' else 'offset').text='9999999' if case=='unknown-heap' else '1' if case=='heap-offset' else str(2**32)
        elif case.startswith('copy-'):
            c=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer')
            copies=[n for n in chunks if n.get('name')=='MTLBlitCommandEncoder::copyFromBuffer']
            if case in ('copy-count-limit','copy-byte-budget'):
                chosen=copies[-1] if case=='copy-count-limit' else c
                count=257-len(copies) if case=='copy-count-limit' else (16*1024*1024//int(field(c,'size').text)+1)
                position=list(chunks).index(chosen)
                for _ in range(count):chunks.insert(position,copy.deepcopy(chosen))
            elif case=='copy-zero-size':field(c,'size').text='0'
            elif case=='copy-size-limit':field(c,'size').text=str(1024*1024+1)
            elif case=='copy-overflow':field(c,'size').text=str(2**64-1)
            elif case=='copy-source-range':field(c,'sourceOffset').text='1'
            elif case=='copy-destination-range':field(c,'destinationOffset').text=str(length-11)
            elif case=='copy-unknown-source':field(c,'sourceBuffer').text='9999999'
            elif case=='copy-after-end':
                end=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::endEncoding');chunks.remove(c);chunks.insert(list(chunks).index(end)+1,c)
        elif case=='dispatch-too-wide':field(field(next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups'),'groups'),'width').text='262145' if version>=46 else '2'
        elif case.startswith('missing-inline-'):
            name={'missing-inline-layout':'MTLCommandEncoder::DescriptorInlineLayout','missing-inline-binding':'MTLCommandEncoder::DescriptorInlineBinding','missing-inline-bytes':'MTLComputeCommandEncoder::setBytes'}[case]
            chunks.remove(next(c for c in chunks if c.get('name')==name))
        elif case.startswith('cpu-'):
            c=next(c for c in chunks if 'ModifyCPUContents' in c.get('name','') and field(c,'Buffer').text==rid)
            field(c,'start' if case=='cpu-range' else 'size').text=str(length if case=='cpu-range' else int(field(c,'size').text)+1)
        for c in chunks:c.set('length','0')
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as z:
            for name,value in data.items():z.writestr(name,value)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS large frame buffer: {len(cases)} API+CLI negative groups')
if __name__=='__main__':main()
