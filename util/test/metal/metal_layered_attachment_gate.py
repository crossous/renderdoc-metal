# SPDX-License-Identifier: MIT
"""Generic layered color attachment initial state, partial range and seek verification."""
from pathlib import Path
import argparse,fcntl,tempfile,os,subprocess,signal,json,hashlib
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--work-dir',type=Path,required=True);p.add_argument('--baseline',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';w=a.work_dir.resolve();assert not w.exists();w.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
m={'status':'RUNNING','backend_sha256':sha(b/'lib/librenderdoc.dylib'),'scope':'Generic API original layered attachment/initial state; not RT or UE whole-frame acceptance','checks':[],'baseline_expected_rejection':a.baseline}
def run(tag,cmd,capture=False,expected=0,no_wait=False):
 e={k:v for k,v in os.environ.items() if not k.startswith('RENDERDOC_') and k!='DYLD_INSERT_LIBRARIES'}
 e.update(MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
 if capture:e['DYLD_INSERT_LIBRARIES']=str(b/'lib/librenderdoc.dylib')
 with (w/(tag+'.log')).open('w') as out,(w/(tag+'-renderdoc.log')).open('a') as keep:
  fcntl.flock(keep,fcntl.LOCK_SH);proc=subprocess.Popen(list(map(str,cmd)),env=e,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
  try:code=proc.wait(timeout=60)
  except subprocess.TimeoutExpired:os.killpg(proc.pid,signal.SIGKILL);proc.wait();code=124
 text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace')
 hits=[x for x in ['Assertion failed','failed assertion','ForceCrash','OVERRUNNING CHUNK','m_ResourceMap.empty','m_ResourceRecords.empty'] if x in text]
 if no_wait and 'Metal replay wait begin' in text:hits.append('GPU wait')
 m['checks'].append({'tag':tag,'exit':code,'expected':expected,'hits':hits});(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n');print(tag,code,flush=True)
 assert code==expected and not hits,text[-4000:];return text
try:
 with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX)
  for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
  run('native-build',['clang++','-std=c++17','-fobjc-arc','-I'+str(r),r/'util/test/metal/metal_layered_attachment_capture.mm','-framework','Foundation','-framework','Metal','-o',w/'native'])
  run('replay-build',['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I'+str(r),r/'util/test/metal/metal_layered_attachment_replay.cpp','-L'+str(b/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(b/'lib'),'-o',w/'replay'])
  for mode in ('volume','array-mip','volume-plane'):
   run(mode+'-native',[w/'native',w/mode,mode])
   if a.baseline and mode!='volume':continue
   run(mode+'-capture',[w/'native',w/mode,mode],True);cap=w/(mode+'_capture.rdc')
   run(mode+'-API',[w/'replay',cap,mode],expected=2 if a.baseline else 0,no_wait=a.baseline)
   run(mode+'-CLI',[b/'bin/renderdoccmd','replay','--loops','2',cap],expected=1 if a.baseline else 0,no_wait=a.baseline)
   run(mode+'-export',[b/'bin/renderdoccmd','convert','-f',cap,'-o',w/(mode+'.zip.xml'),'-c','zip.xml'])
  assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256'];m.update(status='BASELINE REPRODUCED' if a.baseline else 'PASS',bundle_sha256=sha(b/'bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib'))
except Exception as ex:m.update(status='FAIL',error=repr(ex));raise
finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
