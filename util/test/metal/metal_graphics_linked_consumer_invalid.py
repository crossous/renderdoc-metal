# SPDX-License-Identifier: MIT
"""Corrupt linked graphics inputs; require clean rejection before frame GPU work."""
from pathlib import Path
import argparse,copy,fcntl,tempfile,os,subprocess,signal,json,hashlib,struct,zipfile
import xml.etree.ElementTree as E
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--work-dir',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';w=a.work_dir.resolve();assert not w.exists();w.mkdir();s=a.source.resolve();sha=lambda q:hashlib.sha256(q.read_bytes()).hexdigest()
f=lambda c,n:c.find('./*[@name="'+n+'"]');tree=E.parse(s/'original.zip.xml')
with zipfile.ZipFile(s/'original.zip') as z:blobs={n:z.read(n) for n in z.namelist()}
m=dict(status='RUNNING',backend_sha256=sha(b/'lib/librenderdoc.dylib'),source_capture_sha256=sha(s/'graphics_capture.rdc'),checks=[])
def run(tag,cmd,expected=0,negative=False):
 env=os.environ.copy()
 for k in list(env):
  if k.startswith('RENDERDOC_') or k=='DYLD_INSERT_LIBRARIES':env.pop(k)
 env.update(MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
 with (w/(tag+'-renderdoc.log')).open('a') as keep,(w/(tag+'.log')).open('w') as out:
  fcntl.flock(keep,fcntl.LOCK_SH);q=subprocess.Popen(list(map(str,cmd)),env=env,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
  try:c=q.wait(timeout=60)
  except subprocess.TimeoutExpired:os.killpg(q.pid,signal.SIGKILL);q.wait();c=124
 text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace');markers=['Assertion failed','failed assertion','ForceCrash','OVERRUNNING CHUNK','m_ResourceMap.empty','m_ResourceRecords.empty']
 if negative:markers+=['Metal replay wait begin']
 hits=[x for x in markers if x in text];m['checks'].append(dict(tag=tag,exit=c,hits=hits));print(tag,c,flush=True);assert c==expected and not hits,text[-1800:]
 if negative and tag.split('-')[-1]=='API' and not tag.startswith('detached-linked'):assert 'descriptor preflight rejected' in text or 'descriptor initial data' in text,text[-1800:]
try:
 with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX)
  for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
  tags=['selector-outside','vertex-outside','missing-source','unknown-creation','oversized-stride','detached-linked']
  for tag in tags:
   t=copy.deepcopy(tree);cs=t.find('chunks');patches={}
   if tag in ['selector-outside','vertex-outside','unknown-creation']:
    size='8' if tag=='selector-outside' else '6' if tag=='vertex-outside' else '48'
    c=next(c for c in cs if c.get('name')=='MTLDevice::newBufferWithBytes' and f(c,'length').text==size);blob=f(c,'initialData');i=int(blob.text);data=bytearray(blobs[f'{i:06}'])
    if tag=='unknown-creation':data=bytearray();blob.set('byteLength','0')
    else:struct.pack_into('<I' if tag=='selector-outside' else '<H',data,4 if tag=='selector-outside' else 0,4)
    patches[i]=data
    resource=f(c,'Buffer').text
    for update in list(cs):
     if update.get('name')=='Internal_MTLBufferModifyCPUContents' and f(update,'Buffer').text==resource:
      if tag=='unknown-creation':cs.remove(update)
      else:
       assert f(update,'start').text=='0';patches[int(f(update,'data').text)]=data
   elif tag=='missing-source':
    c=next(c for c in cs if c.get('name')=='MTLCommandEncoder::DescriptorInlineBinding' and f(c,'index').text=='6' and f(c,'entry').text=='0');cs.remove(c)
   elif tag=='oversized-stride':
    c=next(c for c in cs if c.get('name')=='MTLRenderCommandEncoder::setVertexBytes' and f(c,'index').text=='6');data=f(c,'data');assert len(data)==32;data[12].text='64'
   elif tag=='detached-linked':
    c=next(c for c in cs if c.get('name','').startswith('MTLDevice::newRenderPipelineStateWithDescriptor'));f(f(c,'descriptor'),'vertexLinkedFunctions').find('./array[@name="functions"]').clear()
   for c in cs:c.set('length',str(int(c.get('length','0'))+128))
   xml=w/(tag+'.zip.xml');t.write(xml,encoding='utf-8',xml_declaration=True)
   with zipfile.ZipFile(xml.with_suffix(''),'w',zipfile.ZIP_DEFLATED) as z:
    for n,v in blobs.items():z.writestr(n,patches.get(int(n),v))
   cap=w/(tag+'.rdc');run(tag+'-import',[b/'bin/renderdoccmd','convert','-f',xml,'-o',cap,'-c','rdc'])
   run(tag+'-API',[b/'metal-ray-b534/final-short/opener',cap],4,True);run(tag+'-CLI',[b/'bin/renderdoccmd','replay','--loops','1',cap],1,True)
  assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256'];m.update(status='PASS',groups=len(tags),rejected_checks=len(tags)*2)
except Exception as ex:m.update(status='FAIL',error=repr(ex));raise
finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
