#!/usr/bin/env python3
"""Native API texture recovery variations, independent of engine/pass identities."""
import argparse, copy, fcntl, hashlib, json, os, re, signal, subprocess, tempfile
from pathlib import Path
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--source-compiled', action='store_true', help='Native JIT library without captured AIR; access display remains unknown')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    build = (args.build_dir or repo/'build-macos-debug').resolve()
    work = args.work_dir.resolve(); work.mkdir(parents=True, exist_ok=True)
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    manifest = dict(status='RUNNING', backend_sha256=sha(build/'lib/librenderdoc.dylib'),
                    checks=[], variants=[], source_compiled=args.source_compiled, production_RT_enabled=False)
    env = {k:v for k,v in os.environ.items() if not k.startswith('RENDERDOC_METAL_') and k!='DYLD_INSERT_LIBRARIES'}
    manifest['fixture_sources_sha256']={str(p.relative_to(repo)):sha(p) for p in (repo/'util/test/metal/metal_descriptor_frame_family_capture.mm',repo/'util/test/metal/metal_descriptor_frame_family_replay.mm')}
    env['MTL_DEBUG_LAYER']='1'
    def save(): (work/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    def run(tag, command, extra=None, expected=0):
        current=dict(env, **(extra or {})); current['RENDERDOC_DEBUG_LOG_FILE']=str(work/(tag+'-renderdoc.log'))
        with (work/(tag+'.log')).open('w') as output:
            process=subprocess.Popen(list(map(str,command)),env=current,stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
            try: code=process.wait(timeout=60)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid,signal.SIGKILL);process.wait();code=124
        output=(work/(tag+'.log')).read_text(errors='replace')
        logpath=work/(tag+'-renderdoc.log')
        backend=logpath.read_text(errors='replace') if logpath.exists() else ''
        bad=('Assertion failed','failed assertion','OVERRUNNING CHUNK','Unexpected Metal resource type','m_ResourceMap.empty')
        passed=code==expected and not any(s in output+backend for s in bad)
        manifest['checks'].append(dict(tag=tag,exit=code,passed=passed));save()
        if not passed: raise RuntimeError(tag+': '+output[-1600:]+backend[-1600:])
        print(tag,code,flush=True);return output
    try:
        for name in ('UnrealEditor','qrenderdoc'):
            assert subprocess.run(['pgrep','-x',name],stdout=subprocess.DEVNULL).returncode != 0, name+' running'
        flags=['clang++','-std=c++17','-fobjc-arc','-mmacosx-version-min=13.0','-I'+str(repo),'-framework','Foundation','-framework','Metal']
        run('build-capture',flags+['-framework','QuartzCore',repo/'util/test/metal/metal_descriptor_frame_family_capture.mm','-o',work/'capture'])
        flags+=['-DRENDERDOC_PLATFORM_APPLE','-L'+str(build/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(build/'lib')]
        run('build-replay',flags+[repo/'util/test/metal/metal_descriptor_frame_family_replay.mm','-o',work/'replay'])
        run('build-opener',flags+[repo/'util/ue/ue_capture_open_probe.cpp','-o',work/'opener'])
        cli=build/'bin/renderdoccmd'
        variants=[('half-wide',769,17,25,5,0,None,1,3),
                  ('half-multiview',513,129,25,5,4096,1,3,3),
                  ('float-wide',1025,9,55,4,8192,None,1,3),
                  ('float-multiview',769,17,55,5,16384,2,2,3),
                  ('many-dispatches',33,17,25,4,4096,None,1,132),
                  ('array-subview',67,19,55,0,4096,None,1,3)]
        for tag,width,height,format,mips,offset,base,count,dispatches in variants:
            settings=dict(RENDERDOC_METAL_FRAME_GENERIC_WIDTH=str(width),RENDERDOC_METAL_FRAME_GENERIC_HEIGHT=str(height),
                          RENDERDOC_METAL_FRAME_GENERIC_FORMAT=str(format),RENDERDOC_METAL_FRAME_R16_MIPS=str(mips),
                          RENDERDOC_METAL_FRAME_GENERIC_OFFSET=str(offset),RENDERDOC_METAL_FRAME_FAMILY_2D='1',
                          RENDERDOC_METAL_FRAME_FAMILY_EXTENDED_2D='1',RENDERDOC_METAL_FRAME_FAMILY_DISPATCHES=str(dispatches))
            if tag=='array-subview':
                for key in ('RENDERDOC_METAL_FRAME_FAMILY_2D','RENDERDOC_METAL_FRAME_FAMILY_EXTENDED_2D','RENDERDOC_METAL_FRAME_R16_MIPS'):settings.pop(key)
                settings.update(RENDERDOC_METAL_FRAME_FAMILY_ARRAY='1',RENDERDOC_METAL_FRAME_GENERIC_LAYERS='19',RENDERDOC_METAL_FRAME_GENERIC_SLICE_BASE='3',RENDERDOC_METAL_FRAME_GENERIC_SLICE_COUNT='9')
            if base is not None: settings.update(RENDERDOC_METAL_FRAME_VIEW_MIP=str(base),RENDERDOC_METAL_FRAME_GENERIC_VIEW_COUNT=str(count))
            source=work/(tag+'.metal'); air=source.with_suffix('.air'); library=source.with_suffix('.metallib')
            run(tag+'-source',[work/'capture'],dict(settings,RENDERDOC_METAL_FRAME_SHADER_EXPORT=str(source)))
            run(tag+'-compile',['xcrun','-sdk','macosx','metal','-c',source,'-o',air])
            run(tag+'-link',['xcrun','-sdk','macosx','metallib',air,'-o',library])
            if args.source_compiled:
                settings['RENDERDOC_METAL_FRAME_EXPECT_UNKNOWN_ACCESS']='1'
                settings['RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT']='1'
            else: settings['RENDERDOC_METAL_FRAME_LIBRARY']=str(library)
            native=run(tag+'-native',[work/'capture'],settings)
            captured=run(tag+'-capture',[work/'capture'],dict(settings,DYLD_INSERT_LIBRARIES=str(build/'lib/librenderdoc.dylib'),RENDERDOC_METAL_CAPTURE_PATH=str(work/tag)))
            ids=re.search(r'FRAME_IDS=(\d+),(\d+)',captured)
            assert ids and 'Native frame family PASS' in native
            for i,suffix in enumerate(('','_2')):
                cap=work/(tag+'_capture'+suffix+'.rdc')
                run(tag+'-API'+str(i),[work/'replay',cap,ids[i+1]], settings)
                run(tag+'-CLI'+str(i),[cli,'replay','--loops','3',cap])
            manifest['variants'].append(dict(tag=tag,width=width,height=height,format=format,mips=mips,offset=offset,view_base=base,view_count=count,dispatches=dispatches,array_layers=19 if tag=='array-subview' else 1,slice_base=3 if tag=='array-subview' else 0,slice_count=9 if tag=='array-subview' else 1,source_sha256=sha(source),native_library='newLibraryWithSource' if args.source_compiled else 'newLibraryWithData',actual_metallib_sha256=None if args.source_compiled else sha(library))); save()
        xml=work/'source.zip.xml'
        run('export',[cli,'convert','-f',work/'half-multiview_capture.rdc','-o',xml,'-c','zip.xml'])
        original=ET.parse(xml)
        field=lambda node,name: node.find('./*[@name="'+name+'"]')
        cases={'zero-width':('width',0),'invalid-format':('pixelFormat',0),'zero-mips':('mipmapLevelCount',0),
               'excess-mips':('mipmapLevelCount',64),'2D-depth':('depth',2),'2D-array':('arrayLength',2),
               'unknown-usage':('usage',1<<63),'storage-mismatch':('storageMode',0)}
        for tag in [*cases,'unaligned-offset','heap-range','missing-source','missing-producer','view-zero-count','view-overflow','view-mip-range','view-slice-range']:
            tree=copy.deepcopy(original);chunks=tree.find('./chunks')
            birth=next(c for c in chunks if c.get('name')=='MTLHeap::newTexture(offset)')
            descriptor=field(birth,'descriptor')
            view=next(c for c in chunks if c.get('name')=='MTLTexture::newTextureViewWithPixelFormat')
            if tag in cases: key,value=cases[tag];field(descriptor,key).text=str(value)
            elif tag=='unaligned-offset':field(birth,'offset').text='1'
            elif tag=='heap-range':field(birth,'offset').text=str(2**64-1)
            elif tag=='missing-source':field(view,'Source').text='99999999'
            elif tag=='missing-producer':
                # Remove the first actual native writer, retain the consumer.
                chunks.remove(next(c for c in chunks if c.get('name')=='MTLComputeCommandEncoder::dispatchThreadgroups'))
            elif tag=='view-zero-count':field(field(view,'levels'),'length').text='0'
            elif tag=='view-overflow':field(field(view,'levels'),'length').text=str(2**64-1)
            elif tag=='view-mip-range':field(field(view,'levels'),'location').text='5'
            elif tag=='view-slice-range':field(field(view,'slices'),'location').text='1'
            for c in chunks:c.set('length',str(int(c.get('length','0'))+128))
            target=work/(tag+'.zip.xml');tree.write(target,encoding='utf-8',xml_declaration=True)
            os.link(xml.with_suffix(''),target.with_suffix(''))
            cap=work/(tag+'.rdc');run(tag+'-import',[cli,'convert','-f',target,'-o',cap,'-c','rdc'])
            output=run(tag+'-API',[work/'opener',cap],dict(RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1'),expected=4)
            assert 'Metal replay wait begin' not in output
            run(tag+'-CLI',[cli,'replay','--loops','1',cap],expected=1)
        assert sha(build/'lib/librenderdoc.dylib')==manifest['backend_sha256']
        manifest.update(status='PASS',bundle_sha256=sha(build/'bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib'))
        assert manifest['bundle_sha256']==manifest['backend_sha256'];save()
    except Exception as error:
        manifest.update(status='FAIL',error=str(error));save();raise

if __name__=='__main__':
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX);main()
