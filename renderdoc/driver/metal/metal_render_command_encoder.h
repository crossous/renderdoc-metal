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

#include "metal_common.h"
#include "metal_device.h"
#include "metal_resources.h"

class WrappedMTLRenderCommandEncoder : public WrappedMTLObject
{
public:
  WrappedMTLRenderCommandEncoder(MTL::RenderCommandEncoder *realMTLRenderCommandEncoder,
                                 ResourceId objId, WrappedMTLDevice *wrappedMTLDevice);

  void SetCommandBuffer(WrappedMTLCommandBuffer *commandBuffer)
  {
    m_CommandBuffer = commandBuffer;
    m_HasGPUWork = false;
    m_ParallelParent = NULL;
  }
  WrappedMTLCommandBuffer *GetCommandBuffer() const { return m_CommandBuffer; }
  void SetParallelParent(WrappedMTLParallelRenderCommandEncoder *parent) { m_ParallelParent = parent; }
  WrappedMTLParallelRenderCommandEncoder *GetParallelParent() const { return m_ParallelParent; }
  void MarkGPUWork() { m_HasGPUWork = true; }
  bool HasGPUWork() const { return m_HasGPUWork; }
  void SetDeferredStoreActions(uint16_t mask) { m_DeferredStoreActions = mask; }
  void ClearDeferredStoreAction(uint32_t attachment) { m_DeferredStoreActions &= ~(1U << attachment); }
  void ResolveDeferredStoreActions();
  DECLARE_FUNCTION_SERIALISED(void, insertDebugSignpost, NS::String *string);
  DECLARE_FUNCTION_SERIALISED(void, pushDebugGroup, NS::String *string);
  DECLARE_FUNCTION_SERIALISED(void, popDebugGroup);
  DECLARE_FUNCTION_SERIALISED(void, setRenderPipelineState,
                              WrappedMTLRenderPipelineState *pipelineState);
  DECLARE_FUNCTION_SERIALISED(void, setVertexAmplificationCount, NS::UInteger count,
                              rdcarray<uint32_t> viewportOffsets,
                              rdcarray<uint32_t> targetOffsets, bool hasMappings);
  DECLARE_FUNCTION_SERIALISED(void, setVertexBuffer, WrappedMTLBuffer *buffer, NS::UInteger offset,
                              NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setVertexBytes, rdcarray<byte> data, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setVertexBufferOffset, NS::UInteger offset,
                              NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setVertexBuffers, rdcarray<WrappedMTLBuffer *> buffers,
                              rdcarray<NS::UInteger> offsets, NS::Range range);
  void setVertexBufferWithStride(WrappedMTLBuffer *buffer, NS::UInteger offset,
                                 NS::UInteger stride, NS::UInteger index);
  void setVertexBuffersWithStrides(rdcarray<WrappedMTLBuffer *> buffers,
                                   rdcarray<NS::UInteger> offsets,
                                   rdcarray<NS::UInteger> strides, NS::Range range);
  void setVertexBufferOffsetWithStride(NS::UInteger offset, NS::UInteger stride,
                                       NS::UInteger index);
  void setVertexBytesWithStride(rdcarray<byte> data, NS::UInteger stride, NS::UInteger index);
  template <typename SerialiserType>
  bool Serialise_setVertexBindingWithStride(SerialiserType &ser,
                                            rdcarray<WrappedMTLBuffer *> buffers,
                                            rdcarray<NS::UInteger> offsets,
                                            rdcarray<NS::UInteger> strides,
                                            rdcarray<byte> data, NS::Range range,
                                            uint32_t variant);
  DECLARE_FUNCTION_SERIALISED(void, setVertexTexture, WrappedMTLTexture *texture,
                              NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setVertexTextures,
                              rdcarray<WrappedMTLTexture *> textures, NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, setVertexSamplerStateWithLOD, WrappedMTLSamplerState *sampler,
                              float lodMinClamp, float lodMaxClamp, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setVertexSamplerStatesWithLOD,
                              rdcarray<WrappedMTLSamplerState *> samplers,
                              rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps,
                              NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, setVertexSamplerState, WrappedMTLSamplerState *sampler,
                              NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setVertexSamplerStates,
                              rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, setFragmentBuffer, WrappedMTLBuffer *buffer,
                              NS::UInteger offset, NS::UInteger index);
  void setFragmentAccelerationStructure(WrappedMTLAccelerationStructure *structure,
                                        NS::UInteger index);
  template <typename SerialiserType>
  bool Serialise_setFragmentAccelerationStructure(SerialiserType &ser,
                                                  WrappedMTLAccelerationStructure *structure,
                                                  NS::UInteger index);
  void setVertexAccelerationStructure(WrappedMTLAccelerationStructure *structure,
                                      NS::UInteger index);
  template <typename SerialiserType>
  bool Serialise_setVertexAccelerationStructure(SerialiserType &ser,
                                                WrappedMTLAccelerationStructure *structure,
                                                NS::UInteger index);
  void setTileAccelerationStructure(WrappedMTLAccelerationStructure *structure,
                                    NS::UInteger index);
  template <typename SerialiserType>
  bool Serialise_setTileAccelerationStructure(SerialiserType &ser,
                                              WrappedMTLAccelerationStructure *structure,
                                              NS::UInteger index);
  void setFragmentVisibleFunctionTable(WrappedMTLVisibleFunctionTable *table,
                                      uint32_t index);
  void setFragmentIntersectionFunctionTable(WrappedMTLIntersectionFunctionTable *table,
                                             uint32_t index);
  void setVertexIntersectionFunctionTable(WrappedMTLIntersectionFunctionTable *table,
                                           uint32_t index);
  void setTileIntersectionFunctionTable(WrappedMTLIntersectionFunctionTable *table,
                                         uint32_t index);
  template <typename SerialiserType>
  bool Serialise_setFragmentIntersectionFunctionTable(SerialiserType &ser,
      WrappedMTLIntersectionFunctionTable *table, uint32_t index);
  template <typename SerialiserType>
  bool Serialise_setVertexIntersectionFunctionTable(SerialiserType &ser,
      WrappedMTLIntersectionFunctionTable *table, uint32_t index);
  template <typename SerialiserType>
  bool Serialise_setTileIntersectionFunctionTable(SerialiserType &ser,
      WrappedMTLIntersectionFunctionTable *table, uint32_t index);
  void setIntersectionFunctionTables(rdcarray<WrappedMTLIntersectionFunctionTable *> tables,
                                     NS::Range range, MTL::RenderStages stage);
  template <typename SerialiserType>
  bool Serialise_setIntersectionFunctionTables(SerialiserType &ser,
      rdcarray<WrappedMTLIntersectionFunctionTable *> tables, NS::Range range,
      MTL::RenderStages stage);
  void setTileVisibleFunctionTable(WrappedMTLVisibleFunctionTable *table, uint32_t index);
  template <typename SerialiserType>
  bool Serialise_setTileVisibleFunctionTable(SerialiserType &ser,
                                            WrappedMTLVisibleFunctionTable *table, uint32_t index);
  void setTileVisibleFunctionTables(rdcarray<WrappedMTLVisibleFunctionTable *> tables,
                                    NS::Range range);
  template <typename SerialiserType>
  bool Serialise_setTileVisibleFunctionTables(SerialiserType &ser,
      rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range);
  void setVertexVisibleFunctionTable(WrappedMTLVisibleFunctionTable *table,
                                     uint32_t index);
  template <typename SerialiserType>
  bool Serialise_setVertexVisibleFunctionTable(SerialiserType &ser,
      WrappedMTLVisibleFunctionTable *table, uint32_t index);
  void setVertexVisibleFunctionTables(
      rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range);
  template <typename SerialiserType>
  bool Serialise_setVertexVisibleFunctionTables(SerialiserType &ser,
      rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range);
  template <typename SerialiserType>
  bool Serialise_setFragmentVisibleFunctionTable(SerialiserType &ser,
      WrappedMTLVisibleFunctionTable *table, uint32_t index);
  void setFragmentVisibleFunctionTables(
      rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range);
  template <typename SerialiserType>
  bool Serialise_setFragmentVisibleFunctionTables(SerialiserType &ser,
      rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, setFragmentBytes, rdcarray<byte> data, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setFragmentBufferOffset, NS::UInteger offset,
                              NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setFragmentBuffers, rdcarray<WrappedMTLBuffer *> buffers,
                              rdcarray<NS::UInteger> offsets, NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, setFragmentTexture, WrappedMTLTexture *texture,
                              NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setFragmentTextures,
                              rdcarray<WrappedMTLTexture *> textures, NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, setFragmentSamplerStateWithLOD, WrappedMTLSamplerState *sampler,
                              float lodMinClamp, float lodMaxClamp, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setFragmentSamplerStatesWithLOD,
                              rdcarray<WrappedMTLSamplerState *> samplers,
                              rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps,
                              NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, setFragmentSamplerState, WrappedMTLSamplerState *sampler,
                              NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setFragmentSamplerStates,
                              rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, setTileBuffer, WrappedMTLBuffer *buffer,
                              NS::UInteger offset, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setObjectBuffer, WrappedMTLBuffer *buffer,
                              NS::UInteger offset, NS::UInteger index);
  void setObjectBytes(rdcarray<byte> data, NS::UInteger index);
  void setObjectBufferOffset(NS::UInteger offset, NS::UInteger index);
  void setObjectBuffers(rdcarray<WrappedMTLBuffer *> buffers,
                        rdcarray<NS::UInteger> offsets, NS::Range range);
  template <typename SerialiserType>
  bool Serialise_setObjectBinding(SerialiserType &ser,
                                  rdcarray<WrappedMTLBuffer *> buffers,
                                  rdcarray<NS::UInteger> offsets,
                                  rdcarray<byte> data, NS::Range range, uint32_t variant);
  void setObjectTextures(rdcarray<WrappedMTLTexture *> textures, NS::Range range,
                         uint32_t variant);
  template <typename SerialiserType>
  bool Serialise_setObjectTextures(SerialiserType &ser,
                                   rdcarray<WrappedMTLTexture *> textures,
                                   NS::Range range, uint32_t variant);
  void setObjectSamplers(rdcarray<WrappedMTLSamplerState *> samplers,
                         rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps,
                         NS::Range range, uint32_t variant);
  template <typename SerialiserType>
  bool Serialise_setObjectSamplers(SerialiserType &ser,
                                   rdcarray<WrappedMTLSamplerState *> samplers,
                                   rdcarray<float> lodMinClamps,
                                   rdcarray<float> lodMaxClamps,
                                   NS::Range range, uint32_t variant);
  void setTileBytes(rdcarray<byte> data, NS::UInteger index);
  void setTileBufferOffset(NS::UInteger offset, NS::UInteger index);
  void setTileBuffers(rdcarray<WrappedMTLBuffer *> buffers,
                      rdcarray<NS::UInteger> offsets, NS::Range range);
  template <typename SerialiserType>
  bool Serialise_setTileBinding(SerialiserType &ser,
                                rdcarray<WrappedMTLBuffer *> buffers,
                                rdcarray<NS::UInteger> offsets,
                                rdcarray<byte> data, NS::Range range, uint32_t variant);
  void setMeshBuffer(WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger index);
  void setMeshBytes(rdcarray<byte> data, NS::UInteger index);
  void setMeshBufferOffset(NS::UInteger offset, NS::UInteger index);
  void setMeshBuffers(rdcarray<WrappedMTLBuffer *> buffers,
                      rdcarray<NS::UInteger> offsets, NS::Range range);
  template <typename SerialiserType>
  bool Serialise_setMeshBinding(SerialiserType &ser,
                                rdcarray<WrappedMTLBuffer *> buffers,
                                rdcarray<NS::UInteger> offsets,
                                rdcarray<byte> data, NS::Range range, uint32_t variant);
  void setMeshTextures(rdcarray<WrappedMTLTexture *> textures, NS::Range range,
                       uint32_t variant);
  template <typename SerialiserType>
  bool Serialise_setMeshTextures(SerialiserType &ser,
                                 rdcarray<WrappedMTLTexture *> textures,
                                 NS::Range range, uint32_t variant);
  void setMeshSamplers(rdcarray<WrappedMTLSamplerState *> samplers,
                       rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps,
                       NS::Range range, uint32_t variant);
  template <typename SerialiserType>
  bool Serialise_setMeshSamplers(SerialiserType &ser,
                                 rdcarray<WrappedMTLSamplerState *> samplers,
                                 rdcarray<float> lodMinClamps,
                                 rdcarray<float> lodMaxClamps,
                                 NS::Range range, uint32_t variant);
  void setTileTextures(rdcarray<WrappedMTLTexture *> textures, NS::Range range,
                       uint32_t variant);
  template <typename SerialiserType>
  bool Serialise_setTileTextures(SerialiserType &ser,
                                 rdcarray<WrappedMTLTexture *> textures,
                                 NS::Range range, uint32_t variant);
  void setTileSamplers(rdcarray<WrappedMTLSamplerState *> samplers,
                       rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps,
                       NS::Range range, uint32_t variant);
  template <typename SerialiserType>
  bool Serialise_setTileSamplers(SerialiserType &ser,
                                 rdcarray<WrappedMTLSamplerState *> samplers,
                                 rdcarray<float> lodMinClamps,
                                 rdcarray<float> lodMaxClamps,
                                 NS::Range range, uint32_t variant);
  DECLARE_FUNCTION_SERIALISED(void, dispatchThreadsPerTile, MTL::Size threadsPerTile);
  DECLARE_FUNCTION_SERIALISED(void, setThreadgroupMemoryLength, NS::UInteger length,
                              NS::UInteger offset, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, drawMeshThreadgroups, MTL::Size threadgroupsPerGrid,
                              MTL::Size threadsPerObjectThreadgroup,
                              MTL::Size threadsPerMeshThreadgroup);
  DECLARE_FUNCTION_SERIALISED(void, drawMeshThreadgroups, WrappedMTLBuffer *indirectBuffer,
                              NS::UInteger indirectBufferOffset,
                              MTL::Size threadsPerObjectThreadgroup,
                              MTL::Size threadsPerMeshThreadgroup);
  DECLARE_FUNCTION_SERIALISED(void, drawMeshThreads, MTL::Size threadsPerGrid,
                              MTL::Size threadsPerObjectThreadgroup,
                              MTL::Size threadsPerMeshThreadgroup);
  DECLARE_FUNCTION_SERIALISED(void, setObjectThreadgroupMemoryLength,
                              NS::UInteger length, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, useResource, WrappedMTLResource *resource,
                              MTL::ResourceUsage usage);
  DECLARE_FUNCTION_SERIALISED(void, useResourceWithStages, WrappedMTLResource *resource,
                              MTL::ResourceUsage usage, MTL::RenderStages stages);
  DECLARE_FUNCTION_SERIALISED(void, useResources, rdcarray<WrappedMTLResource *> resources,
                              MTL::ResourceUsage usage);
  DECLARE_FUNCTION_SERIALISED(void, useResourcesWithStages, rdcarray<WrappedMTLResource *> resources,
                              MTL::ResourceUsage usage, MTL::RenderStages stages);
  void declareHeaps(rdcarray<WrappedMTLHeap *> heaps, MTL::RenderStages stages, uint32_t variant);
  template <typename SerialiserType>
  bool Serialise_declareHeaps(SerialiserType &ser, rdcarray<WrappedMTLHeap *> heaps,
                              MTL::RenderStages stages, uint32_t variant);
  DECLARE_FUNCTION_SERIALISED(void, memoryBarrierWithScope, MTL::BarrierScope scope,
                              MTL::RenderStages after, MTL::RenderStages before);
  DECLARE_FUNCTION_SERIALISED(void, memoryBarrierWithResources, rdcarray<WrappedMTLResource *> resources,
                              MTL::RenderStages after, MTL::RenderStages before);
  DECLARE_FUNCTION_SERIALISED(void, setViewport, MTL::Viewport &viewport);
  DECLARE_FUNCTION_SERIALISED(void, setViewports, rdcarray<MTL::Viewport> viewports);
  DECLARE_FUNCTION_SERIALISED(void, setScissorRect, MTL::ScissorRect &rect);
  DECLARE_FUNCTION_SERIALISED(void, setScissorRects, rdcarray<MTL::ScissorRect> scissors);
  DECLARE_FUNCTION_SERIALISED(void, setFrontFacingWinding, MTL::Winding winding);
  DECLARE_FUNCTION_SERIALISED(void, setCullMode, MTL::CullMode cullMode);
  DECLARE_FUNCTION_SERIALISED(void, setDepthClipMode, MTL::DepthClipMode depthClipMode);
  DECLARE_FUNCTION_SERIALISED(void, setDepthBias, float depthBias, float slopeScale, float clamp);
  DECLARE_FUNCTION_SERIALISED(void, setTriangleFillMode, MTL::TriangleFillMode fillMode);
  DECLARE_FUNCTION_SERIALISED(void, setBlendColor, float red, float green, float blue,
                              float alpha);
  DECLARE_FUNCTION_SERIALISED(void, setDepthStencilState,
                              WrappedMTLDepthStencilState *depthStencilState);
  DECLARE_FUNCTION_SERIALISED(void, setStencilReferenceValue, uint32_t referenceValue);
  DECLARE_FUNCTION_SERIALISED(void, setStencilReferenceValues, uint32_t frontReferenceValue,
                              uint32_t backReferenceValue);
  DECLARE_FUNCTION_SERIALISED(void, setVisibilityResultMode, MTL::VisibilityResultMode mode,
                              NS::UInteger offset);
  DECLARE_FUNCTION_SERIALISED(void, setColorStoreAction, MTL::StoreAction storeAction,
                              NS::UInteger colorAttachmentIndex);
  DECLARE_FUNCTION_SERIALISED(void, setDepthStoreAction, MTL::StoreAction storeAction);
  DECLARE_FUNCTION_SERIALISED(void, setStencilStoreAction, MTL::StoreAction storeAction);
  DECLARE_FUNCTION_SERIALISED(void, setColorStoreActionOptions,
                              MTL::StoreActionOptions storeActionOptions,
                              NS::UInteger colorAttachmentIndex);
  DECLARE_FUNCTION_SERIALISED(void, setDepthStoreActionOptions,
                              MTL::StoreActionOptions storeActionOptions);
  DECLARE_FUNCTION_SERIALISED(void, setStencilStoreActionOptions,
                              MTL::StoreActionOptions storeActionOptions);
  DECLARE_FUNCTION_SERIALISED(void, textureBarrier);
  DECLARE_FUNCTION_SERIALISED(void, drawPrimitives, MTL::PrimitiveType primitiveType,
                              NS::UInteger vertexStart, NS::UInteger vertexCount,
                              NS::UInteger instanceCount, NS::UInteger baseInstance);
  DECLARE_FUNCTION_SERIALISED(void, drawPrimitives, MTL::PrimitiveType primitiveType,
                              WrappedMTLBuffer *indirectBuffer,
                              NS::UInteger indirectBufferOffset);
  void drawPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger vertexStart,
                      NS::UInteger vertexCount);
  void drawPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger vertexStart,
                      NS::UInteger vertexCount, NS::UInteger instanceCount);
  void drawIndexedPrimitives(MTL::PrimitiveType primitiveType, NS::UInteger indexCount,
                              MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer,
                              NS::UInteger indexBufferOffset, NS::UInteger instanceCount);
  DECLARE_FUNCTION_SERIALISED(void, drawIndexedPrimitives, MTL::PrimitiveType primitiveType,
                              NS::UInteger indexCount, MTL::IndexType indexType,
                              WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset);
  DECLARE_FUNCTION_SERIALISED(void, drawIndexedPrimitives, MTL::PrimitiveType primitiveType,
                              NS::UInteger indexCount, MTL::IndexType indexType,
                              WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset,
                              NS::UInteger instanceCount, NS::Integer baseVertex,
                              NS::UInteger baseInstance);
  DECLARE_FUNCTION_SERIALISED(void, drawIndexedPrimitives, MTL::PrimitiveType primitiveType,
                              MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer,
                              NS::UInteger indexBufferOffset, WrappedMTLBuffer *indirectBuffer,
                              NS::UInteger indirectBufferOffset);
  DECLARE_FUNCTION_SERIALISED(void, executeCommandsInBuffer,
                              WrappedMTLIndirectCommandBuffer *icb, NS::Range range);
  void executeCommandsInBufferIndirect(WrappedMTLIndirectCommandBuffer *icb,
                                       WrappedMTLBuffer *rangeBuffer, NS::UInteger offset);
  template <typename SerialiserType>
  bool Serialise_executeCommandsInBufferIndirect(SerialiserType &ser,
                                                  WrappedMTLIndirectCommandBuffer *icb,
                                                  WrappedMTLBuffer *rangeBuffer,
                                                  NS::UInteger offset);
  template <typename SerialiserType>
  bool Serialise_executeCommandsMarker(SerialiserType &ser,
                                       WrappedMTLIndirectCommandBuffer *icb, NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, endEncoding);
  DECLARE_FUNCTION_SERIALISED(void, updateFence, WrappedMTLFence *fence, MTL::RenderStages stages);
  DECLARE_FUNCTION_SERIALISED(void, waitForFence, WrappedMTLFence *fence, MTL::RenderStages stages);
  DECLARE_FUNCTION_SERIALISED(void, setTessellationFactorBuffer, WrappedMTLBuffer *buffer,
                              NS::UInteger offset, NS::UInteger instanceStride);
  DECLARE_FUNCTION_SERIALISED(void, setTessellationFactorScale, float scale);
  DECLARE_FUNCTION_SERIALISED(void, drawPatches, NS::UInteger controlPoints,
                              NS::UInteger patchStart, NS::UInteger patchCount,
                              WrappedMTLBuffer *patchIndexBuffer, NS::UInteger patchIndexBufferOffset,
                              NS::UInteger instanceCount, NS::UInteger baseInstance);
  DECLARE_FUNCTION_SERIALISED(void, drawPatchesIndirect, NS::UInteger controlPoints,
                              WrappedMTLBuffer *patchIndexBuffer, NS::UInteger patchIndexBufferOffset,
                              WrappedMTLBuffer *indirectBuffer, NS::UInteger indirectBufferOffset);
  DECLARE_FUNCTION_SERIALISED(void, drawIndexedPatches, NS::UInteger controlPoints,
                              NS::UInteger patchStart, NS::UInteger patchCount,
                              WrappedMTLBuffer *patchIndexBuffer, NS::UInteger patchIndexBufferOffset,
                              WrappedMTLBuffer *controlPointIndexBuffer,
                              NS::UInteger controlPointIndexBufferOffset,
                              NS::UInteger instanceCount, NS::UInteger baseInstance);
  DECLARE_FUNCTION_SERIALISED(void, drawIndexedPatchesIndirect, NS::UInteger controlPoints,
                              WrappedMTLBuffer *patchIndexBuffer, NS::UInteger patchIndexBufferOffset,
                              WrappedMTLBuffer *controlPointIndexBuffer,
                              NS::UInteger controlPointIndexBufferOffset,
                              WrappedMTLBuffer *indirectBuffer, NS::UInteger indirectBufferOffset);

  enum
  {
    TypeEnum = eResRenderCommandEncoder
  };

