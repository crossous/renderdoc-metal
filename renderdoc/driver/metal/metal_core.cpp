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
#include "metal_parallel_render_command_encoder.h"
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
  m_ReplayChunkIsGPUWork = false;
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
        m_ReplayChunkIsGPUWork = true;
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
          ser, NULL, MTL::DispatchTypeSerial, {});
    case MetalChunk::MTLCommandBuffer_blitCommandEncoderWithDescriptor:
      return m_DummyReplayCommandBuffer->Serialise_blitCommandEncoderWithDescriptor(ser, NULL, false, {});
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
    {
      RDMTL::RenderPassDescriptor descriptor;
      return m_DummyReplayCommandBuffer->Serialise_parallelRenderCommandEncoderWithDescriptor(
          ser, NULL, descriptor);
    }
    case MetalChunk::MTLParallelRenderCommandEncoder_renderCommandEncoder:
      return m_DummyReplayParallelRenderCommandEncoder->Serialise_renderCommandEncoder(ser, NULL);
    case MetalChunk::MTLParallelRenderCommandEncoder_endEncoding:
      return m_DummyReplayParallelRenderCommandEncoder->Serialise_endEncoding(ser);
    case MetalChunk::MTLParallelRenderCommandEncoder_setColorStoreAction:
      return m_DummyReplayParallelRenderCommandEncoder->Serialise_setStoreAction(
          ser, MTL::StoreActionStore, 0, 0);
    case MetalChunk::MTLParallelRenderCommandEncoder_setDepthStoreAction:
      return m_DummyReplayParallelRenderCommandEncoder->Serialise_setStoreAction(
          ser, MTL::StoreActionStore, 0, 1);
    case MetalChunk::MTLParallelRenderCommandEncoder_setStencilStoreAction:
      return m_DummyReplayParallelRenderCommandEncoder->Serialise_setStoreAction(
          ser, MTL::StoreActionStore, 0, 2);
    case MetalChunk::MTLParallelRenderCommandEncoder_setColorStoreActionOptions:
      return m_DummyReplayParallelRenderCommandEncoder->Serialise_setStoreActionOptions(
          ser, MTL::StoreActionOptionNone, 0, 0);
    case MetalChunk::MTLParallelRenderCommandEncoder_setDepthStoreActionOptions:
      return m_DummyReplayParallelRenderCommandEncoder->Serialise_setStoreActionOptions(
          ser, MTL::StoreActionOptionNone, 0, 1);
    case MetalChunk::MTLParallelRenderCommandEncoder_setStencilStoreActionOptions:
      return m_DummyReplayParallelRenderCommandEncoder->Serialise_setStoreActionOptions(
          ser, MTL::StoreActionOptionNone, 0, 2);
    case MetalChunk::MTLParallelRenderCommandEncoder_pushDebugGroup:
      return m_DummyReplayParallelRenderCommandEncoder->Serialise_debugLabel(ser, NULL, 0);
    case MetalChunk::MTLParallelRenderCommandEncoder_popDebugGroup:
      return m_DummyReplayParallelRenderCommandEncoder->Serialise_debugLabel(ser, NULL, 1);
    case MetalChunk::MTLParallelRenderCommandEncoder_insertDebugSignpost:
      return m_DummyReplayParallelRenderCommandEncoder->Serialise_debugLabel(ser, NULL, 2);
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
    case MetalChunk::MTLResource_CaptureGPUIdentity:
      return Serialise_CaptureGPUIdentity(ser, ResourceId(), 0, 0);
    case MetalChunk::MTLBuffer_DeclareDescriptorTable:
      return Serialise_DeclareDescriptorTable(ser, ResourceId(), 0, 0, 0, 0);
    case MetalChunk::MTLDevice_DeclareDescriptorCoverage:
      return Serialise_DeclareDescriptorCoverage(ser, 0);
    case MetalChunk::MTLBuffer_DeclareDescriptorGPUWrites:
      return Serialise_DeclareDescriptorGPUWrites(ser, ResourceId());
    case MetalChunk::MTLComputeCommandEncoder_DeclareDescriptorBytes:
      return Serialise_DeclareDescriptorBytes(ser, ResourceId(), 0, 0, 0, 0);
    case MetalChunk::MTLBuffer_DescriptorCPUWrite:
      return Serialise_DescriptorCPUWrite(ser, ResourceId(), 0, bytebuf());
    case MetalChunk::MTLBuffer_DescriptorSlotEvent:
      return Serialise_DescriptorSlotEvent(ser, ResourceId(), 0, 0, 0, 0, bytebuf());
    case MetalChunk::MTLBuffer_DescriptorSlotBinding:
      return Serialise_DescriptorSlotBinding(ser, ResourceId(), 0, ResourceId(), 0, 0);
    case MetalChunk::MTLCommandEncoder_DescriptorInlineLayout:
      return Serialise_DescriptorInlineLayout(ser, ResourceId(), 0, 0, 0, 0);
    case MetalChunk::MTLCommandEncoder_DescriptorInlineBinding:
      return Serialise_DescriptorInlineBinding(ser, ResourceId(), 0, 0, 0, ResourceId(), 0);
    case MetalChunk::MTLDevice_CaptureComputeIndirectArgumentsCount:
      return Serialise_CaptureComputeIndirectArgumentsCount(ser, 0);
    case MetalChunk::MTLComputeCommandEncoder_CaptureIndirectArguments:
      return Serialise_CaptureComputeIndirectArguments(ser, ResourceId(), ResourceId(), 0,
                                                     ResourceId(), 0, {});
    case MetalChunk::MTLDevice_CaptureRenderIndirectArgumentsCount:
      return Serialise_CaptureRenderIndirectArgumentsCount(ser,0);
    case MetalChunk::MTLRenderCommandEncoder_CaptureIndirectArguments:
      return Serialise_CaptureRenderIndirectArguments(ser,ResourceId(),ResourceId(),ResourceId(),
          0,ResourceId(),0,0,false,{});
    case MetalChunk::MTLBuffer_DescriptorSlotProducer:
      return Serialise_DescriptorSlotProducer(ser, ResourceId(), 0, ResourceId(), ResourceId(), 0);
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
    case MetalChunk::MTLComputeCommandEncoder_useHeap:
      return m_DummyReplayComputeCommandEncoder->Serialise_declareHeaps(ser, {}, false);
    case MetalChunk::MTLComputeCommandEncoder_useHeaps:
      return m_DummyReplayComputeCommandEncoder->Serialise_declareHeaps(ser, {}, true);
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

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_CaptureGPUIdentity(SerialiserType &ser, ResourceId resource,
                                                   uint32_t kind, uint64_t value)
{
  SERIALISE_ELEMENT(resource).Important();
  SERIALISE_ELEMENT(kind).Important();
  SERIALISE_ELEMENT(value).Important();
  SERIALISE_CHECK_READ_ERRORS();
  // Structured export must retain the capture-time values without executing GPU work.
  if(!IsStructuredExporting(m_State) && ser.IsReading() && !m_DescriptorCoverage)
  {
    RDCERR("Metal raw GPU identity relocation is not supported; use CPU structured export");
    return false;
  }
  return true;
}

template bool WrappedMTLDevice::Serialise_CaptureGPUIdentity(ReadSerialiser &, ResourceId,
                                                            uint32_t, uint64_t);
template bool WrappedMTLDevice::Serialise_CaptureGPUIdentity(WriteSerialiser &, ResourceId,
                                                            uint32_t, uint64_t);

void WrappedMTLDevice::CaptureGPUIdentity(WrappedMTLObject *object, uint32_t kind, uint64_t value)
{
  if(!IsCaptureMode(m_State) || !object || !value || kind > 2 || !GetRecord(object))
    return;
  // A cached getter can be used after capture starts even if no encoder API binds
  // the object explicitly. Deduplicate the metadata, not the frame dependency.
  if(IsActiveCapturing(m_State))
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(object), eFrameRef_Read);
  if(Atomic::CmpExch32(&object->m_CapturedGPUIdentity, 0, 1) != 0)
    return;
  CACHE_THREAD_SERIALISER();
  SCOPED_SERIALISE_CHUNK(MetalChunk::MTLResource_CaptureGPUIdentity);
  Serialise_CaptureGPUIdentity(ser, GetResID(object), kind, value);
  Chunk *chunk = scope.Get();
  GetRecord(object)->AddChunk(chunk);
  if(IsActiveCapturing(m_State))
  {
    // Frame-created resources are emitted in the frame stream; retain the record as well
    // for a later capture in which this resource already exists at frame start.
    AddFrameCaptureRecordChunk(chunk->Duplicate());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(object), eFrameRef_Read);
  }
}

static bool FindMetalFrameDiagnosticBoundary(StreamReader *reader, uint64_t version,
                                             uint32_t commits, uint64_t &endOffset)
{
  reader->SetOffset(0);
  ReadSerialiser scan(reader,Ownership::Nothing);scan.SetVersion(version);
  std::map<ResourceId,bool> pending;
  std::set<ResourceId> submitted;
  uint32_t count=0;
  bool accepted=false;
  while(!reader->AtEnd() && !scan.IsErrored())
  {
    MetalChunk chunk=scan.ReadChunk<MetalChunk>();
    const rdcstr name=ToStr(chunk);
    if(name.contains("MTLCommandQueue::commandBuffer"))
    {
      ResourceId queue,command;
      scan.Serialise("CommandQueue"_lit,queue);scan.Serialise("CommandBuffer"_lit,command);
      if(command==ResourceId() || pending.count(command) || submitted.count(command))break;
      pending[command]=false;
    }
    else if(chunk==MetalChunk::MTLCommandBuffer_enqueue || chunk==MetalChunk::MTLCommandBuffer_commit ||
        (name.contains("MTLCommandBuffer::") && (name.contains("Encoder") ||
         name.contains("encodeSignalEvent") || name.contains("encodeWaitForEvent") || name.contains("presentDrawable"))))
    {
      ResourceId command;scan.Serialise("CommandBuffer"_lit,command);
      if(!pending.count(command))break;
      if(chunk==MetalChunk::MTLCommandBuffer_commit)
      {
        pending.erase(command);submitted.insert(command);
        if(++count==commits)
        {
          accepted=true;
          for(const auto &entry:pending)accepted &= !entry.second;
          scan.EndChunk();endOffset=reader->GetOffset();break;
        }
      }
      else pending[command]=true;
    }
    scan.EndChunk();
  }
  accepted &= !scan.IsErrored();reader->SetOffset(0);
  return accepted;
}

