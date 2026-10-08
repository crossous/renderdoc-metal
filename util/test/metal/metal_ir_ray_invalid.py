#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded corrupt converted-IR capture tests; signals/timeouts never count as rejection."""
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
        cases = []
        def add(tag, mutate, reasons=None, success=False, oracle_mode="default"):
            cases.append((tag,mutate,reasons or ['Invalid Metal IR ray ABI closure','Invalid or unsupported explicit Metal descriptor frame data',
                'Invalid or unsupported explicit Metal descriptor initial data'],success,oracle_mode))
        def scalar(tag, name, text):
            add(tag,lambda t,b: setattr(t.find('./chunks/chunk[@name="MTLComputePipelineState::DeclareRayIRDispatch"]/*[@name="'+name+'"]'),'text',str(text)),
                ['Invalid Metal IR ray dispatch declaration','Invalid Metal explicit ray dispatch declaration','Failed to process Metal chunk MTLComputePipelineState::DeclareRayIRDispatch'])
        scalar('zero-pipeline','pipeline',0); scalar('zero-buffer','buffer',0); scalar('unaligned-packet','offset',1)
        scalar('wrong-root-layout','rootCount',3)
        for depth in (1,3):
            def stack(t,b,depth=depth):
                c=next(c for c in t.find('./chunks') if c.get('name') == 'MTLDevice::newComputePipelineStateWithDescriptor')
                field(field(c,'descriptor'),'maxCallStackDepth').text=str(depth)
            add(f'unsupported-IR-stack-depth-{depth}',stack,['Invalid or unsupported Metal compute pipeline descriptor/options'])

        def remove_decl(t,b):
            p=t.find('./chunks'); p.remove(next(c for c in p if c.get('name') == 'MTLComputePipelineState::DeclareRayIRDispatch'))
        add('missing-dispatch-contract',remove_decl,['Metal ray resource layout requires an IR dispatch declaration','Metal IR shader roles require static typed dispatch coverage3'])
        def duplicate(t,b):
            p=t.find('./chunks'); d=next(c for c in p if c.get('name') == 'MTLComputePipelineState::DeclareRayIRDispatch'); p.insert(list(p).index(d)+1,copy.deepcopy(d))
        add('duplicate-contract',duplicate,['Duplicate Metal IR ray dispatch declaration'])
        def coverage(t,b):field(next(c for c in t.find('./chunks') if c.get('name') == 'MTLDevice::DeclareDescriptorCoverage'),'version').text='2'
        add('wrong-coverage',coverage,['Metal IR ray dispatch requires static typed coverage3','Metal IR shader roles require static typed dispatch coverage3'])
        layouts = [(packet,at) for at in (0,16,40,64,104,112,120,128,136,144)] + [(grs,0),(header,0),(header,8)] + [(sbt,at) for at in (16,48,80)]
        for id,at in layouts:
            def remove_field(t,b,id=id,at=at):
                p=t.find('./chunks'); p.remove(next(c for c in p if c.get('name') == 'MTLBuffer::DeclareDescriptorTable' and val(c,'buffer') == id and val(c,'offset') == at))
            add(f'missing-field-{id}-{at}',remove_field)
        def patch(id,at,fmt,value, snapshots_only=False):
            def mutate(t,b):
                for c in t.find('./chunks'):
                    member=None; name=c.get('name')
                    if not snapshots_only and name == 'MTLDevice::newBufferWithBytes' and val(c,'Buffer') == id: member='initialData'
                    if not snapshots_only and name == 'Internal::Initial Contents' and val(c,'id') == id: member='Contents'
                    if name == 'Internal_MTLBufferModifyCPUContents' and val(c,'Buffer') == id: member='data'
                    if member:
                        key=f'{int(field(c,member).text):06}'; data=bytearray(b[key]); struct.pack_into(fmt,data,at,value); b[key]=bytes(data)
            return mutate
        for tag,at,fmt,value in (
            ('zero-width',88,'<I',0),('huge-work',88,'<I',4097),('dispatch-padding',100,'<I',1),
            ('raygen-short',8,'<Q',24),('miss-zero-stride',32,'<Q',0),('hit-local-root-stride',56,'<Q',64),
            ('miss-bounds',24,'<Q',4096),('null-GRS',104,'<Q',0),('resource-heap',112,'<Q',identity[sbt]),
            ('sampler-heap',120,'<Q',identity[sbt]),('extra-IFT-pointer',144,'<Q',identity[sbt]),
            ('callable-range',64,'<Q',identity[sbt]),('null-VFT',128,'<Q',0),('unknown-IFT',136,'<Q',999999)):
            add(tag,patch(packet,at,fmt,value))
        for tag,at,value in (('unbound-shader-index',8,0),('unknown-shader-index',8,65536),
                            ('unknown-intersection-index',0,1),('non-null-static-sampler',16,identity[sbt])):
            add(tag,patch(sbt,at,'<Q',value))
        def combined(*mutations):
            def mutate(t,b):
                for mutation in mutations: mutation(t,b)
            return mutate
        def pad_records(pad):
            return combined(*(patch(sbt,at,'<Q',pad) for at in (24,56,88)))
        add('legal-UE-unused-pad',pad_records(2**64-1),success=True,oracle_mode='ue-pad')
        add('legal-pattern-unused-pad',pad_records(0xa5a5d00d98761234),success=True,oracle_mode='pattern-pad')
        add('legal-null-hit',patch(sbt,72,'<Q',0),success=True,oracle_mode='null-hit')
        add('legal-null-miss',patch(sbt,40,'<Q',0),success=True,oracle_mode='null-miss')
        add('legal-UE-any-hit',combined(pad_records(2**64-1),patch(sbt,64,'<Q',4)),
            success=True,oracle_mode='ue-any-hit')
        for tag,at,value in [('hit-any-hit-is-raygen',64,1),('hit-any-hit-is-closest',64,3),
            ('hit-any-hit-outside-VFT',64,65536),('miss-shader-is-closest',40,3),
            ('hit-shader-is-miss',72,2),('hit-shader-is-any-hit',72,4)]:
            add(tag,patch(sbt,at,'<Q',value))
        role_reason=['Invalid Metal IR shader role declaration',
            'Failed to process Metal chunk MTLFunctionHandle::DeclareRayIRShaderRole']
        def role_chunk(t,role=0):
            return next(c for c in t.find('./chunks') if c.get('name') == 'MTLFunctionHandle::DeclareRayIRShaderRole' and val(c,'role')==role)
        for tag,name,value in [('role-zero-handle','function',0),('role-unknown-handle','function',999999),
            ('role-wrong-type-buffer','function',sbt),('role-wrong-type-PSO','function',val(declaration,'pipeline')),
            ('role-outside-budget','role',4)]:
            def mutate_role(t,b,name=name,value=value):field(role_chunk(t),name).text=str(value)
            add(tag,mutate_role,role_reason)
        def role_duplicate(t,b):
            p=t.find('./chunks'); c=role_chunk(t); p.insert(list(p).index(c)+1,copy.deepcopy(c))
        add('role-duplicate',role_duplicate,role_reason)
        def role_frame(t,b):
            p=t.find('./chunks'); c=role_chunk(t); p.remove(c); p.append(c)
        add('role-frame-declaration',role_frame,role_reason)
        def role_prebirth(t,b):
            p=t.find('./chunks'); c=role_chunk(t); handle=val(c,'function'); p.remove(c)
            birth=next(x for x in p if x.get('name') == 'MTLComputePipelineState::functionHandleWithFunction' and val(x,'Handle') == handle)
            p.insert(list(p).index(birth),c)
        add('role-before-handle-birth',role_prebirth,role_reason)
        def wrong_role(t,b):field(role_chunk(t),'role').text='1'
        add('role-raygen-declared-as-miss',wrong_role)
        def remove_roles(t,b):
            p=t.find('./chunks')
            for c in list(p):
                if c.get('name') == 'MTLFunctionHandle::DeclareRayIRShaderRole':p.remove(c)
        add('legal-old-contract-no-roles',remove_roles,success=True)
        add('any-hit-missing-role-evidence',combined(remove_roles,patch(sbt,64,'<Q',4)))
        add('header-unknown-AS',patch(header,0,'<Q',999999))
        add('header-reserved-data',patch(header,16,'<Q',1))
        add('nonzero-instance-contribution-with-zero-stride',patch(contributions,0,'<I',1))
        add('null-AS-root',patch(grs,0,'<Q',0)); add('UAV-overwrites-SBT',patch(grs,8,'<Q',identity[sbt]))
        add('UAV-overwrites-contributions',patch(grs,8,'<Q',identity[contributions]))
        add('mutated-commit-snapshot',patch(packet,88,'<I',1,True))
        add('legal-hit-stride32',patch(packet,56,'<Q',32),success=True)
        def missing_blas(t,b):
            p=t.find('./chunks'); p.remove(next(c for c in p if c.get('name') == 'Internal::Initial Contents' and field(c,'kind') is not None and val(c,'kind') == 1))
        add('missing-BLAS-initial',missing_blas)
        def geometry_offset(t,b):
            c=next(c for c in t.find('./chunks') if c.get('name') == 'Internal::Initial Contents' and field(c,'kind') is not None and val(c,'kind') == 1)
            field(c,'parameters')[4].text='1'
        add('geometry-IFT-outside-table',geometry_offset)
        def wrong_as_kind(t,b):
            p=t.find('./chunks'); child=next(c for c in p if c.get('name') == 'Internal::Initial Contents' and field(c,'kind') is not None and val(c,'kind') == 1)
            field(next(c for c in p if c.get('name') == 'MTLAccelerationStructure::CaptureGPUIdentity'),'resource').text=field(child,'id').text
        add('header-primitive-AS-instead-of-TLAS',wrong_as_kind)

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
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as gpu_lock:
        fcntl.flock(gpu_lock.fileno(), fcntl.LOCK_EX)
        raise SystemExit(main())
