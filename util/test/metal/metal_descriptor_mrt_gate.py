#!/usr/bin/env python3
"""Check MRT attachment/pipeline compatibility and cross-pass bounds before GPU work."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def main():
    cli, opener, capture, folder = map(Path,sys.argv[1:5]);folder.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    def run(label,args,refuse=False):
        r=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env,timeout=20);out=r.stdout+r.stderr;(folder/(label+'.log')).write_text(out)
        assert r.returncode in ((1,4) if refuse else (0,)),(label,r.returncode,out)
        if refuse:assert 'failed' in out.lower() and 'Metal replay wait begin' not in out,(label,out)
    def f(c,n):return next(x for x in c if x.get('name')==n)
    run('positive-api',[opener,capture]);run('positive-cli',[cli,'replay','--loops','1',capture])
    xml=folder/'source.zip.xml';run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml']);original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive:blobs={n:archive.read(n) for n in archive.namelist()}
    cases=['missing-first-target','missing-second-target','duplicate-target','resolve-target','target-level','target-slice',
        'target-depth','store-discard','third-target','second-pass-extra-target','target-format-mismatch',
        'array-length','target-width','target-height','sample-count','second-pass-pipeline']
    for case in cases:
        tree=copy.deepcopy(original);chunks=tree.find('./chunks');passes=[f(c,'descriptor') for c in chunks if c.get('name') in ('MTLCommandBuffer::renderCommandEncoderWithDescriptor','MTLCommandBuffer::parallelRenderCommandEncoderWithDescriptor')]
        colors=f(passes[0],'colorAttachments');first,second=list(colors)[:2];lastColors=f(passes[1],'colorAttachments')
        if case=='missing-first-target':f(first,'texture').text='0'
        elif case=='missing-second-target':f(second,'texture').text='0'
        elif case=='duplicate-target':f(second,'texture').text=f(first,'texture').text
        elif case=='resolve-target':f(second,'resolveTexture').text=f(first,'texture').text
        elif case in ('target-level','target-slice','target-depth','store-discard'):
            name,value={'target-level':('level','1'),'target-slice':('slice','1'),'target-depth':('depthPlane','1'),'store-discard':('storeAction','0')}[case];f(second,name).text=value
        elif case=='third-target':colors.append(copy.deepcopy(second))
        elif case=='second-pass-extra-target':lastColors.append(copy.deepcopy(second))
        elif case=='target-format-mismatch':f(first,'texture').text=f(second,'texture').text
        elif case in ('array-length','target-width','target-height','sample-count'):
            name,value={'array-length':('renderTargetArrayLength','2'),'target-width':('renderTargetWidth','3'),
                'target-height':('renderTargetHeight','3'),'sample-count':('defaultRasterSampleCount','2')}[case];f(passes[0],name).text=value
        elif case=='second-pass-pipeline':
            pipelines=[c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::setRenderPipelineState'];f(pipelines[1],'pipelineState').text=f(pipelines[0],'pipelineState').text
        for c in chunks:c.set('length','0')
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for n,v in blobs.items():archive.writestr(n,v)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc']);run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS MRT: {len(cases)} API+CLI negative attachment/pipeline/pass bounds groups')


if __name__=='__main__':main()
