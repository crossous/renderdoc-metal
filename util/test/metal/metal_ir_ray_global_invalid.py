#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded global-root/static-sampler converted-IR corruption and positive controls."""
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
    parser.add_argument('--mode', default='global-root', choices=['global-root','ue-global-local-root-six-samplers'])
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
        gbytes=raw(creations[grs],'initialData')
        cbv=next(id for id,va in identity.items() if va+256 == struct.unpack_from('<Q',gbytes,16)[0])
        srv=next(id for id,va in identity.items() if va+4 == struct.unpack_from('<Q',gbytes,40)[0])
        textures=next(id for id,va in identity.items() if va == struct.unpack_from('<Q',gbytes,48)[0])
        samplers=next(id for id,va in identity.items() if va == struct.unpack_from('<Q',gbytes,56)[0])
        pipeline=val(declaration,'pipeline')
        cases=[]
        reasons=['Invalid Metal IR ray ABI closure','Invalid or unsupported explicit Metal descriptor frame data',
            'Invalid or unsupported explicit Metal descriptor initial data','Invalid Metal IR global root declaration',
            'Invalid Metal IR ray dispatch declaration','Failed to process Metal chunk MTLComputePipelineState::DeclareRayIRGlobalRoot']
        def add(tag,mutate,success=False,oracle_mode=None):cases.append((tag,mutate,reasons,success,oracle_mode or args.mode))
        def root_chunk(t,offset=16):
            return next(c for c in t.find('./chunks') if c.get('name') == 'MTLComputePipelineState::DeclareRayIRGlobalRoot' and val(c,'offset')==offset)
        def member(name,value,offset=16):
            def mutate(t,b):field(root_chunk(t,offset),name).text=str(value)
            return mutate
        for tag,name,value in [('unknown-pipeline','pipeline',999999),('zero-pipeline','pipeline',0),
            ('buffer-is-pipeline','pipeline',sbt),('invalid-kind','kind',7),('zero-count','count',0),
            ('too-many-elements','count',65),('zero-bytes','bytes',0),('oversize-view','bytes',65537),
            ('unaligned-field','offset',17),('GRS-field-outside','offset',64)]:add('root-'+tag,member(name,value))
        for tag,name,value,offset in [('CBV-truncated-view','bytes',257,16),('SRV-truncated-view','bytes',32,40),
            ('constants-overlap-CBV','offset',16,24),('constants-size-mismatch','bytes',12,24),
            ('constants-zero-count','count',0,24),('constants-unaligned','offset',25,24),
            ('texture-entry-size-mismatch','bytes',16,48),('sampler-entry-size-mismatch','bytes',120,56),
            ('AS-header-size-mismatch','bytes',63,0),('AS-header-array','count',2,0),
            ('UAV-short-view','bytes',4,8),('UAV-view-not-uint32','bytes',7,8),('UAV-array','count',2,8),
            ('duplicate-AS-header-kind','kind',5,8),('duplicate-UAV-kind','kind',6,0)]:add('root-'+tag,member(name,value,offset))
        def duplicate(t,b):
            p=t.find('./chunks');c=root_chunk(t);p.insert(list(p).index(c)+1,copy.deepcopy(c))
        add('root-duplicate',duplicate)
        def frame(t,b):p=t.find('./chunks');c=root_chunk(t);p.remove(c);p.append(c)
        add('root-frame-declaration',frame)
        for offset in (0,8,16,24,40,48,56):
            def remove(t,b,offset=offset):p=t.find('./chunks');p.remove(root_chunk(t,offset))
            add(f'root-missing-parameter-{offset}',remove)
        for words in (0,1,2,7,9,257):
            def words_mutate(t,b,words=words):field(next(c for c in t.find('./chunks') if c.get('name')=='MTLComputePipelineState::DeclareRayIRDispatch'),'rootCount').text=str(words)
            add(f'root-word-count-{words}',words_mutate)
        for id,at in [(grs,x) for x in (16,40,48,56)] + [(textures,0),(samplers,0)]:
            def remove_layout(t,b,id=id,at=at):
                p=t.find('./chunks');p.remove(next(c for c in p if c.get('name')=='MTLBuffer::DeclareDescriptorTable' and val(c,'buffer')==id and val(c,'offset')==at))
            add(f'missing-global-layout-{id}-{at}',remove_layout)
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
        for tag,id,at,value in [('null-CBV',grs,16,0),('unaligned-CBV',grs,16,identity[cbv]+257),
            ('CBV-outside-view',grs,16,identity[cbv]+1048576),('null-SRV',grs,40,0),
            ('unaligned-SRV',grs,40,identity[srv]+1),('SRV-truncated-view',grs,40,identity[srv]+30),
            ('null-texture-table',grs,48,0),('null-static-sampler-table',grs,56,0),
            ('sampler-table-is-texture-table',grs,56,identity[textures]),('texture-table-is-sampler-table',grs,48,identity[samplers]),
            ('texture-null-ID',textures,8,0),('texture-unknown-ID',textures,8,999999),
            ('texture-nonzero-buffer-VA',textures,0,identity[srv]),('texture-unsupported-metadata',textures,16,1),
            ('sampler-null-ID',samplers,72,0),('sampler-unknown-ID',samplers,72,999999),
            ('sampler-nonzero-texture-word',samplers,80,1),('sampler-unsupported-LOD-bias',samplers,88,1),
            ('UAV-overwrites-global-CBV',grs,8,identity[cbv]),('UAV-overwrites-global-SRV',grs,8,identity[srv]),
            ('UAV-overwrites-static-table',grs,8,identity[samplers]),('UAV-overwrites-texture-table',grs,8,identity[textures]),
            ('UAV-overwrites-GRS',grs,8,identity[grs])]:add(tag,patch(id,at,'<Q',value))
        add('mutated-global-commit-snapshot',patch(grs,24,'<I',201,True))
        def sampler_identity_remap(t,b):
            original=struct.unpack_from('<Q',raw(creations[samplers],'initialData'),72)[0]
            for c in t.find('./chunks'):
                if c.get('name')=='MTLResource::CaptureGPUIdentity' and val(c,'kind')==2 and val(c,'value')==original:field(c,'value').text=str(original+256)
            for layout in chunks:
                if layout.get('name')=='MTLBuffer::DeclareDescriptorTable' and val(layout,'schema')==2:
                    id=val(layout,'buffer')
                    for entry in range(val(layout,'count')):
                        at=val(layout,'offset')+entry*val(layout,'stride')
                        if struct.unpack_from('<Q',raw(creations[id],'initialData'),at)[0]==original:
                            patch(id,at,'<Q',original+256)(t,b)
        add('legal-consistent-sampler-ID-remap',sampler_identity_remap,True)
        add('legal-larger-CBV-view',member('bytes',32),True)
        add('legal-larger-SRV-view',member('bytes',8,40),True)
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
