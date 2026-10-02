#!/usr/bin/env python3
"""Validate repeated heap residency declarations and reject malformed identities before frame GPU work."""
import copy
import os
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5])
    folder.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')
    def run(label, args, reject=False):
        result=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env,timeout=30)
        output=result.stdout+result.stderr
        (folder/(label+'.log')).write_text(output)
        assert result.returncode in ((1,4) if reject else (0,)),(label,result.returncode,output)
        if reject:
            assert 'failed' in output.lower() and 'Metal replay wait begin' not in output,(label,output)
    def field(node,name):return next(x for x in node if x.get('name')==name)
    xml=folder/'source.zip.xml'
    run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
    original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        blobs={name:archive.read(name) for name in archive.namelist()}
    chunks=list(original.find('./chunks'))
    declarations=[c for c in chunks if c.get('name') in ('MTLComputeCommandEncoder::useHeaps','MTLRenderCommandEncoder::useHeaps')]
    assert len(declarations)==3
    for c in declarations:
        ids=[x.text for x in field(c,'heaps')]
        assert len(ids)==4 and len(set(ids))==2 and ids[0]==ids[2] and ids[1]==ids[3]
    cases=['legacy-v64','unknown-first','wrong-type','unknown-duplicate','wrong-encoder',
           'wrong-array','too-many','after-end','render-stages','render-after-end']
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks');data=dict(blobs)
        compute=next(c for c in nodes if c.get('name')=='MTLComputeCommandEncoder::useHeaps')
        render=next(c for c in nodes if c.get('name')=='MTLRenderCommandEncoder::useHeaps')
        if label=='legacy-v64':
            field(next(c for c in nodes if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='64'
        elif label in ('unknown-first','unknown-duplicate'):
            list(field(compute,'heaps'))[0 if label=='unknown-first' else 2].text='999999'
        elif label=='wrong-type':
            buffer=field(next(c for c in nodes if c.get('name')=='MTLDevice::newBufferWithLength'),'Buffer').text
            list(field(compute,'heaps'))[0].text=buffer
        elif label=='wrong-encoder':field(compute,'ComputeCommandEncoder').text='999999'
        elif label=='wrong-array':field(compute,'arrayVariant').text='false'
        elif label=='too-many':
            array=field(compute,'heaps')
            while len(array)<33:array.append(copy.deepcopy(array[0]))
        elif label=='render-stages':field(render,'stagesValue').text='8'
        elif label in ('after-end','render-after-end'):
            c=render if label=='render-after-end' else compute
            kind='RenderCommandEncoder' if c is render else 'ComputeCommandEncoder'
            encoder=field(c,kind).text
            end=next(x for x in nodes if x.get('name')=='MTL'+kind+'::endEncoding' and field(x,kind).text==encoder)
            nodes.remove(c);nodes.insert(list(nodes).index(end)+1,c)
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,payload in data.items():archive.writestr(name,payload)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS duplicate heap residency identity/shape/ownership: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
