#!/usr/bin/env python3
"""Check captured per-use compute indirect arguments and reject incomplete/mismatched records before GPU work."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5])
    folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',
               RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    def run(label, args, reject=False):
        r = subprocess.run(list(map(str,args)), capture_output=True, text=True, env=env, timeout=30)
        text = r.stdout+r.stderr
        (folder/(label+'.log')).write_text(text)
        assert r.returncode in ((1,4) if reject else (0,)), (label,r.returncode,text)
        if reject:
            assert 'failed' in text.lower() and 'Metal replay wait begin' not in text, (label,text)
    def f(node,name): return next(n for n in node if n.get('name')==name)
    xml=folder/'source.zip.xml'
    run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml'])
    original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as z: blobs={n:z.read(n) for n in z.namelist()}
    nodes=original.find('./chunks')
    header_name='MTLDevice::CaptureComputeIndirectArgumentsCount'
    record_name='MTLComputeCommandEncoder::CaptureIndirectArguments'
    header=next(c for c in nodes if c.get('name')==header_name)
    records=[c for c in nodes if c.get('name')==record_name]
    assert f(header,'count').text=='3' and len(records)==3
    assert [int(f(c,'ordinal').text) for c in records]==[0,1,2]
    assert [[int(x.text) for x in f(c,'groups')] for c in records]==[[1,1,1],[3,1,1],[2,1,1]]
    cases=['count-zero','count-short','count-long','count-over-limit','header-missing','header-duplicate',
           'record-missing','records-all-missing','record-duplicate','groups-short','groups-long',
           'command-zero','command-other','encoder-zero','encoder-other','buffer-zero','buffer-other',
           'offset-unaligned','offset-other','ordinal-over-limit','ordinal-duplicate','record-in-frame']
    for label in cases:
        tree=copy.deepcopy(original); nodes=tree.find('./chunks')
        header=next(c for c in nodes if c.get('name')==header_name)
        records=[c for c in nodes if c.get('name')==record_name];first=records[0]
        if label.startswith('count-'):
            f(header,'count').text={'count-zero':'0','count-short':'2','count-long':'4','count-over-limit':'1025'}[label]
        elif label=='header-missing':nodes.remove(header)
        elif label=='header-duplicate':nodes.insert(list(nodes).index(header)+1,copy.deepcopy(header))
        elif label=='record-missing':nodes.remove(first)
        elif label=='records-all-missing':
            for c in records:nodes.remove(c)
        elif label=='record-duplicate':nodes.insert(list(nodes).index(first)+1,copy.deepcopy(first))
        elif label=='groups-short':f(first,'groups').remove(f(first,'groups')[-1])
        elif label=='groups-long':f(first,'groups').append(copy.deepcopy(f(first,'groups')[0]))
        elif label=='record-in-frame':
            scope=next(c for c in nodes if c.get('id')=='5');nodes.remove(first);nodes.insert(list(nodes).index(scope)+1,first)
        else:
            name,value={'command-zero':('command','0'),'command-other':('command','999999'),
                        'encoder-zero':('encoder','0'),'encoder-other':('encoder','999999'),
                        'buffer-zero':('buffer','0'),'buffer-other':('buffer','999999'),
                        'offset-unaligned':('offset','17'),'offset-other':('offset','20'),
                        'ordinal-over-limit':('ordinal','1024'),'ordinal-duplicate':('ordinal','1')}[label]
            f(first,name).text=value
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as z:
            for n,data in blobs.items():z.writestr(n,data)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS captured indirect evidence 1/3/2: {len(cases)} API+CLI negative groups; header, cardinality, identity, ordinal, source, offset and scope')

if __name__=='__main__':main()
