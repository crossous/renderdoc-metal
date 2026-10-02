#!/usr/bin/env python3
"""Validate frame color Load only after an original compute dispatch with a live sourced UAV binding."""
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
    cases=['legacy-v64','missing-initial','duplicate-initial','short-initial','wrong-format','unknown-target','wrong-source-kind','Load-unrecognized-without-initial']
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks');data=dict(blobs)
        begin=next(c for c in nodes if c.get('name')=='MTLCommandBuffer::renderCommandEncoderWithDescriptor');attachment=field(field(begin,'descriptor'),'colorAttachments')[0];target=field(attachment,'texture').text
        initial=next(c for c in nodes if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==target)
        image=next(c for c in nodes if c.get('name')=='[CAMetalLayer nextDrawable]' and field(c,'Texture').text==target)
        if label=='legacy-v64':field(next(c for c in nodes if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='64'
        elif label=='missing-initial':nodes.remove(initial)
        elif label=='duplicate-initial':nodes.insert(list(nodes).index(initial)+1,copy.deepcopy(initial))
        elif label=='short-initial':
            payload=field(initial,'Contents');old=f'{int(payload.text):06d}';key=f'{max(int(k) for k in data if k.isdigit())+1:06d}'
            data[key]=data[old][:-1];payload.text=str(int(key));payload.set('byteLength','15')
        elif label=='wrong-format':field(field(image,'descriptor'),'pixelFormat').text='10'
        elif label=='unknown-target':field(attachment,'texture').text='999999'
        elif label=='wrong-source-kind':
            binding=next(c for c in nodes if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'resource').text==target);field(binding,'kind').text='0'
        elif label=='Load-unrecognized-without-initial':
            image.set('id','1009');image.set('name','MTLDevice::newTextureWithDescriptor');nodes.remove(initial)
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,payload in data.items():archive.writestr(name,payload)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS acquired drawable Native initial pixels/Load/identity: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
