/******************************************************************************
 * The MIT License (MIT)
 * Copyright (c) 2026 Baldur Karlsson
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "metal_indirect_command_buffer.h"
#include "metal_buffer.h"
#include "metal_device.h"
#include "metal_manager.h"
#include "metal_render_pipeline_state.h"

WrappedMTLIndirectCommandBuffer::WrappedMTLIndirectCommandBuffer(MTL::IndirectCommandBuffer *real,
                                                                 ResourceId id,
                                                                 WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

WrappedMTLIndirectRenderCommand::WrappedMTLIndirectRenderCommand(MTL::IndirectRenderCommand *real,
                                                                 ResourceId id,
                                                                 WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLIndirectCommandBuffer::Serialise_indirectRenderCommand(
    SerialiserType &ser, WrappedMTLIndirectRenderCommand *command, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(IndirectCommandBuffer, this);
  SERIALISE_ELEMENT_LOCAL(IndirectRenderCommand, GetResID(command))
      .TypedAs("MTLIndirectRenderCommand"_lit);
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!IndirectCommandBuffer || index >= IndirectCommandBuffer->Count())
    {
      RDCERR("Invalid Metal ICB command index %llu", (uint64_t)index);
      return false;
    }
    MTL::IndirectRenderCommand *real =
        Unwrap(IndirectCommandBuffer)->indirectRenderCommand(index);
    if(!real)
    {
      RDCERR("Failed to get Metal ICB command %llu", (uint64_t)index);
      return false;
    }
    WrappedMTLIndirectRenderCommand *wrapped = NULL;
    GetResourceManager()->WrapResource(IndirectRenderCommand, real, wrapped);
    wrapped->SetParent(IndirectCommandBuffer, index);
    m_Device->AddResource(IndirectRenderCommand, ResourceType::StateObject,
                          "Indirect Render Command");
  }
  return true;
}

WrappedMTLIndirectRenderCommand *WrappedMTLIndirectCommandBuffer::indirectRenderCommand(
    NS::UInteger index)
{
  if(index >= m_Count)
    return NULL;
  MTL::IndirectRenderCommand *real = Unwrap(this)->indirectRenderCommand(index);
  if(!real)
    return NULL;
  WrappedMTLIndirectRenderCommand *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->SetParent(this, index);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIndirectCommandBuffer_indirectRenderCommand);
    Serialise_indirectRenderCommand(ser, wrapped, index);
    GetRecord(this)->AddChunk(scope.Get());
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLIndirectCommandBuffer::Serialise_reset(SerialiserType &ser, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(IndirectCommandBuffer, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!IndirectCommandBuffer || !range.length || range.location >= IndirectCommandBuffer->Count() ||
       range.length > IndirectCommandBuffer->Count() - range.location)
    {
      RDCERR("Invalid Metal ICB reset range or resource");
      return false;
    }
    Unwrap(IndirectCommandBuffer)->reset(range);
    for(NS::UInteger index = range.location; index < range.location + range.length; ++index)
      IndirectCommandBuffer->Draw(index) = MetalIndirectDraw();
  }
  return true;
}

void WrappedMTLIndirectCommandBuffer::reset(NS::Range range)
{
  if(!range.length || range.location >= m_Count || range.length > m_Count - range.location)
  {
    RDCERR("Invalid Metal ICB capture reset range");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->reset(range));
  for(NS::UInteger index = range.location; index < range.location + range.length; ++index)
    m_Draws[index] = MetalIndirectDraw();
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIndirectCommandBuffer_reset);
    Serialise_reset(ser, range);
    GetRecord(this)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLIndirectRenderCommand::Serialise_setRenderPipelineState(
    SerialiserType &ser, WrappedMTLRenderPipelineState *pipeline)
{
  SERIALISE_ELEMENT_LOCAL(IndirectRenderCommand, this);
  SERIALISE_ELEMENT(pipeline).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!IndirectRenderCommand || !pipeline || !IndirectRenderCommand->m_Parent ||
       IndirectRenderCommand->m_Parent->InheritPipelineState())
    {
      RDCERR("Missing Metal ICB command or pipeline");
      return false;
    }
    Unwrap(IndirectRenderCommand)->setRenderPipelineState(Unwrap(pipeline));
    IndirectRenderCommand->m_Parent->Draw(IndirectRenderCommand->m_Index).pipeline = pipeline;
  }
  return true;
}

void WrappedMTLIndirectRenderCommand::setRenderPipelineState(
    WrappedMTLRenderPipelineState *pipeline)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setRenderPipelineState(Unwrap(pipeline)));
  if(m_Parent)
    m_Parent->Draw(m_Index).pipeline = pipeline;
  if(IsCaptureMode(m_State) && m_Parent)
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIndirectRenderCommand_setRenderPipelineState);
    Serialise_setRenderPipelineState(ser, pipeline);
    MetalResourceRecord *record = GetRecord(m_Parent);
    record->AddChunk(scope.Get());
    if(pipeline)
      record->AddParent(GetRecord(pipeline));
  }
}

template <typename SerialiserType>
bool WrappedMTLIndirectRenderCommand::Serialise_setVertexBuffer(
    SerialiserType &ser, WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger slot)
{
  SERIALISE_ELEMENT_LOCAL(IndirectRenderCommand, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(slot).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!IndirectRenderCommand || !IndirectRenderCommand->m_Parent || !buffer ||
       IndirectRenderCommand->m_Parent->InheritBuffers() ||
       slot >= IndirectRenderCommand->m_Parent->MaxVertexBufferBindCount() || slot >= 2 ||
       offset >= Unwrap(buffer)->length())
    {
      RDCERR("Invalid Metal ICB vertex buffer, slot or offset");
      return false;
    }
    Unwrap(IndirectRenderCommand)->setVertexBuffer(Unwrap(buffer), offset, slot);
    MetalIndirectDraw &draw = IndirectRenderCommand->m_Parent->Draw(IndirectRenderCommand->m_Index);
    draw.vertexBuffers[slot] = buffer;
    draw.vertexBufferOffsets[slot] = offset;
  }
  return true;
}

void WrappedMTLIndirectRenderCommand::setVertexBuffer(WrappedMTLBuffer *buffer,
                                                       NS::UInteger offset, NS::UInteger slot)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexBuffer(Unwrap(buffer), offset, slot));
  if(m_Parent)
  {
    MetalIndirectDraw &draw = m_Parent->Draw(m_Index);
    if(slot < 2)
    {
      draw.vertexBuffers[slot] = buffer;
      draw.vertexBufferOffsets[slot] = offset;
    }
  }
  if(IsCaptureMode(m_State) && m_Parent)
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIndirectRenderCommand_setVertexBuffer);
    Serialise_setVertexBuffer(ser, buffer, offset, slot);
    MetalResourceRecord *record = GetRecord(m_Parent);
    record->AddChunk(scope.Get());
    if(buffer)
      record->AddParent(GetRecord(buffer));
  }
}

template <typename SerialiserType>
bool WrappedMTLIndirectRenderCommand::Serialise_drawPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitive, NS::UInteger vertexStart,
    NS::UInteger vertexCount, NS::UInteger instanceCount, NS::UInteger baseInstance)
{
  SERIALISE_ELEMENT_LOCAL(IndirectRenderCommand, this);
  SERIALISE_ELEMENT(primitive).Important();
  SERIALISE_ELEMENT(vertexStart).Important();
  SERIALISE_ELEMENT(vertexCount).Important();
  SERIALISE_ELEMENT(instanceCount).Important();
  SERIALISE_ELEMENT(baseInstance).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!IndirectRenderCommand || !IndirectRenderCommand->m_Parent || !vertexCount ||
       !instanceCount || vertexCount > UINT32_MAX || instanceCount > UINT32_MAX ||
       vertexStart > UINT32_MAX || baseInstance > UINT32_MAX ||
       !((uint64_t)IndirectRenderCommand->m_Parent->CommandTypes() &
         (uint64_t)MTL::IndirectCommandTypeDraw))
    {
      RDCERR("Invalid Metal ICB draw arguments");
      return false;
    }
    Unwrap(IndirectRenderCommand)->drawPrimitives(primitive, vertexStart, vertexCount,
                                                   instanceCount, baseInstance);
    MetalIndirectDraw &draw = IndirectRenderCommand->m_Parent->Draw(IndirectRenderCommand->m_Index);
    draw.primitive = primitive;
    draw.vertexStart = vertexStart;
    draw.vertexCount = vertexCount;
    draw.instanceCount = instanceCount;
    draw.baseInstance = baseInstance;
    draw.indexed = false;
    draw.encoded = true;
  }
  return true;
}

void WrappedMTLIndirectRenderCommand::drawPrimitives(MTL::PrimitiveType primitive,
                                                     NS::UInteger vertexStart,
                                                     NS::UInteger vertexCount,
                                                     NS::UInteger instanceCount,
                                                     NS::UInteger baseInstance)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawPrimitives(primitive, vertexStart, vertexCount,
                                                   instanceCount, baseInstance));
  if(m_Parent)
  {
    MetalIndirectDraw &draw = m_Parent->Draw(m_Index);
    draw.primitive = primitive;
    draw.vertexStart = vertexStart;
    draw.vertexCount = vertexCount;
    draw.instanceCount = instanceCount;
    draw.baseInstance = baseInstance;
    draw.indexed = false;
    draw.encoded = true;
  }
  if(IsCaptureMode(m_State) && m_Parent)
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIndirectRenderCommand_drawPrimitives);
    Serialise_drawPrimitives(ser, primitive, vertexStart, vertexCount, instanceCount, baseInstance);
    GetRecord(m_Parent)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLIndirectRenderCommand::Serialise_drawIndexedPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitive, NS::UInteger indexCount,
    MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer,
    NS::UInteger indexBufferOffset, NS::UInteger instanceCount, NS::Integer baseVertex,
    NS::UInteger baseInstance)
{
  SERIALISE_ELEMENT_LOCAL(IndirectRenderCommand, this);
  SERIALISE_ELEMENT(primitive).Important();
  SERIALISE_ELEMENT(indexCount).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexBuffer).Important();
  SERIALISE_ELEMENT(indexBufferOffset).Important();
  SERIALISE_ELEMENT(instanceCount).Important();
  SERIALISE_ELEMENT(baseVertex).Important();
  SERIALISE_ELEMENT(baseInstance).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    MTL::Buffer *realIndex = Unwrap(indexBuffer);
    const uint64_t indexStride = indexType == MTL::IndexTypeUInt16 ? 2 : 4;
    if(!IndirectRenderCommand || !IndirectRenderCommand->m_Parent || !realIndex ||
       (indexType != MTL::IndexTypeUInt16 && indexType != MTL::IndexTypeUInt32) ||
       !indexCount || !instanceCount || indexCount > UINT32_MAX ||
       instanceCount > UINT32_MAX || baseInstance > UINT32_MAX ||
       baseVertex < INT32_MIN || baseVertex > INT32_MAX ||
       indexBufferOffset % indexStride != 0 || indexBufferOffset > realIndex->length() ||
       indexCount > (realIndex->length() - indexBufferOffset) / indexStride ||
       !((uint64_t)IndirectRenderCommand->m_Parent->CommandTypes() &
         (uint64_t)MTL::IndirectCommandTypeDrawIndexed))
    {
      RDCERR("Invalid Metal indexed ICB arguments or index buffer range");
      return false;
    }
    Unwrap(IndirectRenderCommand)
        ->drawIndexedPrimitives(primitive, indexCount, indexType, realIndex,
                                indexBufferOffset, instanceCount, baseVertex, baseInstance);
    MetalIndirectDraw &draw = IndirectRenderCommand->m_Parent->Draw(IndirectRenderCommand->m_Index);
    draw.primitive = primitive;
    draw.indexCount = indexCount;
    draw.indexType = indexType;
    draw.indexBuffer = indexBuffer;
    draw.indexBufferOffset = indexBufferOffset;
    draw.instanceCount = instanceCount;
    draw.baseVertex = baseVertex;
    draw.baseInstance = baseInstance;
    draw.indexed = true;
    draw.encoded = true;
  }
  return true;
}

void WrappedMTLIndirectRenderCommand::drawIndexedPrimitives(
    MTL::PrimitiveType primitive, NS::UInteger indexCount, MTL::IndexType indexType,
    WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset,
    NS::UInteger instanceCount, NS::Integer baseVertex, NS::UInteger baseInstance)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawIndexedPrimitives(
      primitive, indexCount, indexType, Unwrap(indexBuffer), indexBufferOffset,
      instanceCount, baseVertex, baseInstance));
  if(m_Parent)
  {
    MetalIndirectDraw &draw = m_Parent->Draw(m_Index);
    draw.primitive = primitive;
    draw.indexCount = indexCount;
    draw.indexType = indexType;
    draw.indexBuffer = indexBuffer;
    draw.indexBufferOffset = indexBufferOffset;
    draw.instanceCount = instanceCount;
    draw.baseVertex = baseVertex;
    draw.baseInstance = baseInstance;
    draw.indexed = true;
    draw.encoded = true;
  }
  if(IsCaptureMode(m_State) && m_Parent)
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIndirectRenderCommand_drawIndexedPrimitives);
    Serialise_drawIndexedPrimitives(ser, primitive, indexCount, indexType, indexBuffer,
                                    indexBufferOffset, instanceCount, baseVertex, baseInstance);
    MetalResourceRecord *record = GetRecord(m_Parent);
    record->AddChunk(scope.Get());
    if(indexBuffer)
      record->AddParent(GetRecord(indexBuffer));
  }
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLIndirectCommandBuffer,
                                            WrappedMTLIndirectRenderCommand *,
                                            indirectRenderCommand, NS::UInteger);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLIndirectCommandBuffer, void, reset, NS::Range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLIndirectRenderCommand, void, setRenderPipelineState,
                                WrappedMTLRenderPipelineState *);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLIndirectRenderCommand, void, setVertexBuffer,
                                WrappedMTLBuffer *, NS::UInteger, NS::UInteger);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLIndirectRenderCommand, void, drawPrimitives,
                                MTL::PrimitiveType, NS::UInteger, NS::UInteger, NS::UInteger,
                                NS::UInteger);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLIndirectRenderCommand, void, drawIndexedPrimitives,
                                MTL::PrimitiveType, NS::UInteger, MTL::IndexType,
                                WrappedMTLBuffer *, NS::UInteger, NS::UInteger, NS::Integer,
                                NS::UInteger);
