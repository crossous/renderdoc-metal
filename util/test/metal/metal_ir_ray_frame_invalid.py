#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Finite frame-AS snapshot corruption, ordering and legal replay controls."""
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
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',type=Path);parser.add_argument('capture',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--oracle',type=Path,required=True)
    parser.add_argument('--mode',default='ue-indirect-tlas-private-frame-build')
    args=parser.parse_args();cli=args.command.resolve();capture=args.capture.resolve()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    library=cli.parent.parent/'lib/librenderdoc.dylib'
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    manifest=dict(status='RUNNING',backend_sha256=sha(library),source=str(capture),source_sha256=sha(capture),checks=[])
    env=os.environ.copy()
    for key in tuple(env):
        if key.startswith('RENDERDOC_METAL_') or key=='DYLD_INSERT_LIBRARIES':env.pop(key)
    env['MTL_DEBUG_LAYER']='1'

    def run(tag,command,rejected=False):
        path=out/(tag+'.log');current=env.copy();current['RENDERDOC_DEBUG_LOG_FILE']=str(path)
        with path.open('w') as log:
            fcntl.flock(log.fileno(),fcntl.LOCK_SH)
            process=subprocess.Popen([str(x) for x in command],stdout=log,stderr=subprocess.STDOUT,
                env=current,start_new_session=True)
            try:code=process.wait(timeout=35)
            except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait();code=124
        text=path.read_text(errors='replace')
        passed=(0<code<124 and any(x in text for x in ('Invalid Metal IR ray ABI closure',
            'Invalid or unsupported explicit Metal descriptor frame data','Failed to process Metal chunk'))) if rejected else code==0
        passed &= not any(x in text for x in ('Assertion failed','OVERRUNNING CHUNK',
            'Unexpected Metal resource type','m_ResourceMap.empty'))
        if not passed:raise RuntimeError(f'{tag}: exit{code}: {text[-1800:]}')
        return code

    try:
        original=out/'original.zip.xml';run('export',[cli,'convert','-f',capture,'-o',original,'-c','zip.xml'])
        tree=ET.parse(original);chunks=list(tree.find('./chunks'))
        field=lambda c,n:c.find('./*[@name="'+n+'"]')
        value=lambda c,n:int(field(c,n).text)
        names=lambda t,n:[c for c in t.find('./chunks') if c.get('name')==n]
        build_name='MTLAccelerationStructureCommandEncoder::buildIndirectInstances'+('WithScratchOffset' if 'scratch-offset' in args.mode else '')
        build=lambda t:names(t,build_name)[0]
        raw_field=field(build(tree),'descriptorBytes')
        with ZipFile(str(original)[:-4]) as archive:blobs={n:archive.read(n) for n in archive.namelist()}
        labels={field(c,'label').text:value(c,'resource') for c in chunks if c.get('name')=='MTLResource::setLabel'}
        header=labels['IR heap AS header' if 'heap-as' in args.mode else 'IR AS header']
        selected=value(build(tree),'structure');child=int(list(field(build(tree),'children'))[0].text)
        gpu={value(c,'resource'):value(c,'value') for c in chunks if c.get('name')=='MTLAccelerationStructure::CaptureGPUIdentity'}
        source=value(build(tree),'instances')
        cases=[]
        def add(tag,mutation,legal=False,mode=None):cases.append((tag,mutation,legal,mode or args.mode))
        def scalar(name,val):return lambda t,b:setattr(field(build(t),name),'text',str(val))
        def parameter(index,val):return lambda t,b:setattr(list(field(build(t),'parameters'))[index],'text',str(val))
        def snapshot(at,fmt,val):
            def change(t,b):
                key=f'{int(field(build(t),"descriptorBytes").text):06}';raw=bytearray(b[key])
                struct.pack_into(fmt,raw,at,val);b[key]=bytes(raw)
            return change
        for tag,name,val in [('unknown-encoder','Encoder',999999),('unknown-target','structure',999999),
            ('null-target','structure',0),('target-is-BLAS','structure',child),('unknown-input','instances',999999),
            ('input-is-TLAS','instances',selected),('input-is-Shared','instances',labels['IR frame instance upload']),
            ('unknown-scratch','scratch',999999),('scratch-is-Shared','scratch',labels['IR frame instance upload'])]:
            add(tag,scalar(name,val))
        for tag,index,val in [('unaligned-offset',0,1),('offset-outside-source',0,88),
            ('short-stride',1,64),('unaligned-stride',1,73),('stride-over-budget',1,1048584),
            ('direct-type',2,0),('motion-type',2,4),('zero-count-with-snapshot',3,0),
            ('count-over-IR-limit',3,65),('flags4',4,1),('flags5',5,1),('flags6',6,1),('flags7',7,1)]:
            add(tag,parameter(index,val))
        def children(t,b,ids):
            array=field(build(t),'children')
            for c in list(array):array.remove(c)
            for id in ids:ET.SubElement(array,'ResourceId',typename='MTLAccelerationStructure',width='8').text=str(id)
        add('missing-child',lambda t,b:children(t,b,[]))
        add('duplicate-child',lambda t,b:children(t,b,[child,child]))
        add('child-is-TLAS',lambda t,b:children(t,b,[selected]))
        add('unknown-child',lambda t,b:children(t,b,[999999]))
        add('unknown-child-GPU-ID',snapshot(64,'<Q',999999))
        add('null-child-GPU-ID',snapshot(64,'<Q',0))
        add('child-GPU-ID-is-TLAS',snapshot(64,'<Q',gpu[selected]))
        add('frame-IFT-outside-table',snapshot(56,'<I',2 if 'multi' in args.mode else 1))
        def truncated(t,b):
            c=build(t);node=field(c,'descriptorBytes');key=f'{int(node.text):06}'
            b[key]=b[key][:71];node.set('byteLength','71');c.set('length',str(int(c.get('length'))+64))
        add('truncated-snapshot',truncated)
        def remove_end(t,b):
            parent=t.find('./chunks');c=next(c for c in parent if c.get('name')=='MTLAccelerationStructureCommandEncoder::endEncoding')
            parent.remove(c)
        add('commit-with-live-AS-encoder',remove_end)
        def late_commit(t,b):
            parent=t.find('./chunks');encoder=value(build(t),'Encoder')
            start=next(c for c in parent if c.get('name')=='MTLCommandBuffer::accelerationStructureCommandEncoder' and value(c,'Encoder')==encoder)
            command=value(start,'CommandBuffer');commit=next(c for c in parent if c.get('name')=='MTLCommandBuffer::commit' and value(c,'CommandBuffer')==command)
            parent.remove(commit);parent.append(commit)
        add('dispatch-before-AS-submit',late_commit)
        def different_queue(t,b):
            parent=t.find('./chunks');encoder=value(build(t),'Encoder')
            start=next(c for c in parent if c.get('name')=='MTLCommandBuffer::accelerationStructureCommandEncoder' and value(c,'Encoder')==encoder)
            command=value(start,'CommandBuffer');creation=next(c for c in parent if c.get('name')=='MTLCommandQueue::commandBuffer' and value(c,'CommandBuffer')==command)
            field(creation,'CommandQueue').text='999999'
        add('AS-and-rays-different-queue',different_queue)
        def contribution(t,b):
            # Empty-initial TLAS reads no contributions until this frame build.
            root=next(c for c in t.find('./chunks') if c.get('name')=='Internal::Initial Contents' and value(c,'id')==header)
            raw=b[f'{value(root,"Contents"):06}'];va=struct.unpack_from('<Q',raw,8)[0]
            id=next(value(c,'resource') for c in t.find('./chunks') if c.get('name')=='MTLResource::CaptureGPUIdentity' and value(c,'kind')==0 and value(c,'value')==va)
            c=next(c for c in t.find('./chunks') if c.get('name')=='Internal::Initial Contents' and value(c,'id')==id)
            key=f'{value(c,"Contents"):06}';data=bytearray(b[key]);struct.pack_into('<I',data,0,1);b[key]=bytes(data)
        add('frame-instance-contribution-outside-SBT',contribution)
        def contribution_id(t):
            root=next(c for c in t.find('./chunks') if c.get('name')=='Internal::Initial Contents' and value(c,'id')==header)
            va=struct.unpack_from('<Q',blobs[f'{value(root,"Contents"):06}'],8)[0]
            return next(value(c,'resource') for c in t.find('./chunks') if c.get('name')=='MTLResource::CaptureGPUIdentity' and value(c,'kind')==0 and value(c,'value')==va)
        def gpu_fill(t,b,target):
            c=next(c for c in t.find('./chunks') if c.get('name')=='MTLBlitCommandEncoder::fillBuffer')
            field(c,'buffer').text=str(target(t));span=field(c,'range')
            field(span,'location').text='0';field(span,'length').text='4';field(c,'value').text='1'
        add('GPU-write-frame-contribution',lambda t,b:gpu_fill(t,b,contribution_id))
        add('GPU-write-AS-header',lambda t,b:gpu_fill(t,b,lambda _:header))
        add('GPU-write-dispatch-packet',lambda t,b:gpu_fill(t,b,lambda _:labels['IR dispatch packet']))
        def gpu_copy(t,b):
            c=next(c for c in t.find('./chunks') if c.get('name')=='MTLBlitCommandEncoder::copyFromBuffer' and field(c,'destinationBuffer') is not None)
            field(c,'destinationBuffer').text=str(contribution_id(t));field(c,'destinationOffset').text='0';field(c,'size').text='4'
        add('GPU-copy-frame-contribution',gpu_copy)
        if 'scratch-offset' in args.mode:
            scratch=value(build(tree),'scratch')
            creation=next(c for c in chunks if c.get('name')=='MTLDevice::newBufferWithLength' and value(c,'Buffer')==scratch)
            size=value(creation,'length')
            for tag,offset in [('scratch-offset-unaligned',1),('scratch-offset-at-end',size),
                ('scratch-offset-near-end',size//256*256-256),('scratch-offset-overflow',(1<<64)-1),
                ('scratch-offset-aligned-overflow',(1<<64)-256)]:add(tag,scalar('scratchOffset',offset))
            add('legal-new-chunk-zero-scratch-offset',scalar('scratchOffset',0),True,args.mode+'-scratch-zero-control')
        if 'geometry' in args.mode:
            multi='multi' in args.mode
            geometry_name='MTLAccelerationStructureCommandEncoder::'+('buildFrozenMultiIndexed' if multi else 'buildFrozenTriangles')
            geometry=lambda t:names(t,geometry_name)[0]
            def geometry_scalar(name,val):return lambda t,b:setattr(field(geometry(t),name),'text',str(val))
            def geometry_parameter(index,val):return lambda t,b:setattr(list(field(geometry(t),'parameters'))[index],'text',str(val))
            g=geometry(tree);vertex=value(g,'vertices');index=value(g,'indices');scratch=value(g,'scratch')
            for tag,name,val in [('unknown-encoder','Encoder',999999),('unknown-target','structure',999999),
                ('null-target','structure',0),('target-is-TLAS','structure',selected),('unknown-vertices','vertices',999999),
                ('vertices-is-scratch','vertices',scratch),('vertices-is-Shared','vertices',labels['IR geometry vertex upload']),
                ('unknown-indices','indices',999999),('null-scratch','scratch',0),('unknown-scratch','scratch',999999),
                ('scratch-is-vertices','scratch',vertex),('unknown-kind','kind',99),('kind-is-TLAS','kind',9),
                ('kind-family-mismatch','kind',2 if multi else 8),('wrong-indexed-kind','kind',1 if index else 2)]:
                add('geometry-'+tag,geometry_scalar(name,val))
            for tag,at,val in [('unaligned-offset',0,1),('offset-outside-source',0,256 if multi else 72),('offset-overflow',0,(1<<64)-1),
                ('short-stride',1,4),('unaligned-stride',1,13),('stride-over-budget',1,1048580),
                ('invalid-format',2,999),('zero-count',3,0),('count-over-budget',3,1000001),
                ('IFT-outside-table',4,2 if multi else 1),('IFT-over-budget',4,32),('opaque-overflow',5,2),
                ('duplicate-overflow',6,2),('unsupported-refit',7,1),('usage-overflow',7,(1<<64)-1)]:
                add('geometry-'+tag,geometry_parameter(at,val))
            creation=next(c for c in chunks if c.get('name')=='MTLDevice::newBufferWithLength' and value(c,'Buffer')==scratch)
            size=value(creation,'length')
            for tag,offset in [('scratch-unaligned',1),('scratch-at-end',size),('scratch-near-end',size//256*256-256),
                ('scratch-overflow',(1<<64)-1)]:add('geometry-'+tag,geometry_scalar('scratchOffset',offset))
            def geometry_bytes(t,b,name,change):
                node=field(geometry(t),name);key=f'{int(node.text):06}';raw=bytearray(b[key]);change(raw);b[key]=bytes(raw)
                node.set('byteLength',str(len(raw)))
            add('geometry-truncated-vertices',lambda t,b:geometry_bytes(t,b,'vertexBytes',lambda raw:raw.pop()))
            add('geometry-NaN-vertex',lambda t,b:geometry_bytes(t,b,'vertexBytes',lambda raw:struct.pack_into('<I',raw,160 if multi else 0,0x7fc00000)))
            if index:
                add('geometry-indices-is-Shared',geometry_scalar('indices',labels['IR geometry index upload']))
                add('geometry-index-unaligned',geometry_parameter(8,1))
                add('geometry-index-offset-outside',geometry_parameter(8,64 if multi else 24))
                add('geometry-unknown-index-type',geometry_parameter(9,99))
                add('geometry-truncated-indices',lambda t,b:geometry_bytes(t,b,'indexBytes',lambda raw:raw.pop()))
                add('geometry-index-outside-vertices',lambda t,b:geometry_bytes(t,b,'indexBytes',lambda raw:struct.pack_into('<I',raw,24 if multi else 0,100000)))
            def original_geometry(t,b):
                def restore(raw):
                    shift=-100 if 'heap-as' in args.mode else 100
                    for i in range(3):struct.pack_into('<f',raw,(160 if multi else 0)+i*16,struct.unpack_from('<f',raw,(160 if multi else 0)+i*16)[0]-shift)
                geometry_bytes(t,b,'vertexBytes',restore)
            add('legal-geometry-original-positions',original_geometry,True,args.mode+'-geometry-original-control')
            if multi:
                count=64 if 'multi64' in args.mode else 2
                last=(count-1)*10
                for tag,at,val in [('last-unaligned-offset',last,161),('last-outside-vertex',last,256),
                    ('last-short-stride',last+1,4),('last-invalid-format',last+2,999),
                    ('last-zero-count',last+3,0),('last-IFT-outside-table',last+4,2),
                    ('last-usage-mismatch',last+7,1),('last-index-unaligned',last+8,25),
                    ('last-index-outside',last+8,64),('last-index-type-unknown',last+9,99)]:
                    add('geometry-'+tag,geometry_parameter(at,val))
                def param_shape(t,b,size):
                    node=field(geometry(t),'parameters');items=[copy.deepcopy(x) for x in node]
                    for x in list(node):node.remove(x)
                    for n in range(size):node.append(copy.deepcopy(items[n%len(items)]))
                    geometry(t).set('length',str(int(geometry(t).get('length'))+8192))
                for tag,size in [('one-descriptor',10),('nonmultiple-array',19),('over-limit-array',650)]:
                    add('geometry-'+tag,lambda t,b,size=size:param_shape(t,b,size))
                add('geometry-referenced-Float4-NaN',lambda t,b:geometry_bytes(t,b,'vertexBytes',lambda raw:struct.pack_into('<I',raw,16,0x7fc00000)))
                add('geometry-UInt16-index-outside',lambda t,b:geometry_bytes(t,b,'indexBytes',lambda raw:struct.pack_into('<H',raw,4,65535)))
                # Reversing typed descriptors must change GeometryIndex while
                # keeping the winning descriptor's IFT slot and source ranges.
                def reverse_geometry(t,b):
                    node=field(geometry(t),'parameters');items=[copy.deepcopy(x) for x in node]
                    groups=[items[n:n+10] for n in range(0,len(items),10)]
                    for x in list(node):node.remove(x)
                    for group in reversed(groups):
                        for x in group:node.append(x)
                add('legal-geometry-reversed-order',reverse_geometry,True,args.mode+'-geometry-order-control')
            if 'new-target' in args.mode:
                add('geometry-new-target-without-build',lambda t,b:t.find('./chunks').remove(geometry(t)))
        add('legal-frame-UserID75',snapshot(60,'<I',75),True,args.mode+'-user75')
        def child_remap(t,b):
            replacement=gpu[child]+256
            for c in t.find('./chunks'):
                if c.get('name')=='MTLAccelerationStructure::CaptureGPUIdentity' and value(c,'resource')==child:field(c,'value').text=str(replacement)
                if c.get('name')=='Internal::Initial Contents' and field(c,'vertices') is not None and field(c,'kind') is not None and value(c,'kind')==9:
                    node=field(c,'vertices');key=f'{int(node.text):06}';raw=bytearray(b[key])
                    if len(raw)>=72 and struct.unpack_from('<Q',raw,64)[0]==gpu[child]:struct.pack_into('<Q',raw,64,replacement);b[key]=bytes(raw)
            snapshot(64,'<Q',replacement)(t,b)
        add('legal-child-GPU-ID-remap',child_remap,True)
        def same_command(t,b):
            parent=t.find('./chunks');encoder=value(build(t),'Encoder')
            start=next(c for c in parent if c.get('name')=='MTLCommandBuffer::accelerationStructureCommandEncoder' and value(c,'Encoder')==encoder)
            command=value(start,'CommandBuffer');index=list(parent).index(start)
            removed=set()
            for c in list(parent)[index:]:
                if c.get('name')=='MTLCommandQueue::commandBuffer':removed.add(value(c,'CommandBuffer'));parent.remove(c)
                elif c.get('name') in ('MTLCommandBuffer::commit','MTLCommandBuffer::waitUntilCompleted','MTLCommandBuffer::waitUntilScheduled'):parent.remove(c)
                elif field(c,'CommandBuffer') is not None and value(c,'CommandBuffer') in removed:field(c,'CommandBuffer').text=str(command)
            last=copy.deepcopy(next(c for c in chunks if c.get('name')=='MTLCommandBuffer::commit'))
            field(last,'CommandBuffer').text=str(command)
            end=next(c for c in parent if c.get('name')=='Internal::End of Capture');parent.insert(list(parent).index(end),last)
            wait=copy.deepcopy(next(c for c in chunks if c.get('name')=='MTLCommandBuffer::waitUntilCompleted'))
            field(wait,'CommandBuffer').text=str(command);parent.insert(list(parent).index(end),wait)
        add('legal-build-clear-dispatch-one-command',same_command,True)
        for tag,mutation,legal,mode in cases:
            variant=copy.deepcopy(tree);data=blobs.copy();mutation(variant,data)
            xml=out/(tag+'.zip.xml');variant.write(xml,encoding='unicode',xml_declaration=True)
            with ZipFile(str(xml)[:-4],'w') as archive:
                for name,raw in data.items():archive.writestr(name,raw)
            cap=out/(tag+'.rdc');run(tag+'-import',[cli,'convert','-f',xml,'-o',cap,'-c','rdc'])
            code=run(tag+'-replay',[cli,'replay','--loops','1',cap],not legal)
            if legal:run(tag+'-API',[args.oracle.resolve(),cap,mode])
            manifest['checks'].append(dict(name=tag,legal=legal,exit=code,passed=True))
            (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print('PASS '+tag,flush=True)
        if sha(library)!=manifest['backend_sha256']:raise RuntimeError('backend changed')
        manifest.update(status='PASS',rejected=sum(not c[2] for c in cases),positive_controls=sum(c[2] for c in cases))
    except (OSError,RuntimeError,subprocess.SubprocessError,ValueError,KeyError,StopIteration) as error:
        manifest.update(status='FAIL',error=str(error));print(error,flush=True);return 1
    finally:(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return 0


if __name__=='__main__':
    with (Path(tempfile.gettempdir())/'renderdoc-metal-ir-gpu-tests.lock').open('a') as lock:
        fcntl.flock(lock.fileno(),fcntl.LOCK_EX)
        raise SystemExit(main())
