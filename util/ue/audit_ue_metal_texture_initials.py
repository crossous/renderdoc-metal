#!/usr/bin/env python3
"""CPU-only texture initial byte layouts and live descriptor parent/backing closure."""
import argparse
from collections import Counter
import json
from pathlib import Path
import xml.etree.ElementTree as ET
import zipfile

def node(c,name):return next((n for n in c if n.get('name')==name),None)
def value(c,name,default=0):
    n=node(c,name);return int(n.text) if n is not None else default

def retirement_prelude(chunks, scope, slots, texture_births=False):
    """CPU proof of the bounded unsubmitted prefix, separate from whole-frame acceptance."""
    cpu_births={'MTLDevice::newBufferWithLength','MTLDevice::newBufferWithBytes',
                'MTLHeap::newBuffer(offset)','MTLHeap::newBufferWithLength'}
    metadata={'MTLResource::CaptureGPUIdentity','MTLBuffer::DescriptorSlotBinding',
              'MTLBuffer::DeclareDescriptorTable','MTLBuffer::DeclareDescriptorGPUWrites'}
    texture_creations={'MTLBuffer::newTextureWithDescriptor','MTLHeap::newTexture(offset)'}
    tables={value(c,'buffer') for c in chunks if c.get('name')=='MTLBuffer::DeclareDescriptorTable'}
    buffers={};commands=set();encoders=set();known_encoders=set();copies=[];retired={};issues=[]
    stop=None;copy_bytes=0
    for c in chunks:
        index=int(c.get('chunkIndex'));name=c.get('name','')
        if index<=scope or c.get('id')=='4':continue
        if name=='MTLBuffer::DescriptorSlotEvent':
            event=value(c,'event')
            if event not in (0,1,2):stop={'chunk':index,'api':name};break
            if event==1:
                key=(value(c,'buffer'),value(c,'offset'));s=slots.get(key)
                if s and s['live'] and s['generation']==value(c,'generation') and s['type']==value(c,'descriptorType'):
                    if key in retired:issues.append({'chunk':index,'reason':'duplicate frozen retirement'})
                    retired[key]=index
                if len(retired)>256:issues.append({'chunk':index,'reason':'retirement limit'})
        elif name in cpu_births:
            rid=value(c,'Buffer');length=value(c,'length')
            if not rid or not 0<length<=65536 or rid in buffers:
                issues.append({'chunk':index,'reason':'invalid frame buffer birth'})
            buffers[rid]=length
        elif name in metadata:pass
        elif texture_births and name in texture_creations:
            # Creation legality/Native alias footprint remains a separate backend preflight.
            if name=='MTLBuffer::newTextureWithDescriptor' and value(c,'Buffer') not in buffers:
                issues.append({'chunk':index,'reason':'view precedes backing buffer'})
        elif name.startswith('MTLCommandQueue::commandBuffer'):
            command=value(c,'CommandBuffer')
            if not command or command in commands or len(commands)>=8:
                issues.append({'chunk':index,'reason':'invalid prefix command birth'})
            commands.add(command)
        elif name in ('MTLCommandBuffer::blitCommandEncoder','MTLCommandBuffer::blitCommandEncoderWithDescriptor'):
            command=value(c,'CommandBuffer');encoder=value(c,'BlitCommandEncoder')
            if command not in commands or not encoder or encoder in known_encoders or len(known_encoders)>=8:
                issues.append({'chunk':index,'reason':'invalid prefix blit birth'})
            encoders.add(encoder);known_encoders.add(encoder)
        elif name=='MTLBlitCommandEncoder::copyFromBuffer' and node(c,'destinationBuffer') is not None:
            source=value(c,'sourceBuffer');destination=value(c,'destinationBuffer');size=value(c,'size')
            so=value(c,'sourceOffset');do=value(c,'destinationOffset');encoder=value(c,'BlitCommandEncoder')
            if encoder not in encoders or source==destination or source not in buffers or destination not in buffers or not size or so+size>buffers.get(source,0) or do+size>buffers.get(destination,0) or source in tables or destination in tables or len(copies)>=16 or copy_bytes+size>65536:
                issues.append({'chunk':index,'reason':'unproven prefix plain buffer copy'})
            copies.append({'chunk':index,'source':source,'destination':destination,'source_offset':so,'destination_offset':do,'size':size})
            copy_bytes+=size
        elif name in ('MTLBlitCommandEncoder::setLabel','MTLBlitCommandEncoder::endEncoding'):
            encoder=value(c,'BlitCommandEncoder')
            if encoder not in encoders:issues.append({'chunk':index,'reason':'closed or unknown prefix blit'})
            if name.endswith('endEncoding'):encoders.discard(encoder)
        else:stop={'chunk':index,'api':name};break
    effective={s['bindings'][1] for key,s in slots.items() if s['live'] and 1 in s['bindings'] and key not in retired}
    return {'first_excluded_operation':stop,'exact_generation_frees':len(retired),
            'effective_unique_texture_sources':len(effective),'frame_buffer_copy_bytes':copy_bytes,
            'frame_buffer_copies':copies,'proof_issues':issues},retired,effective

