#!/usr/bin/env python3
"""Validate deferred attachment final actions and encoder ownership before frame submission."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5])
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
               RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    def run(label, args, reject=False):
        r = subprocess.run(list(map(str,args)), capture_output=True, text=True, env=env, timeout=30)
        text = r.stdout+r.stderr
        (folder/(label+'.log')).write_text(text)
        assert r.returncode in ((1,4) if reject else (0,)), (label,r.returncode,text)
        if reject:
            assert 'failed' in text.lower() and 'Metal replay wait begin' not in text, (label,text)
    def f(node,name): return next(n for n in node if n.get('name')==name)
    xml=folder/'source.zip.xml'
    run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
    original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as z: blobs={n:z.read(n) for n in z.namelist()}
    nodes=original.find('./chunks')
    stores=[c for c in nodes if c.get('name','').endswith(('::setColorStoreAction','::setDepthStoreAction','::setStencilStoreAction'))]
    cases=[]
    for i,c in enumerate(stores):
        aspect=c.get('name').split('::')[-1]
        cases += [(f'{i}-{aspect}-missing',i,'missing'),(f'{i}-{aspect}-discard',i,'discard'),
                  (f'{i}-{aspect}-unknown',i,'unknown'),(f'{i}-{aspect}-resolve',i,'resolve'),
                  (f'{i}-{aspect}-encoder',i,'encoder'),(f'{i}-{aspect}-after-end',i,'after-end')]
        if aspect=='setColorStoreAction':cases += [(f'{i}-{aspect}-index',i,'index')]
    assert stores and cases
    for label,ordinal,kind in cases:
        tree=copy.deepcopy(original); nodes=tree.find('./chunks')
        store=[c for c in nodes if c.get('name','').endswith(('::setColorStoreAction','::setDepthStoreAction','::setStencilStoreAction'))][ordinal]
        parallel=store.get('name').startswith('MTLParallelRenderCommandEncoder::')
        encoder='ParallelRenderCommandEncoder' if parallel else 'RenderCommandEncoder'
        if kind=='missing':nodes.remove(store)
        elif kind in ('discard','unknown','resolve'):f(store,'storeAction').text={'discard':'0','unknown':'4','resolve':'2'}[kind]
        elif kind=='encoder':f(store,encoder).text='999999'
        elif kind=='index':f(store,'index' if parallel else 'colorAttachmentIndex').text='7'
        elif kind=='after-end':
            owner=f(store,encoder).text
            end=next(c for c in nodes if c.get('name')==('MTLParallelRenderCommandEncoder::endEncoding' if parallel else 'MTLRenderCommandEncoder::endEncoding') and f(c,encoder).text==owner)
            nodes.remove(store);nodes.insert(list(nodes).index(end)+1,store)
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as z:
            for n,data in blobs.items():z.writestr(n,data)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS deferred final stores: {len(cases)} API+CLI negative groups; missing, invalid final action, attachment, owner and end order')

if __name__=='__main__':main()
