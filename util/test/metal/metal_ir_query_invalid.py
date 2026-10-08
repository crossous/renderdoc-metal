#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reject damaged query declarations/roots/header/grid before replay submissions."""
import argparse
import copy
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True,help='Exported query.zip.xml')
    parser.add_argument('--work-dir',type=Path,required=True)
    parser.add_argument('--oracle',type=Path,help='Validate legal frame controls with the query event oracle')
    parser.add_argument('--mode',default='indirect-private-frame')
    parser.add_argument('--instances',type=int,choices=(1,64),default=1)
    parser.add_argument('--only-legal-controls',action='store_true')
    args=parser.parse_args();r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug'
    w=args.work_dir.resolve();w.mkdir(parents=True,exist_ok=True);tree=ET.parse(args.source)
    with zipfile.ZipFile(args.source.with_suffix('')) as archive:original={name:archive.read(name) for name in archive.namelist()}
    field=lambda node,key:node.find('./*[@name="'+key+'"]')
    m=dict(status='RUNNING',checks=[],rejections=0,legal_controls=0,backend_sha256=hashlib.sha256((b/'lib/librenderdoc.dylib').read_bytes()).hexdigest())
    env=os.environ.copy()
    for key in tuple(env):
        if key.startswith('RENDERDOC_METAL_') or key=='DYLD_INSERT_LIBRARIES':env.pop(key)
    env.update(MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1')
    def save():(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    def run(tag,command,expected):
        e=dict(env,RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
        with (w/(tag+'-renderdoc.log')).open('a') as keep,(w/(tag+'.log')).open('w') as log:
            fcntl.flock(keep,fcntl.LOCK_SH);p=subprocess.Popen(list(map(str,command)),env=e,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            try:code=p.wait(timeout=45)
            except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();code=124
        text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace')
        markers=['OVERRUNNING CHUNK','Assertion failed','failed assertion','File and decompress stream readers do not support seeking','Unexpected Metal resource type','m_ResourceMap.empty']
        if expected!=0:markers.append('Metal replay wait begin')
        hits=[x for x in markers if x in text]
        ok=code==expected and not hits;m['checks'].append(dict(tag=tag,exit=code,passed=ok,diagnostic_hits=hits));save();print(tag,code,flush=True)
        if not ok:raise RuntimeError(tag+text[-2000:])
    cases=[*('null-'+key for key in ['pipeline','roots','header','output']), 'duplicate','frame-declaration','missing-query','wrong-coverage',
        'offset-align','offset-range','header-is-output','roots-is-header','output-is-roots','wrong-output','wrong-pipeline',
        'missing-root-layout','missing-AS-layout','wrong-AS-schema','function-table-schema','root-count','root-stride',
        'root-header-null','root-output-null','root-swap','AS-null','AS-unknown','AS-primitive','header-reserved','contribution-unknown',
        'groups-zero','groups-limit','groups-y','groups-overflow','threads-zero','threads-limit','threads-z','output-overrun','wrong-slot','missing-binding']
    if any(c.get('name')=='Internal::Initial Contents' and field(c,'kind') is not None and field(c,'kind').text=='9'
           for c in tree.find('./chunks')):
        cases+=['recipe-count-zero','recipe-count-65','recipe-offset','recipe-stride','recipe-type','recipe-reserved',
            'recipe-missing-child','recipe-wrong-child','recipe-kind','recipe-empty-kind','recipe-masked-kind',
            'frozen-child-null','frozen-child-unknown','frozen-transform-NaN','frozen-options','contribution-unaligned']
    initial_kinds={int(field(c,'kind').text) for c in tree.find('./chunks')
        if c.get('name')=='Internal::Initial Contents' and field(c,'kind') is not None}
    if 10 in initial_kinds:cases+=['empty-count','empty-type','empty-reserved','empty-child','empty-offset','empty-stride']
    if 11 in initial_kinds:cases+=['masked-count-zero','masked-count-65','masked-type','masked-reserved','masked-child',
        'masked-kind-empty','masked-active-mask','masked-nonnull-ID','masked-transform-NaN','masked-options']
    if any(c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances' for c in tree.find('./chunks')):
        cases+=['frame-encoder','frame-target-null','frame-target-unknown','frame-target-BLAS','frame-input-unknown',
            'frame-input-Shared','frame-input-TLAS','frame-scratch-unknown','frame-scratch-Shared',
            'frame-offset','frame-stride','frame-type','frame-count-zero','frame-count-65','frame-reserved',
            'frame-child-missing','frame-child-duplicate','frame-child-TLAS','frame-child-unknown',
            'frame-ID-null','frame-ID-unknown','frame-ID-TLAS','frame-transform-NaN','frame-options','frame-truncated',
            'frame-live-encoder','frame-late-commit','frame-unknown-queue','frame-fill-header','frame-fill-roots']
        if args.oracle:cases+=['legal-frame-user75','legal-frame-child-remap']
    if 'new-target' in args.mode:
        cases+=['new-target-missing-build','new-target-wrong-built-target','new-target-before-build']
    geometry_names=('MTLAccelerationStructureCommandEncoder::buildFrozenTriangles','MTLAccelerationStructureCommandEncoder::buildFrozenMultiIndexed')
    geometry=next((c for c in tree.find('./chunks') if c.get('name') in geometry_names),None)
    if geometry is not None:
        cases+=['geometry-encoder','geometry-target-null','geometry-target-unknown','geometry-target-TLAS',
            'geometry-vertices-unknown','geometry-vertices-Shared','geometry-vertices-scratch','geometry-scratch-null',
            'geometry-scratch-unknown','geometry-scratch-vertices','geometry-kind','geometry-kind-TLAS','geometry-family',
            'geometry-offset','geometry-offset-end','geometry-offset-overflow','geometry-stride','geometry-format','geometry-count-zero',
            'geometry-count-over','geometry-IFT-over','geometry-opaque-over','geometry-duplicate-over','geometry-refit','geometry-usage-over',
            'geometry-scratch-offset-align','geometry-scratch-offset-overflow','geometry-vertices-truncated','geometry-NaN']
        if int(field(geometry,'indices').text):cases+=['geometry-indices-null','geometry-indices-unknown','geometry-indices-Shared',
            'geometry-index-offset','geometry-index-type','geometry-indices-truncated','geometry-index-outside']
        if geometry.get('name')==geometry_names[1]:
            cases+=['geometry-last-offset','geometry-last-stride','geometry-last-format','geometry-last-count','geometry-last-index-offset',
                'geometry-last-index-type','geometry-last-NaN','geometry-UInt16-outside','geometry-one-descriptor','geometry-short-array','geometry-over-array']
            if args.oracle:cases+=['legal-geometry-reversed-order']
    declarations=[c for c in tree.find('./chunks') if c.get('name')=='MTLComputePipelineState::DeclareRayQueryDispatch']
    headerID=field(declarations[0],'header').text
    headerOffset=int(next(field(c,'offset').text for c in tree.find('./chunks') if c.get('name')=='MTLBuffer::DeclareDescriptorTable' and field(c,'buffer').text==headerID and field(c,'schema').text=='4'))
    if headerOffset:
        cases+=['header-range-before','header-range-align','header-range-end','header-range-overflow',
            'header-range-other-buffer','header-range-AS-layout','header-range-VA-layout','header-range-large-backing']
    if any(c.get('name')=='MTLBuffer::DeclareRayASHeader' for c in tree.find('./chunks')):
        cases += ['typed-null-buffer','typed-null-AS','typed-null-contribution','typed-AS-is-buffer',
                  'typed-AS-is-BLAS','typed-contribution-is-header','typed-contribution-is-output',
                  'typed-offset-align','typed-offset-end','typed-offset-overflow','typed-contribution-align',
                  'typed-contribution-end','typed-snapshot-short','typed-snapshot-AS','typed-snapshot-VA',
                  'typed-snapshot-reserved','typed-duplicate','typed-frame','typed-without-query','typed-coverage']
    dynamic_headers=[c for c in tree.find('./chunks') if c.get('name')=='MTLBuffer::DeclareRayASHeader']
    if len(dynamic_headers)>1 and field(dynamic_headers[0],'buffer').text==field(dynamic_headers[-1],'buffer').text:
        if not args.oracle:
            parser.error('dynamic header controls require --oracle, --mode and --instances matching the capture')
        cases+=['header-write-missing','header-write-first','header-write-last','header-write-AS-type',
                'header-write-contribution-type','header-write-offset','header-write-snapshot-AS',
                'header-write-snapshot-VA','header-write-snapshot-reserved','header-write-alias',
                'header-write-no-first-commit','header-write-GPU-contribution','legal-header-write-repeat']
    if args.only_legal_controls:
        assert args.oracle
        cases=[tag for tag in cases if tag.startswith('legal-')]
        assert cases
    try:
        for tag in cases:
            variant=copy.deepcopy(tree);chunks=variant.find('./chunks');blobs=dict(original)
            declaration=next(c for c in chunks if c.get('name')=='MTLComputePipelineState::DeclareRayQueryDispatch')
            roots=field(declaration,'roots').text;header=field(declaration,'header').text;output=field(declaration,'output').text
            rootOffset=int(field(declaration,'offset').text)
            dispatch=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups')
            layouts=[c for c in chunks if c.get('name')=='MTLBuffer::DeclareDescriptorTable']
            rootLayout=next(c for c in layouts if field(c,'buffer').text==roots)
            ASLayout=next(c for c in layouts if field(c,'buffer').text==header and field(c,'schema').text=='4')
            identities={field(c,'resource').text:int(field(c,'value').text) for c in chunks if c.get('name')=='MTLAccelerationStructure::CaptureGPUIdentity'}
            def patch_word(resource,offset,value):
                changed=False
                for chunk in chunks:
                    packet=None
                    if chunk.get('name')=='Internal::Initial Contents' and field(chunk,'id').text==resource:packet=field(chunk,'Contents')
                    elif chunk.get('name')=='MTLDevice::newBufferWithBytes' and field(chunk,'Buffer').text==resource:packet=field(chunk,'initialData')
                    elif chunk.get('name') in ('MTLBuffer::InternalModifyCPUContents','Internal_MTLBufferModifyCPUContents') and field(chunk,'Buffer').text==resource:
                        start=int(field(chunk,'start').text)
                        if start==0:packet=field(chunk,'data')
                    if packet is not None and int(packet.get('byteLength','0'))>=offset+8:
                        key=f'{int(packet.text):06}';data=bytearray(blobs[key]);struct.pack_into('<Q',data,offset,value);blobs[key]=bytes(data);changed=True
                assert changed
            def patch_header(at,value):patch_word(header,headerOffset+at,value)
            if tag.startswith('header-write-') or tag=='legal-header-write-repeat':
                declarations2=[c for c in chunks if c.get('name')=='MTLBuffer::DeclareRayASHeader']
                initial_header,write=declarations2[0],declarations2[-1]
                if tag=='header-write-missing':chunks.remove(write)
                elif tag=='header-write-first':chunks.remove(write);chunks.insert(list(chunks).index(dispatch),write)
                elif tag=='header-write-last':chunks.remove(write);chunks.append(write)
                elif tag=='header-write-AS-type':field(write,'structure').text=header
                elif tag=='header-write-contribution-type':field(write,'contributions').text=field(write,'structure').text
                elif tag=='header-write-offset':field(write,'contributionOffset').text=str(int(field(write,'contributionOffset').text)+4)
                elif tag.startswith('header-write-snapshot-'):
                    packet=field(write,'bytes');key=f'{int(packet.text):06}';data=bytearray(blobs[key])
                    struct.pack_into('<Q',data,{'header-write-snapshot-AS':0,'header-write-snapshot-VA':8,'header-write-snapshot-reserved':16}[tag],999999);blobs[key]=bytes(data)
                elif tag=='legal-header-write-repeat':chunks.insert(list(chunks).index(write)+1,copy.deepcopy(write))
                elif tag=='header-write-no-first-commit':
                    encoder=field(dispatch,'ComputeCommandEncoder').text
                    creation=next(c for c in chunks if 'computeCommandEncoder' in c.get('name','') and field(c,'ComputeCommandEncoder') is not None and field(c,'ComputeCommandEncoder').text==encoder)
                    command=field(creation,'CommandBuffer').text
                    chunks.remove(next(c for c in chunks if c.get('name')=='MTLCommandBuffer::commit' and field(c,'CommandBuffer').text==command))
                elif tag=='header-write-alias':
                    alias=ET.Element('chunk',id='1089',name='MTLBuffer::makeAliasable',length='8')
                    ET.SubElement(alias,'ResourceId',name='Buffer',typename='MTLBuffer',width='8').text=header
                    # Use the established makeAliasable chunk ID from the enum/string map.
                    enum_source=(r/'renderdoc/driver/metal/metal_common.h').read_text()
                    enum=enum_source.split('enum class MetalChunk',1)[1].split('};',1)[0]
                    names=[line.strip().split('=')[0].strip().rstrip(',') for line in enum.splitlines() if line.strip().startswith(('MTL','Max'))]
                    alias.set('id',str(1000+names.index('MTLBuffer_makeAliasable')));chunks.insert(list(chunks).index(write)+1,alias)
                elif tag=='header-write-GPU-contribution':
                    fill=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::fillBuffer')
                    field(fill,'buffer').text=field(write,'contributions').text
                    field(field(fill,'range'),'location').text=field(write,'contributionOffset').text
                    field(field(fill,'range'),'length').text='4'
                else:raise RuntimeError(tag)
            elif tag.startswith('typed-'):
                typed=next(c for c in chunks if c.get('name')=='MTLBuffer::DeclareRayASHeader')
                scalar={'typed-null-buffer':('buffer',0),'typed-null-AS':('structure',0),'typed-null-contribution':('contributions',0),
                        'typed-AS-is-buffer':('structure',header),'typed-AS-is-BLAS':('structure',min(identities,key=identities.get)),
                        'typed-contribution-is-header':('contributions',header),'typed-contribution-is-output':('contributions',output),
                        'typed-offset-align':('offset',headerOffset+1),'typed-offset-end':('offset',headerOffset+64),
                        'typed-offset-overflow':('offset',2**64-8),'typed-contribution-align':('contributionOffset',1),
                        'typed-contribution-end':('contributionOffset',16384)}
                if tag in scalar:key,value=scalar[tag];field(typed,key).text=str(value)
                elif tag.startswith('typed-snapshot-'):
                    packet=field(typed,'bytes');key=f'{int(packet.text):06}';data=bytearray(blobs[key])
                    if tag=='typed-snapshot-short':data.pop();packet.set('byteLength',str(len(data)))
                    else:struct.pack_into('<Q',data,{'typed-snapshot-AS':0,'typed-snapshot-VA':8,'typed-snapshot-reserved':16}[tag],999999)
                    blobs[key]=bytes(data)
                elif tag=='typed-duplicate':chunks.insert(list(chunks).index(typed),copy.deepcopy(typed))
                elif tag=='typed-frame':chunks.remove(typed);chunks.insert(list(chunks).index(dispatch),typed)
                elif tag=='typed-without-query':
                    for c in list(chunks):
                        if c.get('name')=='MTLComputePipelineState::DeclareRayQueryDispatch':chunks.remove(c)
                elif tag=='typed-coverage':field(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='65'
                else:raise RuntimeError(tag)
            elif tag.startswith('null-'):field(declaration,tag[5:]).text='0'
            elif tag=='duplicate':chunks.insert(list(chunks).index(declaration),copy.deepcopy(declaration))
            elif tag=='frame-declaration':chunks.remove(declaration);chunks.insert(list(chunks).index(dispatch),declaration)
            elif tag=='missing-query':chunks.remove(declaration)
            elif tag=='wrong-coverage':field(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='4'
            elif tag.startswith('offset-'):field(declaration,'offset').text='1' if tag=='offset-align' else str(2**64-8)
            elif tag=='header-is-output':field(declaration,'header').text=output
            elif tag=='roots-is-header':field(declaration,'roots').text=header
            elif tag=='output-is-roots':field(declaration,'output').text=roots
            elif tag=='wrong-output':field(declaration,'output').text=next(field(c,'Buffer').text for c in chunks if c.get('name')=='MTLDevice::newBufferWithBytes' and field(c,'Buffer').text not in (roots,output))
            elif tag=='wrong-pipeline':field(declaration,'pipeline').text=header
            elif tag=='missing-root-layout':chunks.remove(rootLayout)
            elif tag=='missing-AS-layout':chunks.remove(ASLayout)
            elif tag=='wrong-AS-schema':field(ASLayout,'schema').text='0'
            elif tag=='function-table-schema':field(ASLayout,'schema').text='5'
            elif tag=='root-count':field(rootLayout,'count').text='1'
            elif tag=='root-stride':field(rootLayout,'stride').text='16'
            elif tag=='root-header-null':patch_word(roots,rootOffset,0)
            elif tag=='root-output-null':patch_word(roots,rootOffset+8,0)
            elif tag=='root-swap':
                packet=next(field(c,'initialData') for c in chunks if c.get('name')=='MTLDevice::newBufferWithBytes' and field(c,'Buffer').text==roots)
                a,c=struct.unpack_from('<QQ',blobs[f'{int(packet.text):06}'],rootOffset);patch_word(roots,rootOffset,c);patch_word(roots,rootOffset+8,a)
            elif tag.startswith('AS-'):patch_header(0,0 if tag=='AS-null' else 99999999 if tag=='AS-unknown' else min(identities.values()))
            elif tag.startswith('header-range-'):
                captured={field(c,'resource').text:int(field(c,'value').text) for c in chunks if c.get('name')=='MTLResource::CaptureGPUIdentity' and field(c,'kind').text=='0'}
                length=headerOffset+80
                values={'header-range-before':captured[header]+headerOffset-8,'header-range-align':captured[header]+headerOffset+1,
                    'header-range-end':captured[header]+length-32,'header-range-overflow':2**64-8,'header-range-other-buffer':captured[output]}
                if tag in values:patch_word(roots,rootOffset,values[tag])
                elif tag=='header-range-AS-layout':field(ASLayout,'offset').text=str(headerOffset-8)
                elif tag=='header-range-VA-layout':
                    layout=next(c for c in layouts if field(c,'buffer').text==header and field(c,'schema').text=='0')
                    field(layout,'offset').text=str(headerOffset+16)
                else:
                    creation=next(c for c in chunks if c.get('name') in ('MTLDevice::newBufferWithLength','MTLHeap::newBuffer(offset)') and field(c,'Buffer').text==header)
                    field(creation,'length').text=str(16*1024+8)
            elif tag=='header-reserved':patch_header(16,1)
            elif tag=='contribution-unknown':patch_header(8,2**64-1)
            elif tag=='contribution-unaligned':
                packet=next(field(c,'Contents') for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==header)
                value=struct.unpack_from('<Q',blobs[f'{int(packet.text):06}'],headerOffset+8)[0];assert value
                patch_header(8,value+1)
            elif tag.startswith(('recipe-','frozen-')):
                recipe=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'kind') is not None and field(c,'kind').text=='9')
                parameters=field(recipe,'parameters')
                values={'recipe-count-zero':(3,0),'recipe-count-65':(3,65),'recipe-offset':(0,1),
                    'recipe-stride':(1,71),'recipe-type':(2,0),'recipe-reserved':(4,1)}
                if tag in values:index,value=values[tag];parameters[index].text=str(value)
                elif tag=='recipe-missing-child':field(recipe,'children').clear()
                elif tag=='recipe-wrong-child':field(recipe,'children')[0].text=header
                elif tag in ['recipe-kind','recipe-empty-kind','recipe-masked-kind']:field(recipe,'kind').text=str({'recipe-kind':123,'recipe-empty-kind':10,'recipe-masked-kind':11}[tag])
                else:
                    packet=field(recipe,'vertices');key=f'{int(packet.text):06}';data=bytearray(blobs[key])
                    if tag.startswith('frozen-child-'):struct.pack_into('<Q',data,64,0 if tag=='frozen-child-null' else 99999999)
                    elif tag=='frozen-transform-NaN':struct.pack_into('<I',data,0,0x7fc00000)
                    elif tag=='frozen-options':struct.pack_into('<I',data,48,2**32-1)
                    else:raise RuntimeError(tag)
                    blobs[key]=bytes(data)
            elif tag.startswith(('empty-','masked-')):
                kind='10' if tag.startswith('empty-') else '11'
                recipe=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'kind') is not None and field(c,'kind').text==kind)
                parameters=field(recipe,'parameters')
                values={'empty-count':(3,1),'empty-type':(2,0),'empty-reserved':(4,1),'empty-offset':(0,1),'empty-stride':(1,71),
                    'masked-count-zero':(3,0),'masked-count-65':(3,65),'masked-type':(2,0),'masked-reserved':(4,1)}
                if tag in values:index,value=values[tag];parameters[index].text=str(value)
                elif tag.endswith('-child'):
                    node=ET.SubElement(field(recipe,'children'),'ResourceId',typename='ResourceId',width='8');node.text=header
                elif tag=='masked-kind-empty':field(recipe,'kind').text='10'
                else:
                    packet=field(recipe,'vertices');key=f'{int(packet.text):06}';data=bytearray(blobs[key])
                    if tag=='masked-active-mask':struct.pack_into('<I',data,52,255)
                    elif tag=='masked-nonnull-ID':struct.pack_into('<Q',data,64,min(identities.values()))
                    elif tag=='masked-transform-NaN':struct.pack_into('<I',data,0,0x7fc00000)
                    elif tag=='masked-options':struct.pack_into('<I',data,48,2**32-1)
                    else:raise RuntimeError(tag)
                    blobs[key]=bytes(data)
            elif tag.startswith('new-target-'):
                build=next(c for c in chunks if c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances')
                second=next(c for c in chunks if c.get('name')=='MTLComputePipelineState::DeclareRayQueryDispatch' and field(c,'roots').text!=roots)
                oldHeader=field(second,'header').text
                oldPacket=next(field(c,'Contents') for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==oldHeader)
                oldIdentity=struct.unpack_from('<Q',blobs[f'{int(oldPacket.text):06}'],headerOffset)[0]
                oldAS=next(id for id,value in identities.items() if value==oldIdentity)
                if tag=='new-target-missing-build':chunks.remove(build)
                elif tag=='new-target-wrong-built-target':field(build,'structure').text=oldAS
                else:
                    # Separate static headers can switch the old root pointer.
                    # A dynamic header already shares that pointer: move its real
                    # AS-ID publication before the first query instead.
                    hdr=[c for c in chunks if c.get('name')=='MTLBuffer::DeclareRayASHeader']
                    if len(hdr)>1 and field(hdr[0],'buffer').text==field(hdr[-1],'buffer').text:
                        write=hdr[-1];chunks.remove(write);chunks.insert(list(chunks).index(dispatch),write)
                    field(second,'header').text=header
                    rootIdentity=next(int(field(c,'value').text) for c in chunks if c.get('name')=='MTLResource::CaptureGPUIdentity' and field(c,'kind').text=='0' and field(c,'resource').text==header)
                    patch_word(field(second,'roots').text,rootOffset,rootIdentity+headerOffset)
            elif tag.startswith('geometry-') or tag=='legal-geometry-reversed-order':
                g=next(c for c in chunks if c.get('name') in geometry_names);multi=g.get('name')==geometry_names[1]
                top=next(c for c in chunks if c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances')
                vertex=field(g,'vertices').text;index=field(g,'indices').text;scratch=field(g,'scratch').text
                vertexUpload=next(field(c,'sourceBuffer').text for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer') is not None and field(c,'destinationBuffer').text==vertex)
                indexUpload=next((field(c,'sourceBuffer').text for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer') is not None and field(c,'destinationBuffer').text==index),'0')
                scalars={'geometry-encoder':('Encoder',999999),'geometry-target-null':('structure',0),'geometry-target-unknown':('structure',999999),
                    'geometry-target-TLAS':('structure',field(top,'structure').text),'geometry-vertices-unknown':('vertices',999999),
                    'geometry-vertices-Shared':('vertices',vertexUpload),'geometry-vertices-scratch':('vertices',scratch),
                    'geometry-scratch-null':('scratch',0),'geometry-scratch-unknown':('scratch',999999),'geometry-scratch-vertices':('scratch',vertex),
                    'geometry-kind':('kind',123),'geometry-kind-TLAS':('kind',9),'geometry-family':('kind',2 if multi else 8),
                    'geometry-scratch-offset-align':('scratchOffset',1),'geometry-scratch-offset-overflow':('scratchOffset',2**64-256),
                    'geometry-indices-null':('indices',0),'geometry-indices-unknown':('indices',999999),'geometry-indices-Shared':('indices',indexUpload)}
                last=len(field(g,'parameters'))-10
                parameters={'geometry-offset':(0,1),'geometry-offset-end':(0,256 if multi else 72),'geometry-offset-overflow':(0,2**64-1),
                    'geometry-stride':(1,4),'geometry-format':(2,999),'geometry-count-zero':(3,0),'geometry-count-over':(3,1000001),
                    'geometry-IFT-over':(4,999),'geometry-opaque-over':(5,2),'geometry-duplicate-over':(6,2),'geometry-refit':(7,1),'geometry-usage-over':(7,2**64-1),
                    'geometry-index-offset':(8,1),'geometry-index-type':(9,999),'geometry-last-offset':(last,161),
                    'geometry-last-stride':(last+1,4),'geometry-last-format':(last+2,999),'geometry-last-count':(last+3,0),
                    'geometry-last-index-offset':(last+8,25),'geometry-last-index-type':(last+9,999)}
                if tag in scalars:key,value=scalars[tag];field(g,key).text=str(value)
                elif tag in parameters:at,value=parameters[tag];field(g,'parameters')[at].text=str(value)
                elif tag in ['geometry-one-descriptor','geometry-short-array','geometry-over-array','legal-geometry-reversed-order']:
                    node=field(g,'parameters');items=[copy.deepcopy(n) for n in node]
                    for n in list(node):node.remove(n)
                    if tag=='legal-geometry-reversed-order':
                        for at in range(len(items)-10,-1,-10):
                            for n in items[at:at+10]:node.append(n)
                    else:
                        count={'geometry-one-descriptor':10,'geometry-short-array':19,'geometry-over-array':650}[tag]
                        for at in range(count):node.append(copy.deepcopy(items[at%len(items)]))
                        g.set('length',str(int(g.get('length'))+8192))
                else:
                    packet=field(g,'indexBytes' if tag in ['geometry-indices-truncated','geometry-index-outside','geometry-UInt16-outside'] else 'vertexBytes')
                    key=f'{int(packet.text):06}';data=bytearray(blobs[key])
                    if tag.endswith('truncated'):data.pop();packet.set('byteLength',str(len(data)))
                    elif tag in ['geometry-NaN','geometry-last-NaN']:struct.pack_into('<I',data,160 if tag=='geometry-last-NaN' else 16 if multi else 0,0x7fc00000)
                    elif tag=='geometry-index-outside':struct.pack_into('<I',data,24 if multi else 0,100000)
                    elif tag=='geometry-UInt16-outside':struct.pack_into('<H',data,4,65535)
                    else:raise RuntimeError(tag)
                    blobs[key]=bytes(data)
            elif tag.startswith('legal-frame-'):
                build=next(c for c in chunks if c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances')
                child=field(build,'children')[0].text;source=field(build,'instances').text
                upload=next(field(c,'sourceBuffer').text for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer') is not None and field(c,'destinationBuffer').text==source)
                oldID=identities[child];newID=oldID+256
                if tag=='legal-frame-child-remap':
                    for c in chunks:
                        if c.get('name')=='MTLAccelerationStructure::CaptureGPUIdentity' and field(c,'resource').text==child:field(c,'value').text=str(newID)
                for c in chunks:
                    packet=None;stride=72;start=0;isFrame=False
                    if c is build:packet=field(c,'descriptorBytes');isFrame=True
                    elif c.get('name')=='Internal::Initial Contents' and field(c,'kind') is not None and field(c,'kind').text=='9':packet=field(c,'vertices')
                    elif c.get('name')=='MTLDevice::newBufferWithBytes' and field(c,'Buffer').text==upload:packet=field(c,'initialData');stride=80;start=8;isFrame=True
                    elif c.get('name')=='Internal::Initial Contents' and field(c,'id').text==upload:packet=field(c,'Contents');stride=80;start=8;isFrame=True
                    elif c.get('name') in ('MTLBuffer::InternalModifyCPUContents','Internal_MTLBufferModifyCPUContents') and field(c,'Buffer').text==upload:
                        assert int(field(c,'start').text)==0
                        packet=field(c,'data');stride=80;start=8;isFrame=True
                    if packet is None:continue
                    key=f'{int(packet.text):06}';data=bytearray(blobs[key])
                    for at in range(start,len(data)-71,stride):
                        if tag=='legal-frame-user75' and isFrame:struct.pack_into('<I',data,at+60,struct.unpack_from('<I',data,at+60)[0]+1)
                        elif tag=='legal-frame-child-remap' and struct.unpack_from('<Q',data,at+64)[0]==oldID:struct.pack_into('<Q',data,at+64,newID)
                    blobs[key]=bytes(data)
            elif tag.startswith('frame-'):
                build=next(c for c in chunks if c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances')
                child=field(build,'children')[0].text;target=field(build,'structure').text
                source=field(build,'instances').text
                upload=next(field(c,'sourceBuffer').text for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer') is not None and field(c,'destinationBuffer').text==source)
                scalars={'frame-encoder':('Encoder',999999),'frame-target-null':('structure',0),'frame-target-unknown':('structure',999999),
                    'frame-target-BLAS':('structure',child),'frame-input-unknown':('instances',999999),'frame-input-Shared':('instances',upload),
                    'frame-input-TLAS':('instances',target),'frame-scratch-unknown':('scratch',999999),'frame-scratch-Shared':('scratch',upload)}
                parameters={'frame-offset':(0,1),'frame-stride':(1,71),'frame-type':(2,0),'frame-count-zero':(3,0),'frame-count-65':(3,65),'frame-reserved':(4,1)}
                if tag in scalars:key,value=scalars[tag];field(build,key).text=str(value)
                elif tag in parameters:index,value=parameters[tag];field(build,'parameters')[index].text=str(value)
                elif tag.startswith('frame-child-'):
                    node=field(build,'children');node.clear()
                    values=[] if tag=='frame-child-missing' else [child,child] if tag=='frame-child-duplicate' else [target] if tag=='frame-child-TLAS' else ['999999']
                    for value in values:ET.SubElement(node,'ResourceId',typename='ResourceId',width='8').text=value
                elif tag in ['frame-live-encoder','frame-late-commit','frame-unknown-queue']:
                    encoder=field(build,'Encoder').text
                    creation=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::accelerationStructureCommandEncoder' and field(c,'Encoder').text==encoder)
                    command=field(creation,'CommandBuffer').text
                    if tag=='frame-live-encoder':chunks.remove(next(c for c in chunks if c.get('name')=='MTLAccelerationStructureCommandEncoder::endEncoding' and field(c,'Encoder').text==encoder))
                    elif tag=='frame-late-commit':
                        commit=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::commit' and field(c,'CommandBuffer').text==command)
                        chunks.remove(commit);chunks.append(commit)
                    else:field(next(c for c in chunks if c.get('name')=='MTLCommandQueue::commandBuffer' and field(c,'CommandBuffer').text==command),'CommandQueue').text='999999'
                elif tag.startswith('frame-fill-'):
                    fill=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::fillBuffer')
                    field(fill,'buffer').text=header if tag=='frame-fill-header' else roots
                    field(field(fill,'range'),'location').text='0';field(field(fill,'range'),'length').text='4';field(fill,'value').text='1'
                else:
                    packet=field(build,'descriptorBytes');key=f'{int(packet.text):06}';data=bytearray(blobs[key])
                    if tag.startswith('frame-ID-'):struct.pack_into('<Q',data,64,0 if tag=='frame-ID-null' else 99999999 if tag=='frame-ID-unknown' else identities[target])
                    elif tag=='frame-transform-NaN':struct.pack_into('<I',data,0,0x7fc00000)
                    elif tag=='frame-options':struct.pack_into('<I',data,48,2**32-1)
                    elif tag=='frame-truncated':data.pop();packet.set('byteLength',str(len(data)))
                    else:raise RuntimeError(tag)
                    blobs[key]=bytes(data)
            elif tag.startswith('groups-'):field(field(dispatch,'groups'),'width' if tag!='groups-y' else 'height').text=str({'groups-zero':0,'groups-limit':4097,'groups-y':2,'groups-overflow':2**64-1}[tag])
            elif tag.startswith('threads-'):field(field(dispatch,'threadsPerGroup'),'width' if tag!='threads-z' else 'depth').text=str({'threads-zero':0,'threads-limit':1025,'threads-z':2}[tag])
            elif tag=='output-overrun':field(field(dispatch,'groups'),'width').text='2'
            elif tag in ['wrong-slot','missing-binding']:
                binding=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::setBuffer')
                if tag=='wrong-slot':field(binding,'index').text='3'
                else:chunks.remove(binding)
            for chunk in chunks:chunk.set('length',str(int(chunk.get('length','0'))+128))
            xml=w/(tag+'.zip.xml');variant.write(xml,encoding='utf-8',xml_declaration=True)
            with zipfile.ZipFile(xml.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as archive:
                for name,data in blobs.items():archive.writestr(name,data)
            cap=w/(tag+'.rdc');run(tag+'-import',[b/'bin/renderdoccmd','convert','-f',xml,'-o',cap,'-c','rdc'],0)
            if tag.startswith('legal-'):
                mode=args.mode+('-user75' if tag=='legal-frame-user75' else '-geometry-order-control' if tag=='legal-geometry-reversed-order' else '')
                run(tag+'-API',[args.oracle,cap,mode,args.instances],0)
                run(tag+'-CLI',[b/'bin/renderdoccmd','replay','--loops','3',cap],0);m['legal_controls']+=1
            else:
                run(tag+'-API',[b/'metal-ray-b520/final-heaps/opener',cap],4)
                run(tag+'-CLI',[b/'bin/renderdoccmd','replay','--loops','1',cap],1);m['rejections']+=2
            save()
        assert hashlib.sha256((b/'lib/librenderdoc.dylib').read_bytes()).hexdigest()==m['backend_sha256'];m['status']='PASS';save();return 0
    except Exception as ex:m.update(status='FAIL',error=str(ex));save();raise


if __name__=='__main__':
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX);raise SystemExit(main())
