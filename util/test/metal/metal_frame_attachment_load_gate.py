# SPDX-License-Identifier: MIT
"""Bounded frame-native Load/Discard MRT/depth/stencil capture and replay check."""
from pathlib import Path
import argparse,fcntl,tempfile,os,subprocess,signal,json,hashlib
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--work-dir',type=Path,required=True);args=parser.parse_args()
r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';w=args.work_dir.resolve();assert not w.exists();w.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();m={'status':'RUNNING','backend_sha256':sha(b/'lib/librenderdoc.dylib'),'scope':'Generic frame placement MRT and D32S8 native Load/Discard; ordinary rendering, not RT or UE acceptance','checks':[]}
def run(tag,command,capture=False):
 e=os.environ.copy()
 for k in tuple(e):
  if k.startswith('RENDERDOC_') or k=='DYLD_INSERT_LIBRARIES':e.pop(k)
 e.update(MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
 if capture:e.update(DYLD_INSERT_LIBRARIES=str(b/'lib/librenderdoc.dylib'),RENDERDOC_METAL_RAYTRACING_PROBE='1')
 with (w/(tag+'-renderdoc.log')).open('a') as keep,(w/(tag+'.log')).open('w') as out:
  fcntl.flock(keep,fcntl.LOCK_SH);p=subprocess.Popen(list(map(str,command)),env=e,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
  try:c=p.wait(timeout=60)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();c=124
 text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace');hits=[x for x in ['Assertion failed','failed assertion','ForceCrash','OVERRUNNING CHUNK','m_ResourceMap.empty','m_ResourceRecords.empty'] if x in text]
 m['checks'].append({'tag':tag,'exit':c,'hits':hits});(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n');print(tag,c,flush=True);assert c==0 and not hits,text[-3500:];return text
try:
 with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX)
  for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
  run('shader-compile',['xcrun','-sdk','macosx','metal','-c',r/'util/test/metal/metal_frame_attachment_load.metal','-o',w/'shader.air'])
  run('shader-metallib',['xcrun','-sdk','macosx','metallib',w/'shader.air','-o',w/'shader.metallib'])
  run('native-build',['clang++','-std=c++17','-fobjc-arc','-I'+str(r),r/'util/test/metal/metal_frame_attachment_load_capture.mm','-framework','Foundation','-framework','Metal','-o',w/'native'])
  run('replay-build',['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I'+str(r),r/'util/test/metal/metal_frame_attachment_load_replay.cpp','-L'+str(b/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(b/'lib'),'-o',w/'replay'])
  m['captures']=[]
  for mode in ('load', 'discard', 'initial-r16', 'initial-rg8', 'initial-r16float', 'initial-rg16float', 'initial-r32float', 'initial-rg32float', 'initial-rgba32float', 'initial-r8snorm', 'initial-r16snorm', 'initial-rg16unorm'):
   for tag,capture in [('native',False),('capture',True)]:
    text=run(mode+'-'+tag,[w/'native',w/mode,w/'shader.metallib',mode],capture);assert '256 pixels per plane' in text
   cap=w/(mode+'_capture.rdc');assert cap.exists();run(mode+'-API',[w/'replay',cap,*([mode] if mode.startswith('initial-') else [])]);run(mode+'-CLI',[b/'bin/renderdoccmd','replay','--loops','3',cap]);run(mode+'-export',[b/'bin/renderdoccmd','convert','-f',cap,'-o',w/(mode+'.zip.xml'),'-c','zip.xml'])
   m['captures'].append({'mode':mode,'path':str(cap),'sha256':sha(cap),'event_selections':32,'pixels_per_plane_per_event':256})
  assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256'];m.update(status='PASS')
except Exception as ex:m.update(status='FAIL',error=repr(ex));raise
finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
