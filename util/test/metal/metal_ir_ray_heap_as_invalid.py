#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded heap-AS converted-IR corruption and positive controls."""
import argparse
import copy
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from zipfile import ZipFile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', type=Path); parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--oracle', type=Path)
    parser.add_argument('--mode', default='descriptor-heaps-heap-as', choices=['descriptor-heaps-heap-as','ue-global-descriptor-heaps-heap-as-local-root-six-samplers','descriptor-heaps-heap-as-only'])
    args = parser.parse_args(); heap_only='heap-as-only' in args.mode; out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    cli = args.command.resolve(); capture = args.capture.resolve()
    manifest = dict(status='RUNNING', source=str(capture), source_sha256=hashlib.sha256(capture.read_bytes()).hexdigest(), checks=[])
    env = os.environ.copy()
    for key in tuple(env):
        if key.startswith('RENDERDOC_METAL_') or key == 'DYLD_INSERT_LIBRARIES': env.pop(key)
    env['MTL_DEBUG_LAYER'] = '1'

    def run(tag, command, success=True, expected=None):
        log_path = out/(tag+'.log'); current = env.copy(); current['RENDERDOC_DEBUG_LOG_FILE'] = str(log_path)
        with log_path.open('w') as log:
            fcntl.flock(log.fileno(), fcntl.LOCK_SH)
            process = subprocess.Popen([str(x) for x in command], stdout=log, stderr=subprocess.STDOUT,
                env=current, start_new_session=True)
            try: code = process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL); process.wait(); code = 124
        text = log_path.read_text(errors='replace')
        ok = code == 0 if success else 0 < code < 124 and any(reason in text for reason in expected)
        ok &= not any(marker in text for marker in ('Assertion failed','OVERRUNNING CHUNK','Unexpected Metal resource type','m_ResourceMap.empty'))
        if not ok: raise RuntimeError(f'{tag}: exit {code}: {text[-1800:]}')
        return code

    try:
        original = out/'original.zip.xml'
        run('export', [cli, 'convert', '-f', capture, '-o', original, '-c', 'zip.xml'])
        tree = ET.parse(original); parent = tree.find('./chunks'); chunks = list(parent)
        field = lambda c, name: c.find('./*[@name="'+name+'"]')
        val = lambda c, name: int(field(c,name).text)
        find = lambda name: next(c for c in chunks if c.get('name') == name)
        declaration = find('MTLComputePipelineState::DeclareRayIRDispatch')
        packet = val(declaration,'buffer')
        identity = {val(c,'resource'):val(c,'value') for c in chunks if c.get('name') == 'MTLResource::CaptureGPUIdentity' and val(c,'kind') == 0}
        creations = {val(c,'Buffer'):c for c in chunks if c.get('name') == 'MTLDevice::newBufferWithBytes'}
        with ZipFile(str(original)[:-4]) as archive: blobs = {name:archive.read(name) for name in archive.namelist()}
        raw = lambda c, member: blobs[f'{int(field(c,member).text):06}']
        pbytes = raw(creations[packet],'initialData')
        grs = next(id for id,address in identity.items() if address == struct.unpack_from('<Q',pbytes,104)[0])
        sbt = next(id for id,address in identity.items() if address == struct.unpack_from('<Q',pbytes,0)[0])
        header = val(next(c for c in chunks if c.get('name')=='MTLResource::setLabel' and field(c,'label').text=='IR AS header'),'resource')
        header_initial = next(c for c in chunks if c.get('name') == 'Internal::Initial Contents' and val(c,'id') == header)
        contributions = next(id for id,address in identity.items() if address == struct.unpack_from('<Q',raw(header_initial,'Contents'),8)[0])
        output = next(id for id,address in identity.items() if address == struct.unpack_from('<Q',raw(creations[grs],'initialData'),0 if heap_only else 8)[0])
        resource_heap=next(id for id,va in identity.items() if va==struct.unpack_from('<Q',pbytes,112)[0])
        sampler_heap=next(id for id,va in identity.items() if va==struct.unpack_from('<Q',pbytes,120)[0])
        hbytes=raw(creations[resource_heap],'initialData')
        srv=next(id for id,va in identity.items() if va+4==struct.unpack_from('<Q',hbytes,0)[0])
        pipeline=val(declaration,'pipeline')
        heap_header=next(id for id,va in identity.items() if va==struct.unpack_from('<Q',hbytes,48)[0])
        heap_initial=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and val(c,'id')==heap_header)
        heap_bytes=raw(heap_initial,'Contents')
        heap_contributions=next(id for id,va in identity.items() if va==struct.unpack_from('<Q',heap_bytes,8)[0])
        heap_as=struct.unpack_from('<Q',heap_bytes,0)[0]
        as_identities={val(c,'resource'):val(c,'value') for c in chunks if c.get('name')=='MTLAccelerationStructure::CaptureGPUIdentity'}
        recipes={val(c,'id'):c for c in chunks if c.get('name')=='Internal::Initial Contents' and field(c,'kind') is not None and val(c,'kind')==5}
        heap_as_resource=next(id for id,value in as_identities.items() if value==heap_as)
        blas_as=next(value for id,value in as_identities.items() if id not in recipes)
        cases=[]
        reasons=['Invalid Metal IR ray ABI closure','Invalid or unsupported explicit Metal descriptor frame data',
            'Invalid or unsupported explicit Metal descriptor initial data','Invalid Metal IR heap entry declaration',
            'Failed to process Metal chunk MTLComputePipelineState::DeclareRayIRHeapEntry']
        def add(tag,mutate,success=False,oracle_mode=None):cases.append((tag,mutate,reasons,success,oracle_mode or args.mode))
        def heap_chunk(t):return next(c for c in t.find('./chunks') if c.get('name')=='MTLComputePipelineState::DeclareRayIRHeapEntry' and val(c,'heap')==0 and val(c,'index')==2)
        def member(name,value):
            def mutate(t,b):field(heap_chunk(t),name).text=str(value)
            return mutate
        for tag,name,value in [('as-in-sampler-heap','heap',1),('as-is-texture','kind',1),('as-is-sampler','kind',2),
            ('as-is-raw-buffer','kind',0),('unknown-kind','kind',4),('header-view-short','bytes',63),
            ('header-view-large','bytes',65),('header-view-zero','bytes',0),('moved-AS-slot','index',4)]:add(tag,member(name,value))
        def duplicate(t,b):p=t.find('./chunks');c=heap_chunk(t);p.insert(list(p).index(c)+1,copy.deepcopy(c))
        add('duplicate-AS-slot',duplicate)
        def frame(t,b):p=t.find('./chunks');c=heap_chunk(t);p.remove(c);p.append(c)
        add('frame-AS-slot',frame)
        def missing(t,b):p=t.find('./chunks');p.remove(heap_chunk(t))
        add('missing-AS-slot',missing)
        for id,at in [(resource_heap,48),(heap_header,0),(heap_header,8)]:
            def remove(t,b,id=id,at=at):p=t.find('./chunks');p.remove(next(c for c in p if c.get('name')=='MTLBuffer::DeclareDescriptorTable' and val(c,'buffer')==id and val(c,'offset')==at))
            add(f'missing-AS-layout-{id}-{at}',remove)
        def patch(id,at,fmt,value,snapshots_only=False):
            def mutate(t,b):
                for c in t.find('./chunks'):
                    name=c.get('name');part=None
                    if not snapshots_only and name=='MTLDevice::newBufferWithBytes' and val(c,'Buffer')==id:part='initialData'
                    if not snapshots_only and name=='Internal::Initial Contents' and val(c,'id')==id:part='Contents'
                    if name=='Internal_MTLBufferModifyCPUContents' and val(c,'Buffer')==id:part='data'
                    if part:
                        key=f'{int(field(c,part).text):06}';data=bytearray(b[key]);struct.pack_into(fmt,data,at,value);b[key]=bytes(data)
            return mutate
        for tag,id,at,value in [('null-AS-header',resource_heap,48,0),('unknown-AS-header',resource_heap,48,identity[heap_header]+1048576),
            ('unaligned-AS-header',resource_heap,48,identity[heap_header]+1),('AS-header-is-raw-buffer',resource_heap,48,identity[srv]),
            ('AS-header-nonzero-texture-word',resource_heap,56,1),('AS-header-nonzero-metadata',resource_heap,64,64),
            ('header-null-AS-ID',heap_header,0,0),('header-unknown-AS-ID',heap_header,0,999999),('header-BLAS-instead-of-TLAS',heap_header,0,blas_as),
            ('null-contributions',heap_header,8,0),('unknown-contributions',heap_header,8,identity[heap_contributions]+1048576),
            ('unaligned-contributions',heap_header,8,identity[heap_contributions]+1),('contributions-is-live-SRV',heap_header,8,identity[srv]+4),
            ('UAV-overwrites-heap-AS-header',grs,0 if heap_only else 8,identity[heap_header]),('UAV-overwrites-heap-contributions',grs,0 if heap_only else 8,identity[heap_contributions])]:add(tag,patch(id,at,'<Q',value))
        for at in range(16,64,8):add(f'unsupported-AS-header-word-{at}',patch(heap_header,at,'<Q',1))
        add('instance-contribution-outside-hit-records',patch(heap_contributions,0,'<I',1))
        def header_snapshot(t,b,changed=False):
            # These newBufferWithLength headers already have a background base
            # snapshot, so an unchanged commit emits no CPU diff. Construct an
            # actual frame CPU chunk rather than counting a no-op mutation.
            c=copy.deepcopy(next(c for c in t.find('./chunks') if c.get('name')=='Internal_MTLBufferModifyCPUContents'))
            field(c,'Buffer').text=str(heap_header);field(c,'start').text='0';field(c,'size').text='64'
            key=f'{max(int(k) for k in b if k.isdigit())+1:06}'
            data=bytearray(heap_bytes)
            if changed:struct.pack_into('<Q',data,0,999999)
            old_bytes=int(field(c,'data').get('byteLength'))
            b[key]=bytes(data);field(c,'data').text=str(int(key));field(c,'data').set('byteLength','64')
            c.set('length',str(int(c.get('length'))+max(0,64-old_bytes)+64))
            parent=t.find('./chunks');commit=next(v for v in parent if v.get('name')=='MTLCommandBuffer::commit')
            parent.insert(list(parent).index(commit),c)
        add('mutated-AS-header-commit',lambda t,b:header_snapshot(t,b,True))
        add('legal-coherent-AS-header-commit',header_snapshot,True)
        if heap_only:
            def no_validated_AS(t,b):
                missing(t,b)
                for at in (48,56,64):patch(resource_heap,at,'<Q',0)(t,b)
            add('no-direct-root-and-no-proven-heap-AS',no_validated_AS)
        def as_identity_remap(t,b):
            for c in t.find('./chunks'):
                if c.get('name')=='MTLAccelerationStructure::CaptureGPUIdentity' and val(c,'resource')==heap_as_resource:field(c,'value').text=str(heap_as+256)
            patch(heap_header,0,'<Q',heap_as+256)(t,b)
        add('legal-AS-ID-remap',as_identity_remap,True)
        def buffer_identity_remap(t,b,id,target,at):
            for c in t.find('./chunks'):
                if c.get('name')=='MTLResource::CaptureGPUIdentity' and val(c,'kind')==0 and val(c,'resource')==id:field(c,'value').text=str(identity[id]+1048576)
            patch(target,at,'<Q',identity[id]+1048576)(t,b)
        add('legal-header-VA-remap',lambda t,b:buffer_identity_remap(t,b,heap_header,resource_heap,48),True)
        add('legal-contributions-VA-remap',lambda t,b:buffer_identity_remap(t,b,heap_contributions,heap_header,8),True)
        for tag,mutate,reasons,success,oracle_mode in cases:
            variant=copy.deepcopy(tree); data=blobs.copy(); mutate(variant,data)
            xml=out/(tag+'.zip.xml'); variant.write(xml,encoding='unicode',xml_declaration=True)
            with ZipFile(str(xml)[:-4],'w') as archive:
                for name,raw_bytes in data.items():archive.writestr(name,raw_bytes)
            cap=out/(tag+'.rdc'); run(tag+'-convert',[cli,'convert','-f',xml,'-o',cap,'-c','rdc'])
            code=run(tag+'-replay',[cli,'replay','--loops','1',cap],success,reasons)
            if success and args.oracle: run(tag+'-oracle', [args.oracle.resolve(), cap, oracle_mode])
            manifest['checks'].append(dict(name=tag,expected='PASS' if success else 'CLEAN REJECTION',exit=code,passed=True))
            (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
            print('PASS '+tag,flush=True)
        manifest.update(status='PASS', rejected=sum(not c[3] for c in cases), positive_controls=sum(c[3] for c in cases))
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError, KeyError, StopIteration) as error:
        manifest.update(status='FAIL',error=str(error)); print(str(error),flush=True); return 1
    finally:(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return 0

if __name__ == '__main__':
    # All finite IR gates share one process-lifetime lock. Independent gate
    # invocations must not overlap native, capture, replay or compilation work.
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as gpu_lock:
        fcntl.flock(gpu_lock.fileno(), fcntl.LOCK_EX)
        raise SystemExit(main())
