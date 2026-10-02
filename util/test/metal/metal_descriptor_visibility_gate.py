#!/usr/bin/env python3
"""Reject invalid visibility buffer identity/initialization/range/mode/encoder ownership before GPU replay."""
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
    cases=['missing-buffer','unknown-buffer','wrong-type','bad-mode','unaligned','outside','unknown-encoder','after-end','missing-initial','untracked-buffer']
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks');data=dict(blobs)
        mode=next(c for c in nodes if c.get('name')=='MTLRenderCommandEncoder::setVisibilityResultMode' and field(c,'mode').text=='2')
        render=next(c for c in nodes if c.get('name') in ('MTLCommandBuffer::renderCommandEncoderWithDescriptor','MTLCommandBuffer::parallelRenderCommandEncoderWithDescriptor') and field(field(c,'descriptor'),'visibilityResultBuffer').text!='0')
        descriptor=field(render,'descriptor');rid=field(descriptor,'visibilityResultBuffer').text
        if label=='missing-buffer':field(descriptor,'visibilityResultBuffer').text='0'
        elif label=='unknown-buffer':field(descriptor,'visibilityResultBuffer').text='999999'
        elif label=='wrong-type':field(descriptor,'visibilityResultBuffer').text=field(next(c for c in nodes if c.get('name')=='MTLDevice::newSamplerStateWithDescriptor'),'SamplerState').text
        elif label=='bad-mode':field(mode,'mode').text='3'
        elif label=='unaligned':field(mode,'offset').text='4'
        elif label=='outside':field(mode,'offset').text='32'
        elif label=='unknown-encoder':field(mode,'RenderCommandEncoder').text='999999'
        elif label=='after-end':
            encoder=field(mode,'RenderCommandEncoder').text
            end=next(c for c in nodes if c.get('name')=='MTLRenderCommandEncoder::endEncoding' and field(c,'RenderCommandEncoder').text==encoder)
            nodes.remove(mode);nodes.insert(list(nodes).index(end)+1,mode)
        elif label=='missing-initial':nodes.remove(next(c for c in nodes if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==rid))
        elif label=='untracked-buffer':
            create=next(c for c in nodes if c.get('name')=='MTLDevice::newBufferWithLength' and field(c,'Buffer').text==rid)
            field(create,'options').text='256'
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,payload in data.items():archive.writestr(name,payload)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS sourced visibility ownership/initialization/range/mode: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
