# SPDX-License-Identifier: MIT
"""Distinguish invalidated initial generations from missing live and reintroduced addresses."""
from pathlib import Path
import argparse,copy,fcntl,tempfile,hashlib,json,os,subprocess,signal,zipfile
import xml.etree.ElementTree as E
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-dir',type=Path,required=True);p.add_argument('--work-dir',type=Path,required=True);a=p.parse_args()
 r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';s=a.source_dir.resolve();w=a.work_dir.resolve();assert not w.exists();w.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 m={'status':'RUNNING','backend_sha256':sha(b/'lib/librenderdoc.dylib'),'capture_sha256':sha(s/'query_capture.rdc'),'scope':'Initial retired generation, missing live binding and submission-owned address rewrite; no generic missing-slot bypass','checks':[]}
 def run(tag,cmd,expected=0,positive=False):
  env={k:v for k,v in os.environ.items() if not k.startswith('RENDERDOC_') and k!='DYLD_INSERT_LIBRARIES'}
  env.update(RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
  if positive:env.update(RENDERDOC_METAL_RUNTIME_NATIVE_QUERY='1',RENDERDOC_METAL_RUNTIME_QUERY_DYNAMIC_HEAP='1',RENDERDOC_METAL_RUNTIME_RMW_OFFSET='24',RENDERDOC_METAL_RUNTIME_QUERY_INSTANCES='3',RENDERDOC_METAL_RUNTIME_QUERY_ACTIVE_INSTANCE='2',RENDERDOC_METAL_RUNTIME_QUERY_HEADER_OFFSET='128',RENDERDOC_METAL_RUNTIME_QUERY_CONTRIBUTION_OFFSET='32',RENDERDOC_METAL_RUNTIME_QUERY_CREATION_INPUT='4',RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_HEAP='1',RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_SIZE='12288',RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_OFFSET='12280',RENDERDOC_METAL_RUNTIME_QUERY_ALIAS_OFFSET='768',RENDERDOC_METAL_RUNTIME_QUERY_RETIRED_ROW='1')
  with (w/(tag+'.log')).open('w') as out,(w/(tag+'-renderdoc.log')).open('a') as keep:
   fcntl.flock(keep,fcntl.LOCK_SH);child=subprocess.Popen(list(map(str,cmd)),env=env,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
   try:code=child.wait(timeout=45)
   except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait();code=124
  text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace')
  hits=[x for x in ['Assertion failed','failed assertion','ForceCrash','OVERRUNNING CHUNK','m_ResourceMap.empty','m_ResourceRecords.empty'] if x in text]
  if expected and 'Metal replay wait begin' in text:hits.append('GPU wait')
  m['checks'].append({'tag':tag,'exit':code,'expected':expected,'hits':hits});(w/'manifest.json').write_text(json.dumps(m,indent=2));print(tag,code,flush=True);assert code==expected and not hits,text[-3000:]
 try:
  for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
  original=E.parse(s/'original.zip.xml');f=lambda c,n:c.find('./*[@name="'+n+'"]')
  with zipfile.ZipFile(s/'original.zip') as z:blobs={n:z.read(n) for n in z.namelist()}
  for case in ('retirement-generation-mismatch','retirement-missing-live-source','active-binding-missing','CPU-reintroduces-stale-address','CPU-writes-null-control'):
   tree=copy.deepcopy(original);chunks=tree.find('chunks');patches={}
   retirement=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and f(c,'event').text=='1' and f(c,'offset').text=='96');table=f(retirement,'buffer').text
   if case=='retirement-generation-mismatch':f(retirement,'generation').text='2'
   elif case=='retirement-missing-live-source':chunks.remove(retirement)
   elif case=='active-binding-missing':chunks.remove(next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and f(c,'buffer').text==table and f(c,'offset').text=='72' and f(c,'kind').text=='0'))
   else:
    update=copy.deepcopy(next(c for c in chunks if c.get('name')=='Internal_MTLBufferModifyCPUContents'))
    f(update,'Buffer').text=table;f(update,'start').text='96';f(update,'size').text='8';data=f(update,'data')
    index=max(int(n) for n in blobs if n.isdigit())+1;key=f'{index:06}';data.text=str(index);data.set('byteLength','8')
    value=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotEvent' and f(c,'buffer').text==table and f(c,'offset').text=='96' and f(c,'event').text=='2')
    patches[key]=bytes(8) if case.endswith('control') else blobs[f'{int(f(value,"data").text):06}'][:8]
    commit=next(c for c in chunks if c.get('name')=='MTLCommandBuffer::commit');chunks.insert(list(chunks).index(commit),update)
   for c in chunks:c.set('length',str(int(c.get('length','0'))+128))
   xml=w/(case+'.zip.xml');tree.write(xml,encoding='utf-8',xml_declaration=True)
   with zipfile.ZipFile(xml.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as z:
    for k,data in {**blobs,**patches}.items():z.writestr(k,data)
   cap=w/(case+'.rdc');run(case+'-import',[b/'bin/renderdoccmd','convert','-f',xml,'-o',cap,'-c','rdc'])
   positive=case.endswith('control');run(case+'-API',[s/'replay' if positive else b/'metal-ray-b534/final-short/opener',cap],0 if positive else 4,positive)
   run(case+'-CLI',[b/'bin/renderdoccmd','replay','--loops','1',cap],0 if positive else 1,positive)
  assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256'];m['status']='PASS'
 except Exception as ex:m.update(status='FAIL',error=repr(ex));raise
 finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
if __name__=='__main__':
 with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:fcntl.flock(lock,fcntl.LOCK_EX);main()
