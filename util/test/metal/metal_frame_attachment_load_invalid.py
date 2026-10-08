# SPDX-License-Identifier: MIT
"""Corrupt frame MRT/depth/stencil inputs; reject before frame GPU work."""
from pathlib import Path
import argparse,copy,fcntl,tempfile,os,subprocess,signal,json,hashlib,struct,zipfile
import xml.etree.ElementTree as E
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--mode',default='load');p.add_argument('--work-dir',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';w=a.work_dir.resolve();assert not w.exists();w.mkdir();s=a.source.resolve();sha=lambda q:hashlib.sha256(q.read_bytes()).hexdigest()
f=lambda c,n:c.find('./*[@name="'+n+'"]');tree=E.parse(s/(a.mode+'.zip.xml'))
with zipfile.ZipFile(s/(a.mode+'.zip')) as z:blobs={n:z.read(n) for n in z.namelist()}
m=dict(status='RUNNING',backend_sha256=sha(b/'lib/librenderdoc.dylib'),source_capture_sha256=sha(s/(a.mode+'_capture.rdc')),checks=[])
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

try:
 with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX)
  for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
  tags=['color-load-enum','color-store-options','depth-load-enum','stencil-load-enum','color-level','color-slice','color-depth-plane','color-resolve','duplicate-target','depth-slice','stencil-mismatch','missing-color-store','missing-depth-store','missing-stencil-store','late-birth','absent-birth','pipeline-color-mismatch','pipeline-depth-mismatch','missing-vertex']
  for tag in tags:
   t=copy.deepcopy(tree);cs=t.find('chunks');patches={}
   first=next(c for c in cs if c.get('name')=='MTLCommandBuffer::renderCommandEncoderWithDescriptor');desc=f(first,'descriptor');colors=f(desc,'colorAttachments');dep=f(desc,'depthAttachment');stencil=f(desc,'stencilAttachment')
   if tag in ('color-load-enum','depth-load-enum','stencil-load-enum'):
    target=colors[0] if tag.startswith('color') else dep if tag.startswith('depth') else stencil;f(target,'loadAction').text='99'
   elif tag=='color-store-options':f(colors[0],'storeActionOptions').text='1'
   elif tag in ('color-level','color-slice','color-depth-plane','color-resolve'):
    name={'color-level':'level','color-slice':'slice','color-depth-plane':'depthPlane','color-resolve':'resolveTexture'}[tag];f(colors[0],name).text=f(colors[0],'texture').text if tag=='color-resolve' else '1'
   elif tag=='duplicate-target':f(colors[1],'texture').text=f(colors[0],'texture').text
   elif tag=='depth-slice':f(dep,'slice').text='1'
   elif tag=='stencil-mismatch':f(stencil,'texture').text='0'
   elif tag.startswith('missing-') and tag.endswith('-store'):
    method={'missing-color-store':'setColorStoreAction','missing-depth-store':'setDepthStoreAction','missing-stencil-store':'setStencilStoreAction'}[tag]
    c=next(c for c in cs if c.get('name')=='MTLRenderCommandEncoder::'+method);cs.remove(c)
   elif tag in ('late-birth','absent-birth'):
    c=next(c for c in cs if c.get('name','').startswith('MTLHeap::newTexture') and f(c,'Texture').text==f(colors[0],'texture').text);cs.remove(c)
    if tag=='late-birth':cs.insert(list(cs).index(first)+1,c)
   else:
    c=next(c for c in cs if c.get('name','').startswith('MTLDevice::newRenderPipelineStateWithDescriptor'));pd=f(c,'descriptor')
    if tag=='missing-vertex':f(pd,'vertexFunction').text='0'
    elif tag=='pipeline-depth-mismatch':f(pd,'depthAttachmentPixelFormat').text='0'
    else:f(f(pd,'colorAttachments')[0],'pixelFormat').text='10'
   for c in cs:c.set('length',str(int(c.get('length','0'))+128))
   xml=w/(tag+'.zip.xml');t.write(xml,encoding='utf-8',xml_declaration=True)
   with zipfile.ZipFile(xml.with_suffix(''),'w',zipfile.ZIP_DEFLATED) as z:
    for n,v in blobs.items():z.writestr(n,patches.get(int(n),v))
   cap=w/(tag+'.rdc');run(tag+'-import',[b/'bin/renderdoccmd','convert','-f',xml,'-o',cap,'-c','rdc'])
   run(tag+'-API',[b/'metal-ray-b534/final-short/opener',cap],4,True);run(tag+'-CLI',[b/'bin/renderdoccmd','replay','--loops','1',cap],1,True)
  assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256'];m.update(status='PASS',groups=len(tags),rejected_checks=len(tags)*2)
except Exception as ex:m.update(status='FAIL',error=repr(ex));raise
finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