RDResult WrappedMTLDevice::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);
  if(sectionIdx < 0)
    RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

  const char *cpuMetadataCoverage=getenv("RENDERDOC_METAL_CPU_METADATA_COVERAGE");
  const char *preSubmitCoverage=getenv("RENDERDOC_METAL_PRE_SUBMIT_COVERAGE");
  const char *initialUploadCoverage=getenv("RENDERDOC_METAL_INITIAL_UPLOAD_COVERAGE");
  const char *prefixCoverage=getenv("RENDERDOC_METAL_FRAME_PREFIX_COVERAGE");
  const char *prefixCount=getenv("RENDERDOC_METAL_FRAME_PREFIX_COMMITS");
  char *prefixEnd=NULL;
  const unsigned long prefixCommits=prefixCount?strtoul(prefixCount,&prefixEnd,10):0;
  if((cpuMetadataCoverage && strcmp(cpuMetadataCoverage,"60") && strcmp(cpuMetadataCoverage,"61") && strcmp(cpuMetadataCoverage,"62") && strcmp(cpuMetadataCoverage,"63") && strcmp(cpuMetadataCoverage,"64") && strcmp(cpuMetadataCoverage,"65")) ||
     (preSubmitCoverage && strcmp(preSubmitCoverage,"60") && strcmp(preSubmitCoverage,"61") && strcmp(preSubmitCoverage,"62") && strcmp(preSubmitCoverage,"63") && strcmp(preSubmitCoverage,"64") && strcmp(preSubmitCoverage,"65")) ||
     (initialUploadCoverage && strcmp(initialUploadCoverage,"65")) ||
     (prefixCoverage && (strcmp(prefixCoverage,"65") || !prefixCount || !prefixCommits ||
                         prefixCommits>256 || !prefixEnd || *prefixEnd)) ||
     (prefixCount && !prefixCoverage) ||
     (int(cpuMetadataCoverage!=NULL)+int(preSubmitCoverage!=NULL)+int(initialUploadCoverage!=NULL)+int(prefixCoverage!=NULL)>1))
    RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid CPU-only Metal metadata diagnostic version");
  const uint32_t diagnosticCoverage=cpuMetadataCoverage ? uint32_t(atoi(cpuMetadataCoverage)) :
      preSubmitCoverage ? uint32_t(atoi(preSubmitCoverage)) : (initialUploadCoverage || prefixCoverage) ? 65 : 60;
  if(cpuMetadataCoverage)
    fprintf(stderr,"Metal CPU-only metadata preflight begin: candidate coverage%u; loading and GPU replay disabled\n",diagnosticCoverage);
  if(preSubmitCoverage)
    fprintf(stderr,"Metal pre-submit diagnostic begin: candidate coverage%u; frame GPU replay disabled\n",diagnosticCoverage);
  if(initialUploadCoverage)
    fprintf(stderr,"Metal initial upload diagnostic begin: candidate coverage65; frame GPU replay disabled\n");
  if(prefixCoverage)
    fprintf(stderr,"Metal frame prefix diagnostic begin: candidate coverage65; commits=%lu; forced exit after bounded replay\n",prefixCommits);
  RDResult descriptorScan = ScanDescriptorMetadata(rdc, sectionIdx, (cpuMetadataCoverage || preSubmitCoverage || initialUploadCoverage || prefixCoverage) ? diagnosticCoverage : 0);
  if(descriptorScan != ResultCode::Succeeded)
    return descriptorScan;
  if(cpuMetadataCoverage)
    RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
        "Metal CPU-only metadata preflight accepted candidate coverage%u; loading and GPU replay were not executed",diagnosticCoverage);

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

    if(preSubmitCoverage || initialUploadCoverage || prefixCoverage)
    {
      const rdcstr name=GetChunkName((uint32_t)chunk);
      // Background capture proof chunks only contain CPU evidence. Any real
      // command encoding/submission before Scope must stop this diagnostic.
      // CPU replaceRegion remains subject to CPUTextureUploadSpan's Shared/
      // Managed storage and range/pitch validation; it creates no replay CB.
      const bool proof=chunk==MetalChunk::MTLComputeCommandEncoder_CaptureIndirectArguments ||
                       chunk==MetalChunk::MTLRenderCommandEncoder_CaptureIndirectArguments;
      if(!proof && (name.contains("CommandBuffer::") || name.contains("CommandEncoder::")))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
            "Metal pre-submit diagnostic rejected background GPU operation %s",name.c_str());
    }

    bool success = ProcessChunk(ser, chunk);
    ser.EndChunk();

    if(reader->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);
    if(!success)
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Failed to process Metal chunk %s",
                          GetChunkName((uint32_t)chunk).c_str());

    if((SystemChunk)chunk == SystemChunk::CaptureScope)
    {
      // A pre-frame BC placement texture must have captured block contents. Older v0xF
      // captures did not snapshot textures; replaying them would sample uninitialised memory.
      for(const TextureDescription &description : GetReplay()->GetTextures())
      {
        WrappedMTLObject *object = GetResourceManager()->GetResource(description.resourceId, true);
        MTL::Texture *texture = object && object->m_Type == eResTexture ?
                                Unwrap((WrappedMTLTexture *)object) : NULL;
        if(m_DescriptorCoverage >= 27 && texture &&
           texture->storageMode() == MTL::StorageModePrivate && !texture->parentTexture() &&
           !texture->buffer() && (texture->pixelFormat() == MTL::PixelFormatRGBA8Unorm ||
                                 texture->pixelFormat() == MTL::PixelFormatBGRA8Unorm) &&
           m_ReplayTextureInitialContents.count(description.resourceId) == 0)
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
                              "Missing Metal Private color texture initial contents");
        if(!texture || !texture->heap() || texture->heap()->type() != MTL::HeapTypePlacement)
          continue;
        const MTL::PixelFormat format = texture->pixelFormat();
        if((format == MTL::PixelFormatBC1_RGBA ||
            format == MTL::PixelFormatBC1_RGBA_sRGB ||
            format == MTL::PixelFormatBC5_RGUnorm) &&
           m_ReplayTextureInitialContents.count(description.resourceId) == 0)
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
                              "Missing Metal BC placement texture initial contents");
      }
      GetReplay()->WriteFrameRecord().frameInfo.fileOffset = offsetStart;
      frameDataSize = reader->GetSize() - reader->GetOffset();
      m_FrameReader = new StreamReader(reader, frameDataSize);

      // An Empty/Volatile transition is only legal after the GPU has stopped using the
      // resource. Capture completion callbacks are not represented by command-buffer chunks,
      // and replay currently does not reproduce their resource lifetime boundary. Scan before
      // executing any frame GPU work: otherwise Metal Validation can abort the process while
      // setting a still-in-use resource purgeable. Structured export only decodes chunks and
      // must remain available for CPU-only diagnosis of captures rejected by this gate.
      if(!IsStructuredExporting(m_State))
      {
        m_TerminalFramePurgeableBuffers.clear();
        std::map<ResourceId, uint64_t> purgeOffsets;
        ReadSerialiser safetyScan(m_FrameReader, Ownership::Nothing);
        safetyScan.SetVersion(m_SectionVersion);
        while(!m_FrameReader->AtEnd() && !safetyScan.IsErrored())
        {
          MetalChunk candidate = safetyScan.ReadChunk<MetalChunk>();
          const uint64_t offset = m_FrameReader->GetOffset();
          if(candidate == MetalChunk::MTLBuffer_setPurgeableState ||
             candidate == MetalChunk::MTLTexture_setPurgeableState)
          {
            ResourceId resource;
            uint32_t state = 0;
            safetyScan.Serialise(candidate == MetalChunk::MTLBuffer_setPurgeableState ?
                                     "Buffer"_lit : "Texture"_lit,
                                 resource);
            safetyScan.Serialise("State"_lit, state);
            if(resource == ResourceId() ||
               (state != MTL::PurgeableStateKeepCurrent &&
                state != MTL::PurgeableStateNonVolatile &&
                state != MTL::PurgeableStateEmpty))
              RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
                                  "Invalid Metal frame purgeable state or resource");
            if(purgeOffsets.count(resource))
              RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
                                  "Metal buffer is referenced after Empty (%s)",
                                  ToStr(resource).c_str());
            if(candidate == MetalChunk::MTLTexture_setPurgeableState &&
               state == MTL::PurgeableStateEmpty)
              RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
                                  "Metal frame purgeable-state transition requires GPU completion "
                                  "tracking before replay (resource %s)", ToStr(resource).c_str());
            if(candidate == MetalChunk::MTLBuffer_setPurgeableState &&
               state == MTL::PurgeableStateEmpty)
              purgeOffsets[resource] = offset;
          }
          // Search the entire chunk payload for ResourceIds equal to a purged buffer.
          // This is deliberately conservative: a false match rejects a supported capture, while
          // a missing reference could otherwise allow a use after Empty on GPU. The scan also
          // checks child fields in descriptors and argument packets without assuming their layout.
          const uint64_t end = offset + safetyScan.ChunkMetadata().length;
          if(end < offset || end > m_FrameReader->GetSize())
            RETURN_ERROR_RESULT(ResultCode::APIDataCorrupted,
                                "Invalid Metal frame chunk length during purge scan");
          if(!purgeOffsets.empty())
          {
            bytebuf bytes;
            bytes.resize(64 * 1024 + sizeof(ResourceId) - 1);
            size_t overlap = 0;
            while(m_FrameReader->GetOffset() < end)
            {
              const size_t count = (size_t)RDCMIN(uint64_t(64 * 1024),
                                                  end - m_FrameReader->GetOffset());
              if(!m_FrameReader->Read(bytes.data() + overlap, count))
                RETURN_ERROR_RESULT(ResultCode::APIDataCorrupted,
                                    "Invalid Metal frame chunk during purge scan");
              const size_t available = overlap + count;
              for(const auto &purge : purgeOffsets)
              {
                if(offset <= purge.second)
                  continue;
                for(size_t i = 0; i + sizeof(ResourceId) <= available; i++)
                {
                  ResourceId found;
                  memcpy(&found, bytes.data() + i, sizeof(found));
                  if(found == purge.first)
                    RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
                                        "Metal buffer is referenced after Empty (%s)",
                                        ToStr(found).c_str());
                }
              }
              overlap = RDCMIN(available, sizeof(ResourceId) - 1);
              memmove(bytes.data(), bytes.data() + available - overlap, overlap);
            }
          }
          safetyScan.EndChunk();
        }
        if(safetyScan.IsErrored())
          return RDResult(ResultCode::APIDataCorrupted, safetyScan.GetError().message);
        for(const auto &purge : purgeOffsets)
          m_TerminalFramePurgeableBuffers.insert(purge.first);
        m_FrameReader->SetOffset(0);
      }

      if(!PrepareDescriptorTables())
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid or unsupported explicit Metal descriptor initial data");
      if(!ValidateDescriptorFrame())
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid or unsupported explicit Metal descriptor frame data");
      if(preSubmitCoverage)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
            "Metal pre-submit diagnostic accepted candidate coverage%u; initial GPU uploads and frame replay were not executed",diagnosticCoverage);
      uint64_t prefixEndOffset=0;
      if(prefixCoverage && !FindMetalFrameDiagnosticBoundary(m_FrameReader,m_SectionVersion,
                                                            uint32_t(prefixCommits),prefixEndOffset))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
            "Metal frame prefix diagnostic rejected non-quiescent or missing commit boundary; initial GPU uploads and frame replay were not executed");

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
          scan.EndChunk();
        }
        if(scan.IsErrored())
          return RDResult(ResultCode::APIDataCorrupted, scan.GetError().message);
      }
      if(!ResetReplayCPUUpdatedBuffers() || !RestoreReplayPrivateBufferInitialContents() ||
         !RestoreReplayTextureInitialContents() ||
         !GetReplay()->SnapshotTextureViewSources() ||
         !GetReplay()->ResetTextureViewSources())
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal initial CPU buffer data");
      if(initialUploadCoverage)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
            "Metal initial upload diagnostic accepted candidate coverage65; initial GPU uploads completed and frame replay was not executed");
      if(getenv("RENDERDOC_METAL_TRACE_INDIRECT_REPLAY"))
      {
        std::set<ResourceId> traced;
        for(const auto &proof : m_CapturedComputeIndirectArguments)
          if(traced.insert(proof.second.buffer).second)
          {
            bytebuf actual;
            GetReplay()->GetBufferData(proof.second.buffer, 0, 32, actual);
            fprintf(stderr, "Metal indirect initial readback: buffer=%s bytes=%zu words=",
                    ToStr(proof.second.buffer).c_str(), actual.size());
            for(size_t i = 0; i + 4 <= actual.size(); i += 4)
            {
              uint32_t word = 0;
              memcpy(&word, actual.data() + i, 4);
              fprintf(stderr, "%u,", word);
            }
            fprintf(stderr, "\n");
          }
      }
      m_FrameReader->SetOffset(0);

      RDResult status = ContextReplayLog(m_State, ~0U, eReplay_Full,prefixEndOffset);
      if(status != ResultCode::Succeeded)
      {
        // A rejected chunk can leave an encoder open. Complete the command buffer while its
        // referenced replay resources are still alive, even when loading the capture fails.
        FinishReplayCommands();
        return status;
      }

      if(GetReplay()->HasPendingComputeIndirectActions() || GetReplay()->HasPendingRenderIndirectActions())
      {
        for(ResourceId id : m_PendingReplayBufferPurges)
          if(GetReplay()->HasPendingComputeIndirectActionFor(id))
            RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
                                "Metal terminal purge conflicts with indirect argument read (%s)",
                                ToStr(id).c_str());
        if(!FinishReplayCommands())
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
                              "Invalid Metal replay completion before indirect action resolve");
        if(!GetReplay()->ResolvePendingComputeIndirectActions())
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
                              "Metal compute indirect execution-point arguments do not match capture");
        if(!GetReplay()->ResolvePendingRenderIndirectActions())
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
                              "Metal render indirect execution-point arguments do not match capture");
      }
      if(prefixCoverage)
      {
        if(!FinishReplayCommands())
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Metal frame prefix diagnostic failed Native completion");
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
            "Metal frame prefix diagnostic accepted candidate coverage65; %lu commits completed; capture loading stopped at commit boundary",prefixCommits);
      }

      break;
    }
  }

  m_StructuredFile->Swap(*m_StoredStructuredData);
  m_StructuredFile = m_StoredStructuredData;

  // Keep Private snapshots as well: a GPU-written indirect argument or vertex buffer can be
  // read before the frame writes it, and every later event replay must start from those bytes.
  for(auto it = m_ReplayBufferInitialContents.begin(); it != m_ReplayBufferInitialContents.end();)
  {
    WrappedMTLObject *object = GetResourceManager()->GetResource(it->first, true);
    const bool privateBuffer = object && object->m_Type == eResBuffer && object->m_Real &&
                               Unwrap((WrappedMTLBuffer *)object)->storageMode() ==
                                   MTL::StorageModePrivate;
    if(m_ReplayCPUUpdatedBuffers.count(it->first) == 0 && !privateBuffer)
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
                                            ReplayLogType replayType, uint64_t diagnosticEndOffset)
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
  if(diagnosticEndOffset)
  {
    if(!IsLoading(m_State) || diagnosticEndOffset<startOffset || diagnosticEndOffset>endOffset)
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid Metal loading diagnostic boundary");
    endOffset=diagnosticEndOffset;
  }

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

