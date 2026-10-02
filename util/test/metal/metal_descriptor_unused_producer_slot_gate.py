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
    cases=['old-coverage','missing-producers','consumer-reads-empty']
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks')
        scope=next(c for c in nodes if c.get('id')=='5')

        data=dict(blobs)
        if label=='old-coverage':
            declaration=next(c for c in nodes if c.get('name')=='MTLDevice::DeclareDescriptorCoverage')
            field(declaration,'version').text='62'
        elif label=='missing-producers':
            for c in list(nodes):
                if c.get('name')=='MTLBuffer::DescriptorSlotProducer':nodes.remove(c)
        elif label=='consumer-reads-empty':
            gpu={field(c,'buffer').text for c in nodes if c.get('name')=='MTLBuffer::DeclareDescriptorGPUWrites'}
            retirement=next(c for c in nodes if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and
                field(c,'buffer').text in gpu and field(c,'offset').text=='24' and field(c,'event').text=='1')
            nodes.remove(retirement)
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,payload in data.items():archive.writestr(name,payload)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS unused producer destination slot: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
