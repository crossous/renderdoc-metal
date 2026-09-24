/******************************************************************************
 * The MIT License (MIT)
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

struct MetalIndirectDraw
{
  WrappedMTLRenderPipelineState *pipeline = NULL;
  WrappedMTLBuffer *vertexBuffers[2] = {};
  NS::UInteger vertexBufferOffsets[2] = {};
  MTL::PrimitiveType primitive = MTL::PrimitiveTypeTriangle;
  NS::UInteger vertexStart = 0;
  NS::UInteger vertexCount = 0;
  WrappedMTLBuffer *indexBuffer = NULL;
  NS::UInteger indexBufferOffset = 0;
  MTL::IndexType indexType = MTL::IndexTypeUInt16;
  NS::UInteger indexCount = 0;
  NS::Integer baseVertex = 0;
  NS::UInteger instanceCount = 0;
  NS::UInteger baseInstance = 0;
  bool indexed = false;
  bool encoded = false;
};

class WrappedMTLIndirectCommandBuffer : public WrappedMTLObject
{
public:
  WrappedMTLIndirectCommandBuffer(MTL::IndirectCommandBuffer *real, ResourceId id,
                                  WrappedMTLDevice *device);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLIndirectRenderCommand *, indirectRenderCommand,
                                          NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, reset, NS::Range range);
  NS::UInteger Count() const { return m_Count; }
  void SetCount(NS::UInteger count) { m_Count = count; m_Draws.resize(count); }
  NS::UInteger MaxVertexBufferBindCount() const { return m_MaxVertexBufferBindCount; }
  void SetMaxVertexBufferBindCount(NS::UInteger count) { m_MaxVertexBufferBindCount = count; }
  MTL::IndirectCommandType CommandTypes() const { return m_CommandTypes; }
  void SetCommandTypes(MTL::IndirectCommandType types) { m_CommandTypes = types; }
  MetalIndirectDraw &Draw(NS::UInteger index) { return m_Draws[index]; }
  const MetalIndirectDraw &Draw(NS::UInteger index) const { return m_Draws[index]; }
  bool SupportedDescriptor() const { return m_SupportedDescriptor; }
  void SetSupportedDescriptor(bool supported) { m_SupportedDescriptor = supported; }
  bool InheritPipelineState() const { return m_InheritPipelineState; }
  void SetInheritPipelineState(bool inherit) { m_InheritPipelineState = inherit; }
  bool InheritBuffers() const { return m_InheritBuffers; }
  void SetInheritBuffers(bool inherit) { m_InheritBuffers = inherit; }

  enum { TypeEnum = eResIndirectCommandBuffer };

private:
  NS::UInteger m_Count = 0;
  NS::UInteger m_MaxVertexBufferBindCount = 0;
  MTL::IndirectCommandType m_CommandTypes = (MTL::IndirectCommandType)0;
  bool m_SupportedDescriptor = false;
  bool m_InheritPipelineState = false;
  bool m_InheritBuffers = false;
  rdcarray<MetalIndirectDraw> m_Draws;
};

class WrappedMTLIndirectRenderCommand : public WrappedMTLObject
{
public:
  WrappedMTLIndirectRenderCommand(MTL::IndirectRenderCommand *real, ResourceId id,
                                  WrappedMTLDevice *device);
  void SetParent(WrappedMTLIndirectCommandBuffer *parent, NS::UInteger index)
  {
    m_Parent = parent;
    m_Index = index;
  }
  DECLARE_FUNCTION_SERIALISED(void, setRenderPipelineState,
                              WrappedMTLRenderPipelineState *pipeline);
  DECLARE_FUNCTION_SERIALISED(void, setVertexBuffer, WrappedMTLBuffer *buffer,
                              NS::UInteger offset, NS::UInteger slot);
  DECLARE_FUNCTION_SERIALISED(void, drawPrimitives, MTL::PrimitiveType primitive,
                              NS::UInteger vertexStart, NS::UInteger vertexCount,
                              NS::UInteger instanceCount, NS::UInteger baseInstance);
  DECLARE_FUNCTION_SERIALISED(void, drawIndexedPrimitives, MTL::PrimitiveType primitive,
                              NS::UInteger indexCount, MTL::IndexType indexType,
                              WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset,
                              NS::UInteger instanceCount, NS::Integer baseVertex,
                              NS::UInteger baseInstance);

  enum { TypeEnum = eResIndirectRenderCommand };

private:
  WrappedMTLIndirectCommandBuffer *m_Parent = NULL;
  NS::UInteger m_Index = 0;
};
