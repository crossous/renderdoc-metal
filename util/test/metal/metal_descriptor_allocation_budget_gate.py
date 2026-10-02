#!/usr/bin/env python3
"""Reject malformed/oversized backing allocation plans before Native creation and GPU work."""
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
    cases=['heap-zero','heap-over-limit','heap-duplicate','heap-in-frame','aggregate-over-limit']
    for label in cases:
        tree=copy.deepcopy(original);nodes=tree.find('./chunks')
        heap=next(c for c in nodes if c.get('name')=='MTLDevice::newHeapWithDescriptor')
        if label=='heap-zero':field(heap,'size').text='0'
        elif label=='heap-over-limit':field(heap,'size').text=str(129*1024*1024)
        elif label=='heap-duplicate':nodes.insert(list(nodes).index(heap)+1,copy.deepcopy(heap))
        elif label=='heap-in-frame':
            nodes.remove(heap)
            scope=next(c for c in nodes if c.get('id')=='5')
            nodes.insert(list(nodes).index(scope)+1,heap)
        elif label=='aggregate-over-limit':
            for index in range(32):
                extra=copy.deepcopy(heap);field(extra,'Heap').text=str(100000+index)
                field(extra,'size').text=str(128*1024*1024)
                nodes.insert(list(nodes).index(heap)+1,extra)
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for name,data in blobs.items():archive.writestr(name,data)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS allocation budget: {len(cases)} API+CLI rejection groups before frame GPU work')


if __name__=='__main__':main()
