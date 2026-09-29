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

#pragma once

#include "api/replay/rdcstr.h"
#include "common/common.h"
#include "common/timing.h"
#include "official/metal-cpp.h"
#include "serialise/serialiser.h"
#include "metal_resources.h"
#include "metal_types.h"

enum class MetalChunk : uint32_t
{
  MTLCreateSystemDefaultDevice = (uint32_t)SystemChunk::FirstDriverChunk,
  MTLDevice_newCommandQueue,
  MTLDevice_newCommandQueueWithMaxCommandBufferCount,
  MTLDevice_newHeapWithDescriptor,
  MTLDevice_newBufferWithLength,
  MTLDevice_newBufferWithBytes,
  MTLDevice_newBufferWithBytesNoCopy,
  MTLDevice_newDepthStencilStateWithDescriptor,
  MTLDevice_newTextureWithDescriptor,
  MTLDevice_newTextureWithDescriptor_iosurface,
  MTLDevice_newTextureWithDescriptor_nextDrawable,
  MTLDevice_newSharedTextureWithDescriptor,
  MTLDevice_newSharedTextureWithHandle,
  MTLDevice_newSamplerStateWithDescriptor,
  MTLDevice_newDefaultLibrary,
  MTLDevice_newDefaultLibraryWithBundle,
  MTLDevice_newLibraryWithFile,
  MTLDevice_newLibraryWithURL,
  MTLDevice_newLibraryWithData,
  MTLDevice_newLibraryWithSource,
  MTLDevice_newLibraryWithStitchedDescriptor,
  MTLDevice_newRenderPipelineStateWithDescriptor,
  MTLDevice_newRenderPipelineStateWithDescriptor_options,
  MTLDevice_newComputePipelineStateWithFunction,
  MTLDevice_newComputePipelineStateWithFunction_options,
  MTLDevice_newComputePipelineStateWithDescriptor,
  MTLDevice_newFence,
  MTLDevice_newRenderPipelineStateWithTileDescriptor,
  MTLDevice_newArgumentEncoderWithArguments,
  MTLDevice_supportsRasterizationRateMapWithLayerCount,
  MTLDevice_newRasterizationRateMapWithDescriptor,
  MTLDevice_newIndirectCommandBufferWithDescriptor,
  MTLDevice_newEvent,
  MTLDevice_newSharedEvent,
  MTLDevice_newSharedEventWithHandle,
  MTLDevice_newCounterSampleBufferWithDescriptor,
  MTLDevice_newDynamicLibrary,
  MTLDevice_newDynamicLibraryWithURL,
  MTLDevice_newBinaryArchiveWithDescriptor,
  MTLLibrary_newFunctionWithName,
  MTLLibrary_newFunctionWithName_constantValues,
  MTLLibrary_newFunctionWithDescriptor,
  MTLLibrary_newIntersectionFunctionWithDescriptor,
  MTLFunction_newArgumentEncoderWithBufferIndex,
  MTLCommandQueue_commandBuffer,
  MTLCommandQueue_commandBufferWithDescriptor,
  MTLCommandQueue_commandBufferWithUnretainedReferences,
  MTLCommandBuffer_enqueue,
  MTLCommandBuffer_commit,
  MTLCommandBuffer_addScheduledHandler,
  MTLCommandBuffer_presentDrawable,
  MTLCommandBuffer_presentDrawable_atTime,
  MTLCommandBuffer_presentDrawable_afterMinimumDuration,
  MTLCommandBuffer_waitUntilScheduled,
  MTLCommandBuffer_addCompletedHandler,
  MTLCommandBuffer_waitUntilCompleted,
  MTLCommandBuffer_blitCommandEncoder,
  MTLCommandBuffer_renderCommandEncoderWithDescriptor,
  MTLCommandBuffer_computeCommandEncoderWithDescriptor,
  MTLCommandBuffer_blitCommandEncoderWithDescriptor,
  MTLCommandBuffer_computeCommandEncoder,
  MTLCommandBuffer_computeCommandEncoderWithDispatchType,
  MTLCommandBuffer_encodeWaitForEvent,
  MTLCommandBuffer_encodeSignalEvent,
  MTLCommandBuffer_parallelRenderCommandEncoderWithDescriptor,
  MTLCommandBuffer_resourceStateCommandEncoder,
  MTLCommandBuffer_resourceStateCommandEncoderWithDescriptor,
  MTLCommandBuffer_accelerationStructureCommandEncoder,
  MTLCommandBuffer_pushDebugGroup,
  MTLCommandBuffer_popDebugGroup,
  MTLTexture_setPurgeableState,
  MTLTexture_makeAliasable,
  MTLTexture_getBytes,
  MTLTexture_getBytes_slice,
  MTLTexture_replaceRegion,
  MTLTexture_replaceRegion_slice,
  MTLTexture_newTextureViewWithPixelFormat,
  MTLTexture_newTextureViewWithPixelFormat_subset,
  MTLTexture_newTextureViewWithPixelFormat_subset_swizzle,
  MTLTexture_newSharedTextureHandle,
  MTLTexture_remoteStorageTexture,
  MTLTexture_newRemoteTextureViewForDevice,
  MTLRenderPipelineState_functionHandleWithFunction,
  MTLRenderPipelineState_newVisibleFunctionTableWithDescriptor,
  MTLRenderPipelineState_newIntersectionFunctionTableWithDescriptor,
  MTLRenderPipelineState_newRenderPipelineStateWithAdditionalBinaryFunctions,
  MTLRenderCommandEncoder_endEncoding,
  MTLRenderCommandEncoder_insertDebugSignpost,
  MTLRenderCommandEncoder_pushDebugGroup,
  MTLRenderCommandEncoder_popDebugGroup,
  MTLRenderCommandEncoder_setRenderPipelineState,
  MTLRenderCommandEncoder_setVertexBytes,
  MTLRenderCommandEncoder_setVertexBuffer,
  MTLRenderCommandEncoder_setVertexBufferOffset,
  MTLRenderCommandEncoder_setVertexBuffers,
  MTLRenderCommandEncoder_setVertexTexture,
  MTLRenderCommandEncoder_setVertexTextures,
  MTLRenderCommandEncoder_setVertexSamplerState,
  MTLRenderCommandEncoder_setVertexSamplerState_lodclamp,
  MTLRenderCommandEncoder_setVertexSamplerStates,
  MTLRenderCommandEncoder_setVertexSamplerStates_lodclamp,
  MTLRenderCommandEncoder_setVertexVisibleFunctionTable,
  MTLRenderCommandEncoder_setVertexVisibleFunctionTables,
  MTLRenderCommandEncoder_setVertexIntersectionFunctionTable,
  MTLRenderCommandEncoder_setVertexIntersectionFunctionTables,
  MTLRenderCommandEncoder_setVertexAccelerationStructure,
  MTLRenderCommandEncoder_setViewport,
  MTLRenderCommandEncoder_setViewports,
  MTLRenderCommandEncoder_setFrontFacingWinding,
  MTLRenderCommandEncoder_setVertexAmplificationCount,
  MTLRenderCommandEncoder_setCullMode,
  MTLRenderCommandEncoder_setDepthClipMode,
  MTLRenderCommandEncoder_setDepthBias,
  MTLRenderCommandEncoder_setScissorRect,
  MTLRenderCommandEncoder_setScissorRects,
  MTLRenderCommandEncoder_setTriangleFillMode,
  MTLRenderCommandEncoder_setFragmentBytes,
  MTLRenderCommandEncoder_setFragmentBuffer,
  MTLRenderCommandEncoder_setFragmentBufferOffset,
  MTLRenderCommandEncoder_setFragmentBuffers,
  MTLRenderCommandEncoder_setFragmentTexture,
  MTLRenderCommandEncoder_setFragmentTextures,
  MTLRenderCommandEncoder_setFragmentSamplerState,
  MTLRenderCommandEncoder_setFragmentSamplerState_lodclamp,
  MTLRenderCommandEncoder_setFragmentSamplerStates,
  MTLRenderCommandEncoder_setFragmentSamplerStates_lodclamp,
  MTLRenderCommandEncoder_setFragmentVisibleFunctionTable,
  MTLRenderCommandEncoder_setFragmentVisibleFunctionTables,
  MTLRenderCommandEncoder_setFragmentIntersectionFunctionTable,
  MTLRenderCommandEncoder_setFragmentIntersectionFunctionTables,
  MTLRenderCommandEncoder_setFragmentAccelerationStructure,
  MTLRenderCommandEncoder_setBlendColor,
  MTLRenderCommandEncoder_setDepthStencilState,
  MTLRenderCommandEncoder_setStencilReferenceValue,
  MTLRenderCommandEncoder_setStencilFrontReferenceValue,
  MTLRenderCommandEncoder_setVisibilityResultMode,
  MTLRenderCommandEncoder_setColorStoreAction,
  MTLRenderCommandEncoder_setDepthStoreAction,
  MTLRenderCommandEncoder_setStencilStoreAction,
  MTLRenderCommandEncoder_setColorStoreActionOptions,
  MTLRenderCommandEncoder_setDepthStoreActionOptions,
  MTLRenderCommandEncoder_setStencilStoreActionOptions,
  MTLRenderCommandEncoder_drawPrimitives,
  MTLRenderCommandEncoder_drawPrimitives_instanced,
  MTLRenderCommandEncoder_drawPrimitives_instanced_base,
  MTLRenderCommandEncoder_drawPrimitives_indirect,
  MTLRenderCommandEncoder_drawIndexedPrimitives,
  MTLRenderCommandEncoder_drawIndexedPrimitives_instanced,
  MTLRenderCommandEncoder_drawIndexedPrimitives_instanced_base,
  MTLRenderCommandEncoder_drawIndexedPrimitives_indirect,
  MTLRenderCommandEncoder_textureBarrier,
  MTLRenderCommandEncoder_updateFence,
  MTLRenderCommandEncoder_waitForFence,
  MTLRenderCommandEncoder_setTessellationFactorBuffer,
  MTLRenderCommandEncoder_setTessellationFactorScale,
  MTLRenderCommandEncoder_drawPatches,
  MTLRenderCommandEncoder_drawPatches_indirect,
  MTLRenderCommandEncoder_drawIndexedPatches,
  MTLRenderCommandEncoder_drawIndexedPatches_indirect,
  MTLRenderCommandEncoder_setTileBytes,
  MTLRenderCommandEncoder_setTileBuffer,
  MTLRenderCommandEncoder_setTileBufferOffset,
  MTLRenderCommandEncoder_setTileBuffers,
  MTLRenderCommandEncoder_setTileTexture,
  MTLRenderCommandEncoder_setTileTextures,
  MTLRenderCommandEncoder_setTileSamplerState,
  MTLRenderCommandEncoder_setTileSamplerState_lodclamp,
  MTLRenderCommandEncoder_setTileSamplerStates,
  MTLRenderCommandEncoder_setTileSamplerStates_lodclamp,
  MTLRenderCommandEncoder_setTileVisibleFunctionTable,
  MTLRenderCommandEncoder_setTileVisibleFunctionTables,
  MTLRenderCommandEncoder_setTileIntersectionFunctionTable,
  MTLRenderCommandEncoder_setTileIntersectionFunctionTables,
  MTLRenderCommandEncoder_setTileAccelerationStructure,
  MTLRenderCommandEncoder_dispatchThreadsPerTile,
  MTLRenderCommandEncoder_setThreadgroupMemoryLength,
  MTLRenderCommandEncoder_useResource,
  MTLRenderCommandEncoder_useResource_stages,
  MTLRenderCommandEncoder_useResources,
  MTLRenderCommandEncoder_useResources_stages,
  MTLRenderCommandEncoder_useHeap,
  MTLRenderCommandEncoder_useHeap_stages,
  MTLRenderCommandEncoder_useHeaps,
  MTLRenderCommandEncoder_useHeaps_stages,
  MTLRenderCommandEncoder_executeCommandsInBuffer,
  MTLRenderCommandEncoder_executeCommandsInBuffer_indirect,
  MTLRenderCommandEncoder_memoryBarrierWithScope,
  MTLRenderCommandEncoder_memoryBarrierWithResources,
  MTLRenderCommandEncoder_sampleCountersInBuffer,
  MTLBuffer_setPurgeableState,
  MTLBuffer_makeAliasable,
  MTLBuffer_contents,
  MTLBuffer_didModifyRange,
  MTLBuffer_newTextureWithDescriptor,
  MTLBuffer_addDebugMarker,
  MTLBuffer_removeAllDebugMarkers,
  MTLBuffer_remoteStorageBuffer,
  MTLBuffer_newRemoteBufferViewForDevice,
  MTLBuffer_InternalModifyCPUContents,
  MTLBlitCommandEncoder_setLabel,
  MTLBlitCommandEncoder_endEncoding,
  MTLBlitCommandEncoder_insertDebugSignpost,
  MTLBlitCommandEncoder_pushDebugGroup,
  MTLBlitCommandEncoder_popDebugGroup,
  MTLBlitCommandEncoder_synchronizeResource,
  MTLBlitCommandEncoder_synchronizeTexture,
  MTLBlitCommandEncoder_copyFromBuffer_toBuffer,
  MTLBlitCommandEncoder_copyFromBuffer_toTexture,
  MTLBlitCommandEncoder_copyFromBuffer_toTexture_options,
  MTLBlitCommandEncoder_copyFromTexture_toBuffer,
  MTLBlitCommandEncoder_copyFromTexture_toBuffer_options,
  MTLBlitCommandEncoder_copyFromTexture_toTexture,
  MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_origin,
  MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_count,
  MTLBlitCommandEncoder_generateMipmapsForTexture,
  MTLBlitCommandEncoder_fillBuffer,
  MTLBlitCommandEncoder_updateFence,
  MTLBlitCommandEncoder_waitForFence,
  MTLBlitCommandEncoder_getTextureAccessCounters,
  MTLBlitCommandEncoder_resetTextureAccessCounters,
  MTLBlitCommandEncoder_optimizeContentsForGPUAccess,
  MTLBlitCommandEncoder_optimizeContentsForGPUAccess_slice_level,
  MTLBlitCommandEncoder_optimizeContentsForCPUAccess,
  MTLBlitCommandEncoder_optimizeContentsForCPUAccess_slice_level,
  MTLBlitCommandEncoder_resetCommandsInBuffer,
  MTLBlitCommandEncoder_copyIndirectCommandBuffer,
  MTLBlitCommandEncoder_optimizeIndirectCommandBuffer,
  MTLBlitCommandEncoder_sampleCountersInBuffer,
  MTLBlitCommandEncoder_resolveCounters,
  MTLComputeCommandEncoder_endEncoding,
  MTLComputeCommandEncoder_setComputePipelineState,
  MTLComputeCommandEncoder_setTexture,
  MTLComputeCommandEncoder_dispatchThreadgroups,
  MTLArgumentEncoder_setArgumentBuffer,
  MTLArgumentEncoder_setTexture,
  MTLArgumentEncoder_setSamplerState,
  MTLIndirectCommandBuffer_indirectRenderCommand,
  MTLIndirectRenderCommand_setRenderPipelineState,
  MTLIndirectRenderCommand_setVertexBuffer,
  MTLIndirectRenderCommand_drawPrimitives,
  MTLRenderCommandEncoder_executeCommandsMarker,
  MTLIndirectRenderCommand_drawIndexedPrimitives,
  MTLIndirectCommandBuffer_reset,
  MTLComputeCommandEncoder_dispatchThreads,
  MTLComputeCommandEncoder_setBuffer,
  MTLComputeCommandEncoder_setSamplerState,
  MTLComputeCommandEncoder_setTextures,
  MTLComputeCommandEncoder_setSamplerStates,
  MTLComputeCommandEncoder_setBuffers,
  MTLComputeCommandEncoder_dispatchThreadgroups_indirect,
  // Append new chunks: never renumber existing capture IDs.
  MTLComputeCommandEncoder_setSamplerState_lodclamp,
  MTLComputeCommandEncoder_setSamplerStates_lodclamp,
  MTLComputeCommandEncoder_setBytes,
  MTLComputeCommandEncoder_setBufferOffset,
  MTLComputeCommandEncoder_setThreadgroupMemoryLength,
  MTLComputeCommandEncoder_useResource,
  MTLComputeCommandEncoder_useResources,
  MTLComputeCommandEncoder_memoryBarrierWithScope,
  MTLComputeCommandEncoder_memoryBarrierWithResources,
  MTLComputeCommandEncoder_pushDebugGroup,
  MTLComputeCommandEncoder_insertDebugSignpost,
  MTLComputeCommandEncoder_popDebugGroup,
  MTLComputeCommandEncoder_updateFence,
  MTLComputeCommandEncoder_waitForFence,
  MTLDevice_newLibraryWithSource_async,
  MTLDevice_newRenderPipelineStateWithDescriptor_async,
  MTLDevice_newRenderPipelineStateWithDescriptor_options_async,
  MTLDevice_newComputePipelineStateWithFunction_async,
  MTLDevice_newComputePipelineStateWithFunction_options_async,
  MTLDevice_newComputePipelineStateWithDescriptor_async,
  MTLIndirectRenderCommand_reset,
  MTLIndirectCommandBuffer_unavailableInitialContents,
  MTLLibrary_newFunctionWithName_constantValues_async,
  MTLLibrary_newFunctionWithDescriptor_async,
  MTLArgumentEncoder_setArgumentBuffer_arrayElement,
  MTLArgumentEncoder_setBuffer,
  MTLArgumentEncoder_constantDataAtIndex,
  MTLArgumentEncoder_unsupportedEncoding,
  MTLRenderCommandEncoder_setVertexBuffer_stride,
  MTLRenderCommandEncoder_setVertexBuffers_strides,
  MTLRenderCommandEncoder_setVertexBufferOffset_stride,
  MTLRenderCommandEncoder_setVertexBytes_stride,
  MTLSharedEvent_unsupportedHostMutation,
  MTLSharedEvent_setInitialSignaledValue,
  MTLHeap_newBuffer,
  MTLHeap_newTexture,
  MTLDevice_newRenderPipelineStateWithTileDescriptor_async,
  MTLDevice_newRenderPipelineStateWithMeshDescriptor,
  MTLRenderCommandEncoder_drawMeshThreadgroups,
  MTLRenderCommandEncoder_setMeshBuffer,
  MTLRenderCommandEncoder_setMeshBytes,
  MTLRenderCommandEncoder_setMeshBufferOffset,
  MTLRenderCommandEncoder_setMeshBuffers,
  MTLRenderCommandEncoder_setMeshTexture,
  MTLRenderCommandEncoder_setMeshTextures,
  MTLRenderCommandEncoder_setMeshSamplerState,
  MTLRenderCommandEncoder_setMeshSamplerStates,
  MTLRenderCommandEncoder_setMeshSamplerState_lodclamp,
  MTLRenderCommandEncoder_setMeshSamplerStates_lodclamp,
  MTLRenderCommandEncoder_drawMeshThreads,
  MTLDevice_newRenderPipelineStateWithMeshDescriptor_async,
  MTLDevice_newRenderPipelineStateWithObjectMeshDescriptor,
  MTLRenderCommandEncoder_setObjectBuffer,
  MTLRenderCommandEncoder_setObjectBytes,
  MTLRenderCommandEncoder_setObjectBufferOffset,
  MTLRenderCommandEncoder_setObjectBuffers,
  MTLRenderCommandEncoder_setObjectTexture,
  MTLRenderCommandEncoder_setObjectTextures,
  MTLRenderCommandEncoder_setObjectSamplerState,
  MTLRenderCommandEncoder_setObjectSamplerStates,
  MTLRenderCommandEncoder_setObjectSamplerState_lodclamp,
  MTLRenderCommandEncoder_setObjectSamplerStates_lodclamp,
  MTLRenderCommandEncoder_setObjectThreadgroupMemoryLength,
  MTLDevice_newRenderPipelineStateWithObjectMeshDescriptor_async,
  MTLRenderCommandEncoder_drawMeshThreadgroups_indirect,
  MTLRasterizationRateMap_copyParameterDataToBuffer,
  MTLDevice_newArgumentEncoderWithBufferBinding,
  MTLHeap_newBufferWithOffset,
  MTLHeap_newTextureWithOffset,
  MTLArgumentEncoder_newArgumentEncoderForBufferAtIndex,
  MTLVisibleFunctionTable_setFunction,
  MTLComputePipelineState_functionHandleWithFunction,
  MTLComputePipelineState_newVisibleFunctionTableWithDescriptor,
  MTLComputeCommandEncoder_setVisibleFunctionTable,
  MTLComputeCommandEncoder_setVisibleFunctionTables,
  MTLArgumentEncoder_setVisibleFunctionTable,
  MTLDevice_newAccelerationStructureWithSize,
  MTLAccelerationStructureCommandEncoder_buildAccelerationStructure,
  MTLAccelerationStructureCommandEncoder_writeCompactedAccelerationStructureSize,
  MTLAccelerationStructureCommandEncoder_endEncoding,
  MTLAccelerationStructureCommandEncoder_buildIndexedTriangle,
  MTLAccelerationStructureCommandEncoder_buildBoundingBox,
  MTLDevice_newAccelerationStructureWithDescriptor,
  MTLAccelerationStructureCommandEncoder_copyAccelerationStructure,
  MTLAccelerationStructureCommandEncoder_copyAndCompactAccelerationStructure,
  MTLAccelerationStructureCommandEncoder_buildRefittableTriangle,
  MTLAccelerationStructureCommandEncoder_refitTriangle,
  MTLComputeCommandEncoder_setAccelerationStructure,
  MTLAccelerationStructureCommandEncoder_buildInstance,
  MTLAccelerationStructureCommandEncoder_buildInstances,
  MTLAccelerationStructureCommandEncoder_buildDistinctInstances,
  MTLAccelerationStructureCommandEncoder_buildNonOpaqueTriangle,
  MTLIntersectionFunctionTable_setFunction,
  MTLIntersectionFunctionTable_setOpaqueTriangleFunction,
  MTLIntersectionFunctionTable_setOpaqueTriangleFunctions,
  MTLIntersectionFunctionTable_setBuffer,
  MTLIntersectionFunctionTable_setBuffers,
  MTLIntersectionFunctionTable_setVisibleFunctionTable,
  MTLIntersectionFunctionTable_setVisibleFunctionTables,
  MTLCommandBuffer_accelerationStructureCommandEncoderWithDescriptor,
  MTLAccelerationStructureCommandEncoder_buildOpaqueTriangle,
  MTLAccelerationStructureCommandEncoder_buildIndexedOpaqueTriangle,
  MTLAccelerationStructureCommandEncoder_refitTriangleExtended,
  MTLAccelerationStructureCommandEncoder_buildBoundingBoxExtended,
  MTLAccelerationStructureCommandEncoder_buildBoundingBoxStrided,
  MTLComputePipelineState_newIntersectionFunctionTableWithDescriptor,
  MTLComputeCommandEncoder_setIntersectionFunctionTable,
  MTLComputeCommandEncoder_setIntersectionFunctionTables,
  MTLAccelerationStructureCommandEncoder_buildBoundingBoxTableOffset,
  MTLAccelerationStructureCommandEncoder_buildBoundingBoxOpaque,
  MTLAccelerationStructureCommandEncoder_buildIndexedTriangleOffset,
  MTLAccelerationStructureCommandEncoder_buildIndexedTriangleExtended,
  MTLAccelerationStructureCommandEncoder_buildTriangleTableOffset,
  MTLAccelerationStructureCommandEncoder_buildBoundingBoxNoDuplicate,
  MTLAccelerationStructureCommandEncoder_buildTriangleNoDuplicate,
  MTLAccelerationStructureCommandEncoder_buildRefittableTriangleNoDuplicate,
  MTLAccelerationStructureCommandEncoder_refitTriangleNoDuplicate,
  MTLAccelerationStructureCommandEncoder_buildFormattedTriangle,
  MTLAccelerationStructureCommandEncoder_buildIndexedFormattedTriangle,
  MTLAccelerationStructureCommandEncoder_buildRefittableFormattedTriangle,
  MTLAccelerationStructureCommandEncoder_refitFormattedTriangle,
  MTLAccelerationStructureCommandEncoder_buildRefittableBoundingBox,
  MTLAccelerationStructureCommandEncoder_refitBoundingBox,
  MTLAccelerationStructureCommandEncoder_buildRefittableIndexedTriangle,
  MTLAccelerationStructureCommandEncoder_refitIndexedTriangle,
  MTLAccelerationStructureCommandEncoder_buildMultipleDistinctInstances,
  MTLAccelerationStructureCommandEncoder_buildRepeatedDistinctInstances,
  MTLDevice_newLibraryWithStitchedDescriptor_async,
  MTLBinaryArchive_addComputePipelineFunctionsWithDescriptor,
  MTLBinaryArchive_addRenderPipelineFunctionsWithDescriptor,
  MTLBinaryArchive_addFunctionWithDescriptor,
  MTLBinaryArchive_addLibraryWithDescriptor,
  MTLBinaryArchive_addTileRenderPipelineFunctionsWithDescriptor,
  MTLBinaryArchive_addMeshRenderPipelineFunctionsWithDescriptor,
  MTLComputeCommandEncoder_useHeap,
  MTLComputeCommandEncoder_useHeaps,
  MTLParallelRenderCommandEncoder_renderCommandEncoder,
  MTLParallelRenderCommandEncoder_endEncoding,
  MTLParallelRenderCommandEncoder_setColorStoreAction,
  MTLParallelRenderCommandEncoder_setDepthStoreAction,
  MTLParallelRenderCommandEncoder_setStencilStoreAction,
  MTLParallelRenderCommandEncoder_setColorStoreActionOptions,
  MTLParallelRenderCommandEncoder_setDepthStoreActionOptions,
  MTLParallelRenderCommandEncoder_setStencilStoreActionOptions,
  MTLParallelRenderCommandEncoder_pushDebugGroup,
  MTLParallelRenderCommandEncoder_popDebugGroup,
  MTLParallelRenderCommandEncoder_insertDebugSignpost,
  Max
};

