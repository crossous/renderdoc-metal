/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
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

class WrappedMTLComputeCommandEncoder : public WrappedMTLObject
{
public:
  WrappedMTLComputeCommandEncoder(MTL::ComputeCommandEncoder *real, ResourceId id,
                                  WrappedMTLDevice *device);

  void SetCommandBuffer(WrappedMTLCommandBuffer *commandBuffer) { m_CommandBuffer = commandBuffer; }
  WrappedMTLCommandBuffer *GetCommandBuffer() const { return m_CommandBuffer; }
  DECLARE_FUNCTION_SERIALISED(void, endEncoding);
  DECLARE_FUNCTION_SERIALISED(void, updateFence, WrappedMTLFence *fence);
  DECLARE_FUNCTION_SERIALISED(void, waitForFence, WrappedMTLFence *fence);
  DECLARE_FUNCTION_SERIALISED(void, useResource, WrappedMTLResource *resource, MTL::ResourceUsage usage);
  DECLARE_FUNCTION_SERIALISED(void, useResources, rdcarray<WrappedMTLResource *> resources,
                              MTL::ResourceUsage usage);
  void declareHeaps(rdcarray<WrappedMTLHeap *> heaps, bool arrayVariant);
  template <typename SerialiserType>
  bool Serialise_declareHeaps(SerialiserType &ser, rdcarray<WrappedMTLHeap *> heaps,
                             bool arrayVariant);
  DECLARE_FUNCTION_SERIALISED(void, memoryBarrierWithScope, MTL::BarrierScope scope);
  DECLARE_FUNCTION_SERIALISED(void, memoryBarrierWithResources, rdcarray<WrappedMTLResource *> resources);
  DECLARE_FUNCTION_SERIALISED(void, pushDebugGroup, NS::String *string);
  DECLARE_FUNCTION_SERIALISED(void, insertDebugSignpost, NS::String *string);
  DECLARE_FUNCTION_SERIALISED(void, popDebugGroup);
  DECLARE_FUNCTION_SERIALISED(void, setComputePipelineState,
                              WrappedMTLComputePipelineState *pipeline);
  void setVisibleFunctionTable(WrappedMTLVisibleFunctionTable *table, uint32_t index);
  template <typename SerialiserType>
  bool Serialise_setVisibleFunctionTable(SerialiserType &ser,
                                         WrappedMTLVisibleFunctionTable *table, uint32_t index);
  void setVisibleFunctionTables(rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range);
  template <typename SerialiserType>
  bool Serialise_setVisibleFunctionTables(SerialiserType &ser,
      rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range);
  void setIntersectionFunctionTable(WrappedMTLIntersectionFunctionTable *table, uint32_t index);
  template <typename SerialiserType>
  bool Serialise_setIntersectionFunctionTable(SerialiserType &ser,
      WrappedMTLIntersectionFunctionTable *table, uint32_t index);
  void setIntersectionFunctionTables(rdcarray<WrappedMTLIntersectionFunctionTable *> tables,
                                     NS::Range range);
  template <typename SerialiserType>
  bool Serialise_setIntersectionFunctionTables(SerialiserType &ser,
      rdcarray<WrappedMTLIntersectionFunctionTable *> tables, NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, setTexture, WrappedMTLTexture *texture, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setTextures, rdcarray<WrappedMTLTexture *> textures,
                              NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, setSamplerStateWithLOD, WrappedMTLSamplerState *sampler,
                              float lodMinClamp, float lodMaxClamp, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setSamplerStatesWithLOD,
                              rdcarray<WrappedMTLSamplerState *> samplers,
                              rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps,
                              NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, setSamplerState, WrappedMTLSamplerState *sampler,
                              NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setSamplerStates,
                              rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, setBuffer, WrappedMTLBuffer *buffer, NS::UInteger offset,
                              NS::UInteger index);
  void setAccelerationStructure(WrappedMTLAccelerationStructure *structure,
                                NS::UInteger index);
  template <typename SerialiserType>
  bool Serialise_setAccelerationStructure(SerialiserType &ser,
                                          WrappedMTLAccelerationStructure *structure,
                                          NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setBytes, rdcarray<byte> data, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setBufferOffset, NS::UInteger offset, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setThreadgroupMemoryLength, NS::UInteger length,
                              NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setBuffers, rdcarray<WrappedMTLBuffer *> buffers,
                              rdcarray<NS::UInteger> offsets, NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, dispatchThreadgroups, MTL::Size &groups,
                              MTL::Size &threadsPerGroup);
  DECLARE_FUNCTION_SERIALISED(void, dispatchThreadgroups, WrappedMTLBuffer *indirectBuffer,
                              NS::UInteger indirectBufferOffset, MTL::Size &threadsPerGroup);
  DECLARE_FUNCTION_SERIALISED(void, dispatchThreads, MTL::Size &grid,
                              MTL::Size &threadsPerGroup);

  enum
  {
    TypeEnum = eResComputeCommandEncoder
  };

private:
  WrappedMTLCommandBuffer *m_CommandBuffer = NULL;
  WrappedMTLComputePipelineState *m_Pipeline = NULL;
  WrappedMTLVisibleFunctionTable *m_VisibleTables[31] = {};
  WrappedMTLIntersectionFunctionTable *m_IntersectionTables[31] = {};
  bool ValidateFunctionTableBindings() const;
  bool m_CaptureIndirectArguments = false;
  uint32_t m_CaptureIndirectOrdinal = 0;
  uint64_t m_IndirectReplayEpoch = ~0ULL;
  uint32_t m_IndirectReplayOrdinal = 0;
  MetalComputeIndirectCapture m_IndirectCapture;

};