bool WrappedMTLDevice::ReplayMissingCopySubmissionPrefix(WrappedMTLCommandBuffer *target)
{
  if(m_DescriptorCoverage < 52 || !IsActiveReplaying(m_State)) return true;
  const ResourceId targetID = GetResID(target);
  const uint64_t savedOffset = m_FrameReader->GetOffset(), savedChunk = m_CurChunkOffset;
  bool success = true;
  auto replayChunk = [&](uint64_t offset) {
    m_FrameReader->SetOffset(offset); m_CurChunkOffset = offset;
    ReadSerialiser dependency(m_FrameReader, Ownership::Nothing);
    dependency.SetVersion(m_SectionVersion); dependency.SetUserData(GetResourceManager());
    MetalChunk chunk = dependency.ReadChunk<MetalChunk>();
    const bool result = !dependency.IsErrored() && ProcessChunk(dependency, chunk);
    dependency.EndChunk();
    return result && !dependency.IsErrored();
  };
  // Like D3D12/Vulkan submission replay, retain every earlier GPU submission in
  // the prefix, even when its encoding appears later than this event in the file.
  // Reconstruct only fully preflighted ordinary copies and timeline signals. Vulkan
  // queue replay likewise handles submission synchronization separately from draw encoding.
  for(ResourceId prior : m_DescriptorSubmissionOrder)
  {
    if(prior == targetID) break;
    const auto state = m_ReplayCommandBuffers.find(prior);
    if(state != m_ReplayCommandBuffers.end() && state->second.committed) continue;
    const auto planned = m_DescriptorPartialCopySubmissions.find(prior);
    if(planned == m_DescriptorPartialCopySubmissions.end() || !planned->second.valid ||
       (!planned->second.copies && !planned->second.signals) ||
       (state != m_ReplayCommandBuffers.end() && state->second.encoded))
    {
      RDCERR("Unsupported missing Metal submission prefix %s before %s", ToStr(prior).c_str(), ToStr(targetID).c_str());
      success = false; break;
    }
    if(Unwrap(target)->status() != MTL::CommandBufferStatusNotEnqueued)
    {
      RDCERR("Cannot replay a missing producer behind a reserved consumer"); success = false; break;
    }
    rdcarray<uint64_t> births;
    for(ResourceId buffer : planned->second.buffers)
    {
      auto object = GetResourceManager()->GetResource(buffer, true);
      if(object && object->m_Type == eResBuffer && object->m_Real) continue;
      const auto birth = m_DescriptorFrameBufferBirthOffsets.find(buffer);
      const auto description = m_DescriptorFrameBuffers.find(buffer);
      if(birth == m_DescriptorFrameBufferBirthOffsets.end() || description == m_DescriptorFrameBuffers.end())
      { success = false; break; }
      if(description->second.heap != ResourceId()) {
        auto parent = GetResourceManager()->GetResource(description->second.heap, true);
        if(!parent || parent->m_Type != eResHeap || !parent->m_Real) { success = false; break; }
      }
      births.push_back(birth->second);
    }
    std::sort(births.begin(), births.end());
    if(success) for(uint64_t birth : births) if(!replayChunk(birth)) { success = false; break; }
    if(!success) break;
    if(!Process::GetEnvVariable("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT").empty())
      fprintf(stderr,"Metal replay missing copy submission prefix: %s copies=%u signals=%u births=%zu\n",ToStr(prior).c_str(),planned->second.copies,planned->second.signals,births.size());
    for(uint64_t offset : planned->second.chunks) {
      // Its creation may already be in the selected event prefix.
      if(state != m_ReplayCommandBuffers.end() && offset == planned->second.chunks.front()) continue;
      if(!replayChunk(offset)) { success = false; break; }
    }
    if(!success) break;
    const auto done = m_ReplayCommandBuffers.find(prior);
    if(done == m_ReplayCommandBuffers.end() || !done->second.committed) { success = false; break; }
  }
  m_FrameReader->SetOffset(savedOffset); m_CurChunkOffset = savedChunk;
  return SelectReplayCommandBuffer(target) && success;
}

bool WrappedMTLDevice::FinishReplayCommands()
{
  bool success = true;
  const bool traceWaits = !Process::GetEnvVariable("RENDERDOC_METAL_TRACE_REPLAY_WAITS").empty();
  // Explicit enqueue reserves queue order before commit. At a partial replay boundary,
  // submit those reservations before buffers which were only created. Otherwise a CPU
  // snapshot wait on a newly committed empty buffer can wait behind our own uncommitted
  // reservation. D3D12/Vulkan likewise use submission order rather than creation order.
  rdcarray<ResourceId> tailOrder = m_ReplayCommandBufferQueueOrder;
  if(m_DescriptorCoverage >= 52 && IsActiveReplaying(m_State))
    for(ResourceId id : m_DescriptorSubmissionOrder)
      if(m_ReplayCommandBuffers.count(id) && !tailOrder.contains(id)) tailOrder.push_back(id);
  for(ResourceId id : m_ReplayCommandBufferOrder)
    if(!tailOrder.contains(id)) tailOrder.push_back(id);
  for(ResourceId id : tailOrder)
  {
    auto it = m_ReplayCommandBuffers.find(id);
    if(it == m_ReplayCommandBuffers.end() || !it->second.buffer)
      continue;
    // A later submission may already have been created, while its encoders and
    // frame resources have not reached the seek target. Do not apply that submission's
    // future CPU snapshots or invent an empty commit unless it reserved native queue order.
    if(!it->second.committed && !it->second.encoded &&
       Unwrap(it->second.buffer)->status() == MTL::CommandBufferStatusNotEnqueued)
      continue;
    if(!it->second.committed && !ReplayMissingCopySubmissionPrefix(it->second.buffer))
    {
      success = false; continue;
    }
    if(!SelectReplayCommandBuffer(it->second.buffer))
    {
      success = false;
      continue;
    }
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
      if(!m_ReplayRenderCommandEncoder->GetParallelParent())
        m_ReplayRenderCommandEncoder->ResolveDeferredStoreActions();
      Unwrap(m_ReplayRenderCommandEncoder)->endEncoding();
      m_ReplayRenderCommandEncoder = NULL;
    }
    if(m_ReplayParallelRenderCommandEncoder)
    {
      m_ReplayParallelRenderCommandEncoder->ResolveDeferredStoreActions();
      Unwrap(m_ReplayParallelRenderCommandEncoder)->endEncoding();
      m_ReplayParallelRenderCommandEncoder = NULL;
    }
    if(!m_ReplayCommandBufferCommitted)
    {
      if(!ApplyReplayCPUBufferUpdates(m_ReplayCommandBuffer))
      {
        RDCERR("Invalid Metal CPU buffer update before partial replay submission");
        success = false;
        // An encoder has already been ended. Submit only if its inputs were restored.
        it->second.buffer = NULL;
        continue;
      }
      Unwrap(m_ReplayCommandBuffer)->commit();
      m_ReplayCommandBufferCommitted = true;
      it->second.committed = true;
    }
  }
  // A buffer can wait on an event signaled by another buffer created later in this frame. Commit
  // every partial-replay tail before waiting on any of them, preserving Metal queue dependencies.
  for(ResourceId id : m_ReplayCommandBufferOrder)
  {
    auto it = m_ReplayCommandBuffers.find(id);
    if(it == m_ReplayCommandBuffers.end() || !it->second.buffer || !it->second.committed)
      continue;
    MTL::CommandBuffer *real = Unwrap(it->second.buffer);
    if(traceWaits)
      fprintf(stderr, "Metal replay wait begin: buffer=%s status=%llu\n",
              ToStr(id).c_str(), (uint64_t)real->status());
    real->waitUntilCompleted();
    if(traceWaits)
      fprintf(stderr, "Metal replay wait end: buffer=%s status=%llu\n",
              ToStr(id).c_str(), (uint64_t)real->status());
    if(NS::Error *error = real->error())
    {
      RDCERR("Metal replay command buffer failed: %s", error->localizedDescription()->utf8String());
      success = false;
    }
    it->second.buffer = NULL;
  }
  if(success && !ApplyTerminalReplayBufferPurges())
    success = false;
  m_ReplayCommandBuffers.clear();
  m_ReplayCommandBufferOrder.clear();
  m_ReplayCommandBufferQueueOrder.clear();
  m_PendingReplayCPUBufferUpdates.clear();
  m_ReplayCommandBuffer = NULL;
  m_ReplayRenderCommandEncoder = NULL;
  m_ReplayParallelRenderCommandEncoder = NULL;
  m_ReplayBlitCommandEncoder = NULL;
  m_ReplayAccelerationStructureCommandEncoder = NULL;
  m_ReplayComputeCommandEncoder = NULL;
  m_ReplayCommandBufferCommitted = false;
  m_ReplayRenderTarget = ResourceId();
  GetReplay()->ClearEncoderContexts();
  return success;
}

