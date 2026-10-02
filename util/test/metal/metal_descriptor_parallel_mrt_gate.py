#!/usr/bin/env python3
"""Check sourced parallel parent/child ownership and five-attachment compatibility."""
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
    source_chunks=original.find('./chunks')
    parallel=any(c.get('name')=='MTLCommandBuffer::parallelRenderCommandEncoderWithDescriptor' for c in source_chunks)
    cases=['old-contract','missing-first-target','missing-fifth-target','duplicate-fifth-target','fifth-resolve-target',
        'fifth-level','fifth-slice','fifth-discard','fifth-format-mismatch','second-pass-extra-target',
        'first-pass-pipeline','second-pass-pipeline','target-width','target-height','sample-count','array-length']
    if parallel:cases+=['unknown-parent','unknown-command','child-before-parent','missing-parent',
        'child-after-parent-end','parent-ended-with-child','active-sibling','duplicate-child',
        'duplicate-parent','duplicate-parent-end','missing-child-end','missing-parent-end','commit-before-parent-end',
        'missing-store-resolution','duplicate-store-resolution','unknown-store-parent','store-resolution-index',
        'store-resolution-discard','store-after-parent-end']
    for case in cases:
        tree=copy.deepcopy(original);chunks=tree.find('./chunks');passes=[f(c,'descriptor') for c in chunks if c.get('name') in ('MTLCommandBuffer::renderCommandEncoderWithDescriptor','MTLCommandBuffer::parallelRenderCommandEncoderWithDescriptor')]
        colors=f(passes[0],'colorAttachments');first,second=list(colors)[:2];lastColors=f(passes[1],'colorAttachments')
        fifth=list(colors)[4]
        if case=='old-contract':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='19'
        elif case=='missing-first-target':f(first,'texture').text='0'
        elif case=='missing-fifth-target':f(fifth,'texture').text='0'
        elif case=='duplicate-fifth-target':f(fifth,'texture').text=f(second,'texture').text
        elif case=='fifth-resolve-target':f(fifth,'resolveTexture').text=f(first,'texture').text
        elif case in ('fifth-level','fifth-slice','fifth-discard'):
            name,value={'fifth-level':('level','1'),'fifth-slice':('slice','1'),'fifth-discard':('storeAction','0')}[case];f(fifth,name).text=value
        elif case=='fifth-format-mismatch':f(first,'texture').text=f(fifth,'texture').text
        elif case=='second-pass-extra-target':lastColors.append(copy.deepcopy(fifth))
        elif case in ('first-pass-pipeline','second-pass-pipeline'):
            pipelines=[c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::setRenderPipelineState']
            i=0 if case=='first-pass-pipeline' else 1;f(pipelines[i],'pipelineState').text=f(pipelines[1-i],'pipelineState').text
        elif case in ('array-length','target-width','target-height','sample-count'):
            name,value={'array-length':('renderTargetArrayLength','2'),'target-width':('renderTargetWidth','3'),
                'target-height':('renderTargetHeight','3'),'sample-count':('defaultRasterSampleCount','2')}[case];f(passes[0],name).text=value
        else:
            parent=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::parallelRenderCommandEncoderWithDescriptor')
            children=[c for c in chunks if c.get('name')=='MTLParallelRenderCommandEncoder::renderCommandEncoder']
            child_ends=[c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::endEncoding']
            parent_end=next(c for c in chunks if c.get('name')=='MTLParallelRenderCommandEncoder::endEncoding')
            commit=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::commit')
            def move(c,before):chunks.remove(c);chunks.insert(list(chunks).index(before),c)
            if case=='unknown-parent':f(children[0],'ParallelRenderCommandEncoder').text='0'
            elif case=='unknown-command':f(parent,'CommandBuffer').text='0'
            elif case=='child-before-parent':move(children[0],parent)
            elif case=='missing-parent':chunks.remove(parent)
            elif case=='child-after-parent-end':move(parent_end,children[1])
            elif case=='parent-ended-with-child':move(parent_end,child_ends[1])
            elif case=='active-sibling':move(children[1],child_ends[0])
            elif case=='duplicate-child':f(children[1],'RenderCommandEncoder').text=f(children[0],'RenderCommandEncoder').text
            elif case=='duplicate-parent':chunks.insert(list(chunks).index(parent),copy.deepcopy(parent))
            elif case=='duplicate-parent-end':chunks.insert(list(chunks).index(parent_end),copy.deepcopy(parent_end))
            elif case=='missing-child-end':chunks.remove(child_ends[1])
            elif case=='missing-parent-end':chunks.remove(parent_end)
            elif case=='commit-before-parent-end':move(commit,parent_end)
            else:
                store=next(c for c in chunks if c.get('name')=='MTLParallelRenderCommandEncoder::setColorStoreAction')
                if case=='missing-store-resolution':chunks.remove(store)
                elif case=='duplicate-store-resolution':chunks.insert(list(chunks).index(store),copy.deepcopy(store))
                elif case=='unknown-store-parent':f(store,'ParallelRenderCommandEncoder').text='0'
                elif case=='store-resolution-index':f(store,'index').text='7'
                elif case=='store-resolution-discard':f(store,'storeAction').text='0'
                elif case=='store-after-parent-end':
                    chunks.remove(store);chunks.insert(list(chunks).index(parent_end)+1,store)
        for c in chunks:c.set('length','0')
        target=folder/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for n,v in blobs.items():archive.writestr(n,v)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc']);run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS parallel/five MRT: {len(cases)} API+CLI negative attachment/pipeline/parent-child/commit groups')


if __name__=='__main__':main()
