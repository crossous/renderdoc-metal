#!/usr/bin/env python3
"""Sourced render/compute counter attachment validation before frame submission."""
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
    create=next(c for c in nodes if c.get('name')=='MTLDevice::newCounterSampleBufferWithDescriptor')
    count=int(f(create,'sampleCount').text)
    wrong=next(f(c,'Buffer').text for c in nodes if 'newBuffer' in c.get('name','') and any(n.get('name')=='Buffer' for n in c))
    cases=[('create-count-zero','create','sampleCount','0'),('create-count-small','create','sampleCount','3'),
           ('create-set-unknown','create','counterSetName','unknown'),('create-storage-private','create','storageMode','2')]
    for kind,indices in [('render',['startOfVertexSampleIndex','endOfVertexSampleIndex','startOfFragmentSampleIndex','endOfFragmentSampleIndex']),
                         ('compute',['startOfEncoderSampleIndex','endOfEncoderSampleIndex'])]:
        cases += [(kind+'-id-zero',kind,'sampleBufferId','0'),(kind+'-object-zero',kind,'sampleBuffer','0'),
                  (kind+'-object-unknown',kind,'sampleBuffer','999999'),(kind+'-object-type',kind,'sampleBuffer',wrong),
                  (kind+'-id-unknown',kind,'sampleBufferId','999999')]
        cases += [(kind+'-'+index,kind,index,str(count)) for index in indices]
    cases += [('compute-array-short','compute','short',''),('compute-array-long','compute','long',''),
              ('render-array-long','render','long',''),('compute-empty-with-index','compute','empty',''),
              ('compute-dispatch-invalid','compute','dispatchType','99')]
    for label,kind,name,value in cases:
        tree=copy.deepcopy(original); nodes=tree.find('./chunks')
        if kind=='create':
            c=next(c for c in nodes if c.get('name')=='MTLDevice::newCounterSampleBufferWithDescriptor'); f(c,name).text=value
        else:
            if kind=='render':
                c=next(c for c in nodes if c.get('name') in ('MTLCommandBuffer::renderCommandEncoderWithDescriptor','MTLCommandBuffer::parallelRenderCommandEncoderWithDescriptor'))
                attachments=f(f(c,'descriptor'),'sampleBufferAttachments')
            else:
                c=next(c for c in nodes if c.get('name')=='MTLCommandBuffer::computeCommandEncoderWithDescriptor');attachments=f(c,'attachments')
            if name=='short': attachments.remove(attachments[-1])
            elif name=='long':
                while len(attachments)<=4: attachments.append(copy.deepcopy(attachments[0]))
            elif name=='empty':
                f(attachments[0],'sampleBuffer').text='0';f(attachments[0],'sampleBufferId').text='0'
            elif name=='dispatchType': f(c,name).text=value
            else: f(attachments[0],name).text=value
        for c in nodes:c.set('length','0')
        target=folder/(label+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as z:
            for n,data in blobs.items():z.writestr(n,data)
        rdc=folder/(label+'.rdc')
        run(label+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(label+'-api',[opener,rdc],True);run(label+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS render/compute counter attachment preflight: {len(cases)} API+CLI negative groups')

if __name__=='__main__':main()
