# SPDX-License-Identifier: MIT
"""Check creation input identity, payload and timing on a real Native RT capture."""
from pathlib import Path
import argparse, copy, fcntl, hashlib, json, os, signal, subprocess, tempfile, zipfile
import xml.etree.ElementTree as ET

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-dir',type=Path,required=True)
    parser.add_argument('--work-dir',type=Path,required=True);args=parser.parse_args()
    r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug'
    source=args.source_dir.resolve();w=args.work_dir.resolve();assert not w.exists();w.mkdir()
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    m=dict(status='RUNNING',backend_sha256=sha(b/'lib/librenderdoc.dylib'),
           capture_sha256=sha(source/'query_capture.rdc'),checks=[],production_RT_enabled=False,
           scope='Actual original creation payload and timing contract; unrelated ordinary upload/dispatch removal is not a malformed oracle')
    def run(tag,command,expected):
        env={k:v for k,v in os.environ.items() if not k.startswith('RENDERDOC_') and k!='DYLD_INSERT_LIBRARIES'}
        env.update(RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
        with (w/(tag+'.log')).open('w') as out,(w/(tag+'-renderdoc.log')).open('a') as keep:
            fcntl.flock(keep,fcntl.LOCK_SH)
            child=subprocess.Popen(list(map(str,command)),env=env,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
            try:code=child.wait(timeout=45)
            except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait();code=124
        text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace')
        hits=[x for x in ['Assertion failed','failed assertion','ForceCrash','OVERRUNNING CHUNK','m_ResourceMap.empty','m_ResourceRecords.empty','Metal replay wait begin'] if x in text]
        m['checks'].append(dict(tag=tag,exit=code,hits=hits));(w/'manifest.json').write_text(json.dumps(m,indent=2));print(tag,code,flush=True)
        assert code==expected and not hits,text[-3000:]
    try:
        for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
        original=ET.parse(source/'original.zip.xml');f=lambda c,n:c.find('./*[@name="'+n+'"]')
        with zipfile.ZipFile(source/'original.zip') as z:blobs={n:z.read(n) for n in z.namelist()}
        for case in ('required-input-missing','birth-offset-mismatch','short-original-payload','birth-after-encoded-use'):
            tree=copy.deepcopy(original);chunks=tree.find('./chunks');patches={}
            binding=next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and f(c,'offset').text=='72' and f(c,'kind').text=='0')
            target=f(binding,'resource').text
            birth=next(c for c in chunks if c.get('name')=='MTLBuffer::CaptureHeapBirthContents' and f(c,'buffer').text==target)
            if case=='required-input-missing':chunks.remove(birth)
            elif case=='birth-offset-mismatch':f(birth,'offset').text=str(int(f(birth,'offset').text)+256)
            elif case=='short-original-payload':
                data=f(birth,'data');index=int(data.text);key=f'{index:06}';patches[key]=blobs[key][:-1];data.set('byteLength',str(len(patches[key])))
            else:
                use=next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::useResource' and f(c,'resource').text==target)
                chunks.remove(birth);chunks.insert(list(chunks).index(use)+1,birth)
            for c in chunks:c.set('length',str(int(c.get('length','0'))+128))
            xml=w/(case+'.zip.xml');tree.write(xml,encoding='utf-8',xml_declaration=True)
            with zipfile.ZipFile(xml.with_suffix(''),'w',compression=zipfile.ZIP_DEFLATED) as z:
                for key,data in blobs.items():z.writestr(key,patches.get(key,data))
            cap=w/(case+'.rdc');cli=b/'bin/renderdoccmd'
            run(case+'-import',[cli,'convert','-f',xml,'-o',cap,'-c','rdc'],0)
            run(case+'-API',[b/'metal-ray-b534/final-short/opener',cap],4)
            run(case+'-CLI',[cli,'replay','--loops','1',cap],1)
        assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256'];m.update(status='PASS')
    except Exception as ex:m.update(status='FAIL',error=repr(ex));raise
    finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')

if __name__=='__main__':
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX);main()