bool WrappedMTLDevice::DeferTerminalBufferPurge(ResourceId id)
{
  if(!m_TerminalFramePurgeableBuffers.count(id) || m_PendingReplayBufferPurges.count(id))
  {
    RDCERR("Invalid Metal terminal buffer purge %s", ToStr(id).c_str());
    return false;
  }
  m_PendingReplayBufferPurges.insert(id);
  return true;
}

bool WrappedMTLDevice::CanReplayImplicitBufferAlias(ResourceId before, ResourceId after) const
{
  if(m_DescriptorCoverage < 17 ||
     (m_DescriptorCoverage < 18 && !IsFramePlacementResource(before))) return false;
  for(const DescriptorTable &table : m_DescriptorTables)
    if(table.buffer == before || table.buffer == after)
      if(m_DescriptorCoverage < 25 ||
         !m_ValidatedDescriptorBackingAliases.count(make_rdcpair(before, after))) return false;
  for(const auto &entry : m_ReplayCommandBuffers)
  {
    const ReplayCommandBufferState &state = entry.second;
    // Placement creation does not read, clear or retire an existing allocation.
    // v24 retains every referenced native object and uses the tracked heap for
    // command dependencies, including work that is still being encoded.
    if(m_DescriptorCoverage >= 24)
    {
      if(!state.buffer || !Unwrap(state.buffer) ||
         Unwrap(state.buffer)->status() == MTL::CommandBufferStatusError) return false;
      continue;
    }
    // Unencoded, unreserved future command objects have no GPU work to order.
    if(m_DescriptorCoverage >= 22 && state.buffer && !state.encoded && !state.committed &&
       Unwrap(state.buffer) && Unwrap(state.buffer)->status() == MTL::CommandBufferStatusNotEnqueued)
      continue;
    if(!state.buffer || !state.committed || state.render || state.parallel || state.blit ||
       state.acceleration || state.compute || !Unwrap(state.buffer) ||
       Unwrap(state.buffer)->status() == MTL::CommandBufferStatusError ||
       (m_DescriptorCoverage < 22 &&
        Unwrap(state.buffer)->status() != MTL::CommandBufferStatusCompleted)) return false;
  }
  return true;
}

bool WrappedMTLDevice::CanReplayRetiredTextureAlias(ResourceId before, ResourceId after)
{
  auto proof = m_RetiredTextureAliasConsumers.find(make_rdcpair(before, after));
  if(m_DescriptorCoverage < 65 || proof == m_RetiredTextureAliasConsumers.end()) return false;
  if(m_DescriptorPreflight) return true;
  // A retired texture's physical backing must outlive every encoded consumer.
  // Reuse the existing committed-command completion path; never submit or wait
  // on an unfinished command merely to permit a new allocation.
  for(ResourceId consumer : proof->second)
  {
    auto command = m_ReplayCommandBuffers.find(consumer);
    if(command == m_ReplayCommandBuffers.end() || !command->second.committed ||
       !command->second.buffer || !Unwrap(command->second.buffer)) return false;
    MTL::CommandBuffer *native = Unwrap(command->second.buffer);
    if(native->status() != MTL::CommandBufferStatusCompleted &&
       !WaitReplayCommandBuffer(command->second.buffer, "retired texture backing reuse")) return false;
    if(native->error()) return false;
  }
  return true;
}

bool WrappedMTLDevice::ApplyTerminalReplayBufferPurges()
{
  // All submitted work has completed in FinishReplayCommands before this is called. Deferring
  // Empty preserves the capture's logical end-of-frame lifetime without racing GPU use.
  for(ResourceId id : m_PendingReplayBufferPurges)
  {
    WrappedMTLObject *object = GetResourceManager()->GetResource(id, true);
    if(!object || object->m_Type != eResBuffer || !object->m_Real)
      return false;
    MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)object);
    if(buffer->setPurgeableState(MTL::PurgeableStateEmpty) != MTL::PurgeableStateNonVolatile)
      return false;
    m_ReplayPurgedBuffers.insert(id);
  }
  m_PendingReplayBufferPurges.clear();
  return true;
}

bool WrappedMTLDevice::RestoreReplayPurgedBuffers()
{
  for(ResourceId id : m_ReplayPurgedBuffers)
  {
    WrappedMTLObject *object = GetResourceManager()->GetResource(id, true);
    if(!object || object->m_Type != eResBuffer || !object->m_Real)
      return false;
    Unwrap((WrappedMTLBuffer *)object)->setPurgeableState(MTL::PurgeableStateNonVolatile);
  }
  m_ReplayPurgedBuffers.clear();
  return true;
}

RDResult WrappedMTLDevice::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  if(replayType != eReplay_OnlyDraw)
  {
    if(!FinishReplayCommands())
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal replay completion");
    if(!RestoreReplayPurgedBuffers())
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal purgeable buffer restore");
    // Frame-created placement children belong to a previous execution of the frame stream.
    // Release their native allocations before restoring the frame-start contents. Their
    // ResourceIds and metadata stay registered so the creation chunks can bind fresh objects.
    for(ResourceId id : m_FrameBufferTextureViews)
    {
      WrappedMTLObject *wrapped = GetResourceManager()->GetResource(id, true);
      if(wrapped && wrapped->m_Real)
        GetResourceManager()->ReplaceRealResource(wrapped, (NS::Object *)NULL, true);
    }
    std::set<WrappedMTLHeap *> affectedHeaps;
    for(const auto &resource : m_FramePlacementResources)
    {
      m_ReplayAliasableResources.erase(resource.first);
      WrappedMTLObject *wrapped = GetResourceManager()->GetResource(resource.first, true);
      if(wrapped && wrapped->m_Real)
        GetResourceManager()->ReplaceRealResource(wrapped, (NS::Object *)NULL, true);
      if(resource.second) affectedHeaps.insert(resource.second);
    }
    for(WrappedMTLHeap *heap : affectedHeaps)
      heap->ResetFramePlacementRanges();
    if(!ResetReplayCPUUpdatedBuffers())
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal Shared buffer reset");
    if(!RestoreReplayPrivateBufferInitialContents())
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal Private buffer reset");
    if(!RestoreReplayTextureInitialContents())
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal texture reset");
    if(!GetReplay()->ResetTextureViewSources())
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal texture view reset");
  }

  RDResult result = ContextReplayLog(CaptureState::ActiveReplaying, endEventID, replayType);
  if(result != ResultCode::Succeeded || replayType != eReplay_WithoutDraw)
  {
    if(!FinishReplayCommands())
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal replay completion");
  }
  return result;
}

bool WrappedMTLDevice::SetReplayCommandBuffer(WrappedMTLCommandBuffer *commandBuffer)
{
  if(!commandBuffer || m_ReplayCommandBuffers.size() >= 256 ||
     m_ReplayCommandBuffers.count(GetResID(commandBuffer)))
    return false;
  ReplayCommandBufferState state;
  state.buffer = commandBuffer;
  m_ReplayCommandBuffers[GetResID(commandBuffer)] = state;
  m_ReplayCommandBufferOrder.push_back(GetResID(commandBuffer));
  if(!SelectReplayCommandBuffer(commandBuffer))
    return false;
  return true;
}

bool WrappedMTLDevice::SelectReplayCommandBuffer(WrappedMTLCommandBuffer *buffer)
{
  if(!buffer || !buffer->m_Real)
    return false;
  auto next = m_ReplayCommandBuffers.find(GetResID(buffer));
  if(next == m_ReplayCommandBuffers.end() || next->second.buffer != buffer)
    return false;
  if(m_ReplayCommandBuffer == buffer)
    return true;
  if(m_ReplayCommandBuffer)
  {
    auto current = m_ReplayCommandBuffers.find(GetResID(m_ReplayCommandBuffer));
    if(current != m_ReplayCommandBuffers.end())
    {
      current->second.render = m_ReplayRenderCommandEncoder;
      current->second.parallel = m_ReplayParallelRenderCommandEncoder;
      current->second.blit = m_ReplayBlitCommandEncoder;
      current->second.acceleration = m_ReplayAccelerationStructureCommandEncoder;
      current->second.compute = m_ReplayComputeCommandEncoder;
      current->second.renderTarget = m_ReplayRenderTarget;
      current->second.committed = m_ReplayCommandBufferCommitted;
    }
  }
  m_ReplayCommandBuffer = buffer;
  m_ReplayRenderCommandEncoder = next->second.render;
  m_ReplayParallelRenderCommandEncoder = next->second.parallel;
  m_ReplayBlitCommandEncoder = next->second.blit;
  m_ReplayAccelerationStructureCommandEncoder = next->second.acceleration;
  m_ReplayComputeCommandEncoder = next->second.compute;
  m_ReplayRenderTarget = next->second.renderTarget;
  m_ReplayCommandBufferCommitted = next->second.committed;
  GetReplay()->ActivateEncoderContext(GetResID(buffer));
  return true;
}

bool WrappedMTLDevice::IsReplayCommandBufferCommitted(WrappedMTLCommandBuffer *buffer) const
{
  if(!buffer)
    return true;
  auto it = m_ReplayCommandBuffers.find(GetResID(buffer));
  return it == m_ReplayCommandBuffers.end() || it->second.committed ||
         (m_ReplayCommandBuffer == buffer && m_ReplayCommandBufferCommitted);
}

bool WrappedMTLDevice::WaitReplayCommandBuffer(WrappedMTLCommandBuffer *buffer, const char *reason)
{
  if(!buffer || !buffer->m_Real) return false;
  auto state = m_ReplayCommandBuffers.find(GetResID(buffer));
  // Native Metal waits include CPU callbacks. Replay never installs capture callbacks,
  // and must not wait on an uncommitted/enqueued reservation.
  if(state == m_ReplayCommandBuffers.end() || state->second.buffer != buffer ||
     !IsReplayCommandBufferCommitted(buffer)) return false;
  MTL::CommandBuffer *real = Unwrap(buffer);
  const bool trace = !Process::GetEnvVariable("RENDERDOC_METAL_TRACE_REPLAY_WAITS").empty();
  if(trace) fprintf(stderr, "Metal replay wait begin: %s buffer=%s status=%llu\n", reason, ToStr(GetResID(buffer)).c_str(), (uint64_t)real->status());
  real->waitUntilCompleted();
  if(trace) fprintf(stderr, "Metal replay wait end: %s buffer=%s status=%llu\n", reason, ToStr(GetResID(buffer)).c_str(), (uint64_t)real->status());
  if(NS::Error *error = real->error())
  {
    RDCERR("Metal captured completion wait failed: %s", error->localizedDescription()->utf8String());
    return false;
  }
  return real->status() == MTL::CommandBufferStatusCompleted;
}

