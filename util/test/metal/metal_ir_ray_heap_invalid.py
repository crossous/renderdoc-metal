#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded descriptor-heap converted-IR corruption and positive controls."""
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
    parser.add_argument('--mode', default='descriptor-heaps', choices=['descriptor-heaps','ue-global-descriptor-heaps-local-root-six-samplers'])
    args = parser.parse_args(); out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
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
        header = next(id for id,address in identity.items() if address == struct.unpack_from('<Q',raw(creations[grs],'initialData'),0)[0])
        header_initial = next(c for c in chunks if c.get('name') == 'Internal::Initial Contents' and val(c,'id') == header)
        contributions = next(id for id,address in identity.items() if address == struct.unpack_from('<Q',raw(header_initial,'Contents'),8)[0])
        output = next(id for id,address in identity.items() if address == struct.unpack_from('<Q',raw(creations[grs],'initialData'),8)[0])
        resource_heap=next(id for id,va in identity.items() if va==struct.unpack_from('<Q',pbytes,112)[0])
        sampler_heap=next(id for id,va in identity.items() if va==struct.unpack_from('<Q',pbytes,120)[0])
        hbytes=raw(creations[resource_heap],'initialData')
        srv=next(id for id,va in identity.items() if va+4==struct.unpack_from('<Q',hbytes,0)[0])
        pipeline=val(declaration,'pipeline')
        cases=[]
        reasons=['Invalid Metal IR ray ABI closure','Invalid or unsupported explicit Metal descriptor frame data',
            'Invalid or unsupported explicit Metal descriptor initial data','Invalid Metal IR heap entry declaration',
            'Failed to process Metal chunk MTLComputePipelineState::DeclareRayIRHeapEntry']
        def add(tag,mutate,success=False,oracle_mode=None):cases.append((tag,mutate,reasons,success,oracle_mode or args.mode))
        def heap_chunk(t,heap=0,index=0):
            return next(c for c in t.find('./chunks') if c.get('name')=='MTLComputePipelineState::DeclareRayIRHeapEntry' and val(c,'heap')==heap and val(c,'index')==index)
        def member(name,value,heap=0,index=0):
            def mutate(t,b):field(heap_chunk(t,heap,index),name).text=str(value)
            return mutate
        for tag,name,value in [('unknown-pipeline','pipeline',999999),('zero-pipeline','pipeline',0),
            ('buffer-is-pipeline','pipeline',resource_heap),('unknown-heap','heap',2),('buffer-in-sampler-heap','heap',1),
            ('unknown-kind','kind',3),('index-overflow','index',2730),('zero-view','bytes',0),('oversize-view','bytes',65537)]:add('entry-'+tag,member(name,value))
        for tag,name,value,heap,index in [('texture-with-bytes','bytes',1,0,1),('sampler-with-bytes','bytes',1,1,1),
            ('sampler-in-resource-heap','heap',0,1,1),('texture-in-sampler-heap','heap',1,0,1),
            ('view-past-backing','bytes',32,0,0),('wrong-metadata-extent','bytes',8,0,0),('moved-live-entry','index',2,0,3)]:add('entry-'+tag,member(name,value,heap,index))
        def duplicate(t,b):p=t.find('./chunks');c=heap_chunk(t);p.insert(list(p).index(c)+1,copy.deepcopy(c))
        add('entry-duplicate',duplicate)
        def frame(t,b):p=t.find('./chunks');c=heap_chunk(t);p.remove(c);p.append(c)
        add('entry-frame-declaration',frame)
        for heap,index in [(0,0),(0,1),(0,3),(1,1)]:
            def remove(t,b,heap=heap,index=index):p=t.find('./chunks');p.remove(heap_chunk(t,heap,index))
            add(f'missing-heap-entry-{heap}-{index}',remove)
        for id,at in [(resource_heap,0),(resource_heap,72),(sampler_heap,24)]:
            def remove_layout(t,b,id=id,at=at):
                p=t.find('./chunks');p.remove(next(c for c in p if c.get('name')=='MTLBuffer::DeclareDescriptorTable' and val(c,'buffer')==id and val(c,'offset')==at))
            add(f'missing-heap-layout-{id}-{at}',remove_layout)
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
        for tag,id,at,value in [('null-resource-heap',packet,112,0),('null-sampler-heap',packet,120,0),
            ('swapped-resource-heap',packet,112,identity[sampler_heap]),('swapped-sampler-heap',packet,120,identity[resource_heap]),
            ('heap-pointer-unknown',packet,112,identity[resource_heap]+1048576),('heap-pointer-unaligned',packet,112,identity[resource_heap]+1),
            ('null-SRV',resource_heap,0,0),('SRV-unaligned',resource_heap,0,identity[srv]+5),
            ('SRV-short-view',resource_heap,0,identity[srv]+30),('SRV-unknown',resource_heap,0,identity[srv]+1048576),
            ('SRV-unexpected-texture-ID',resource_heap,8,1),('SRV-zero-metadata',resource_heap,16,0),
            ('SRV-unsupported-typed-flag',resource_heap,16,2**63+4),('SRV-unsupported-element-offset',resource_heap,16,2**32+4),
            ('texture-null-ID',resource_heap,32,0),('texture-unknown-ID',resource_heap,32,999999),
            ('texture-nonzero-VA',resource_heap,24,identity[srv]),('texture-unsupported-metadata',resource_heap,40,1),
            ('sampler-null-ID',sampler_heap,24,0),('sampler-unknown-ID',sampler_heap,24,999999),
            ('sampler-nonzero-texture-word',sampler_heap,32,1),('sampler-unsupported-bias',sampler_heap,40,1),
            ('undeclared-resource-hole-live',resource_heap,48,identity[srv]+4),('undeclared-resource-hole-scalar',resource_heap,64,4),
            ('undeclared-sampler-hole-live',sampler_heap,0,1),('undeclared-sampler-hole-scalar',sampler_heap,16,1),
            ('UAV-overwrites-heap-SRV',grs,8,identity[srv]),('UAV-overwrites-resource-heap',grs,8,identity[resource_heap]),
            ('UAV-overwrites-sampler-heap',grs,8,identity[sampler_heap])]:add(tag,patch(id,at,'<Q',value))
        add('mutated-heap-commit-snapshot',patch(resource_heap,16,'<Q',8,True))
        def identity_remap(t,b,kind,id,at):
            original=struct.unpack_from('<Q',raw(creations[id],'initialData'),at)[0]
            for c in t.find('./chunks'):
                if c.get('name')=='MTLResource::CaptureGPUIdentity' and val(c,'kind')==kind and val(c,'value')==original:field(c,'value').text=str(original+256)
            for layout in chunks:
                if layout.get('name')=='MTLBuffer::DeclareDescriptorTable' and val(layout,'schema')==(2 if kind==2 else 1):
                    target=val(layout,'buffer')
                    for entry in range(val(layout,'count')):
                        field_at=val(layout,'offset')+entry*val(layout,'stride')+(8 if kind==1 else 0)
                        if struct.unpack_from('<Q',raw(creations[target],'initialData'),field_at)[0]==original:patch(target,field_at,'<Q',original+256)(t,b)
        add('legal-consistent-sampler-ID-remap',lambda t,b:identity_remap(t,b,2,sampler_heap,24),True)
        add('legal-consistent-texture-ID-remap',lambda t,b:identity_remap(t,b,1,resource_heap,32),True)
        def buffer_remap(t,b):
            for c in t.find('./chunks'):
                if c.get('name')=='MTLResource::CaptureGPUIdentity' and val(c,'kind')==0 and val(c,'resource')==srv:field(c,'value').text=str(identity[srv]+1048576)
            for at in (0,72):patch(resource_heap,at,'<Q',struct.unpack_from('<Q',hbytes,at)[0]+1048576)(t,b)
        add('legal-consistent-buffer-VA-remap',buffer_remap,True)
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
