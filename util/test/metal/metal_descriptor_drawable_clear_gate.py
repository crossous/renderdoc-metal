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
    cases=['legacy-v64','missing-clear','Load-without-initial','DontCare-without-initial','read-before-clear','clear-unsubmitted-other-command','partial-clear-extent','unrecognized-image-birth','wrong-native-format']
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks');data=dict(blobs)
        begin=next(c for c in nodes if c.get('name')=='MTLCommandBuffer::renderCommandEncoderWithDescriptor')
        descriptor=field(begin,'descriptor');attachment=field(descriptor,'colorAttachments')[0]
        # Keep this suite exercising absence of acquired initial pixels, even
        # after newer capture builds preserve them automatically.
        for initial in list(nodes):
            if initial.get('name')=='Internal::Initial Contents' and field(initial,'id').text==field(attachment,'texture').text:nodes.remove(initial)
        if label=='legacy-v64':field(next(c for c in nodes if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='64'
        elif label=='missing-clear':
            end=next(c for c in nodes if c.get('name')=='MTLRenderCommandEncoder::endEncoding');nodes.remove(begin);nodes.remove(end)
        elif label in ('Load-without-initial','DontCare-without-initial'):field(attachment,'loadAction').text='1' if label.startswith('Load') else '0'
        elif label=='read-before-clear':
            cs=next(c for c in nodes if c.get('name')=='MTLCommandBuffer::computeCommandEncoder');end=next(c for c in nodes if c.get('name')=='MTLRenderCommandEncoder::endEncoding')
            index=list(nodes).index(cs);nodes.remove(begin);nodes.remove(end)
            csEnd=next(c for c in nodes if c.get('name')=='MTLComputeCommandEncoder::endEncoding');index=list(nodes).index(csEnd)+1;nodes.insert(index,begin);nodes.insert(index+1,end)
        elif label=='clear-unsubmitted-other-command':
            create=next(c for c in nodes if c.get('name').startswith('MTLCommandQueue::commandBuffer'))
            new=copy.deepcopy(create);field(new,'CommandBuffer').text='999998';nodes.insert(list(nodes).index(create),new);field(begin,'CommandBuffer').text='999998'
        elif label=='partial-clear-extent':field(descriptor,'renderTargetWidth').text='1'
        elif label=='unrecognized-image-birth':
            image=next(c for c in nodes if c.get('name')=='[CAMetalLayer nextDrawable]' and field(c,'Texture').text==field(attachment,'texture').text);image.set('id','1009');image.set('name','MTLDevice::newTextureWithDescriptor')
        elif label=='wrong-native-format':
            image=next(c for c in nodes if c.get('name')=='[CAMetalLayer nextDrawable]' and field(c,'Texture').text==field(attachment,'texture').text);field(field(image,'descriptor'),'pixelFormat').text='70'
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,payload in data.items():archive.writestr(name,payload)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS drawable initialization/order/extent/identity: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
