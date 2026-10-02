#!/usr/bin/env python3
"""Validate independent signal command ownership during unfinished render descriptor recording."""
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
    cases=['zero-value','unknown-event','wrong-event-type','unknown-command','own-active-signal',
           'own-active-commit','duplicate-signal','signal-after-commit','commit-before-signal',
           'reserved-queue-order']
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks');data=dict(blobs)
        signal=next(c for c in nodes if c.get('name')=='MTLCommandBuffer::encodeSignalEvent')
        signalCommand=field(signal,'CommandBuffer').text
        commit=next(c for c in nodes if c.get('name')=='MTLCommandBuffer::commit' and field(c,'CommandBuffer').text==signalCommand)
        owner=next(c for c in nodes if c.get('name')=='MTLCommandBuffer::renderCommandEncoderWithDescriptor')
        mainCommand=field(owner,'CommandBuffer').text
        if label=='zero-value':field(signal,'value').text='0'
        elif label=='unknown-event':field(signal,'event').text='999999'
        elif label=='wrong-event-type':field(signal,'event').text=field(next(c for c in nodes if c.get('name')=='MTLDevice::newBufferWithLength'),'Buffer').text
        elif label=='unknown-command':field(signal,'CommandBuffer').text='999999'
        elif label=='own-active-signal':field(signal,'CommandBuffer').text=mainCommand
        elif label=='own-active-commit':field(commit,'CommandBuffer').text=mainCommand
        elif label=='duplicate-signal':nodes.insert(list(nodes).index(signal)+1,copy.deepcopy(signal))
        elif label=='signal-after-commit':nodes.remove(signal);nodes.insert(list(nodes).index(commit)+1,signal)
        elif label=='commit-before-signal':nodes.remove(commit);nodes.insert(list(nodes).index(signal),commit)
        elif label=='reserved-queue-order':
            enqueue=next(c for c in nodes if c.get('name')=='MTLCommandBuffer::enqueue' and field(c,'CommandBuffer').text==signalCommand)
            other=next(c for c in nodes if c.get('name')=='MTLCommandBuffer::enqueue' and field(c,'CommandBuffer').text!=signalCommand)
            nodes.remove(enqueue);nodes.insert(list(nodes).index(other)+1,enqueue)
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,payload in data.items():archive.writestr(name,payload)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS interleaved signal/commit ownership/timeline/queue order: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
