#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU-only ray workload/typed-contract inventory; never claims replay or output PASS."""
import argparse
from collections import Counter
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def field(node,name):
    return node.find('./*[@name="'+name+'"]')


def number(node,name,default=0):
    item=field(node,name)
    return int(item.text) if item is not None and item.text else default


def analyse_chunks(chunks):
    functions,pipelines,encoders={}, {}, {}
    acceleration_structures=set();function_libraries={};indirect_records={};indirect_ordinals={};encoder_commands={};invalid_indirect_records=set()
    declarations=set();counts=Counter();dispatches=[];builds=[];scope=False
    for chunk in chunks:
        name=chunk.get('name','');counts[name]+=1
        if name.startswith('MTLLibrary::newFunction'):
            text=field(chunk,'FunctionName')
            if text is None:text=field(chunk,'functionName')
            if text is not None:
                functions[number(chunk,'Function')]=text.text or ''
                function_libraries[number(chunk,'Function')]=number(chunk,'Library')
        if name.startswith('MTLDevice::newComputePipelineState'):
            descriptor=field(chunk,'descriptor')
            function=number(descriptor,'computeFunction') if descriptor is not None else (number(chunk,'function') or number(chunk,'computeFunction'))
            pipelines[number(chunk,'ComputePipelineState')]=dict(function=function,
                linked_functions=[int(x.text) for x in descriptor.findall('./struct[@name="linkedFunctions"]/array/ResourceId')]
                    if descriptor is not None else [])
        if name in ('MTLHeap::newAccelerationStructure','MTLDevice::newAccelerationStructureWithSize'):
            acceleration_structures.add(number(chunk,'Structure'))
        if name=='MTLAccelerationStructure::CaptureGPUIdentity':
            acceleration_structures.add(number(chunk,'resource'))
        if name=='MTLComputePipelineState::DeclareRayIRDispatch':declarations.add(number(chunk,'pipeline'))
        if name=='MTLComputeCommandEncoder::CaptureIndirectArguments':
            groups=field(chunk,'groups');key=(number(chunk,'encoder'),number(chunk,'ordinal'))
            record=dict(command=number(chunk,'command'),buffer=number(chunk,'buffer'),offset=number(chunk,'offset'),
                groups=[int(x.text) for x in groups] if groups is not None else [])
            if scope or key in indirect_records or not key[0] or not record['command'] or not record['buffer'] or record['offset']%4 or len(record['groups'])!=3:
                invalid_indirect_records.add(key)
            indirect_records[key]=record
        if chunk.get('id')=='5':scope=True
        if not scope:continue
        if name.startswith('MTLAccelerationStructureCommandEncoder::build') or name.startswith('MTLAccelerationStructureCommandEncoder::refit'):
            builds.append(dict(chunk_index=chunk.get('chunkIndex'),kind=name,target=number(chunk,'structure',number(chunk,'destination'))))
        encoder=number(chunk,'ComputeCommandEncoder')
        if not encoder:continue
        if name.startswith('MTLCommandBuffer::computeCommandEncoder'):
            command=number(chunk,'CommandBuffer')
            encoder_commands[encoder]=command if encoder not in encoder_commands else 0
        state=encoders.setdefault(encoder,dict(pipeline=0,buffers={},structures={},resident_AS=set(),groups=[],root_bytes={}))
        if name.endswith('::setComputePipelineState'):state['pipeline']=number(chunk,'pipeline')
        elif name.endswith('::setBuffer'):state['buffers'][number(chunk,'index')]=dict(buffer=number(chunk,'buffer'),offset=number(chunk,'offset'))
        elif name.endswith('::setBytes'):
            data=field(chunk,'data')
            if data is not None:state['root_bytes'][number(chunk,'index')]=len(data)
        elif name.endswith('::setAccelerationStructure'):state['structures'][number(chunk,'index')]=number(chunk,'structure')
        elif name.endswith('::useResources'):
            resources=field(chunk,'resources')
            if resources is not None:
                state['resident_AS'].update(int(x.text) for x in resources if x.text and int(x.text) in acceleration_structures)
        elif name.endswith('::useResource'):
            resource=number(chunk,'resource')
            if resource in acceleration_structures:state['resident_AS'].add(resource)
        elif name.endswith('::pushDebugGroup'):
            text=field(chunk,'string');state['groups'].append(text.text or '' if text is not None else '')
        elif name.endswith('::popDebugGroup') and state['groups']:state['groups'].pop()
        elif '::dispatch' in name:
            pipeline=pipelines.get(state['pipeline'],{});kernel=functions.get(pipeline.get('function',0),'')
            structures={i:r for i,r in state['structures'].items() if r}
            evidence=None
            if field(chunk,'indirectBuffer') is not None:
                ordinal=indirect_ordinals.get(encoder,0);indirect_ordinals[encoder]=ordinal+1
                record=indirect_records.get((encoder,ordinal))
                if record and (encoder,ordinal) not in invalid_indirect_records and record['command']==encoder_commands.get(encoder) and record['buffer']==number(chunk,'indirectBuffer') and record['offset']==number(chunk,'indirectBufferOffset'):
                    evidence=record
            direct_groups=field(chunk,'groups')
            threads=field(chunk,'threadsPerGroup')
            ir=kernel=='RaygenIndirection'
            classification='IR RAY DISPATCH STRUCTURAL EVIDENCE' if ir else 'INLINE RAY CANDIDATE; SHADER VERIFICATION REQUIRED' if structures or state['resident_AS'] else 'COMPUTE; RAY USE NOT PROVEN'
            dispatches.append(dict(chunk_index=chunk.get('chunkIndex'),chunk=name,encoder=encoder,pipeline=state['pipeline'],kernel=kernel,
                function=pipeline.get('function',0),library=function_libraries.get(pipeline.get('function',0),0),
                captured_indirect=evidence,direct_groups=[int(x.text) for x in direct_groups] if direct_groups is not None else None,
                threads_per_group=[int(x.text) for x in threads] if threads is not None else None,
                root_bytes=dict(state['root_bytes']),
                classification=classification,debug_groups=list(state['groups']),AS_bindings=structures,AS_residency=sorted(state['resident_AS']),
                IR_packet_binding=state['buffers'].get(3) if ir else None,
                IR_typed_dispatch_declared=state['pipeline'] in declarations if ir else None,
                linked_functions=[functions.get(f,str(f)) for f in pipeline.get('linked_functions',[])] if ir else []))
    ir=[d for d in dispatches if d['classification'].startswith('IR RAY')]
    inline=[d for d in dispatches if (d['AS_bindings'] or d['AS_residency']) and not d['classification'].startswith('IR RAY')]
    return dict(status='CPU INVENTORY ONLY',GPU_commands_submitted=0,replay_validated=False,ray_outputs_validated=False,
        frame_metadata_found=scope,frame_AS_builds=builds,compute_dispatches=dispatches,IR_dispatches=ir,inline_candidates=inline,
        ordinary_or_unproven_compute_count=len(dispatches)-len(ir)-len(inline),
        IR_missing_packet_bindings=[d['chunk_index'] for d in ir if not d['IR_packet_binding'] or not d['IR_packet_binding']['buffer']],
        IR_missing_typed_pipeline_declarations=sorted({d['pipeline'] for d in ir if not d['IR_typed_dispatch_declared']}),
        typed_shader_role_count=counts['MTLFunctionHandle::DeclareRayIRShaderRole'],
        typed_global_root_count=counts['MTLComputePipelineState::DeclareRayIRGlobalRoot'],
        limitations=['No GPU address or arbitrary integer inference.',
            'AS bindings, residency or debug labels alone do not prove that a compute shader traces rays.',
            'IR classification uses the bound PSO and exact SDK kernel name; packet contents, native completion, outputs, resource bindings and replay require separate verification.'])


def analyse(path):
    def chunks():
        for _event,node in ET.iterparse(path,events=('end',)):
            if node.tag=='chunk':
                yield node
                node.clear()
    return analyse_chunks(chunks())


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xml',type=Path);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();result=analyse(args.xml);result['xml']=str(args.xml.resolve())
    args.output.write_text(json.dumps(result,indent=2)+'\n');print(result['status'],'IR',len(result['IR_dispatches']),'inline candidates',len(result['inline_candidates']))


if __name__=='__main__':main()
