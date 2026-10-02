#!/usr/bin/env python3
"""Exercise forced diagnostic exits, exclusive flags, and a safe Native commit boundary."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

def main():
    cli,opener,capture,folder=map(Path,sys.argv[1:5]);folder.mkdir(parents=True,exist_ok=True)
    base=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_INITIAL_PRIVATE='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')
    flags=['RENDERDOC_METAL_CPU_METADATA_COVERAGE','RENDERDOC_METAL_PRE_SUBMIT_COVERAGE','RENDERDOC_METAL_INITIAL_UPLOAD_COVERAGE','RENDERDOC_METAL_FRAME_PREFIX_COVERAGE','RENDERDOC_METAL_FRAME_PREFIX_COMMITS']
    for k in flags:base.pop(k,None)
    def open_case(label,capture,extra,expected,gpu=False,wait=False):
        p=subprocess.run([str(opener),str(capture)],env=dict(base,**extra),capture_output=True,text=True,timeout=60)
        out=p.stdout+p.stderr;(folder/(label+'.log')).write_text(out)
        assert p.returncode==4 and expected in out,(label,p.returncode,out)
        assert ('initial contents upload' in out)==gpu,(label,out)
        assert ('Metal replay wait begin' in out)==wait,(label,out)
    open_case('initial-only',capture,{'RENDERDOC_METAL_INITIAL_UPLOAD_COVERAGE':'65'},'initial GPU uploads completed and frame replay was not executed',True)
    open_case('prefix-one',capture,{'RENDERDOC_METAL_FRAME_PREFIX_COVERAGE':'65','RENDERDOC_METAL_FRAME_PREFIX_COMMITS':'1'},'1 commits completed; capture loading stopped at commit boundary',True,True)
    negatives=[{'RENDERDOC_METAL_INITIAL_UPLOAD_COVERAGE':v} for v in ('','64','66','65x')]
    negatives += [{'RENDERDOC_METAL_FRAME_PREFIX_COVERAGE':'65','RENDERDOC_METAL_FRAME_PREFIX_COMMITS':v} for v in ('','0','257','-1','1x')]
    negatives += [{'RENDERDOC_METAL_FRAME_PREFIX_COVERAGE':'65'}, {'RENDERDOC_METAL_FRAME_PREFIX_COMMITS':'1'},
                  {'RENDERDOC_METAL_FRAME_PREFIX_COVERAGE':'64','RENDERDOC_METAL_FRAME_PREFIX_COMMITS':'1'}]
    for other in ('RENDERDOC_METAL_CPU_METADATA_COVERAGE','RENDERDOC_METAL_PRE_SUBMIT_COVERAGE'):
        negatives += [{'RENDERDOC_METAL_INITIAL_UPLOAD_COVERAGE':'65',other:'65'},
                      {'RENDERDOC_METAL_FRAME_PREFIX_COVERAGE':'65','RENDERDOC_METAL_FRAME_PREFIX_COMMITS':'1',other:'65'}]
    negatives += [{'RENDERDOC_METAL_INITIAL_UPLOAD_COVERAGE':'65','RENDERDOC_METAL_FRAME_PREFIX_COVERAGE':'65','RENDERDOC_METAL_FRAME_PREFIX_COMMITS':'1'}]
    for i,e in enumerate(negatives):open_case('flags-'+str(i),capture,e,'Invalid CPU-only Metal metadata diagnostic version')
    open_case('missing-boundary',capture,{'RENDERDOC_METAL_FRAME_PREFIX_COVERAGE':'65','RENDERDOC_METAL_FRAME_PREFIX_COMMITS':'256'},'rejected non-quiescent or missing commit boundary')
    xml=folder/'source.zip.xml'
    subprocess.run([str(cli),'convert','-f',str(capture),'-o',str(xml),'-c','zip.xml'],check=True,capture_output=True,timeout=60)
    tree=ET.parse(xml);cs=tree.find('./chunks');scope=next(i for i,c in enumerate(cs) if c.get('id')=='5')
    def f(c,n):return next(v for v in c if v.get('name')==n)
    creation=next(c for c in list(cs)[scope+1:] if c.get('name')=='MTLCommandQueue::commandBuffer')
    first_enqueue=next(c for c in list(cs)[scope+1:] if c.get('name')=='MTLCommandBuffer::enqueue')
    aux=copy.deepcopy(creation);f(aux,'CommandBuffer').text='9999999';cs.insert(list(cs).index(first_enqueue)+1,aux)
    enqueue=copy.deepcopy(first_enqueue)
    f(enqueue,'CommandBuffer').text='9999999';cs.insert(list(cs).index(aux)+1,enqueue)
    commit=copy.deepcopy(next(c for c in list(cs)[scope+1:] if c.get('name')=='MTLCommandBuffer::commit'))
    f(commit,'CommandBuffer').text='9999999';end=next(c for c in cs if c.get('id')=='6');cs.insert(list(cs).index(end),commit)
    for c in cs:c.set('length','0')
    target=folder/'reserved-future.zip.xml';tree.write(target,encoding='utf-8',xml_declaration=True)
    with zipfile.ZipFile(xml.with_suffix('')) as source, zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as dest:
        for n in source.namelist():dest.writestr(n,source.read(n))
    rdc=folder/'reserved-future.rdc';subprocess.run([str(cli),'convert','-f',str(target),'-o',str(rdc),'-c','rdc'],check=True,capture_output=True,timeout=60)
    open_case('reserved-future',rdc,{'RENDERDOC_METAL_FRAME_PREFIX_COVERAGE':'65','RENDERDOC_METAL_FRAME_PREFIX_COMMITS':'1'},'rejected non-quiescent or missing commit boundary')
    open_case('reserved-complete',rdc,{'RENDERDOC_METAL_FRAME_PREFIX_COVERAGE':'65','RENDERDOC_METAL_FRAME_PREFIX_COMMITS':'2'},'2 commits completed; capture loading stopped at commit boundary',True,True)
    print(f'PASS bounded diagnostics:3 Native positives/{len(negatives)+2} pre-GPU rejection groups, mandatory forced exits')
if __name__=='__main__':main()