bool WrappedMTLDevice::CanEncodeReplayEvent(WrappedMTLCommandBuffer *buffer)
{
  return SelectReplayCommandBuffer(buffer) && !m_ReplayCommandBufferCommitted &&
         !m_ReplayRenderCommandEncoder && !m_ReplayComputeCommandEncoder &&
         !m_ReplayBlitCommandEncoder && !m_ReplayAccelerationStructureCommandEncoder &&
         !m_ReplayParallelRenderCommandEncoder;
}

WrappedMTLRenderCommandEncoder *WrappedMTLDevice::GetReplayRenderCommandEncoder(
    WrappedMTLRenderCommandEncoder *encoder)
{
  if(encoder && encoder->m_Type == eResRenderCommandEncoder &&
     SelectReplayCommandBuffer(encoder->GetCommandBuffer()))
  {
    if(m_ReplayChunkIsGPUWork && m_ReplayRenderCommandEncoder == encoder)
      encoder->MarkGPUWork();
    return m_ReplayRenderCommandEncoder;
  }
  return NULL;
}

WrappedMTLParallelRenderCommandEncoder *WrappedMTLDevice::GetReplayParallelRenderCommandEncoder(
    WrappedMTLParallelRenderCommandEncoder *encoder)
{
  if(encoder && encoder->m_Type == eResParallelRenderCommandEncoder &&
     SelectReplayCommandBuffer(encoder->GetCommandBuffer()))
    return m_ReplayParallelRenderCommandEncoder;
  return NULL;
}

WrappedMTLBlitCommandEncoder *WrappedMTLDevice::GetReplayBlitCommandEncoder(
    WrappedMTLBlitCommandEncoder *encoder)
{
  if(encoder && encoder->m_Type == eResBlitCommandEncoder &&
     SelectReplayCommandBuffer(encoder->GetCommandBuffer()))
    return m_ReplayBlitCommandEncoder;
  return NULL;
}

WrappedMTLComputeCommandEncoder *WrappedMTLDevice::GetReplayComputeCommandEncoder(
    WrappedMTLComputeCommandEncoder *encoder)
{
  if(encoder && encoder->m_Type == eResComputeCommandEncoder &&
     SelectReplayCommandBuffer(encoder->GetCommandBuffer()))
    return m_ReplayComputeCommandEncoder;
  return NULL;
}

WrappedMTLAccelerationStructureCommandEncoder *
WrappedMTLDevice::GetReplayAccelerationStructureCommandEncoder(
    WrappedMTLAccelerationStructureCommandEncoder *encoder)
{
  if(encoder && encoder->m_Type == eResAccelerationStructureCommandEncoder &&
     SelectReplayCommandBuffer(encoder->GetCommandBuffer()))
    return m_ReplayAccelerationStructureCommandEncoder;
  return NULL;
}

bool WrappedMTLDevice::EnqueueReplayCommandBuffer(WrappedMTLCommandBuffer *buffer)
{
  if(!CanEncodeReplayEvent(buffer) ||
     Unwrap(buffer)->status() != MTL::CommandBufferStatusNotEnqueued ||
     m_ReplayCommandBufferQueueOrder.contains(GetResID(buffer))) return false;
  Unwrap(buffer)->enqueue();
  m_ReplayCommandBufferQueueOrder.push_back(GetResID(buffer));
  return true;
}

void WrappedMTLDevice::MarkReplayCommandBufferEncoded()
{
  if(m_ReplayCommandBuffer)
    m_ReplayCommandBuffers[GetResID(m_ReplayCommandBuffer)].encoded = true;
}

void WrappedMTLDevice::MarkReplayCommandBufferCommitted()
{
  if(m_ReplayCommandBuffer)
  {
    m_ReplayCommandBufferCommitted = true;
    m_ReplayCommandBuffers[GetResID(m_ReplayCommandBuffer)].committed = true;
    if(!m_ReplayCommandBufferQueueOrder.contains(GetResID(m_ReplayCommandBuffer)))
      m_ReplayCommandBufferQueueOrder.push_back(GetResID(m_ReplayCommandBuffer));
  }
}

void WrappedMTLDevice::AssignPendingReplayCPUBufferUpdates(WrappedMTLCommandBuffer *buffer)
{
  if(buffer && !m_PendingReplayCPUBufferUpdates.empty())
  {
    rdcarray<CPUBufferUpdate> &updates = m_ReplayCPUBufferUpdates[GetResID(buffer)];
    updates.append(m_PendingReplayCPUBufferUpdates);
    m_PendingReplayCPUBufferUpdates.clear();
  }
}

bool WrappedMTLDevice::ApplyFutureSharedAliasCPUUpdate(const CPUBufferUpdate &update)
{
  auto future = m_DescriptorFrameBuffers.find(update.buffer);
  if(m_DescriptorCoverage < 24 || future == m_DescriptorFrameBuffers.end() ||
     (future->second.options & 0xf0ULL) != MTL::ResourceStorageModeShared ||
     update.offset > future->second.length ||
     update.data.size() > future->second.length - update.offset) return false;
  // A partial tail may precede this resource's birth even though its submission
  // snapshot contains CPU writes made later. A standalone allocation cannot affect
  // earlier GPU work; a placement allocation can affect still-live physical aliases.
  if(future->second.heap == ResourceId()) return true;
  bool futureTable = false;
  for(const auto &table : m_DescriptorTables)
    futureTable |= table.buffer == update.buffer;
  auto parent = GetResourceManager()->GetResource(future->second.heap, true);
  MTL::Heap *heap = parent && parent->m_Type == eResHeap ? Unwrap((WrappedMTLHeap *)parent) : NULL;
  if(!heap || heap->storageMode() != MTL::StorageModeShared ||
     heap->hazardTrackingMode() != MTL::HazardTrackingModeTracked ||
     future->second.offset > heap->size() || update.offset > heap->size() - future->second.offset ||
     update.data.size() > heap->size() - future->second.offset - update.offset) return false;
  const uint64_t start = future->second.offset + update.offset, end = start + update.data.size();
  if(futureTable)
  {
    if(m_DescriptorCoverage < 65) return false;
    bool validatedAlias = false;
    for(const auto &pair : m_ValidatedDescriptorBackingAliases)
      validatedAlias |= pair.second == update.buffer;
    if(!validatedAlias) return false;
    // A submission snapshot can include a later-created descriptor table. Do
    // not materialize that future logical identity or write its captured VAs
    // into an earlier backing. The preflight alias proof retires every old
    // slot and closes its consumers at the later birth. Defer these bytes only
    // when every currently overlapping backing has that exact proof and all
    // its GPU consumers have actually completed before this submission.
    for(const BufferDescription &description : GetReplay()->GetBuffers())
    {
      auto object = GetResourceManager()->GetResource(description.resourceId, true);
      MTL::Buffer *native = object && object->m_Type == eResBuffer && object->m_Real ?
          Unwrap((WrappedMTLBuffer *)object) : NULL;
      if(!native || native->heap() != heap ||
         IsReplayResourceAliasable(description.resourceId) ||
         start >= native->heapOffset() + native->length() ||
         native->heapOffset() >= end) continue;
      const auto proof = m_DescriptorBackingAliasConsumers.find(
          make_rdcpair(description.resourceId, update.buffer));
      if(proof == m_DescriptorBackingAliasConsumers.end()) return false;
      for(ResourceId consumer : proof->second)
      {
        const auto state = m_ReplayCommandBuffers.find(consumer);
        if(state == m_ReplayCommandBuffers.end() || !state->second.buffer ||
           !state->second.committed ||
           Unwrap(state->second.buffer)->status() != MTL::CommandBufferStatusCompleted ||
           Unwrap(state->second.buffer)->error()) return false;
      }
    }
    if(!Process::GetEnvVariable("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT").empty())
      fprintf(stderr, "Metal deferred future descriptor snapshot after completed alias consumers: buffer=%s offset=%llu bytes=%zu\n",
              ToStr(update.buffer).c_str(), (unsigned long long)update.offset, update.data.size());
    return true;
  }
  for(const BufferDescription &description : GetReplay()->GetBuffers())
  {
    auto object = GetResourceManager()->GetResource(description.resourceId, true);
    MTL::Buffer *target = object && object->m_Type == eResBuffer ? Unwrap((WrappedMTLBuffer *)object) : NULL;
    if(!target || target->heap() != heap || IsReplayResourceAliasable(description.resourceId)) continue;
    if(target->storageMode() != MTL::StorageModeShared || !target->contents()) return false;
    const uint64_t begin = RDCMAX(start, uint64_t(target->heapOffset()));
    const uint64_t limit = RDCMIN(end, uint64_t(target->heapOffset()) + target->length());
    if(begin >= limit) continue;
    for(const auto &table : m_DescriptorTables)
      if(table.buffer == description.resourceId) return false;
    const uint64_t offset = begin - target->heapOffset();
    bytebuf data(update.data.data() + begin - start, size_t(limit - begin));
    memcpy((byte *)target->contents() + offset, data.data(), data.size());
    if(!ApplyDescriptorCPUUpdate(description.resourceId, offset, data) ||
       !GetReplay()->RestoreArgumentBufferResources(description.resourceId)) return false;
  }
  return true;
}

