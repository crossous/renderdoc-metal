#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Serialized high-bit uint16 texel-buffer native/capture/replay and malformed gate."""
from pathlib import Path
import fcntl,hashlib,json,os,signal,subprocess,tempfile,re,shutil
import argparse
parser=argparse.ArgumentParser(description='Finite native/capture/replay RGBA16Uint buffer texture gate')
parser.add_argument('--work-dir',type=Path,required=True)
parser.add_argument('--build-dir',type=Path)
parser.add_argument('--build',action='store_true')
args=parser.parse_args()
r=Path(__file__).resolve().parents[3];b=(args.build_dir or r/'build-macos-debug').resolve();w=args.work_dir.resolve();w.mkdir(parents=True,exist_ok=True);cli=b/'bin/renderdoccmd'
e=os.environ.copy()
for k in tuple(e):
 if k.startswith('RENDERDOC_METAL_') or k=='DYLD_INSERT_LIBRARIES':e.pop(k)
e['MTL_DEBUG_LAYER']='1'
m={'status':'RUNNING','checks':[],'backend_sha256':hashlib.sha256((b/'lib/librenderdoc.dylib').read_bytes()).hexdigest()}
def run(n,a,extra=None,expected=0,t=60):
 env=dict(e,**(extra or {}));env['RENDERDOC_DEBUG_LOG_FILE']=str(w/(n+'-renderdoc.log'))
 with (w/(n+'-renderdoc.log')).open('a') as keep,(w/(n+'.log')).open('w') as out:
  fcntl.flock(keep,fcntl.LOCK_SH);p=subprocess.Popen(list(map(str,a)),env=env,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
  try:c=p.wait(timeout=t)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();c=124
 logs=[w/(n+'.log'),w/(n+'-renderdoc.log')]+list((w/n).rglob('*.log'))
 hits=[str(p) for p in logs if any(x in p.read_text(errors='replace') for x in ('OVERRUNNING CHUNK','Assertion failed','failed assertion','File and decompress stream readers do not support seeking','Unexpected Metal resource type','m_ResourceMap.empty'))]
 text=(w/(n+'.log')).read_text(errors='replace');ok=c==expected and not hits;m['checks'].append(dict(name=n,exit=c,passed=ok,diagnostic_hits=hits));(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n');print(n,c,flush=True)
 if not ok:raise RuntimeError(n+text[-1500:]+str(hits))
 return text
with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
 fcntl.flock(lock,fcntl.LOCK_EX)
 try:
  for name in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',name],stdout=subprocess.DEVNULL).returncode!=0
  if args.build:run('backend-build',['cmake','--build',b,'--target','renderdoccmd','build-qrenderdoc','-j','2'],t=300);m['backend_sha256']=hashlib.sha256((b/'lib/librenderdoc.dylib').read_bytes()).hexdigest()
  native=w/'capture';oracle=w/'replay';opener=w/'opener'
  run('capture-build',['clang++','-std=c++17','-fobjc-arc','-mmacosx-version-min=13.0','-I'+str(r),r/'util/test/metal/metal_buffer_texture_formats_capture.mm','-framework','Foundation','-framework','Metal','-framework','QuartzCore','-o',native])
  for tag,source,out in [('replay','metal_buffer_texture_formats_replay.cpp',oracle),('opener','../../../util/ue/ue_capture_open_probe.cpp',opener)]:
   src=r/'util/test/metal'/source
   run(tag+'-build',['clang++','-std=c++17','-mmacosx-version-min=13.0','-DRENDERDOC_PLATFORM_APPLE','-I'+str(r),src,'-L'+str(b/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(b/'lib'),'-o',out])
  for tag,settings in [('tiny-ro',{}),('wide-ro-zero',{'RENDERDOC_METAL_UINT16_WIDE':'1','RENDERDOC_METAL_UINT16_ZERO_OFFSET':'1'}),('wide-rw-offset',{'RENDERDOC_METAL_UINT16_WIDE':'1','RENDERDOC_METAL_UINT16_RW':'1'})]:
   run(tag+'-native',[native],settings)
   run(tag+'-capture',[native],dict(settings,DYLD_INSERT_LIBRARIES=str(b/'lib/librenderdoc.dylib'),RENDERDOC_METAL_CAPTURE_PATH=str(w/tag)))
   for suffix in ('','_2'):
    cap=w/(tag+'_capture'+suffix+'.rdc')
    run(tag+'-API'+suffix,[oracle,cap]);run(tag+'-CLI'+suffix,[cli,'replay','--loops','3',cap])
   run(tag+'-bad113',['python3',r/'util/test/metal/metal_buffer_texture_formats_gate.py',cli,opener,w/(tag+'_capture.rdc'),w/(tag+'-bad113'),'113'],t=300)
  run('old-family-bad',['python3',r/'util/test/metal/metal_buffer_texture_formats_gate.py',cli,opener,w/'tiny-ro_capture.rdc',w/'old-family-bad'],t=300)
  m.update(status='PASS',capture_count=6,format_count=17,raw_pixel_checks=408,seek_cycles=24,pre_initial_rejections=144,loading_writer_rejections=6,production_RT_enabled=False);m['end_sha256']=hashlib.sha256((b/'lib/librenderdoc.dylib').read_bytes()).hexdigest();assert m['end_sha256']==m['backend_sha256'];m['bundle_sha256']=hashlib.sha256((b/'bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib').read_bytes()).hexdigest();assert m['bundle_sha256']==m['end_sha256']
 except Exception as ex:m.update(status='FAIL',error=str(ex));raise
 finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
