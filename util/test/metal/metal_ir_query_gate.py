#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded converted DX12 RayQuery baseline and explicit unsupported replay check."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir',type=Path,required=True)
    parser.add_argument('--typed',action='store_true')
    parser.add_argument('--explicit-header',action='store_true')
    parser.add_argument('--annotation-controls',action='store_true')
    parser.add_argument('--dynamic-header',action='store_true')
    parser.add_argument('--reuse-header',action='store_true')
    parser.add_argument('--private-contribution',action='store_true')
    parser.add_argument('--heap-header',action='store_true')
    parser.add_argument('--heap-query',action='store_true')
    parser.add_argument('--heap-frame-header',action='store_true')
    parser.add_argument('--query-indirect-groups',type=int,choices=(1,132))
    parser.add_argument('--query-indirect-offset',type=int,choices=(0,16,52),default=16)
    parser.add_argument('--query-unretained',action='store_true')
    parser.add_argument('--query-same-encoder',action='store_true')
    parser.add_argument('--query-dynamic-outputs',action='store_true',help='Two UAV roles, five textures, slot rebinding and GPU-produced SRV')
    parser.add_argument('--query-texture-dimensions',action='store_true',help='Actual AIR metadata queries before sampling')
    parser.add_argument('--query-read-texture-storage',choices=('shared','private','private-placement'),default='shared')
    parser.add_argument('--query-read-texture-writable',action='store_true')
    parser.add_argument('--query-output-texture',choices=('none','r32uint','rgba32float','bufferuint'),default='none')
    parser.add_argument('--query-output-storage',choices=('shared','private','private-placement'),default='shared')
    parser.add_argument('--cbv-roots',type=int,choices=(5,6))
    parser.add_argument('--static-samplers',action='store_true',help='4/5 CBVs plus the six-entry static sampler table')
    parser.add_argument('--cbv-backing-bytes',type=int,choices=(80,2097152),default=80)
    parser.add_argument('--cbv-frame',action='store_true')
    parser.add_argument('--cbv-storage',choices=('shared','private','private-placement'),default='shared')
    parser.add_argument('--cbv-offset',type=int,choices=(16,64,58624),default=16)
    parser.add_argument('--query-grid-height',type=int,choices=(1,66),default=1)
    parser.add_argument('--placement-input',action='store_true')
    parser.add_argument('--empty-frame-build',action='store_true')
    parser.add_argument('--compact-private-size',action='store_true')
    parser.add_argument('--placement-frame-geometry',action='store_true')
    parser.add_argument('--placement-geometry',choices=('triangle','indexed','multi2','multi64','ue-indexed'))
    parser.add_argument('--contribution-offset',type=int,choices=(0,32,4096),default=0)
    parser.add_argument('--root-offset',type=int,choices=(0,8),default=0)
    parser.add_argument('--mode',choices=('default','indirect-shared','indirect-private','indirect-private-empty','indirect-private-masked',
        'indirect-private-frame','indirect-private-empty-frame','indirect-private-masked-frame',
        'indirect-private-frame-geometry','indirect-private-frame-geometry-indexed',
        'indirect-private-frame-geometry-multi-indexed','indirect-private-frame-geometry-multi64-indexed',
        'indirect-private-frame-new-target'),default='default')
    parser.add_argument('--header-offset',type=int,choices=(0,32,4096,16304),default=0)
    parser.add_argument('--instances',type=int,choices=(1,3,64,65),default=1)
    parser.add_argument('--reject-replay',action='store_true',help='Expect a valid typed capture to exceed the supported query range')
    parser.add_argument('--engine',type=Path,default=Path('/Users/Shared/Epic Games/UE_5.8/Engine'))
    args=parser.parse_args()
    if args.query_output_texture=='bufferuint' and args.query_output_storage=='shared':parser.error('typed buffer view fixture requires Private backing')
    if args.query_texture_dimensions and not args.static_samplers:parser.error('texture dimensions require mixed roots')
    if args.query_dynamic_outputs and not (args.static_samplers and args.query_output_texture=='r32uint' and args.query_indirect_groups==132 and args.query_grid_height==66 and not args.query_same_encoder and not args.empty_frame_build):parser.error('dynamic outputs require R32Uint mixed roots, 132 indirect groups with height66')
    if args.cbv_offset==58624 and args.cbv_backing_bytes!=2097152:parser.error('large CBV offset requires the 2MiB backing')
    if args.query_output_storage!='shared' and not (args.cbv_roots and args.heap_frame_header):parser.error('Private output fixture requires CBV heap-frame query')
    if args.query_output_texture!='none' and not (args.static_samplers and args.heap_frame_header):parser.error('texture UAV fixture requires mixed heap-frame roots')
    if (args.query_read_texture_storage!='shared' or args.query_read_texture_writable) and not args.static_samplers:parser.error('read texture fixture requires mixed roots')
    repo=Path(__file__).resolve().parents[3]
    if args.cbv_backing_bytes!=80 and (not args.cbv_roots or args.cbv_storage=='private-placement'):parser.error('large backing needs Shared or standalone Private CBVs')
    if args.cbv_frame and not (args.cbv_roots and args.static_samplers):parser.error('--cbv-frame requires mixed roots')
    if args.cbv_storage!='shared' and not args.cbv_roots:parser.error('--cbv-storage requires CBV roots')
    if args.static_samplers and not args.cbv_roots:parser.error('--static-samplers requires --cbv-roots')
    if args.query_grid_height!=1 and (not args.cbv_roots or args.query_indirect_groups!=132):parser.error('2D fixture needs CBV roots and 132 total query groups')
    if args.cbv_roots and (not args.heap_frame_header or args.root_offset):parser.error('--cbv-roots requires heap frame Header and inline roots at offset0')
    if args.query_indirect_groups and not args.heap_frame_header:
        parser.error('--query-indirect-groups requires --heap-frame-header')
    if args.query_unretained and not args.query_indirect_groups:parser.error('--query-unretained requires indirect query')
    if args.query_same_encoder and not args.query_indirect_groups:parser.error('--query-same-encoder requires indirect query')
    if args.heap_query and (not args.typed or not args.explicit_header or ('frame' in args.mode and not args.heap_frame_header) or args.annotation_controls or args.reject_replay):
        parser.error('--heap-query requires --typed --explicit-header and a frame-initial query mode')
    if args.heap_frame_header and (not args.heap_query or 'frame' not in args.mode or args.dynamic_header or args.reuse_header or args.header_offset):
        parser.error('--heap-frame-header requires --heap-query frame mode, offset0 and a new Header')
    if args.placement_frame_geometry and (not args.placement_input or "geometry" not in args.mode):parser.error("--placement-frame-geometry requires placement frame geometry mode")
    if args.placement_geometry and (not args.placement_input or "geometry" in args.mode):parser.error("--placement-geometry requires placement frame TLAS without frame geometry")
    if args.empty_frame_build and not args.placement_input:parser.error("--empty-frame-build requires --placement-input")
    if args.placement_input and ("private" not in args.mode or "frame" not in args.mode or not args.typed or not args.explicit_header):
        parser.error("--placement-input requires typed explicit Private frame query")
    if args.reject_replay and (not args.typed or args.instances!=65):parser.error('--reject-replay requires --typed --instances 65')
    work=args.work_dir.resolve();work.mkdir(parents=True,exist_ok=True);engine=args.engine.resolve()
    build=repo/'build-macos-debug';lib=build/'lib/librenderdoc.dylib';cli=build/'bin/renderdoccmd'
    dxc=engine/'Binaries/ThirdParty/ShaderConductor/Mac';ir=engine/'Binaries/ThirdParty/Apple/MetalShaderConverter/Mac'
    includes=engine/'Source/ThirdParty/Apple/MetalShaderConverter/include'
    dxcinc=engine/'Source/ThirdParty/ShaderConductor/ShaderConductor/External/DirectXShaderCompiler/include'
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    m=dict(status='RUNNING',checks=[],backend_sha256=sha(lib),production_RT_enabled=False,
        replay_validated=False,scope='Static Shared converted inline-query roots/header/output' if args.typed else 'Small converted inline-query baseline; replay expected unsupported')
    env=os.environ.copy()
    for key in tuple(env):
        if key.startswith(('RENDERDOC_METAL_','RENDERDOC_IR_QUERY_')) or key=='DYLD_INSERT_LIBRARIES':env.pop(key)
    env['MTL_DEBUG_LAYER']='1'
    m['dynamic_outputs']=args.query_dynamic_outputs
    m['query_texture_dimensions']=args.query_texture_dimensions
    if args.query_dynamic_outputs:env['RENDERDOC_IR_QUERY_DYNAMIC_OUTPUTS']='1'
    m['query_read_texture_storage']=args.query_read_texture_storage
    m['query_read_texture_writable']=args.query_read_texture_writable
    env['RENDERDOC_IR_QUERY_READ_TEXTURE_STORAGE']=args.query_read_texture_storage
    if args.query_read_texture_writable:env['RENDERDOC_IR_QUERY_READ_TEXTURE_WRITABLE']='1'
    m['query_output_texture']=args.query_output_texture
    env['RENDERDOC_IR_QUERY_OUTPUT_TEXTURE']=args.query_output_texture
    m['query_output_storage']=args.query_output_storage
    env['RENDERDOC_IR_QUERY_OUTPUT_STORAGE']=args.query_output_storage
    m['cbv_root_count']=args.cbv_roots or 0;m['cbv_offset']=args.cbv_offset
    if args.cbv_roots:env.update(RENDERDOC_IR_QUERY_CBV_ROOTS=str(args.cbv_roots),RENDERDOC_IR_QUERY_CBV_OFFSET=str(args.cbv_offset),RENDERDOC_IR_QUERY_GRID_HEIGHT=str(args.query_grid_height))
    m['cbv_backing_bytes']=args.cbv_backing_bytes
    if args.cbv_roots:env['RENDERDOC_IR_QUERY_CBV_BACKING']=str(args.cbv_backing_bytes)
    m['frame_CBVs']=args.cbv_frame
    if args.cbv_frame:env['RENDERDOC_IR_QUERY_FRAME_CBV']='1'
    m['cbv_storage']=args.cbv_storage
    if args.cbv_roots:env['RENDERDOC_IR_QUERY_CBV_STORAGE']=args.cbv_storage
    m['static_samplers']=args.static_samplers
    if args.static_samplers:env['RENDERDOC_IR_QUERY_MIXED_ROOTS']='1'
    m['query_grid_height']=args.query_grid_height
    if args.query_indirect_groups:
        env['RENDERDOC_IR_QUERY_INDIRECT_GROUPS']=str(args.query_indirect_groups)
        env['RENDERDOC_IR_QUERY_INDIRECT_OFFSET']=str(args.query_indirect_offset)
        env['RENDERDOC_METAL_CAPTURE_INDIRECT_ARGUMENTS']='1'
    m['query_indirect_groups']=args.query_indirect_groups
    m['query_indirect_offset']=args.query_indirect_offset
    m['query_unretained']=args.query_unretained
    if args.query_unretained:env['RENDERDOC_IR_QUERY_UNRETAINED']='1'
    m['query_same_encoder']=args.query_same_encoder
    if args.query_same_encoder:env['RENDERDOC_IR_QUERY_SAME_ENCODER']='1'
    if args.placement_input:env['RENDERDOC_IR_QUERY_PLACEMENT_INPUT']='1'
    m['placement_input']=args.placement_input
    m['placement_geometry']=args.placement_geometry
    m['placement_frame_geometry']=args.placement_frame_geometry
    if args.placement_frame_geometry:env['RENDERDOC_IR_QUERY_PLACEMENT_FRAME_GEOMETRY']='1'
    if args.placement_geometry:env['RENDERDOC_IR_QUERY_PLACEMENT_GEOMETRY']=args.placement_geometry
    initialGeometryCount=64 if args.placement_geometry=='multi64' else 2 if args.placement_geometry=='multi2' else 1
    initialIndexLength=352 if args.placement_geometry=='ue-indexed' else 64
    m['compact_private_size']=args.compact_private_size
    if args.compact_private_size:env['RENDERDOC_IR_QUERY_COMPACT_PRIVATE_SIZE']='1'
    m['empty_frame_build']=args.empty_frame_build
    if args.empty_frame_build:env['RENDERDOC_IR_QUERY_EMPTY_FRAME_BUILD']='1'
    largePlacementInput=args.placement_input and "geometry" not in args.mode
    inputLength=589824 if largePlacementInput else args.instances*80+8
    inputOffset,inputStride=(0,72) if largePlacementInput else (8,80)
    for flag,key in [(args.dynamic_header,'DYNAMIC_HEADER'),(args.private_contribution,'PRIVATE_CONTRIBUTION'),(args.heap_header,'HEAP_HEADER')]:
        if flag:env['RENDERDOC_IR_QUERY_'+key]='1'
    if args.dynamic_header:assert args.explicit_header and args.typed and 'new-target' in args.mode
    if args.reuse_header:
        assert args.dynamic_header and 'geometry' not in args.mode
        env['RENDERDOC_IR_QUERY_REUSE_HEADER']='1'
    m['reuse_header']=args.reuse_header
    if args.private_contribution or args.heap_header:assert args.explicit_header
    m['dynamic_header']=args.dynamic_header;m['private_contribution']=args.private_contribution;m['heap_header']=args.heap_header
    if args.explicit_header:env['RENDERDOC_IR_QUERY_EXPLICIT_HEADER']='1'
    if args.heap_query:env['RENDERDOC_IR_QUERY_HEAP_QUERY']='1'
    if args.heap_frame_header:env['RENDERDOC_IR_QUERY_HEAP_FRAME_HEADER']='1'
    m['heap_frame_header']=args.heap_frame_header
    m['heap_query']=args.heap_query
    if args.heap_query:m['scope']='Typed initial AS / CPU kind3 descriptor heap query; frame-born headers and frame AS builds remain unsupported'
    if args.heap_frame_header:m['scope']='Frame-born 64-byte Header / CPU kind3 slot / current Private typed TLAS build / heap-query'
    if args.annotation_controls:
        assert args.explicit_header and args.typed
        env['RENDERDOC_IR_QUERY_ANNOTATION_CONTROLS']='1'
    env['RENDERDOC_IR_QUERY_CONTRIBUTION_OFFSET']=str(args.contribution_offset)
    m['explicit_header']=args.explicit_header;m['contribution_offset']=args.contribution_offset
    if args.typed:env['RENDERDOC_IR_QUERY_TYPED']='1'
    env['RENDERDOC_IR_QUERY_ROOT_OFFSET']=str(args.root_offset);m['root_offset']=args.root_offset
    env['RENDERDOC_IR_QUERY_MODE']=args.mode;m['mode']=args.mode
    env['RENDERDOC_IR_QUERY_HEADER_OFFSET']=str(args.header_offset);m['header_offset']=args.header_offset
    env['RENDERDOC_IR_QUERY_INSTANCES']=str(args.instances);m['instances']=args.instances
    assert args.mode!='default' or args.instances==1
    frame='frame' in args.mode
    geometry='geometry' in args.mode
    geometry_count=64 if 'multi64' in args.mode else 2 if 'multi' in args.mode else 1
    hit=0 if args.empty_frame_build else (1250 if geometry else 1000)+17*(74+args.instances-1)+13*((geometry_count if geometry else initialGeometryCount)-1)+(47 if args.placement_geometry=="ue-indexed" else 0) if frame else 0 if 'empty' in args.mode or 'masked' in args.mode else 1000 if args.mode=='default' else 1000+17*(73+args.instances-1)
    if args.cbv_roots and hit:hit+=11+(17 if args.cbv_roots==6 else 0)+(72 if args.static_samplers else 0)
    if args.query_output_texture=='bufferuint' and hit:hit+=5
    m['expected_results']=[hit,hit,0,0]
    def save():(work/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    def run(tag,command,extra=None,expected=0,seconds=60):
        e=dict(env,**(extra or {}));e['RENDERDOC_DEBUG_LOG_FILE']=str(work/(tag+'-renderdoc.log'))
        with (work/(tag+'-renderdoc.log')).open('a') as keep,(work/(tag+'.log')).open('w') as log:
            fcntl.flock(keep,fcntl.LOCK_SH)
            p=subprocess.Popen(list(map(str,command)),env=e,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            try:c=p.wait(timeout=seconds)
            except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();c=124
        text=(work/(tag+'.log')).read_text(errors='replace');diagnostics=text+(work/(tag+'-renderdoc.log')).read_text(errors='replace')
        hits=[x for x in ['Assertion failed','failed assertion','OVERRUNNING CHUNK','File and decompress stream readers do not support seeking','Unexpected Metal resource type','m_ResourceMap.empty'] if x in diagnostics]
        if expected and 'Metal replay wait begin' in diagnostics:hits.append('Metal replay wait begin')
        ok=c==expected and not hits;m['checks'].append(dict(tag=tag,exit=c,passed=ok,diagnostic_hits=hits));save();print(tag,c,flush=True)
        if not ok:raise RuntimeError(tag+diagnostics[-2500:])
        return text
    try:
        for name in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',name],stdout=subprocess.DEVNULL).returncode!=0
        run('convert-build',['clang++','-std=c++17','-I'+str(dxcinc),'-I'+str(includes),repo/'util/test/metal/metal_ir_ray_convert.cpp',
            '-L'+str(dxc),'-ldxcompiler','-L'+str(ir),'-lmetalirconverter','-Wl,-rpath,'+str(dxc),'-Wl,-rpath,'+str(ir),'-o',work/'convert'])
        run('convert',[work/'convert',repo/'util/test/metal'/('metal_ir_query_mixed_shaders.hlsl' if args.static_samplers else 'metal_ir_query_cbv_shaders.hlsl' if args.cbv_roots else 'metal_ir_query_shaders.hlsl'),work,
            ('inline-query-descriptor-heaps-heap-as-only'+('-cbv'+str(args.cbv_roots)+('-mixed-static' if args.static_samplers else '')+('-texture-uav'+('-float4' if args.query_output_texture=='rgba32float' else '-bufferuint' if args.query_output_texture=='bufferuint' else '') if args.query_output_texture!='none' else '')+('-dynamic-uav' if args.query_dynamic_outputs else '')+('-texture-dimensions' if args.query_texture_dimensions else '') if args.cbv_roots else '')) if args.heap_query else 'inline-query'])
        env['RENDERDOC_IR_QUERY_REFLECTION_PATH']=str(work/'query.reflection.json')
        run('native-build',['clang++','-std=c++17','-fobjc-arc','-I'+str(repo),'-I'+str(includes),repo/'util/test/metal/metal_ir_query_native.mm',
            '-framework','Foundation','-framework','Metal','-framework','QuartzCore','-o',work/'native'])
        for tag,extra,argv in [('native',{},[]),('capture',dict(DYLD_INSERT_LIBRARIES=str(lib),RENDERDOC_METAL_RAYTRACING_PROBE='1'),[work/'query'])]:
            text=run(tag,[work/'native',work/'query.metallib',*argv],extra,seconds=30)
            if args.query_output_storage!='shared' and args.query_output_texture=='none':assert 'PASS Private'+(' placement' if args.query_output_storage=='private-placement' else '')+' query output readback:' in text
            if args.query_dynamic_outputs:assert f'PASS dynamic multi-UAV {args.query_output_storage}: five textures/528 texels each' in text
            elif args.query_output_texture!='none':assert f'PASS texture UAV {args.query_output_texture} {args.query_output_storage} GPU readback:' in text
            assert f'PASS converted RayQuery hit/hit/miss/short={hit},{hit},0,0' in text
            if 'private' in args.mode:assert f'PASS Private/GPU query instance input cleared after AS build: {inputLength} bytes zero' in text
            if args.placement_geometry:assert f'PASS initial Private placement geometry frozen then cleared: 256/{0 if args.placement_geometry=="triangle" else initialIndexLength} bytes, {initialGeometryCount} geometries' in text
            if args.compact_private_size:assert 'PASS Private placement compact size offset256=' in text
            if args.heap_frame_header:assert 'PASS heap-query frame-born Header and Private TLAS build; '+('three nonzero queries on one encoder' if args.query_same_encoder else 'two nonzero queries and one zero-work query' if args.query_indirect_groups else 'one nonzero query') in text
            if args.query_indirect_groups and not args.query_same_encoder:assert f'PASS per-use GPU-written Private query arguments groups=1/{args.query_indirect_groups}/0 offset={args.query_indirect_offset}; all {args.query_indirect_groups*4} query uints verified' in text
            if args.query_same_encoder:assert f'PASS same encoder Private query arguments groups={args.query_indirect_groups}/{args.query_indirect_groups}/{args.query_indirect_groups} offset={args.query_indirect_offset}; all {args.query_indirect_groups*4} query uints verified' in text
            if frame and not args.heap_frame_header:
                beforeHit=0 if 'empty' in args.mode or 'masked' in args.mode else 1000+17*(73+args.instances-1)
                assert f'PASS frame query before={beforeHit}/{beforeHit}/0/0 after={hit}/{hit}/0/0; rebuilt from GPU/Private input' in text
            if args.header_offset:assert f'PASS bounded Shared header offset={args.header_offset} length={args.header_offset+80}; padding unchanged' in text
            if geometry:assert f'PASS Private/GPU query frame geometry cleared after BLAS: {256 if geometry_count>1 else 72} vertex/{64 if geometry_count>1 else 24 if "indexed" in args.mode else 0} index bytes; geometries={geometry_count}' in text
        if args.annotation_controls:
            assert 'PASS explicit header annotation controls 12 rejected, duplicate immutable declaration accepted' in (work/'capture.log').read_text()
        cap=work/'query_capture.rdc';assert cap.exists()
        run('export',[cli,'convert','-f',cap,'-o',work/'query.zip.xml','-c','zip.xml'])
        if args.typed and not args.reject_replay:
            run('replay-build',['clang++','-std=c++17','-fobjc-arc','-DRENDERDOC_PLATFORM_APPLE','-I'+str(repo),
                repo/'util/test/metal'/('metal_ir_compute_multi_uav_replay.mm' if args.query_dynamic_outputs else 'metal_ir_query_heap_frame_replay.mm' if args.heap_frame_header else 'metal_ir_query_heap_replay.mm' if args.heap_query else 'metal_ir_query_replay.mm'),'-framework','Foundation','-framework','Metal',
                '-L'+str(build/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(build/'lib'),'-o',work/'replay'])
            run('API',[work/'replay',cap,*([hit]+([args.query_indirect_groups]+([1] if args.query_same_encoder else []) if args.query_indirect_groups else []) if args.heap_query else [args.mode+('-dynamic-header' if args.dynamic_header else ''),args.instances])]);run('CLI',[cli,'replay','--loops','3',cap])
            m['replay_validated']=True
            if args.reuse_header:
                second=work/'query_capture_2.rdc';assert second.exists()
                assert 'PASS reused same header background refresh and second capture' in (work/'capture.log').read_text()
                run('reused-API',[work/'replay',second,args.mode+'-dynamic-header-reused',args.instances]);run('reused-CLI',[cli,'replay','--loops','3',second])
                m['second_capture_sha256']=sha(second)
        else:
            run('unsupported-API',[build/'metal-ray-b520/final-heaps/opener',cap],dict(RENDERDOC_METAL_TRACE_REPLAY_WAITS='1'),expected=4)
            run('unsupported-CLI',[cli,'replay','--loops','1',cap],dict(RENDERDOC_METAL_TRACE_REPLAY_WAITS='1'),expected=1)
        sys.path.insert(0,str(repo/'util/shader_tools'));from metal_air_processor import disassemble
        sys.path.insert(0,str(repo/'util/ue'));from audit_ue_metal_ray_air import query_calls
        name=next(line.split('=',1)[1] for line in (work/'native.log').read_text().splitlines() if line.startswith('Query function='))
        air=disassemble(work/'query.metallib',name);(work/'query.ll').write_text(air)
        calls=query_calls(air);assert calls
        if args.query_texture_dimensions:
            dimensions=[line for line in air.splitlines() if 'call ' in line and
                        ('@air.get_width_texture_2d' in line or '@air.get_height_texture_2d' in line)]
            assert len(dimensions)==2;m['actual_texture_dimension_calls']=len(dimensions)
        m['ray_query_calls']=calls;m['reflection']=json.loads((work/'query.reflection.json').read_text())
        assert m['reflection']['UsesRayQuery'] and m['reflection']['ShaderType']=='Compute'
        assert [(e['EltOffset'],e['Size'],e['Type']) for e in m['reflection']['TopLevelArgumentBuffer']]==([(i*8,8,'CBV') for i in range(args.cbv_roots-1)]+[((args.cbv_roots-1)*8,8,'Table')] if args.static_samplers else [(i*8,8,'CBV') for i in range(args.cbv_roots)] if args.cbv_roots else [(0,8,'UAV'),(8,8,'Constant')] if args.heap_query else [(0,8,'SRV'),(8,8,'UAV')])
        chunks=ET.parse(work/'query.zip.xml').find('./chunks')
        compilerFacts=[c for c in chunks if c.get('name')=='MTLComputePipelineState::CaptureIRComputeReflection']
        assert len(compilerFacts)==1
        capturedReflection=json.loads(compilerFacts[0].find('./*[@name="reflection"]').text)
        assert capturedReflection==m['reflection']
        m['immutable_compiler_reflection_captured']=True
        cbvDeclarations=[c for c in chunks if c.get('name')=='MTLComputePipelineState::DeclareRayQueryHeapCBVRoot']
        assert len(cbvDeclarations)==(args.cbv_roots or 0)
        m['captured_CBVs']=len(cbvDeclarations)
        if args.static_samplers:
            samples=[line for line in air.splitlines() if 'call ' in line and '@air.sample_texture_2d.' in line]
            assert len(samples)==6
            assert {r['tableStartIndex'] for r in m['reflection']['UsedResources'] if r['tableStartIndex']!=4294967295}==set(range(6))
            m['actual_texture_sample_calls']=len(samples)
            assert len([c for c in chunks if c.get('name')=='MTLComputePipelineState::DeclareIRComputeRoot'])==args.cbv_roots
            heap_entries=[c for c in chunks if c.get('name')=='MTLComputePipelineState::DeclareIRComputeHeapEntry']
            assert sorted(int(c.find('./*[@name="kind"]').text) for c in heap_entries)==([1,8,8] if args.query_dynamic_outputs else [1,1,4] if args.query_output_texture=='bufferuint' else [1,4] if args.query_output_texture!='none' else [1])
            if args.query_output_texture!='none':
                writes=[line for line in air.splitlines() if 'call ' in line and ('@air.write_texture_buffer' if args.query_output_texture=='bufferuint' else '@air.write_texture_2d.') in line]
                assert len(writes)==(2 if args.query_dynamic_outputs else 1)
                m['actual_texture_write_calls']=len(writes)
                if args.query_output_texture=='bufferuint':
                    reads=[line for line in air.splitlines() if 'call ' in line and '@air.read_texture_buffer' in line]
                    sizes=[line for line in air.splitlines() if 'call ' in line and '@air.get_width_texture_buffer' in line]
                    assert len(reads)==1 and len(sizes)==1
                    m['actual_texel_read_calls']=1;m['actual_texel_width_calls']=1
        declarations=[c for c in chunks if c.get('name')=='MTLBuffer::DeclareRayASHeader']
        assert len(declarations)==(1 if args.heap_frame_header else (2 if 'new-target' in args.mode else 1) if args.explicit_header else 0)
        for declaration in declarations:
            assert int(declaration.find('./*[@name="offset"]').text)==args.header_offset
            assert int(declaration.find('./*[@name="contributionOffset"]').text)==args.contribution_offset+(16 if args.dynamic_header and declaration is declarations[-1] else 0)
            assert int(declaration.find('./*[@name="bytes"]').get('byteLength'))==64
        m['typed_header_declarations']=len(declarations)
        if args.mode!='default' and not (args.heap_frame_header and 'new-target' in args.mode):
            recipes=[c for c in chunks if c.get('name')=='Internal::Initial Contents' and
                c.find('./*[@name="kind"]') is not None and int(c.find('./*[@name="kind"]').text) in (9,10,11)]
            assert len(recipes)==1
            recipe=recipes[0];kind=int(recipe.find('./*[@name="kind"]').text)
            expected_kind=10 if 'empty' in args.mode else 11 if 'masked' in args.mode else 9
            assert kind==expected_kind
            parameters=[int(n.text) for n in recipe.find('./*[@name="parameters"]')]
            assert parameters==[inputOffset,inputStride,3,0 if kind==10 else args.instances,0,0,0,0]
            raw=recipe.find('./*[@name="vertices"]')
            assert int(raw.get('byteLength'))==(0 if kind==10 else args.instances*72)
            m['captured_initial_TLAS_kind']=kind;m['captured_initial_TLAS_parameters']=parameters
        if args.placement_geometry:
            kind=8 if initialGeometryCount>1 else 2 if args.placement_geometry and args.placement_geometry!='triangle' else 1
            primitives=[c for c in chunks if c.get('name')=='Internal::Initial Contents' and
                c.find('./*[@name="kind"]') is not None and int(c.find('./*[@name="kind"]').text)==kind]
            assert len(primitives)==1
            frozen=primitives[0];allocations={c.find('./*[@name="Buffer"]').text:c for c in chunks if c.get('name')=='MTLHeap::newBuffer(offset)'}
            with zipfile.ZipFile(work/'query.zip') as z:
                for field,length in [('source',256)]+([('indexSource',initialIndexLength)] if kind in (2,8) else []):
                    id=frozen.find('./*[@name="'+field+'"]').text;a=allocations[id]
                    assert int(a.find('./*[@name="length"]').text)==length and int(a.find('./*[@name="options"]').text)==544
                    initial=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and c.find('./*[@name="id"]').text==id)
                    assert z.read(f"{int(initial.find('./*[@name="Contents"]').text):06}")==bytes(length)
            if args.placement_geometry=='ue-indexed':
                assert [int(n.text) for n in frozen.find('./*[@name="parameters"]')]==[0,12,30,48,0,0,1,0,0,0]
                assert int(frozen.find('./*[@name="vertices"]').get('byteLength'))==72
                assert int(frozen.find('./*[@name="indices"]').get('byteLength'))==288
            m['verified_heap_BLAS_kind']=kind;m['verified_heap_BLAS_geometry_count']=initialGeometryCount
        if args.compact_private_size:
            compactRecipes=[c for c in chunks if c.get('name')=='Internal::Initial Contents' and
                c.find('./*[@name="kind"]') is not None and c.find('./*[@name="kind"]').text==('8' if initialGeometryCount>1 else '2' if args.placement_geometry and args.placement_geometry!='triangle' else '1') and
                c.find('./*[@name="compacted"]').text=='true']
            assert len(compactRecipes)==1
            m['verified_compacted_BLAS_initial_recipe']=compactRecipes[0].find('./*[@name="id"]').text
        with zipfile.ZipFile(work/'query.zip') as archive:
            library_matches=[c for c in chunks if c.get('name') in ('MTLDevice::newLibraryWithData','MTLDevice::newLibraryWithURL') and
                hashlib.sha256(archive.read(f"{int(c.find('./*[@name=\"data\"]').text):06}")).hexdigest()==sha(work/'query.metallib')]
        assert len(library_matches)==1
        dispatches=[c for c in chunks if c.get('name')==('MTLComputeCommandEncoder::dispatchThreadgroups(indirect)' if args.query_indirect_groups else 'MTLComputeCommandEncoder::dispatchThreadgroups')]
        assert len(dispatches)==(3 if args.query_indirect_groups else 2 if frame and not args.heap_frame_header else 1)
        if args.query_indirect_groups:
            proof=[c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::CaptureIndirectArguments']
            assert len(proof)==3
            f=lambda c,n:c.find('./*[@name="'+n+'"]')
            records={(f(c,'encoder').text,int(f(c,'ordinal').text)):c for c in proof}
            assert len(records)==3
            buffer=f(proof[0],'buffer').text
            queryWidths=[args.query_indirect_groups]*3 if args.query_same_encoder else [1,args.query_indirect_groups,0]
            for ordinal,(dispatch,width) in enumerate(zip(dispatches,queryWidths)):
                record=records[(f(dispatch,'ComputeCommandEncoder').text,ordinal if args.query_same_encoder else 0)]
                assert f(record,'buffer').text==f(dispatch,'indirectBuffer').text==buffer
                assert int(f(record,'offset').text)==int(f(dispatch,'indirectBufferOffset').text)==args.query_indirect_offset
                assert [int(x.text) for x in f(record,'groups')]==([width//args.query_grid_height,args.query_grid_height,1] if width!=1 else [1,1,1])
            allocation=next(c for c in chunks if c.get('name')=='MTLHeap::newBuffer(offset)' and f(c,'Buffer').text==buffer)
            assert int(f(allocation,'length').text)==64 and int(f(allocation,'options').text)==544
            m['verified_per_use_indirect_query_groups']=[[n//args.query_grid_height,args.query_grid_height,1] if n!=1 else [1,1,1] for n in queryWidths]
        for dispatch in dispatches:
            if not args.query_indirect_groups:assert [int(n.text) for n in dispatch.find('./*[@name="groups"]')]==[1,1,1]
            assert [int(n.text) for n in dispatch.find('./*[@name="threadsPerGroup"]')]==[4,1,1]
        if frame:
            builds=[c for c in chunks if c.get('name')=='MTLAccelerationStructureCommandEncoder::buildIndirectInstances']
            assert len(builds)==1
            assert [int(n.text) for n in builds[0].find('./*[@name="parameters"]')]==[inputOffset,inputStride,3,0 if args.empty_frame_build else args.instances,0,0,0,0]
            assert int(builds[0].find('./*[@name="descriptorBytes"]').get('byteLength'))==(0 if args.empty_frame_build else args.instances*72)
            m['captured_frame_TLAS_builds']=1
            if args.placement_input:
                allocations={c.find('./*[@name="Buffer"]').text:c for c in chunks if c.get('name')=='MTLHeap::newBuffer(offset)'}
                heaps={c.find('./*[@name="Heap"]').text:c for c in chunks if c.get('name')=='MTLDevice::newHeapWithDescriptor'}
                for field,length in [('instances',inputLength),('scratch',1376256 if largePlacementInput else None)]:
                    allocation=allocations[builds[0].find('./*[@name="'+field+'"]').text]
                    actualLength=int(allocation.find('./*[@name="length"]').text)
                    assert actualLength==length if length is not None else 0<actualLength<=65536
                    assert int(allocation.find('./*[@name="options"]').text)==544
                    parent=heaps[allocation.find('./*[@name="Heap"]').text]
                    assert int(parent.find('./*[@name="hazardMode"]').text)==2
                    assert int(parent.find('./*[@name="type"]').text)==1
                m['verified_Private_tracked_placement_sizes']=[inputLength,actualLength]

            if 'new-target' in args.mode:
                target=builds[0].find('./*[@name="structure"]').text
                if not args.heap_frame_header:
                    initialID=recipe.find('./*[@name="id"]').text
                    assert target!=initialID
                assert not any(c.get('name')=='Internal::Initial Contents' and c.find('./*[@name="id"]').text==target for c in chunks)
                m['new_TLAS_target_without_initial_recipe']=target
                assert all('PASS new TLAS target old=' in (work/(tag+'.log')).read_text() for tag in ('native','capture'))
            if geometry:
                name='MTLAccelerationStructureCommandEncoder::'+('buildFrozenMultiIndexed' if geometry_count>1 else 'buildFrozenTriangles')
                geometries=[c for c in chunks if c.get('name')==name];assert len(geometries)==1
                build=geometries[0];assert int(build.find('./*[@name="kind"]').text)==(8 if geometry_count>1 else 2 if 'indexed' in args.mode else 1)
                assert int(build.find('./*[@name="scratchOffset"]').text)==256
                assert len(build.find('./*[@name="parameters"]'))==(geometry_count*10 if geometry_count>1 else 10 if 'indexed' in args.mode else 8)
                m['captured_frame_geometry_builds']=1;m['geometry_count']=geometry_count
                if args.placement_frame_geometry:
                    allocations={c.find('./*[@name="Buffer"]').text:c for c in chunks if c.get('name')=='MTLHeap::newBuffer(offset)'}
                    for field in ['vertices','scratch']+(['indices'] if 'indexed' in args.mode else []):
                        allocation=allocations[build.find('./*[@name="'+field+'"]').text]
                        assert int(allocation.find('./*[@name="options"]').text)==544
                    m['verified_frame_geometry_Private_placement']=True

        m['captured_library_matches_native']=True;m['captured_nonzero_query_dispatches']=3 if args.query_same_encoder else 2 if args.query_indirect_groups else len(dispatches)
        m['hashes']={str(p):sha(p) for p in [lib,ir/'libmetalirconverter.dylib',dxc/'libdxcompiler.dylib',
            engine/'Build/Build.version',includes/'metal_irconverter_runtime/metal_irconverter_runtime.h',
            includes/'metal_irconverter_runtime/ir_raytracing.h',
            repo/'util/test/metal/metal_ir_ray_convert.cpp',repo/'util/test/metal/metal_ir_query_shaders.hlsl',repo/'util/test/metal/metal_ir_query_cbv_shaders.hlsl',repo/'util/test/metal/metal_ir_query_mixed_shaders.hlsl',
            repo/'util/test/metal/metal_ir_query_native.mm',repo/'util/test/metal/metal_ir_query_replay.mm',
            repo/'util/test/metal/metal_ir_query_heap_replay.mm',repo/'util/test/metal/metal_ir_query_heap_frame_replay.mm',
            repo/'util/test/metal/metal_ir_compute_multi_uav_replay.mm',
            Path(__file__),work/'pipeline.dxil',work/'query.metallib',cap,work/'native',work/'convert']}
        m['engine_version']=json.loads((engine/'Build/Build.version').read_text())
        if args.typed and not args.reject_replay:m['hashes'][str(work/'replay')]=sha(work/'replay')
        assert sha(lib)==m['backend_sha256'];m['status']='NATIVE/CAPTURE/REPLAY PASS' if args.typed and not args.reject_replay else 'NATIVE/CAPTURE PASS; REPLAY UNSUPPORTED';save()
        return 0
    except Exception as ex:m.update(status='FAIL',error=str(ex));save();raise


if __name__=='__main__':
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX);sys.exit(main())
