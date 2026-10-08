# SPDX-License-Identifier: MIT
"""Bounded independent writable texture synchronization and full Native output."""
from pathlib import Path
import argparse, copy, fcntl, hashlib, json, os, signal, subprocess, tempfile, time, sys, re, shutil
import xml.etree.ElementTree as ET

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir',type=Path,required=True);parser.add_argument('--mode',choices=['2d','array'],required=True);parser.add_argument('--baseline',action='store_true');parser.add_argument('--constant-table',action='store_true');parser.add_argument('--constant-variant',action='store_true');parser.add_argument('--integer-bit-pattern',action='store_true');parser.add_argument('--integer-variant',action='store_true');parser.add_argument('--unknown-usage',action='store_true');parser.add_argument('--contract-source',type=Path);args=parser.parse_args()
    if args.integer_bit_pattern and args.mode!='2d':parser.error('Integer ABI uses its own 2D workload')
    if args.integer_variant and not args.integer_bit_pattern:parser.error('Variant requires integer ABI workload')
    if args.constant_variant and not args.constant_table:parser.error('Constant variant requires constant-table workload')
    if args.constant_table and args.integer_bit_pattern:parser.error('Use separate constant and integer workloads')
    r=Path(__file__).resolve().parents[3];b=r/'build-macos-debug';w=args.work_dir.resolve()
    assert not w.exists();w.mkdir()
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    sources=[r/'util/test/metal'/('metal_texture_fence'+suffix) for suffix in ('.metal','_capture.mm','_replay.cpp','_gate.py')]
    m=dict(status='RUNNING',backend_sha256=sha(b/'lib/librenderdoc.dylib'),
           bundle_sha256=sha(b/'bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib'),
           sources_sha256={str(p.relative_to(r)):sha(p) for p in sources},checks=[],captures=[],production_RT_enabled=False,unknown_usage=args.unknown_usage,integer_bit_pattern=args.integer_bit_pattern,integer_variant=args.integer_variant)
    save=lambda:(w/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    def run(tag,command,capture=False,expected=0):
        started=time.monotonic()
        e={k:v for k,v in os.environ.items() if not k.startswith('RENDERDOC_') and k!='DYLD_INSERT_LIBRARIES'}
        e.update(MTL_DEBUG_LAYER='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1',RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_DEBUG_LOG_FILE=str(w/(tag+'-renderdoc.log')))
        if args.constant_table:e['RENDERDOC_METAL_FENCE_CONSTANT_TABLE']='1'
        if args.constant_variant:e['RENDERDOC_METAL_FENCE_CONSTANT_VARIANT']='1'
        if args.integer_bit_pattern:e['RENDERDOC_METAL_FENCE_INTEGER_BITS']='1'
        if args.integer_variant:e['RENDERDOC_METAL_FENCE_INTEGER_VARIANT']='1'
        if args.unknown_usage:e['RENDERDOC_METAL_FENCE_UNKNOWN_USAGE']='1'
        if capture:e['DYLD_INSERT_LIBRARIES']=str(b/'lib/librenderdoc.dylib')
        with (w/(tag+'.log')).open('w') as out,(w/(tag+'-renderdoc.log')).open('a') as keep:
            fcntl.flock(keep,fcntl.LOCK_SH)
            p=subprocess.Popen(list(map(str,command)),env=e,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
            try:code=p.wait(timeout=60)
            except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();code=124
        text=(w/(tag+'.log')).read_text(errors='replace')+(w/(tag+'-renderdoc.log')).read_text(errors='replace')
        hits=[s for s in ('Assertion failed','failed assertion','ForceCrash','OVERRUNNING CHUNK','m_ResourceMap.empty','m_ResourceRecords.empty') if s in text]
        if expected:assert 'Metal replay wait begin' not in text
        m['checks'].append(dict(tag=tag,exit=code,strict=hits,seconds=time.monotonic()-started));save();print(tag,code,flush=True)
        assert code==expected and not hits,text[-4500:]
        return text
    try:
        for n in ('UnrealEditor','qrenderdoc'):assert subprocess.run(['pgrep','-x',n],stdout=subprocess.DEVNULL).returncode!=0
        cli=b/'bin/renderdoccmd'
        if args.contract_source:
            cap=args.contract_source.resolve()/(args.mode+'_capture.rdc')
        else:
            run('shader-compile',['xcrun','-sdk','macosx','metal','-c',sources[0],*(['-DINTEGER_BIT_PATTERN=1'] if args.integer_bit_pattern else []),*(['-DCONSTANT_TABLE=1'] if args.constant_table else []),*(['-DCONSTANT_VARIANT=1'] if args.constant_variant else []),'-o',w/'shader.air'])
            run('shader-link',['xcrun','-sdk','macosx','metallib',w/'shader.air','-o',w/'shader.metallib'])
            if args.constant_table:
                sys.path.insert(0,str(r/'util/shader_tools'));from metal_air_processor import disassemble
                module=disassemble(w/'shader.metallib','fence_'+args.mode)
                (w/'constant-table.ll').write_text(module)
                assert re.search(r'^@[^\n]+addrspace\(2\) constant \[',module,re.M)
                m['constant_table']=True;m['constant_variant']=args.constant_variant;save()
            if args.integer_bit_pattern:
                sys.path.insert(0,str(r/'util/shader_tools'));from metal_air_processor import disassemble
                shutil.copyfile(w/'shader.metallib',w/'source-signed.metallib')
                module=disassemble(w/'source-signed.metallib','fence_2d')
                (w/'source-signed.ll').write_text(module)
                # Keep the original signed MSL texture type/root metadata and
                # every instruction/operand. i32 texel bits survive the Native
                # unsigned integer read/write ABI used by converted shaders.
                changed,n=re.subn(r'@air\.((?:read|write)_texture_2d(?:\.rtz)?)\.s\.v4i32',r'@air.\1.u.v4i32',module)
                assert n>=4
                asm=w/'integer-abi.ll';asm.write_text(changed)
                target=re.search(r'^target triple = "([^"\n]+)"',module,re.M)[1]
                language=re.search(r'!"Metal", i32 ([0-9]+), i32 ([0-9]+)',module)
                run('integer-ABI-assemble',['xcrun','-sdk','macosx','metal','-c','-O0','-std=metal'+language[1]+'.'+language[2],'-target',target,asm,'-o',w/'integer-abi.air'])
                run('integer-ABI-link',['xcrun','-sdk','macosx','metallib',w/'integer-abi.air','-o',w/'shader.metallib'])
                m['integer_ABI_calls_and_declarations']=n;save()
            run('capture-build',['clang++','-std=c++17','-fobjc-arc','-I'+str(r),sources[1],'-framework','Foundation','-framework','Metal','-o',w/'native'])
            link=['clang++','-std=c++17','-DRENDERDOC_PLATFORM_APPLE','-I'+str(r),'-L'+str(b/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(b/'lib')]
            run('replay-build',link+[sources[2],'-o',w/'replay'])
            run('opener-build',link+[r/'util/ue/ue_capture_open_probe.cpp','-o',w/'opener'])
            for mode in (args.mode,):
                run(mode+'-native',[w/'native',w/mode,w/'shader.metallib',mode])
                run(mode+'-capture',[w/'native',w/mode,w/'shader.metallib',mode],capture=True)
                cap=w/(mode+'_capture.rdc')
                run(mode+'-API',[w/'replay',cap,mode],expected=2 if args.baseline else 0);run(mode+'-CLI',[cli,'replay','--loops','2',cap],expected=1 if args.baseline else 0)
                m['captures'].append(dict(mode=mode,path=str(cap),sha256=sha(cap),event_selections=0 if args.baseline else 6));save()
        if not args.baseline:
            xml=w/'original.zip.xml';run('export',[cli,'convert','-f',cap,'-o',xml,'-c','zip.xml'])
            original=ET.parse(xml);field=lambda c,n:c.find('./*[@name="'+n+'"]')
            for case in ('missing-texture-binding','readonly-descriptor')+(() if args.unknown_usage else ('missing-native-write-usage',))+(('float-integer-family',) if args.integer_bit_pattern else ()):
                tree=copy.deepcopy(original);chunks=tree.find('./chunks')
                if case=='missing-texture-binding':
                    chunks.remove(next(c for c in chunks if c.get('name')=='MTLBuffer::DescriptorSlotBinding' and field(c,'kind').text=='1'))
                elif case=='readonly-descriptor':
                    for c in chunks:
                        if c.get('name')=='MTLBuffer::DescriptorSlotEvent':field(c,'descriptorType').text='4'
                elif case=='float-integer-family':
                    factory=next(c for c in chunks if c.get('name')=='MTLHeap::newTexture(offset)')
                    field(field(factory,'descriptor'),'pixelFormat').text='105' if args.integer_variant else '55'
                    view=next(c for c in chunks if c.get('name')=='MTLTexture::newTextureViewWithPixelFormat')
                    field(view,'format').text='105' if args.integer_variant else '55'
                else:
                    factory=next(c for c in chunks if c.get('name')=='MTLHeap::newTexture(offset)')
                    usage=field(field(factory,'descriptor'),'usage');usage.text=str(int(usage.text)&~2)
                for c in chunks:c.set('length',str(int(c.get('length','0'))+128))
                target=w/(case+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
                os.link(xml.with_suffix(''),target.with_suffix(''))
                damaged=w/(case+'.rdc');run(case+'-import',[cli,'convert','-f',target,'-o',damaged,'-c','rdc'])
                run(case+'-API',[(args.contract_source.resolve() if args.contract_source else w)/'opener',damaged],expected=4)
                run(case+'-CLI',[cli,'replay','--loops','1',damaged],expected=1)
        assert sha(b/'lib/librenderdoc.dylib')==m['backend_sha256']==m['bundle_sha256']
        m.update(status='PASS',scope='Native texture write/fence/read on typed writable view; not UE or RT enablement',baseline_expected_rejection=args.baseline)
    except Exception as ex:m.update(status='FAIL',error=repr(ex));raise
    finally:save()

if __name__=='__main__':
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX);main()
