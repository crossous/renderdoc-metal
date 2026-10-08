#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Damage typed descriptor-heap query captures; require rejection before GPU waits."""
import argparse, copy, fcntl, hashlib, json, os, re, signal, struct, subprocess, tempfile
from pathlib import Path
import xml.etree.ElementTree as ET
import zipfile

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True)
    p.add_argument('--work-dir',type=Path,required=True)
    p.add_argument('--oracle',type=Path,required=True)
    p.add_argument('--hit',type=int,default=1000)
    p.add_argument('--only-frame-geometry',action='store_true',
                   help='Exercise frozen placement BLAS inputs with coverage65 query consumers')
    p.add_argument('--encoder-template',type=Path,required=True,
                   help='Exported capture with blit fill and AS encoder chunks')
    a=p.parse_args();r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';w=a.work_dir.resolve();w.mkdir(parents=True,exist_ok=True)
    original=ET.parse(a.source)
    templates=ET.parse(a.encoder_template).find('./chunks')
    with zipfile.ZipFile(a.source.with_suffix('')) as z:blobs={n:z.read(n) for n in z.namelist()}
    f=lambda c,n:c.find('./*[@name="'+n+'"]')
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    m={'status':'RUNNING','backend_sha256':sha(b/'lib/librenderdoc.dylib'),
       'encoder_template_sha256':sha(a.encoder_template),'checks':[],'rejections':0,'legal_controls':0}
    def run(tag,argv,expected):
        e=os.environ.copy()
        for k in tuple(e):
            if k.startswith('RENDERDOC_') or k=='DYLD_INSERT_LIBRARIES':e.pop(k)
        e.update(MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
        with (w/(tag+'-renderdoc.log')).open('a') as keep,(w/(tag+'.log')).open('w') as log:
            fcntl.flock(keep,fcntl.LOCK_SH);child=subprocess.Popen(list(map(str,argv)),env=e,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            try:code=child.wait(timeout=45)
            except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait();code=124
        text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace')
        markers=['Assertion failed','failed assertion','OVERRUNNING CHUNK','File and decompress stream readers do not support seeking','Unexpected Metal resource type','m_ResourceMap.empty','m_ResourceRecords.empty']
        if expected:markers.append('Metal replay wait begin')
        hits=[x for x in markers if x in text];ok=code==expected and not hits
        m['checks'].append(dict(tag=tag,exit=code,passed=ok,diagnostic_hits=hits));(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n');print(tag,code,flush=True)
        if not ok:raise RuntimeError(tag+text[-1800:])
    cases=['missing-query','duplicate-query','frame-query','null-pipeline','wrong-pipeline','null-heap','null-output','heap-is-output','slot-align','slot-range',
        'missing-binding','plain-kind0','texture-kind1','null-header-source','wrong-header-source','member-align','member-end','duplicate-binding','wrong-slot-type',
        'missing-header','duplicate-header','frame-header','null-AS','AS-is-buffer','null-contribution','contribution-is-header','header-offset-align',
        'contribution-offset-align','contribution-offset-end','snapshot-AS','snapshot-VA','snapshot-reserved','snapshot-short',
        'root-pointer','root-slot','root-reserved','root-short','inline-missing','inline-source','inline-offset','heap-binding-offset','pipeline-unbound',
        'header-GPU-before','header-GPU-after','contribution-GPU-before','contribution-GPU-after',
        'AS-encoder-unended','pipeline-untyped-consumer','child-AS-as-TLAS','missing-AS-recipe',
        'legal-original']
    chunks=list(original.find('./chunks'))
    header_decl=next(c for c in chunks if c.get('name')=='MTLBuffer::DeclareRayASHeader')
    frame_header=chunks.index(header_decl)>next(i for i,c in enumerate(chunks) if c.get('id')=='5')
    m['frame_header']=frame_header
    if frame_header:
        cases=[tag for tag in cases if tag not in ['frame-header','child-AS-as-TLAS','missing-AS-recipe']]
        cases[-1:-1]=['header-before-birth','header-after-binding','header-missing-birth','header-large-backing',
                      'header-private-backing','header-missing-identity','missing-AS-build','build-AS-is-buffer',
                      'build-child-is-buffer','build-missing-child','build-source-is-output','build-count65',
                      'build-stride','build-scratch-is-header','build-scratch-offset-align','build-snapshot-NaN',
                      'build-snapshot-child-null','build-snapshot-options','build-unsubmitted','build-after-query']
    if frame_header and any(c.get('name')=='Internal::Initial Contents' and f(c,'kind') is not None and
                            f(c,'id').text==f(header_decl,'structure').text for c in chunks):
        # Omitting a redundant frame rebuild can legally query the initial AS.
        cases.remove('missing-AS-build')
    builds0=[c for c in chunks if c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances']
    if builds0:
        source0=f(builds0[0],'instances').text
        if any(c.get('name')=='MTLHeap::newBuffer(offset)' and f(c,'Buffer').text==source0 for c in chunks):
            cases[-1:-1]=['placement-input-untracked','placement-input-shared','legal-placement-input-heap-untracked',
                          'placement-input-heap-automatic','placement-input-range','legal-placement-scratch-heap-untracked']
    heap_geometry=[c for c in chunks if c.get('name')=='Internal::Initial Contents' and f(c,'kind') is not None and
        f(c,'kind').text in ['1','2','8'] and any(a.get('name')=='MTLHeap::newBuffer(offset)' and
        f(a,'Buffer').text==f(c,'source').text for a in chunks)]
    if heap_geometry:
        cases[-1:-1]=['geometry-source-is-header',('geometry-source-is-scratch' if f(heap_geometry[0],'kind').text=='8' else 'legal-geometry-source-is-scratch'),'geometry-source-range',
                      'geometry-frozen-short','geometry-count-zero','geometry-vertex-format','geometry-stride',
                      'geometry-table-range','geometry-opaque','geometry-usage','geometry-capacity-zero',
                      'legal-geometry-source-heap-untracked']
        if f(heap_geometry[0],'kind').text in ['2','8']:
            cases[-1:-1]=['geometry-index-is-header','geometry-index-offset','geometry-index-type',
                          'geometry-index-outside-vertices','geometry-index-short']
        if f(heap_geometry[0],'kind').text=='8':
            cases[-1:-1]=['geometry-count-one','geometry-last-format','geometry-last-range','geometry-descriptors-short']
    if any(c.get('name')=='Internal::Initial Contents' and f(c,'kind') is not None and f(c,'kind').text=='9'
           for c in original.find('./chunks')):
        cases[-1:-1]=['recipe-count-zero','recipe-count-65','recipe-offset','recipe-stride','recipe-type',
                      'recipe-reserved','recipe-missing-child','recipe-wrong-child','recipe-kind',
                      'frozen-child-null','frozen-child-unknown','frozen-transform-NaN','frozen-options']
    if any(c.get('name')=='Internal::Initial Contents' and f(c,'kind') is not None and f(c,'kind').text=='10'
           for c in original.find('./chunks')):
        cases[-1:-1]=['empty-count','empty-type','empty-reserved','empty-offset','empty-stride','empty-child']
    if a.only_frame_geometry:
        geometry=next(c for c in chunks if c.get('name') in [
            'MTLAccelerationStructureCommandEncoder::buildFrozenTriangles',
            'MTLAccelerationStructureCommandEncoder::buildFrozenMultiIndexed'])
        cases=['frame-geometry-kind','frame-geometry-target','frame-geometry-source',
               'frame-geometry-offset','frame-geometry-stride','frame-geometry-format',
               'frame-geometry-count','frame-geometry-opaque','frame-geometry-usage',
               'frame-geometry-short','frame-geometry-scratch','frame-geometry-scratch-offset',
               'legal-original']
        if f(geometry,'kind').text in ['2','8']:
            cases[-1:-1]=['frame-geometry-index-source','frame-geometry-index-offset',
                          'frame-geometry-index-type','frame-geometry-index-short',
                          'frame-geometry-index-outside']
        if f(geometry,'kind').text=='8':
            cases[-1:-1]=['frame-geometry-last-format','frame-geometry-last-range',
                          'frame-geometry-array-short']
    try:
        for tag in cases:
            tree=copy.deepcopy(original);ch=tree.find('./chunks');data=dict(blobs)
            d=next(c for c in ch if c.get('name')=='MTLComputePipelineState::DeclareRayQueryHeapDispatch')
            h=next(c for c in ch if c.get('name')=='MTLBuffer::DeclareRayASHeader')
            binding=next(c for c in ch if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and f(c,'kind').text=='3')
            root=next(c for c in ch if c.get('name')=='MTLComputeCommandEncoder::setBytes')
            inline=next(c for c in ch if c.get('name')=='MTLCommandEncoder::DescriptorInlineBinding')
            scope=next(c for c in ch if c.get('id')=='5');header=f(h,'buffer').text
            if tag.startswith('frame-geometry-'):
                g=next(c for c in ch if c.get('name') in [
                    'MTLAccelerationStructureCommandEncoder::buildFrozenTriangles',
                    'MTLAccelerationStructureCommandEncoder::buildFrozenMultiIndexed'])
                params=f(g,'parameters')
                values={'frame-geometry-offset':(0,256),'frame-geometry-stride':(1,1),
                        'frame-geometry-format':(2,0),'frame-geometry-count':(3,0),
                        'frame-geometry-opaque':(5,2),'frame-geometry-usage':(7,2),
                        'frame-geometry-index-offset':(8,1048576),'frame-geometry-index-type':(9,2),
                        'frame-geometry-last-format':(len(params)-8,0),
                        'frame-geometry-last-range':(len(params)-10,256)}
                if tag in values:i,v=values[tag];params[i].text=str(v)
                elif tag=='frame-geometry-kind':f(g,'kind').text='123'
                elif tag in ['frame-geometry-target','frame-geometry-source','frame-geometry-index-source','frame-geometry-scratch']:
                    f(g,{'frame-geometry-target':'structure','frame-geometry-source':'vertices',
                         'frame-geometry-index-source':'indices','frame-geometry-scratch':'scratch'}[tag]).text=header
                elif tag=='frame-geometry-scratch-offset':f(g,'scratchOffset').text='1'
                elif tag=='frame-geometry-array-short':params.remove(params[-1])
                else:
                    value=f(g,'indexBytes' if tag.startswith('frame-geometry-index-') else 'vertexBytes')
                    key=f'{int(value.text):06}';raw=bytearray(data[key])
                    if tag.endswith('-short'):
                        raw=raw[:-1];value.set('byteLength',str(len(raw)))
                    elif tag=='frame-geometry-index-outside':
                        at=int(params[-2].text) if f(g,'kind').text=='8' else 0
                        struct.pack_into('<H' if params[-1].text=='0' else '<I',raw,at,65535)
                    else:raise RuntimeError(tag)
                    data[key]=bytes(raw)
            elif tag=='missing-query':ch.remove(d)
            elif tag=='duplicate-query':ch.insert(list(ch).index(d),copy.deepcopy(d))
            elif tag=='frame-query':ch.remove(d);ch.insert(list(ch).index(scope)+1,d)
            elif tag.startswith('null-') and tag[5:] in ['pipeline','heap','output']:f(d,tag[5:]).text='0'
            elif tag=='wrong-pipeline':f(d,'pipeline').text=header
            elif tag=='heap-is-output':f(d,'output').text=f(d,'heap').text
            elif tag=='slot-align':f(d,'slotOffset').text='8'
            elif tag=='slot-range':f(d,'slotOffset').text=str(24*1000)
            elif tag=='missing-binding':ch.remove(binding)
            elif tag in ['plain-kind0','texture-kind1']:f(binding,'kind').text='0' if tag=='plain-kind0' else '1'
            elif tag=='null-header-source':f(binding,'resource').text='0'
            elif tag=='wrong-header-source':f(binding,'resource').text=f(h,'contributions').text
            elif tag=='member-align':f(binding,'memberOffset').text=str(int(f(binding,'memberOffset').text)+1)
            elif tag=='member-end':f(binding,'memberOffset').text='16384'
            elif tag=='duplicate-binding':ch.insert(list(ch).index(binding),copy.deepcopy(binding))
            elif tag=='wrong-slot-type':
                for c in ch:
                    if c.get('name')=='MTLBuffer::DescriptorSlotEvent':f(c,'descriptorType').text='0'
            elif tag=='missing-header':ch.remove(h)
            elif tag=='duplicate-header':ch.insert(list(ch).index(h),copy.deepcopy(h))
            elif tag=='frame-header':ch.remove(h);ch.insert(list(ch).index(scope)+1,h)
            elif tag=='null-AS':f(h,'structure').text='0'
            elif tag=='AS-is-buffer':f(h,'structure').text=header
            elif tag=='null-contribution':f(h,'contributions').text='0'
            elif tag=='contribution-is-header':f(h,'contributions').text=header
            elif tag=='header-offset-align':f(h,'offset').text=str(int(f(h,'offset').text)+1)
            elif tag=='contribution-offset-align':f(h,'contributionOffset').text=str(int(f(h,'contributionOffset').text)+1)
            elif tag=='contribution-offset-end':f(h,'contributionOffset').text='16384'
            elif tag.startswith('snapshot-'):
                value=f(h,'bytes');name=f'{int(value.text):06}';raw=bytearray(data[name])
                if tag=='snapshot-short':raw=raw[:56];value.set('byteLength','56')
                else:struct.pack_into('<Q',raw,{'snapshot-AS':0,'snapshot-VA':8,'snapshot-reserved':16}[tag],1)
                data[name]=bytes(raw)
            elif tag.startswith('root-'):
                raw=f(root,'data')
                if tag=='root-short':raw.remove(raw[-1])
                else:raw[{'root-pointer':0,'root-slot':8,'root-reserved':12}[tag]].text='2'
            elif tag=='inline-missing':ch.remove(inline)
            elif tag=='inline-source':f(inline,'resource').text=header
            elif tag=='inline-offset':f(inline,'memberOffset').text='4'
            elif tag=='heap-binding-offset':
                c=next(c for c in ch if c.get('name')=='MTLComputeCommandEncoder::setBuffer' and f(c,'index').text=='0');f(c,'offset').text='24'
            elif tag=='pipeline-unbound':
                c=next(c for c in ch if c.get('name')=='MTLComputeCommandEncoder::setComputePipelineState');f(c,'pipeline').text='0'
            elif '-GPU-' in tag or tag=='AS-encoder-unended':
                compute=next(c for c in ch if c.get('name')=='MTLCommandBuffer::computeCommandEncoder')
                command=f(compute,'CommandBuffer').text
                names=(['MTLCommandBuffer::accelerationStructureCommandEncoder',
                         ] if tag=='AS-encoder-unended' else
                       ['MTLCommandBuffer::blitCommandEncoder','MTLBlitCommandEncoder::fillBuffer',
                        'MTLBlitCommandEncoder::endEncoding'])
                added=[copy.deepcopy(next(c for c in templates if c.get('name')==name)) for name in names]
                for c in added:
                    for name in ('BlitCommandEncoder','Encoder'):
                        if f(c,name) is not None:f(c,name).text='900001'
                    if f(c,'CommandBuffer') is not None:f(c,'CommandBuffer').text=command
                    if c.get('name')=='MTLBlitCommandEncoder::fillBuffer':
                        is_header=tag.startswith('header-')
                        f(c,'buffer').text=header if is_header else f(h,'contributions').text
                        location=int(f(h,'offset' if is_header else 'contributionOffset').text)
                        f(f(c,'range'),'location').text=str(location+16 if is_header else location)
                        f(f(c,'range'),'length').text='4';f(c,'value').text='9'
                where=list(ch).index(compute)
                if tag.endswith('-after'):
                    end=next(c for c in ch if c.get('name')=='MTLComputeCommandEncoder::endEncoding')
                    where=list(ch).index(end)+1
                for i,c in enumerate(added):ch.insert(where+i,c)
            elif tag=='pipeline-untyped-consumer':
                create=next(c for c in ch if c.get('name')=='MTLDevice::newComputePipelineStateWithFunction')
                extra=copy.deepcopy(create);f(extra,'ComputePipelineState').text='900002'
                ch.insert(list(ch).index(create)+1,extra)
                c=next(c for c in ch if c.get('name')=='MTLComputeCommandEncoder::setComputePipelineState')
                f(c,'pipeline').text='900002'
            elif tag.startswith('header-') and tag not in ['header-offset-align','header-GPU-before','header-GPU-after']:
                create=next(c for c in ch if c.get('name') in ['MTLHeap::newBuffer(offset)','MTLDevice::newBufferWithLength'] and f(c,'Buffer').text==header)
                if tag=='header-before-birth':ch.remove(h);ch.insert(list(ch).index(create),h)
                elif tag=='header-after-binding':ch.remove(h);ch.insert(list(ch).index(binding)+1,h)
                elif tag=='header-missing-birth':ch.remove(create)
                elif tag=='header-large-backing':f(create,'length').text='16400'
                elif tag=='header-private-backing':f(create,'options').text='32'
                elif tag=='header-missing-identity':
                    identity=next(c for c in ch if c.get('name')=='MTLResource::CaptureGPUIdentity' and f(c,'resource').text==header);ch.remove(identity)
                else:raise RuntimeError(tag)
            elif tag.startswith(('geometry-','legal-geometry-')):
                geom=next(c for c in ch if c.get('name')=='Internal::Initial Contents' and f(c,'kind') is not None and
                    f(c,'kind').text in ['1','2','8'] and any(a.get('name')=='MTLHeap::newBuffer(offset)' and
                    f(a,'Buffer').text==f(c,'source').text for a in ch))
                values=f(geom,'parameters');kind=int(f(geom,'kind').text)
                if tag=='geometry-source-is-header':f(geom,'source').text=header
                elif tag in ['geometry-source-is-scratch','legal-geometry-source-is-scratch']:
                    build=next(c for c in ch if c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances');f(geom,'source').text=f(build,'scratch').text
                elif tag=='geometry-source-range':values[0].text='256'
                elif tag=='geometry-frozen-short' or tag=='geometry-index-short':
                    field=f(geom,'vertices' if tag=='geometry-frozen-short' else 'indices');key=f'{int(field.text):06}';data[key]=data[key][:-1];field.set('byteLength',str(len(data[key])))
                elif tag=='geometry-capacity-zero':
                    create=next(c for c in ch if c.get('name') in ['MTLDevice::newAccelerationStructureWithSize','MTLHeap::newAccelerationStructure'] and f(c,'Structure').text==f(geom,'id').text);f(create,'size').text='0'
                elif tag=='legal-geometry-source-heap-untracked':
                    allocation=next(c for c in ch if c.get('name')=='MTLHeap::newBuffer(offset)' and f(c,'Buffer').text==f(geom,'source').text)
                    heap=next(c for c in ch if c.get('name')=='MTLDevice::newHeapWithDescriptor' and f(c,'Heap').text==f(allocation,'Heap').text);f(heap,'hazardMode').text='1'
                elif tag=='geometry-index-is-header':f(geom,'indexSource').text=header
                elif tag=='geometry-index-outside-vertices':
                    field=f(geom,'indices');key=f'{int(field.text):06}';raw=bytearray(data[key]);offset=int(values[-2].text) if kind==8 else int(values[8].text);typ=int(values[-1].text) if kind==8 else int(values[9].text);struct.pack_into('<H' if typ==0 else '<I',raw,offset,65535);data[key]=bytes(raw)
                elif tag=='geometry-count-one':
                    for node in list(values)[10:]:values.remove(node)
                elif tag=='geometry-descriptors-short':values.remove(values[-1])
                else:
                    edits={'geometry-count-zero':(3,0),'geometry-vertex-format':(2,0),'geometry-stride':(1,1),
                        'geometry-table-range':(4,32),'geometry-opaque':(5,2),'geometry-usage':(7,2),
                        'geometry-index-offset':(8,1048576),'geometry-index-type':(9,2),
                        'geometry-last-format':(len(values)-8,0),'geometry-last-range':(len(values)-10,256)}
                    pos,value=edits[tag];values[pos].text=str(value)
            elif tag.startswith(('placement-','legal-placement-')):
                build=next(c for c in ch if c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances')
                resource=f(build,'scratch' if 'scratch' in tag else 'instances').text
                allocation=next(c for c in ch if c.get('name')=='MTLHeap::newBuffer(offset)' and f(c,'Buffer').text==resource)
                heap=next(c for c in ch if c.get('name')=='MTLDevice::newHeapWithDescriptor' and f(c,'Heap').text==f(allocation,'Heap').text)
                placementTag=tag.removeprefix('legal-')
                if placementTag=='placement-input-untracked':f(allocation,'options').text='288'
                elif placementTag=='placement-input-shared':f(allocation,'options').text='512'
                elif 'heap-untracked' in tag:f(heap,'hazardMode').text='1'
                elif placementTag=='placement-input-heap-automatic':f(heap,'type').text='0'
                elif placementTag=='placement-input-range':f(allocation,'offset').text=str(int(f(heap,'size').text)+256)
                else:raise RuntimeError(tag)
            elif tag=='missing-AS-build' or tag.startswith('build-'):
                build=next(c for c in ch if c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances')
                if tag=='missing-AS-build':ch.remove(build)
                elif tag=='build-AS-is-buffer':f(build,'structure').text=header
                elif tag=='build-child-is-buffer':f(build,'children')[0].text=header
                elif tag=='build-missing-child':f(build,'children').clear()
                elif tag=='build-source-is-output':f(build,'instances').text=f(d,'output').text
                elif tag=='build-count65':f(build,'parameters')[3].text='65'
                elif tag=='build-stride':f(build,'parameters')[1].text='71'
                elif tag=='build-scratch-is-header':f(build,'scratch').text=header
                elif tag=='build-scratch-offset-align':
                    build.set('name','MTLAccelerationStructureCommandEncoder::buildIndirectInstancesWithScratchOffset')
                    # Derive the appended variant ID from the same enum as the backend.
                    names=re.findall(r'^  (MTL\w+)',(r/'renderdoc/driver/metal/metal_common.h').read_text(),re.M)
                    delta=names.index('MTLAccelerationStructureCommandEncoder_buildIndirectInstancesWithScratchOffset')-names.index('MTLAccelerationStructureCommandEncoder_buildIndirectInstances')
                    build.set('id',str(int(build.get('id'))+delta));ET.SubElement(build,'uint',name='scratchOffset',typename='uint64_t',width='8').text='1'
                elif tag.startswith('build-snapshot-'):
                    value=f(build,'descriptorBytes');key=f'{int(value.text):06}';raw=bytearray(data[key])
                    if tag=='build-snapshot-NaN':struct.pack_into('<I',raw,0,0x7fc00000)
                    elif tag=='build-snapshot-child-null':struct.pack_into('<Q',raw,64,0)
                    else:struct.pack_into('<I',raw,48,2**32-1)
                    data[key]=bytes(raw)
                else:
                    encoder=f(build,'Encoder').text
                    create=next(c for c in ch if c.get('name')=='MTLCommandBuffer::accelerationStructureCommandEncoder' and f(c,'Encoder').text==encoder)
                    command=f(create,'CommandBuffer').text
                    commit=next(c for c in ch if c.get('name')=='MTLCommandBuffer::commit' and f(c,'CommandBuffer').text==command)
                    wait=next(c for c in ch if c.get('name')=='MTLCommandBuffer::waitUntilCompleted' and f(c,'CommandBuffer').text==command)
                    if tag=='build-unsubmitted':ch.remove(commit);ch.remove(wait)
                    else:
                        assert tag=='build-after-query'
                        creation=next(c for c in ch if c.get('name').startswith('MTLCommandQueue::commandBuffer') and f(c,'CommandBuffer').text==command)
                        moved=list(ch)[list(ch).index(creation):list(ch).index(wait)+1]
                        for c in moved:ch.remove(c)
                        dispatch=next(c for c in ch if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups')
                        where=list(ch).index(dispatch)+1
                        for i,c in enumerate(moved):ch.insert(where+i,c)
            elif tag in ('child-AS-as-TLAS','missing-AS-recipe'):
                recipe=next(c for c in ch if c.get('name')=='Internal::Initial Contents' and
                            f(c,'kind') is not None and f(c,'id').text==f(h,'structure').text)
                if tag=='missing-AS-recipe':ch.remove(recipe)
                else:
                    child=next(f(c,'id').text for c in ch if c.get('name')=='Internal::Initial Contents' and
                               f(c,'kind') is not None and f(c,'kind').text=='1')
                    child_id=next(f(c,'value').text for c in ch
                                  if c.get('name')=='MTLAccelerationStructure::CaptureGPUIdentity' and
                                  f(c,'resource').text==child)
                    f(h,'structure').text=child
                    snapshot_name=f'{int(f(h,"bytes").text):06}'
                    snapshot=data[snapshot_name];patched=struct.pack('<Q',int(child_id))+snapshot[8:]
                    # Keep captured raw copies coherent so the rejection checks AS type.
                    for name,raw in tuple(data.items()):
                        if snapshot in raw:data[name]=raw.replace(snapshot,patched)
            elif tag.startswith(('recipe-','frozen-','empty-')):
                recipe=next(c for c in ch if c.get('name')=='Internal::Initial Contents' and
                            f(c,'kind') is not None and f(c,'kind').text==('10' if tag.startswith('empty-') else '9'))
                values={'recipe-count-zero':(3,0),'recipe-count-65':(3,65),'recipe-offset':(0,1),
                        'recipe-stride':(1,71),'recipe-type':(2,0),'recipe-reserved':(4,1),
                        'empty-count':(3,1),'empty-type':(2,0),'empty-reserved':(4,1),
                        'empty-offset':(0,1),'empty-stride':(1,71)}
                if tag in values:index,value=values[tag];f(recipe,'parameters')[index].text=str(value)
                elif tag=='recipe-missing-child':f(recipe,'children').clear()
                elif tag=='recipe-wrong-child':f(recipe,'children')[0].text=header
                elif tag=='empty-child':ET.SubElement(f(recipe,'children'),'ResourceId',typename='ResourceId',width='8').text=header
                elif tag=='recipe-kind':f(recipe,'kind').text='123'
                else:
                    key=f'{int(f(recipe,"vertices").text):06}';raw=bytearray(data[key])
                    if tag.startswith('frozen-child-'):struct.pack_into('<Q',raw,64,0 if tag=='frozen-child-null' else 99999999)
                    elif tag=='frozen-transform-NaN':struct.pack_into('<I',raw,0,0x7fc00000)
                    elif tag=='frozen-options':struct.pack_into('<I',raw,48,2**32-1)
                    else:raise RuntimeError(tag)
                    data[key]=bytes(raw)
            else:assert tag=='legal-original'
            xml=w/(tag+'.zip.xml');tree.write(xml,encoding='utf-8',xml_declaration=True)
            with zipfile.ZipFile(xml.with_suffix(''),'w',zipfile.ZIP_DEFLATED) as z:
                for name,raw in data.items():z.writestr(name,raw)
            cap=w/(tag+'.rdc');run(tag+'-import',[b/'bin/renderdoccmd','convert','-f',xml,'-o',cap,'-c','rdc'],0)
            if tag.startswith('legal-'):
                run(tag+'-API',[a.oracle,cap,a.hit],0);run(tag+'-CLI',[b/'bin/renderdoccmd','replay','--loops','3',cap],0);m['legal_controls']+=1
            else:
                run(tag+'-API',[b/'metal-ray-b534/final-short/opener',cap],4);run(tag+'-CLI',[b/'bin/renderdoccmd','replay','--loops','1',cap],1);m['rejections']+=2
        assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256'];m['status']='PASS'
    except Exception as e:m.update(status='FAIL',error=str(e));raise
    finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')

if __name__=='__main__':
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX);main()
