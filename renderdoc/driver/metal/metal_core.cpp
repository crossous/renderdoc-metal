/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "metal_core.h"
#include "serialise/rdcfile.h"
#include "metal_blit_command_encoder.h"
#include "metal_argument_encoder.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_compute_command_encoder.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_library.h"
#include "metal_binary_archive.h"
#include "metal_render_command_encoder.h"
#include "metal_render_pipeline_state.h"
#include "metal_compute_pipeline_state.h"
#include "metal_visible_function_table.h"
#include "metal_acceleration_structure.h"
#include "metal_acceleration_structure_command_encoder.h"
#include "metal_replay.h"
#include "metal_sampler_state.h"
#include "metal_indirect_command_buffer.h"
#include "metal_heap.h"
#include "metal_rate_map.h"
#include "metal_texture.h"

static Threading::CriticalSection s_DrawableTexturesLock;
static rdcflatmap<MTL::Drawable *, WrappedMTLTexture *> s_DrawableTextures;

WriteSerialiser &WrappedMTLDevice::GetThreadSerialiser()
{
  WriteSerialiser *ser = (WriteSerialiser *)Threading::GetTLSValue(threadSerialiserTLSSlot);
  if(ser)
    return *ser;

  // slow path, but rare
  ser = new WriteSerialiser(new StreamWriter(1024), Ownership::Stream);

  uint32_t flags = WriteSerialiser::ChunkDuration | WriteSerialiser::ChunkTimestamp |
                   WriteSerialiser::ChunkThreadID;

  if(RenderDoc::Inst().GetCaptureOptions().captureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;

  ser->SetChunkMetadataRecording(flags);
  ser->SetUserData(GetResourceManager());
  ser->SetVersion(MetalInitParams::CurrentVersion);

  Threading::SetTLSValue(threadSerialiserTLSSlot, (void *)ser);

  {
    SCOPED_LOCK(m_ThreadSerialisersLock);
    m_ThreadSerialisers.push_back(ser);
  }

  return *ser;
}

void WrappedMTLDevice::AddAction(const ActionDescription &a)
{
  if(m_Replay)
    m_Replay->AddAction(a);
}

void WrappedMTLDevice::AddEvent()
{
  if(m_Replay && m_StructuredFile)
    m_Replay->AddEvent((uint32_t)m_StructuredFile->chunks.size() - 1, m_CurChunkOffset);
}

#define METAL_CHUNK_NOT_HANDLED()                               \
  {                                                             \
    RDCERR("MetalChunk::%s not handled", ToStr(chunk).c_str()); \
    return false;                                               \
  }

bool WrappedMTLDevice::ProcessChunk(ReadSerialiser &ser, MetalChunk chunk)
{
  // A legacy textureBarrier before the first GPU operation in a pass is redundant. Track every
  // render execution chunk so that case can be replayed without calling an API rejected by this
  // device's Metal validation layer; post-work barriers remain explicitly unsupported.
  if(m_ReplayRenderCommandEncoder)
  {
    switch(chunk)
    {
      case MetalChunk::MTLRenderCommandEncoder_drawPrimitives:
      case MetalChunk::MTLRenderCommandEncoder_drawPrimitives_instanced:
      case MetalChunk::MTLRenderCommandEncoder_drawPrimitives_instanced_base:
      case MetalChunk::MTLRenderCommandEncoder_drawPrimitives_indirect:
      case MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives:
      case MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced:
      case MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced_base:
      case MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_indirect:
      case MetalChunk::MTLRenderCommandEncoder_drawPatches:
      case MetalChunk::MTLRenderCommandEncoder_drawPatches_indirect:
      case MetalChunk::MTLRenderCommandEncoder_drawIndexedPatches:
      case MetalChunk::MTLRenderCommandEncoder_drawIndexedPatches_indirect:
      case MetalChunk::MTLRenderCommandEncoder_dispatchThreadsPerTile:
      case MetalChunk::MTLRenderCommandEncoder_executeCommandsInBuffer:
      case MetalChunk::MTLRenderCommandEncoder_executeCommandsInBuffer_indirect:
      case MetalChunk::MTLRenderCommandEncoder_drawMeshThreadgroups:
      case MetalChunk::MTLRenderCommandEncoder_drawMeshThreads:
      case MetalChunk::MTLRenderCommandEncoder_drawMeshThreadgroups_indirect:
        m_ReplayRenderCommandEncoder->MarkGPUWork();
        break;
      default: break;
    }
  }
  switch(chunk)
  {
    case MetalChunk::MTLDevice_newLibraryWithSource_async:
      return Serialise_asyncLibrary(ser, NULL, NULL, NULL, false);
    case MetalChunk::MTLDevice_newRenderPipelineStateWithDescriptor_async:
    case MetalChunk::MTLDevice_newRenderPipelineStateWithDescriptor_options_async:
    {
      RDMTL::RenderPipelineDescriptor descriptor;
      return Serialise_newRenderPipelineStateWithDescriptorOptions(
          ser, NULL, descriptor, MTL::PipelineOptionNone, false);
    }
    case MetalChunk::MTLDevice_newComputePipelineStateWithFunction_async:
    case MetalChunk::MTLDevice_newComputePipelineStateWithFunction_options_async:
      return Serialise_newComputePipelineStateWithFunctionOptions(
          ser, NULL, NULL, MTL::PipelineOptionNone, NULL, NULL);
    case MetalChunk::MTLDevice_newComputePipelineStateWithDescriptor_async:
    {
      RDMTL::ComputePipelineDescriptor descriptor;
      return Serialise_newComputePipelineStateWithDescriptor(
          ser, NULL, descriptor, MTL::PipelineOptionNone, false);
    }
    case MetalChunk::MTLCreateSystemDefaultDevice:
      return Serialise_MTLCreateSystemDefaultDevice(ser);
    case MetalChunk::MTLDevice_newCommandQueue: return Serialise_newCommandQueue(ser, NULL);
    case MetalChunk::MTLDevice_newCommandQueueWithMaxCommandBufferCount:
      return Serialise_newCommandQueue(ser, NULL, 0);
    case MetalChunk::MTLDevice_newHeapWithDescriptor:
      return Serialise_newHeap(ser, NULL, 0, MTL::StorageModePrivate,
                                MTL::CPUCacheModeDefaultCache,
                                MTL::HazardTrackingModeDefault, MTL::HeapTypeAutomatic);
    case MetalChunk::MTLDevice_newBufferWithLength:
    case MetalChunk::MTLDevice_newBufferWithBytes:
      return Serialise_newBufferWithBytes(ser, NULL, NULL, 0, MTL::ResourceOptionCPUCacheModeDefault);
    case MetalChunk::MTLDevice_newBufferWithBytesNoCopy:
      return Serialise_newBufferWithBytesNoCopy(ser, NULL, {}, 0, MTL::ResourceStorageModeShared);
    case MetalChunk::MTLDevice_newDepthStencilStateWithDescriptor:
    {
      RDMTL::DepthStencilDescriptor descriptor;
      return Serialise_newDepthStencilStateWithDescriptor(ser, NULL, descriptor);
    }
    case MetalChunk::MTLDevice_newTextureWithDescriptor:
    case MetalChunk::MTLDevice_newTextureWithDescriptor_iosurface:
    case MetalChunk::MTLDevice_newTextureWithDescriptor_nextDrawable:
    {
      RDMTL::TextureDescriptor descriptor;
      return Serialise_newTextureWithDescriptor(ser, NULL, descriptor);
    }
    case MetalChunk::MTLDevice_newSharedTextureWithDescriptor:
    {
      RDMTL::TextureDescriptor descriptor;
      return Serialise_newSharedTextureWithDescriptor(ser, NULL, descriptor);
    }
    case MetalChunk::MTLDevice_newSharedTextureWithHandle:
      return Serialise_newSharedTextureWithHandle(ser, NULL, NULL);
    case MetalChunk::MTLDevice_newSamplerStateWithDescriptor:
    {
      RDMTL::SamplerDescriptor descriptor;
      return Serialise_newSamplerStateWithDescriptor(ser, NULL, descriptor);
    }
    case MetalChunk::MTLDevice_newDefaultLibrary: return Serialise_newDefaultLibrary(ser, NULL);
    case MetalChunk::MTLDevice_newDefaultLibraryWithBundle:
    case MetalChunk::MTLDevice_newLibraryWithFile:
    case MetalChunk::MTLDevice_newLibraryWithURL:
    case MetalChunk::MTLDevice_newLibraryWithData:
    {
      bytebuf data;
      return Serialise_newLibraryBinary(ser, NULL, "", data);
    }
    case MetalChunk::MTLDevice_newLibraryWithSource:
      return Serialise_newLibraryWithSource(ser, NULL, NULL, NULL, NULL);
    case MetalChunk::MTLDevice_newLibraryWithStitchedDescriptor:
    case MetalChunk::MTLDevice_newLibraryWithStitchedDescriptor_async:
      return Serialise_newStitchedLibrary(ser, NULL, NULL, "", "", 0);
    case MetalChunk::MTLDevice_newRenderPipelineStateWithDescriptor:
    {
      RDMTL::RenderPipelineDescriptor descriptor;
      return Serialise_newRenderPipelineStateWithDescriptor(ser, NULL, descriptor, NULL);
    }
    case MetalChunk::MTLDevice_newRenderPipelineStateWithDescriptor_options:
    {
      RDMTL::RenderPipelineDescriptor descriptor;
      return Serialise_newRenderPipelineStateWithDescriptorOptions(
          ser, NULL, descriptor, MTL::PipelineOptionNone, false);
    }
    case MetalChunk::MTLDevice_newComputePipelineStateWithFunction:
      return Serialise_newComputePipelineStateWithFunction(ser, NULL, NULL, NULL);
    case MetalChunk::MTLDevice_newComputePipelineStateWithFunction_options:
      return Serialise_newComputePipelineStateWithFunctionOptions(
          ser, NULL, NULL, MTL::PipelineOptionNone, NULL, NULL);
    case MetalChunk::MTLDevice_newComputePipelineStateWithDescriptor:
    {
      RDMTL::ComputePipelineDescriptor descriptor;
      return Serialise_newComputePipelineStateWithDescriptor(
          ser, NULL, descriptor, MTL::PipelineOptionNone, false);
    }
    case MetalChunk::MTLDevice_newFence: return Serialise_newFence(ser, NULL);
    case MetalChunk::MTLDevice_newAccelerationStructureWithSize:
      return Serialise_newAccelerationStructureWithSize(ser, NULL, 0);
    case MetalChunk::MTLDevice_newAccelerationStructureWithDescriptor:
      return Serialise_newAccelerationStructureWithSize(ser, NULL, 0);
    case MetalChunk::MTLDevice_newRenderPipelineStateWithTileDescriptor:
    case MetalChunk::MTLDevice_newRenderPipelineStateWithTileDescriptor_async:
      return Serialise_newTileRenderPipelineState(ser, NULL, NULL, {}, 0, 0, false, 0, false, {});
    case MetalChunk::MTLDevice_newRenderPipelineStateWithMeshDescriptor:
    case MetalChunk::MTLDevice_newRenderPipelineStateWithMeshDescriptor_async:
      return Serialise_newMeshRenderPipelineState(ser, NULL, NULL, NULL, NULL, {}, 0, 0, 0, false);
    case MetalChunk::MTLDevice_newRenderPipelineStateWithObjectMeshDescriptor:
    case MetalChunk::MTLDevice_newRenderPipelineStateWithObjectMeshDescriptor_async:
      return Serialise_newObjectMeshPipelineState(ser, NULL, NULL, NULL, NULL, {},
                                                  0, 0, 0, 0, 0, 0, false);
    case MetalChunk::MTLDevice_newArgumentEncoderWithArguments:
      return Serialise_newArgumentEncoderWithArguments(ser, NULL, {});
    case MetalChunk::MTLDevice_newArgumentEncoderWithBufferBinding:
      return Serialise_newArgumentEncoderWithBufferBinding(ser, NULL, {}, 0, 0, false);
    case MetalChunk::MTLDevice_supportsRasterizationRateMapWithLayerCount:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLDevice_newRasterizationRateMapWithDescriptor:
      return Serialise_newRasterizationRateMap(ser, NULL, MTL::Size::Make(0, 0, 0), {}, {}, false);
    case MetalChunk::MTLRasterizationRateMap_copyParameterDataToBuffer:
      return m_DummyReplayRateMap->Serialise_copyParameterDataToBuffer(ser, NULL, 0);
    case MetalChunk::MTLDevice_newIndirectCommandBufferWithDescriptor:
      return Serialise_newIndirectCommandBufferWithDescriptor(
          ser, NULL, MTL::IndirectCommandTypeDraw, false, false, 0, 0, 0,
          MTL::ResourceStorageModeShared);
    case MetalChunk::MTLDevice_newEvent: return Serialise_newEvent(ser, NULL);
    case MetalChunk::MTLDevice_newSharedEvent: return Serialise_newSharedEvent(ser, NULL);
    case MetalChunk::MTLSharedEvent_setInitialSignaledValue:
      return Serialise_setSharedEventInitialValue(ser, NULL, 0);
    case MetalChunk::MTLDevice_newSharedEventWithHandle:
      return Serialise_importSharedEventHandle(ser, NULL, NULL);
    case MetalChunk::MTLDevice_newCounterSampleBufferWithDescriptor:
      return Serialise_newCounterSampleBuffer(ser, NULL, "", 0, 0, false);
    case MetalChunk::MTLDevice_newDynamicLibrary:
      return Serialise_newDynamicLibrary(ser, NULL, NULL, false);
    case MetalChunk::MTLDevice_newDynamicLibraryWithURL:
    {
      bytebuf data;
      return Serialise_newDynamicLibraryWithURL(ser, NULL, "", "", data);
    }
    case MetalChunk::MTLDevice_newBinaryArchiveWithDescriptor:
    {
      bytebuf data;
      return Serialise_newBinaryArchive(ser, NULL, data);
    }
    case MetalChunk::MTLBinaryArchive_addComputePipelineFunctionsWithDescriptor:
    {
      RDMTL::ComputePipelineDescriptor descriptor;
      return m_DummyReplayBinaryArchive->Serialise_addComputePipelineFunctions(ser, descriptor);
    }
    case MetalChunk::MTLBinaryArchive_addRenderPipelineFunctionsWithDescriptor:
    {
      RDMTL::RenderPipelineDescriptor descriptor;
      return m_DummyReplayBinaryArchive->Serialise_addRenderPipelineFunctions(ser, descriptor);
    }
    case MetalChunk::MTLBinaryArchive_addFunctionWithDescriptor:
      return m_DummyReplayBinaryArchive->Serialise_addFunction(ser, NULL, "");
    case MetalChunk::MTLBinaryArchive_addLibraryWithDescriptor:
      return m_DummyReplayBinaryArchive->Serialise_addLibrary(ser, NULL, "", "");
    case MetalChunk::MTLBinaryArchive_addTileRenderPipelineFunctionsWithDescriptor:
      return m_DummyReplayBinaryArchive->Serialise_addTilePipelineFunctions(
          ser, NULL, {}, 1, 0, false);
    case MetalChunk::MTLBinaryArchive_addMeshRenderPipelineFunctionsWithDescriptor:
      return m_DummyReplayBinaryArchive->Serialise_addMeshPipelineFunctions(
          ser, NULL, NULL, {}, 1, 0, 0);

    case MetalChunk::MTLLibrary_newFunctionWithName:
      return m_DummyReplayLibrary->Serialise_newFunctionWithName(ser, NULL, NULL);
    case MetalChunk::MTLLibrary_newFunctionWithName_constantValues:
    case MetalChunk::MTLLibrary_newFunctionWithDescriptor:
    case MetalChunk::MTLLibrary_newFunctionWithName_constantValues_async:
    case MetalChunk::MTLLibrary_newFunctionWithDescriptor_async:
      return m_DummyReplayLibrary->Serialise_newSpecializedFunction(ser, NULL, {});
    case MetalChunk::MTLLibrary_newIntersectionFunctionWithDescriptor:
      return m_DummyReplayLibrary->Serialise_newSpecializedFunction(ser, NULL, {});

    case MetalChunk::MTLFunction_newArgumentEncoderWithBufferIndex:
    {
      WrappedMTLFunction dummy(NULL, ResourceId(), this);
      return dummy.Serialise_newArgumentEncoder(ser, NULL, 0);
    }

    case MetalChunk::MTLCommandQueue_commandBuffer:
      return m_DummyReplayCommandQueue->Serialise_commandBuffer(ser, NULL);
    case MetalChunk::MTLCommandQueue_commandBufferWithDescriptor:
      return m_DummyReplayCommandQueue->Serialise_commandBufferWithDescriptor(ser, NULL, true, 0);
    case MetalChunk::MTLCommandQueue_commandBufferWithUnretainedReferences:
      return m_DummyReplayCommandQueue->Serialise_commandBufferWithUnretainedReferences(ser,
                                                                                         NULL);
    case MetalChunk::MTLCommandBuffer_enqueue:
      return m_DummyReplayCommandBuffer->Serialise_enqueue(ser);
    case MetalChunk::MTLCommandBuffer_commit:
      return m_DummyReplayCommandBuffer->Serialise_commit(ser);
    case MetalChunk::MTLCommandBuffer_addScheduledHandler:
      return m_DummyReplayCommandBuffer->Serialise_handlerRegistration(ser);
    case MetalChunk::MTLCommandBuffer_presentDrawable:
      return m_DummyReplayCommandBuffer->Serialise_presentDrawable(ser, NULL);
    case MetalChunk::MTLCommandBuffer_presentDrawable_atTime:
      return m_DummyReplayCommandBuffer->Serialise_presentDrawableTimed(ser, NULL, 0.0, false);
    case MetalChunk::MTLCommandBuffer_presentDrawable_afterMinimumDuration:
      return m_DummyReplayCommandBuffer->Serialise_presentDrawableTimed(ser, NULL, 0.0, true);
    case MetalChunk::MTLCommandBuffer_waitUntilScheduled:
      return m_DummyReplayCommandBuffer->Serialise_waitUntilScheduled(ser);
    case MetalChunk::MTLCommandBuffer_addCompletedHandler:
      return m_DummyReplayCommandBuffer->Serialise_handlerRegistration(ser);
    case MetalChunk::MTLCommandBuffer_waitUntilCompleted:
      return m_DummyReplayCommandBuffer->Serialise_waitUntilCompleted(ser);
    case MetalChunk::MTLCommandBuffer_blitCommandEncoder:
      return m_DummyReplayCommandBuffer->Serialise_blitCommandEncoder(ser, NULL);
    case MetalChunk::MTLCommandBuffer_renderCommandEncoderWithDescriptor:
    {
      RDMTL::RenderPassDescriptor descriptor;
      return m_DummyReplayCommandBuffer->Serialise_renderCommandEncoderWithDescriptor(ser, NULL,
                                                                                      descriptor);
    }
    case MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDescriptor:
      return m_DummyReplayCommandBuffer->Serialise_computeCommandEncoderWithDescriptor(
          ser, NULL, MTL::DispatchTypeSerial);
    case MetalChunk::MTLCommandBuffer_blitCommandEncoderWithDescriptor:
      return m_DummyReplayCommandBuffer->Serialise_blitCommandEncoderWithDescriptor(ser, NULL, false);
    case MetalChunk::MTLCommandBuffer_computeCommandEncoder:
      return m_DummyReplayCommandBuffer->Serialise_computeCommandEncoder(ser, NULL);
    case MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDispatchType:
      return m_DummyReplayCommandBuffer->Serialise_computeCommandEncoder(
          ser, NULL, MTL::DispatchTypeSerial);
    case MetalChunk::MTLCommandBuffer_encodeWaitForEvent:
      return m_DummyReplayCommandBuffer->Serialise_encodeEvent(ser, NULL, 0, false);
    case MetalChunk::MTLCommandBuffer_encodeSignalEvent:
      return m_DummyReplayCommandBuffer->Serialise_encodeEvent(ser, NULL, 0, true);
    case MetalChunk::MTLCommandBuffer_parallelRenderCommandEncoderWithDescriptor:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_resourceStateCommandEncoder: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_resourceStateCommandEncoderWithDescriptor:
      METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLCommandBuffer_accelerationStructureCommandEncoder:
      return m_DummyReplayCommandBuffer->Serialise_accelerationStructureCommandEncoder(ser, NULL);
    case MetalChunk::MTLCommandBuffer_accelerationStructureCommandEncoderWithDescriptor:
      return m_DummyReplayCommandBuffer->Serialise_accelerationStructureCommandEncoderWithDescriptor(
          ser, NULL, false);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildAccelerationStructure:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildTriangle(
          ser, NULL, NULL, 0, 0, NULL, 0);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildRefittableTriangle:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildTriangle(
          ser, NULL, NULL, 0, 0, NULL, 0, true);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_refitTriangle:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_refitTriangle(
          ser, NULL, NULL, 0, NULL);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_refitTriangleExtended:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_refitTriangleExtended(
          ser, NULL, NULL, NULL, 0, NULL, 0);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildBoundingBoxExtended:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildBoundingBoxExtended(
          ser, NULL, NULL, 0, 0, NULL, 0);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildBoundingBoxStrided:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildBoundingBoxStrided(
          ser, NULL, NULL, 0, 0, 0, NULL, 0);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildInstance:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildInstance(
          ser, NULL, NULL, NULL, NULL, {});
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildInstances:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildInstances(
          ser, NULL, NULL, NULL, NULL, 0, {});
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildDistinctInstances:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildDistinctInstances(
          ser, NULL, NULL, NULL, NULL, NULL, {});
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildMultipleDistinctInstances:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildMultipleDistinctInstances(
          ser, NULL, {}, NULL, NULL, {});
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildRepeatedDistinctInstances:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildRepeatedDistinctInstances(
          ser, NULL, {}, NULL, NULL, 0, {});
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildNonOpaqueTriangle:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildTriangle(
          ser, NULL, NULL, 0, 0, NULL, 0, false, true);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildOpaqueTriangle:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildTriangle(
          ser, NULL, NULL, 0, 0, NULL, 0, false, false, true);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndexedTriangle:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildIndexedTriangle(
          ser, NULL, NULL, NULL, MTL::IndexTypeUInt16, 0, NULL);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndexedOpaqueTriangle:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildIndexedTriangle(
          ser, NULL, NULL, NULL, MTL::IndexTypeUInt16, 0, NULL, true);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildBoundingBox:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildBoundingBox(
          ser, NULL, NULL, 0, NULL);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_copyAccelerationStructure:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_copyAccelerationStructure(
          ser, NULL, NULL);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_copyAndCompactAccelerationStructure:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_copyAndCompactAccelerationStructure(
          ser, NULL, NULL, NULL, 0, MTL::DataTypeULong, 0);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_writeCompactedAccelerationStructureSize:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_writeCompactedSize(
          ser, NULL, NULL, 0, MTL::DataTypeULong);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_endEncoding:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_endEncoding(ser);
    case MetalChunk::MTLComputeCommandEncoder_setAccelerationStructure:
      return m_DummyReplayComputeCommandEncoder->Serialise_setAccelerationStructure(
          ser, NULL, 0);
    case MetalChunk::MTLCommandBuffer_pushDebugGroup:
      return m_DummyReplayCommandBuffer->Serialise_pushDebugGroup(ser, NULL);
    case MetalChunk::MTLCommandBuffer_popDebugGroup:
      return m_DummyReplayCommandBuffer->Serialise_popDebugGroup(ser);

    case MetalChunk::MTLTexture_setPurgeableState:
      return m_DummyReplayTexture->Serialise_setPurgeableState(ser, MTL::PurgeableStateKeepCurrent);
    case MetalChunk::MTLTexture_makeAliasable:
      return m_DummyReplayTexture->Serialise_makeAliasable(ser);
    case MetalChunk::MTLTexture_getBytes:
    {
      MTL::Region region = {};
      return m_DummyReplayTexture->Serialise_getBytes(ser, NULL, 0, region, 0);
    }
    case MetalChunk::MTLTexture_getBytes_slice:
    {
      MTL::Region region = {};
      return m_DummyReplayTexture->Serialise_getBytes(ser, NULL, 0, 0, region, 0, 0);
    }
    case MetalChunk::MTLTexture_replaceRegion:
    {
      MTL::Region region = {};
      return m_DummyReplayTexture->Serialise_replaceRegion(ser, region, 0, NULL, 0);
    }
    case MetalChunk::MTLTexture_replaceRegion_slice:
    {
      MTL::Region region = {};
      return m_DummyReplayTexture->Serialise_replaceRegion(ser, region, 0, 0, NULL, 0, 0);
    }
    case MetalChunk::MTLTexture_newTextureViewWithPixelFormat:
    case MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset:
    case MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset_swizzle:
    {
      MTL::TextureSwizzleChannels identity = {MTL::TextureSwizzleRed, MTL::TextureSwizzleGreen,
                                              MTL::TextureSwizzleBlue, MTL::TextureSwizzleAlpha};
      const uint32_t variant = chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat ? 0 :
                               chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset ? 1 : 2;
      return m_DummyReplayTexture->Serialise_newTextureView(
          ser, NULL, MTL::PixelFormatInvalid, MTL::TextureType2D, NS::Range::Make(0, 0),
          NS::Range::Make(0, 0), identity, variant);
    }
    case MetalChunk::MTLTexture_newSharedTextureHandle:
      return m_DummyReplayTexture->Serialise_newSharedTextureHandle(ser);
    case MetalChunk::MTLTexture_remoteStorageTexture: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLTexture_newRemoteTextureViewForDevice: METAL_CHUNK_NOT_HANDLED();

    case MetalChunk::MTLRenderPipelineState_functionHandleWithFunction:
      return m_DummyReplayRenderPipelineState->Serialise_functionHandle(
          ser, NULL, NULL, MTL::RenderStageFragment);
    case MetalChunk::MTLRenderPipelineState_newVisibleFunctionTableWithDescriptor:
      return m_DummyReplayRenderPipelineState->Serialise_newVisibleFunctionTable(
          ser, NULL, 0, MTL::RenderStageFragment);
    case MetalChunk::MTLVisibleFunctionTable_setFunction:
      return m_DummyReplayVisibleFunctionTable->Serialise_setFunction(ser, NULL, 0);
    case MetalChunk::MTLIntersectionFunctionTable_setFunction:
      return m_DummyReplayIntersectionFunctionTable->Serialise_setFunction(ser, NULL, 0);
    case MetalChunk::MTLIntersectionFunctionTable_setOpaqueTriangleFunction:
      return m_DummyReplayIntersectionFunctionTable->Serialise_setOpaqueTriangleFunction(
          ser, MTL::IntersectionFunctionSignatureNone, 0);
    case MetalChunk::MTLIntersectionFunctionTable_setOpaqueTriangleFunctions:
      return m_DummyReplayIntersectionFunctionTable->Serialise_setOpaqueTriangleFunctions(
          ser, MTL::IntersectionFunctionSignatureNone, NS::Range::Make(0, 0));
    case MetalChunk::MTLIntersectionFunctionTable_setBuffer:
      return m_DummyReplayIntersectionFunctionTable->Serialise_setBuffer(ser, NULL, 0, 0);
    case MetalChunk::MTLIntersectionFunctionTable_setBuffers:
      return m_DummyReplayIntersectionFunctionTable->Serialise_setBuffers(
          ser, {}, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLIntersectionFunctionTable_setVisibleFunctionTable:
      return m_DummyReplayIntersectionFunctionTable->Serialise_setVisibleFunctionTable(
          ser, NULL, 0);
    case MetalChunk::MTLIntersectionFunctionTable_setVisibleFunctionTables:
      return m_DummyReplayIntersectionFunctionTable->Serialise_setVisibleFunctionTables(
          ser, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLArgumentEncoder_setVisibleFunctionTable:
      return m_DummyReplayArgumentEncoder->Serialise_setVisibleFunctionTable(ser, NULL, 0);
    case MetalChunk::MTLComputePipelineState_functionHandleWithFunction:
      return m_DummyReplayComputePipelineState->Serialise_functionHandle(ser, NULL, NULL);
    case MetalChunk::MTLComputePipelineState_newVisibleFunctionTableWithDescriptor:
      return m_DummyReplayComputePipelineState->Serialise_newVisibleFunctionTable(ser, NULL, 0);
    case MetalChunk::MTLComputePipelineState_newIntersectionFunctionTableWithDescriptor:
      return m_DummyReplayComputePipelineState->Serialise_newIntersectionFunctionTable(ser, NULL, 0);
    case MetalChunk::MTLComputeCommandEncoder_setVisibleFunctionTable:
      return m_DummyReplayComputeCommandEncoder->Serialise_setVisibleFunctionTable(ser, NULL, 0);
    case MetalChunk::MTLComputeCommandEncoder_setVisibleFunctionTables:
      return m_DummyReplayComputeCommandEncoder->Serialise_setVisibleFunctionTables(
          ser, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLComputeCommandEncoder_setIntersectionFunctionTable:
      return m_DummyReplayComputeCommandEncoder->Serialise_setIntersectionFunctionTable(ser, NULL, 0);
    case MetalChunk::MTLComputeCommandEncoder_setIntersectionFunctionTables:
      return m_DummyReplayComputeCommandEncoder->Serialise_setIntersectionFunctionTables(
          ser, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildBoundingBoxTableOffset:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildBoundingBoxTableOffset(
          ser, NULL, NULL, 0, 0, 0, 0, NULL, 0);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildBoundingBoxOpaque:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildBoundingBoxOpaque(
          ser, NULL, NULL, 0, 0, 0, 0, NULL, 0);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndexedTriangleOffset:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildIndexedTriangleOffset(
          ser, NULL, NULL, NULL, MTL::IndexTypeUInt16, 0, 0, NULL, false);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndexedTriangleExtended:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildIndexedTriangleExtended(
          ser, NULL, NULL, 0, NULL, MTL::IndexTypeUInt16, 0, 0, NULL, 0, false);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildTriangleTableOffset:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildTriangleTableOffset(
          ser, NULL, NULL, 0, NULL, MTL::IndexTypeUInt16, 0, 0, NULL, 0, 0, false);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildBoundingBoxNoDuplicate:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildBoundingBoxNoDuplicate(
          ser, NULL, NULL, 0, 0, 0, 0, NULL, 0, false);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildTriangleNoDuplicate:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildTriangleNoDuplicate(
          ser, NULL, NULL, 0, NULL, MTL::IndexTypeUInt16, 0, 0, NULL, 0, 0, false);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildRefittableTriangleNoDuplicate:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildRefittableTriangleNoDuplicate(
          ser, NULL, NULL, 0, NULL);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_refitTriangleNoDuplicate:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_refitTriangleNoDuplicate(
          ser, NULL, NULL, NULL, 0, NULL, 0);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildFormattedTriangle:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildFormattedTriangle(
          ser, NULL, NULL, 0, 0, MTL::AttributeFormatFloat3, 0, NULL, 0, 0, false, true);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndexedFormattedTriangle:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildIndexedFormattedTriangle(
          ser, NULL, NULL, 0, 0, MTL::AttributeFormatFloat3, NULL,
          MTL::IndexTypeUInt16, 0, 0, NULL, 0, 0, false, true);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildRefittableFormattedTriangle:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildRefittableFormattedTriangle(
          ser, NULL, NULL, 0, MTL::AttributeFormatFloat3, 0, NULL, true);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_refitFormattedTriangle:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_refitFormattedTriangle(
          ser, NULL, NULL, NULL, 0, MTL::AttributeFormatFloat3, 0, NULL, 0, true);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildRefittableBoundingBox:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildRefittableBoundingBox(
          ser, NULL, NULL, 0, 0, 0, 0, NULL, 0, false, true);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_refitBoundingBox:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_refitBoundingBox(
          ser, NULL, NULL, NULL, 0, 0, 0, 0, NULL, 0, false, true);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_buildRefittableIndexedTriangle:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_buildRefittableIndexedTriangle(
          ser, NULL, NULL, 0, 0, MTL::AttributeFormatFloat3, NULL, MTL::IndexTypeUInt16,
          0, 0, 0, NULL, 0, false, true);
    case MetalChunk::MTLAccelerationStructureCommandEncoder_refitIndexedTriangle:
      return m_DummyReplayAccelerationStructureCommandEncoder->Serialise_refitIndexedTriangle(
          ser, NULL, NULL, NULL, 0, 0, MTL::AttributeFormatFloat3, NULL,
          MTL::IndexTypeUInt16, 0, 0, 0, NULL, 0, false, true);
    case MetalChunk::MTLRenderPipelineState_newIntersectionFunctionTableWithDescriptor:
      return m_DummyReplayRenderPipelineState->Serialise_newIntersectionFunctionTable(
          ser, NULL, 0, MTL::RenderStageFragment);
    case MetalChunk::MTLRenderPipelineState_newRenderPipelineStateWithAdditionalBinaryFunctions:
      METAL_CHUNK_NOT_HANDLED();

    case MetalChunk::MTLRenderCommandEncoder_endEncoding:
      return m_DummyReplayRenderCommandEncoder->Serialise_endEncoding(ser);
    case MetalChunk::MTLRenderCommandEncoder_insertDebugSignpost:
      return m_DummyReplayRenderCommandEncoder->Serialise_insertDebugSignpost(ser, NULL);
    case MetalChunk::MTLRenderCommandEncoder_pushDebugGroup:
      return m_DummyReplayRenderCommandEncoder->Serialise_pushDebugGroup(ser, NULL);
    case MetalChunk::MTLRenderCommandEncoder_popDebugGroup:
      return m_DummyReplayRenderCommandEncoder->Serialise_popDebugGroup(ser);
    case MetalChunk::MTLRenderCommandEncoder_setRenderPipelineState:
      return m_DummyReplayRenderCommandEncoder->Serialise_setRenderPipelineState(ser, NULL);
    case MetalChunk::MTLRenderCommandEncoder_setVertexBytes:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexBytes(ser, {}, 0);
    case MetalChunk::MTLRenderCommandEncoder_setVertexBuffer:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexBuffer(ser, NULL, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_setVertexBufferOffset:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexBufferOffset(ser, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_setVertexBuffers:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexBuffers(
          ser, {}, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setVertexBuffer_stride:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexBindingWithStride(
          ser, {}, {}, {}, {}, NS::Range::Make(0, 0), 0);
    case MetalChunk::MTLRenderCommandEncoder_setVertexBuffers_strides:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexBindingWithStride(
          ser, {}, {}, {}, {}, NS::Range::Make(0, 0), 1);
    case MetalChunk::MTLRenderCommandEncoder_setVertexBufferOffset_stride:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexBindingWithStride(
          ser, {}, {}, {}, {}, NS::Range::Make(0, 0), 2);
    case MetalChunk::MTLRenderCommandEncoder_setVertexBytes_stride:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexBindingWithStride(
          ser, {}, {}, {}, {}, NS::Range::Make(0, 0), 3);
    case MetalChunk::MTLRenderCommandEncoder_setVertexTexture:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexTexture(ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setVertexTextures:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexTextures(ser, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setVertexSamplerState:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexSamplerState(ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setVertexSamplerState_lodclamp:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexSamplerStateWithLOD(
          ser, NULL, 0.0f, 0.0f, 0);
    case MetalChunk::MTLRenderCommandEncoder_setVertexSamplerStates:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexSamplerStates(ser, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setVertexSamplerStates_lodclamp:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexSamplerStatesWithLOD(
          ser, {}, {}, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setVertexVisibleFunctionTable:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexVisibleFunctionTable(
          ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setVertexVisibleFunctionTables:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexVisibleFunctionTables(
          ser, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setVertexIntersectionFunctionTable:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexIntersectionFunctionTable(
          ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setVertexIntersectionFunctionTables:
      return m_DummyReplayRenderCommandEncoder->Serialise_setIntersectionFunctionTables(
          ser, {}, NS::Range::Make(0, 0), MTL::RenderStageVertex);
    case MetalChunk::MTLRenderCommandEncoder_setVertexAccelerationStructure:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexAccelerationStructure(
          ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setViewport:
    {
      MTL::Viewport viewport;
      return m_DummyReplayRenderCommandEncoder->Serialise_setViewport(ser, viewport);
    }
    case MetalChunk::MTLRenderCommandEncoder_setViewports:
      return m_DummyReplayRenderCommandEncoder->Serialise_setViewports(ser, {});
    case MetalChunk::MTLRenderCommandEncoder_setFrontFacingWinding:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFrontFacingWinding(
          ser, MTL::WindingClockwise);
    case MetalChunk::MTLRenderCommandEncoder_setVertexAmplificationCount:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVertexAmplificationCount(
          ser, 0, {}, {}, false);
    case MetalChunk::MTLRenderCommandEncoder_setCullMode:
      return m_DummyReplayRenderCommandEncoder->Serialise_setCullMode(ser, MTL::CullModeNone);
    case MetalChunk::MTLRenderCommandEncoder_setDepthClipMode:
      return m_DummyReplayRenderCommandEncoder->Serialise_setDepthClipMode(
          ser, MTL::DepthClipModeClip);
    case MetalChunk::MTLRenderCommandEncoder_setDepthBias:
      return m_DummyReplayRenderCommandEncoder->Serialise_setDepthBias(ser, 0.0f, 0.0f, 0.0f);
    case MetalChunk::MTLRenderCommandEncoder_setScissorRect:
    {
      MTL::ScissorRect rect = {};
      return m_DummyReplayRenderCommandEncoder->Serialise_setScissorRect(ser, rect);
    }
    case MetalChunk::MTLRenderCommandEncoder_setScissorRects:
      return m_DummyReplayRenderCommandEncoder->Serialise_setScissorRects(ser, {});
    case MetalChunk::MTLRenderCommandEncoder_setTriangleFillMode:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTriangleFillMode(
          ser, MTL::TriangleFillModeFill);
    case MetalChunk::MTLRenderCommandEncoder_setFragmentBytes:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentBytes(ser, {}, 0);
    case MetalChunk::MTLRenderCommandEncoder_setFragmentBuffer:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentBuffer(ser, NULL, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_setFragmentBufferOffset:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentBufferOffset(ser, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_setFragmentBuffers:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentBuffers(
          ser, {}, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setFragmentTexture:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentTexture(ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setFragmentTextures:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentTextures(ser, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerState:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentSamplerState(ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerState_lodclamp:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentSamplerStateWithLOD(
          ser, NULL, 0.0f, 0.0f, 0);
    case MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerStates:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentSamplerStates(ser, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerStates_lodclamp:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentSamplerStatesWithLOD(
          ser, {}, {}, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setFragmentVisibleFunctionTable:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentVisibleFunctionTable(
          ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setFragmentVisibleFunctionTables:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentVisibleFunctionTables(
          ser, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setFragmentIntersectionFunctionTable:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentIntersectionFunctionTable(
          ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setFragmentIntersectionFunctionTables:
      return m_DummyReplayRenderCommandEncoder->Serialise_setIntersectionFunctionTables(
          ser, {}, NS::Range::Make(0, 0), MTL::RenderStageFragment);
    case MetalChunk::MTLRenderCommandEncoder_setFragmentAccelerationStructure:
      return m_DummyReplayRenderCommandEncoder->Serialise_setFragmentAccelerationStructure(
          ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setBlendColor:
      return m_DummyReplayRenderCommandEncoder->Serialise_setBlendColor(
          ser, 0.0f, 0.0f, 0.0f, 0.0f);
    case MetalChunk::MTLRenderCommandEncoder_setDepthStencilState:
      return m_DummyReplayRenderCommandEncoder->Serialise_setDepthStencilState(ser, NULL);
    case MetalChunk::MTLRenderCommandEncoder_setStencilReferenceValue:
      return m_DummyReplayRenderCommandEncoder->Serialise_setStencilReferenceValue(ser, 0);
    case MetalChunk::MTLRenderCommandEncoder_setStencilFrontReferenceValue:
      return m_DummyReplayRenderCommandEncoder->Serialise_setStencilReferenceValues(ser, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_setVisibilityResultMode:
      return m_DummyReplayRenderCommandEncoder->Serialise_setVisibilityResultMode(
          ser, MTL::VisibilityResultModeDisabled, 0);
    case MetalChunk::MTLRenderCommandEncoder_setColorStoreAction:
      return m_DummyReplayRenderCommandEncoder->Serialise_setColorStoreAction(
          ser, MTL::StoreActionStore, 0);
    case MetalChunk::MTLRenderCommandEncoder_setDepthStoreAction:
      return m_DummyReplayRenderCommandEncoder->Serialise_setDepthStoreAction(
          ser, MTL::StoreActionStore);
    case MetalChunk::MTLRenderCommandEncoder_setStencilStoreAction:
      return m_DummyReplayRenderCommandEncoder->Serialise_setStencilStoreAction(
          ser, MTL::StoreActionStore);
    case MetalChunk::MTLRenderCommandEncoder_setColorStoreActionOptions:
      return m_DummyReplayRenderCommandEncoder->Serialise_setColorStoreActionOptions(
          ser, MTL::StoreActionOptionNone, 0);
    case MetalChunk::MTLRenderCommandEncoder_setDepthStoreActionOptions:
      return m_DummyReplayRenderCommandEncoder->Serialise_setDepthStoreActionOptions(
          ser, MTL::StoreActionOptionNone);
    case MetalChunk::MTLRenderCommandEncoder_setStencilStoreActionOptions:
      return m_DummyReplayRenderCommandEncoder->Serialise_setStencilStoreActionOptions(
          ser, MTL::StoreActionOptionNone);
    case MetalChunk::MTLRenderCommandEncoder_drawPrimitives:
    case MetalChunk::MTLRenderCommandEncoder_drawPrimitives_instanced:
    case MetalChunk::MTLRenderCommandEncoder_drawPrimitives_instanced_base:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawPrimitives(
          ser, MTL::PrimitiveTypePoint, 0, 0, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_drawPrimitives_indirect:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawPrimitives(
          ser, MTL::PrimitiveTypePoint, (WrappedMTLBuffer *)NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawIndexedPrimitives(
          ser, MTL::PrimitiveTypePoint, 0, MTL::IndexTypeUInt16, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced:
    case MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced_base:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawIndexedPrimitives(
          ser, MTL::PrimitiveTypePoint, 0, MTL::IndexTypeUInt16, NULL, 0, 0, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_indirect:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawIndexedPrimitives(
          ser, MTL::PrimitiveTypeTriangle, MTL::IndexTypeUInt16, NULL, 0, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_textureBarrier:
      return m_DummyReplayRenderCommandEncoder->Serialise_textureBarrier(ser);
    case MetalChunk::MTLRenderCommandEncoder_updateFence:
      return m_DummyReplayRenderCommandEncoder->Serialise_updateFence(ser, NULL, MTL::RenderStageVertex);
    case MetalChunk::MTLRenderCommandEncoder_waitForFence:
      return m_DummyReplayRenderCommandEncoder->Serialise_waitForFence(ser, NULL, MTL::RenderStageVertex);
    case MetalChunk::MTLRenderCommandEncoder_setTessellationFactorBuffer:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTessellationFactorBuffer(ser, NULL, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_setTessellationFactorScale:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTessellationFactorScale(ser, 1.0f);
    case MetalChunk::MTLRenderCommandEncoder_drawPatches:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawPatches(ser, 0, 0, 0, NULL, 0, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_drawPatches_indirect:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawPatchesIndirect(ser, 0, NULL, 0, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_drawIndexedPatches:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawIndexedPatches(ser, 0, 0, 0, NULL, 0,
                                                                              NULL, 0, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_drawIndexedPatches_indirect:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawIndexedPatchesIndirect(
          ser, 0, NULL, 0, NULL, 0, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setTileBytes:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileBinding(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 0);
    case MetalChunk::MTLRenderCommandEncoder_setTileBuffer:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileBuffer(ser, NULL, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_setTileBufferOffset:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileBinding(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 1);
    case MetalChunk::MTLRenderCommandEncoder_setTileBuffers:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileBinding(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 2);
    case MetalChunk::MTLRenderCommandEncoder_setTileTexture:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileTextures(
          ser, {}, NS::Range::Make(0, 0), 0);
    case MetalChunk::MTLRenderCommandEncoder_setTileTextures:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileTextures(
          ser, {}, NS::Range::Make(0, 0), 1);
    case MetalChunk::MTLRenderCommandEncoder_setTileSamplerState:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileSamplers(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 0);
    case MetalChunk::MTLRenderCommandEncoder_setTileSamplerState_lodclamp:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileSamplers(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 2);
    case MetalChunk::MTLRenderCommandEncoder_setTileSamplerStates:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileSamplers(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 1);
    case MetalChunk::MTLRenderCommandEncoder_setTileSamplerStates_lodclamp:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileSamplers(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 3);
    case MetalChunk::MTLRenderCommandEncoder_setTileVisibleFunctionTable:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileVisibleFunctionTable(ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setTileVisibleFunctionTables:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileVisibleFunctionTables(
          ser, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setTileIntersectionFunctionTable:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileIntersectionFunctionTable(
          ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_setTileIntersectionFunctionTables:
      return m_DummyReplayRenderCommandEncoder->Serialise_setIntersectionFunctionTables(
          ser, {}, NS::Range::Make(0, 0), MTL::RenderStageTile);
    case MetalChunk::MTLRenderCommandEncoder_setTileAccelerationStructure:
      return m_DummyReplayRenderCommandEncoder->Serialise_setTileAccelerationStructure(
          ser, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_dispatchThreadsPerTile:
    {
      MTL::Size threads = {};
      return m_DummyReplayRenderCommandEncoder->Serialise_dispatchThreadsPerTile(ser, threads);
    }
    case MetalChunk::MTLRenderCommandEncoder_setThreadgroupMemoryLength:
      return m_DummyReplayRenderCommandEncoder->Serialise_setThreadgroupMemoryLength(ser, 0, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_drawMeshThreadgroups:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawMeshThreadgroups(
          ser, MTL::Size::Make(0, 0, 0), MTL::Size::Make(0, 0, 0),
          MTL::Size::Make(0, 0, 0));
    case MetalChunk::MTLRenderCommandEncoder_drawMeshThreadgroups_indirect:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawMeshThreadgroups(
          ser, (WrappedMTLBuffer *)NULL, 0, MTL::Size::Make(0, 0, 0),
          MTL::Size::Make(0, 0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setMeshBuffer:
      return m_DummyReplayRenderCommandEncoder->Serialise_setMeshBinding(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 0);
    case MetalChunk::MTLRenderCommandEncoder_setMeshBytes:
      return m_DummyReplayRenderCommandEncoder->Serialise_setMeshBinding(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 1);
    case MetalChunk::MTLRenderCommandEncoder_setMeshBufferOffset:
      return m_DummyReplayRenderCommandEncoder->Serialise_setMeshBinding(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 2);
    case MetalChunk::MTLRenderCommandEncoder_setMeshBuffers:
      return m_DummyReplayRenderCommandEncoder->Serialise_setMeshBinding(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 3);
    case MetalChunk::MTLRenderCommandEncoder_setMeshTexture:
      return m_DummyReplayRenderCommandEncoder->Serialise_setMeshTextures(
          ser, {}, NS::Range::Make(0, 0), 0);
    case MetalChunk::MTLRenderCommandEncoder_setMeshTextures:
      return m_DummyReplayRenderCommandEncoder->Serialise_setMeshTextures(
          ser, {}, NS::Range::Make(0, 0), 1);
    case MetalChunk::MTLRenderCommandEncoder_setMeshSamplerState:
      return m_DummyReplayRenderCommandEncoder->Serialise_setMeshSamplers(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 0);
    case MetalChunk::MTLRenderCommandEncoder_setMeshSamplerStates:
      return m_DummyReplayRenderCommandEncoder->Serialise_setMeshSamplers(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 1);
    case MetalChunk::MTLRenderCommandEncoder_setMeshSamplerState_lodclamp:
      return m_DummyReplayRenderCommandEncoder->Serialise_setMeshSamplers(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 2);
    case MetalChunk::MTLRenderCommandEncoder_setMeshSamplerStates_lodclamp:
      return m_DummyReplayRenderCommandEncoder->Serialise_setMeshSamplers(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 3);
    case MetalChunk::MTLRenderCommandEncoder_drawMeshThreads:
      return m_DummyReplayRenderCommandEncoder->Serialise_drawMeshThreads(
          ser, MTL::Size::Make(0, 0, 0), MTL::Size::Make(0, 0, 0),
          MTL::Size::Make(0, 0, 0));
    case MetalChunk::MTLRenderCommandEncoder_setObjectBuffer:
      return m_DummyReplayRenderCommandEncoder->Serialise_setObjectBuffer(ser, NULL, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_setObjectBytes:
      return m_DummyReplayRenderCommandEncoder->Serialise_setObjectBinding(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 0);
    case MetalChunk::MTLRenderCommandEncoder_setObjectBufferOffset:
      return m_DummyReplayRenderCommandEncoder->Serialise_setObjectBinding(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 1);
    case MetalChunk::MTLRenderCommandEncoder_setObjectBuffers:
      return m_DummyReplayRenderCommandEncoder->Serialise_setObjectBinding(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 2);
    case MetalChunk::MTLRenderCommandEncoder_setObjectTexture:
      return m_DummyReplayRenderCommandEncoder->Serialise_setObjectTextures(
          ser, {}, NS::Range::Make(0, 0), 0);
    case MetalChunk::MTLRenderCommandEncoder_setObjectTextures:
      return m_DummyReplayRenderCommandEncoder->Serialise_setObjectTextures(
          ser, {}, NS::Range::Make(0, 0), 1);
    case MetalChunk::MTLRenderCommandEncoder_setObjectSamplerState:
      return m_DummyReplayRenderCommandEncoder->Serialise_setObjectSamplers(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 0);
    case MetalChunk::MTLRenderCommandEncoder_setObjectSamplerStates:
      return m_DummyReplayRenderCommandEncoder->Serialise_setObjectSamplers(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 1);
    case MetalChunk::MTLRenderCommandEncoder_setObjectSamplerState_lodclamp:
      return m_DummyReplayRenderCommandEncoder->Serialise_setObjectSamplers(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 2);
    case MetalChunk::MTLRenderCommandEncoder_setObjectSamplerStates_lodclamp:
      return m_DummyReplayRenderCommandEncoder->Serialise_setObjectSamplers(
          ser, {}, {}, {}, NS::Range::Make(0, 0), 3);
    case MetalChunk::MTLRenderCommandEncoder_setObjectThreadgroupMemoryLength:
      return m_DummyReplayRenderCommandEncoder->Serialise_setObjectThreadgroupMemoryLength(
          ser, 0, 0);
    case MetalChunk::MTLRenderCommandEncoder_useResource:
      return m_DummyReplayRenderCommandEncoder->Serialise_useResource(
          ser, NULL, MTL::ResourceUsageRead);
    case MetalChunk::MTLRenderCommandEncoder_useResource_stages:
      return m_DummyReplayRenderCommandEncoder->Serialise_useResourceWithStages(
          ser, NULL, MTL::ResourceUsageRead, MTL::RenderStageVertex);
    case MetalChunk::MTLRenderCommandEncoder_useResources:
      return m_DummyReplayRenderCommandEncoder->Serialise_useResources(ser, {}, MTL::ResourceUsageRead);
    case MetalChunk::MTLRenderCommandEncoder_useResources_stages:
      return m_DummyReplayRenderCommandEncoder->Serialise_useResourcesWithStages(
          ser, {}, MTL::ResourceUsageRead, MTL::RenderStageVertex);
    case MetalChunk::MTLRenderCommandEncoder_useHeap:
      return m_DummyReplayRenderCommandEncoder->Serialise_declareHeaps(
          ser, {}, MTL::RenderStageVertex, 0);
    case MetalChunk::MTLRenderCommandEncoder_useHeap_stages:
      return m_DummyReplayRenderCommandEncoder->Serialise_declareHeaps(
          ser, {}, MTL::RenderStageVertex, 1);
    case MetalChunk::MTLRenderCommandEncoder_useHeaps:
      return m_DummyReplayRenderCommandEncoder->Serialise_declareHeaps(
          ser, {}, MTL::RenderStageVertex, 2);
    case MetalChunk::MTLRenderCommandEncoder_useHeaps_stages:
      return m_DummyReplayRenderCommandEncoder->Serialise_declareHeaps(
          ser, {}, MTL::RenderStageVertex, 3);
    case MetalChunk::MTLRenderCommandEncoder_executeCommandsInBuffer:
      return m_DummyReplayRenderCommandEncoder->Serialise_executeCommandsInBuffer(
          ser, NULL, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_executeCommandsMarker:
      return m_DummyReplayRenderCommandEncoder->Serialise_executeCommandsMarker(
          ser, NULL, NS::Range::Make(0, 0));
    case MetalChunk::MTLRenderCommandEncoder_executeCommandsInBuffer_indirect:
      return m_DummyReplayRenderCommandEncoder->Serialise_executeCommandsInBufferIndirect(
          ser, NULL, NULL, 0);
    case MetalChunk::MTLRenderCommandEncoder_memoryBarrierWithScope:
      return m_DummyReplayRenderCommandEncoder->Serialise_memoryBarrierWithScope(
          ser, MTL::BarrierScopeBuffers, MTL::RenderStageVertex, MTL::RenderStageVertex);
    case MetalChunk::MTLRenderCommandEncoder_memoryBarrierWithResources:
      return m_DummyReplayRenderCommandEncoder->Serialise_memoryBarrierWithResources(
          ser, {}, MTL::RenderStageVertex, MTL::RenderStageVertex);
    case MetalChunk::MTLRenderCommandEncoder_sampleCountersInBuffer: METAL_CHUNK_NOT_HANDLED();

    case MetalChunk::MTLBuffer_setPurgeableState:
      return m_DummyBuffer->Serialise_setPurgeableState(ser, MTL::PurgeableStateKeepCurrent);
    case MetalChunk::MTLBuffer_makeAliasable:
      return m_DummyBuffer->Serialise_makeAliasable(ser);
    case MetalChunk::MTLBuffer_contents: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBuffer_didModifyRange:
    {
      NS::Range range = NS::Range::Make(0, 0);
      return m_DummyBuffer->Serialise_didModifyRange(ser, range);
    }
    case MetalChunk::MTLBuffer_newTextureWithDescriptor:
    {
      RDMTL::TextureDescriptor descriptor;
      return m_DummyBuffer->Serialise_newTextureWithDescriptor(ser, NULL, descriptor, 0, 0);
    }
    case MetalChunk::MTLBuffer_addDebugMarker:
      return m_DummyBuffer->Serialise_addDebugMarker(ser, NULL, NS::Range::Make(0, 0));
    case MetalChunk::MTLBuffer_removeAllDebugMarkers:
      return m_DummyBuffer->Serialise_removeAllDebugMarkers(ser);
    case MetalChunk::MTLBuffer_remoteStorageBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBuffer_newRemoteBufferViewForDevice: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBuffer_InternalModifyCPUContents:
      return m_DummyBuffer->Serialise_InternalModifyCPUContents(ser, 0, 0, NULL);

    case MetalChunk::MTLHeap_newBuffer:
      return m_DummyReplayHeap->Serialise_newBuffer(
          ser, NULL, 0, MTL::ResourceStorageModePrivate);
    case MetalChunk::MTLHeap_newBufferWithOffset:
      return m_DummyReplayHeap->Serialise_newBufferWithOffset(
          ser, NULL, 0, MTL::ResourceStorageModePrivate, 0);
    case MetalChunk::MTLHeap_newTexture:
    {
      RDMTL::TextureDescriptor descriptor;
      return m_DummyReplayHeap->Serialise_newTexture(ser, NULL, descriptor);
    }
    case MetalChunk::MTLHeap_newTextureWithOffset:
    {
      RDMTL::TextureDescriptor descriptor;
      return m_DummyReplayHeap->Serialise_newTextureWithOffset(ser, NULL, descriptor, 0);
    }

    case MetalChunk::MTLBlitCommandEncoder_setLabel:
      return m_DummyReplayBlitCommandEncoder->Serialise_setLabel(ser, NULL);
    case MetalChunk::MTLBlitCommandEncoder_endEncoding:
      return m_DummyReplayBlitCommandEncoder->Serialise_endEncoding(ser);
    case MetalChunk::MTLBlitCommandEncoder_insertDebugSignpost:
      return m_DummyReplayBlitCommandEncoder->Serialise_insertDebugSignpost(ser, NULL);
    case MetalChunk::MTLBlitCommandEncoder_pushDebugGroup:
      return m_DummyReplayBlitCommandEncoder->Serialise_pushDebugGroup(ser, NULL);
    case MetalChunk::MTLBlitCommandEncoder_popDebugGroup:
      return m_DummyReplayBlitCommandEncoder->Serialise_popDebugGroup(ser);
    case MetalChunk::MTLBlitCommandEncoder_synchronizeResource:
      return m_DummyReplayBlitCommandEncoder->Serialise_synchronizeResource(ser, NULL);
    case MetalChunk::MTLBlitCommandEncoder_synchronizeTexture:
      return m_DummyReplayBlitCommandEncoder->Serialise_synchronizeTexture(ser, NULL, 0, 0);
    case MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toBuffer:
      return m_DummyReplayBlitCommandEncoder->Serialise_copyFromBuffer(ser, NULL, 0, NULL, 0, 0);
    case MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toTexture:
    case MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toTexture_options:
    {
      MTL::Origin origin = {};
      MTL::Size size = {};
      return m_DummyReplayBlitCommandEncoder->Serialise_copyFromBuffer(
          ser, NULL, 0, 0, 0, size, NULL, 0, 0, origin, MTL::BlitOptionNone);
    }
    case MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toBuffer:
    case MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toBuffer_options:
    {
      MTL::Origin origin = {};
      MTL::Size size = {};
      return m_DummyReplayBlitCommandEncoder->Serialise_copyFromTexture(
          ser, NULL, 0, 0, origin, size, NULL, 0, 0, 0, MTL::BlitOptionNone);
    }
    case MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture:
      return m_DummyReplayBlitCommandEncoder->Serialise_copyFromTexture(
          ser, (WrappedMTLTexture *)NULL, (WrappedMTLTexture *)NULL);
    case MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_origin:
    {
      MTL::Origin origin = {};
      MTL::Size size = {};
      return m_DummyReplayBlitCommandEncoder->Serialise_copyFromTexture(
          ser, NULL, 0, 0, origin, size, NULL, 0, 0, origin);
    }
    case MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_count:
      return m_DummyReplayBlitCommandEncoder->Serialise_copyFromTexture(
          ser, NULL, 0, 0, NULL, 0, 0, 0, 0);
    case MetalChunk::MTLBlitCommandEncoder_generateMipmapsForTexture:
      return m_DummyReplayBlitCommandEncoder->Serialise_generateMipmapsForTexture(ser, NULL);
    case MetalChunk::MTLBlitCommandEncoder_fillBuffer:
    {
      NS::Range range = NS::Range::Make(0, 0);
      return m_DummyReplayBlitCommandEncoder->Serialise_fillBuffer(ser, NULL, range, 0);
    }
    case MetalChunk::MTLBlitCommandEncoder_updateFence:
      return m_DummyReplayBlitCommandEncoder->Serialise_updateFence(ser, NULL);
    case MetalChunk::MTLBlitCommandEncoder_waitForFence:
      return m_DummyReplayBlitCommandEncoder->Serialise_waitForFence(ser, NULL);
    case MetalChunk::MTLComputeCommandEncoder_updateFence:
      return m_DummyReplayComputeCommandEncoder->Serialise_updateFence(ser, NULL);
    case MetalChunk::MTLComputeCommandEncoder_waitForFence:
      return m_DummyReplayComputeCommandEncoder->Serialise_waitForFence(ser, NULL);
    case MetalChunk::MTLBlitCommandEncoder_getTextureAccessCounters: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_resetTextureAccessCounters: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_optimizeContentsForGPUAccess:
      return m_DummyReplayBlitCommandEncoder->Serialise_optimizeContentsForGPUAccess(ser, NULL);
    case MetalChunk::MTLBlitCommandEncoder_optimizeContentsForGPUAccess_slice_level:
      return m_DummyReplayBlitCommandEncoder->Serialise_optimizeContentsForGPUAccess(ser, NULL, 0, 0);
    case MetalChunk::MTLBlitCommandEncoder_optimizeContentsForCPUAccess:
      return m_DummyReplayBlitCommandEncoder->Serialise_optimizeContentsForCPUAccess(ser, NULL);
    case MetalChunk::MTLBlitCommandEncoder_optimizeContentsForCPUAccess_slice_level:
      return m_DummyReplayBlitCommandEncoder->Serialise_optimizeContentsForCPUAccess(ser, NULL, 0, 0);
    case MetalChunk::MTLBlitCommandEncoder_resetCommandsInBuffer:
    {
      NS::Range range = NS::Range::Make(0, 0);
      return m_DummyReplayBlitCommandEncoder->Serialise_resetCommandsInBuffer(ser, NULL, range);
    }
    case MetalChunk::MTLBlitCommandEncoder_copyIndirectCommandBuffer:
    {
      NS::Range range = NS::Range::Make(0, 0);
      return m_DummyReplayBlitCommandEncoder->Serialise_copyIndirectCommandBuffer(
          ser, NULL, range, NULL, 0);
    }
    case MetalChunk::MTLBlitCommandEncoder_optimizeIndirectCommandBuffer:
    {
      NS::Range range = NS::Range::Make(0, 0);
      return m_DummyReplayBlitCommandEncoder->Serialise_optimizeIndirectCommandBuffer(ser, NULL, range);
    }
    case MetalChunk::MTLBlitCommandEncoder_sampleCountersInBuffer: METAL_CHUNK_NOT_HANDLED();
    case MetalChunk::MTLBlitCommandEncoder_resolveCounters:
    {
      NS::Range range = NS::Range::Make(0, 0);
      return m_DummyReplayBlitCommandEncoder->Serialise_resolveCounters(
          ser, NULL, range, NULL, 0);
    }
    case MetalChunk::MTLComputeCommandEncoder_endEncoding:
      return m_DummyReplayComputeCommandEncoder->Serialise_endEncoding(ser);
    case MetalChunk::MTLComputeCommandEncoder_useResource:
      return m_DummyReplayComputeCommandEncoder->Serialise_useResource(ser, NULL, MTL::ResourceUsageRead);
    case MetalChunk::MTLComputeCommandEncoder_useResources:
      return m_DummyReplayComputeCommandEncoder->Serialise_useResources(ser, {}, MTL::ResourceUsageRead);
    case MetalChunk::MTLComputeCommandEncoder_memoryBarrierWithScope:
      return m_DummyReplayComputeCommandEncoder->Serialise_memoryBarrierWithScope(ser, MTL::BarrierScopeBuffers);
    case MetalChunk::MTLComputeCommandEncoder_memoryBarrierWithResources:
      return m_DummyReplayComputeCommandEncoder->Serialise_memoryBarrierWithResources(ser, {});
    case MetalChunk::MTLComputeCommandEncoder_pushDebugGroup:
      return m_DummyReplayComputeCommandEncoder->Serialise_pushDebugGroup(ser, NULL);
    case MetalChunk::MTLComputeCommandEncoder_insertDebugSignpost:
      return m_DummyReplayComputeCommandEncoder->Serialise_insertDebugSignpost(ser, NULL);
    case MetalChunk::MTLComputeCommandEncoder_popDebugGroup:
      return m_DummyReplayComputeCommandEncoder->Serialise_popDebugGroup(ser);
    case MetalChunk::MTLComputeCommandEncoder_setComputePipelineState:
      return m_DummyReplayComputeCommandEncoder->Serialise_setComputePipelineState(ser, NULL);
    case MetalChunk::MTLComputeCommandEncoder_setTexture:
      return m_DummyReplayComputeCommandEncoder->Serialise_setTexture(ser, NULL, 0);
    case MetalChunk::MTLComputeCommandEncoder_setBuffer:
      return m_DummyReplayComputeCommandEncoder->Serialise_setBuffer(ser, NULL, 0, 0);
    case MetalChunk::MTLComputeCommandEncoder_setBytes:
      return m_DummyReplayComputeCommandEncoder->Serialise_setBytes(ser, {}, 0);
    case MetalChunk::MTLComputeCommandEncoder_setBufferOffset:
      return m_DummyReplayComputeCommandEncoder->Serialise_setBufferOffset(ser, 0, 0);
    case MetalChunk::MTLComputeCommandEncoder_setThreadgroupMemoryLength:
      return m_DummyReplayComputeCommandEncoder->Serialise_setThreadgroupMemoryLength(ser, 0, 0);
    case MetalChunk::MTLComputeCommandEncoder_setSamplerState:
      return m_DummyReplayComputeCommandEncoder->Serialise_setSamplerState(ser, NULL, 0);
    case MetalChunk::MTLComputeCommandEncoder_setSamplerState_lodclamp:
      return m_DummyReplayComputeCommandEncoder->Serialise_setSamplerStateWithLOD(
          ser, NULL, 0.0f, 0.0f, 0);
    case MetalChunk::MTLComputeCommandEncoder_setSamplerStates_lodclamp:
      return m_DummyReplayComputeCommandEncoder->Serialise_setSamplerStatesWithLOD(
          ser, {}, {}, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLComputeCommandEncoder_setTextures:
      return m_DummyReplayComputeCommandEncoder->Serialise_setTextures(ser, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLComputeCommandEncoder_setSamplerStates:
      return m_DummyReplayComputeCommandEncoder->Serialise_setSamplerStates(ser, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLComputeCommandEncoder_setBuffers:
      return m_DummyReplayComputeCommandEncoder->Serialise_setBuffers(ser, {}, {}, NS::Range::Make(0, 0));
    case MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups:
    {
      MTL::Size groups = MTL::Size::Make(0, 0, 0);
      MTL::Size threads = MTL::Size::Make(0, 0, 0);
      return m_DummyReplayComputeCommandEncoder->Serialise_dispatchThreadgroups(ser, groups,
                                                                                threads);
    }
    case MetalChunk::MTLComputeCommandEncoder_dispatchThreads:
    {
      MTL::Size grid = MTL::Size::Make(0, 0, 0);
      MTL::Size threads = MTL::Size::Make(0, 0, 0);
      return m_DummyReplayComputeCommandEncoder->Serialise_dispatchThreads(ser, grid, threads);
    }
    case MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups_indirect:
    {
      MTL::Size threads = MTL::Size::Make(0, 0, 0);
      return m_DummyReplayComputeCommandEncoder->Serialise_dispatchThreadgroups(
          ser, (WrappedMTLBuffer *)NULL, 0, threads);
    }
    case MetalChunk::MTLArgumentEncoder_setArgumentBuffer:
      return m_DummyReplayArgumentEncoder->Serialise_setArgumentBuffer(ser, NULL, 0);
    case MetalChunk::MTLArgumentEncoder_newArgumentEncoderForBufferAtIndex:
      return m_DummyReplayArgumentEncoder->Serialise_newArgumentEncoder(ser, NULL, 0, 0, 0, false);
    case MetalChunk::MTLArgumentEncoder_setArgumentBuffer_arrayElement:
      return m_DummyReplayArgumentEncoder->Serialise_setArgumentBufferArray(ser, NULL, 0, 0);
    case MetalChunk::MTLArgumentEncoder_setBuffer:
      return m_DummyReplayArgumentEncoder->Serialise_setBuffer(ser, NULL, 0, 0);
    case MetalChunk::MTLArgumentEncoder_constantDataAtIndex:
      return m_DummyReplayArgumentEncoder->Serialise_constantDataAtIndex(ser, 0);
    case MetalChunk::MTLArgumentEncoder_unsupportedEncoding:
      return m_DummyReplayArgumentEncoder->Serialise_unsupportedEncoding(ser);
    case MetalChunk::MTLArgumentEncoder_setTexture:
      return m_DummyReplayArgumentEncoder->Serialise_setTexture(ser, NULL, 0);
    case MetalChunk::MTLArgumentEncoder_setSamplerState:
      return m_DummyReplayArgumentEncoder->Serialise_setSamplerState(ser, NULL, 0);
    case MetalChunk::MTLIndirectCommandBuffer_indirectRenderCommand:
      return m_DummyReplayIndirectCommandBuffer->Serialise_indirectRenderCommand(ser, NULL, 0);
    case MetalChunk::MTLIndirectRenderCommand_setRenderPipelineState:
      return m_DummyReplayIndirectRenderCommand->Serialise_setRenderPipelineState(ser, NULL);
    case MetalChunk::MTLIndirectRenderCommand_setVertexBuffer:
      return m_DummyReplayIndirectRenderCommand->Serialise_setVertexBuffer(ser, NULL, 0, 0);
    case MetalChunk::MTLIndirectRenderCommand_drawPrimitives:
      return m_DummyReplayIndirectRenderCommand->Serialise_drawPrimitives(
          ser, MTL::PrimitiveTypeTriangle, 0, 0, 0, 0);
    case MetalChunk::MTLIndirectRenderCommand_drawIndexedPrimitives:
      return m_DummyReplayIndirectRenderCommand->Serialise_drawIndexedPrimitives(
          ser, MTL::PrimitiveTypeTriangle, 0, MTL::IndexTypeUInt16, NULL, 0, 0, 0, 0);
    case MetalChunk::MTLIndirectCommandBuffer_reset:
      return m_DummyReplayIndirectCommandBuffer->Serialise_reset(ser, NS::Range::Make(0, 0));
    case MetalChunk::MTLIndirectRenderCommand_reset:
      return m_DummyReplayIndirectRenderCommand->Serialise_reset(ser);
    case MetalChunk::MTLIndirectCommandBuffer_unavailableInitialContents:
      return m_DummyReplayIndirectCommandBuffer->Serialise_unavailableInitialContents(ser);
    case MetalChunk::MTLSharedEvent_unsupportedHostMutation:
      RDCERR("Metal shared-event CPU mutation or handle export cannot be replayed");
      return false;

    // no default to get compile error if a chunk is not handled
    case MetalChunk::Max: break;
  }

  {
    SystemChunk system = (SystemChunk)chunk;
    if(system == SystemChunk::DriverInit)
    {
      MetalInitParams InitParams;
      SERIALISE_ELEMENT(InitParams);

      SERIALISE_CHECK_READ_ERRORS();
    }
    else if(system == SystemChunk::InitialContentsList)
    {
      // TODO: Create initial contents
      RDCERR("SystemChunk::InitialContentsList not handled");

      SERIALISE_CHECK_READ_ERRORS();
    }
    else if(system == SystemChunk::InitialContents)
    {
      return Serialise_InitialState(ser, ResourceId(), NULL, NULL);
    }
    else if(system == SystemChunk::CaptureScope)
    {
      return Serialise_CaptureScope(ser);
    }
    else if(system == SystemChunk::CaptureBegin)
    {
      return Serialise_BeginCaptureFrame(ser);
    }
    else if(system == SystemChunk::CaptureEnd)
    {
      SERIALISE_ELEMENT_LOCAL(PresentedImage, ResourceId()).TypedAs("MTLTexture"_lit);

      SERIALISE_CHECK_READ_ERRORS();

      if(PresentedImage != ResourceId())
      {
        m_LastPresentedImage = PresentedImage;
        WrappedMTLTexture *texture = (WrappedMTLTexture *)GetResourceManager()->GetResource(
            PresentedImage, true);
        if(texture)
          GetReplay()->AddTexture(PresentedImage, Unwrap(texture), true);
      }

      if(IsLoading(m_State))
      {
        AddEvent();

        ActionDescription action;
        action.customName = "End of Capture";
        action.flags |= ActionFlags::Present;
        action.copyDestination = m_LastPresentedImage;
        AddAction(action);
      }
      return true;
    }
    else if(system < SystemChunk::FirstDriverChunk)
    {
      RDCERR("Unexpected system chunk in capture data: %u", system);
      ser.SkipCurrentChunk();

      SERIALISE_CHECK_READ_ERRORS();
    }
    else
    {
      RDCERR("Unrecognised Chunk type %d", chunk);
      return false;
    }
  }

  return true;
}

rdcstr WrappedMTLDevice::GetChunkName(uint32_t idx)
{
  if((SystemChunk)idx < SystemChunk::FirstDriverChunk)
    return ToStr((SystemChunk)idx);

  return ToStr((MetalChunk)idx);
}

RDResult WrappedMTLDevice::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);
  if(sectionIdx < 0)
    RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

  StreamReader *reader = rdc->ReadSection(sectionIdx);
  if(reader->IsErrored())
  {
    RDResult result = reader->GetError();
    delete reader;
    return result;
  }

  ReadSerialiser ser(reader, Ownership::Stream);
  ser.SetUserData(GetResourceManager());
  ser.ConfigureStructuredExport(&GetChunkName, storeStructuredBuffers, 0, 1.0);
  m_StructuredFile = &ser.GetStructuredFile();
  m_StructuredFile->version = m_SectionVersion;
  ser.SetVersion(m_SectionVersion);

  uint64_t frameDataSize = 0;

  while(!reader->AtEnd())
  {
    uint64_t offsetStart = reader->GetOffset();
    MetalChunk chunk = ser.ReadChunk<MetalChunk>();
    if(reader->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);

    bool success = ProcessChunk(ser, chunk);
    ser.EndChunk();

    if(reader->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);
    if(!success)
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Failed to process Metal chunk %s",
                          GetChunkName((uint32_t)chunk).c_str());

    if((SystemChunk)chunk == SystemChunk::CaptureScope)
    {
      GetReplay()->WriteFrameRecord().frameInfo.fileOffset = offsetStart;
      frameDataSize = reader->GetSize() - reader->GetOffset();
      m_FrameReader = new StreamReader(reader, frameDataSize);

      if(!GetReplay()->SnapshotTextureViewSources())
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal texture view initial state");

      // Discover CPU-updated buffers before executing the loading pass, so its first submission
      // also sees captured (possibly non-zero) initial bytes. Argument packets additionally need
      // restoration even without a frame CPU update; every copy below relocates their addresses.
      for(ResourceId id : GetReplay()->GetArgumentBuffers())
      {
        if(m_ReplayBufferInitialContents.count(id) == 0)
        {
          WrappedMTLBuffer *buffer = (WrappedMTLBuffer *)GetResourceManager()->GetResource(id);
          m_ReplayBufferInitialContents[id] = bytebuf((byte *)Unwrap(buffer)->contents(),
                                                       Unwrap(buffer)->length());
        }
        m_ReplayCPUUpdatedBuffers.insert(id);
      }
      // CPU-initialised member buffers may never change inside the frame. Restore all captured
      // Shared initial states, not only buffers discovered in InternalModifyCPUContents below.
      for(const auto &initial : m_ReplayBufferInitialContents)
      {
        WrappedMTLBuffer *buffer = (WrappedMTLBuffer *)GetResourceManager()->GetResource(initial.first);
        if(Unwrap(buffer)->storageMode() == MTL::StorageModeShared)
          m_ReplayCPUUpdatedBuffers.insert(initial.first);
      }
      {
        ReadSerialiser scan(m_FrameReader, Ownership::Nothing);
        scan.SetVersion(m_SectionVersion);
        while(!m_FrameReader->AtEnd() && !scan.IsErrored())
        {
          MetalChunk candidate = scan.ReadChunk<MetalChunk>();
          if(candidate == MetalChunk::MTLBuffer_InternalModifyCPUContents)
          {
            ResourceId buffer;
            scan.Serialise("Buffer"_lit, buffer);
            if(m_ReplayBufferInitialContents.count(buffer))
              m_ReplayCPUUpdatedBuffers.insert(buffer);
          }
          scan.SkipCurrentChunk();
          scan.EndChunk();
        }
        if(scan.IsErrored())
          return RDResult(ResultCode::APIDataCorrupted, scan.GetError().message);
      }
      if(!ResetReplayCPUUpdatedBuffers() || !GetReplay()->ResetTextureViewSources())
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal initial CPU buffer data");
      m_FrameReader->SetOffset(0);

      RDResult status = ContextReplayLog(m_State, ~0U, eReplay_Full);
      if(status != ResultCode::Succeeded)
      {
        // A rejected chunk can leave an encoder open. Complete the command buffer while its
        // referenced replay resources are still alive, even when loading the capture fails.
        FinishReplayCommands();
        return status;
      }

      if(GetReplay()->HasPendingComputeIndirectActions())
      {
        FinishReplayCommands();
        GetReplay()->ResolvePendingComputeIndirectActions();
      }

      break;
    }
  }

  m_StructuredFile->Swap(*m_StoredStructuredData);
  m_StructuredFile = m_StoredStructuredData;

  // Only submission-time CPU-updated buffers need the reset cache introduced for this path.
  // Other initial-content/resource types keep their existing replay handling.
  for(auto it = m_ReplayBufferInitialContents.begin(); it != m_ReplayBufferInitialContents.end();)
  {
    if(m_ReplayCPUUpdatedBuffers.count(it->first) == 0)
      it = m_ReplayBufferInitialContents.erase(it);
    else
      ++it;
  }

  GetReplay()->WriteFrameRecord().frameInfo.uncompressedFileSize =
      rdc->GetSectionProperties(sectionIdx).uncompressedSize;
  GetReplay()->WriteFrameRecord().frameInfo.compressedFileSize =
      rdc->GetSectionProperties(sectionIdx).compressedSize;
  GetReplay()->WriteFrameRecord().frameInfo.persistentSize = frameDataSize;

  return ResultCode::Succeeded;
}

RDResult WrappedMTLDevice::ContextReplayLog(CaptureState readType, uint32_t endEventID,
                                            ReplayLogType replayType)
{
  if(!m_FrameReader)
    RETURN_ERROR_RESULT(ResultCode::InvalidParameter,
                        "Can't replay Metal capture without frame reader");

  m_State = readType;
  if(replayType != eReplay_OnlyDraw)
    ++m_ReplayEpoch;
  m_FrameReader->SetOffset(0);

  ReadSerialiser ser(m_FrameReader, Ownership::Nothing);
  ser.SetUserData(GetResourceManager());
  ser.SetVersion(m_SectionVersion);

  SDFile *previousStructuredFile = m_StructuredFile;
  if(IsLoading(m_State) || IsStructuredExporting(m_State))
  {
    ser.ConfigureStructuredExport(&GetChunkName, IsStructuredExporting(m_State), 0, 1.0);
    ser.GetStructuredFile().Swap(*m_StructuredFile);
    m_StructuredFile = &ser.GetStructuredFile();
  }

  MetalChunk header = ser.ReadChunk<MetalChunk>();
  if((SystemChunk)header != SystemChunk::CaptureBegin)
  {
    if(m_StructuredFile != previousStructuredFile)
    {
      m_StructuredFile->Swap(*previousStructuredFile);
      m_StructuredFile = previousStructuredFile;
    }
    RETURN_ERROR_RESULT(ResultCode::APIDataCorrupted,
                        "Metal frame stream does not begin with CaptureBegin");
  }

  if(IsLoading(m_State) || IsStructuredExporting(m_State))
    ProcessChunk(ser, header);
  else
    ser.SkipCurrentChunk();
  ser.EndChunk();

  uint64_t startOffset = ser.GetReader()->GetOffset();
  uint64_t endOffset = ser.GetReader()->GetSize();

  if(IsActiveReplaying(m_State))
  {
    const APIEvent *event = GetReplay()->GetEvent(endEventID);
    if(!event)
    {
      if(m_StructuredFile != previousStructuredFile)
      {
        m_StructuredFile->Swap(*previousStructuredFile);
        m_StructuredFile = previousStructuredFile;
      }
      return ResultCode::Succeeded;
    }

    if(replayType == eReplay_WithoutDraw)
    {
      endOffset = event->fileOffset;
    }
    else
    {
      const uint32_t replayEndEvent = GetReplay()->GetMultiActionEndEvent(event->eventId);
      endOffset = GetReplay()->GetNextEventOffset(replayEndEvent, ser.GetReader()->GetSize());
      if(replayType == eReplay_OnlyDraw)
      {
        startOffset = event->fileOffset;
        ser.GetReader()->SetOffset(startOffset);
      }
    }
  }

  while(!ser.GetReader()->AtEnd() && ser.GetReader()->GetOffset() < endOffset)
  {
    m_CurChunkOffset = ser.GetReader()->GetOffset();
    MetalChunk chunk = ser.ReadChunk<MetalChunk>();
    if(ser.IsErrored())
      break;

    const uint32_t eventBeforeChunk = IsLoading(m_State) ? GetReplay()->GetNextEventID() : 0;
    bool success = ProcessChunk(ser, chunk);
    // Keep every frame call addressable. Actions already call AddEvent() themselves.
    if(IsLoading(m_State) && success && GetReplay()->GetNextEventID() == eventBeforeChunk)
      AddEvent();
    ser.EndChunk();

    if(ser.IsErrored())
      break;
    if(!success)
    {
      if(m_StructuredFile != previousStructuredFile)
      {
        m_StructuredFile->Swap(*previousStructuredFile);
        m_StructuredFile = previousStructuredFile;
      }
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Failed to replay Metal chunk %s",
                          GetChunkName((uint32_t)chunk).c_str());
    }

    if((SystemChunk)chunk == SystemChunk::CaptureEnd)
      break;

    RenderDoc::Inst().SetProgress(
        LoadProgress::FrameEventsRead,
        float(m_CurChunkOffset - startOffset) /
            float(RDCMAX(1ULL, endOffset - startOffset)));
  }

  RDResult result = ResultCode::Succeeded;
  if(ser.IsErrored())
    result = RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);

  if(m_StructuredFile != previousStructuredFile)
  {
    m_StructuredFile->Swap(*previousStructuredFile);
    m_StructuredFile = previousStructuredFile;
  }

  return result;
}

void WrappedMTLDevice::FinishReplayCommands()
{
  if(m_ReplayAccelerationStructureCommandEncoder)
  {
    Unwrap(m_ReplayAccelerationStructureCommandEncoder)->endEncoding();
    m_ReplayAccelerationStructureCommandEncoder = NULL;
  }
  if(m_ReplayComputeCommandEncoder)
  {
    Unwrap(m_ReplayComputeCommandEncoder)->endEncoding();
    m_ReplayComputeCommandEncoder = NULL;
  }
  if(m_ReplayBlitCommandEncoder)
  {
    Unwrap(m_ReplayBlitCommandEncoder)->endEncoding();
    m_ReplayBlitCommandEncoder = NULL;
  }

  if(m_ReplayRenderCommandEncoder)
  {
    m_ReplayRenderCommandEncoder->ResolveDeferredStoreActions();
    Unwrap(m_ReplayRenderCommandEncoder)->endEncoding();
    m_ReplayRenderCommandEncoder = NULL;
  }

  if(m_ReplayCommandBuffer)
  {
    if(!m_ReplayCommandBufferCommitted)
      Unwrap(m_ReplayCommandBuffer)->commit();
    Unwrap(m_ReplayCommandBuffer)->waitUntilCompleted();
    if(NS::Error *error = Unwrap(m_ReplayCommandBuffer)->error())
      RDCERR("Metal replay command buffer failed: %s",
             error->localizedDescription()->utf8String());
    m_ReplayCommandBuffer = NULL;
    m_ReplayCommandBufferCommitted = false;
  }
}

RDResult WrappedMTLDevice::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  if(replayType != eReplay_OnlyDraw)
  {
    FinishReplayCommands();
    if(!ResetReplayCPUUpdatedBuffers() || !GetReplay()->ResetTextureViewSources())
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal CPU buffer reset");
  }

  RDResult result = ContextReplayLog(CaptureState::ActiveReplaying, endEventID, replayType);
  if(result != ResultCode::Succeeded || replayType != eReplay_WithoutDraw)
    FinishReplayCommands();
  return result;
}

bool WrappedMTLDevice::SetReplayCommandBuffer(WrappedMTLCommandBuffer *commandBuffer)
{
  // A following submission can update the same shared memory used by the preceding submission.
  // Finish the preceding replay buffer before making those CPU writes visible.
  FinishReplayCommands();
  m_ReplayCommandBuffer = commandBuffer;
  m_ReplayCommandBufferCommitted = false;
  if(IsActiveReplaying(m_State))
  {
    auto it = m_ReplayCPUBufferUpdates.find(GetResID(commandBuffer));
    if(it != m_ReplayCPUBufferUpdates.end())
      for(const CPUBufferUpdate &update : it->second)
      {
        WrappedMTLObject *object = GetResourceManager()->GetResource(update.buffer, true);
        if(!object || object->m_Type != eResBuffer || !object->m_Real)
          return false;
        MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)object);
        if(buffer->storageMode() != MTL::StorageModeShared || !buffer->contents() ||
           update.offset > buffer->length() || update.data.size() > buffer->length() - update.offset)
          return false;
        memcpy((byte *)buffer->contents() + update.offset, update.data.data(), update.data.size());
        if(!GetReplay()->RestoreArgumentBufferResources(update.buffer))
          return false;
      }
  }
  return true;
}

bool WrappedMTLDevice::RecordReplayBufferInitialContents(ResourceId id, const bytebuf &contents)
{
  WrappedMTLObject *object = GetResourceManager()->GetResource(id, true);
  if(!object || object->m_Type != eResBuffer || !object->m_Real ||
     contents.size() != Unwrap((WrappedMTLBuffer *)object)->length() ||
     m_ReplayBufferInitialContents.count(id))
  {
    RDCERR("Invalid Metal initial buffer contents");
    return false;
  }
  m_ReplayBufferInitialContents[id] = contents;
  return true;
}

bool WrappedMTLDevice::ReplayCPUBufferUpdate(WrappedMTLBuffer *wrapped, uint64_t start,
                                            const bytebuf &data)
{
  if(!wrapped || wrapped->m_Type != eResBuffer || !Unwrap(wrapped))
    return false;
  MTL::Buffer *buffer = Unwrap(wrapped);
  if(buffer->storageMode() != MTL::StorageModeShared || !buffer->contents() || data.empty() ||
     start > buffer->length() || data.size() > buffer->length() - start || !m_ReplayCommandBuffer)
    return false;
  if(IsLoading(m_State))
  {
    ResourceId id = GetResID(wrapped);
    if(m_ReplayBufferInitialContents.count(id) == 0)
      m_ReplayBufferInitialContents[id] = bytebuf((byte *)buffer->contents(), buffer->length());
    m_ReplayCPUUpdatedBuffers.insert(id);
    CPUBufferUpdate update = {id, start, data};
    m_ReplayCPUBufferUpdates[GetResID(m_ReplayCommandBuffer)].push_back(update);
    memcpy((byte *)buffer->contents() + start, data.data(), data.size());
    if(!GetReplay()->RestoreArgumentBufferResources(id))
      return false;
  }
  // Active replay has already applied the validated update at command-buffer creation.
  return true;
}

bool WrappedMTLDevice::ResetReplayCPUUpdatedBuffers()
{
  for(ResourceId id : m_ReplayCPUUpdatedBuffers)
  {
    WrappedMTLObject *object = GetResourceManager()->GetResource(id, true);
    if(!object || object->m_Type != eResBuffer || !object->m_Real)
      return false;
    MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)object);
    auto it = m_ReplayBufferInitialContents.find(id);
    if(it == m_ReplayBufferInitialContents.end() || buffer->storageMode() != MTL::StorageModeShared ||
       !buffer->contents() || it->second.size() != buffer->length())
      return false;
    memcpy(buffer->contents(), it->second.data(), it->second.size());
    if(!GetReplay()->RestoreArgumentBufferResources(id))
      return false;
  }
  return true;
}

void WrappedMTLDevice::AddResource(ResourceId id, ResourceType type, const char *defaultNamePrefix)
{
  ResourceDescription &descr = GetReplay()->GetResourceDesc(id);

  uint64_t num;
  memcpy(&num, &id, sizeof(uint64_t));
  descr.name = defaultNamePrefix + (" " + ToStr(num));
  descr.autogeneratedName = true;
  descr.type = type;
  AddResourceCurChunk(descr);
}

void WrappedMTLDevice::DerivedResource(ResourceId parentLive, ResourceId child)
{
  ResourceId parentId = parentLive;

  GetReplay()->GetResourceDesc(parentId).derivedResources.push_back(child);
  GetReplay()->GetResourceDesc(child).parentResources.push_back(parentId);
}

void WrappedMTLDevice::AddResourceCurChunk(ResourceDescription &descr)
{
  descr.initialisationChunks.push_back((uint32_t)m_StructuredFile->chunks.size() - 1);
}

void WrappedMTLDevice::WaitForGPU()
{
  MTL::CommandBuffer *mtlCommandBuffer = m_mtlCommandQueue->commandBuffer();
  mtlCommandBuffer->commit();
  mtlCommandBuffer->waitUntilCompleted();
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_BeginCaptureFrame(SerialiserType &ser)
{
  // TODO: serialise image references and states

  SERIALISE_CHECK_READ_ERRORS();

  return true;
}

void WrappedMTLDevice::StartFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsBackgroundCapturing(m_State))
    return;

  RDCLOG("Starting capture");
  {
    SCOPED_LOCK(m_CaptureCommandBuffersLock);
    RDCASSERT(m_CaptureCommandBuffersSubmitted.empty());
  }

  m_CaptureTimer.Restart();

  GetResourceManager()->ResetCaptureStartTime();

  m_AppControlledCapture = true;

  FrameDescription frame;
  frame.frameNumber = ~0U;
  frame.captureTime = Timing::GetUnixTimestamp();
  m_CapturedFrames.push_back(frame);

  GetResourceManager()->ClearReferencedResources();
  // TODO: handle tracked memory

  // need to do all this atomically so that no other commands
  // will check to see if they need to mark dirty or
  // mark pending dirty and go into the frame record.
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);

    GetResourceManager()->PrepareInitialContents();

    RDCDEBUG("Attempting capture");
    m_FrameCaptureRecord->DeleteChunks();
    m_State = CaptureState::ActiveCapturing;
    ++m_CaptureEpoch;
  }

  GetResourceManager()->MarkResourceFrameReferenced(GetResID(this), eFrameRef_Read);

  // A buffer-backed texture can be the only resource bound by the frame. The parent buffer's
  // initial bytes are still required to reconstruct that texture's shared allocation.
  {
    SCOPED_LOCK(m_BufferTextureParentsLock);
    for(ResourceId id : m_BufferTextureParents)
      if(GetResourceManager()->HasResource(id))
        GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }

  // TODO: are there other resources that need to be marked as frame referenced
}

void WrappedMTLDevice::RegisterBufferTextureParent(ResourceId texture, ResourceId id)
{
  if(!IsCaptureMode(m_State) || texture == ResourceId() || id == ResourceId())
    return;
  {
    SCOPED_LOCK(m_BufferTextureParentsLock);
    m_BufferTextureParents.insert(id);
    m_BufferTextureParentByView[texture] = id;
  }
  GetResourceManager()->MarkDirtyResource(id);
  if(IsActiveCapturing(m_State))
    GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_Read);
}

void WrappedMTLDevice::EndCaptureFrame(ResourceId backbuffer)
{
  CACHE_THREAD_SERIALISER();
  ser.SetActionChunk();
  SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureEnd);

  SERIALISE_ELEMENT_LOCAL(PresentedImage, backbuffer).TypedAs("MTLTexture"_lit);

  m_FrameCaptureRecord->AddChunk(scope.Get());
}

bool WrappedMTLDevice::EndFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  RDCLOG("Finished capture, Frame %u", m_CapturedFrames.back().frameNumber);

  ResourceId bbId;
  WrappedMTLTexture *backBuffer = m_CapturedBackbuffer;
  m_CapturedBackbuffer = NULL;
  if(backBuffer)
  {
    bbId = GetResID(backBuffer);
  }
  if(bbId == ResourceId())
  {
    RDCERR("Invalid Capture backbuffer");
    return false;
  }
  GetResourceManager()->MarkResourceFrameReferenced(bbId, eFrameRef_Read);

  // atomically transition to IDLE
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);
    EndCaptureFrame(bbId);
    m_State = CaptureState::BackgroundCapturing;
  }

  {
    SCOPED_LOCK(m_CaptureCommandBuffersLock);
    // wait for the GPU to be idle
    for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
    {
      WrappedMTLCommandBuffer *commandBuffer = (WrappedMTLCommandBuffer *)(record->m_Resource);
      Unwrap(commandBuffer)->waitUntilCompleted();
      // Remove the reference on the real resource added during commit()
      Unwrap(commandBuffer)->release();
    }

    if(m_CaptureCommandBuffersSubmitted.empty())
      WaitForGPU();
  }

  RenderDoc::FramePixels fp;

  MTL::Texture *mtlBackBuffer = Unwrap(backBuffer);

  // The backbuffer has to be a non-framebufferOnly texture
  // to be able to copy the pixels for the thumbnail
  if(!mtlBackBuffer->framebufferOnly())
  {
    const uint32_t maxSize = 2048;

    MTL::CommandBuffer *mtlCommandBuffer = m_mtlCommandQueue->commandBuffer();
    MTL::BlitCommandEncoder *mtlBlitEncoder = mtlCommandBuffer->blitCommandEncoder();

    uint32_t sourceWidth = (uint32_t)mtlBackBuffer->width();
    uint32_t sourceHeight = (uint32_t)mtlBackBuffer->height();
    MTL::Origin sourceOrigin(0, 0, 0);
    MTL::Size sourceSize(sourceWidth, sourceHeight, 1);

    MTL::PixelFormat format = mtlBackBuffer->pixelFormat();
    uint32_t bytesPerRow = GetByteSize(sourceWidth, 1, 1, format, 0);
    NS::UInteger bytesPerImage = sourceHeight * bytesPerRow;

    MTL::Buffer *mtlCpuPixelBuffer =
        Unwrap(this)->newBuffer(bytesPerImage, MTL::ResourceStorageModeShared);

    mtlBlitEncoder->copyFromTexture(mtlBackBuffer, 0, 0, sourceOrigin, sourceSize,
                                    mtlCpuPixelBuffer, 0, bytesPerRow, bytesPerImage);
    mtlBlitEncoder->endEncoding();

    mtlCommandBuffer->commit();
    mtlCommandBuffer->waitUntilCompleted();

    fp.len = (uint32_t)mtlCpuPixelBuffer->length();
    fp.data = new uint8_t[fp.len];
    memcpy(fp.data, mtlCpuPixelBuffer->contents(), fp.len);

    mtlCpuPixelBuffer->release();

    ResourceFormat fmt = MakeResourceFormat(format);
    fp.width = sourceWidth;
    fp.height = sourceHeight;
    fp.pitch = bytesPerRow;
    fp.stride = fmt.compByteWidth * fmt.compCount;
    fp.bpc = fmt.compByteWidth;
    fp.bgra = fmt.BGRAOrder();
    fp.max_width = maxSize;
    fp.pitch_requirement = 8;

    // TODO: handle different resource formats
  }

  RDCFile *rdc =
      RenderDoc::Inst().CreateRDC(RDCDriver::Metal, m_CapturedFrames.back().frameNumber, fp);

  StreamWriter *captureWriter = NULL;

  if(rdc)
  {
    SectionProperties props;

    // Compress with LZ4 so that it's fast
    props.flags = SectionFlags::LZ4Compressed;
    props.version = m_SectionVersion;
    props.type = SectionType::FrameCapture;

    captureWriter = rdc->WriteSection(props);
  }
  else
  {
    captureWriter = new StreamWriter(StreamWriter::InvalidStream);
  }

  uint64_t captureSectionSize = 0;

  {
    WriteSerialiser ser(captureWriter, Ownership::Stream);

    ser.SetChunkMetadataRecording(GetThreadSerialiser().GetChunkMetadataRecording());
    ser.SetUserData(GetResourceManager());

    {
      m_InitParams.Set(Unwrap(this), m_ID);
      SCOPED_SERIALISE_CHUNK(SystemChunk::DriverInit, m_InitParams.GetSerialiseSize());
      SERIALISE_ELEMENT(m_InitParams);
    }

    RDCDEBUG("Inserting Resource Serialisers");
    GetResourceManager()->InsertReferencedChunks(ser);
    GetResourceManager()->InsertInitialContentsChunks(ser);

    RDCDEBUG("Creating Capture Scope");
    GetResourceManager()->Serialise_InitialContentsNeeded(ser);
    // TODO: memory references

    // need over estimate of chunk size when writing directly to file
    {
      SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureScope, 16);
      Serialise_CaptureScope(ser);
    }

    {
      uint64_t maxCaptureBeginChunkSizeInBytes = 16;
      SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureBegin, maxCaptureBeginChunkSizeInBytes);
      Serialise_BeginCaptureFrame(ser);
    }

    // don't need to lock access to m_CaptureCommandBuffersSubmitted as
    // no longer in active capture (the transition is thread-protected)
    // nothing will be pushed to the vector

    {
      std::map<int64_t, Chunk *> recordlist;
      size_t countCmdBuffers = m_CaptureCommandBuffersSubmitted.size();
      // ensure all command buffer records within the frame even if recorded before
      // serialised order must be preserved
      for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
      {
        size_t prevSize = recordlist.size();
        (void)prevSize;
        record->Insert(recordlist);
      }

      size_t prevSize = recordlist.size();
      (void)prevSize;
      m_FrameCaptureRecord->Insert(recordlist);
      RDCDEBUG("Adding %zu/%zu frame capture chunks to file serialiser",
               recordlist.size() - prevSize, recordlist.size());

      float num = float(recordlist.size());
      float idx = 0.0f;

      for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
      {
        RenderDoc::Inst().SetProgress(CaptureProgress::SerialiseFrameContents, idx / num);
        idx += 1.0f;
        it->second->Write(ser);
      }
    }
    captureSectionSize = captureWriter->GetOffset();
  }

  RDCLOG("Captured Metal frame with %f MB capture section in %f seconds",
         double(captureSectionSize) / (1024.0 * 1024.0), m_CaptureTimer.GetMilliseconds() / 1000.0);

  RenderDoc::Inst().FinishCaptureWriting(rdc, m_CapturedFrames.back().frameNumber);

  // delete tracked cmd buffers - had to keep them alive until after serialiser flush.
  CaptureClearSubmittedCmdBuffers();

  GetResourceManager()->ResetLastWriteTimes();
  GetResourceManager()->MarkUnwrittenResources();

  // TODO: handle memory resources in the resource manager

  GetResourceManager()->ClearReferencedResources();
  GetResourceManager()->FreeInitialContents();

  // TODO: handle memory resources in the initial contents

  return true;
}

bool WrappedMTLDevice::DiscardFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  RDCLOG("Discarding frame capture.");

  RenderDoc::Inst().FinishCaptureWriting(NULL, m_CapturedFrames.back().frameNumber);

  m_CapturedFrames.pop_back();

  // atomically transition to IDLE
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);
    m_State = CaptureState::BackgroundCapturing;
  }

  CaptureClearSubmittedCmdBuffers();

  GetResourceManager()->MarkUnwrittenResources();

  // TODO: handle memory resources in the resource manager

  GetResourceManager()->ClearReferencedResources();
  GetResourceManager()->FreeInitialContents();

  // TODO: handle memory resources in the initial contents

  return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_CaptureScope(SerialiserType &ser)
{
  uint32_t frameNumber = ser.IsWriting() ? m_CapturedFrames.back().frameNumber : 0;
  SERIALISE_ELEMENT(frameNumber);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // TODO: implement RD MTL replay
  }
  return true;
}

void WrappedMTLDevice::CaptureCmdBufCPUWrites(MetalResourceRecord *record)
{
  if(IsActiveCapturing(m_State))
  {
    std::unordered_set<ResourceId> refIDs;
    record->AddReferencedIDs(refIDs);
    {
      SCOPED_LOCK(m_BufferTextureParentsLock);
      for(const auto &alias : m_BufferTextureParentByView)
        if(refIDs.count(alias.first))
          refIDs.insert(alias.second);
    }
    // snapshot/detect any CPU modifications to the contents
    // of referenced MTLBuffer with shared storage mode
    for(auto it = refIDs.begin(); it != refIDs.end(); ++it)
    {
      ResourceId id = *it;
      MetalResourceRecord *refRecord = GetResourceManager()->GetResourceRecord(id);
      if(refRecord && refRecord->m_Type == eResBuffer)
      {
        MetalBufferInfo *bufInfo = refRecord->bufInfo;
        if(bufInfo->storageMode == MTL::StorageModeShared)
        {
          size_t diffStart = 0;
          size_t diffEnd = bufInfo->length;
          bool foundDifference = true;
          if(!bufInfo->baseSnapshot.isEmpty())
          {
            foundDifference = FindDiffRange(bufInfo->data, bufInfo->baseSnapshot.data(),
                                            bufInfo->length, diffStart, diffEnd);
            if(diffEnd <= diffStart)
              foundDifference = false;
          }

          if(foundDifference)
          {
            if(bufInfo->data == NULL)
            {
              RDCERR("Writing buffer memory %s that is NULL", ToStr(id).c_str());
              continue;
            }
            Chunk *chunk = NULL;
            {
              CACHE_THREAD_SERIALISER();
              SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_InternalModifyCPUContents);
              ((WrappedMTLBuffer *)refRecord->m_Resource)
                  ->Serialise_InternalModifyCPUContents(ser, diffStart, diffEnd, bufInfo);
              chunk = scope.Get();
            }
            record->AddChunk(chunk);
          }
        }
      }
    }
  }
}

void WrappedMTLDevice::CaptureCmdBufSubmit(MetalResourceRecord *record)
{
  RDCASSERTEQUAL(record->cmdInfo->status, MetalCmdBufferStatus::Submitted);
  RDCASSERT(IsCaptureMode(m_State));
  WrappedMTLCommandBuffer *commandBuffer = (WrappedMTLCommandBuffer *)(record->m_Resource);
  if(IsActiveCapturing(m_State))
  {
    // The record will get deleted at the end of active frame capture.
    record->AddRef();
    record->MarkResourceFrameReferenced(GetResID(commandBuffer->GetCommandQueue()), eFrameRef_Read);
    // pull in frame refs from this command buffer
    record->AddResourceReferences(GetResourceManager());
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_commit);
      commandBuffer->Serialise_commit(ser);
      chunk = scope.Get();
    }
    record->AddChunk(chunk);
    m_CaptureCommandBuffersSubmitted.push_back(record);
  }
  else
  {
    // Remove the reference on the real resource added during commit()
    Unwrap(commandBuffer)->release();
  }
  if(record->cmdInfo->presented)
  {
    AdvanceFrame();
    Present(record);
  }
  // In background or active capture mode the record reference is incremented in
  // CaptureCmdBufEnqueue
  record->Delete(GetResourceManager());
}

void WrappedMTLDevice::CaptureCmdBufCommit(MetalResourceRecord *cbRecord)
{
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  if(cbRecord->cmdInfo->status != MetalCmdBufferStatus::Enqueued)
    CaptureCmdBufEnqueue(cbRecord);

  RDCASSERTEQUAL(cbRecord->cmdInfo->status, MetalCmdBufferStatus::Enqueued);
  cbRecord->cmdInfo->status = MetalCmdBufferStatus::Committed;

  size_t countSubmitted = 0;
  for(MetalResourceRecord *record : m_CaptureCommandBuffersEnqueued)
  {
    if(record->cmdInfo->status == MetalCmdBufferStatus::Committed)
    {
      record->cmdInfo->status = MetalCmdBufferStatus::Submitted;
      ++countSubmitted;
      CaptureCmdBufSubmit(record);
      continue;
    }
    break;
  };
  m_CaptureCommandBuffersEnqueued.erase(0, countSubmitted);
}

void WrappedMTLDevice::CaptureCmdBufEnqueue(MetalResourceRecord *cbRecord)
{
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  RDCASSERTEQUAL(cbRecord->cmdInfo->status, MetalCmdBufferStatus::Unknown);
  cbRecord->cmdInfo->status = MetalCmdBufferStatus::Enqueued;
  cbRecord->AddRef();
  m_CaptureCommandBuffersEnqueued.push_back(cbRecord);

  RDCDEBUG("Enqueing CommandBufferRecord %s %d", ToStr(cbRecord->GetResourceID()).c_str(),
           m_CaptureCommandBuffersEnqueued.count());
}

void WrappedMTLDevice::AdvanceFrame()
{
  if(IsBackgroundCapturing(m_State))
    RenderDoc::Inst().Tick();

  m_FrameCounter++;    // first present becomes frame #1, this function is at the end of the frame
}

void WrappedMTLDevice::FirstFrame()
{
  // if we have to capture the first frame, begin capturing immediately
  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(0))
  {
    RenderDoc::Inst().StartFrameCapture(DeviceOwnedWindow(this, NULL));

    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = 0;
  }
}

void WrappedMTLDevice::Present(MetalResourceRecord *record)
{
  WrappedMTLTexture *backBuffer = record->cmdInfo->backBuffer;
  {
    SCOPED_LOCK(m_CapturePotentialBackBuffersLock);
    if(m_CapturePotentialBackBuffers.count(backBuffer) == 0)
    {
      RDCERR("Capture ignoring Present called on unknown backbuffer");
      return;
    }
  }

  CA::MetalLayer *outputLayer = record->cmdInfo->outputLayer;
  DeviceOwnedWindow devWnd(this, outputLayer);

  bool activeWindow = RenderDoc::Inst().IsActiveWindow(devWnd);

  RenderDoc::Inst().AddActiveDriver(RDCDriver::Metal, true);

  if(!activeWindow)
    return;

  if(IsActiveCapturing(m_State))
  {
    RDCASSERT(m_CapturedBackbuffer == NULL);
    m_CapturedBackbuffer = backBuffer;

    if(!m_AppControlledCapture)
      RenderDoc::Inst().EndFrameCapture(devWnd);
  }

  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && IsBackgroundCapturing(m_State))
  {
    RenderDoc::Inst().StartFrameCapture(devWnd);

    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = m_FrameCounter;
  }
}

void WrappedMTLDevice::CaptureClearSubmittedCmdBuffers()
{
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
  {
    record->Delete(GetResourceManager());
  }

  m_CaptureCommandBuffersSubmitted.clear();
}

void WrappedMTLDevice::RegisterMetalLayer(CA::MetalLayer *mtlLayer)
{
  SCOPED_LOCK(m_CaptureOutputLayersLock);
  if(m_CaptureOutputLayers.count(mtlLayer) == 0)
  {
    m_CaptureOutputLayers.insert(mtlLayer);
    TrackedCAMetalLayer::Track(mtlLayer, this);

    DeviceOwnedWindow devWnd(this, mtlLayer);
    RenderDoc::Inst().AddFrameCapturer(devWnd, &m_Capturer);
  }
}

void WrappedMTLDevice::UnregisterMetalLayer(CA::MetalLayer *mtlLayer)
{
  SCOPED_LOCK(m_CaptureOutputLayersLock);
  RDCASSERT(m_CaptureOutputLayers.count(mtlLayer));
  m_CaptureOutputLayers.erase(mtlLayer);

  DeviceOwnedWindow devWnd(this, mtlLayer);
  RenderDoc::Inst().RemoveFrameCapturer(devWnd);
}

void WrappedMTLDevice::RegisterDrawableInfo(CA::MetalDrawable *caMtlDrawable,
                                            MTL::Texture *realTexture)
{
  MetalDrawableInfo drawableInfo;
  drawableInfo.mtlLayer = caMtlDrawable->layer();
  drawableInfo.texture = WrapDrawableTexture(realTexture);
  drawableInfo.drawableID = caMtlDrawable->drawableID();
  {
    SCOPED_LOCK(m_CaptureDrawablesLock);
    RDCASSERTEQUAL(m_CaptureDrawableInfos.find(caMtlDrawable), m_CaptureDrawableInfos.end());
    m_CaptureDrawableInfos[caMtlDrawable] = drawableInfo;
  }
  {
    SCOPED_LOCK(s_DrawableTexturesLock);
    s_DrawableTextures[caMtlDrawable] = drawableInfo.texture;
  }
}

WrappedMTLTexture *WrappedMTLDevice::GetDrawableTexture(MTL::Drawable *mtlDrawable)
{
  SCOPED_LOCK(s_DrawableTexturesLock);
  auto it = s_DrawableTextures.find(mtlDrawable);
  return it == s_DrawableTextures.end() ? NULL : it->second;
}

MetalDrawableInfo WrappedMTLDevice::UnregisterDrawableInfo(MTL::Drawable *mtlDrawable)
{
  MetalDrawableInfo drawableInfo;
  {
    SCOPED_LOCK(m_CaptureDrawablesLock);
    auto it = m_CaptureDrawableInfos.find(mtlDrawable);
    if(it != m_CaptureDrawableInfos.end())
    {
      drawableInfo = it->second;
      m_CaptureDrawableInfos.erase(it);
      {
        SCOPED_LOCK(s_DrawableTexturesLock);
        s_DrawableTextures.erase(mtlDrawable);
      }
      return drawableInfo;
    }
  }
  // Not found by pointer fall back and check by drawableID
  NS::UInteger drawableID = mtlDrawable->drawableID();
  for(auto it = m_CaptureDrawableInfos.begin(); it != m_CaptureDrawableInfos.end(); ++it)
  {
    drawableInfo = it->second;
    if(drawableInfo.drawableID == drawableID)
    {
      MTL::Drawable *registeredDrawable = it->first;
      m_CaptureDrawableInfos.erase(it);
      {
        SCOPED_LOCK(s_DrawableTexturesLock);
        s_DrawableTextures.erase(registeredDrawable);
      }
      return drawableInfo;
    }
  }
  drawableInfo.mtlLayer = NULL;
  drawableInfo.texture = NULL;
  return drawableInfo;
}

MetalInitParams::MetalInitParams()
{
  memset(this, 0, sizeof(MetalInitParams));
}

uint64_t MetalInitParams::GetSerialiseSize()
{
  size_t ret = sizeof(*this);
  return (uint64_t)ret;
}

void MetalInitParams::Set(MTL::Device *pRealDevice, ResourceId device)
{
  DeviceID = device;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MetalInitParams &el)
{
  SERIALISE_MEMBER(DeviceID).TypedAs("MTLDevice"_lit);
}

INSTANTIATE_SERIALISE_TYPE(MetalInitParams);