private:
  WrappedMTLCommandBuffer *m_CommandBuffer;
  WrappedMTLParallelRenderCommandEncoder *m_ParallelParent = NULL;
  bool m_HasGPUWork = false;
  uint16_t m_DeferredStoreActions = 0;
  WrappedMTLRenderPipelineState *m_EncoderPipeline = NULL;
  WrappedMTLBuffer *m_EncoderVertexBuffers[MAX_RENDER_PASS_BUFFER_ATTACHMENTS] = {};
  NS::UInteger m_EncoderVertexOffsets[MAX_RENDER_PASS_BUFFER_ATTACHMENTS] = {};
  WrappedMTLBuffer *m_EncoderTileBuffers[MAX_RENDER_PASS_BUFFER_ATTACHMENTS] = {};
  NS::UInteger m_EncoderTileOffsets[MAX_RENDER_PASS_BUFFER_ATTACHMENTS] = {};
  WrappedMTLBuffer *m_EncoderMeshBuffers[MAX_RENDER_PASS_BUFFER_ATTACHMENTS] = {};
  NS::UInteger m_EncoderMeshOffsets[MAX_RENDER_PASS_BUFFER_ATTACHMENTS] = {};
  WrappedMTLBuffer *m_EncoderObjectBuffers[MAX_RENDER_PASS_BUFFER_ATTACHMENTS] = {};
  NS::UInteger m_EncoderObjectOffsets[MAX_RENDER_PASS_BUFFER_ATTACHMENTS] = {};
};
