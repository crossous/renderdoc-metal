#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Join immutable converter reflection to native PSOs and actual AIR, independently of effects."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET
import zipfile

from audit_ue_metal_ray_capture import analyse, field, number
from audit_ue_metal_ray_air import query_calls
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'shader_tools'))
from metal_air_processor import disassemble


def inspect(xml, output):
    xml=Path(xml);output=Path(output);output.mkdir(parents=True,exist_ok=True)
    functions={};libraries={};pipelines={};facts={};frame=False
    for _,c in ET.iterparse(xml,events=('end',)):
        if c.tag!='chunk':continue
        name=c.get('name','')
        if c.get('id')=='5':frame=True
        if name.startswith('MTLDevice::newLibrary') and field(c,'data') is not None:
            libraries[number(c,'Library')]=f'{number(c,"data"):06}'
        if name.startswith('MTLLibrary::newFunction'):
            n=field(c,'FunctionName')
            if n is None:n=field(c,'functionName')
            if n is not None:functions[number(c,'Function')]=(number(c,'Library'),n.text or '')
        if name.startswith('MTLDevice::newComputePipelineState'):
            descriptor=field(c,'descriptor')
            function=number(descriptor,'computeFunction') if descriptor is not None else number(c,'function')
            # The oldest function-only capture spelling uses computeFunction.
            if not function:function=number(c,'computeFunction')
            pipelines[number(c,'ComputePipelineState')]=function
        if name=='MTLComputePipelineState::CaptureIRComputeReflection':
            p=number(c,'pipeline');text=field(c,'reflection').text or ''
            if frame or not p or p in facts:raise ValueError('duplicate/frame/zero immutable PSO fact')
            facts[p]=text
        c.clear()
    inventory=analyse(xml);dispatches=inventory['compute_dispatches']
    result=dict(status='CPU PSO/COMPILER/AIR EVIDENCE ONLY',GPU_commands_submitted=0,
        replay_validated=False,ray_outputs_validated=False,xml=str(xml.resolve()),entries=[],
        bound_ray_query_dispatches=[],limitations=[
            'Compiler metadata is a captured fact, not a replay eligibility declaration.',
            'Dynamic heap indices, resource access ranges and producer/consumer dependencies need separate validation.',
            'AIR calls and nonzero dispatch groups do not establish output correctness or that each invocation takes the query branch.'])
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        for pipeline,text in sorted(facts.items()):
            item=dict(pipeline=pipeline,reflection_sha256=hashlib.sha256(text.encode()).hexdigest())
            try:
                reflection=json.loads(text)
                if reflection['ShaderType']!='Compute':raise ValueError('non-compute compiler payload')
                roots=reflection['TopLevelArgumentBuffer'];end=0
                for root in sorted(roots,key=lambda r:r['EltOffset']):
                    offset,size=root['EltOffset'],root['Size']
                    if type(offset)!=int or type(size)!=int or offset<end or size<=0 or offset+size>65536:
                        raise ValueError('invalid/overlapping compiler root range')
                    end=offset+size
                groups=reflection['state']['tg_size']
                if len(groups)!=3 or any(type(v)!=int or v<1 or v>1024 for v in groups) or groups[0]*groups[1]*groups[2]>1024:
                    raise ValueError('invalid compiler threadgroup size')
                function=pipelines[pipeline];library,kernel=functions[function]
                data=archive.read(libraries[library]);sha=hashlib.sha256(data).hexdigest()
                stem=f'{pipeline}-{sha[:16]}'
                binary=output/(stem+'.metallib');binary.write_bytes(data)
                module=disassemble(binary,kernel);air=output/(stem+'.ll');air.write_text(module)
                calls=query_calls(module)
                runtime=reflection.get('Origin')=='MetalIRRuntimeBindings'
                claimed=reflection.get('UsesRayQuery')
                if runtime:
                    if claimed is not None:raise ValueError('runtime ABI cannot claim compiler query use')
                    if any(type(reflection.get(k))!=int or not 0<=reflection[k]<31 for k in
                        ('RootBindPoint','ResourceHeapBindPoint','SamplerHeapBindPoint')):
                        raise ValueError('invalid runtime ABI binding points')
                elif type(claimed)!=bool or claimed!=bool(calls):raise ValueError('compiler/AIR ray-query mismatch')
                bound=[d for d in dispatches if d['pipeline']==pipeline]
                for dispatch in bound:
                    if dispatch['threads_per_group']!=groups:
                        raise ValueError('metadata/native dispatch threadgroup mismatch')
                    if runtime and dispatch['root_bytes'].get(reflection['RootBindPoint'])!=end:
                        raise ValueError('runtime ABI/native root payload length mismatch')
                item.update(status='PSO RUNTIME ABI/AIR INSPECTED' if runtime else 'PSO COMPILER/AIR MATCH',
                    metadata_origin='runtime binding ABI' if runtime else 'compiler JSON',function=function,library=library,
                    kernel=kernel,library_sha256=sha,AIR=str(air.resolve()),uses_ray_query=bool(calls),
                    binding_points={k:reflection[k] for k in ('RootBindPoint','ResourceHeapBindPoint','SamplerHeapBindPoint') if k in reflection},
                    root_bytes=end,roots=roots,used_resources=reflection.get('UsedResources',[]),
                    threadgroup_size=groups,dispatches=bound,ray_query_calls=calls)
                if calls:
                    for dispatch in bound:
                        work=dispatch['direct_groups'] or (dispatch['captured_indirect'] or {}).get('groups')
                        if work and len(work)==3 and all(v>0 for v in work):
                            result['bound_ray_query_dispatches'].append(dict(pipeline=pipeline,groups=work,
                                chunk_index=dispatch['chunk_index'],library_sha256=sha))
            except Exception as error:item.update(status='UNVALIDATED',error=str(error))
            result['entries'].append(item)
    result['unvalidated_entries']=sum(e['status']=='UNVALIDATED' for e in result['entries'])
    (output/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--xml',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();result=inspect(a.xml,a.output)
    print('PSO facts',len(result['entries']),'unvalidated',result['unvalidated_entries'],
        'nonzero query dispatches',len(result['bound_ray_query_dispatches']))
    return 1 if not result['entries'] or result['unvalidated_entries'] else 0


if __name__=='__main__':sys.exit(main())
