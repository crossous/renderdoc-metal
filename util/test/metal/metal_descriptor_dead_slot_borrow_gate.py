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
    cases=['legacy-v64','missing-retirement','stale-generation','unsubmitted-consumer','unknown-consumer','short-value','missing-binding','wrong-binding-type','aux-borrow-before-retire','missing-aux-commit']
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks');data=dict(blobs)
        gpu={field(c,'buffer').text for c in nodes if c.get('name')=='MTLBuffer::DeclareDescriptorGPUWrites'}
        scope=next(c for c in nodes if c.get('id')=='5')
        events=[c for c in list(nodes)[list(nodes).index(scope)+1:] if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'buffer').text in gpu and field(c,'offset').text=='0']
        allocation=next(c for c in events if field(c,'event').text=='0')
        free=next(c for c in events if field(c,'event').text=='1')
        value=next(c for c in events if field(c,'event').text=='2')
        binding=nodes[list(nodes).index(value)+1]
        commit=next(c for c in nodes if c.get('name')=='MTLCommandBuffer::commit')
        if label=='legacy-v64':field(next(c for c in nodes if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='64'
        elif label=='missing-retirement':nodes.remove(free)
        elif label=='stale-generation':field(allocation,'generation').text='1'
        elif label=='unsubmitted-consumer':nodes.remove(commit)
        elif label=='unknown-consumer':field(commit,'CommandBuffer').text='999999'
        elif label=='short-value':
            payload=field(value,'data');old=f'{int(payload.text):06d}';key=f'{max(int(k) for k in data if k.isdigit())+1:06d}'
            data[key]=data[old][:-1];payload.text=str(int(key));payload.set('byteLength','23')
        elif label=='missing-binding':nodes.remove(binding)
        elif label=='wrong-binding-type':field(binding,'kind').text='0'
        elif label=='aux-borrow-before-retire':
            dispatches=[c for c in nodes if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups']
            nodes.remove(free);nodes.insert(list(nodes).index(dispatches[3])+1,free)
        elif label=='missing-aux-commit':
            commits=[c for c in nodes if c.get('name')=='MTLCommandBuffer::commit'];nodes.remove(commits[1])
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,payload in data.items():archive.writestr(name,payload)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS submitted generation retirement/consumer/value/source: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
