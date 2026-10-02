#!/usr/bin/env python3
"""Extract one captured UE UpdateDescriptorHandle shader and exact bounded opaque-copy input.

CPU only. Does not execute the original UE frame or redistribute its compiled shader.
The payload's VA/texture fields are opaque bytes in this isolated copy pass; no consumer
of those entries is included. This is not complete UE descriptor relocation acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
import zipfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xml',type=Path);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--minimum-updates',type=int,choices=range(1,9),default=1)
    args=parser.parse_args();chunks=list(ET.parse(args.xml).find('./chunks'))
    def node(c,n):return next(x for x in c if x.get('name')==n)
    def value(c,n):return node(c,n).text
    def integer(c,n):return int(value(c,n))
    scope=next(i for i,c in enumerate(chunks) if c.get('id')=='5');frame=chunks[scope+1:]
    groups={}
    for c in frame:
        if c.get('name')=='MTLBuffer::DescriptorSlotProducer':groups.setdefault(integer(c,'encoder'),[]).append(c)
    selected=next(g for g in groups.values() if args.minimum_updates<=len(g)<=8 and
                  all(integer(c,'offset')%24==0 and integer(c,'offset')//24<2048 for c in g))
    first=selected[0];encoder=integer(first,'encoder')
    creation=next(c for c in frame if c.get('name','').startswith('MTLCommandBuffer::computeCommandEncoder') and value(c,'ComputeCommandEncoder')==str(encoder))
    command=integer(creation,'CommandBuffer')
    owned=[c for c in frame if any(x.get('name') in ('ComputeCommandEncoder','encoder') and x.text==str(encoder) for x in c)]
    dispatches=[c for c in owned if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups'];assert len(dispatches)==1
    for n in ('groups','threadsPerGroup'):
        assert [int(x.text) for x in node(dispatches[0],n)]==[1,1,1]
    pipeline=integer(next(c for c in owned if c.get('name')=='MTLComputeCommandEncoder::setComputePipelineState'),'pipeline')
    pc=next(c for c in chunks if c.get('name','').startswith('MTLDevice::newComputePipeline') and any(x.get('name')=='ComputePipelineState' and x.text==str(pipeline) for x in c))
    descriptor=node(pc,'descriptor');function=integer(descriptor,'computeFunction')
    assert integer(descriptor,'maxTotalThreadsPerThreadgroup')==1
    fc=next(c for c in chunks if c.get('name','').startswith('MTLLibrary::newFunction') and any(x.get('name')=='Function' and x.text==str(function) for x in c))
    assert value(fc,'supported')=='true'
    for name in ('constantNames','constantIndices','constantTypes','constantValues'):assert not list(node(fc,name))
    library=integer(fc,'Library');lc=next(c for c in chunks if c.get('name')=='MTLDevice::newLibraryWithData' and integer(c,'Library')==library)
    root=next(c for c in owned if c.get('name')=='MTLCommandEncoder::DescriptorInlineBinding' and integer(c,'index')==2 and integer(c,'entry')==0)
    root_buffer,root_offset=integer(root,'resource'),integer(root,'memberOffset')
    producers=[c for c in frame if c.get('name')=='MTLBuffer::DescriptorSlotProducer' and integer(c,'encoder')==encoder]
    assert 0<len(producers)<=8
    destination=integer(first,'buffer');assert all(integer(c,'buffer')==destination for c in producers)
    offsets=[integer(c,'offset') for c in producers];assert all(o%24==0 and o//24<2048 for o in offsets)
    args.output.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(args.xml.with_suffix('')) as archive:
        def blob(c,n):return archive.read(f'{integer(c,n):06d}')
        initial=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and integer(c,'id')==root_buffer)
        memory=bytearray(blob(initial,'Contents'))
        commit=next(i for i,c in enumerate(chunks) if c.get('name')=='MTLCommandBuffer::commit' and integer(c,'CommandBuffer')==command)
        for c in chunks[scope+1:commit]:
            if c.get('name')=='Internal_MTLBufferModifyCPUContents' and integer(c,'Buffer')==root_buffer:
                start=integer(c,'start');data=blob(c,'data');assert start+len(data)<=len(memory);memory[start:start+len(data)]=data
        uniform=bytes(memory[root_offset:root_offset+16]);assert len(uniform)==16
        count,indices_handle,entries_handle,destination_handle=struct.unpack('<4I',uniform)
        assert count==len(producers) and (indices_handle,entries_handle,destination_handle)==(1,0,2)
        table=integer(next(c for c in owned if c.get('name')=='MTLComputeCommandEncoder::setBuffer' and integer(c,'index')==0),'buffer')
        index_binding=next(c for c in chunks[:chunks.index(first)] if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and integer(c,'buffer')==table and integer(c,'offset')==24)
        assert integer(index_binding,'resource')==root_buffer and integer(index_binding,'kind')==0
        index_offset=integer(index_binding,'memberOffset');indices=bytes(memory[index_offset:index_offset+count*4])
        assert list(struct.unpack('<'+'I'*count,indices))==[o//24 for o in offsets]
        payload=b''
        for producer in producers:
            source=integer(producer,'source');offset=integer(producer,'sourceOffset')
            seed=next(c for c in reversed(chunks[:chunks.index(producer)]) if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and integer(c,'buffer')==source and integer(c,'offset')==offset and integer(c,'event')==2)
            packet=blob(seed,'data');assert len(packet)==24
            assert source==root_buffer and bytes(memory[offset:offset+24])==packet
            payload+=packet
        code=blob(lc,'data');assert code[:4]==b'MTLB' and len(code)<64*1024
        sampler_binding=next(c for c in owned if c.get('name')=='MTLCommandEncoder::DescriptorInlineBinding' and integer(c,'index')==2 and integer(c,'entry')==1)
        sampler_id=integer(sampler_binding,'resource')
        sampler_initial=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and integer(c,'id')==sampler_id)
        samplers=blob(sampler_initial,'Contents');assert 0<len(samplers)<=1024
        for name,data in [('shader.metallib',code),('payload.bin',payload),('indices.bin',indices),('uniform.bin',uniform),('samplers.bin',samplers)]:
            (args.output/name).write_bytes(data)
        manifest={'capture_xml':str(args.xml.resolve()),'encoder':encoder,'command':command,'pipeline':pipeline,'function':value(fc,'functionName'),
            'shader_sha256':hashlib.sha256(code).hexdigest(),'count':count,'destination_offsets':offsets,
            'destination_bytes':max(offsets)+24,'root_buffer':root_buffer,'root_offset':root_offset,
            'payload_sha256':hashlib.sha256(payload).hexdigest(),'mode':'opaque copy only; no descriptor consumer; not full UE replay'}
        (args.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))


if __name__=='__main__':main()