def audit(xml):
    chunks=ET.parse(xml).find('./chunks');scope=next(int(c.get('chunkIndex')) for c in chunks if c.get('id')=='5')
    creations,views,initials,buffer_initials,slots={}, {}, {}, {}, {}
    replacements={};retirement_boundary=None
    # Independent logical Metal format widths; no native heap footprint/alignment assumptions.
    sizes={10:1,11:1,12:1,13:1,20:2,23:2,25:2,30:2,40:4,53:4,55:4,60:4,63:4,65:4,
           70:4,71:4,73:4,80:4,81:4,90:4,92:4,103:8,105:8,110:8,115:8,123:16,125:16,250:2,252:4,260:5}
    bc={130:8,131:8,132:16,133:16,134:16,135:16,140:8,141:8,142:16,143:16,150:16,151:16,152:16,153:16}
    issues=[];fmt_counts=Counter();total=0
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        for c in chunks:
            name=c.get('name','');index=int(c.get('chunkIndex'))
            if name in ('MTLDevice::newTextureWithDescriptor','MTLHeap::newTexture(offset)','MTLHeap::newTextureWithDescriptor','MTLBuffer::newTextureWithDescriptor','[CAMetalLayer nextDrawable]'):
                desc=node(c,'descriptor')
                if desc is not None:
                    creations[value(c,'Texture')]={'chunk':index,'api':name,'descriptor':{n.get('name'):int(n.text) for n in desc if n.text and n.text.isdigit()},'buffer':value(c,'Buffer')}
            elif name.startswith('MTLTexture::newTextureView'):
                views[value(c,'View')]=value(c,'Source')
            elif name=='MTLTexture::replaceRegion' and index<scope:
                replacements.setdefault(value(c,'Texture'),[]).append(index)
            elif c.get('id')=='3':
                content=node(c,'Contents');rid=value(c,'id');kind=value(c,'type')
                if content is None:continue
                actual=len(archive.read(f'{int(content.text):06d}'))
                if actual!=int(content.get('byteLength')):issues.append({'resource':rid,'reason':'archive length mismatch'})
                if kind==9:
                    if rid in initials:issues.append({'resource':rid,'reason':'duplicate initial record'})
                    initials[rid]=actual
                elif kind==1:buffer_initials[rid]=actual
            if index>scope and retirement_boundary is None and ('CommandQueue::commandBuffer' in name or
                 'CommandBuffer::' in name or 'CommandEncoder::' in name):retirement_boundary=index
            if index>=scope:continue
            if name=='MTLBuffer::DescriptorSlotEvent':
                key=(value(c,'buffer'),value(c,'offset'));event=value(c,'event')
                if event==0:slots[key]={'live':True,'generation':value(c,'generation'),'type':value(c,'descriptorType'),'bindings':{}}
                elif event==1 and key in slots:slots[key]['live']=False
                elif event in (2,3) and key in slots:slots[key]['bindings']={}
            elif name=='MTLBuffer::DescriptorSlotBinding':
                key=(value(c,'buffer'),value(c,'offset'))
                if key in slots:slots[key]['bindings'][value(c,'kind')]=value(c,'resource')
    for rid,actual in initials.items():
        info=creations.get(rid);desc=info['descriptor'] if info else {};fmt=desc.get('pixelFormat',0)
        kind=desc.get('textureType',0);block=4 if fmt in bc else 1;size=bc.get(fmt,sizes.get(fmt))
        if size is None:issues.append({'resource':rid,'reason':'unknown format','format':fmt});continue
        w,h,d=desc.get('width',0),desc.get('height',0),desc.get('depth',1);arrays=desc.get('arrayLength',1)
        slices=6*arrays if kind in (5,6) else arrays if kind==3 else 1
        expected=sum(((max(1,w>>m)+block-1)//block)*((max(1,h>>m)+block-1)//block)*size*(max(1,d>>m) if kind==7 else 1)*slices for m in range(desc.get('mipmapLevelCount',0)))
        if expected!=actual:issues.append({'resource':rid,'format':fmt,'type':kind,'expected':expected,'actual':actual,'reason':'packed size mismatch'})
        fmt_counts[str(fmt)]+=1;total+=actual
    # Keep raw frozen sources, and separately prove exact-generation frees before any CB creation.
    live_sources=sorted({s['bindings'][1] for s in slots.values() if s['live'] and 1 in s['bindings']})
    retired={}
    for c in chunks:
        index=int(c.get('chunkIndex'))
        if scope<index<(retirement_boundary or scope) and c.get('name')=='MTLBuffer::DescriptorSlotEvent' and value(c,'event')==1:
            key=(value(c,'buffer'),value(c,'offset'));s=slots.get(key)
            if s and s['live'] and s['generation']==value(c,'generation'):retired[key]=index
    effective_sources={s['bindings'][1] for key,s in slots.items() if s['live'] and 1 in s['bindings'] and key not in retired}
    prefix,_,_=retirement_prelude(chunks,scope,slots)
    candidate,candidate_retired,candidate_sources=retirement_prelude(chunks,scope,slots,True)
    classes=Counter();candidate_classes=Counter();sources=[]
    for rid in live_sources:
        parent=rid;visited=set()
        while parent in views and parent not in visited:visited.add(parent);parent=views[parent]
        info=creations.get(parent);backing=info.get('buffer',0) if info else 0
        if parent in initials:status='texture_initial'
        elif backing:status='buffer_initial' if backing in buffer_initials else 'buffer_initial_missing'
        elif info and info['chunk']>=scope:status='frame_birth'
        elif info and info['api']=='[CAMetalLayer nextDrawable]':status='drawable'
        elif info and info['descriptor'].get('storageMode')==0:status='shared_texture_without_initial'
        else:status='texture_initial_missing'
        candidate_status='retired_in_unsubmitted_prefix' if rid not in candidate_sources else status
        candidate_classes[candidate_status]+=1
        if rid not in effective_sources:status='retired_before_command_buffer'
        classes[status]+=1
        sources.append({'resource':rid,'parent':parent,'backing_buffer':backing,'status':status,
                        'initial_bytes':initials.get(parent,buffer_initials.get(backing,0)),
                        'background_replace_region_chunks':replacements.get(parent,[]),**(info or {})})
    return {'gpu_commands_submitted':0,'scope':scope,'texture_initial_records':len(initials),
            'texture_initial_bytes':total,'format_counts':dict(fmt_counts),'packed_layout_issues':issues,
            'live_unique_texture_sources':len(live_sources),'effective_unique_texture_sources':len(effective_sources),
            'first_command_buffer_chunk':retirement_boundary,'exact_generation_early_frees':len(retired),
            'source_classes':dict(classes),'v30_unsubmitted_prefix':prefix,
            'cpu_texture_birth_candidate':{**candidate,'source_classes':dict(candidate_classes)},'sources':sources}

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('xml',type=Path);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    result=audit(args.xml);args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='sources'},indent=2))
    sys_exit=1 if result['packed_layout_issues'] else 0
    raise SystemExit(sys_exit)
