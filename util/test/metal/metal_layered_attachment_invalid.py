# SPDX-License-Identifier: MIT
"""Reject genuine attachment range and required initial-state corruption before GPU work."""
from pathlib import Path
import argparse,copy,fcntl,tempfile,hashlib,json,os,subprocess,signal,zipfile
import xml.etree.ElementTree as ET
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-dir',type=Path,required=True);p.add_argument('--work-dir',type=Path,required=True);a=p.parse_args()
 r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';source=a.source_dir.resolve();w=a.work_dir.resolve();assert not w.exists();w.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 m={'status':'RUNNING','backend_sha256':sha(b/'lib/librenderdoc.dylib'),'scope':'Real API subresource/range and necessary background input corruption; no undefined-data equality oracle','checks':[]}
 def run(tag,cmd,expected):
  env={k:v for k,v in os.environ.items() if not k.startswith('RENDERDOC_') and k!='DYLD_INSERT_LIBRARIES'}
  env.update(RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
  with (w/(tag+'.log')).open('w') as out,(w/(tag+'-renderdoc.log')).open('a') as keep:
   fcntl.flock(keep,fcntl.LOCK_SH);child=subprocess.Popen(list(map(str,cmd)),env=env,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
   try:code=child.wait(timeout=45)
   except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait();code=124
  text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace');hits=[x for x in ['Assertion failed','failed assertion','ForceCrash','OVERRUNNING CHUNK','m_ResourceMap.empty','m_ResourceRecords.empty','Metal replay wait begin'] if x in text]
  m['checks'].append({'tag':tag,'exit':code,'expected':expected,'hits':hits});(w/'manifest.json').write_text(json.dumps(m,indent=2));print(tag,code,flush=True);assert code==expected and not hits,text[-3000:]
 try:
  for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
  f=lambda c,n:c.find('./*[@name="'+n+'"]')
  for case in ('layers-out-of-range','layers-overflow','mip-out-of-range','slice-out-of-range','extent-out-of-range','required-initial-missing','required-initial-short'):
   mode='array-mip' if case in ('slice-out-of-range','extent-out-of-range') else 'volume'
   tree=ET.parse(source/(mode+'.zip.xml'));chunks=tree.find('chunks');patches={}
   with zipfile.ZipFile(source/(mode+'.zip')) as z:blobs={n:z.read(n) for n in z.namelist()}
   passes=[c for c in chunks if c.get('name')=='MTLCommandBuffer::renderCommandEncoderWithDescriptor'];descriptor=f(passes[-2],'descriptor');attachment=f(descriptor,'colorAttachments')[0]
   if case=='layers-out-of-range':f(descriptor,'renderTargetArrayLength').text='9'
   elif case=='layers-overflow':f(descriptor,'renderTargetArrayLength').text=str(2**64-1)
   elif case=='mip-out-of-range':f(attachment,'level').text='2'
   elif case=='slice-out-of-range':f(attachment,'slice').text='5'
   elif case=='extent-out-of-range':f(descriptor,'renderTargetWidth').text='15'
   else:
    initial=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and f(c,'type').get('string')=='eResTexture')
    if case=='required-initial-missing':chunks.remove(initial)
    else:
     data=f(initial,'Contents');key=f'{int(data.text):06}';patches[key]=blobs[key][:-1];data.set('byteLength',str(len(patches[key])))
   for c in chunks:c.set('length',str(int(c.get('length','0'))+128))
   xml=w/(case+'.zip.xml');tree.write(xml,encoding='utf-8',xml_declaration=True)
   with zipfile.ZipFile(xml.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as z:
    for k,data in blobs.items():z.writestr(k,patches.get(k,data))
   cap=w/(case+'.rdc');run(case+'-import',[b/'bin/renderdoccmd','convert','-f',xml,'-o',cap,'-c','rdc'],0)
   run(case+'-API',[b/'metal-ray-b534/final-short/opener',cap],4);run(case+'-CLI',[b/'bin/renderdoccmd','replay','--loops','1',cap],1)
  assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256'];m['status']='PASS'
 except Exception as ex:m.update(status='FAIL',error=repr(ex));raise
 finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
if __name__=='__main__':
 with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:fcntl.flock(lock,fcntl.LOCK_EX);main()
