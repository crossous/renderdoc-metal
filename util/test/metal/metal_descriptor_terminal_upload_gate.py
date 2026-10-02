#!/usr/bin/env python3
"""Keep terminal upload-buffer discard behind existing GPU completion and no-use-after scans."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

def main():
    cli,opener,capture,folder=map(Path,sys.argv[1:5]);folder.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')
    def run(label,args,reject=False):
        p=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env,timeout=45)
        out=p.stdout+p.stderr;(folder/(label+'.log')).write_text(out)
        assert p.returncode in ((1,4) if reject else (0,)),(label,p.returncode,out)
        if reject:assert 'failed' in out.lower() and 'Metal replay wait begin' not in out,(label,out)
    def f(c,n):return next(v for v in c if v.get('name')==n)
    run('positive-api',[opener,capture]);run('positive-cli',[cli,'replay','--loops','1',capture])
    xml=folder/'source.zip.xml';run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
    original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as z:blobs={n:z.read(n) for n in z.namelist()}
    cases=['legacy64','keep-current','nonvolatile','volatile','invalid-state','unknown-buffer','texture-buffer','duplicate','use-after-empty','unsubmitted-consumer','table-buffer']
    for case in cases:
        tree=copy.deepcopy(original);cs=tree.find('./chunks')
        purge=next(c for c in cs if c.get('name')=='MTLBuffer::setPurgeableState')
        rid=f(purge,'Buffer').text
        if case=='legacy64':f(next(c for c in cs if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='64'
        elif case in ('keep-current','nonvolatile','volatile','invalid-state'):f(purge,'State').text={'keep-current':'1','nonvolatile':'2','volatile':'3','invalid-state':'5'}[case]
        elif case=='unknown-buffer':f(purge,'Buffer').text='9999999'
        elif case=='texture-buffer':f(purge,'Buffer').text=f(next(c for c in cs if c.get('name')=='MTLDevice::newTextureWithDescriptor'),'Texture').text
        elif case=='table-buffer':f(purge,'Buffer').text=f(next(c for c in cs if c.get('name')=='MTLBuffer::DeclareDescriptorTable'),'buffer').text
        elif case=='duplicate':cs.insert(list(cs).index(purge)+1,copy.deepcopy(purge))
        elif case=='use-after-empty':
            c=next(c for c in cs if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and f(c,'sourceBuffer').text==rid)
            cs.remove(purge);cs.insert(list(cs).index(c),purge)
        elif case=='unsubmitted-consumer':cs.remove(next(c for c in cs if c.get('name')=='MTLCommandBuffer::commit'))
        for c in cs:c.set('length','0')
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as z:
            for n,v in blobs.items():z.writestr(n,v)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print('PASS terminal Shared upload buffers: 11 API+CLI completion/state/lifetime rejection groups')
if __name__=='__main__':main()