bool WrappedMTLDevice::ApplyReplayCPUBufferUpdates(WrappedMTLCommandBuffer *buffer)
{
  if(!buffer || !IsActiveReplaying(m_State))
    return buffer != NULL;
  auto state = m_ReplayCommandBuffers.find(GetResID(buffer));
  if(state == m_ReplayCommandBuffers.end() || state->second.buffer != buffer)
    return false;
  if(state->second.cpuUpdatesApplied)
    return true;
  if(m_DescriptorCoverage >= 12)
    for(const auto &entry : m_ReplayCommandBuffers)
      if(entry.second.buffer && entry.second.buffer != buffer &&
         IsReplayCommandBufferCommitted(entry.second.buffer) &&
         !WaitReplayCommandBuffer(entry.second.buffer, "CPU snapshot restoration")) return false;
  auto it = m_ReplayCPUBufferUpdates.find(GetResID(buffer));
  if(it != m_ReplayCPUBufferUpdates.end())
  {
    for(const CPUBufferUpdate &update : it->second)
    {
      WrappedMTLObject *object = GetResourceManager()->GetResource(update.buffer, true);
      if(!object || !object->m_Real)
      {
        if(!ApplyFutureSharedAliasCPUUpdate(update))
        {
          if(!Process::GetEnvVariable("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT").empty())
            fprintf(stderr, "Metal submission CPU snapshot future alias failed: command=%s buffer=%s offset=%llu bytes=%zu\n",
                    ToStr(GetResID(buffer)).c_str(), ToStr(update.buffer).c_str(),
                    (unsigned long long)update.offset, update.data.size());
          return false;
        }
        continue;
      }
      if(object->m_Type != eResBuffer) return false;
      MTL::Buffer *resource = Unwrap((WrappedMTLBuffer *)object);
      if(resource->storageMode() != MTL::StorageModeShared || !resource->contents() ||
         update.offset > resource->length() ||
         update.data.size() > resource->length() - update.offset)
        return false;
      memcpy((byte *)resource->contents() + update.offset, update.data.data(), update.data.size());
      if(!ApplyDescriptorCPUUpdate(update.buffer, update.offset, update.data))
      {
        if(!Process::GetEnvVariable("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT").empty())
          fprintf(stderr, "Metal submission CPU snapshot descriptor overlay failed: command=%s buffer=%s offset=%llu bytes=%zu\n",
                  ToStr(GetResID(buffer)).c_str(), ToStr(update.buffer).c_str(),
                  (unsigned long long)update.offset, update.data.size());
        return false;
      }
      if(!GetReplay()->RestoreArgumentBufferResources(update.buffer))
        return false;
    }
  }
  if(m_DescriptorCoverage >= 25)
    for(const auto &slot : m_DescriptorSlotShadow)
    {
      if(!slot.second.live || slot.second.gpuExpected ||
         (m_DescriptorCoverage>=63 && slot.second.data.empty() &&
          m_DescriptorGPUWrittenBuffers.count(slot.first.first)) ||
         IsDescriptorPreludeRetirement(slot.first, slot.second)) continue;
      auto object = GetResourceManager()->GetResource(slot.first.first, true);
      MTL::Buffer *native = object && object->m_Type == eResBuffer ? Unwrap((WrappedMTLBuffer *)object) : NULL;
      bytebuf patched;
      if(!native || !native->contents() || slot.first.second > native->length() ||
         24 > native->length() - slot.first.second || !PatchDescriptorSlot(slot.second, patched) ||
         memcmp((byte *)native->contents() + slot.first.second, patched.data(), 24))
      {
        RDCERR("Metal live descriptor bytes do not match the logical source before submission");
        return false;
      }
    }
  state->second.cpuUpdatesApplied = true;
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

bool WrappedMTLDevice::RestoreReplayPrivateBufferInitialContents()
{
  // D3D12 and Vulkan restore GPU-local initial state before executing the frame. Metal's
  // Initial Contents chunks were only cached; contents() cannot write a Private buffer.
  // Reuse a bounded Shared staging buffer so a large frame does not duplicate all captured
  // Private bytes in GPU-visible allocation. Wait before reusing staging storage.
  static constexpr uint64_t stagingCapacity = 16 * 1024 * 1024;
  MTL::Buffer *staging = NULL;
  MTL::CommandBuffer *command = NULL;
  MTL::BlitCommandEncoder *blit = NULL;
  NS::AutoreleasePool *pool = NULL;
  uint64_t staged = 0;
  uint64_t total = 0;
  uint32_t batches = 0;

  auto flush = [&]() -> bool {
    if(!command)
      return true;
    blit->endEncoding();
    command->commit();
    command->waitUntilCompleted();
    NS::Error *error = command->error();
    const MTL::CommandBufferStatus status = command->status();
    if(status != MTL::CommandBufferStatusCompleted)
      RDCERR("Metal Private initial contents upload failed: status=%u error=%s",
             (uint32_t)status,
             error ? error->localizedDescription()->utf8String() : "none");
    command = NULL;
    blit = NULL;
    staged = 0;
    batches++;
    pool->drain();
    pool = NULL;
    return status == MTL::CommandBufferStatusCompleted;
  };

  for(const auto &initial : m_ReplayBufferInitialContents)
  {
    if(IsFramePlacementResource(initial.first))
      continue;
    WrappedMTLObject *object = GetResourceManager()->GetResource(initial.first, true);
    if(!object || object->m_Type != eResBuffer || !object->m_Real)
    {
      flush();
      if(staging) staging->release();
      RDCERR("Missing Metal initial buffer %s", ToStr(initial.first).c_str());
      return false;
    }
    MTL::Buffer *destination = Unwrap((WrappedMTLBuffer *)object);
    if(destination->storageMode() != MTL::StorageModePrivate)
      continue;
    if(initial.second.size() != destination->length())
    {
      flush();
      if(staging) staging->release();
      RDCERR("Invalid Metal Private initial buffer length %s", ToStr(initial.first).c_str());
      return false;
    }
    if(!staging)
    {
      staging = Unwrap(this)->newBuffer(stagingCapacity, MTL::ResourceStorageModeShared);
      if(!staging || !staging->contents())
      {
        if(staging) staging->release();
        RDCERR("Couldn't allocate Metal Private initial contents staging buffer");
        return false;
      }
    }
    for(uint64_t offset = 0; offset < initial.second.size();)
    {
      if(staged == stagingCapacity && !flush())
      {
        staging->release();
        return false;
      }
      if(!command)
      {
        pool = NS::AutoreleasePool::alloc()->init();
        command = m_mtlCommandQueue->commandBuffer();
        blit = command ? command->blitCommandEncoder() : NULL;
        if(!blit)
        {
          if(pool) pool->drain();
          staging->release();
          RDCERR("Couldn't create Metal Private initial contents blit encoder");
          return false;
        }
      }
      const uint64_t size = RDCMIN(uint64_t(initial.second.size()) - offset,
                                   stagingCapacity - staged);
      memcpy((byte *)staging->contents() + staged, initial.second.data() + offset, size);
      blit->copyFromBuffer(staging, staged, destination, offset, size);
      staged += size;
      offset += size;
      total += size;
    }
  }
  const bool success = flush();
  if(staging) staging->release();
  if(!Process::GetEnvVariable("RENDERDOC_METAL_TRACE_INITIAL_PRIVATE").empty())
    fprintf(stderr, "Metal Private initial contents: bytes=%llu batches=%u success=%d\n",
            (unsigned long long)total, batches, success ? 1 : 0);
  return success;
}

bool WrappedMTLDevice::ReplayCPUBufferUpdate(WrappedMTLBuffer *wrapped, uint64_t start,
                                            const bytebuf &data, bool submissionSnapshot)
{
  // Commit-time Shared snapshots belong to an existing, not-yet-committed submission.
  // Explicit descriptor CPU annotations are separate CPU events and may precede encoding.
  if(submissionSnapshot && m_DescriptorCoverage >= 51)
  {
    // Encoding can switch away from the submission whose snapshot follows now.
    // Select the validated owner instead of attributing bytes to the last encoder.
    const auto owner = m_DescriptorSubmissionSnapshotOwners.find(m_CurChunkOffset);
    if(owner == m_DescriptorSubmissionSnapshotOwners.end()) return false;
    const auto state = m_ReplayCommandBuffers.find(owner->second);
    if(state == m_ReplayCommandBuffers.end() || !CanEncodeReplayEvent(state->second.buffer)) return false;
  }
  if(submissionSnapshot && (!m_ReplayCommandBuffer ||
      IsReplayCommandBufferCommitted(m_ReplayCommandBuffer))) return false;
  if(!wrapped || wrapped->m_Type != eResBuffer || !Unwrap(wrapped))
    return false;
  MTL::Buffer *buffer = Unwrap(wrapped);
  if(buffer->storageMode() != MTL::StorageModeShared || !buffer->contents() || data.empty() ||
     start > buffer->length() || data.size() > buffer->length() - start)
    return false;
  if(m_DescriptorCoverage >= 12)
  {
    // As in D3D12/Vulkan submit replay, serialize CPU snapshot restoration against
    // previously committed GPU work. Preflight proves one queue and excludes waits on event/fence
    // dependencies in a future submission, so these waits cannot target reservations.
    for(const auto &entry : m_ReplayCommandBuffers)
      if(entry.second.buffer && IsReplayCommandBufferCommitted(entry.second.buffer) &&
         !WaitReplayCommandBuffer(entry.second.buffer, "CPU snapshot restoration")) return false;
  }
  if(IsLoading(m_State))
  {
    ResourceId id = GetResID(wrapped);
    if(m_ReplayBufferInitialContents.count(id) == 0)
      m_ReplayBufferInitialContents[id] = bytebuf((byte *)buffer->contents(), buffer->length());
    m_ReplayCPUUpdatedBuffers.insert(id);
    CPUBufferUpdate update = {id, start, data};
    m_PendingReplayCPUBufferUpdates.push_back(update);
    memcpy((byte *)buffer->contents() + start, data.data(), data.size());
    if(!ApplyDescriptorCPUUpdate(id, start, data))
      return false;
    if(!GetReplay()->RestoreArgumentBufferResources(id))
      return false;
  }
  // Active replay uses the loading pass's submission-owned snapshot before the corresponding
  // command buffer is committed. The in-stream update chunk only validates its payload here.
  return true;
}

bool WrappedMTLDevice::ResetReplayCPUUpdatedBuffers()
{
  if(m_DescriptorCoverage >= 4)
  {
    m_DescriptorSlotShadow = m_DescriptorSlotInitial;
    m_DescriptorInlineShadow.clear();
    m_DescriptorGPUCopyExpected.clear();
    m_DescriptorDispatches.clear();
  }
  for(ResourceId id : m_ReplayCPUUpdatedBuffers)
  {
    // Frame-created placement buffers are released before each seek and recreated by their
    // creation chunk. Their cached CPU writes are applied at command-buffer submission, so
    // there is no frame-start native allocation to restore here.
    if(IsFramePlacementResource(id))
      continue;
    WrappedMTLObject *object = GetResourceManager()->GetResource(id, true);
    if(!object || object->m_Type != eResBuffer || !object->m_Real)
    {
      RDCERR("Metal Shared reset missing buffer %s (object=%p type=%u real=%p framePlacement=%u snapshot=%u)",
             ToStr(id).c_str(), object, object ? (uint32_t)object->m_Type : 0,
             object ? object->m_Real : NULL, IsFramePlacementResource(id) ? 1U : 0U,
             m_ReplayBufferInitialContents.count(id) ? 1U : 0U);
      return false;
    }
    MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)object);
    auto it = m_ReplayBufferInitialContents.find(id);
    if(it == m_ReplayBufferInitialContents.end() || buffer->storageMode() != MTL::StorageModeShared ||
       !buffer->contents() || it->second.size() != buffer->length())
    {
      RDCERR("Metal Shared reset invalid buffer %s (snapshot=%llu mode=%u contents=%p length=%llu)",
             ToStr(id).c_str(),
             it == m_ReplayBufferInitialContents.end() ? 0ULL : (uint64_t)it->second.size(),
             (uint32_t)buffer->storageMode(),
             buffer->storageMode() == MTL::StorageModeShared ? buffer->contents() : NULL,
             (uint64_t)buffer->length());
      return false;
    }
    memcpy(buffer->contents(), it->second.data(), it->second.size());
    if(m_DescriptorCoverage >= 4 && !OverlayDescriptorSlotBuffer(id, true))
      return false;
    if(m_DescriptorRawContents.count(id))
    {
      m_DescriptorRawContents[id] = it->second;
      if(!RestoreDescriptorTable(id, it->second))
        return false;
    }
    if(!GetReplay()->RestoreArgumentBufferResources(id))
    {
      RDCERR("Metal Shared reset argument buffer relocation failed for %s", ToStr(id).c_str());
      return false;
    }
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

void WrappedMTLDevice::RecordCaptureSubmission(MTL::CommandBuffer *buffer)
{
  SCOPED_LOCK(m_CapturePendingGPULock);
  for(size_t i = m_CapturePendingGPU.size(); i > 0; --i)
  {
    MTL::CommandBuffer *old = m_CapturePendingGPU[i - 1];
    if(old->status() == MTL::CommandBufferStatusCompleted)
    {
      old->release();
      m_CapturePendingGPU.erase(i - 1);
    }
  }
  m_CapturePendingGPU.push_back(buffer->retain());
}

bool WrappedMTLDevice::WaitForCaptureSubmittedGPU()
{
  {
    SCOPED_LOCK(m_CaptureCommandBuffersLock);
    for(const auto &queue : m_CaptureCommandBuffersEnqueued)
    {
      bool reservation = false;
      for(MetalResourceRecord *record : queue.second)
      {
        reservation |= record->cmdInfo->status == MetalCmdBufferStatus::Enqueued;
        if(reservation && record->cmdInfo->status == MetalCmdBufferStatus::Committed)
        {
          // Waiting under the cutoff would require a later commit blocked by that
          // same cutoff. Fail this boundary without queueing a marker or GPU work.
          RDCERR("Cannot snapshot Metal initial state: committed work follows an uncommitted enqueue reservation");
          return false;
        }
      }
    }
  }
  rdcarray<MTL::CommandBuffer *> submitted;
  {
    SCOPED_LOCK(m_CapturePendingGPULock);
    for(MTL::CommandBuffer *buffer : m_CapturePendingGPU)
      submitted.push_back(buffer->retain());
  }
  const bool trace = !Process::GetEnvVariable("RENDERDOC_METAL_TRACE_CAPTURE_WAITS").empty();
  bool success = true;
  if(trace) fprintf(stderr, "Metal capture initial GPU wait: submitted=%zu\n", submitted.size());
  for(MTL::CommandBuffer *buffer : submitted)
  {
    // waitUntilCompleted includes application completion handlers. They may need
    // the transition read lock held by this start cutoff, so wait only for Metal's
    // final GPU status here. All previously committed queues are included, and no
    // new marker is queued behind an uncommitted enqueue reservation.
    while(buffer->status() != MTL::CommandBufferStatusCompleted &&
          buffer->status() != MTL::CommandBufferStatusError)
      Threading::Sleep(1);
    if(buffer->status() == MTL::CommandBufferStatusError)
    {
      RDCERR("Metal GPU submission failed before initial snapshot");
      success = false;
    }
    buffer->release();
  }
  if(trace) fprintf(stderr, "Metal capture initial GPU wait complete: success=%d\n", success ? 1 : 0);
  return success;
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
  m_FailedCaptureStart = false;

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

    // D3D12/Vulkan wait application submissions before snapshotting initial state.
    // Shared storage exposes CPU bytes, but does not make unfinished GPU writes visible.
    if(!WaitForCaptureSubmittedGPU())
    {
      fprintf(stderr, "Metal controlled capture start failed while waiting for application submissions\n");
      m_FailedCaptureStart = true;
      m_CapturedFrames.pop_back();
      return;
    }

    GetResourceManager()->PrepareInitialContents();

    RDCDEBUG("Attempting capture");
    m_FrameCaptureRecord->DeleteChunks();
    SnapshotDescriptorHistory();
    m_CapturedBackbuffer.store(NULL);
    m_State = CaptureState::ActiveCapturing;
    ++m_CaptureEpoch;
  }

  GetResourceManager()->MarkResourceFrameReferenced(GetResID(this), eFrameRef_Read);
  GetResourceManager()->RefGPUIdentityResources();
  for(const DescriptorTable &table : m_DescriptorTables)
    if(GetResourceManager()->HasResource(table.buffer) &&
       !m_CaptureRetiredDescriptorBackings.count(table.buffer))
      GetResourceManager()->MarkResourceFrameReferenced(table.buffer, eFrameRef_Read);

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
    return !m_FailedCaptureStart;

  RDCLOG("Finished capture, Frame %u", m_CapturedFrames.back().frameNumber);

  ResourceId bbId;
  WrappedMTLTexture *backBuffer = NULL;
  bool pendingReservation = false;
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);
    {
      SCOPED_LOCK(m_CaptureCommandBuffersLock);
      for(const auto &queue : m_CaptureCommandBuffersEnqueued)
        for(MetalResourceRecord *record : queue.second)
        {
          pendingReservation |= record->cmdInfo->status == MetalCmdBufferStatus::Committed &&
                                record->cmdInfo->captureCommitEpoch == m_CaptureEpoch;
        }
    }
    if(!pendingReservation)
      backBuffer = m_CapturedBackbuffer.exchange(NULL);
    if(backBuffer)
    {
      bbId = GetResID(backBuffer);
      GetResourceManager()->MarkResourceFrameReferenced(bbId, eFrameRef_Read);
      EndCaptureFrame(bbId);
      m_State = CaptureState::BackgroundCapturing;
    }
  }
  if(bbId == ResourceId())
  {
    fprintf(stderr, "Metal controlled capture ended without a backbuffer (pending queue reservation=%d)\n", pendingReservation ? 1 : 0);
    if(pendingReservation)
      RDCERR("Incomplete Metal capture: committed work follows an uncommitted queue reservation");
    else
      RDCERR("Invalid Capture backbuffer; discarding controlled Metal capture");
    DiscardFrameCapture(devWnd);
    return false;
  }

  rdcarray<MTL::CommandBuffer *> submitted;
  {
    SCOPED_LOCK(m_CaptureCommandBuffersLock);
    for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
    {
      RDCASSERT(record->m_Type == eResCommandBuffer && record->cmdInfo &&
                record->cmdInfo->retainedNative);
      submitted.push_back(record->cmdInfo->retainedNative->retain());
    }
  }
  // Metal's wait also waits application CPU completion handlers. They may enqueue
  // or commit more work; never hold the capture tracking lock while calling it.
  for(MTL::CommandBuffer *buffer : submitted)
  {
    buffer->waitUntilCompleted();
    buffer->release();
  }
  if(submitted.empty())
    WaitForGPU();

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
    // Creation during this frame must remain in the frame stream. Resource records retain
    // the same chunk for later captures if the object survives beyond this frame.
    {
      SCOPED_LOCK(m_CapturedFrameResourcesLock);
      for(ResourceId id : m_CapturedFrameResources)
        if(MetalResourceRecord *record = GetResourceManager()->GetResourceRecord(id))
          record->DataWritten = true;
    }
    GetResourceManager()->InsertReferencedChunks(ser);
    GetResourceManager()->InsertInitialContentsChunks(ser);
    if(!Process::GetEnvVariable("RENDERDOC_METAL_CAPTURE_INDIRECT_ARGUMENTS").empty())
    {
      uint32_t count = 0;
      for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
        for(const auto &evidence : record->cmdInfo->indirectArguments)
          if(evidence.epoch == m_CaptureEpoch) count++;
      {
        SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_CaptureComputeIndirectArgumentsCount, 32);
        Serialise_CaptureComputeIndirectArgumentsCount(ser, count);
      }
      for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
        for(const auto &evidence : record->cmdInfo->indirectArguments)
        {
          if(evidence.epoch != m_CaptureEpoch) continue;
          rdcarray<uint32_t> groups;
          const auto &snapshot=evidence.readback.snapshot;
          if(!Process::GetEnvVariable("RENDERDOC_METAL_TRACE_INDIRECT_CAPTURE").empty())
            fprintf(stderr, "Metal indirect capture: encoder=%s ordinal=%u status=%u error=%p snapshot=%p marker=%08x\n",
                    ToStr(evidence.encoder).c_str(), evidence.ordinal,
                    (unsigned)record->cmdInfo->retainedNative->status(),
                    record->cmdInfo->retainedNative->error(), snapshot.get(),
                    snapshot && snapshot->contents() ? ((const uint32_t *)snapshot->contents())[3] : 0);
          if(record->cmdInfo->retainedNative->status() == MTL::CommandBufferStatusCompleted &&
             !record->cmdInfo->retainedNative->error() && snapshot && snapshot->contents() &&
             ((const uint32_t *)snapshot->contents())[3] == 0x52444349)
          {
            const uint32_t *saved=(const uint32_t *)snapshot->contents();
            groups.assign(saved,3);
          }
          SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_CaptureIndirectArguments, 128);
          Serialise_CaptureComputeIndirectArguments(ser,evidence.command,evidence.encoder,
              evidence.ordinal,evidence.buffer,evidence.offset,groups);
        }
    }
    if(CapturingRenderIndirectArguments())
    {
      uint32_t count=0;
      for(MetalResourceRecord *record:m_CaptureCommandBuffersSubmitted)
        for(const auto &evidence:record->cmdInfo->renderIndirectArguments)
          if(evidence.epoch==m_CaptureEpoch)count++;
      {
        SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_CaptureRenderIndirectArgumentsCount,32);
        Serialise_CaptureRenderIndirectArgumentsCount(ser,count);
      }
      for(MetalResourceRecord *record:m_CaptureCommandBuffersSubmitted)
        for(const auto &evidence:record->cmdInfo->renderIndirectArguments)
        {
          if(evidence.epoch!=m_CaptureEpoch)continue;
          rdcarray<uint32_t> arguments;
          if(record->cmdInfo->retainedNative->status()==MTL::CommandBufferStatusCompleted &&
             !record->cmdInfo->retainedNative->error() && evidence.readback.snapshot &&
             evidence.readback.snapshot->contents())
            arguments.assign((const uint32_t *)evidence.readback.snapshot->contents(),evidence.wordCount);
          SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_CaptureIndirectArguments,160);
          Serialise_CaptureRenderIndirectArguments(ser,evidence.command,evidence.encoder,evidence.pass,
              evidence.ordinal,evidence.buffer,evidence.offset,evidence.wordCount,evidence.writesDeclared,arguments);
        }
    }
    // This is an owned snapshot taken before ActiveCapturing, never a resource
    // record which can be appended by background retirement while writing the file.
    for(Chunk *chunk : m_DescriptorHistorySnapshot)
      chunk->Write(ser);

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
  ReleaseCapturedDirectPresentation();

  GetResourceManager()->ResetLastWriteTimes();
  GetResourceManager()->MarkUnwrittenResources();
  {
    SCOPED_LOCK(m_CapturedFrameResourcesLock);
    m_CapturedFrameResources.clear();
  }

  // TODO: handle memory resources in the resource manager

  GetResourceManager()->ClearReferencedResources();
  GetResourceManager()->FreeInitialContents();

  ClearDescriptorHistorySnapshot();

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
    m_CapturedBackbuffer.store(NULL);
    m_State = CaptureState::BackgroundCapturing;
  }

  CaptureClearSubmittedCmdBuffers();
  ReleaseCapturedDirectPresentation();

  GetResourceManager()->MarkUnwrittenResources();
  {
    SCOPED_LOCK(m_CapturedFrameResourcesLock);
    m_CapturedFrameResources.clear();
  }

  // TODO: handle memory resources in the resource manager

  GetResourceManager()->ClearReferencedResources();
  GetResourceManager()->FreeInitialContents();
  ClearDescriptorHistorySnapshot();

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
    GetResourceManager()->AddSharedHeapBufferReferences(refIDs);
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
        // Also retain the conservative indirect dependency in the capture resource
        // graph. A frame-created buffer may only be reached through useHeaps.
        record->AddParent(refRecord);
        record->MarkResourceFrameReferenced(id, eFrameRef_Read);
        // In the explicit provenance contract, GPU-written descriptor allocations are
        // captured only through application-declared CPU writes. A Shared-memory diff
        // cannot distinguish completed GPU output from a CPU write.
        if(m_DescriptorGPUWrittenBuffers.count(id))
          continue;
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

