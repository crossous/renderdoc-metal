#!/usr/bin/env python3
"""Prove complete submit-time Shared snapshots without promoting them to initial state."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

def main():
    cli, opener, replay, capture, folder=map(Path,sys.argv[1:6]);va=sys.argv[6]
    folder.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    def run(label,args,reject=False):
        p=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env,timeout=60)
        out=p.stdout+p.stderr;(folder/(label+'.log')).write_text(out)
        assert p.returncode in ((1,4) if reject else (0,)),(label,p.returncode,out)
        if reject:assert 'failed' in out.lower() and 'Metal replay wait begin' not in out,(label,out)
    def f(c,n):return next(v for v in c if v.get('name')==n)
    xml=folder/'source.zip.xml';run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
    original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as z:blobs={n:z.read(n) for n in z.namelist()}
    cs=original.find('./chunks');scope=next(i for i,c in enumerate(cs) if c.get('id')=='5')
    first_copy=next(c for c in list(cs)[scope+1:] if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer')
    source=f(first_copy,'sourceBuffer').text
    initial=next(c for c in cs if c.get('id')=='3' and f(c,'id').text==source)
    cs.remove(initial)
    cases=['positive','missing-snapshot','partial-snapshot','snapshot-range','snapshot-after-commit','private-source','legacy64','short-snapshot']
    for case in cases:
        tree=copy.deepcopy(original);chunks=tree.find('./chunks');data=dict(blobs)
        snapshots=[c for c in chunks if 'ModifyCPUContents' in c.get('name','') and f(c,'Buffer').text==source]
        assert snapshots,'fixture must record a Native full CPU change'
        snapshot=snapshots[0];commit=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::commit')
        birth=next(c for c in chunks if c.get('name')=='MTLDevice::newBufferWithLength' and f(c,'Buffer').text==source)
        if case=='missing-snapshot':
            for c in snapshots:chunks.remove(c)
        elif case in ('partial-snapshot','short-snapshot'):
            b=f(snapshot,'data');key=f'{int(b.text):06d}';value=data[key];value=value[:-1]
            data[key]=value;b.set('byteLength',str(len(value)))
            if case=='partial-snapshot':f(snapshot,'size').text=str(len(value))
        elif case=='snapshot-range':f(snapshot,'start').text='1'
        elif case=='snapshot-after-commit':chunks.remove(snapshot);chunks.insert(list(chunks).index(commit)+1,snapshot)
        elif case=='private-source':f(birth,'options').text='32'
        elif case=='legacy64':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='64'
        for c in chunks:c.set('length','0')
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as z:
            for n,v in data.items():z.writestr(n,v)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        if case=='positive':
            run('positive-replay',[replay,rdc,va]);run('positive-cli',[cli,'replay','--loops','1',rdc])
        else:
            run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print('PASS full submission initial Shared snapshot: 4 reset seeks and7 API+CLI rejection groups')
if __name__=='__main__':main()
