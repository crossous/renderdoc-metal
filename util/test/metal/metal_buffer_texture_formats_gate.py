#!/usr/bin/env python3
"""Reject invalid TextureBuffer initial bytes and creation ranges before GPU uploads."""
import fcntl
import signal
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

def main():
    cli,opener,capture,folder=map(Path,sys.argv[1:5]);target_format=int(sys.argv[5]) if len(sys.argv)>5 else None;folder.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_INITIAL_PRIVATE='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')
    def run(label,args,refuse=False,preGPU=True):
        backend=folder/(label+'-renderdoc.log');child_env=dict(env,RENDERDOC_DEBUG_LOG_FILE=str(backend))
        with backend.open('a') as keep,(folder/(label+'.log')).open('w') as log:
            fcntl.flock(keep,fcntl.LOCK_SH)
            child=subprocess.Popen(list(map(str,args)),env=child_env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            try:code=child.wait(timeout=30)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid,signal.SIGKILL);child.wait();code=124
        output=(folder/(label+'.log')).read_text(errors='replace')+backend.read_text(errors='replace')
        assert not any(marker in output for marker in ('Assertion failed','failed assertion','OVERRUNNING CHUNK','m_ResourceMap.empty')),(label,output)
        assert code in ((1,4) if refuse else (0,)),(label,code,output)
        if refuse:
            assert 'failed' in output.lower(),(label,output)
            for marker in (() if not preGPU else ('Metal replay wait begin','Metal Private initial contents:','Metal texture initial contents upload')):
                assert marker not in output,(label,output)
    def f(c,name):return next(n for n in c if n.get('name')==name)
    xml=folder/'source.zip.xml';run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
    original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as z:blobs={n:z.read(n) for n in z.namelist()}
    cases=('offset-end','offset-misaligned','row-short','row-misaligned','width-limit','height-two',
           'width-zero','width-overflow','unknown-usage','optimized','mips-two','storage-shared','unsupported-format','unknown-parent','duplicate-initial','short-initial','long-initial','unknown-initial')
    if target_format==113:
        original_view=next(c for c in original.find('./chunks') if c.get('name')=='MTLBuffer::newTextureWithDescriptor' and int(f(f(c,'descriptor'),'pixelFormat').text)==113)
        if int(f(f(original_view,'descriptor'),'usage').text)==3:cases+=('writer-readonly','writer-missing-texture','writer-grid-overflow')
    for case in cases:
        tree=copy.deepcopy(original);data=dict(blobs);chunks=tree.find('./chunks')
        view=next(c for c in chunks if c.get('name')=='MTLBuffer::newTextureWithDescriptor' and (target_format is None or int(f(f(c,'descriptor'),'pixelFormat').text)==target_format))
        desc=f(view,'descriptor');parent=f(view,'Buffer').text
        creation=next(c for c in chunks if c.get('name')=='MTLDevice::newBufferWithLength' and f(c,'Buffer').text==parent)
        initial=next(c for c in chunks if c.get('id')=='3' and f(c,'id').text==parent)
        if case=='width-zero':f(desc,'width').text='0'
        elif case=='width-overflow':f(desc,'width').text='18446744073709551615'
        elif case=='unknown-usage':f(desc,'usage').text='19'
        elif case=='writer-readonly':f(desc,'usage').text='1'
        elif case in ('writer-missing-texture','writer-grid-overflow'):
            dispatches=[c for c in chunks if c.get('name') in ('MTLComputeCommandEncoder::dispatchThreads','MTLComputeCommandEncoder::dispatchThreadgroups')]
            writer=dispatches[2];encoder=f(writer,'ComputeCommandEncoder').text
            if case=='writer-grid-overflow':f(f(writer,'grid'),'width').text='18446744073709551615'
            else:
                binding=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::setTexture' and f(c,'ComputeCommandEncoder').text==encoder)
                f(binding,'texture').text='0'
        elif case=='optimized':f(desc,'allowGPUOptimizedContents').text='true'
        elif case=='offset-end':f(view,'offset').text=f(creation,'length').text
        elif case=='offset-misaligned':f(view,'offset').text='1'
        elif case=='row-short':f(view,'bytesPerRow').text='1'
        elif case=='row-misaligned':f(view,'bytesPerRow').text=str(int(f(view,'bytesPerRow').text)+1)
        elif case=='width-limit':f(desc,'width').text='33554433'
        elif case=='height-two':f(desc,'height').text='2'
        elif case=='mips-two':f(desc,'mipmapLevelCount').text='2'
        elif case=='storage-shared':f(desc,'storageMode').text='0'
        elif case=='unsupported-format':f(desc,'pixelFormat').text='252'
        elif case=='unknown-parent':f(view,'Buffer').text='9999999'
        elif case=='duplicate-initial':chunks.insert(list(chunks).index(initial),copy.deepcopy(initial))
        elif case=='unknown-initial':f(initial,'id').text='9999999'
        else:
            payload=f(initial,'Contents');key=f'{int(payload.text):06d}'
            data[key]=data[key][:-1] if case=='short-initial' else data[key]+b'\x01';payload.set('byteLength',str(len(data[key])))
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as z:
            for key,value in data.items():z.writestr(key,value)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        preGPU=not case.startswith('writer-')
        run(case+'-api',[opener,rdc],True,preGPU);run(case+'-cli',[cli,'replay','--loops','1',rdc],True,preGPU)
    print(f'PASS TextureBuffer formats: 18 API+CLI initial-byte/format/range/alignment groups before initial GPU uploads; {len(cases)-18} writer groups rejected during loading before writer dispatch (initial uploads allowed)')
if __name__=='__main__':main()
