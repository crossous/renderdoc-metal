#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded local-root/static-sampler converted-IR corruption and positive controls."""
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
    parser.add_argument('--mode', default='local-root', choices=['local-root','ue-local-root-six-samplers'])
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
        cbv=next(id for id,va in identity.items() if va == struct.unpack_from('<Q',raw(creations[sbt],'initialData'),128)[0])
        srv=next(id for id,va in identity.items() if va+12 == struct.unpack_from('<Q',raw(creations[sbt],'initialData'),152)[0])
        textures=next(id for id,va in identity.items() if va == struct.unpack_from('<Q',raw(creations[sbt],'initialData'),160)[0])
        samplers=next(id for id,va in identity.items() if va == struct.unpack_from('<Q',raw(creations[sbt],'initialData'),112)[0])
        roles={val(c,'role'):val(c,'function') for c in chunks if c.get('name') == 'MTLFunctionHandle::DeclareRayIRShaderRole'}
        cases=[]
        reasons=['Invalid Metal IR ray ABI closure','Invalid or unsupported explicit Metal descriptor frame data',
            'Invalid or unsupported explicit Metal descriptor initial data','Invalid Metal IR local root declaration',
            'Failed to process Metal chunk MTLFunctionHandle::DeclareRayIRLocalRoot']
        def add(tag,mutate,success=False,oracle_mode=None):
            cases.append((tag,mutate,reasons,success,oracle_mode or args.mode))
        def root_chunk(t,offset=32,role=2):
            return next(c for c in t.find('./chunks') if c.get('name') == 'MTLFunctionHandle::DeclareRayIRLocalRoot' and
                val(c,'function')==roles[role] and val(c,'offset')==offset)
        def member(name,value,offset=32,role=2):
            def mutate(t,b):field(root_chunk(t,offset,role),name).text=str(value)
            return mutate
        for tag,name,value in [('unknown-function','function',999999),('zero-function','function',0),
            ('buffer-is-function','function',sbt),('invalid-kind','kind',5),('zero-count','count',0),
            ('too-many-elements','count',65),('zero-bytes','bytes',0),('oversize-view','bytes',65537),
            ('identifier-overlap','offset',24),('unaligned-field','offset',33),('record-field-outside','offset',4096)]:
            add('root-'+tag,member(name,value))
        add('CBV-truncated-declared-view',member('bytes',512))
        add('SRV-truncated-declared-view',member('bytes',32,56))
        add('root-constants-overlap-CBV',member('offset',32,40))
        add('root-constants-size-mismatch',member('bytes',12,40))
        add('root-texture-entry-size-mismatch',member('bytes',16,64))
        add('root-sampler-identifier-offset-wrong',member('offset',24,16))
        def duplicate_root(t,b):
            p=t.find('./chunks');c=root_chunk(t);p.insert(list(p).index(c)+1,copy.deepcopy(c))
        add('root-duplicate',duplicate_root)
        def frame_root(t,b):
            p=t.find('./chunks');c=root_chunk(t);p.remove(c);p.append(c)
        add('root-frame-declaration',frame_root)
        for offset in (16,32,40,56,64):
            def remove(t,b,offset=offset):p=t.find('./chunks');p.remove(root_chunk(t,offset))
            add(f'root-missing-hit-parameter-{offset}',remove)
        for id,at in [(sbt,x) for x in (112,128,152,160,208,224,248,256)] + [(textures,0),(samplers,0)]:
            def remove_layout(t,b,id=id,at=at):
                p=t.find('./chunks');p.remove(next(c for c in p if c.get('name') == 'MTLBuffer::DeclareDescriptorTable' and
                    val(c,'buffer')==id and val(c,'offset')==at))
            add(f'missing-local-layout-{id}-{at}',remove_layout)
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
        for tag,id,at,value in [('null-CBV',sbt,224,0),('unaligned-CBV',sbt,224,identity[cbv]+1),
            ('CBV-outside-view',sbt,224,identity[cbv]+1048576),('null-SRV',sbt,248,0),
            ('unaligned-SRV',sbt,248,identity[srv]+1),('SRV-truncated-view',sbt,248,identity[srv]+30),
            ('null-texture-table',sbt,256,0),('null-static-sampler-table',sbt,208,0),
            ('sampler-table-is-texture-table',sbt,208,identity[textures]),
            ('texture-table-is-sampler-table',sbt,256,identity[samplers]),
            ('texture-null-ID',textures,8,0),('texture-unknown-ID',textures,8,999999),
            ('texture-nonzero-buffer-VA',textures,0,identity[srv]),('texture-unsupported-metadata',textures,16,1),
            ('sampler-null-ID',samplers,0,0),('sampler-unknown-ID',samplers,0,999999),
            ('sampler-nonzero-texture-word',samplers,8,1),('sampler-unsupported-LOD-bias',samplers,16,1),
            ('nonzero-untyped-record-padding',sbt,264,1),('UAV-overwrites-local-CBV',grs,8,identity[cbv]),
            ('UAV-overwrites-local-SRV',grs,8,identity[srv]),('UAV-overwrites-static-table',grs,8,identity[samplers])]:
            add(tag,patch(id,at,'<Q',value))
        add('mutated-local-commit-snapshot',patch(sbt,232,'<I',21,True))
        def gridstride(t,b):
            patch(packet,32,'<Q',64)(t,b);patch(packet,56,'<Q',64)(t,b)
        add('local-table-size-not-multiple-of-stride',gridstride)
        def sampler_identity_remap(t,b):
            at=72 if 'six-samplers' in args.mode else 0
            original=struct.unpack_from('<Q',raw(creations[samplers],'initialData'),at)[0]
            for c in t.find('./chunks'):
                if c.get('name') == 'MTLResource::CaptureGPUIdentity' and val(c,'kind') == 2 and val(c,'value') == original:
                    field(c,'value').text=str(original+256)
            patch(samplers,at,'<Q',original+256)(t,b)
        add('legal-consistent-sampler-ID-remap',sampler_identity_remap,True)
        # Enlarging an explicitly bounded CBV view is legal when backing contents
        # cover it. It must not change fixed shader output or relocation semantics.
        add('legal-larger-CBV-view',member('bytes',32),True)
        def zero_stride(t,b):patch(packet,56,'<Q',0)(t,b)
        add('legal-local-hit-zero-stride',zero_stride,True)
        def nulled_hit(t,b):
            for at in range(192,288,8):
                # Keep the unused UE pad scalar, clear shader and all payload fields.
                if at != 216:patch(sbt,at,'<Q',0)(t,b)
        add('legal-local-null-hit',nulled_hit,True,args.mode+'-null-hit')
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
