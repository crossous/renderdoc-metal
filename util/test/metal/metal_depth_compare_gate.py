# SPDX-License-Identifier: MIT
"""Serial bounded Native depth comparison and typed restoration check."""
from pathlib import Path
import argparse,fcntl,tempfile,os,subprocess,signal,json,hashlib,sys,time
import xml.etree.ElementTree as ET
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--work-dir',type=Path,required=True);p.add_argument('--independent',action='store_true');p.add_argument('--baseline',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';w=a.work_dir.resolve();assert not w.exists();w.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
m={'status':'RUNNING','backend_sha256':sha(b/'lib/librenderdoc.dylib'),'independent':a.independent,'baseline_expected_rejection':a.baseline,'scope':'Native depth compare resource/initial/sampler recovery; not RT or UE acceptance','checks':[]}
def run(tag,cmd,capture=False,expected=0):
    e={k:v for k,v in os.environ.items() if not k.startswith('RENDERDOC_') and k!='DYLD_INSERT_LIBRARIES'}
    e.update(MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
    if a.independent:e['RENDERDOC_METAL_DEPTH_INDEPENDENT']='1'
    if capture:e['DYLD_INSERT_LIBRARIES']=str(b/'lib/librenderdoc.dylib')
    start=time.monotonic()
    with (w/(tag+'.log')).open('w') as out,(w/(tag+'-renderdoc.log')).open('a') as keep:
        fcntl.flock(keep,fcntl.LOCK_SH);child=subprocess.Popen(list(map(str,cmd)),env=e,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
        try:code=child.wait(timeout=60)
        except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait();code=124
    text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace')
    hits=[s for s in ['Assertion failed','failed assertion','ForceCrash','OVERRUNNING CHUNK','m_ResourceMap.empty','m_ResourceRecords.empty'] if s in text]
    m['checks'].append(dict(tag=tag,exit=code,strict=hits,seconds=time.monotonic()-start));(w/'manifest.json').write_text(json.dumps(m,indent=2));print(tag,code,flush=True)
    assert code==expected and not hits,text[-3500:]
    if expected:assert 'Metal replay wait begin' not in text
    return text
try:
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
        run('shader-compile',['xcrun','-sdk','macosx','metal','-c',r/'util/test/metal/metal_depth_compare.metal',*(['-DINDEPENDENT_DEPTH=1'] if a.independent else []),'-o',w/'shader.air'])
        run('metallib',['xcrun','-sdk','macosx','metallib',w/'shader.air','-o',w/'shader.metallib'])
        sys.path.insert(0,str(r/'util/shader_tools'));from metal_air_processor import disassemble
        air=disassemble(w/'shader.metallib','depth_compare');(w/'entry.ll').write_text(air);assert '@air.sample_compare_depth_' in air
        run('native-build',['clang++','-std=c++17','-fobjc-arc','-I'+str(r),r/'util/test/metal/metal_depth_compare_capture.mm','-framework','Foundation','-framework','Metal','-o',w/'native'])
        run('replay-build',['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I'+str(r),r/'util/test/metal/metal_depth_compare_replay.cpp','-L'+str(b/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(b/'lib'),'-o',w/'replay'])
        for tag,capture in [('native',False),('capture',True)]:
            assert 'PASS Native' in run(tag,[w/'native',w/'depth',w/'shader.metallib'],capture)
            current=sha(w/'depth.native-depth.bin')
            if not capture:m['native_pixels_sha256']=current
            else:assert current==m['native_pixels_sha256']
        cap=w/'depth_capture.rdc';assert cap.exists();run('API',[w/'replay',cap,w/'depth.native-depth.bin'],expected=4 if a.baseline else 0);run('CLI',[b/'bin/renderdoccmd','replay','--loops','2',cap],expected=1 if a.baseline else 0)
        run('export',[b/'bin/renderdoccmd','convert','-f',cap,'-o',w/'original.zip.xml','-c','zip.xml'])
        if not a.baseline:
            run('opener-build',['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I'+str(r),r/'util/ue/ue_capture_open_probe.cpp','-L'+str(b/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(b/'lib'),'-o',w/'opener'])
            tree=ET.parse(w/'original.zip.xml');chunks=tree.find('./chunks');field=lambda c,n:c.find('./*[@name="'+n+'"]')
            removed=[c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::setBuffer' and field(c,'index').text==str(6 if a.independent else 3)];assert removed
            for c in removed:chunks.remove(c)
            for c in chunks:c.set('length',str(int(c.get('length','0'))+128))
            invalid=w/'missing-sampler-binding.zip.xml';tree.write(invalid,encoding='utf-8',xml_declaration=True);os.link(w/'original.zip',invalid.with_suffix(''))
            run('invalid-import',[b/'bin/renderdoccmd','convert','-f',invalid,'-o',w/'missing-sampler-binding.rdc','-c','rdc'])
            run('invalid-API',[w/'opener',w/'missing-sampler-binding.rdc'],expected=4);run('invalid-CLI',[b/'bin/renderdoccmd','replay','--loops','1',w/'missing-sampler-binding.rdc'],expected=1)
        assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256']
        m.update(status='PASS',capture_sha256=sha(cap),bundle_sha256=sha(b/'bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib'),sources={str(p.relative_to(r)):sha(p) for p in (r/'util/test/metal').glob('metal_depth_compare*') if p.is_file()})
except Exception as ex:m.update(status='FAIL',error=repr(ex));raise
finally:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