void WrappedMTLDevice::CaptureCmdBufSubmit(MetalResourceRecord *record,
                                           rdcarray<PendingCapturePresent> &presents)
{
  RDCASSERTEQUAL(record->cmdInfo->status, MetalCmdBufferStatus::Submitted);
  RDCASSERT(IsCaptureMode(m_State));
  WrappedMTLCommandBuffer *commandBuffer = (WrappedMTLCommandBuffer *)(record->m_Resource);
  if(record->cmdInfo->presented)
  {
    MetalCmdBufferRecordingInfo *info = record->cmdInfo;
    PendingCapturePresent present;
    present.record = record;
    present.backBuffer = info->backBuffer;
    present.layer = info->outputLayer ? info->outputLayer->retain() : NULL;
    present.textureProxy = info->retainedPresentedTextureProxy
                               ? info->retainedPresentedTextureProxy->retain() : NULL;
    present.textureNative = info->retainedPresentedTextureNative
                                ? info->retainedPresentedTextureNative->retain() : NULL;
    present.drawable = info->retainedDrawable ? info->retainedDrawable->retain() : NULL;
    present.commitEpoch = info->captureCommitEpoch;
    record->AddRef();
    presents.push_back(present);
  }
  if(IsActiveCapturing(m_State) && record->cmdInfo->captureCommitEpoch == m_CaptureEpoch)
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
    ReleaseCapturedCommandBuffer(record);
  }
  // In background or active capture mode the record reference is incremented in
  // CaptureCmdBufEnqueue
  record->Delete(GetResourceManager());
}

