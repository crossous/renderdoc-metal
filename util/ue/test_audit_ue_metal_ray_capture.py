#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import unittest
import xml.etree.ElementTree as ET
from audit_ue_metal_ray_capture import analyse_chunks


def chunk(name,id='1000',**values):
    c=ET.Element('chunk',name=name,id=id,chunkIndex='1')
    for key,value in values.items():ET.SubElement(c,'value',name=key).text=str(value)
    return c


def pipeline(id,function):
    c=chunk('MTLDevice::newComputePipelineStateWithDescriptor',ComputePipelineState=id)
    d=ET.SubElement(c,'struct',name='descriptor');ET.SubElement(d,'ResourceId',name='computeFunction').text=str(function)
    return c


class RayInventoryTests(unittest.TestCase):
    def prefix(self):
        return [chunk('MTLLibrary::newFunctionWithName',Function=1,FunctionName='RaygenIndirection'),
            chunk('MTLLibrary::newFunctionWithName',Function=2,FunctionName='ordinary_compute'),
            pipeline(3,1),pipeline(4,2),chunk('Internal::Frame Metadata',id='5')]

    def test_bound_pipeline_and_packet(self):
        r=analyse_chunks(self.prefix()+[chunk('MTLComputeCommandEncoder::setComputePipelineState',ComputeCommandEncoder=8,pipeline=3),
            chunk('MTLComputeCommandEncoder::setBuffer',ComputeCommandEncoder=8,buffer=9,index=3,offset=16),
            chunk('MTLComputeCommandEncoder::dispatchThreadgroups',ComputeCommandEncoder=8)])
        self.assertEqual(len(r['IR_dispatches']),1);self.assertEqual(r['IR_dispatches'][0]['IR_packet_binding'],{'buffer':9,'offset':16})
        self.assertEqual(r['IR_missing_typed_pipeline_declarations'],[3]);self.assertFalse(r['replay_validated'])

    def test_UE_descriptor_function_name_and_residency(self):
        xs=[chunk('MTLLibrary::newFunctionWithDescriptor_completionHandler',Function=1,functionName='RaygenIndirection'),
            pipeline(3,1),chunk('MTLHeap::newAccelerationStructure',Structure=7),
            chunk('Internal::Frame Metadata',id='5'),
            chunk('MTLComputeCommandEncoder::setComputePipelineState',ComputeCommandEncoder=8,pipeline=3)]
        resources=chunk('MTLComputeCommandEncoder::useResources',ComputeCommandEncoder=8)
        array=ET.SubElement(resources,'array',name='resources')
        for resource in (7,99):ET.SubElement(array,'ResourceId').text=str(resource)
        r=analyse_chunks(xs+[resources,chunk('MTLComputeCommandEncoder::dispatchThreadgroups',ComputeCommandEncoder=8)])
        self.assertEqual(len(r['IR_dispatches']),1)
        self.assertEqual(r['IR_dispatches'][0]['AS_residency'],[7])
        self.assertEqual(r['IR_missing_packet_bindings'],['1'])

    def test_resident_AS_is_candidate_only_and_encoder_scoped(self):
        xs=self.prefix()+[chunk('MTLAccelerationStructure::CaptureGPUIdentity',resource=7),
            chunk('MTLComputeCommandEncoder::useResource',ComputeCommandEncoder=8,resource=7),
            chunk('MTLComputeCommandEncoder::dispatchThreadgroups',ComputeCommandEncoder=8),
            chunk('MTLComputeCommandEncoder::dispatchThreadgroups',ComputeCommandEncoder=10)]
        r=analyse_chunks(xs)
        self.assertEqual(len(r['inline_candidates']),1)
        self.assertEqual(r['ordinary_or_unproven_compute_count'],1)
        self.assertFalse(r['ray_outputs_validated'])

    def test_indirect_evidence_requires_owner_source_and_unique_record(self):
        evidence=chunk('MTLComputeCommandEncoder::CaptureIndirectArguments',command=5,encoder=8,ordinal=0,buffer=9,offset=16)
        a=ET.SubElement(evidence,'array',name='groups')
        for n in (2,3,1):ET.SubElement(a,'uint').text=str(n)
        xs=self.prefix();scope=xs.pop()
        owner=chunk('MTLCommandBuffer::computeCommandEncoder',CommandBuffer=5,ComputeCommandEncoder=8)
        dispatch=chunk('MTLComputeCommandEncoder::dispatchThreadgroups(indirect)',ComputeCommandEncoder=8,indirectBuffer=9,indirectBufferOffset=16)
        r=analyse_chunks(xs+[evidence,scope,owner,dispatch])
        self.assertEqual(r['compute_dispatches'][0]['captured_indirect']['groups'],[2,3,1])
        owner.find('./*[@name="CommandBuffer"]').text='6'
        self.assertIsNone(analyse_chunks(xs+[evidence,scope,owner,dispatch])['compute_dispatches'][0]['captured_indirect'])
        owner.find('./*[@name="CommandBuffer"]').text='5'
        self.assertIsNone(analyse_chunks(xs+[evidence,evidence,scope,owner,dispatch])['compute_dispatches'][0]['captured_indirect'])

    def test_debug_label_does_not_prove_rays(self):
        r=analyse_chunks(self.prefix()+[chunk('MTLComputeCommandEncoder::setComputePipelineState',ComputeCommandEncoder=8,pipeline=4),
            chunk('MTLComputeCommandEncoder::pushDebugGroup',ComputeCommandEncoder=8,string='Lumen DispatchRays: Compute'),
            chunk('MTLComputeCommandEncoder::dispatchThreadgroups',ComputeCommandEncoder=8)])
        self.assertEqual(r['IR_dispatches'],[]);self.assertEqual(r['ordinary_or_unproven_compute_count'],1)

    def test_pipeline_rebinding_and_encoder_isolation(self):
        r=analyse_chunks(self.prefix()+[chunk('MTLComputeCommandEncoder::setComputePipelineState',ComputeCommandEncoder=8,pipeline=3),
            chunk('MTLComputeCommandEncoder::setComputePipelineState',ComputeCommandEncoder=8,pipeline=4),
            chunk('MTLComputeCommandEncoder::dispatchThreadgroups',ComputeCommandEncoder=8),
            chunk('MTLComputeCommandEncoder::dispatchThreadgroups',ComputeCommandEncoder=10)])
        self.assertEqual(r['IR_dispatches'],[]);self.assertEqual(r['ordinary_or_unproven_compute_count'],2)

    def test_initialization_commands_are_not_frame_dispatches(self):
        xs=self.prefix();scope=xs.pop();r=analyse_chunks(xs+[chunk('MTLComputeCommandEncoder::setComputePipelineState',ComputeCommandEncoder=8,pipeline=3),
            chunk('MTLComputeCommandEncoder::dispatchThreadgroups',ComputeCommandEncoder=8),scope])
        self.assertEqual(r['IR_dispatches'],[])

    def test_bound_AS_is_only_inline_candidate_and_can_be_cleared(self):
        r=analyse_chunks(self.prefix()+[chunk('MTLComputeCommandEncoder::setAccelerationStructure',ComputeCommandEncoder=8,structure=7,index=0),
            chunk('MTLComputeCommandEncoder::dispatchThreadgroups',ComputeCommandEncoder=8),
            chunk('MTLComputeCommandEncoder::setAccelerationStructure',ComputeCommandEncoder=8,structure=0,index=0),
            chunk('MTLComputeCommandEncoder::dispatchThreadgroups',ComputeCommandEncoder=8)])
        self.assertEqual(len(r['inline_candidates']),1);self.assertEqual(r['ordinary_or_unproven_compute_count'],1);self.assertFalse(r['ray_outputs_validated'])


if __name__=='__main__':unittest.main()
