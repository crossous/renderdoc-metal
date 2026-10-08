# SPDX-License-Identifier: MIT
"""Bounded independent stencil-aspect restoration and real contract negatives."""
from pathlib import Path
import argparse, copy, fcntl, hashlib, json, os, signal, subprocess, tempfile
import xml.etree.ElementTree as ET

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir',type=Path,required=True);args=parser.parse_args()
    r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';w=args.work_dir.resolve()
    assert not w.exists();w.mkdir()
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    sources=[r/'util/test/metal'/('metal_stencil_aspect'+suffix) for suffix in ('.metal','_capture.mm','_replay.cpp','_gate.py')]
    m=dict(status='RUNNING',backend_sha256=sha(b/'lib/librenderdoc.dylib'),
           bundle_sha256=sha(b/'bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib'),
           sources_sha256={str(p.relative_to(r)):sha(p) for p in sources},checks=[],captures=[],production_RT_enabled=False)
    save=lambda:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    def run(tag,command,capture=False,expected=0):
        e={k:v for k,v in os.environ.items() if not k.startswith('RENDERDOC_') and k!='DYLD_INSERT_LIBRARIES'}
        e.update(MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
        if capture:e['DYLD_INSERT_LIBRARIES']=str(b/'lib/librenderdoc.dylib')
        with (w/(tag+'.log')).open('w') as out,(w/(tag+'-renderdoc.log')).open('a') as keep:
            fcntl.flock(keep,fcntl.LOCK_SH)
            p=subprocess.Popen(list(map(str,command)),env=e,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
            try:code=p.wait(timeout=60)
            except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();code=124
        text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace')
        hits=[s for s in ('Assertion failed','failed assertion','ForceCrash','OVERRUNNING CHUNK','m_ResourceMap.empty','m_ResourceRecords.empty') if s in text]
        m['checks'].append(dict(tag=tag,exit=code,strict=hits));save();print(tag,code,flush=True)
        assert code==expected and not hits,text[-4500:]
        return text
    try:
        for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
        run('shader-compile',['xcrun','-sdk','macosx','metal','-c',sources[0],'-o',w/'shader.air'])
        run('shader-link',['xcrun','-sdk','macosx','metallib',w/'shader.air','-o',w/'shader.metallib'])
        run('capture-build',['clang++','-std=c++17','-fobjc-arc','-I'+str(r),sources[1],'-framework','Foundation','-framework','Metal','-o',w/'native'])
        link=['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I'+str(r),'-L'+str(b/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(b/'lib')]
        run('replay-build',link+[sources[2],'-o',w/'replay'])
        run('opener-build',link+[r/'util/ue/ue_capture_open_probe.cpp','-o',w/'opener'])
        cli=b/'bin/renderdoccmd'
        for mode in ('2d','array'):
            run(mode+'-native',[w/'native',w/mode,w/'shader.metallib',mode])
            run(mode+'-capture',[w/'native',w/mode,w/'shader.metallib',mode],capture=True)
            cap=w/(mode+'_capture.rdc')
            run(mode+'-API',[w/'replay',cap,mode]);run(mode+'-CLI',[cli,'replay','--loops','2',cap])
            m['captures'].append(dict(mode=mode,path=str(cap),sha256=sha(cap),event_selections=6));save()
        xml=w/'array.zip.xml';run('export',[cli,'convert','-f',w/'array_capture.rdc','-o',xml,'-c','zip.xml'])
        original=ET.parse(xml);field=lambda c,n:c.find('./*[@name="'+n+'"]')
        for case in ('missing-parent','mip-overflow','slice-overflow','wrong-aspect','missing-format-view-usage','aspect-as-allocation'):
            tree=copy.deepcopy(original);chunks=tree.find('./chunks')
            view=next(c for c in chunks if c.get('name')=='MTLTexture::newTextureViewWithPixelFormat')
            if case=='missing-parent':field(view,'Source').text='999999999'
            elif case=='mip-overflow':field(field(view,'levels'),'location').text='4'
            elif case=='slice-overflow':field(field(view,'slices'),'location').text='3'
            elif case=='wrong-aspect':field(view,'format').text='70'
            else:
                parent=next(c for c in chunks if c.get('name')=='MTLHeap::newTexture(offset)')
                field(field(parent,'descriptor'),'pixelFormat' if case=='aspect-as-allocation' else 'usage').text='261' if case=='aspect-as-allocation' else '5'
            for c in chunks:c.set('length',str(int(c.get('length','0'))+128))
            target=w/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
            os.link(xml.with_suffix(''),target.with_suffix(''))
            cap=w/(case+'.rdc');run(case+'-import',[cli,'convert','-f',target,'-o',cap,'-c','rdc'])
            output=run(case+'-API',[w/'opener',cap],expected=4)
            assert 'Metal replay wait begin' not in output
        assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256']==m['bundle_sha256']
        m.update(status='PASS',scope='Native aspect projection, defined parent clears and GPU output; not UE or RT enablement')
    except Exception as ex:m.update(status='FAIL',error=repr(ex));raise
    finally:save()

if __name__=='__main__':
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX);main()