void WrappedMTLDevice::CaptureCmdBufCommit(MetalResourceRecord *cbRecord,
                                           rdcarray<PendingCapturePresent> &presents)
{
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  if(cbRecord->cmdInfo->status != MetalCmdBufferStatus::Enqueued)
    CaptureCmdBufEnqueue(cbRecord);

  RDCASSERTEQUAL(cbRecord->cmdInfo->status, MetalCmdBufferStatus::Enqueued);
  cbRecord->cmdInfo->status = MetalCmdBufferStatus::Committed;
  cbRecord->cmdInfo->captureCommitEpoch = IsActiveCapturing(m_State) ? m_CaptureEpoch : 0;

  auto &enqueued = m_CaptureCommandBuffersEnqueued[GetResID(cbRecord->cmdInfo->queue)];
  size_t countSubmitted = 0;
  for(MetalResourceRecord *record : enqueued)
  {
    if(record->cmdInfo->status == MetalCmdBufferStatus::Committed)
    {
      record->cmdInfo->status = MetalCmdBufferStatus::Submitted;
      ++countSubmitted;
      CaptureCmdBufSubmit(record, presents);
      continue;
    }
    break;
  };
  enqueued.erase(0, countSubmitted);
}

void WrappedMTLDevice::ProcessCapturePresents(rdcarray<PendingCapturePresent> &presents)
{
  for(PendingCapturePresent &present : presents)
  {
    AdvanceFrame();
    Present(present.backBuffer, present.layer, present.commitEpoch);
    {
      SCOPED_READLOCK(m_CapTransitionLock);
      SCOPED_LOCK(m_CaptureCommandBuffersLock);
      MetalCmdBufferRecordingInfo *info = present.record->cmdInfo;
      // Retain only the selected thumbnail acquisition through capture end.
      if(info->retainedDrawable && info->backBuffer != m_CapturedBackbuffer.load())
      {
        info->retainedDrawable->release();
        info->retainedDrawable = NULL;
      }
    }
    if(present.textureNative) present.textureNative->release();
    if(present.textureProxy) present.textureProxy->release();
    if(present.drawable) present.drawable->release();
    if(present.layer) present.layer->release();
    present.record->Delete(GetResourceManager());
  }
  presents.clear();
}

void WrappedMTLDevice::CaptureCmdBufEnqueue(MetalResourceRecord *cbRecord)
{
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  RDCASSERTEQUAL(cbRecord->cmdInfo->status, MetalCmdBufferStatus::Unknown);
  cbRecord->cmdInfo->status = MetalCmdBufferStatus::Enqueued;
  cbRecord->AddRef();
  auto &enqueued = m_CaptureCommandBuffersEnqueued[GetResID(cbRecord->cmdInfo->queue)];
  enqueued.push_back(cbRecord);

  RDCDEBUG("Enqueing CommandBufferRecord %s %d", ToStr(cbRecord->GetResourceID()).c_str(),
           enqueued.count());
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

void WrappedMTLDevice::Present(WrappedMTLTexture *backBuffer, CA::MetalLayer *outputLayer,
                               uint64_t commitEpoch)
{
  {
    SCOPED_LOCK(m_CapturePotentialBackBuffersLock);
    if(m_CapturePotentialBackBuffers.count(backBuffer) == 0)
    {
      RDCERR("Capture ignoring Present called on unknown backbuffer");
      return;
    }
  }

  DeviceOwnedWindow devWnd(this, outputLayer);

  bool activeWindow = RenderDoc::Inst().IsActiveWindow(devWnd);

  RenderDoc::Inst().AddActiveDriver(RDCDriver::Metal, true);

  if(!activeWindow)
    return;

  if(IsActiveCapturing(m_State) && commitEpoch == m_CaptureEpoch)
  {
    WrappedMTLTexture *empty = NULL;
    m_CapturedBackbuffer.compare_exchange_strong(empty, backBuffer);

    if(!m_AppControlledCapture)
      RenderDoc::Inst().EndFrameCapture(devWnd);
  }

  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && IsBackgroundCapturing(m_State))
  {
    RenderDoc::Inst().StartFrameCapture(devWnd);

    if(IsActiveCapturing(m_State))
    {
      m_AppControlledCapture = false;
      m_CapturedFrames.back().frameNumber = m_FrameCounter;
    }
  }
}

void WrappedMTLDevice::PresentDrawable(CA::MetalDrawable *drawable)
{
  if(!IsCaptureMode(m_State)) return;
  MetalDrawableInfo info = UnregisterDrawableInfo(drawable);
  if(!info.texture || !info.mtlLayer) return;
  {
    SCOPED_LOCK(m_CapturePotentialBackBuffersLock);
    if(m_CapturePotentialBackBuffers.count(info.texture) == 0) return;
  }
  AdvanceFrame();
  DeviceOwnedWindow window(this, info.mtlLayer);
  RenderDoc::Inst().AddActiveDriver(RDCDriver::Metal, true);
  if(!RenderDoc::Inst().IsActiveWindow(window)) return;
  if(IsActiveCapturing(m_State))
  {
    {
      SCOPED_READLOCK(m_CapTransitionLock);
      SCOPED_LOCK(m_CapturedPresentationLock);
      if(IsActiveCapturing(m_State) && !m_CapturedBackbuffer.load())
      {
        m_CapturedDirectTextureNative = Unwrap(info.texture)->retain();
        m_CapturedDirectTextureProxy = ((NS::Object *)info.texture)->retain();
        m_CapturedDirectDrawable = drawable->retain();
        m_CapturedBackbuffer.store(info.texture);
      }
    }
    if(!m_AppControlledCapture && !m_DirectPresentEndPending.exchange(true))
    {
      // CAMetalDrawable::present may run inside the command buffer's scheduled
      // callback. EndFrameCapture waits for submitted command buffers, including
      // that callback's buffer, so finish on another queue after returning here.
      struct PendingEnd
      {
        WrappedMTLDevice *device;
        DeviceOwnedWindow window;
      };
      PendingEnd *pending = new PendingEnd{this, window};
      dispatch_async_f(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), pending,
                       [](void *context) {
                         PendingEnd *end = (PendingEnd *)context;
                         RenderDoc::Inst().EndFrameCapture(end->window);
                         end->device->m_DirectPresentEndPending.store(false);
                         delete end;
                       });
    }
  }
  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && IsBackgroundCapturing(m_State))
  {
    RenderDoc::Inst().StartFrameCapture(window);
    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = m_FrameCounter;
  }
}

void WrappedMTLDevice::CaptureClearSubmittedCmdBuffers()
{
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  for(MetalResourceRecord *record : m_CaptureCommandBuffersSubmitted)
  {
    ReleaseCapturedCommandBuffer(record);
    record->Delete(GetResourceManager());
  }

  m_CaptureCommandBuffersSubmitted.clear();
}

void WrappedMTLDevice::ReleaseCapturedCommandBuffer(MetalResourceRecord *record)
{
  RDCASSERT(record && record->m_Type == eResCommandBuffer && record->cmdInfo);
  MetalCmdBufferRecordingInfo *info = record->cmdInfo;
  MTL::CommandBuffer *native = info->retainedNative;
  NS::Object *proxy = info->retainedProxy;
  info->retainedNative = NULL;
  info->retainedProxy = NULL;
  NS::Object *textureProxy = info->retainedPresentedTextureProxy;
  MTL::Texture *textureNative = info->retainedPresentedTextureNative;
  MTL::Drawable *drawable = info->retainedDrawable;
  info->retainedPresentedTextureProxy = NULL;
  info->retainedPresentedTextureNative = NULL;
  info->retainedDrawable = NULL;
  if(native)
    native->release();
  // The proxy's dealloc may delete the wrapper and drop the record's application reference.
  // Callers keep their own record reference until after this release returns.
  if(proxy)
    proxy->release();
  if(textureNative) textureNative->release();
  if(textureProxy) textureProxy->release();
  if(drawable) drawable->release();
}

void WrappedMTLDevice::ReleaseCapturedDirectPresentation()
{
  SCOPED_LOCK(m_CapturedPresentationLock);
  if(m_CapturedDirectTextureNative) m_CapturedDirectTextureNative->release();
  if(m_CapturedDirectTextureProxy) m_CapturedDirectTextureProxy->release();
  if(m_CapturedDirectDrawable) m_CapturedDirectDrawable->release();
  m_CapturedDirectTextureNative = NULL;
  m_CapturedDirectTextureProxy = NULL;
  m_CapturedDirectDrawable = NULL;
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
  const NS::UInteger drawableID = caMtlDrawable->drawableID();
  {
    SCOPED_LOCK(m_CaptureDrawablesLock);
    auto existing = m_CaptureDrawableInfos.find(caMtlDrawable);
    if(existing != m_CaptureDrawableInfos.end())
    {
      if(existing->second.drawableID == drawableID)
        return;
      // CAMetalLayer can recycle a drawable pointer when an earlier acquisition was
      // not presented through this command buffer. The pointer now denotes the new
      // acquisition; retain its current texture rather than the stale lookup entry.
      RDCWARN("Replacing stale Metal drawable registration %p (ID %llu -> %llu)",
              caMtlDrawable, (uint64_t)existing->second.drawableID, (uint64_t)drawableID);
      m_CaptureDrawableInfos.erase(existing);
    }
  }
  MetalDrawableInfo drawableInfo;
  drawableInfo.mtlLayer = caMtlDrawable->layer();
  drawableInfo.texture = WrapDrawableTexture(realTexture);
  drawableInfo.drawableID = drawableID;
  {
    SCOPED_LOCK(m_CaptureDrawablesLock);
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
  SCOPED_LOCK(m_CaptureDrawablesLock);
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
