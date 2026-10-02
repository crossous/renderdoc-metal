#!/usr/bin/env python3
"""Reject invalid fresh CPU payload slots after earlier encoded work before frame GPU work."""
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
    cases=['old-coverage','depth-limit','unknown-encoder','pop-underflow','after-end']
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks')
        scope=next(c for c in nodes if c.get('id')=='5')

        data=dict(blobs)
        push=next(c for c in nodes if c.get('name')=='MTLBlitCommandEncoder::pushDebugGroup')
        encoder=field(push,'BlitCommandEncoder').text
        pop=next((c for c in nodes if c.get('name')=='MTLBlitCommandEncoder::popDebugGroup'),None)
        if pop is None:
            pop=copy.deepcopy(push);pop.set('name','MTLBlitCommandEncoder::popDebugGroup');pop.set('id',str(int(push.get('id'))+1))
            for child in list(pop)[1:]:pop.remove(child)
        end=next(c for c in nodes if c.get('name')=='MTLBlitCommandEncoder::endEncoding' and field(c,'BlitCommandEncoder').text==encoder)
        if label=='old-coverage':
            declaration=next(c for c in nodes if c.get('name')=='MTLDevice::DeclareDescriptorCoverage')
            field(declaration,'version').text='64'
        elif label=='depth-limit':
            for _ in range(65):nodes.insert(list(nodes).index(push),copy.deepcopy(push))
        elif label=='unknown-encoder':field(push,'BlitCommandEncoder').text='999999'
        elif label=='pop-underflow':
            if pop in list(nodes):nodes.remove(pop)
            nodes.insert(list(nodes).index(push),pop)
        elif label=='after-end':
            nodes.remove(push);nodes.insert(list(nodes).index(end)+1,push)
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,payload in data.items():archive.writestr(name,payload)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS blit marker ownership/stack: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