DECLARE_REFLECTION_ENUM(MetalChunk);

// must be at the start of any function that serialises
#define CACHE_THREAD_SERIALISER() WriteSerialiser &ser = m_Device->GetThreadSerialiser();

#define SERIALISE_TIME_CALL(...)                                                                \
  {                                                                                             \
    WriteSerialiser &ser = m_Device->GetThreadSerialiser();                                     \
    ser.ChunkMetadata().timestampMicro = Timing::GetTick();                                     \
    __VA_ARGS__;                                                                                \
    ser.ChunkMetadata().durationMicro = Timing::GetTick() - ser.ChunkMetadata().timestampMicro; \
  }

#define DECLARE_FUNCTION_SERIALISED(ret, func, ...) \
  ret func(__VA_ARGS__);                            \
  template <typename SerialiserType>                \
  bool CONCAT(Serialise_, func(SerialiserType &ser, ##__VA_ARGS__));

#define INSTANTIATE_FUNCTION_SERIALISED(CLASS, ret, func, ...)                       \
  template bool CLASS::CONCAT(Serialise_, func(ReadSerialiser &ser, ##__VA_ARGS__)); \
  template bool CLASS::CONCAT(Serialise_, func(WriteSerialiser &ser, ##__VA_ARGS__));

#define DECLARE_FUNCTION_WITH_RETURN_SERIALISED(ret, func, ...) \
  ret func(__VA_ARGS__);                                        \
  template <typename SerialiserType>                            \
  bool CONCAT(Serialise_, func(SerialiserType &ser, ret, ##__VA_ARGS__));

#define INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(CLASS, ret, func, ...)                \
  template bool CLASS::CONCAT(Serialise_, func(ReadSerialiser &ser, ret, ##__VA_ARGS__)); \
  template bool CLASS::CONCAT(Serialise_, func(WriteSerialiser &ser, ret, ##__VA_ARGS__));

// A handy macro to say "is the serialiser reading and we're doing replay-mode stuff?"
// The reason we check both is that checking the first allows the compiler to eliminate the other
// path at compile-time, and the second because we might be just struct-serialising in which case we
// should be doing no work to restore states.
// Writing is unambiguously during capture mode, so we don't have to check both in that case.
#define IsReplayingAndReading() (ser.IsReading() && IsReplayMode(m_Device->GetState()))

#ifdef __OBJC__
#define METAL_NOT_HOOKED()                                                            \
  do                                                                                  \
  {                                                                                   \
    RDCFATAL("Metal %s %s not hooked", object_getClassName(self), sel_getName(_cmd)); \
  } while((void)0, 0)
#endif

#define METAL_CAPTURE_NOT_IMPLEMENTED()                                \
  do                                                                   \
  {                                                                    \
    RDCERR("Metal '%s' capture not implemented", __PRETTY_FUNCTION__); \
  } while((void)0, 0)

// similar to RDCUNIMPLEMENTED but without the debugbreak
#define METAL_NOT_IMPLEMENTED(...)                                            \
  do                                                                          \
  {                                                                           \
    RDCWARN("Metal '%s' not implemented -" __VA_ARGS__, __PRETTY_FUNCTION__); \
  } while((void)0, 0)

// similar to RDCUNIMPLEMENTED but for things that are hit often so we don't want to fire the
// debugbreak.
#define METAL_NOT_IMPLEMENTED_ONCE(...)                                          \
  do                                                                             \
  {                                                                              \
    static bool msgprinted = false;                                              \
    if(!msgprinted)                                                              \
      RDCWARN("Metal '%s' not implemented - " __VA_ARGS__, __PRETTY_FUNCTION__); \
    msgprinted = true;                                                           \
  } while((void)0, 0)

BlendMultiplier MakeBlendMultiplier(MTL::BlendFactor blend);
BlendOperation MakeBlendOp(MTL::BlendOperation op);
byte MakeWriteMask(MTL::ColorWriteMask mask);
ResourceFormat MakeResourceFormat(MTL::PixelFormat mtlFormat);
uint32_t GetByteSize(uint32_t width, uint32_t height, uint32_t depth, MTL::PixelFormat mtlFormat,
                     uint32_t mip);
