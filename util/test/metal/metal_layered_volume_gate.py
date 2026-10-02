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
    cases=['legacy-v64','missing-dispatch','no-UAV-slot','missing-root-binding','zero-work','read-source-uncommitted','layer-overflow','level-outside','depth-plane-outside','mixed-target-shape']
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks');data=dict(blobs)
        dispatch=next(c for c in nodes if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups')
        slot=next(c for c in nodes if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'event').text=='0' and field(c,'descriptorType').text=='5')
        if label=='legacy-v64':field(next(c for c in nodes if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='64'
        elif label=='missing-dispatch':nodes.remove(dispatch)
        elif label=='no-UAV-slot':
            for c in nodes:
                if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'descriptorType').text=='5':field(c,'descriptorType').text='4'
        elif label=='missing-root-binding':nodes.remove(next(c for c in nodes if c.get('name')=='MTLCommandEncoder::DescriptorInlineBinding'))
        elif label=='zero-work':field(field(dispatch,'groups'),'width').text='0'
        elif label=='read-source-uncommitted':
            # A writer recorded on another unsubmitted command is not a predecessor.
            create=next(c for c in nodes if c.get('name').startswith('MTLCommandQueue::commandBuffer'))
            new=copy.deepcopy(create);field(new,'CommandBuffer').text='999998';nodes.insert(list(nodes).index(create),new)
            begin=next(c for c in nodes if c.get('name').startswith('MTLCommandBuffer::computeCommandEncoder'))
            field(begin,'CommandBuffer').text='999998'
        elif label in ('layer-overflow','level-outside','depth-plane-outside'):
            passes=[c for c in nodes if c.get('name')=='MTLCommandBuffer::renderCommandEncoderWithDescriptor']
            descriptor=field(passes[1],'descriptor')
            if label=='layer-overflow':field(descriptor,'renderTargetArrayLength').text='5'
            else:
                attachment=field(descriptor,'colorAttachments')[0]
                field(attachment,'level' if label=='level-outside' else 'depthPlane').text='1'
        elif label=='mixed-target-shape':
            births=[c for c in nodes if c.get('name')=='MTLHeap::newTexture(offset)']
            desc=field(births[1],'descriptor');field(desc,'textureType').text='2';field(desc,'depth').text='1'
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,payload in data.items():archive.writestr(name,payload)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS frame color Load Native predecessor/typed UAV/work: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
