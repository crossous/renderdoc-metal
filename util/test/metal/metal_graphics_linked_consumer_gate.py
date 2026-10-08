# SPDX-License-Identifier: MIT
"""Bounded Native linked graphics capture/replay development check."""
from pathlib import Path
import argparse,fcntl,tempfile,os,subprocess,signal,json,hashlib,sys,time,copy,re
import xml.etree.ElementTree as ET
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--work-dir',type=Path,required=True);parser.add_argument('--early-fragment',action='store_true');parser.add_argument('--independent',action='store_true');parser.add_argument('--baseline',action='store_true');parser.add_argument('--local-helper',action='store_true');parser.add_argument('--gpu-indices',action='store_true');parser.add_argument('--pointer-origins',action='store_true');args=parser.parse_args()
if args.independent and not args.early_fragment:parser.error('Independent variant requires fragment workload')
if args.local_helper and not args.early_fragment:parser.error('Local helper requires fragment workload')
if args.baseline and not args.early_fragment:parser.error('Baseline reproduces fragment metadata rejection')
r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';w=args.work_dir.resolve();assert not w.exists();w.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();m={'status':'RUNNING','backend_sha256':sha(b/'lib/librenderdoc.dylib'),'scope':'Fresh sourced linked graphics and unused AS heap slot; ordinary draw, not RT or UE acceptance','checks':[]}
def run(tag,command,capture=False,expected=0):
 started=time.monotonic()
 e=os.environ.copy()
 for k in tuple(e):
  if k.startswith('RENDERDOC_') or k=='DYLD_INSERT_LIBRARIES':e.pop(k)
 e.update(MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
 if args.pointer_origins:e['RENDERDOC_METAL_GRAPHICS_POINTER_ORIGINS']='1'
 if args.gpu_indices:e['RENDERDOC_METAL_GRAPHICS_GPU_INDICES']='1'
 if args.early_fragment:e['RENDERDOC_METAL_GRAPHICS_EARLY_FRAGMENT']='1'
 if args.independent:e['RENDERDOC_METAL_GRAPHICS_FRAGMENT_INDEPENDENT']='1'
 if capture:e.update(DYLD_INSERT_LIBRARIES=str(b/'lib/librenderdoc.dylib'),RENDERDOC_METAL_RAYTRACING_PROBE='1')
 with (w/(tag+'-renderdoc.log')).open('a') as keep,(w/(tag+'.log')).open('w') as out:
  fcntl.flock(keep,fcntl.LOCK_SH);p=subprocess.Popen(list(map(str,command)),env=e,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
  try:c=p.wait(timeout=60)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();c=124
 text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace');hits=[x for x in ['Assertion failed','failed assertion','ForceCrash','OVERRUNNING CHUNK','m_ResourceMap.empty','m_ResourceRecords.empty'] if x in text]
 m['checks'].append({'tag':tag,'exit':c,'hits':hits,'seconds':time.monotonic()-started});(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n');print(tag,c,flush=True);assert c==expected and not hits,text[-3500:]
 if expected:assert 'Metal replay wait begin' not in text
 return text
try:
 with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX)
  for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
  for tag in ['main','stage']:
   run(tag+'-compile',['xcrun','-sdk','macosx','metal','-c',r/f'util/test/metal/metal_graphics_linked_{tag}.metal',*(['-DEARLY_FRAGMENT=1'] if tag=='main' and args.early_fragment else []),*(['-DINDEPENDENT_FRAGMENT=1'] if tag=='main' and args.independent else []),*(['-DLOCAL_FRAGMENT_HELPER=1'] if tag=='main' and args.local_helper else []),*(['-DPOINTER_SELECTION_ORIGINS=1'] if tag=='main' and args.pointer_origins else []),*(['-DGPU_INDEX_PRODUCER=1'] if tag=='main' and args.gpu_indices else []),'-o',w/(tag+'.air')])
   run(tag+'-metallib',['xcrun','-sdk','macosx','metallib',w/(tag+'.air'),'-o',w/(tag+'.metallib')])
  if args.early_fragment:
   sys.path.insert(0,str(r/'util/shader_tools'));from metal_air_processor import disassemble
   air=disassemble(w/'main.metallib','linked_fragment');(w/'fragment.ll').write_text(air)
   assert re.search(r'@linked_fragment, ![0-9]+, ![0-9]+, !"',air)
   if args.local_helper:assert len(re.findall(r'^define ',air,re.M))>=2
  run('native-build',['clang++','-std=c++17','-fobjc-arc','-I'+str(r),r/'util/test/metal/metal_graphics_linked_consumer_capture.mm','-framework','Foundation','-framework','Metal','-o',w/'native'])
  run('replay-build',['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I'+str(r),r/'util/test/metal/metal_graphics_linked_consumer_replay.cpp','-L'+str(b/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(b/'lib'),'-o',w/'replay'])
  for tag,capture in [('native',False),('capture',True)]:
   text=run(tag,[w/'native',w/'graphics',w/'main.metallib',w/'stage.metallib'],capture);assert 'depth pixels=0.75' in text
  cap=w/'graphics_capture.rdc';assert cap.exists();run('API',[w/'replay',cap],expected=4 if args.baseline else 0);run('CLI',[b/'bin/renderdoccmd','replay','--loops','3',cap],expected=1 if args.baseline else 0);run('export',[b/'bin/renderdoccmd','convert','-f',cap,'-o',w/'original.zip.xml','-c','zip.xml'])
  if args.early_fragment and not args.baseline:
   run('opener-build',['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I'+str(r),r/'util/ue/ue_capture_open_probe.cpp','-L'+str(b/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(b/'lib'),'-o',w/'opener'])
   tree=ET.parse(w/'original.zip.xml');chunks=tree.find('./chunks');field=lambda c,n:c.find('./*[@name="'+n+'"]')
   removed=[c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::setFragmentBuffer' and field(c,'index').text==str(11 if args.independent else 3)]
   assert removed
   for c in removed:chunks.remove(c)
   for c in chunks:c.set('length',str(int(c.get('length','0'))+128))
   invalid=w/'missing-fragment-binding.zip.xml';tree.write(invalid,encoding='utf-8',xml_declaration=True);os.link(w/'original.zip',invalid.with_suffix(''))
   run('missing-binding-import',[b/'bin/renderdoccmd','convert','-f',invalid,'-o',w/'missing-fragment-binding.rdc','-c','rdc'])
   run('missing-binding-API',[w/'opener',w/'missing-fragment-binding.rdc'],expected=4);run('missing-binding-CLI',[b/'bin/renderdoccmd','replay','--loops','1',w/'missing-fragment-binding.rdc'],expected=1)
  if args.gpu_indices and not args.baseline:
   for case in ('index-range-overrun','legal-no-index-producer'):
    tree=ET.parse(w/'original.zip.xml');chunks=tree.find('./chunks');field=lambda c,n:c.find('./*[@name="'+n+'"]')
    if case=='index-range-overrun':
     draws=[c for c in chunks if c.get('name')=='MTLRenderCommandEncoder::drawIndexedPrimitives'];assert len(draws)==2
     for c in draws:field(c,'indexBufferOffset').text=str(32 if args.independent else 6)
    else:
     calls=[c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups'];assert len(calls)==1
     chunks.remove(calls[0])
    for c in chunks:c.set('length',str(int(c.get('length','0'))+128))
    if case=='index-range-overrun':
     # Keep duplicate original API constants consistent with the changed draw;
     # only the actual Native buffer read interval is now illegal.
     changed=0
     for c in chunks:
      if c.get('name')=='MTLRenderCommandEncoder::setVertexBytes' and field(c,'index').text=='4':
       data=field(c,'data');assert len(data)==20
       data[8].text=str(32 if args.independent else 6);changed+=1
     assert changed==2
    xml=w/(case+'.zip.xml');tree.write(xml,encoding='utf-8',xml_declaration=True);os.link(w/'original.zip',xml.with_suffix(''))
    run(case+'-import',[b/'bin/renderdoccmd','convert','-f',xml,'-o',w/(case+'.rdc'),'-c','rdc'])
    run(case+'-API',[w/'opener',w/(case+'.rdc')],expected=4 if case=='index-range-overrun' else 0)
    run(case+'-CLI',[b/'bin/renderdoccmd','replay','--loops','1',w/(case+'.rdc')],expected=1 if case=='index-range-overrun' else 0)
   m['index_contracts']='GPU values are ordinary Native inputs; read interval overrun rejects, removing producer with intact legal creation input remains replayable with changed index values.'
  m.update(pointer_origins=args.pointer_origins,gpu_indices=args.gpu_indices,local_helper=args.local_helper,early_fragment=args.early_fragment,independent=args.independent,baseline_expected_rejection=args.baseline,sources_sha256={str(p.relative_to(r)):sha(p) for p in [r/'util/test/metal'/('metal_graphics_linked_'+n) for n in ['main.metal','consumer_capture.mm','consumer_replay.cpp','consumer_gate.py']]})
  assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256'];m.update(status='PASS',capture_sha256=sha(cap),events=0 if args.baseline else 32,descriptors=0 if args.baseline else 16,depth_pixels_per_event=851 if args.independent else 256,bundle_sha256=sha(b/'bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib'))
except Exception as ex:m.update(status='FAIL',error=repr(ex));raise
finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
