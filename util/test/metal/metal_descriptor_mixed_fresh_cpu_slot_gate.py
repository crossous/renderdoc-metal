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
    cases=['old-coverage','written-slot','opaque-table-write','short-value','unaligned-value','wrong-generation','missing-source','unknown-source']
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks')
        scope=next(c for c in nodes if c.get('id')=='5')

        gpu={field(c,'buffer').text for c in nodes if c.get('name')=='MTLBuffer::DeclareDescriptorGPUWrites'}
        value=next(c for c in list(nodes)[list(nodes).index(scope)+1:] if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'buffer').text in gpu and field(c,'offset').text=='24' and field(c,'event').text=='2')
        binding=nodes[list(nodes).index(value)+1]
        assert binding.get('name')=='MTLBuffer::DescriptorSlotBinding'
        data=dict(blobs)
        if label=='old-coverage':
            declaration=next(c for c in nodes if c.get('name')=='MTLDevice::DeclareDescriptorCoverage')
            field(declaration,'version').text='63'
        elif label=='written-slot':
            producer=next(c for c in nodes if c.get('name')=='MTLBuffer::DescriptorSlotProducer')
            field(producer,'offset').text='24'
        elif label=='opaque-table-write':
            producer=next(c for c in nodes if c.get('name')=='MTLBuffer::DescriptorSlotProducer')
            field(producer,'encoder').text='999999'
        elif label=='short-value':
            payload=field(value,'data');old=f'{int(payload.text):06d}'
            key=f'{max(int(k) for k in data if k.isdigit())+1:06d}'
            data[key]=data[old][:-1];payload.text=str(int(key));payload.set('byteLength',str(len(data[key])))
        elif label=='unaligned-value':field(value,'offset').text='1'
        elif label=='wrong-generation':field(value,'generation').text='9999'
        elif label=='missing-source':nodes.remove(binding)
        elif label=='unknown-source':field(binding,'resource').text='999999'
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,payload in data.items():archive.writestr(name,payload)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS mixed fresh CPU slot: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
