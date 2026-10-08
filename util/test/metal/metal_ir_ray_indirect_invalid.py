#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded indirect-TLAS converted-IR corruption and positive controls."""
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
    parser.add_argument('--mode', default='ue-indirect-tlas-private')
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
        with ZipFile(str(original)[:-4]) as archive:blobs={name:archive.read(name) for name in archive.namelist()}
        heap='heap-as' in args.mode
        identities={val(c,'resource'):val(c,'value') for c in chunks if c.get('name')=='MTLResource::CaptureGPUIdentity' and val(c,'kind')==0}
        as_ids={val(c,'resource'):val(c,'value') for c in chunks if c.get('name')=='MTLAccelerationStructure::CaptureGPUIdentity'}
        header=val(next(c for c in chunks if c.get('name')=='MTLResource::setLabel' and field(c,'label').text==('IR heap AS header' if heap else 'IR AS header')),'resource')
        header_initial=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and val(c,'id')==header)
        header_bytes=blobs[f'{int(field(header_initial,"Contents").text):06}']
        selected_gpu,contribution_va=struct.unpack_from('<QQ',header_bytes)
        selected=next(id for id,gpu in as_ids.items() if gpu==selected_gpu)
        selected_initial=next(c for c in chunks if c.get('name')=='Internal::Initial Contents' and val(c,'id')==selected and field(c,'kind') is not None)
        kind=val(selected_initial,'kind');assert kind in (9,10,11)
        contributions=next(id for id,va in identities.items() if va==contribution_va)
        source=val(selected_initial,'source')
        children=[int(c.text) for c in field(selected_initial,'children')]
        cases=[]
        reasons=['Invalid Metal IR ray ABI closure','Invalid Metal acceleration structure initial contents',
            'Failed to process Metal chunk','Failed to replay Metal chunk','Invalid or unsupported explicit Metal descriptor initial data']
        def add(tag,mutate,success=False,mode=None):cases.append((tag,mutate,reasons,success,mode or args.mode))
        def recipe(t):return next(c for c in t.find('./chunks') if c.get('name')=='Internal::Initial Contents' and val(c,'id')==selected and field(c,'kind') is not None)
        def scalar(name,value):return lambda t,b:setattr(field(recipe(t),name),'text',str(value))
        def parameter(index,value):return lambda t,b:setattr(list(field(recipe(t),'parameters'))[index],'text',str(value))
        def blob_patch(id,member,at,fmt,value):
            def mutate(t,b):
                for c in t.find('./chunks'):
                    part=None
                    if c.get('name')=='Internal::Initial Contents' and val(c,'id')==id and field(c,member) is not None:part=member
                    if member=='Contents' and c.get('name')=='MTLDevice::newBufferWithBytes' and val(c,'Buffer')==id:part='initialData'
                    if part:
                        key=f'{int(field(c,part).text):06}';raw=bytearray(b[key]);struct.pack_into(fmt,raw,at,value);b[key]=bytes(raw)
            return mutate
        def replace_vertices(raw):
            def mutate(t,b):
                chunk=recipe(t);node=field(chunk,'vertices')
                # Growing an empty byte buffer also changes its alignment. Give
                # the structured writer enough room; test the typed recipe, not
                # an accidental chunk overrun from its old capture byte length.
                growth=max(0,len(raw)-int(node.get('byteLength')))
                chunk.set('length',str(int(chunk.get('length'))+growth+64))
                node.set('byteLength',str(len(raw)));b[f'{int(node.text):06}']=raw
            return mutate
        for tag,name,value in [('unknown-schema','schema',99),('wrong-schema','schema',4),('unknown-kind','kind',99),('primitive-kind','kind',1),('wrong-recipe-kind','kind',5)]:add(tag,scalar(name,value))
        for tag,index,value in [('unaligned-source-offset',0,1),('source-offset-outside',0,88),('short-stride',1,64),('unaligned-stride',1,73),('stride-outside-budget',1,1048584),('direct-type',2,0),('motion-type',2,4),('count-outside-IR-budget',3,65),('nonzero-p4',4,1),('nonzero-p5',5,1),('nonzero-p6',6,1),('nonzero-p7',7,1)]:add(tag,parameter(index,value))
        add('unknown-source',scalar('source',999999))
        add('source-is-AS',scalar('source',selected))
        add('null-header-AS',blob_patch(header,'Contents',0,'<Q',0))
        add('unknown-header-AS',blob_patch(header,'Contents',0,'<Q',999999))
        add('reserved-header-word',blob_patch(header,'Contents',16,'<Q',1))
        add('unknown-contribution-VA',blob_patch(header,'Contents',8,'<Q',contribution_va+1048576))
        add('unaligned-contribution-VA',blob_patch(header,'Contents',8,'<Q',contribution_va+1))
        raw=blobs[f'{int(field(selected_initial,"vertices").text):06}']
        if kind==10:
            add('empty-has-vertices',replace_vertices(bytes(72)))
            add('empty-count-nonzero',parameter(3,1))
            def empty_child(t,b):field(recipe(t),'children').append(copy.deepcopy(field(recipe(t),'id')))
            add('empty-has-child',empty_child)
            add('legal-unused-contribution',blob_patch(contributions,'Contents',0,'<I',999999),True)
            add('legal-null-contribution-VA',blob_patch(header,'Contents',8,'<Q',0),True)
        else:
            add('zero-count-nonempty-recipe',parameter(3,0))
            add('truncated-instance',replace_vertices(raw[:71]))
            add('unknown-instance-AS',blob_patch(selected,'vertices',64,'<Q',999999))
            add('instance-is-TLAS',blob_patch(selected,'vertices',64,'<Q',selected_gpu))
            add('instance-contribution-outside-SBT',blob_patch(contributions,'Contents',0,'<I',1))
            if kind==9:
                add('null-instance-AS',blob_patch(selected,'vertices',64,'<Q',0))
                add('geometry-IFT-slot-outside-table',blob_patch(selected,'vertices',56,'<I',1))
                def missing_child(t,b):
                    a=field(recipe(t),'children')
                    for v in list(a):a.remove(v)
                add('missing-child',missing_child)
                def duplicate_child(t,b):a=field(recipe(t),'children');a.append(copy.deepcopy(list(a)[0]))
                add('duplicate-child',duplicate_child)
                add('legal-UserID-74',blob_patch(selected,'vertices',60,'<I',74),True,args.mode+'-user74')
                def child_remap(t,b):
                    child=children[0];gpu=as_ids[child]+256
                    for c in t.find('./chunks'):
                        if c.get('name')=='MTLAccelerationStructure::CaptureGPUIdentity' and val(c,'resource')==child:field(c,'value').text=str(gpu)
                    blob_patch(selected,'vertices',64,'<Q',gpu)(t,b)
                add('legal-child-GPU-ID-remap',child_remap,True)
            else:
                add('masked-active-mask',blob_patch(selected,'vertices',52,'<I',255))
                if children:raise ValueError('masked fixture has child refs')
                add('legal-masked-UserID-74',blob_patch(selected,'vertices',60,'<I',74),True)
        def as_remap(t,b):
            for c in t.find('./chunks'):
                if c.get('name')=='MTLAccelerationStructure::CaptureGPUIdentity' and val(c,'resource')==selected:field(c,'value').text=str(selected_gpu+256)
            blob_patch(header,'Contents',0,'<Q',selected_gpu+256)(t,b)
        add('legal-TLAS-GPU-ID-remap',as_remap,True)
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
