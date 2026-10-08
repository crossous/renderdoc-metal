#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Native compiled MSL/runtime-root ABI, actual typed heap accesses, and pre-GPU corruption checks."""
import argparse,copy,fcntl,hashlib,json,os,re,signal,struct,subprocess,tempfile,zipfile
from pathlib import Path
import xml.etree.ElementTree as ET

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--work-dir',type=Path,required=True);p.add_argument('--ray-library',type=Path,required=True,help='Known converted RayQuery main metallib, used only as a pre-GPU rejected runtime consumer');p.add_argument('--copy-chain',choices=['direct','background','frame','separate','opaque'],default='direct');p.add_argument('--scalar-root-bytes',type=int,default=0);p.add_argument('--scalar-root-offset',type=int,default=0);p.add_argument('--loop',action='store_true');p.add_argument('--group-builtins',action='store_true');p.add_argument('--republish-gpu-descriptors',action='store_true',help='Replace background GPU identities with actual sourced frame GPU copies');p.add_argument('--background-gpu-descriptors',action='store_true',help='Real GPU-owned descriptor initial state, without frame descriptor copies');p.add_argument('--gpu-descriptors',action='store_true');p.add_argument('--scalar-counter',action='store_true');p.add_argument('--guarded-index',action='store_true');p.add_argument('--nullable-source',action='store_true',help='Native nullable source selection before a GPU-generated relative index');p.add_argument('--native-sampler-heap',action='store_true',help='Actual Native sampler heap binding and two per-dispatch gather slots');p.add_argument('--legacy-register-effects',action='store_true',help='Reassemble Native register declarations with legacy nounwind-only effects; instruction bodies unchanged');p.add_argument('--native-ray-query',action='store_true',help='Independent Native intersection-query shader with public AS header; no explicit query-dispatch declaration');p.add_argument('--query-offset',type=int,default=0);p.add_argument('--query-instances',type=int,choices=[1,2,3],default=1);p.add_argument('--query-pointer-branches',action='store_true');p.add_argument('--query-register-api',action='store_true');p.add_argument('--query-dynamic-heap',action='store_true');p.add_argument('--query-null-rows',type=int,default=0);p.add_argument('--query-null-gpu-publish',action='store_true');p.add_argument('--query-null-opaque-writer',action='store_true');p.add_argument('--query-group-indices',action='store_true');p.add_argument('--query-cold-rows',type=int,default=0);p.add_argument('--query-retired-row',action='store_true',help='Background retired generation retains stale raw address; actual query never accesses it');p.add_argument('--query-partial-alias',action='store_true');p.add_argument('--query-alias-offset',type=int,default=256);p.add_argument('--query-partial-heap',action='store_true');p.add_argument('--capture-without-coverage',action='store_true',help='Omit application coverage annotation; require backend protocol metadata and full unmodified capture GPU replay');p.add_argument('--query-birth-pending',action='store_true',help='Finite disjoint GPU prefix left in flight at real heap-buffer birth');p.add_argument('--query-birth-prefix',action='store_true',help='Completed disjoint same-heap command before new logical birth');p.add_argument('--positive-only',action='store_true',help='Directed Native/capture/API/CLI and event checks; no unrelated corruption matrix');p.add_argument('--query-creation-input',choices=['zero-private','zero-shared','bytes-shared','heap-birth']);p.add_argument('--query-partial-gpu-producer',action='store_true');p.add_argument('--query-producer-slot',type=int,default=2);p.add_argument('--query-partial-gpu-same-submit',action='store_true');p.add_argument('--query-partial-offset',type=int,default=64);p.add_argument('--query-partial-size',type=int,default=1024);p.add_argument('--query-active-instance',type=int,default=0);p.add_argument('--query-header-offset',type=int,default=0);p.add_argument('--query-contribution-offset',type=int,default=0);p.add_argument('--threadgroup-atomics',action='store_true',help='Native threadgroup allocator; local memory is not a captured buffer');p.add_argument('--read-modify-write',action='store_true',help='Actual same-dispatch scalar read/modify/write; CPU facts must be invalidated');p.add_argument('--rmw-offset',type=int,default=0);p.add_argument('--register-bit-effects',action='store_true',help='Native integer register intrinsics affect dynamic output indexing; CPU values remain unknown');p.add_argument('--native-lanes',action='store_true',help='Native SIMD collectives, without CPU lane-result evaluation');p.add_argument('--partial-view-init',action='store_true',help='Initialize only four texels; preserve undefined unused texels until Native writes');p.add_argument('--pixel-reader',action='store_true',help='Read restored texel pixels before their Native writes');p.add_argument('--view-shared-root',action='store_true',help='Share one backing allocation between disjoint root bytes and texel view');p.add_argument('--texture-buffer-view',action='store_true',help='Frame-born Private texel-buffer view, metadata and Native pixel writes');p.add_argument('--texture-producer-submit',action='store_true',help='Commit the actual texture producer before its consumer');p.add_argument('--texture-atomics',action='store_true',help='Native texture atomic store producer followed by load/RMW dependency');p.add_argument('--texture-dimensions',action='store_true',help='Frame-born Private texture metadata query without pixel initialization');p.add_argument('--literal-projection',action='store_true',help='Native consecutive writes with exact bytecode-derived fallback address spans');p.add_argument('--call-effects',action='store_true',help='Native declared memory effects without CPU function result simulation');p.add_argument('--pointer-select',action='store_true',help='Keep Native conditional pointer selection without CPU predicate evaluation');p.add_argument('--restoration-display',action='store_true',help='Verify legal Native dynamic offset/conditional coverage separately from access display; run structural corruptions only');p.add_argument('--scalar-control',choices=['conditional','overwrite']);p.add_argument('--buffer-types',action='store_true');p.add_argument('--query-contribution-producer',action='store_true');p.add_argument('--contribution-baseline',action='store_true');args=p.parse_args()
 if args.scalar_root_bytes and (args.scalar_root_bytes>48*1024*1024 or args.scalar_root_offset<0 or args.scalar_root_offset%4 or args.scalar_root_offset+24>args.scalar_root_bytes or not args.positive_only or args.copy_chain!='direct' or args.native_ray_query):p.error('Scalar interval fixture requires a bounded direct ordinary root')
 if args.query_contribution_producer and (not args.native_ray_query or not args.positive_only or args.query_partial_gpu_producer or args.query_partial_heap):p.error('Contribution producer uses directed Native query fixture')
 if args.contribution_baseline and not args.query_contribution_producer:p.error('Contribution baseline needs its directed fixture')
 if args.native_ray_query and any([args.loop,args.scalar_counter,args.guarded_index,args.call_effects,args.texture_dimensions,args.read_modify_write]):p.error('Native query uses its own shader')
 if (args.query_offset or args.query_header_offset or args.query_contribution_offset or args.query_instances!=1) and not args.native_ray_query:p.error('Query variants require Native query')
 if not 0<=args.query_active_instance<args.query_instances:p.error('Active instance outside fixture')
 if args.query_dynamic_heap and not args.native_ray_query:p.error('Dynamic query heap needs Native query')
 if args.query_null_rows and (not args.query_dynamic_heap or args.query_partial_heap or not 1<=args.query_null_rows<=4096):p.error('Null rows require dynamic query and its own fixture')
 if args.query_null_opaque_writer and (not args.query_null_rows or not args.gpu_descriptors or args.query_null_gpu_publish):p.error('Opaque null writer requires its own GPU table fixture')
 if args.query_null_gpu_publish and (not args.query_null_rows or not args.gpu_descriptors):p.error('Null GPU publication needs null rows and GPU descriptors')
 if args.query_partial_heap and (not args.query_dynamic_heap or args.query_partial_offset%4 or args.query_partial_offset<0 or args.query_partial_size<4 or args.query_partial_size>2097152 or args.query_partial_offset>args.query_partial_size-4):p.error('Partial heap requires dynamic query and a valid 4-byte Native upload')
 if args.query_retired_row and (not args.native_ray_query or not args.query_dynamic_heap or not args.query_partial_heap):p.error('Retired row requires the Native dynamic heap query fixture')
 if (args.query_birth_prefix or args.query_birth_pending) and args.query_creation_input!='heap-birth':p.error('Birth prefix requires raw heap birth fixture')
 if args.query_creation_input and (not args.query_partial_heap or args.query_partial_alias or args.query_partial_gpu_producer or args.query_group_indices):p.error('Creation input requires its own Native partial-buffer query')
 if args.query_partial_alias and (not args.query_partial_heap or args.query_partial_gpu_same_submit or args.query_alias_offset<256 or args.query_alias_offset%256):p.error('Alias fixture needs partial upload and aligned backing offset')
 if args.query_group_indices and (not args.query_partial_heap or args.query_null_rows):p.error('Group index fixture requires partial query heap')
 if args.query_cold_rows and ((not args.query_group_indices and args.query_creation_input!='heap-birth') or not 1<=args.query_cold_rows<=4096):p.error('Cold rows require group indices')
 if args.query_partial_gpu_same_submit and not (args.query_partial_gpu_producer or args.query_contribution_producer):p.error('Same-submit requires GPU partial producer')
 if not 0<=args.query_producer_slot<31:p.error('Producer binding slot outside Native API')
 if args.query_partial_gpu_producer and not args.query_partial_heap:p.error('GPU partial producer requires partial heap fixture')
 if args.query_register_api and not args.native_ray_query:p.error('Query API fixture requires Native query')
 if args.query_pointer_branches and (not args.native_ray_query or args.query_instances<2):p.error('Pointer branch fixture needs Native query and two metadata elements')
 if not 0<=args.query_header_offset<=128 or args.query_header_offset%8:p.error('Invalid header offset')
 if not 0<=args.query_contribution_offset<=64 or args.query_contribution_offset%4:p.error('Invalid contribution offset')
 if not 0<=args.query_offset<=32:p.error('Query offset outside fixture')
 if args.threadgroup_atomics and not args.call_effects:p.error('Threadgroup atomics requires Native call fixture')
 if args.read_modify_write and (not args.call_effects or not args.pointer_select or not args.restoration_display):p.error('RMW requires call effects, pointer selection and restoration/display')
 if args.rmw_offset and not args.read_modify_write:p.error('Offset variant requires RMW')
 if not 0<=args.rmw_offset<=32:p.error('RMW offset outside fixture buffer')
 if args.legacy_register_effects and not args.register_bit_effects:p.error('Legacy effects requires register bit fixture')
 if args.register_bit_effects and not args.native_lanes:p.error('Register effects uses actual Native lane/counter inputs')
 if args.native_sampler_heap and not args.texture_atomics:p.error('Native sampler heap uses the real texture producer')
 if args.republish_gpu_descriptors and not args.background_gpu_descriptors:p.error('Republishing requires background GPU initial state')
 if args.background_gpu_descriptors and not args.gpu_descriptors:p.error('Background GPU descriptors requires --gpu-descriptors')
 if args.partial_view_init and (not args.texture_buffer_view or not args.pixel_reader):p.error('Partial view init requires a buffer view pixel reader')
 if args.pixel_reader and not args.texture_buffer_view:p.error('Pixel reader requires buffer view')
 if args.view_shared_root and not args.texture_buffer_view:p.error('Shared root requires buffer view')
 if args.texture_buffer_view and (not args.texture_dimensions or args.texture_atomics):p.error('Buffer view requires dimensions and ordinary writes')
 if args.texture_producer_submit and not args.texture_atomics:p.error('Producer submit requires texture atomics')
 if args.native_lanes and not args.call_effects:p.error('Native lanes uses the call-effects fixture')
 if args.texture_atomics and (not args.texture_dimensions or not args.native_lanes):p.error('Texture atomics requires dimensions and native lanes')
 if args.texture_dimensions and not args.literal_projection:p.error('Texture dimensions uses the literal-projection fixture')
 if args.literal_projection and not args.call_effects:p.error('Literal projection uses the call-effects fixture')
 if args.nullable_source and not args.call_effects:p.error('Nullable source uses the synchronized call-effects fixture')
 if args.call_effects and not args.pointer_select:p.error('Call effects uses the pointer-select fixture')
 if args.pointer_select and not args.restoration_display:p.error('Pointer select uses restoration/display fixture')
 if args.restoration_display and not args.guarded_index:p.error('Restoration/display fixture requires --guarded-index')
 if args.guarded_index and (not args.scalar_counter or args.scalar_control):p.error('Guarded index uses the exact scalar producer')
 if args.gpu_descriptors and (args.scalar_control or args.buffer_types or args.copy_chain=='opaque'):p.error('GPU descriptor fixture uses ordinary buffer tags and a supported scalar path')
 if args.scalar_control and not args.scalar_counter:p.error('Scalar controls require --scalar-counter')
 if args.scalar_counter and (args.loop or args.group_builtins or args.copy_chain!='direct'):p.error('Scalar counter uses a separate producer in the direct fixture')
 if args.group_builtins and (args.loop or args.copy_chain=='opaque'):p.error('Group builtin fixture uses ordinary non-opaque kernel')
 if args.loop and args.copy_chain=='opaque':p.error('Opaque writer control uses the ordinary kernel')
 if args.buffer_types and not args.loop:p.error('Buffer tag coverage uses the indexed loop fixture')
 r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';w=args.work_dir.resolve();w.mkdir(parents=True,exist_ok=True)
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();backend=b/'lib/librenderdoc.dylib'
 m=dict(status='RUNNING',backend_sha256=sha(backend),scalar_root_bytes=args.scalar_root_bytes,scalar_root_offset=args.scalar_root_offset,native_ray_query=args.native_ray_query,query_contribution_producer=args.query_contribution_producer,query_offset=args.query_offset,query_instances=args.query_instances,query_pointer_branches=args.query_pointer_branches,query_register_api=args.query_register_api,query_dynamic_heap=args.query_dynamic_heap,query_null_rows=args.query_null_rows,query_null_gpu_publish=args.query_null_gpu_publish,query_null_opaque_writer=args.query_null_opaque_writer,query_partial_alias=args.query_partial_alias,query_alias_offset=args.query_alias_offset,query_group_indices=args.query_group_indices,query_cold_rows=args.query_cold_rows,query_retired_row=args.query_retired_row,query_creation_input=args.query_creation_input,query_birth_prefix=args.query_birth_prefix,query_birth_pending=args.query_birth_pending,positive_only=args.positive_only,query_partial_heap=args.query_partial_heap,query_partial_gpu_producer=args.query_partial_gpu_producer,query_producer_slot=args.query_producer_slot,query_partial_gpu_same_submit=args.query_partial_gpu_same_submit,query_partial_offset=args.query_partial_offset,query_partial_size=args.query_partial_size,query_active_instance=args.query_active_instance,query_header_offset=args.query_header_offset,query_contribution_offset=args.query_contribution_offset,threadgroup_atomics=args.threadgroup_atomics,read_modify_write=args.read_modify_write,rmw_offset=args.rmw_offset,copy_chain=args.copy_chain,scalar_counter=args.scalar_counter,gpu_descriptors=args.gpu_descriptors,background_gpu_descriptors=args.background_gpu_descriptors,republish_gpu_descriptors=args.republish_gpu_descriptors,guarded_index=args.guarded_index,restoration_display=args.restoration_display,pointer_select=args.pointer_select,call_effects=args.call_effects,nullable_source=args.nullable_source,literal_projection=args.literal_projection,texture_dimensions=args.texture_dimensions,texture_buffer_view=args.texture_buffer_view,pixel_reader=args.pixel_reader,partial_view_init=args.partial_view_init,view_shared_root=args.view_shared_root,native_lanes=args.native_lanes,register_bit_effects=args.register_bit_effects,legacy_register_effects=args.legacy_register_effects,native_sampler_heap=args.native_sampler_heap,texture_atomics=args.texture_atomics,texture_producer_submit=args.texture_producer_submit,scalar_control=args.scalar_control,indexed_loop=args.loop,buffer_types=args.buffer_types,scope='B544 generic runtime API development; Native intersection-query' if args.native_ray_query else 'B544 runtime typed-resource development; ordinary MSL with accurately declared runtime-root layout, no RT dispatch, not final UE acceptance',checks=[])
 def run(tag,cmd,expected=0,capture=False,pre_gpu=False):
  e=os.environ.copy()
  for k in tuple(e):
   if k.startswith('RENDERDOC_') or k=='DYLD_INSERT_LIBRARIES':e.pop(k)
  if args.scalar_root_bytes:e.update(RENDERDOC_METAL_RUNTIME_SCALAR_ROOT_BYTES=str(args.scalar_root_bytes),RENDERDOC_METAL_RUNTIME_SCALAR_ROOT_OFFSET=str(args.scalar_root_offset))
  if args.native_sampler_heap:e['RENDERDOC_METAL_RUNTIME_NATIVE_SAMPLER_HEAP']='1'
  if args.partial_view_init:e['RENDERDOC_METAL_RUNTIME_PARTIAL_VIEW_INIT']='1'
  if args.republish_gpu_descriptors:e['RENDERDOC_METAL_RUNTIME_REPUBLISH_GPU_DESCRIPTORS']='1'
  if args.background_gpu_descriptors:e['RENDERDOC_METAL_RUNTIME_BACKGROUND_GPU_DESCRIPTORS']='1'
  if args.view_shared_root:e['RENDERDOC_METAL_RUNTIME_VIEW_SHARED_ROOT']='1'
  if args.texture_buffer_view:e['RENDERDOC_METAL_RUNTIME_TEXTURE_BUFFER_VIEW']='1'
  if args.texture_producer_submit:e['RENDERDOC_METAL_RUNTIME_TEXTURE_PRODUCER_SUBMIT']='1'
  if args.texture_atomics:e['RENDERDOC_METAL_RUNTIME_TEXTURE_ATOMICS']='1'
  if args.texture_dimensions:e['RENDERDOC_METAL_RUNTIME_TEXTURE_DIMENSIONS']='1'
  if args.literal_projection:e['RENDERDOC_METAL_RUNTIME_LITERAL_PROJECTION']='1'
  if args.call_effects:e['RENDERDOC_METAL_RUNTIME_CALL_EFFECTS']='1'
  if args.query_contribution_producer:e['RENDERDOC_METAL_RUNTIME_QUERY_CONTRIBUTION_PRODUCER']='1';e['RENDERDOC_METAL_RUNTIME_QUERY_PRODUCER_SLOT']=str(args.query_producer_slot)
  if args.native_ray_query:
   e['RENDERDOC_METAL_RUNTIME_NATIVE_QUERY']='1'
   e['RENDERDOC_METAL_RUNTIME_QUERY_INSTANCES']=str(args.query_instances)
   e['RENDERDOC_METAL_RUNTIME_QUERY_ACTIVE_INSTANCE']=str(args.query_active_instance)
   if args.query_dynamic_heap:e['RENDERDOC_METAL_RUNTIME_QUERY_DYNAMIC_HEAP']='1'
   if args.query_null_rows:e['RENDERDOC_METAL_RUNTIME_QUERY_NULL_ROWS']=str(args.query_null_rows)
   if args.query_null_gpu_publish:e['RENDERDOC_METAL_RUNTIME_QUERY_NULL_GPU_PUBLISH']='1'
   if args.query_null_opaque_writer:e['RENDERDOC_METAL_RUNTIME_QUERY_NULL_OPAQUE_WRITER']='1'
   if args.query_creation_input:e['RENDERDOC_METAL_RUNTIME_QUERY_CREATION_INPUT']=str(['zero-private','zero-shared','bytes-shared','heap-birth'].index(args.query_creation_input)+1)
   if args.query_partial_alias:e.update(RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_ALIAS='1',RENDERDOC_METAL_RUNTIME_QUERY_ALIAS_OFFSET=str(args.query_alias_offset))
   if args.query_creation_input=='heap-birth':e['RENDERDOC_METAL_RUNTIME_QUERY_ALIAS_OFFSET']=str(args.query_alias_offset)
   if args.query_birth_prefix:e['RENDERDOC_METAL_RUNTIME_QUERY_BIRTH_PREFIX']='1'
   if args.query_birth_pending:e.update(RENDERDOC_METAL_RUNTIME_QUERY_BIRTH_PENDING='1',RENDERDOC_METAL_TRACE_HEAP_BIRTH='1')
   if args.query_retired_row:e['RENDERDOC_METAL_RUNTIME_QUERY_RETIRED_ROW']='1'
   if args.query_group_indices:e['RENDERDOC_METAL_RUNTIME_QUERY_GROUP_INDICES']='1'
   if args.query_cold_rows:e['RENDERDOC_METAL_RUNTIME_QUERY_COLD_ROWS']=str(args.query_cold_rows)
   if args.query_partial_gpu_producer:e.update(RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_GPU_PRODUCER='1',RENDERDOC_METAL_RUNTIME_QUERY_PRODUCER_SLOT=str(args.query_producer_slot))
   if args.query_partial_gpu_same_submit:e['RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_GPU_SAME_SUBMIT']='1'
   if args.query_partial_heap:e.update(RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_HEAP='1',RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_OFFSET=str(args.query_partial_offset),RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_SIZE=str(args.query_partial_size))
   e['RENDERDOC_METAL_RUNTIME_QUERY_HEADER_OFFSET']=str(args.query_header_offset)
   e['RENDERDOC_METAL_RUNTIME_QUERY_CONTRIBUTION_OFFSET']=str(args.query_contribution_offset)
   e['RENDERDOC_METAL_RUNTIME_RMW_OFFSET']=str(args.query_offset)
  if args.read_modify_write:
   e['RENDERDOC_METAL_RUNTIME_READ_MODIFY_WRITE']='1'
   e['RENDERDOC_METAL_RUNTIME_RMW_OFFSET']=str(args.rmw_offset)
  if args.guarded_index:e['RENDERDOC_METAL_RUNTIME_GUARDED_INDEX']='1'
  if args.gpu_descriptors:e['RENDERDOC_METAL_RUNTIME_GPU_DESCRIPTORS']='1'
  if args.scalar_counter:e['RENDERDOC_METAL_RUNTIME_SCALAR_COUNTER']='1'
  if args.scalar_control:e['RENDERDOC_METAL_RUNTIME_SCALAR_'+args.scalar_control.upper()]='1'
  if args.group_builtins:e['RENDERDOC_METAL_RUNTIME_GROUP_BUILTINS']='1'
  e.update(MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
  if args.capture_without_coverage:e['RENDERDOC_METAL_RUNTIME_NO_COVERAGE']='1'
  if tag=='coverage-free-pre-submit':e['RENDERDOC_METAL_PRE_SUBMIT_COVERAGE']='65'
  if capture:e.update(DYLD_INSERT_LIBRARIES=str(backend),RENDERDOC_METAL_RAYTRACING_PROBE='1')
  with (w/(tag+'-renderdoc.log')).open('a') as keep,(w/(tag+'.log')).open('w') as out:
   fcntl.flock(keep,fcntl.LOCK_SH);child=subprocess.Popen(list(map(str,cmd)),env=e,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
   try:code=child.wait(timeout=60)
   except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait();code=124
  text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace')
  markers=['Assertion failed','failed assertion','ForceCrash','OVERRUNNING CHUNK','m_ResourceMap.empty','m_ResourceRecords.empty']
  if pre_gpu:markers.append('Metal replay wait begin')
  hits=[x for x in markers if x in text];passed=code==expected and not hits
  m['checks'].append(dict(tag=tag,exit=code,passed=passed,hits=hits));(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n');print(tag,code,flush=True)
  if not passed:raise RuntimeError(tag+'\n'+text[-3000:])
  return text
 try:
  with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
   fcntl.flock(lock,fcntl.LOCK_EX)
   for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
   shader='metal_runtime_query_consumer.metal' if args.native_ray_query else 'metal_runtime_guarded_index.metal' if args.guarded_index else 'metal_runtime_scalar_counter.metal' if args.scalar_counter else 'metal_runtime_indexed_copy.metal' if args.loop else 'metal_runtime_group_builtins.metal' if args.group_builtins else 'metal_runtime_heap_consumer.metal'
   run('shader-build',['xcrun','-sdk','macosx','metal','-c',r/'util/test/metal'/shader,*(['-DPOINTER_SELECT=1'] if args.pointer_select else []),*(['-DQUERY_CONTRIBUTION_PRODUCER=1','-DQUERY_CONTRIBUTION_SLOT='+str(args.query_producer_slot),'-DQUERY_CONTRIBUTION_COUNT_SLOT='+str(9 if args.query_producer_slot==2 else 3)] if args.query_contribution_producer else []),*(['-DQUERY_POINTER_BRANCHES=1'] if args.query_pointer_branches else []),*(['-DQUERY_REGISTER_API=1'] if args.query_register_api else []),*(['-DQUERY_DYNAMIC_HEAP=1'] if args.query_dynamic_heap else []),*(['-DQUERY_GROUP_INDICES=1'] if args.query_group_indices else []),*(['-DQUERY_NULL_ROWS='+str(args.query_null_rows)] if args.query_null_rows else []),*(['-DQUERY_CREATION_ZERO=1'] if args.query_creation_input and args.query_creation_input not in ['bytes-shared','heap-birth'] else []),*(['-DQUERY_PARTIAL_HEAP=1','-DQUERY_PARTIAL_OFFSET='+str(args.query_partial_offset),'-DQUERY_PRODUCER_SLOT='+str(args.query_producer_slot),'-DQUERY_PRODUCER_BYTE_OFFSET='+str(args.query_partial_offset+(args.query_alias_offset if args.query_partial_alias else 0))] if args.query_partial_heap else []),*(['-DQUERY_SWAPPED_INITIAL=1'] if args.query_dynamic_heap and args.gpu_descriptors and (not args.background_gpu_descriptors or args.republish_gpu_descriptors) else []),*(['-DCALL_EFFECTS=1'] if args.call_effects else []),*(['-DNULLABLE_SOURCE=1'] if args.nullable_source else []),*(['-DLITERAL_PROJECTION=1'] if args.literal_projection else []),*(['-DTEXTURE_DIMENSIONS=1'] if args.texture_dimensions else []),*(['-DTEXTURE_BUFFER_VIEW=1'] if args.texture_buffer_view else []),*(['-DTEXTURE_PIXEL_READ=1'] if args.pixel_reader else []),*(['-DPARTIAL_VIEW_INIT=1'] if args.partial_view_init else []),*(['-DNATIVE_LANES=1'] if args.native_lanes else []),*(['-DTHREADGROUP_ATOMICS=1'] if args.threadgroup_atomics else []),*(['-DREAD_MODIFY_WRITE=1'] if args.read_modify_write else []),*(['-DREGISTER_BIT_EFFECTS=1'] if args.register_bit_effects else []),*(['-DTEXTURE_ATOMICS=1'] if args.texture_atomics else []),*(['-DNATIVE_SAMPLER_HEAP=1'] if args.native_sampler_heap else []),'-o',w/'query.air'])
   run('metallib-build',['xcrun','-sdk','macosx','metallib',w/'query.air','-o',w/'query.metallib'])
   if args.legacy_register_effects:
    import sys,shutil
    sys.path.insert(0,str(r/'util/shader_tools'))
    from metal_air_processor import disassemble
    shutil.copy2(w/'query.metallib',w/'query-modern.metallib')
    entries=['runtime_clear','runtime_counter']+(['runtime_texture_producer'] if args.texture_atomics else [])
    objects=[];changed=0
    for entry in entries:
     module=disassemble(w/'query-modern.metallib',entry)
     # Only declaration effect attributes change. Native instructions and
     # API/root metadata originate from this independently compiled fixture.
     def legacy_declaration(match):
      nonlocal changed
      changed+=1
      return match[1]+' nounwind'
     module=re.sub(r'^(declare [^@]+@air\.(?:clz|ctz|popcount|reverse_bits)\.[^\n]+\))[^\n]*$',legacy_declaration,module,flags=re.M)
     source=w/(entry+'-legacy.ll');source.write_text(module);obj=source.with_suffix('.air')
     target=re.search(r'^target triple = "([^"\n]+)"',module,re.M)[1]
     language=re.search(r'!"Metal", i32 ([0-9]+), i32 ([0-9]+)',module)
     run(entry+'-legacy-build',['xcrun','-sdk','macosx','metal','-c','-O0','-std=metal'+language[1]+'.'+language[2],'-target',target,source,'-o',obj]);objects.append(obj)
    assert changed>0
    run('legacy-metallib',['xcrun','-sdk','macosx','metallib',*objects,'-o',w/'query.metallib'])
    m['legacy_register_declarations']=changed
    m['register_module_sha256']={p.name:sha(p) for p in w.glob('*-legacy.ll')}

   if args.texture_dimensions:
    run('pixel-reader-build',['xcrun','-sdk','macosx','metal','-c',r/'util/test/metal'/shader,'-DPOINTER_SELECT=1','-DCALL_EFFECTS=1','-DNULLABLE_SOURCE=1','-DLITERAL_PROJECTION=1','-DTEXTURE_DIMENSIONS=1','-DTEXTURE_PIXEL_READ=1',*(['-DTEXTURE_BUFFER_VIEW=1'] if args.texture_buffer_view else []),*(['-DPARTIAL_VIEW_INIT=1'] if args.partial_view_init else []),'-o',w/'pixel-reader.air'])
    run('pixel-reader-metallib',['xcrun','-sdk','macosx','metallib',w/'pixel-reader.air','-o',w/'pixel-reader.metallib'])
   run('native-build',['clang++','-std=c++17','-fobjc-arc','-I'+str(r),r/'util/test/metal/metal_runtime_heap_consumer_capture.mm','-framework','Foundation','-framework','Metal','-o',w/'native'])
   run('replay-build',['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I'+str(r),r/'util/test/metal/metal_runtime_heap_consumer_replay.cpp','-L'+str(b/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(b/'lib'),'-o',w/'replay'])
   run('CPU-build',['clang++','-std=c++17','-I'+str(r),r/'util/test/metal/metal_air_access_test.cpp','-o',w/'air-test'])
   run('CPU',[w/'air-test'])
   for tag,capture in [('native',False),('capture',True)]:
    tail=[args.copy_chain,'loop'] if args.loop else [] if args.copy_chain=='direct' else [args.copy_chain]
    if args.buffer_types:tail+=['buffer-types']
    text=run(tag,[w/'native',w/'query',w/'query.metallib']+tail,capture=capture)
    if args.background_gpu_descriptors:assert 'PASS background GPU descriptors: two actual copied identities' in text
    assert ('PASS runtime indexed loop:' if args.loop else 'PASS runtime Native read-modify-write:' if args.read_modify_write else 'PASS runtime Native query:' if args.native_ray_query else 'PASS runtime dynamic heap stores: A[3]=123 B[7]=456') in text
   cap=w/'query_capture.rdc';assert cap.exists()
   if args.copy_chain=='opaque':
    run('opaque-writer-API',[b/'metal-ray-b534/final-short/opener',cap],4,pre_gpu=True)
    run('opaque-writer-CLI',[b/'bin/renderdoccmd','replay','--loops','1',cap],1,pre_gpu=True)
    assert sha(backend)==m['backend_sha256']
    m.update(status='PASS',capture_sha256=sha(cap),shader_sha256=sha(w/'query.metallib'),scope='Fresh native/capture correct; unknown GPU writer between known copies rejected before GPU replay',replay_rejected_before_GPU=True,capability_enablement_certified=False)
    return
   if args.scalar_control:
    run('scalar-control-API',[b/'metal-ray-b534/final-short/opener',cap],4,pre_gpu=True)
    run('scalar-control-CLI',[b/'bin/renderdoccmd','replay','--loops','1',cap],1,pre_gpu=True)
    assert sha(backend)==m['backend_sha256']
    m.update(status='PASS',capture_sha256=sha(cap),shader_sha256=sha(w/'query.metallib'),scope='Fresh native/capture correct; conditional or later-copy scalar publication remains conservatively unsupported and is rejected before GPU replay',replay_rejected_before_GPU=True,capability_enablement_certified=False)
    return
   if args.query_null_opaque_writer:
    text=run('opaque-null-API',[b/'metal-ray-b534/final-short/opener',cap],4,pre_gpu=True)
    assert 'Metal runtime consumer:' in text and 'unknownBuffer=1' in text, 'Expected unqualified GPU pointer writer closure was not reached'
    run('opaque-null-CLI',[b/'bin/renderdoccmd','replay','--loops','1',cap],1,pre_gpu=True)
    m.update(status='PASS EXPECTED RECOVERY REFUSAL',replay_rejected_before_GPU=True,capability_enablement_certified=False)
    return
   if args.contribution_baseline:
    run('baseline-API',[w/'replay',cap],3,pre_gpu=True);run('baseline-CLI',[b/'bin/renderdoccmd','replay','--loops','1',cap],1,pre_gpu=True)
    m.update(status='PASS EXPECTED RECOVERY REFUSAL',scope='Actual Native RT output/capture passes; ordinary contribution GPU producer rejected by old frame-wide immutability; no replay GPU',capture_sha256=sha(cap));return
   api_text=run('API',[w/'replay',cap]);run('CLI',[b/'bin/renderdoccmd','replay','--loops','3',cap])
   if args.restoration_display or args.query_dynamic_heap:assert 'accessDisplay=partial' in api_text,'Partial display qualification was not exercised'
   run('export',[b/'bin/renderdoccmd','convert','-f',cap,'-o',w/'original.zip.xml','-c','zip.xml'])
   tree=ET.parse(w/'original.zip.xml');field=lambda c,n:c.find('./*[@name="'+n+'"]')
   with zipfile.ZipFile(w/'original.zip') as z:blobs={n:z.read(n) for n in z.namelist()}
   cs=tree.find('chunks')
   if args.capture_without_coverage:
    declarations=[c for c in cs if c.get('name')=='MTLDevice::DeclareDescriptorCoverage']
    assert len(declarations)==1 and field(declarations[0],'version').text=='65'
    m['application_coverage_declaration']=False
    m['backend_capture_protocol']=65
    if args.query_creation_input=='heap-birth':assert any(c.get('name')=='MTLBuffer::CaptureHeapBirthContents' for c in cs)
   if args.background_gpu_descriptors:
    boundary=next(int(c.get('chunkIndex')) for c in cs if c.get('name')=='Internal::Beginning of Capture')
    m['background_GPU_values']=sum(c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'event').text=='3' and int(c.get('chunkIndex'))<boundary for c in cs)
    m['frame_GPU_values']=sum(c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'event').text=='3' and int(c.get('chunkIndex'))>boundary for c in cs)
    assert m['background_GPU_values']==2 and m['frame_GPU_values']==(2 if args.republish_gpu_descriptors else 0)+int(args.query_null_gpu_publish)
   if args.query_creation_input=='heap-birth':assert any(c.get('name')=='MTLBuffer::CaptureHeapBirthContents' for c in cs)
   if args.query_contribution_producer:
    # Corrupt the real typed source interval, rather than ordinary GPU values.
    invalid=copy.deepcopy(tree);cs_bad=invalid.find('chunks')
    headers=[c for c in cs_bad if c.get('name')=='MTLBuffer::DeclareRayASHeader'];assert headers
    for c in headers:field(c,'contributionOffset').text=str(args.query_contribution_offset+args.query_instances*4+4)
    for c in cs_bad:c.set('length',str(int(c.get('length','0'))+128))
    xml=w/'invalid-contribution-range.zip.xml';invalid.write(xml,encoding='utf-8',xml_declaration=True)
    os.link(w/'original.zip',xml.with_suffix(''))
    run('contribution-range-import',[b/'bin/renderdoccmd','convert','-f',xml,'-o',w/'invalid-contribution-range.rdc','-c','rdc'])
    run('contribution-range-API',[b/'metal-ray-b534/final-short/opener',w/'invalid-contribution-range.rdc'],4,pre_gpu=True)
    run('contribution-range-CLI',[b/'bin/renderdoccmd','replay','--loops','1',w/'invalid-contribution-range.rdc'],1,pre_gpu=True)
    m['contribution_negative']='Declared nonzero header pointer source offset lies beyond the real contribution allocation; ordinary shader values remain untouched; reject before GPU.'
   if args.positive_only:
    assert sha(backend)==m['backend_sha256']
    if args.query_birth_pending:
     assert 'GPU-prefix-original-birth-bytes' in (w/'capture.log').read_text(),'GPU prefix observation was not exercised'
    m.update(status='INCOMPLETE',execution={'GPU':'COMPLETED'},output_comparison={'status':'MATCH','scope':'Native query output and complete defined heap birth bytes; replay events/EID0'},overall_acceptance={'status':'INCOMPLETE','reason':'UE and final RT gates pending'},capture_sha256=sha(cap),shader_sha256=sha(w/'query.metallib'),event_checks=56 if args.query_birth_prefix or args.query_contribution_producer else 48,public_access_checks=20,capability_enablement_certified=False,scope='Directed Native/capture/full GPU replay and EID0 only; corruption matrix NOT RUN')
    return
   assert sum(c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups' for c in cs)==(6 if args.texture_atomics else 4 if args.scalar_counter else 3 if args.query_partial_gpu_producer else 2)
   assert any(c.get('name')=='MTLBuffer::DeclareRayASHeader' for c in cs)
   assert not any('DeclareRayQuery' in c.get('name','') for c in cs)
   mainFunction=field(next(c for c in cs if c.get('name')=='MTLLibrary::newFunctionWithName' and field(c,'FunctionName').text=='runtime_clear'),'Function').text
   mainPipeline=field(next(c for c in cs if c.get('name')=='MTLDevice::newComputePipelineStateWithFunction' and field(c,'computeFunction').text==mainFunction),'ComputePipelineState').text
   consumer=field(next(c for c in cs if c.get('name')=='MTLComputeCommandEncoder::setComputePipelineState' and field(c,'pipeline').text==mainPipeline),'ComputeCommandEncoder').text
   root=next(c for c in cs if c.get('name')=='MTLCommandEncoder::DescriptorInlineBinding' and field(c,'entry').text=='0' and field(c,'encoder').text==consumer)
   cbv=field(root,'resource').text
   update=next(c for c in cs if c.get('name')=='Internal_MTLBufferModifyCPUContents' and field(c,'size').text==('24' if args.loop else '16'))
   upload=field(update,'Buffer').text
   bad_tags=['unknown-index','AS-as-buffer','write-outside','readonly-UAV','missing-ABI','missing-root','missing-source','unknown-GPU-input','unqualified-runtime-ray']
   if args.native_ray_query:bad_tags+=['missing-query-header','missing-query-AS-initial','missing-query-child-initial','missing-query-contribution-initial']
   if args.query_dynamic_heap:bad_tags+=['missing-dynamic-initial-A','missing-dynamic-initial-B']
   if args.query_null_rows:bad_tags+=['null-field-unrestored-address','retired-nonzero-buffer']
   if args.query_null_gpu_publish:bad_tags+=['missing-null-copy','missing-null-source-publication','missing-null-source-initial']
   if args.query_partial_heap:bad_tags+=['missing-partial-source']+([] if args.query_creation_input else ['missing-partial-upload'])
   if args.query_creation_input=='bytes-shared':bad_tags.append('missing-creation-bytes')
   if args.query_creation_input=='heap-birth':bad_tags+=['missing-heap-birth-input','heap-birth-offset-outside','heap-birth-short-input']
   if args.query_partial_alias:bad_tags+=['alias-outside-defined-range','alias-invalid-owner']
   if args.query_partial_alias and not args.query_partial_gpu_producer:bad_tags.append('late-alias-producer')
   if args.query_partial_alias and args.query_cold_rows and not args.query_partial_gpu_producer:bad_tags.append('alias-overwrite-undefined')
   if args.query_group_indices and not args.query_partial_gpu_producer:bad_tags.append('partial-narrow-initial')
   if args.query_cold_rows and args.query_group_indices:bad_tags+=['namespace-cold-row-reachable','namespace-outside-table']
   if args.query_partial_gpu_producer and not args.query_partial_gpu_same_submit:bad_tags.append('late-partial-producer')
   if args.read_modify_write:bad_tags+=['missing-RMW-initial']
   if args.pointer_select:bad_tags+=['embedded-captured-address']
   if args.literal_projection:bad_tags+=['embedded-derived-address','embedded-spanning-address']
   if args.texture_dimensions:bad_tags+=['missing-dimension-source','CBV-as-dimension-texture','missing-pixel-initial']
   if args.texture_buffer_view:bad_tags+=['readonly-buffer-texture','view-format-mismatch','view-offset-outside','view-alias-before-use']
   if args.view_shared_root:bad_tags.append('view-root-overlap')
   if args.texture_atomics:bad_tags.remove('missing-pixel-initial');bad_tags+=['missing-texture-producer','readonly-atomic-texture','readback-offset-outside','readback-row-short','readback-missing-submit']
   if args.native_sampler_heap:bad_tags+=['missing-native-sampler-binding','missing-native-sampler-source','sampler-as-buffer']
   if args.gpu_descriptors and (not args.background_gpu_descriptors or args.republish_gpu_descriptors):bad_tags+=['missing-descriptor-copy','wrong-GPU-descriptor-source','wrong-GPU-descriptor-value']
   if args.background_gpu_descriptors:bad_tags+=['missing-initial-GPU-source','wrong-initial-GPU-source','wrong-initial-GPU-value']
   if args.scalar_counter:bad_tags+=['missing-counter-producer','readonly-counter']
   if args.group_builtins or args.guarded_index:bad_tags+=['zero-divisor']
   if args.copy_chain!='direct':bad_tags+=['missing-copy-producer','unpublished-copy-range','copy-source-outside']
   if args.loop:
    bad_tags.remove('write-outside');bad_tags+=['loop-index-outside','loop-count-overflow','loop-count-zero','loop-unknown-input','loop-input-writer-overlap','loop-descriptor-range']
   if args.restoration_display or args.native_ray_query:
    # These alter shader numerical inputs or remove a writer whose restored
    # initial contents remain valid. They are not missing-pointer captures.
    # Do not replay intentionally invalid division/out-of-allocation mutants.
    reclassified=['write-outside','zero-divisor','missing-counter-producer']
    # Native creation state and replayed ordinary producers are separate from
    # numerical output expectations. These edits describe a changed/partly
    # unspecified ordinary program, not intrinsically malformed API streams.
    if args.query_partial_heap:reclassified+=['missing-partial-upload','late-partial-producer','late-alias-producer','alias-overwrite-undefined','namespace-cold-row-reachable']
    m['legacy_numeric_controls_not_run']=reclassified
    bad_tags=[tag for tag in bad_tags if tag not in reclassified]
   aliasTarget=aliasProducer=None
   if args.query_partial_alias:
    aliasBinding=next(c for c in cs if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='72' and field(c,'kind').text=='0')
    aliasTarget=field(aliasBinding,'resource').text
    aliasBirth=next(c for c in cs if c.get('name')=='MTLHeap::newBuffer(offset)' and field(c,'Buffer').text==aliasTarget)
    aliasProducer=next(field(c,'Buffer').text for c in cs if c.get('name')=='MTLHeap::newBuffer(offset)' and field(c,'Heap').text==field(aliasBirth,'Heap').text and field(c,'Buffer').text!=aliasTarget)
   for tag in bad_tags:
    t=copy.deepcopy(tree);chunks=t.find('chunks');patches={}
    if tag in ['alias-outside-defined-range','alias-invalid-owner']:
     birth=next(c for c in chunks if c.get('name')=='MTLHeap::newBuffer(offset)' and field(c,'Buffer').text==aliasTarget)
     if tag=='alias-outside-defined-range':field(birth,'offset').text=str(args.query_alias_offset*2+args.query_partial_size)
     else:field(birth,'Heap').text='999998'
    elif tag=='alias-overwrite-undefined':
     cold=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='96' and field(c,'kind').text=='0')
     overwrite=copy.deepcopy(next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer').text==aliasProducer))
     factory=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::blitCommandEncoder' and int(c.get('chunkIndex'))>int(aliasBirth.get('chunkIndex')))
     field(overwrite,'BlitCommandEncoder').text=field(factory,'BlitCommandEncoder').text
     field(overwrite,'sourceBuffer').text=field(cold,'resource').text;field(overwrite,'sourceOffset').text='0'
     field(overwrite,'destinationBuffer').text=aliasTarget;field(overwrite,'destinationOffset').text=str(args.query_partial_offset)
     chunks.insert(list(chunks).index(factory)+1,overwrite)
    elif tag=='late-alias-producer':
     copyChunk=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer').text==aliasProducer)
     encoder=field(copyChunk,'BlitCommandEncoder').text
     factory=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::blitCommandEncoder' and field(c,'BlitCommandEncoder').text==encoder)
     owner=field(factory,'CommandBuffer').text
     begin=next(i for i,c in enumerate(chunks) if c.get('name')=='MTLCommandQueue::commandBuffer' and field(c,'CommandBuffer').text==owner)
     end=next(i for i,c in enumerate(chunks) if c.get('name')=='MTLCommandBuffer::waitUntilCompleted' and field(c,'CommandBuffer').text==owner)
     producer=list(chunks)[begin:end+1]
     for c in producer:chunks.remove(c)
     consumerEnd=next(i for i,c in enumerate(chunks) if c.get('name')=='MTLCommandBuffer::waitUntilCompleted')
     for i,c in enumerate(producer):chunks.insert(consumerEnd+1+i,c)
    elif tag in ['namespace-cold-row-reachable','namespace-outside-table']:
     dispatch=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups' and field(c,'ComputeCommandEncoder').text==consumer)
     size=field(dispatch,'groups');width=field(size,'width');assert width.text=='3'
     width.text=str(4 if tag=='namespace-cold-row-reachable' else 4+args.query_cold_rows)
    elif tag=='partial-narrow-initial':
     binding=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='72' and field(c,'kind').text=='0')
     if args.query_partial_gpu_producer:
      # A genuine four-byte producer cannot be shortened by a copy mutant.
      continue
     producer=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer').text==(aliasProducer if args.query_partial_alias else field(binding,'resource').text))
     field(producer,'size').text='2'
    elif tag=='null-field-unrestored-address':
     heap=next(c for c in chunks if c.get('name')=='MTLBuffer::DeclareDescriptorTable' and int(field(c,'count').text)>3)
     initial=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==field(heap,'buffer').text)
     index=int(next(initial.iter('buffer')).text);data=bytearray(blobs[f'{index:06}']);struct.pack_into('<Q',data,(2+args.query_null_rows)*24,0xdead0000);patches[index]=data
    elif tag=='retired-nonzero-buffer':
     event=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'offset').text=='72' and field(c,'event').text=='1')
     field(event,'offset').text='24';field(event,'descriptorType').text='5'
    elif tag in ['missing-null-copy','missing-null-source-publication','missing-null-source-initial']:
     event=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'offset').text=='72' and field(c,'event').text=='3')
     null_copy=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer').text==field(event,'buffer').text and field(c,'destinationOffset').text=='72')
     if tag=='missing-null-copy':chunks.remove(null_copy)
     elif tag=='missing-null-source-publication':
      source=field(null_copy,'sourceBuffer').text
      for c in list(chunks):
       if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'buffer').text==source:chunks.remove(c)
     else:
      initial=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==field(null_copy,'sourceBuffer').text);chunks.remove(initial)
    elif tag=='late-partial-producer':
     dispatch=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups')
     encoder=field(dispatch,'ComputeCommandEncoder').text
     factory=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::computeCommandEncoder' and field(c,'ComputeCommandEncoder').text==encoder)
     owner=field(factory,'CommandBuffer').text
     begin=next(i for i,c in enumerate(chunks) if c.get('name')=='MTLCommandQueue::commandBuffer' and field(c,'CommandBuffer').text==owner)
     end=next(i for i,c in enumerate(chunks) if c.get('name')=='MTLCommandBuffer::waitUntilCompleted' and field(c,'CommandBuffer').text==owner)
     producer=list(chunks)[begin:end+1]
     for c in producer:chunks.remove(c)
     consumerEnd=next(i for i,c in enumerate(chunks) if c.get('name')=='MTLCommandBuffer::waitUntilCompleted')
     for i,c in enumerate(producer):chunks.insert(consumerEnd+1+i,c)
    elif tag in ['missing-heap-birth-input','heap-birth-offset-outside','heap-birth-short-input']:
     binding=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='72' and field(c,'kind').text=='0')
     target=field(binding,'resource').text
     initial=next(c for c in chunks if c.get('name')=='MTLBuffer::CaptureHeapBirthContents' and field(c,'buffer').text==target)
     if tag=='missing-heap-birth-input':chunks.remove(initial)
     elif tag=='heap-birth-offset-outside':field(initial,'offset').text=str(args.query_partial_size*4)
     else:
      index=int(field(initial,'data').text);patches[index]=blobs[f'{index:06}'][:-1];field(initial,'data').set('byteLength',str(args.query_partial_size-1))
    elif tag=='missing-creation-bytes':
     binding=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='72' and field(c,'kind').text=='0')
     target=field(binding,'resource').text
     birth=next(c for c in chunks if c.get('name')=='MTLDevice::newBufferWithBytes' and field(c,'Buffer').text==target)
     initial=field(birth,'initialData');initial.set('byteLength','0');patches[int(initial.text)]=b''
    elif tag.startswith('missing-partial-'):
     binding=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='72' and field(c,'kind').text=='0')
     target=field(binding,'resource').text
     if tag=='missing-partial-source':chunks.remove(binding)
     else:
      copies=[c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer').text==(aliasProducer if args.query_partial_alias else target)]
      if args.query_partial_gpu_producer:
       producer=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups');chunks.remove(producer)
      else:
       assert copies,tag
       for c in copies:chunks.remove(c)
    elif tag.startswith('missing-dynamic-initial-'):
     binding=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text==('24' if tag.endswith('-A') else '48') and field(c,'kind').text=='0')
     target=field(binding,'resource').text
     initial=[c for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==target]
     assert initial,tag
     for c in initial:chunks.remove(c)
    elif tag.startswith('missing-query-'):
     header=next(c for c in chunks if c.get('name')=='MTLBuffer::DeclareRayASHeader')
     if tag=='missing-query-header':chunks.remove(header)
     else:
      target=field(header,'contributions' if tag=='missing-query-contribution-initial' else 'structure').text
      if tag=='missing-query-child-initial':
       parent=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==target)
       target=next(x.text for x in field(parent,'children') if x.text not in [None,'0'])
      initial=[c for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==target]
      assert initial,tag
      for c in initial:chunks.remove(c)
    elif tag in ['embedded-captured-address','embedded-derived-address','embedded-spanning-address']:

     # Make this captured identity match the literal, preserving all typed
     # source/value metadata. The bytecode would need real relocation; reject
     # independently of the optional numerical predicate, before any GPU work.
     slot=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='48')
     target=field(slot,'resource').text
     identities=[c for c in chunks if c.get('name')=='MTLResource::CaptureGPUIdentity' and field(c,'resource').text==target]
     assert identities
     old=int(field(identities[0],'value').text)
     replacement=1028 if tag=='embedded-derived-address' else 1026 if tag=='embedded-spanning-address' else 1024
     for c in identities:field(c,'value').text=str(replacement)
     for i,v in blobs.items():
      # Only replace exact aligned captured identity fields, preserving other
      # bytes. This is a test mutation, not runtime address discovery.
      data=bytearray(v);changed=False
      for pos in range(0,len(v)-7,8):
       if struct.unpack_from('<Q',v,pos)[0]==old:struct.pack_into('<Q',data,pos,replacement);changed=True
      if changed:patches[int(i)]=data
    elif tag=='missing-RMW-initial':
     binding=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='24' and field(c,'kind').text=='0')
     target=field(binding,'resource').text
     initial=[c for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'id').text==target]
     assert initial
     for c in initial:chunks.remove(c)
    elif tag=='missing-texture-producer':
     producer=next(c for c in chunks if c.get('name')=='MTLLibrary::newFunctionWithName' and field(c,'FunctionName').text=='runtime_texture_producer');function=field(producer,'Function').text
     pipeline=next(c for c in chunks if c.get('name')=='MTLDevice::newComputePipelineStateWithFunction' and field(c,'computeFunction').text==function);pso=field(pipeline,'ComputePipelineState').text
     encoders={field(c,'ComputeCommandEncoder').text for c in chunks if c.get('name')=='MTLComputeCommandEncoder::setComputePipelineState' and field(c,'pipeline').text==pso}
     for c in list(chunks):
      if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups' and field(c,'ComputeCommandEncoder').text in encoders:chunks.remove(c)
    elif tag.startswith('readback-'):
     transfer=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromTexture')
     if tag=='readback-offset-outside':field(transfer,'destinationOffset').text='2112'
     elif tag=='readback-row-short':field(transfer,'destinationBytesPerRow').text='16'
     else:
      encoder=field(transfer,'BlitCommandEncoder').text
      creation=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::blitCommandEncoder' and field(c,'BlitCommandEncoder').text==encoder)
      final=field(creation,'CommandBuffer').text
      prior=[c for c in chunks if c.get('name')=='MTLCommandBuffer::commit' and field(c,'CommandBuffer').text!=final]
      assert prior
      for c in prior:chunks.remove(c)
    elif tag=='readonly-atomic-texture':
     for c in chunks:
      if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'offset').text=='96':field(c,'descriptorType').text='4'
    elif tag=='missing-dimension-source':
     c=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='96');chunks.remove(c)
    elif tag=='CBV-as-dimension-texture':
     for c in chunks:
      if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'offset').text=='96':field(c,'descriptorType').text='6'
    elif tag in ['readonly-buffer-texture','view-format-mismatch','view-offset-outside','view-alias-before-use','view-root-overlap']:
     binding=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='96' and field(c,'kind').text=='1')
     texture=field(binding,'resource').text
     factory=next(c for c in chunks if c.get('name')=='MTLBuffer::newTextureWithDescriptor' and field(c,'Texture').text==texture)
     if tag=='readonly-buffer-texture':
      for c in chunks:
       if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'offset').text=='96':field(c,'descriptorType').text='2'
     elif tag=='view-format-mismatch':field(factory,'descriptor').find('./*[@name="pixelFormat"]').text='55'
     elif tag=='view-offset-outside':field(factory,'offset').text='1024'
     elif tag=='view-root-overlap':field(factory,'offset').text='16'
     else:
      resource=field(factory,'Buffer').text
      # A true alias/lifetime mutation uses the same captured API chunk schema.
      enum=re.search(r'enum class MetalChunk.*?\{(.*?)\n\};',(r/'renderdoc/driver/metal/metal_common.h').read_text(),re.S)
      assert enum
      entries=[x.strip().split('=')[0].strip() for x in enum.group(1).split(',')]
      chunkID=1000+entries.index('MTLBuffer_makeAliasable')-entries.index('MTLCreateSystemDefaultDevice')
      c=ET.Element('chunk',{'id':str(chunkID),'name':'MTLBuffer::makeAliasable','length':'0'})
      ET.SubElement(c,'ResourceId',{'name':'Buffer','typename':'MTLBuffer','width':'8'}).text=resource
      chunks.insert(list(chunks).index(factory)+1,c)
    elif tag=='missing-pixel-initial':
     if args.texture_buffer_view:
      slot=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='96' and field(c,'kind').text=='0');parent=field(slot,'resource').text
      for c in list(chunks):
       # Preserve the real 16-byte root uploads. They do not restore the
       # disjoint pixel interval of this texture-buffer view.
       if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer').text==parent and ((field(c,'destinationOffset').text=='0' and field(c,'size').text=='1024') or (args.partial_view_init and field(c,'destinationOffset').text=='256' and field(c,'size').text=='16')):chunks.remove(c)
     library=next(c for c in chunks if c.get('name')=='MTLDevice::newLibraryWithURL');blob=field(library,'data');data=(w/'pixel-reader.metallib').read_bytes();patches[int(blob.text)]=data;blob.set('byteLength',str(len(data)));library.set('length',str(len(data)+512))
    elif tag in ['missing-initial-GPU-source','wrong-initial-GPU-source','wrong-initial-GPU-value']:
     boundary=next(int(c.get('chunkIndex')) for c in chunks if c.get('name')=='Internal::Beginning of Capture')
     c=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='24' and int(c.get('chunkIndex'))<boundary and any(e.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(e,'buffer').text==field(c,'buffer').text and field(e,'offset').text=='24' and field(e,'event').text=='3' and int(e.get('chunkIndex'))<int(c.get('chunkIndex')) for e in chunks))
     if tag=='missing-initial-GPU-source':chunks.remove(c)
     elif tag=='wrong-initial-GPU-source':field(c,'resource').text=cbv
     else:
      event=next(e for e in chunks if e.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(e,'buffer').text==field(c,'buffer').text and field(e,'offset').text=='24' and field(e,'event').text=='3' and int(e.get('chunkIndex'))<boundary)
      i=int(field(event,'data').text);data=bytearray(blobs[f'{i:06}']);struct.pack_into('<Q',data,16,128);patches[i]=data
    elif tag in ['missing-native-sampler-binding','missing-native-sampler-source','sampler-as-buffer']:
     if tag=='missing-native-sampler-binding':
      c=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::setBuffer' and field(c,'ComputeCommandEncoder').text==consumer and field(c,'index').text=='1');chunks.remove(c)
     else:
      c=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'kind').text=='2' and field(c,'offset').text=='24')
      if tag=='missing-native-sampler-source':chunks.remove(c)
      else:
       table=field(c,'buffer').text
       for e in chunks:
        if e.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(e,'buffer').text==table and field(e,'offset').text=='24':field(e,'descriptorType').text='4'
    elif tag=='missing-descriptor-copy':
     c=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'size').text=='24');chunks.remove(c)
    elif tag=='wrong-GPU-descriptor-source':
     c=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and int(c.get('chunkIndex','0'))>int(root.get('chunkIndex','0')) and field(c,'offset').text=='48')
     field(c,'memberOffset').text='4'
    elif tag=='wrong-GPU-descriptor-value':
     c=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'event').text=='3' and (not args.background_gpu_descriptors or int(c.get('chunkIndex'))>boundary))
     i=int(field(c,'data').text);data=bytearray(blobs[f'{i:06}']);struct.pack_into('<Q',data,16,128);patches[i]=data
    elif tag=='missing-counter-producer':
     c=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups');chunks.remove(c)
    elif tag=='readonly-counter':
     for c in chunks:
      if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'offset').text=='72':field(c,'descriptorType').text='4'
    elif tag=='zero-divisor':
     u=next(c for c in chunks if c.get('name')=='Internal_MTLBufferModifyCPUContents' and field(c,'Buffer').text==upload and field(c,'size').text=='16');i=int(field(u,'data').text);data=bytearray(blobs[f'{i:06}']);struct.pack_into('<I',data,12,0);patches[i]=data
    elif tag in ['unknown-index','AS-as-buffer','write-outside']:
     u=next(c for c in chunks if c.get('name')=='Internal_MTLBufferModifyCPUContents' and field(c,'Buffer').text==upload and field(c,'size').text==('24' if args.loop else '16'))
     # Select a genuinely absent slot from the captured bindings. Newly added
     # buffer/texture slots are legal bindings, not missing-source controls.
     absent=1+max(int(field(c,'offset').text)//24 for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding')
     if tag=='unknown-index':assert not any(c.get('name')=='MTLBuffer::DescriptorSlotBinding' and int(field(c,'offset').text)==absent*24 for c in chunks)
     i=int(field(u,'data').text);data=bytearray(blobs[f'{i:06}']);struct.pack_into('<I',data,4 if tag=='write-outside' else 0,64 if tag=='write-outside' else 0 if tag=='AS-as-buffer' else absent);patches[i]=data
    elif tag=='readonly-UAV':
     for c in chunks:
      if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'offset').text=='24':field(c,'descriptorType').text='4'
    elif tag=='missing-ABI':
     for c in list(chunks):
      if c.get('name')=='MTLComputePipelineState::CaptureIRComputeReflection':chunks.remove(c)
    elif tag=='missing-root':
     c=next(c for c in chunks if c.get('name')=='MTLCommandEncoder::DescriptorInlineBinding' and field(c,'entry').text=='0' and field(c,'encoder').text==consumer);chunks.remove(c)
    elif tag=='missing-source':
     c=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='24');chunks.remove(c)
    elif tag=='unqualified-runtime-ray':
     ray=args.ray_library.resolve();assert ray.is_file();data=ray.read_bytes();m['unqualified_ray_library_sha256']=sha(ray)
     library=next(c for c in chunks if c.get('name')=='MTLDevice::newLibraryWithURL')
     blob=field(library,'data');patches[int(blob.text)]=data;blob.set('byteLength',str(len(data)))
     library.set('length',str(len(data)+512))
     function=next(c for c in chunks if c.get('name')=='MTLLibrary::newFunctionWithName');field(function,'FunctionName').text='main'
    elif tag=='unknown-GPU-input':
     c=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer').text==cbv and field(c,'destinationOffset').text==field(root,'memberOffset').text)
     output=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='24')
     field(c,'sourceBuffer').text=field(output,'resource').text
    elif tag=='missing-copy-producer':
     c=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'sourceBuffer').text==upload);chunks.remove(c)
    elif tag in ['unpublished-copy-range','copy-source-outside']:
     c=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer').text==cbv)
     field(c,'sourceOffset').text='16' if tag=='unpublished-copy-range' else '64'
    elif tag.startswith('loop-count-'):
     u=next(c for c in chunks if c.get('name')=='Internal_MTLBufferModifyCPUContents' and field(c,'Buffer').text==upload)
     i=int(field(u,'data').text);data=bytearray(blobs[f'{i:06}']);struct.pack_into('<I',data,12,0 if tag=='loop-count-zero' else 0xffffffff);patches[i]=data
    elif tag in ['loop-input-writer-overlap','loop-descriptor-range']:
     target=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='24')
     table=field(target,'buffer').text;output=field(target,'resource').text
     event=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'offset').text=='24' and field(c,'event').text=='2')
     target_data=blobs[f'{int(field(event,"data").text):06}'];slot_offset=72 if tag=='loop-input-writer-overlap' else 24
     if tag=='loop-input-writer-overlap':
      source=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='72')
      old_source=field(source,'resource').text
      field(source,'resource').text=output;field(source,'memberOffset').text='0'
      upload_copy=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer').text==old_source)
      field(upload_copy,'destinationBuffer').text=output;field(upload_copy,'destinationOffset').text='0'
     for c in chunks:
      if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and field(c,'buffer').text==table and field(c,'offset').text==str(slot_offset) and field(c,'event').text=='2':
       i=int(field(c,'data').text);data=bytearray(blobs[f'{i:06}'])
       if tag=='loop-input-writer-overlap':data[:8]=target_data[:8]
       else:struct.pack_into('<I',data,16,64)
       patches[i]=data
      if c.get('name')=='Internal::Initial Contents' and field(c,'id') is not None and field(c,'id').text==table:
       i=int(field(c,'Contents').text);data=bytearray(blobs[f'{i:06}'])
       if tag=='loop-input-writer-overlap':data[slot_offset:slot_offset+8]=target_data[:8]
       else:struct.pack_into('<I',data,slot_offset+16,64)
       patches[i]=data
    elif tag in ['loop-index-outside','loop-unknown-input']:
     slot=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'offset').text=='72')
     index_buffer=field(slot,'resource').text
     c=next(c for c in chunks if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer').text==index_buffer)
     if tag=='loop-unknown-input':chunks.remove(c)
     else:
      index_upload=field(c,'sourceBuffer').text
      u=next(c for c in chunks if c.get('name')=='Internal_MTLBufferModifyCPUContents' and field(c,'Buffer').text==index_upload)
      i=int(field(u,'data').text);data=bytearray(blobs[f'{i:06}']);struct.pack_into('<I',data,0,64);patches[i]=data
    for c in chunks:c.set('length',str(int(c.get('length','0'))+128))
    xml=w/(tag+'.zip.xml');t.write(xml,encoding='utf-8',xml_declaration=True)
    with zipfile.ZipFile(xml.with_suffix(''),'w',zipfile.ZIP_DEFLATED) as z:
     for n,v in blobs.items():z.writestr(n,patches.get(int(n),v))
    bad=w/(tag+'.rdc');run(tag+'-import',[b/'bin/renderdoccmd','convert','-f',xml,'-o',bad,'-c','rdc'])
    if tag=='missing-null-source-initial':
     # The complete typed CPU publication itself restores all 24 null bytes.
     # Removing a redundant raw initial blob is a legal recovery control.
     run(tag+'-control-API',[w/'replay',bad]);run(tag+'-control-CLI',[b/'bin/renderdoccmd','replay','--loops','3',bad])
     continue
    text=run(tag+'-API',[b/'metal-ray-b534/final-short/opener',bad],4,pre_gpu=True)
    if tag=='late-partial-producer':assert 'Metal dynamic descriptor contents unavailable:' in text, 'A future GPU producer must not initialize an earlier consumer'
    if tag=='embedded-captured-address':assert 'Metal runtime relocation required: embedded shader address=1024' in text,'Embedded address qualification was not exercised'
    if tag=='embedded-derived-address':assert 'Metal runtime relocation required: embedded shader address=1028' in text,'Derived address qualification was not exercised'
    if tag=='embedded-spanning-address':assert 'Metal runtime relocation required: embedded shader address=1024' in text,'Spanning address qualification was not exercised'
    if tag=='view-root-overlap':
     # A mutable root is re-analysed without stale scalar bytes. Its dynamic
     # GPU-selected write additionally needs independent writer coverage.
     assert ('Metal runtime texture scalar writer overlap:' in text or
             ('Metal runtime mutable scalar facts invalidated:' in text and
              'reason=same-dispatch mutable input' in text and
              (re.search(r'unknownBuffer=[1-9][0-9]*',text) or
               'Metal dynamic descriptor writer coverage unavailable:' in text))), 'Mutable root namespace qualification was not exercised'
    if tag=='missing-texture-producer':assert 'Metal runtime texture contents unavailable:' in text,'Atomic initial-state qualification was not exercised'
    if tag=='missing-pixel-initial':assert 'Metal runtime texture contents unavailable:' in text,'Pixel initial-state qualification was not exercised'
    if tag=='loop-input-writer-overlap':assert ('Metal runtime scalar writer overlap:' in text or
      ('Metal runtime mutable scalar facts invalidated:' in text and re.search(r'unknownBuffer=[1-9][0-9]*',text))), 'Mutable descriptor namespace qualification was not exercised'
    run(tag+'-CLI',[b/'bin/renderdoccmd','replay','--loops','1',bad],1,pre_gpu=True)
   assert sha(backend)==m['backend_sha256'];m.update(status='PASS',capture_sha256=sha(cap),shader_sha256=sha(w/'query.metallib'),event_checks=72 if args.texture_atomics else 56 if args.query_partial_heap and not args.query_creation_input else 48,public_access_checks=20,public_descriptor_checks=40 if args.native_ray_query else 60 if args.loop else 40 if args.texture_dimensions else 20,damaged_groups=len(bad_tags)-int(args.query_null_gpu_publish),legal_recovery_controls=int(args.query_null_gpu_publish),capability_enablement_certified=False)
 except Exception as ex:m.update(status='FAIL',error=repr(ex));raise
 finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
if __name__=='__main__':main()
