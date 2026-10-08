#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU evidence for the fixed converted TraceRay sample, not a replay permission."""
import json
from pathlib import Path
import struct
import sys
import xml.etree.ElementTree as ET
from zipfile import ZipFile


def audit_capture(path, mode="default"):
    local = 'local-root' in mode
    global_root = 'global-' in mode
    grs_size = 64 if global_root else 16
    heaps = 'descriptor-heaps' in mode
    heap_as = 'heap-as' in mode
    heap_only = 'heap-as-only' in mode
    indirect = 'indirect-tlas' in mode
    inactive = indirect and any(x in mode for x in ('-empty','-masked'))
    frame = 'frame-build' in mode
    identity_count = (3 if heap_as else 2 if indirect and (frame or not inactive) else 1) + (1 if "geometry-new-target" in mode else 0)
    sampler_count = 6 if 'six-samplers' in mode else 1
    stride = 96 if local else 32
    sbt_size = stride*3
    tree = ET.parse(path); chunks = tree.findall('./chunks/chunk')
    value = lambda c, name: int(c.find('./*[@name="'+name+'"]').text)
    with ZipFile(Path(str(path)[:-4])) as archive:
        def data(c, name):
            node = c.find('./buffer[@name="'+name+'"]')
            raw = archive.read(f'{int(node.text):06}')
            if len(raw) != int(node.get('byteLength')): raise ValueError('invalid exported blob length')
            return raw
        sources = {value(c,'Buffer'): data(c,'initialData') for c in chunks
            if c.get('name') == 'MTLDevice::newBufferWithBytes'}
        addresses = {value(c,'resource'): value(c,'value') for c in chunks
            if c.get('name') == 'MTLResource::CaptureGPUIdentity' and value(c,'kind') == 0}
        by_address = {va:sources[id] for id,va in addresses.items() if id in sources}
        labels={value(c,'resource'):c.find('./string[@name="label"]').text for c in chunks
            if c.get('name')=='MTLResource::setLabel'}
        packets=[id for id,raw in sources.items() if len(raw)==152]
        if len(packets)!=(2 if frame else 1):raise ValueError('dispatch packet count differs')
        packet_ids=[id for id in packets if labels.get(id)=='IR dispatch packet']
        if len(packet_ids) != 1: raise ValueError('missing unique dispatch packet')
        packet_id=packet_ids[0]; packet=sources[packet_id]
        grs_va=struct.unpack_from('<Q',packet,104)[0]; sbt_va=struct.unpack_from('<Q',packet,0)[0]
        grs_id=next(id for id,va in addresses.items() if va==grs_va)
        sbt_id=next(id for id,va in addresses.items() if va==sbt_va)
        grs_data=by_address[grs_va]; sbt_data=by_address[sbt_va]
        if len(grs_data)!=grs_size or len(sbt_data)!=sbt_size: raise ValueError('IR backing extent differs')
        output_va=struct.unpack_from('<Q',grs_data,0 if heap_only else 8)[0]
        output_id=next(id for id,va in addresses.items() if va==output_va)
        if sources[output_id] != struct.pack('<II',7,7): raise ValueError('output initial state differs')
        tables = {value(c,'kind'): value(c,'value') for c in chunks
            if c.get('name') == 'MTLFunctionTable::CaptureGPUIdentity'}
        structures = [value(c,'value') for c in chunks
            if c.get('name') == 'MTLAccelerationStructure::CaptureGPUIdentity']
        initials = [c for c in chunks if c.get('name') == 'Internal::Initial Contents' and
            c.find('./enum[@name="type"]').get('string') == 'eResBuffer']
        header_pointer = struct.unpack_from('<Q', grs_data, 0)[0]
        if heap_only:
            named=[c for c in chunks if c.get('name')=='MTLResource::setLabel' and c.find('./string[@name="label"]') is not None and c.find('./string[@name="label"]').text=='IR AS header']
            if len(named)!=1: raise ValueError('missing unused fixture AS header label')
            header_pointer=addresses[value(named[0],'resource')]
        headers = [c for c in initials if addresses.get(value(c, 'id')) == header_pointer and
            c.find('./buffer[@name="Contents"]').get('byteLength') == '64']
        for c in initials:
            if value(c,'id') in addresses: by_address[addresses[value(c,'id')]]=data(c,'Contents')
        if len(headers)!=1 or len(structures)!=identity_count: raise ValueError('missing AS header/contributions identity')
        header=data(headers[0],'Contents');as_id,contribution_va=struct.unpack_from('<QQ',header)
        if len(header)!=64 or as_id not in structures or any(header[16:]) or by_address[contribution_va]!=bytes(4):raise ValueError('AS header or instance contributions mismatch')
        if struct.unpack_from('<QQ',grs_data) != ((output_va,0) if heap_only else (addresses[value(headers[0],'id')],output_va)):
            raise ValueError('GRS sources mismatch')
        records = [struct.unpack_from('<QQQQ',sbt_data,i*stride) for i in range(3)]
        null_hit = any(x in mode for x in ('null-hit','null-both','no-closest'))
        null_miss = any(x in mode for x in ('null-miss','null-both'))
        any_hit = 'any-hit' in mode
        pad = (2**64-1) if 'ue-' in mode else 0xa5a5d00d98761234 if mode == 'pattern-pad' else 0
        expected = [(0,1,0,pad),(0,0 if null_miss else 2,0,pad),
            (4 if any_hit else 0,0 if null_hit else 3,0,pad)]
        local_sources = {}
        if local:
            local_backing = {addresses[value(c,'Buffer')]: data(c,'initialData') for c in chunks
                if c.get('name') == 'MTLDevice::newBufferWithBytes' and value(c,'Buffer') in addresses}
            active = next(i for i in (1,2) if expected[i][1] or expected[i][0])
            cb = struct.unpack_from('<Q',sbt_data,active*stride+32)[0]-(256 if active==2 else 0)
            srv = struct.unpack_from('<Q',sbt_data,active*stride+56)[0]-(8 if active==2 else 12)
            if len(local_backing[cb])!=512 or len(local_backing[srv])!=32: raise ValueError('local backing differs')
            static = records[active][2]
            for i in (1,2):
                used = expected[i][1] or expected[i][0]
                expected[i] = (*expected[i][:2],static if used else 0,pad)
                params = struct.unpack_from('<QIIIIQQ',sbt_data,i*stride+32)
                wanted = (cb+(256 if i==2 else 0),20 if i==2 else 30,0,0,1,
                    srv+(8 if i==2 else 12),params[-1]) if used else (0,0,0,0,0,0,0)
                if params != wanted or any(sbt_data[i*stride+72:(i+1)*stride]):
                    raise ValueError('local root pointers/constants or record padding mismatch')
                if used:
                    texture_entry = struct.unpack('<QQQ',local_backing[params[-1]])
                    sampler_entries = [struct.unpack_from('<QQQ',local_backing[static],j*24) for j in range(sampler_count)]
                    if texture_entry[0] or not texture_entry[1] or texture_entry[2] or \
                        any(not e[0] or e[1] or e[2] for e in sampler_entries):
                        raise ValueError('IR texture/sampler entry metadata differs')
                    local_local_backing = dict(CBV=cb,SRV=srv,texture_table=params[-1],static_samplers=static,sampler_ids=[e[0] for e in sampler_entries])
        if records != expected:
            raise ValueError('shader identifiers, nullable roles or scalar padding mismatch')
        sbt = sbt_va
        regions = [struct.unpack_from('<QQ',packet,0), *[struct.unpack_from('<QQQ',packet,i) for i in (16,40,64)]]
        if regions != [(sbt,32),(sbt+stride,stride,stride),(sbt+stride*2,stride,stride if local else 0),(0,0,0)]:
            raise ValueError('SBT range/stride/null mismatch')
        if struct.unpack_from('<III',packet,88) != (2,1,1) or any(packet[100:104]):
            raise ValueError('dispatch dimensions or padding mismatch')
        if struct.unpack_from('<QQQQQQ',packet,104) != (grs_va,struct.unpack_from('<Q',packet,112)[0] if heaps else 0,struct.unpack_from('<Q',packet,120)[0] if heaps else 0,tables[0],tables[1],0):
            raise ValueError('GRS/heaps/function tables mismatch')
        global_sources = {}
        if global_root:
            cbv=struct.unpack_from('<Q',grs_data,16)[0]; srv=struct.unpack_from('<Q',grs_data,40)[0]
            texture_table,samplers=struct.unpack_from('<QQ',grs_data,48)
            if struct.unpack_from('<IIII',grs_data,24)!=(200,0,0,1) or len(by_address[cbv-256])!=512 or struct.unpack_from('<I',by_address[cbv-256],256)[0]!=1000 or struct.unpack_from('<I',by_address[srv-4],4)[0]!=5:
                raise ValueError('global CBV/SRV/constants differ')
            texture_entry=struct.unpack('<QQQ',by_address[texture_table])
            entries=[struct.unpack_from('<QQQ',by_address[samplers],i*24) for i in range(6)]
            if texture_entry[0] or not texture_entry[1] or texture_entry[2] or any(not e[0] or e[1] or e[2] for e in entries): raise ValueError('global resource table metadata differs')
            global_sources=dict(CBV=cbv,SRV=srv,texture_table=texture_table,static_samplers=samplers,sampler_ids=[e[0] for e in entries])
        heap_sources = {}
        if heaps:
            resource_heap,sampler_heap=struct.unpack_from('<QQ',packet,112)
            entries=[struct.unpack_from('<QQQ',by_address[resource_heap],i*24) for i in range(4)]
            samplers=[struct.unpack_from('<QQQ',by_address[sampler_heap],i*24) for i in range(2)]
            if (entries[2]!=(0,0,0) if not heap_as else not entries[2][0] or entries[2][1:]!=(0,0)) or samplers[0]!=(0,0,0) or entries[1][0] or not entries[1][1] or entries[1][2] or not samplers[1][0] or any(samplers[1][1:]):raise ValueError('heap holes or texture/sampler entry differs')
            raw_va=entries[0][0]-4
            if entries[0][1:]!=(0,4) or entries[3]!=(raw_va+12,0,4) or len(by_address[raw_va])!=32 or struct.unpack_from('<I',by_address[raw_va],4)[0]!=17 or struct.unpack_from('<I',by_address[raw_va],12)[0]!=31:raise ValueError('heap SRV views or metadata differ')
            if heap_as:
                h=by_address[entries[2][0]];heap_id,heap_contribution_va=struct.unpack_from('<QQ',h)
                if len(h)!=64 or heap_id not in structures or heap_id==as_id or heap_contribution_va==contribution_va or any(h[16:]) or by_address[heap_contribution_va]!=bytes(8):raise ValueError('heap AS is not distinct valid header/TLAS/contributions')
            heap_sources=dict(resource_heap=resource_heap,sampler_heap=sampler_heap,buffer=raw_va,entries=entries,samplers=samplers)
        indirect_evidence = {}
        if indirect:
            selected_header=by_address[entries[2][0]] if heap_as else header
            selected_as=struct.unpack_from('<Q',selected_header)[0]
            gpu_ids={value(c,'resource'):value(c,'value') for c in chunks if c.get('name')=='MTLAccelerationStructure::CaptureGPUIdentity'}
            selected_id=next(id for id,gpu in gpu_ids.items() if gpu==selected_as)
            recipe=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and c.find('./uint[@name="kind"]') is not None and value(c,'id')==selected_id)
            kind=value(recipe,'kind');expected_kind=10 if '-empty' in mode else 11 if '-masked' in mode else 9
            parameters=[int(v.text) for v in recipe.find('./array[@name="parameters"]')]
            children=[int(v.text) for v in recipe.find('./array[@name="children"]')]
            raw=data(recipe,'vertices');expected_count=0 if expected_kind==10 else 1
            if kind!=expected_kind or parameters!=[8,80,3,expected_count,0,0,0,0] or len(raw)!=expected_count*72:raise ValueError('frozen indirect recipe shape differs')
            if expected_count:
                mask,ift,user_id,child_gpu=struct.unpack_from('<IIIQ',raw,52)
                x=struct.unpack_from('<f',raw,36)[0]
                if user_id!=73 or ift or x!=(100 if heap_as else 0):raise ValueError('indirect UserID/transform/IFT differs')
                if kind==9:
                    if len(children)!=1 or gpu_ids.get(children[0])!=child_gpu or mask!=255:raise ValueError('typed indirect child identity differs')
                elif children or child_gpu or mask:raise ValueError('all-masked no-child packet differs')
            elif children:raise ValueError('empty TLAS has child references')
            indirect_evidence=dict(kind=kind,parameters=parameters,children=children,packed_bytes=len(raw),UserID=73 if expected_count else 'NOT READ',input='Private/GPU cleared after build' if '-private' in mode else 'Shared')
        frame_evidence={}
        if frame:
            before_id=next(id for id in packets if labels.get(id)=='IR before dispatch packet')
            before=sources[before_id];before_grs=struct.unpack_from('<Q',before,104)[0]
            if before[:104]!=packet[:104] or before[112:]!=packet[112:] or before_grs==grs_va:
                raise ValueError('before/after packet association differs')
            before_roots=bytearray(by_address[before_grs]);current_roots=bytearray(grs_data)
            output_at=0 if heap_only else 8
            before_va=struct.unpack_from('<Q',before_roots,output_at)[0]
            if before_va==output_va or by_address[before_va]!=struct.pack('<II',7,7):
                raise ValueError('independent before-output initial backing differs')
            before_roots[output_at:output_at+8]=current_roots[output_at:output_at+8]
            if before_roots!=current_roots:raise ValueError('before-root scalar/input association differs')
            builds=[c for c in chunks if c.get('name') in ('MTLAccelerationStructureCommandEncoder::buildIndirectInstances','MTLAccelerationStructureCommandEncoder::buildIndirectInstancesWithScratchOffset')]
            if len(builds)!=1:raise ValueError('frame AS build count differs')
            build=builds[0]
            if 'scratch-offset' not in mode and build.find('./*[@name="scratchOffset"]') is not None:raise ValueError('legacy zero-offset chunk changed wire format')
            if 'scratch-offset' in mode and (build.get('name')!='MTLAccelerationStructureCommandEncoder::buildIndirectInstancesWithScratchOffset' or value(build,'scratchOffset')!=256):raise ValueError('missing typed scratch offset')
            parameters=[int(v.text) for v in build.find('./array[@name="parameters"]')]
            children=[int(v.text) for v in build.find('./array[@name="children"]')]
            raw=data(build,'descriptorBytes')
            if value(build,'structure')!=selected_id or parameters!=[8,80,3,1,0,0,0,0] or len(raw)!=72 or len(children)!=1:
                raise ValueError('frame AS snapshot/target differs')
            mask,ift,user_id,child_gpu=struct.unpack_from('<IIIQ',raw,52)
            if mask!=255 or ift or user_id!=74 or gpu_ids.get(children[0])!=child_gpu:
                raise ValueError('frame AS snapshot UserID/BLAS association differs')
            if 'geometry' in mode:
                multi='multi' in mode
                geometries=[c for c in chunks if c.get('name')=='MTLAccelerationStructureCommandEncoder::'+('buildFrozenMultiIndexed' if multi else 'buildFrozenTriangles')]
                if len(geometries)!=1:raise ValueError('missing unique frozen geometry build')
                g=geometries[0];kind=value(g,'kind');gp=[int(v.text) for v in g.find('./array[@name="parameters"]')]
                indexed='indexed' in mode
                vertex_bytes=data(g,'vertexBytes');index_bytes=data(g,'indexBytes')
                if multi:
                    count=64 if 'multi64' in mode else 2
                    if kind!=8 or len(gp)!=10*count or len(vertex_bytes)!=256 or len(index_bytes)!=64 or value(g,'structure')!=children[0] or value(g,'scratchOffset')!=256:
                        raise ValueError('multi-geometry snapshot/target differs')
                    shift=-100 if heap_as else 100
                    for n in range(count):
                        p=gp[n*10:n*10+10];last=n+1==count
                        if p[:2]!=([160,16] if last else [16,32]) or p[2]!=(30 if last else 31) or p[3:8]!=[1,int(last),0,1,0] or p[8]!= (24 if last else 4) or p[9]!=(1 if last else 0):raise ValueError('multi geometry order/index/IFT/flags differs')
                    for i,expected in enumerate(((-1+shift,-1,0),(1+shift,-1,0),(shift,1,0))):
                        if struct.unpack_from('<fff',vertex_bytes,160+i*16)!=expected:raise ValueError('last geometry frozen vertices differ')
                    if struct.unpack_from('<HHH',index_bytes,4)!=(2,0,1) or struct.unpack_from('<III',index_bytes,24)!=(2,0,1):raise ValueError('mixed index snapshots differ')
                    if struct.unpack_from('<I',vertex_bytes,236)[0]!=0x7fc00000:raise ValueError('unreferenced padding lost')
                else:
                    if value(g,'structure')!=children[0] or kind!=(2 if indexed else 1) or gp[:2]!=[16,16] or gp[3:8]!=[1,0,0,1,0] or value(g,'scratchOffset')!=256 or len(vertex_bytes)!=44:
                        raise ValueError('geometry source/offset/format/stride/target differs')
                    shift=-100 if heap_as else 100
                    for i,expected in enumerate(((-1+shift,-1,0),(1+shift,-1,0),(shift,1,0))):
                        if struct.unpack_from('<fff',vertex_bytes,i*16)!=expected:raise ValueError('GPU geometry frozen vertices differ')
                    if indexed and (gp[8]!=4 or struct.unpack('<III',index_bytes)!=(2,0,1)):raise ValueError('GPU index snapshot differs')
                    if not indexed and (index_bytes or value(g,'indices')):raise ValueError('unindexed geometry has index backing')
                if labels.get(value(g,'vertices'))!='IR geometry vertex source':raise ValueError('geometry source label differs')
            frame_evidence=dict(target=selected_id,source=value(build,'instances'),UserID=74,
                children=children,child_gpu=child_gpu,parameters=parameters,packed_bytes=72,
                before_packet=before_id,independent_GPU_outputs=True)
        return dict(status='PASS CPU ABI EVIDENCE', mode=mode, shader_records=records, shader_indices=[r[1] for r in records],
            frame_evidence=frame_evidence, indirect_evidence=indirect_evidence, local_sources=local_sources, callable='NULL', hit_stride=stride if local else 0, dimensions=[2,1,1], AS_header_size=64,
            replay_permission='NONE', packet_buffer=packet_id, GRS_buffer=grs_id, SBT_buffer=sbt_id, global_sources=global_sources, heap_sources=heap_sources)


if __name__ == '__main__':
    print(json.dumps(audit_capture(Path(sys.argv[1]),sys.argv[2] if len(sys.argv)>2 else "default"),indent=2))
