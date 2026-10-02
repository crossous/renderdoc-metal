#!/usr/bin/env python3
"""Reject invalid TextureBuffer initial bytes and creation ranges before GPU uploads."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

def main():
    cli,opener,capture,folder=map(Path,sys.argv[1:5]);folder.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_INITIAL_PRIVATE='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')
    def run(label,args,refuse=False):
        r=subprocess.run(list(map(str,args)),env=env,capture_output=True,text=True,timeout=30)
        output=r.stdout+r.stderr;(folder/(label+'.log')).write_text(output)
        assert r.returncode in ((1,4) if refuse else (0,)),(label,r.returncode,output)
        if refuse:
            assert 'failed' in output.lower(),(label,output)
            for marker in ('Metal replay wait begin','Metal Private initial contents:','Metal texture initial contents upload'):
                assert marker not in output,(label,output)
    def f(c,name):return next(n for n in c if n.get('name')==name)
    xml=folder/'source.zip.xml';run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
    original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as z:blobs={n:z.read(n) for n in z.namelist()}
    cases=('offset-end','offset-misaligned','row-short','row-misaligned','width-limit','height-two',
           'mips-two','storage-shared','unsupported-format','unknown-parent','duplicate-initial','short-initial','long-initial','unknown-initial')
    for case in cases:
        tree=copy.deepcopy(original);data=dict(blobs);chunks=tree.find('./chunks')
        view=next(c for c in chunks if c.get('name')=='MTLBuffer::newTextureWithDescriptor')
        desc=f(view,'descriptor');parent=f(view,'Buffer').text
        creation=next(c for c in chunks if c.get('name')=='MTLDevice::newBufferWithLength' and f(c,'Buffer').text==parent)
        initial=next(c for c in chunks if c.get('id')=='3' and f(c,'id').text==parent)
        if case=='offset-end':f(view,'offset').text=f(creation,'length').text
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
        run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS TextureBuffer formats: {len(cases)} API+CLI initial-byte/format/range/alignment groups')
if __name__=='__main__':main()
