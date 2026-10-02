#!/usr/bin/env python3
"""Validate Native graphics scalar buffer bindings and reject invalid range/identity/lifetime before frame GPU work."""
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
    cases=['legacy-v64']+[stage+'-'+case for stage in ('vertex','fragment') for case in
        ('unknown','wrong-type','unaligned','outside','slot-limit','nil-offset','after-end','missing-active')]
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks');data=dict(blobs)
        if label=='legacy-v64':
            field(next(c for c in nodes if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='64'
        else:
            stage,case=label.split('-',1)
            method='MTLRenderCommandEncoder::set'+stage.title()+'Buffer'
            binding=next(c for c in nodes if c.get('name')==method and field(c,'index').text=='2')
            if case=='unknown':field(binding,'buffer').text='999999'
            elif case=='wrong-type':
                field(binding,'buffer').text=field(next(c for c in nodes if c.get('name')=='MTLDevice::newSamplerStateWithDescriptor'),'SamplerState').text
            elif case=='unaligned':field(binding,'offset').text='1'
            elif case=='outside':field(binding,'offset').text='32'
            elif case=='slot-limit':field(binding,'index').text='31'
            elif case=='nil-offset':field(binding,'buffer').text='0';field(binding,'offset').text='4'
            elif case=='missing-active':nodes.remove(binding)
            elif case=='after-end':
                encoder=field(binding,'RenderCommandEncoder').text
                end=next(c for c in nodes if c.get('name')=='MTLRenderCommandEncoder::endEncoding' and field(c,'RenderCommandEncoder').text==encoder)
                nodes.remove(binding);nodes.insert(list(nodes).index(end)+1,binding)
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,payload in data.items():archive.writestr(name,payload)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS graphics buffer identity/range/ownership: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
