#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Finite native/capture/replay gate for frame-born R16Float mips, packed 2D or R8 arrays."""
import argparse
import copy
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir',type=Path,required=True)
    parser.add_argument('--build-dir',type=Path)
    parser.add_argument('--family',choices=('mips','mipviews','largeheaps','packed2d','r8array','r11array','halfarray','uintvolume','r11volume','r11large2d','packed2drt','r16uint2d'),default='mips')
    args=parser.parse_args();repo=Path(__file__).resolve().parents[3]
    build=(args.build_dir or repo/'build-macos-debug').resolve();work=args.work_dir.resolve();work.mkdir(parents=True,exist_ok=True)
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    manifest=dict(status='RUNNING',backend_sha256=sha(build/'lib/librenderdoc.dylib'),checks=[],native_capture_pairs=0,
        malformed_rejections=0,production_RT_enabled=False,family=args.family)
    env=os.environ.copy()
    for key in tuple(env):
        if key.startswith('RENDERDOC_METAL_') or key=='DYLD_INSERT_LIBRARIES':env.pop(key)
    env['MTL_DEBUG_LAYER']='1'
    def save():(work/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    def run(tag,command,extra=None,expected=0,seconds=60):
        current=dict(env,**(extra or {}));current['RENDERDOC_DEBUG_LOG_FILE']=str(work/(tag+'-renderdoc.log'))
        with (work/(tag+'-renderdoc.log')).open('a') as retained,(work/(tag+'.log')).open('w') as log:
            fcntl.flock(retained,fcntl.LOCK_SH)
            process=subprocess.Popen(list(map(str,command)),env=current,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            try:code=process.wait(timeout=seconds)
            except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait();code=124
        output=(work/(tag+'.log')).read_text(errors='replace')
        backend=(work/(tag+'-renderdoc.log')).read_text(errors='replace')
        markers=('Assertion failed','failed assertion','OVERRUNNING CHUNK','Unexpected Metal resource type','m_ResourceMap.empty','File and decompress stream readers do not support seeking')
        passed=code==expected and not any(x in output+backend for x in markers)
        manifest['checks'].append(dict(tag=tag,exit=code,passed=passed));save()
        if not passed:raise RuntimeError(tag+': '+output[-2000:]+backend[-1500:])
        print(tag,code,flush=True);return output
    cli=build/'bin/renderdoccmd'
    try:
        for name in ('UnrealEditor','qrenderdoc'):
            if subprocess.run(['pgrep','-x',name],stdout=subprocess.DEVNULL).returncode==0:raise RuntimeError(name+' is running')
        flags=['clang++','-std=c++17','-fobjc-arc','-mmacosx-version-min=13.0','-I'+str(repo),'-framework','Foundation','-framework','Metal']
        run('capture-build',flags+['-framework','QuartzCore',repo/'util/test/metal/metal_descriptor_frame_family_capture.mm','-o',work/'capture'])
        replayflags=flags+['-DRENDERDOC_PLATFORM_APPLE','-L'+str(build/'lib'),'-lrenderdoc','-Wl,-rpath,'+str(build/'lib')]
        run('replay-build',replayflags+[repo/'util/test/metal/metal_descriptor_frame_family_replay.mm','-o',work/'replay'])
        run('opener-build',replayflags+[repo/'util/ue/ue_capture_open_probe.cpp','-o',work/'opener'])
        variants=[(f'mip{mips}',dict(RENDERDOC_METAL_FRAME_R16_MIPS=str(mips),RENDERDOC_METAL_FRAME_FAMILY_2D='1',RENDERDOC_METAL_FRAME_FAMILY_EXTENDED_2D='1',**({'RENDERDOC_METAL_FRAME_R16_MAXSIZE':'1'} if mips==10 else {}))) for mips in (8,9,10)]
        if args.family=='mipviews':
            variants=[(f'view{mip}',dict(RENDERDOC_METAL_FRAME_R16_MIPS='8' if mip<8 else '10',RENDERDOC_METAL_FRAME_VIEW_MIP=str(mip),RENDERDOC_METAL_FRAME_FAMILY_2D='1',RENDERDOC_METAL_FRAME_FAMILY_EXTENDED_2D='1',**({'RENDERDOC_METAL_FRAME_R16_MAXSIZE':'1'} if mip==9 else {}))) for mip in (0,1,3,7,9)]
        if args.family=='largeheaps':
            variants=[(f'heap{mib}',dict(RENDERDOC_METAL_FRAME_R16_MIPS='8',RENDERDOC_METAL_FRAME_FAMILY_2D='1',RENDERDOC_METAL_FRAME_FAMILY_EXTENDED_2D='1',RENDERDOC_METAL_FRAME_HEAP_BYTES=str(mib*1024*1024))) for mib in (129,192)]
        if args.family in ('packed2d','packed2drt'):
            variants=[(tag,dict(RENDERDOC_METAL_FRAME_FAMILY_PACKED10='1',RENDERDOC_METAL_FRAME_FAMILY_2D='1',RENDERDOC_METAL_FRAME_PACKED_2D='1',**extra)) for tag,extra in (
                ('packedUE',{}),('packedMax',{'RENDERDOC_METAL_FRAME_PACKED_MAXSIZE':'1'}),('packedSmall',{'RENDERDOC_METAL_FRAME_PACKED_SMALL':'1'}))]
        if args.family=='r8array':
            variants=[(tag,dict(RENDERDOC_METAL_FRAME_FAMILY_UNORM8='1',RENDERDOC_METAL_FRAME_FAMILY_ARRAY='1',RENDERDOC_METAL_FRAME_R8_ARRAY='1',**extra)) for tag,extra in (
                ('arrayUE',{}),('arrayMax',{'RENDERDOC_METAL_FRAME_R8_ARRAY_MAXSIZE':'1'}),('arraySmall',{'RENDERDOC_METAL_FRAME_R8_ARRAY_SMALL':'1'}))]
        if args.family=='r11array':
            variants=[(tag,dict(RENDERDOC_METAL_FRAME_FAMILY_PACKED11='1',RENDERDOC_METAL_FRAME_FAMILY_ARRAY='1',RENDERDOC_METAL_FRAME_R11_ARRAY='1',**extra)) for tag,extra in (
                ('arrayUE',{}),('arrayMax',{'RENDERDOC_METAL_FRAME_R11_ARRAY_MAXSIZE':'1'}),('arrayLayers',{'RENDERDOC_METAL_FRAME_R11_ARRAY_LAYERS':'1'}),('arraySmall',{'RENDERDOC_METAL_FRAME_R11_ARRAY_SMALL':'1'}))]
        if args.family=='halfarray':
            variants=[(tag,dict(RENDERDOC_METAL_FRAME_FAMILY_ARRAY='1',RENDERDOC_METAL_FRAME_HALF_ARRAY='1',**extra)) for tag,extra in (
                ('arrayUE',{}),('arrayMax',{'RENDERDOC_METAL_FRAME_HALF_ARRAY_MAXSIZE':'1'}),('arrayLayers',{'RENDERDOC_METAL_FRAME_HALF_ARRAY_LAYERS':'1'}),('arraySmall',{'RENDERDOC_METAL_FRAME_HALF_ARRAY_SMALL':'1'}),
                ('arrayLegacy',{'RENDERDOC_METAL_FRAME_HALF_ARRAY_SMALL':'1','RENDERDOC_METAL_FRAME_HALF_ARRAY_LEGACY_RTV':'1','RENDERDOC_METAL_FRAME_HALF_ARRAY_LEGACY64':'1'}))]
        if args.family=='uintvolume':
            variants=[(tag,dict(RENDERDOC_METAL_FRAME_FAMILY_UINT='1',RENDERDOC_METAL_FRAME_FAMILY_WORK='1',RENDERDOC_METAL_FRAME_UINT_VOLUME=kind)) for tag,kind in [('volumeUE','UE'),('volumeMax','max'),('volumeSmall','small')]]
        if args.family=='r11volume':
            variants=[(tag,dict(RENDERDOC_METAL_FRAME_FAMILY_PACKED11='1',RENDERDOC_METAL_FRAME_FAMILY_WORK='1',RENDERDOC_METAL_FRAME_R11_VOLUME=kind)) for tag,kind in [('packedVolumeUE','UE'),('packedVolumeMax','max'),('packedVolumeSmall','small')]]
        if args.family=='r11large2d':
            variants=[(tag,dict(RENDERDOC_METAL_FRAME_FAMILY_PACKED11='1',RENDERDOC_METAL_FRAME_FAMILY_2D='1',RENDERDOC_METAL_FRAME_FAMILY_WORK='1',RENDERDOC_METAL_FRAME_R11_LARGE_2D=kind)) for tag,kind in [('packed2DMax','max'),('packed2DRect','rect'),('packed2DSmall','small')]]
        if args.family=='packed2drt':
            for _,settings in variants:settings['RENDERDOC_METAL_FRAME_PACKED_RENDER']='1'
        if args.family=='r16uint2d':
            variants=[(tag,dict(RENDERDOC_METAL_FRAME_FAMILY_UINT='1',RENDERDOC_METAL_FRAME_FAMILY_2D='1',RENDERDOC_METAL_FRAME_FAMILY_WORK='1',RENDERDOC_METAL_FRAME_R16_UINT_2D=kind)) for tag,kind in [('shortUE','UE'),('shortMax','max'),('shortSmall','small')]]
        for tag,settings in variants:
            # Usage comes from proven AIR accesses, not the typed residency closure.
            # Use the exact generated MSL for both native and captured metallib runs.
            source=work/f'{tag}.metal';air=work/f'{tag}.air';library=work/f'{tag}.metallib'
            run(f'{tag}-source',[work/'capture'],dict(settings,RENDERDOC_METAL_FRAME_SHADER_EXPORT=str(source)))
            run(f'{tag}-compile',['xcrun','-sdk','macosx','metal','-c',source,'-o',air])
            run(f'{tag}-link',['xcrun','-sdk','macosx','metallib',air,'-o',library])
            settings['RENDERDOC_METAL_FRAME_LIBRARY']=str(library)
            native=run(f'{tag}-native',[work/'capture'],settings)
            captured=run(f'{tag}-capture',[work/'capture'],dict(settings,DYLD_INSERT_LIBRARIES=str(build/'lib/librenderdoc.dylib'),RENDERDOC_METAL_CAPTURE_PATH=str(work/tag)))
            IDs=re.search(r'FRAME_IDS=(\d+),(\d+)',captured)
            assert IDs and 'Native frame family PASS' in native
            for i,suffix in enumerate(('','_2')):
                cap=work/f'{tag}_capture{suffix}.rdc'
                run(f'{tag}-API{i}',[work/'replay',cap,IDs[i+1]])
                run(f'{tag}-CLI{i}',[cli,'replay','--loops','3',cap])
                manifest['native_capture_pairs']+=1
        original=work/'source.zip.xml'
        run('export',[cli,'convert','-f',work/f'{variants[0][0]}_capture.rdc','-o',original,'-c','zip.xml'])
        tree=ET.parse(original)
        field=lambda node,name:node.find('./*[@name="'+name+'"]')
        cases=dict(zero_mips=('mipmapLevelCount',0),too_many_mips=('mipmapLevelCount',10),width_limit=('width',513),height_limit=('height',513),
            zero_width=('width',0),zero_height=('height',0),samples=('sampleCount',2),depth=('depth',2),array=('arrayLength',2),
            wrong_format=('pixelFormat',115),usage_read=('usage',1),usage_render=('usage',7),storage_shared=('storageMode',0),untracked=('hazardTrackingMode',1))
        if args.family not in ('mips','mipviews','largeheaps'):cases.update(too_many_mips=('mipmapLevelCount',2),wrong_format=('pixelFormat',0))
        if args.family in ('r8array','r11array','halfarray'):cases.update(array=('arrayLength',9),zero_array=('arrayLength',0))
        if args.family=='uintvolume':
            cases.update(width_limit=('width',257),height_limit=('height',65),depth=('depth',65),zero_depth=('depth',0),
                array=('arrayLength',2),wrong_format=('pixelFormat',55),atomic_usage=('usage',19))
        if args.family=='r11volume':
            cases.update(width_limit=('width',65),height_limit=('height',65),depth=('depth',65),zero_depth=('depth',0),
                array=('arrayLength',2),wrong_format=('pixelFormat',252),atomic_usage=('usage',19))
        if args.family=='r11large2d':
            cases.update(width_limit=('width',4097),height_limit=('height',4097),wrong_format=('pixelFormat',252),
                atomic_usage=('usage',19),unknown_usage=('usage',35))
        if args.family in ('packed2d','packed2drt'):
            # RW+RT7 is supported; RT+read5 lacks the declared writer contract.
            cases.update(usage_render=('usage',5))
        if args.family=='packed2drt':
            cases.update(atomic_usage=('usage',19),unknown_usage=('usage',35))
        if args.family=='r16uint2d':
            cases.update(width_limit=('width',4097),height_limit=('height',513),wrong_format=('pixelFormat',252),
                atomic_usage=('usage',19),unknown_usage=('usage',35))
        viewcases=['view-level-range','view-level-overflow','view-count-zero','view-count-two','view-parent','view-type','view-format','view-swizzle','duplicate-view','view-before-parent','view-self-parent'] if args.family=='mipviews' else []
        heapcases=dict(heap_zero=('size',0),heap_small=('size',4095),heap_limit=('size',576*1024*1024+1),heap_overflow=('size',2**64-1),
            heap_null=('Heap',0),heap_memoryless=('storageMode',3),heap_cache=('cacheMode',1),heap_hazard=('hazardMode',3),heap_type=('type',2)) if args.family=='largeheaps' else {}
        heapother=['heap_duplicate','heap_shared_automatic','heap_frame_birth','heap_total_budget'] if args.family=='largeheaps' else []
        for tag in [*cases,'legacy64','heap_range','unaligned_heap','duplicate_birth',*viewcases,*heapcases,*heapother]:
            variant=copy.deepcopy(tree);chunks=variant.find('./chunks');birth=next(c for c in chunks if c.get('name')=='MTLHeap::newTexture(offset)')
            if tag in cases:
                key,value=cases[tag];field(field(birth,'descriptor'),key).text=str(value)
            elif tag=='legacy64':field(next(c for c in chunks if c.get('name')=='MTLDevice::DeclareDescriptorCoverage'),'version').text='64'
            elif tag=='heap_range':field(birth,'offset').text=str(2**32)
            elif tag=='unaligned_heap':field(birth,'offset').text='1'
            elif tag=='duplicate_birth':chunks.insert(list(chunks).index(birth),copy.deepcopy(birth))
            elif tag in heapcases or tag in heapother:
                heap=next(c for c in chunks if c.get('name')=='MTLDevice::newHeapWithDescriptor')
                if tag in heapcases:
                    key,value=heapcases[tag];field(heap,key).text=str(value)
                elif tag=='heap_duplicate':chunks.insert(list(chunks).index(heap),copy.deepcopy(heap))
                elif tag=='heap_shared_automatic':field(heap,'storageMode').text='0';field(heap,'type').text='0'
                elif tag=='heap_frame_birth':chunks.remove(heap);chunks.insert(list(chunks).index(birth),heap)
                elif tag=='heap_total_budget':
                    field(heap,'size').text=str(576*1024*1024)
                    for i in range(5):
                        extra=copy.deepcopy(heap);field(extra,'Heap').text=str(9000000+i);chunks.insert(list(chunks).index(heap),extra)
            else:
                view=next(c for c in chunks if c.get('name')=='MTLTexture::newTextureViewWithPixelFormat')
                if tag.startswith('view-level'):field(field(view,'levels'),'location').text='8' if tag=='view-level-range' else str(2**64-1)
                elif tag.startswith('view-count'):field(field(view,'levels'),'length').text='0' if tag=='view-count-zero' else '2'
                elif tag=='view-parent':field(view,'Source').text='9999999'
                elif tag=='view-type':field(view,'type').text='3'
                elif tag=='view-format':field(view,'format').text='92'
                elif tag=='view-swizzle':field(field(view,'swizzle'),'red').text='0'
                elif tag=='duplicate-view':chunks.insert(list(chunks).index(view),copy.deepcopy(view))
                elif tag=='view-before-parent':chunks.remove(view);chunks.insert(list(chunks).index(birth),view)
                elif tag=='view-self-parent':field(view,'Source').text=field(view,'View').text
            for chunk in chunks:chunk.set('length',str(int(chunk.get('length','0'))+128))
            xml=work/(tag+'.zip.xml');variant.write(xml,encoding='utf-8',xml_declaration=True);os.link(original.with_suffix(''),xml.with_suffix(''))
            cap=work/(tag+'.rdc');run(tag+'-import',[cli,'convert','-f',xml,'-o',cap,'-c','rdc'])
            rejected=run(tag+'-API',[work/'opener',cap],dict(RENDERDOC_METAL_TRACE_REPLAY_WAITS='1',RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT='1'),expected=4)
            assert 'failed' in rejected.lower() and 'Metal replay wait begin' not in rejected
            if tag in heapcases or tag in heapother:
                assert ('Metal replay allocation budget:' if tag=='heap_total_budget' else 'Metal heap metadata rejection:') in rejected
            run(tag+'-CLI',[cli,'replay','--loops','1',cap],expected=1)
            manifest['malformed_rejections']+=2
        assert sha(build/'lib/librenderdoc.dylib')==manifest['backend_sha256']
        manifest.update(status='PASS',bundle_sha256=sha(build/'bin/qrenderdoc.app/Contents/lib/librenderdoc.dylib'))
        assert manifest['bundle_sha256']==manifest['backend_sha256'];save();return 0
    except Exception as error:manifest.update(status='FAIL',error=str(error));save();raise


if __name__=='__main__':
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX);sys.exit(main())
