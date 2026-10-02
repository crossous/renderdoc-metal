#!/usr/bin/env python3
"""Validate stage-specific sourced roots and bounded draws before GPU submission."""
import copy
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile


def main():
    cli, opener, capture, folder = map(Path, sys.argv[1:5]); folder.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MTL_DEBUG_LAYER='1', RENDERDOC_METAL_TRACE_REPLAY_WAITS='1', RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1')
    def run(label, args, refuse=False):
        r = subprocess.run(list(map(str, args)), capture_output=True, text=True, env=env, timeout=20)
        output=r.stdout+r.stderr; (folder/(label+'.log')).write_text(output)
        assert r.returncode in ((1,4) if refuse else (0,)), (label,r.returncode,output)
        if refuse: assert 'failed' in output.lower() and 'Metal replay wait begin' not in output, (label,output)
    def f(c,n): return next(x for x in c if x.get('name')==n)
    def index_upload(chunks,draw):
        sources={f(c,'Buffer').text for c in chunks if c.get('name')=='MTLHeap::newBuffer(offset)' and (int(f(c,'options').text)&0xf0)==0}
        return next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and f(c,'destinationBuffer').text==f(draw,'indexBuffer').text and f(c,'sourceBuffer').text in sources)
    run('positive-api',[opener,capture]); run('positive-cli',[cli,'replay','--loops','1',capture])
    xml=folder/'source.zip.xml'; run('export',[cli,'convert','-f',capture,'-o',xml,'-c','zip.xml']); original=ET.parse(xml)
    with zipfile.ZipFile(xml.with_suffix('')) as archive: blobs={n:archive.read(n) for n in archive.namelist()}
    coverage=int(f(next(c for c in original.find('./chunks') if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text)
    indexed = any(c.get('name') == 'MTLRenderCommandEncoder::drawIndexedPrimitives' for c in original.find('./chunks'))
    producers=[c for c in original.find('./chunks') if c.get('name')=='MTLBuffer::DescriptorSlotProducer']
    cases=['v8','missing-vertex-layout','missing-fragment-layout','missing-vertex-binding','missing-fragment-binding',
        'vertex-stage','fragment-stage','vertex-root-va','fragment-root-va','vertex-member','fragment-member',
        'vertex-size','fragment-size','missing-vertex-bytes','missing-fragment-bytes','missing-pipeline',
        'vertex-count','instance-count','vertex-start','base-instance','primitive','draw-before-pipeline',
        'bytes-before-layout','missing-render-end','duplicate-render-end','duplicate-render-birth','draw-after-end']
    if len(producers)>2:
        cases += ['legacy44-batch','producer-source-unknown','producer-source-offset','producer-destination-offset',
                  'missing-producer','duplicate-producer','missing-gpu-value','gpu-value-mismatch','gpu-binding-unknown',
                  'producer-before-dispatch','producer-after-end','producer-range-end']
    if len(producers)==256: cases += ['producer-count-limit']
    if indexed:
        cases += ['v13', 'index-type', 'index-buffer-zero', 'index-buffer-unknown', 'index-buffer-wrong-type',
            'index-offset-end', 'index-offset-overflow', 'index-base-vertex', 'index-values',
            'missing-draw-params-layout', 'missing-index-kind-layout', 'missing-draw-params', 'missing-index-kind',
            'draw-params-length', 'draw-params-count', 'draw-params-offset', 'draw-params-instances',
            'draw-params-base-vertex', 'draw-params-base-instance', 'index-kind-value', 'index-kind-length',
            'plain-layout-stage', 'plain-layout-index', 'plain-layout-count', 'plain-layout-stride',
            'root-marked-plain', 'params-before-layout', 'index-snapshot-change', 'index-creation-missing', 'index-gpu-writes']
        draw=next(c for c in original.find('./chunks') if c.get('name')=='MTLRenderCommandEncoder::drawIndexedPrimitives')
        private_index=any(c.get('name')=='MTLDevice::newBufferWithLength' and f(c,'Buffer').text==f(draw,'indexBuffer').text and (int(f(c,'options').text)&0xf0)==32 for c in original.find('./chunks'))
        frame_index=any(c.get('name')=='MTLHeap::newBuffer(offset)' and f(c,'Buffer').text==f(draw,'indexBuffer').text for c in original.find('./chunks'))
        if private_index or frame_index:
            cases.remove('index-snapshot-change')
            if private_index:cases+=['legacy47-private-index','private-index-initial-short']
        if frame_index:
            cases+=['legacy49-index-upload','missing-source-snapshot','snapshot-after-upload-commit','upload-source-unknown','upload-source-offset-end','upload-destination-offset-end','upload-size-overflow','source-snapshot-short','upload-after-draw','upload-after-source-write']
    late_upload=bool(frame_index) and list(original.find('./chunks')).index(index_upload(original.find('./chunks'),draw))>list(original.find('./chunks')).index(draw) if indexed else False
    if late_upload:cases.remove('upload-after-source-write')
    if coverage>=49:
        cases+=['legacy48-draw-work','draw-count-limit','draw-work-per-call','draw-work-frame-budget','base-instance-u32','base-vertex-i32','index-effective-negative']
    for case in cases:
        tree,data=copy.deepcopy(original),dict(blobs); chunks=tree.find('./chunks')
        vl=next(c for c in chunks if c.get('name')=='MTLCommandEncoder::DescriptorInlineLayout' and f(c,'stage').text=='1')
        fl=next(c for c in chunks if c.get('name')=='MTLCommandEncoder::DescriptorInlineLayout' and f(c,'stage').text=='2')
        vb=next(c for c in chunks if c.get('name')=='MTLCommandEncoder::DescriptorInlineBinding' and f(c,'stage').text=='1')
        fb=next(c for c in chunks if c.get('name')=='MTLCommandEncoder::DescriptorInlineBinding' and f(c,'stage').text=='2')
        vertex=next(c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::setVertexBytes')
        fragment=next(c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::setFragmentBytes')
        draw=next(c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::'+('drawIndexedPrimitives' if indexed else 'drawPrimitives'))
        pipeline=next(c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::setRenderPipelineState')
        end=next(c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::endEncoding')
        birth=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::renderCommandEncoderWithDescriptor')
        if indexed:
            pl=next(c for c in chunks if c.get('name')=='MTLCommandEncoder::DescriptorInlineLayout' and f(c,'stage').text=='1' and f(c,'index').text=='4')
            kl=next(c for c in chunks if c.get('name')=='MTLCommandEncoder::DescriptorInlineLayout' and f(c,'stage').text=='1' and f(c,'index').text=='5')
            pb=next(c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::setVertexBytes' and f(c,'index').text=='4')
            kb=next(c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::setVertexBytes' and f(c,'index').text=='5')
        def move(c,before,after=False): chunks.remove(c); chunks.insert(list(chunks).index(before)+int(after),c)
        def corrupt(c,short=False):
            node=f(c,'data')
            if short:
                for child in list(node)[8:]: node.remove(child)
            else:
                for child in list(node)[:8]: child.text='255'
        if case in ('legacy44-batch','producer-source-unknown','producer-source-offset','producer-destination-offset',
                    'missing-producer','duplicate-producer','missing-gpu-value','gpu-value-mismatch','gpu-binding-unknown',
                    'producer-before-dispatch','producer-after-end','producer-range-end','producer-count-limit'):
            producer=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotProducer')
            producer_list=[c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotProducer']
            gpu=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and f(c,'buffer').text==f(producer,'buffer').text and f(c,'event').text=='3')
            binding=next(c for c in list(chunks)[list(chunks).index(gpu)+1:] if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and f(c,'buffer').text==f(producer,'buffer').text)
            if case=='legacy44-batch':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='44'
            elif case=='producer-source-unknown':f(producer,'source').text='9999999'
            elif case=='producer-source-offset':f(producer,'sourceOffset').text='1'
            elif case=='producer-destination-offset':f(producer,'offset').text='1'
            elif case=='producer-range-end':f(producer,'offset').text=str(len(producer_list)*24)
            elif case=='missing-producer':chunks.remove(producer)
            elif case=='duplicate-producer':chunks.insert(list(chunks).index(producer),copy.deepcopy(producer))
            elif case=='producer-count-limit':
                # A complete repeat has valid source, GPU expected bytes and binding.
                # Place it after all 256 complete packets to isolate the frame count cap.
                end_producer=producer_list[-1]
                last_gpu=next(c for c in list(chunks)[list(chunks).index(end_producer)+1:] if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and f(c,'event').text=='3')
                last_binding=next(c for c in list(chunks)[list(chunks).index(last_gpu)+1:] if c.get('name')=='MTLBuffer::DescriptorSlotBinding')
                insertion=list(chunks).index(last_binding)+1
                for extra in (producer,gpu,binding):chunks.insert(insertion,copy.deepcopy(extra));insertion+=1
            elif case=='missing-gpu-value':chunks.remove(gpu)
            elif case=='gpu-value-mismatch':
                payload=bytearray(data[f'{int(f(gpu,"data").text):06d}']);payload[16]^=1;data[f'{int(f(gpu,"data").text):06d}']=bytes(payload)
            elif case=='gpu-binding-unknown':f(binding,'resource').text='9999999'
            elif case in ('producer-before-dispatch','producer-after-end'):
                point=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::'+('dispatchThreadgroups' if case=='producer-before-dispatch' else 'endEncoding') and f(c,'ComputeCommandEncoder').text==f(producer,'encoder').text)
                move(producer,point,case=='producer-after-end')
        elif case=='v8':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='8'
        elif case in ('missing-vertex-layout','missing-fragment-layout','missing-vertex-binding','missing-fragment-binding',
                      'missing-vertex-bytes','missing-fragment-bytes','missing-pipeline','missing-render-end'):
            chunks.remove({'missing-vertex-layout':vl,'missing-fragment-layout':fl,'missing-vertex-binding':vb,
                'missing-fragment-binding':fb,'missing-vertex-bytes':vertex,'missing-fragment-bytes':fragment,
                'missing-pipeline':pipeline,'missing-render-end':end}[case])
        elif case=='vertex-stage':f(vl,'stage').text='3'
        elif case=='fragment-stage':f(fl,'stage').text='4'
        elif case=='vertex-root-va':corrupt(vertex)
        elif case=='fragment-root-va':corrupt(fragment)
        elif case=='vertex-size':corrupt(vertex,True)
        elif case=='fragment-size':corrupt(fragment,True)
        elif case=='vertex-member':f(vb,'memberOffset').text='24'
        elif case=='fragment-member':f(fb,'memberOffset').text='24'
        elif case in ('vertex-count','instance-count','vertex-start','base-instance','primitive'):
            name,value={'vertex-count':('indexCount' if indexed else 'vertexCount','4'),'instance-count':('instanceCount','2'),
                'vertex-start':('indexBufferOffset' if indexed else 'vertexStart','1'),'base-instance':('baseInstance','1'),'primitive':('primitiveType','0')}[case]
            if coverage>=49 and case in ('vertex-count','instance-count'):value='0'
            f(draw,name).text=value
        elif case=='draw-before-pipeline':move(draw,pipeline)
        elif case=='bytes-before-layout':move(vertex,vl)
        elif case=='duplicate-render-end':chunks.insert(list(chunks).index(end),copy.deepcopy(end))
        elif case=='duplicate-render-birth':chunks.insert(list(chunks).index(birth),copy.deepcopy(birth))
        elif case=='draw-after-end':move(draw,end,True)
        elif case in ('legacy48-draw-work','draw-count-limit','draw-work-per-call','draw-work-frame-budget','base-instance-u32','base-vertex-i32','index-effective-negative'):
            def params_word(index,value):
                for i,byte in enumerate((value&0xffffffff).to_bytes(4,'little')):f(pb,'data')[index*4+i].text=str(byte)
            if case=='legacy48-draw-work':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='48'
            elif case=='draw-count-limit':
                all_draws=[c for c in chunks if c.get('name')==draw.get('name')]
                for _ in range(513-len(all_draws)):chunks.insert(list(chunks).index(end),copy.deepcopy(draw))
            elif case in ('draw-work-per-call','draw-work-frame-budget'):
                instances=65536//int(f(draw,'indexCount').text)+1 if case=='draw-work-per-call' else 1024
                params_word(1,instances)
                all_draws=[c for c in chunks if c.get('name')==draw.get('name')]
                for d in all_draws:f(d,'instanceCount').text=str(instances)
                if case=='draw-work-frame-budget':
                    for _ in range(512-len(all_draws)):chunks.insert(list(chunks).index(end),copy.deepcopy(draw))
            elif case=='base-instance-u32':f(draw,'baseInstance').text=str(2**32);params_word(4,0)
            elif case=='base-vertex-i32':f(draw,'baseVertex').text=str(2**31);params_word(3,2**31)
            else:f(draw,'baseVertex').text=str(-2**31);params_word(3,-2**31)
        elif case in ('legacy49-index-upload','missing-source-snapshot','snapshot-after-upload-commit','upload-source-unknown','upload-source-offset-end','upload-destination-offset-end','upload-size-overflow','source-snapshot-short','upload-after-draw','upload-after-source-write'):
            upload=index_upload(chunks,draw)
            snapshot=next(c for c in chunks if c.get('name')=='Internal_MTLBufferModifyCPUContents' and f(c,'Buffer').text==f(upload,'sourceBuffer').text)
            if case=='legacy49-index-upload':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='49'
            elif case=='missing-source-snapshot':chunks.remove(snapshot)
            elif case=='snapshot-after-upload-commit':
                commit=next(c for c in list(chunks)[list(chunks).index(snapshot)+1:] if c.get('name')=='MTLCommandBuffer::commit')
                move(snapshot,commit,True)
            elif case=='upload-source-unknown':f(upload,'sourceBuffer').text='999999'
            elif case=='upload-source-offset-end':f(upload,'sourceOffset').text='32'
            elif case=='upload-destination-offset-end':f(upload,'destinationOffset').text='256'
            elif case=='upload-size-overflow':f(upload,'size').text=str(2**64-1)
            elif case=='source-snapshot-short':
                node=f(snapshot,'data');name=f'{int(node.text):06d}';data[name]=data[name][:-1];node.set('byteLength',str(len(data[name])));f(snapshot,'size').text=str(len(data[name]))
            elif case=='upload-after-draw':move(upload,draw,True)
            else:
                write=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::setBuffer' and f(c,'index').text=='1')
                f(write,'buffer').text=f(upload,'sourceBuffer').text
                compute_end=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::endEncoding' and f(c,'ComputeCommandEncoder').text==f(write,'ComputeCommandEncoder').text)
                encoder=f(upload,'BlitCommandEncoder').text
                upload_birth=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::blitCommandEncoder' and f(c,'BlitCommandEncoder').text==encoder)
                upload_end=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::endEncoding' and f(c,'BlitCommandEncoder').text==encoder)
                block=list(chunks)[list(chunks).index(upload_birth):list(chunks).index(upload_end)+1]
                for c in block:chunks.remove(c)
                point=list(chunks).index(compute_end)+1
                for c in block:chunks.insert(point,c);point+=1
        elif case=='legacy47-private-index':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='47'
        elif case=='private-index-initial-short':
            initial=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and f(c,'id').text==f(draw,'indexBuffer').text)
            payload=f(initial,'Contents');name=f'{int(payload.text):06d}';data[name]=data[name][:-1];payload.set('byteLength',str(len(data[name])))
        elif case=='v13':f(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='13'
        elif case in ('index-type','index-buffer-zero','index-buffer-unknown','index-buffer-wrong-type','index-offset-end','index-offset-overflow','index-base-vertex'):
            name,value={'index-type':('indexType','2'),'index-buffer-zero':('indexBuffer','0'),
                'index-buffer-unknown':('indexBuffer','999999'),'index-buffer-wrong-type':('indexBuffer',f(pipeline,'pipelineState').text),
                'index-offset-end':('indexBufferOffset','32'),'index-offset-overflow':('indexBufferOffset',str(2**64-1)),
                'index-base-vertex':('baseVertex','1')}[case]
            f(draw,name).text=value
        elif case in ('index-values','index-snapshot-change'):
            index_id=f(draw,'indexBuffer').text
            if case=='index-values' and frame_index:
                upload=index_upload(chunks,draw)
                initial=next(c for c in chunks if c.get('name')=='Internal_MTLBufferModifyCPUContents' and f(c,'Buffer').text==f(upload,'sourceBuffer').text)
                member='data'
            elif case=='index-values':
                initial=next((c for c in chunks if c.get('name')=='Internal::Initial Contents' and f(c,'id').text==index_id),None)
                member='Contents'
                if initial is None:
                    initial=next(c for c in chunks if c.get('name')=='MTLDevice::newBufferWithBytes' and f(c,'Buffer').text==index_id)
                    member='initialData'
            else:
                initial=next(c for c in chunks if c.get('name')=='Internal_MTLBufferModifyCPUContents' and f(c,'Buffer').text==index_id)
                member='data'
            name=f'{int(f(initial,member).text):06d}';contents=bytearray(data[name])
            offset=int(f(draw,'indexBufferOffset').text);stride=2 if f(draw,'indexType').text=='0' else 4
            value=0 if coverage>=49 and int(f(draw,'baseVertex').text)<0 else (2**(stride*8)-1)
            contents[offset:offset+stride]=value.to_bytes(stride,'little');data[name]=bytes(contents)
        elif case.startswith('missing-draw-') or case.startswith('missing-index-'):
            chunks.remove({'missing-draw-params-layout':pl,'missing-index-kind-layout':kl,'missing-draw-params':pb,'missing-index-kind':kb}[case])
        elif case=='draw-params-length':f(pb,'data').remove(f(pb,'data')[-1])
        elif case.startswith('draw-params-'):
            offset={'draw-params-count':0,'draw-params-instances':4,'draw-params-offset':8,'draw-params-base-vertex':12,'draw-params-base-instance':16}[case]
            f(pb,'data')[offset].text='255'
        elif case=='index-kind-value':f(kb,'data')[0].text='3'
        elif case=='index-kind-length':f(kb,'data').remove(f(kb,'data')[-1])
        elif case.startswith('plain-layout-'):
            name,value={'plain-layout-stage':('stage','2'),'plain-layout-index':('index','0'),'plain-layout-count':('count','1'),'plain-layout-stride':('stride','24')}[case]
            f(pl,name).text=value
        elif case=='root-marked-plain':f(vl,'count').text='0'
        elif case=='params-before-layout':move(pb,pl)
        elif case=='index-creation-missing':
            if frame_index:
                chunks.remove(index_upload(chunks,draw))
            elif private_index:
                chunks.remove(next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and f(c,'id').text==f(draw,'indexBuffer').text))
            else:
                creation=next(c for c in chunks if c.get('name')=='MTLDevice::newBufferWithBytes' and f(c,'Buffer').text==f(draw,'indexBuffer').text)
                payload=f(creation,'initialData');data[f'{int(payload.text):06d}']=b'';payload.set('byteLength','0')
        elif case=='index-gpu-writes':
            binding=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::setBuffer' and f(c,'index').text=='1')
            f(binding,'buffer').text=f(draw,'indexBuffer').text
        for c in chunks: c.set('length','0')
        target=folder/(case+'.zip.xml'); tree.write(target,encoding='utf-8',xml_declaration=True)
        with zipfile.ZipFile(target.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
            for n,v in data.items():archive.writestr(n,v)
        rdc=folder/(case+'.rdc');run(case+'-convert',[cli,'convert','-f',target,'-o',rdc,'-c','rdc'])
        run(case+'-api',[opener,rdc],True);run(case+'-cli',[cli,'replay','--loops','1',rdc],True)
    print(f'PASS sourced graphics: {len(cases)} API+CLI negative stage/root/binding/draw/order groups')


if __name__=='__main__':main()
