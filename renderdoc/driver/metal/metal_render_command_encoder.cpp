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

#include "metal_render_command_encoder.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_depth_stencil_state.h"
#include "metal_manager.h"
#include "metal_render_pipeline_state.h"
#include "metal_replay.h"
#include "metal_sampler_state.h"
#include "metal_indirect_command_buffer.h"
#include "metal_texture.h"

WrappedMTLRenderCommandEncoder::WrappedMTLRenderCommandEncoder(
    MTL::RenderCommandEncoder *realMTLRenderCommandEncoder, ResourceId objId,
    WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLRenderCommandEncoder, objId, wrappedMTLDevice,
                       wrappedMTLDevice->GetStateRef())
{
  if(realMTLRenderCommandEncoder && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setRenderPipelineState(
    SerialiserType &ser, WrappedMTLRenderPipelineState *pipelineState)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(pipelineState).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_EncoderPipeline = pipelineState;
    Unwrap(RenderCommandEncoder)->setRenderPipelineState(Unwrap(pipelineState));
    m_Device->GetReplay()->BindRenderPipeline(GetResID(pipelineState));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setRenderPipelineState(WrappedMTLRenderPipelineState *pipelineState)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setRenderPipelineState(Unwrap(pipelineState)));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setRenderPipelineState);
      Serialise_setRenderPipelineState(ser, pipelineState);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(pipelineState), eFrameRef_Read);
  }
  else
  {
// TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexBuffer(SerialiserType &ser,
                                                               WrappedMTLBuffer *buffer,
                                                               NS::UInteger offset,
                                                               NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(RenderCommandEncoder == NULL || buffer == NULL)
    {
      RDCERR("Missing Metal vertex buffer at slot %llu", (uint64_t)index);
      return false;
    }
    if(index >= 31)
    {
      RDCERR("Invalid Metal vertex buffer slot %llu", (uint64_t)index);
      return false;
    }
    if(offset >= Unwrap(buffer)->length())
    {
      RDCERR("Invalid Metal vertex buffer offset %llu for %llu-byte buffer at slot %llu",
             (uint64_t)offset, (uint64_t)Unwrap(buffer)->length(), (uint64_t)index);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setVertexBuffer(Unwrap(buffer), offset, index);
    if(index < 2)
    {
      m_EncoderVertexBuffers[index] = buffer;
      m_EncoderVertexOffsets[index] = offset;
    }
    m_Device->GetReplay()->BindVertexBuffer((uint32_t)index, GetResID(buffer), (uint64_t)offset);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexBuffer(WrappedMTLBuffer *buffer, NS::UInteger offset,
                                                     NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexBuffer(Unwrap(buffer), offset, index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexBuffer);
      Serialise_setVertexBuffer(ser, buffer, offset, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexTexture(SerialiserType &ser,
                                                                WrappedMTLTexture *texture,
                                                                NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(index >= 128 || texture == NULL || RenderCommandEncoder == NULL)
    {
      RDCERR("Cannot replay Metal vertex texture slot %llu with a null resource or encoder",
             (uint64_t)index);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setVertexTexture(Unwrap(texture), index);
    m_Device->GetReplay()->BindVertexTexture((uint32_t)index, GetResID(texture));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexTexture(WrappedMTLTexture *texture,
                                                      NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexTexture(Unwrap(texture), index));
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexTexture);
      Serialise_setVertexTexture(ser, texture, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    record->MarkResourceFrameReferenced(GetResID(texture), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexSamplerState(
    SerialiserType &ser, WrappedMTLSamplerState *sampler, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(sampler).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(index >= 128 || sampler == NULL || RenderCommandEncoder == NULL)
    {
      RDCERR("Cannot replay Metal vertex sampler slot %llu with a null resource or encoder",
             (uint64_t)index);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setVertexSamplerState(Unwrap(sampler), index);
    m_Device->GetReplay()->BindVertexSampler((uint32_t)index, GetResID(sampler));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexSamplerState(WrappedMTLSamplerState *sampler,
                                                           NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexSamplerState(Unwrap(sampler), index));
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexSamplerState);
      Serialise_setVertexSamplerState(ser, sampler, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexTextures(
    SerialiserType &ser, rdcarray<WrappedMTLTexture *> textures, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (range.location >= 128 || range.length > 128 - range.location))
  {
    RDCERR("Invalid Metal vertex texture batch range %llu+%llu",
           (uint64_t)range.location, (uint64_t)range.length);
    return false;
  }

  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLTexture *resource : textures)
      bound.push_back(resource != NULL ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(textures).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(RenderCommandEncoder == NULL || textures.size() != range.length ||
       bound.size() != range.length)
    {
      RDCERR("Invalid Metal vertex texture batch resource count");
      return false;
    }
    rdcarray<const MTL::Texture *> real;
    for(size_t i = 0; i < textures.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0 && textures[i] == NULL) ||
         (bound[i] == 0 && textures[i] != NULL))
      {
        RDCERR("Missing or invalid Metal vertex texture resource at slot %llu",
               (uint64_t)(range.location + i));
        return false;
      }
      real.push_back(Unwrap(textures[i]));
    }
    Unwrap(RenderCommandEncoder)->setVertexTextures(real.data(), range);
    for(size_t i = 0; i < textures.size(); i++)
      m_Device->GetReplay()->BindVertexTexture((uint32_t)(range.location + i),
                                    GetResID(textures[i]));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexTextures(rdcarray<WrappedMTLTexture *> textures,
                                               NS::Range range)
{
  rdcarray<const MTL::Texture *> real;
  for(WrappedMTLTexture *resource : textures)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexTextures(real.data(), range));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexTextures);
      Serialise_setVertexTextures(ser, textures, range);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    for(WrappedMTLTexture *resource : textures)
      if(resource != NULL)
        record->MarkResourceFrameReferenced(GetResID(resource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexSamplerStates(
    SerialiserType &ser, rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (range.location >= 128 || range.length > 128 - range.location))
  {
    RDCERR("Invalid Metal vertex sampler batch range %llu+%llu",
           (uint64_t)range.location, (uint64_t)range.length);
    return false;
  }

  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLSamplerState *resource : samplers)
      bound.push_back(resource != NULL ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(samplers).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(RenderCommandEncoder == NULL || samplers.size() != range.length ||
       bound.size() != range.length)
    {
      RDCERR("Invalid Metal vertex sampler batch resource count");
      return false;
    }
    rdcarray<const MTL::SamplerState *> real;
    for(size_t i = 0; i < samplers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0 && samplers[i] == NULL) ||
         (bound[i] == 0 && samplers[i] != NULL))
      {
        RDCERR("Missing or invalid Metal vertex sampler resource at slot %llu",
               (uint64_t)(range.location + i));
        return false;
      }
      real.push_back(Unwrap(samplers[i]));
    }
    Unwrap(RenderCommandEncoder)->setVertexSamplerStates(real.data(), range);
    for(size_t i = 0; i < samplers.size(); i++)
      m_Device->GetReplay()->BindVertexSampler((uint32_t)(range.location + i),
                                    GetResID(samplers[i]));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexSamplerStates(rdcarray<WrappedMTLSamplerState *> samplers,
                                               NS::Range range)
{
  rdcarray<const MTL::SamplerState *> real;
  for(WrappedMTLSamplerState *resource : samplers)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexSamplerStates(real.data(), range));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexSamplerStates);
      Serialise_setVertexSamplerStates(ser, samplers, range);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    for(WrappedMTLSamplerState *resource : samplers)
      if(resource != NULL)
        record->MarkResourceFrameReferenced(GetResID(resource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentTextures(
    SerialiserType &ser, rdcarray<WrappedMTLTexture *> textures, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (range.location >= 128 || range.length > 128 - range.location))
  {
    RDCERR("Invalid Metal fragment texture batch range %llu+%llu",
           (uint64_t)range.location, (uint64_t)range.length);
    return false;
  }

  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLTexture *resource : textures)
      bound.push_back(resource != NULL ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(textures).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(RenderCommandEncoder == NULL || textures.size() != range.length ||
       bound.size() != range.length)
    {
      RDCERR("Invalid Metal fragment texture batch resource count");
      return false;
    }
    rdcarray<const MTL::Texture *> real;
    for(size_t i = 0; i < textures.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0 && textures[i] == NULL) ||
         (bound[i] == 0 && textures[i] != NULL))
      {
        RDCERR("Missing or invalid Metal fragment texture resource at slot %llu",
               (uint64_t)(range.location + i));
        return false;
      }
      real.push_back(Unwrap(textures[i]));
    }
    Unwrap(RenderCommandEncoder)->setFragmentTextures(real.data(), range);
    for(size_t i = 0; i < textures.size(); i++)
      m_Device->GetReplay()->BindFragmentTexture((uint32_t)(range.location + i),
                                    GetResID(textures[i]));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentTextures(rdcarray<WrappedMTLTexture *> textures,
                                               NS::Range range)
{
  rdcarray<const MTL::Texture *> real;
  for(WrappedMTLTexture *resource : textures)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentTextures(real.data(), range));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentTextures);
      Serialise_setFragmentTextures(ser, textures, range);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    for(WrappedMTLTexture *resource : textures)
      if(resource != NULL)
        record->MarkResourceFrameReferenced(GetResID(resource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentSamplerStates(
    SerialiserType &ser, rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (range.location >= 128 || range.length > 128 - range.location))
  {
    RDCERR("Invalid Metal fragment sampler batch range %llu+%llu",
           (uint64_t)range.location, (uint64_t)range.length);
    return false;
  }

  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLSamplerState *resource : samplers)
      bound.push_back(resource != NULL ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(samplers).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(RenderCommandEncoder == NULL || samplers.size() != range.length ||
       bound.size() != range.length)
    {
      RDCERR("Invalid Metal fragment sampler batch resource count");
      return false;
    }
    rdcarray<const MTL::SamplerState *> real;
    for(size_t i = 0; i < samplers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0 && samplers[i] == NULL) ||
         (bound[i] == 0 && samplers[i] != NULL))
      {
        RDCERR("Missing or invalid Metal fragment sampler resource at slot %llu",
               (uint64_t)(range.location + i));
        return false;
      }
      real.push_back(Unwrap(samplers[i]));
    }
    Unwrap(RenderCommandEncoder)->setFragmentSamplerStates(real.data(), range);
    for(size_t i = 0; i < samplers.size(); i++)
      m_Device->GetReplay()->BindFragmentSampler((uint32_t)(range.location + i),
                                    GetResID(samplers[i]));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentSamplerStates(rdcarray<WrappedMTLSamplerState *> samplers,
                                               NS::Range range)
{
  rdcarray<const MTL::SamplerState *> real;
  for(WrappedMTLSamplerState *resource : samplers)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentSamplerStates(real.data(), range));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerStates);
      Serialise_setFragmentSamplerStates(ser, samplers, range);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    for(WrappedMTLSamplerState *resource : samplers)
      if(resource != NULL)
        record->MarkResourceFrameReferenced(GetResID(resource), eFrameRef_Read);
  }
}
template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentBuffer(SerialiserType &ser,
                                                                 WrappedMTLBuffer *buffer,
                                                                 NS::UInteger offset,
                                                                 NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setFragmentBuffer(Unwrap(buffer), offset, index);
    m_Device->GetReplay()->BindFragmentBuffer((uint32_t)index, GetResID(buffer), (uint64_t)offset);
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentBufferOffset(SerialiserType &ser,
                                                                       NS::UInteger offset,
                                                                       NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setFragmentBufferOffset(offset, index);
    m_Device->GetReplay()->SetFragmentBufferOffset((uint32_t)index, (uint64_t)offset);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentBufferOffset(NS::UInteger offset,
                                                              NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentBufferOffset(offset, index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentBufferOffset);
      Serialise_setFragmentBufferOffset(ser, offset, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
  }
}

void WrappedMTLRenderCommandEncoder::setFragmentBuffer(WrappedMTLBuffer *buffer,
                                                       NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentBuffer(Unwrap(buffer), offset, index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentBuffer);
      Serialise_setFragmentBuffer(ser, buffer, offset, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentTexture(SerialiserType &ser,
                                                                  WrappedMTLTexture *texture,
                                                                  NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setFragmentTexture(Unwrap(texture), index);
    m_Device->GetReplay()->BindFragmentTexture((uint32_t)index, GetResID(texture));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentTexture(WrappedMTLTexture *texture, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentTexture(Unwrap(texture), index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentTexture);
      Serialise_setFragmentTexture(ser, texture, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(texture), eFrameRef_Read);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentSamplerState(
    SerialiserType &ser, WrappedMTLSamplerState *sampler, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(sampler).Important();
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setFragmentSamplerState(Unwrap(sampler), index);
    m_Device->GetReplay()->BindFragmentSampler((uint32_t)index, GetResID(sampler));
  }

  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentSamplerState(WrappedMTLSamplerState *sampler,
                                                              NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentSamplerState(Unwrap(sampler), index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerState);
      Serialise_setFragmentSamplerState(ser, sampler, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_useResource(SerialiserType &ser,
                                                           WrappedMTLResource *resource,
                                                           MTL::ResourceUsage usage)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(resource).Important();
  uint64_t usageValue = (uint64_t)usage;
  SERIALISE_ELEMENT(usageValue).Important();
  SERIALISE_CHECK_READ_ERRORS();

  usage = (MTL::ResourceUsage)usageValue;

  if(IsReplayingAndReading())
  {
    if(RenderCommandEncoder == NULL || resource == NULL)
    {
      RDCERR("Cannot replay Metal useResource with a null encoder or resource");
      return false;
    }
    Unwrap(RenderCommandEncoder)->useResource(Unwrap(resource), usage);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::useResource(WrappedMTLResource *resource,
                                                 MTL::ResourceUsage usage)
{
  SERIALISE_TIME_CALL(Unwrap(this)->useResource(Unwrap(resource), usage));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_useResource);
    Serialise_useResource(ser, resource, usage);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(resource),
                                        (usage & MTL::ResourceUsageWrite) ? eFrameRef_ReadBeforeWrite
                                                                         : eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setViewport(SerialiserType &ser,
                                                           MTL::Viewport &viewport)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(viewport).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setViewport(viewport);
    m_Device->GetReplay()->SetViewport(viewport);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setViewport(MTL::Viewport &viewport)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setViewport(viewport));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setViewport);
      Serialise_setViewport(ser, viewport);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setScissorRect(SerialiserType &ser,
                                                              MTL::ScissorRect &rect)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(rect).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setScissorRect(rect);
    m_Device->GetReplay()->SetScissor(rect);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setScissorRect(MTL::ScissorRect &rect)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setScissorRect(rect));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setScissorRect);
      Serialise_setScissorRect(ser, rect);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFrontFacingWinding(SerialiserType &ser,
                                                                    MTL::Winding winding)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(winding).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setFrontFacingWinding(winding);
    m_Device->GetReplay()->SetFrontFacingWinding(winding);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFrontFacingWinding(MTL::Winding winding)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFrontFacingWinding(winding));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFrontFacingWinding);
      Serialise_setFrontFacingWinding(ser, winding);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setCullMode(SerialiserType &ser,
                                                           MTL::CullMode cullMode)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(cullMode).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setCullMode(cullMode);
    m_Device->GetReplay()->SetCullMode(cullMode);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setCullMode(MTL::CullMode cullMode)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setCullMode(cullMode));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setCullMode);
      Serialise_setCullMode(ser, cullMode);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setDepthStencilState(
    SerialiserType &ser, WrappedMTLDepthStencilState *depthStencilState)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(depthStencilState).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setDepthStencilState(Unwrap(depthStencilState));
    m_Device->GetReplay()->BindDepthStencilState(GetResID(depthStencilState));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setDepthStencilState(
    WrappedMTLDepthStencilState *depthStencilState)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setDepthStencilState(Unwrap(depthStencilState)));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setDepthStencilState);
      Serialise_setDepthStencilState(ser, depthStencilState);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(depthStencilState), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setStencilReferenceValue(
    SerialiserType &ser, uint32_t referenceValue)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(referenceValue).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->setStencilReferenceValue(referenceValue);
    m_Device->GetReplay()->SetStencilReferenceValue(referenceValue);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setStencilReferenceValue(uint32_t referenceValue)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setStencilReferenceValue(referenceValue));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setStencilReferenceValue);
      Serialise_setStencilReferenceValue(ser, referenceValue);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setStencilReferenceValues(
    SerialiserType &ser, uint32_t frontReferenceValue, uint32_t backReferenceValue)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(frontReferenceValue).Important();
  SERIALISE_ELEMENT(backReferenceValue).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)
        ->setStencilReferenceValues(frontReferenceValue, backReferenceValue);
    m_Device->GetReplay()->SetStencilReferenceValues(frontReferenceValue, backReferenceValue);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setStencilReferenceValues(uint32_t frontReferenceValue,
                                                                uint32_t backReferenceValue)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setStencilReferenceValues(frontReferenceValue,
                                                               backReferenceValue));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setStencilFrontReferenceValue);
      Serialise_setStencilReferenceValues(ser, frontReferenceValue, backReferenceValue);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitiveType, NS::UInteger vertexStart,
    NS::UInteger vertexCount, NS::UInteger instanceCount, NS::UInteger baseInstance)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(primitiveType);
  SERIALISE_ELEMENT(vertexStart);
  SERIALISE_ELEMENT(vertexCount).Important();
  SERIALISE_ELEMENT(instanceCount);
  SERIALISE_ELEMENT(baseInstance);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    const char *primitiveName = NULL;
    switch(primitiveType)
    {
      case MTL::PrimitiveTypePoint: primitiveName = "Point"; break;
      case MTL::PrimitiveTypeLine: primitiveName = "Line"; break;
      case MTL::PrimitiveTypeLineStrip: primitiveName = "Line Strip"; break;
      case MTL::PrimitiveTypeTriangle:
      case MTL::PrimitiveTypeTriangleStrip: break;
      default:
        RDCERR("Invalid Metal drawPrimitives primitive type %llu", (uint64_t)primitiveType);
        return false;
    }
    if(vertexStart > UINT32_MAX || vertexCount == 0 || vertexCount > UINT32_MAX ||
       instanceCount == 0 || instanceCount > UINT32_MAX || baseInstance > UINT32_MAX)
    {
      RDCERR("Invalid Metal drawPrimitives vertex/instance count or 32-bit action range");
      return false;
    }
    m_Device->GetReplay()->SetPrimitiveTopology(primitiveType);
    m_Device->GetReplay()->SetIndirectBuffer(ResourceId(), 0, 0);
    Unwrap(RenderCommandEncoder)
        ->drawPrimitives(primitiveType, vertexStart, vertexCount, instanceCount, baseInstance);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = primitiveName
                              ? StringFormat::Fmt("drawPrimitives(%s, %llu)", primitiveName,
                                                  (uint64_t)vertexCount)
                              : StringFormat::Fmt("drawPrimitives(%llu)", (uint64_t)vertexCount);
      action.flags = ActionFlags::Drawcall;
      if(instanceCount > 1 || baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = (uint32_t)vertexCount;
      action.numInstances = (uint32_t)instanceCount;
      action.vertexOffset = (uint32_t)vertexStart;
      action.instanceOffset = (uint32_t)baseInstance;
      m_Device->GetReplay()->SetActionOutputs(action);
      AddAction(action);
    }
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitiveType, WrappedMTLBuffer *indirectBuffer,
    NS::UInteger indirectBufferOffset)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(primitiveType);
  SERIALISE_ELEMENT(indirectBuffer).Important();
  SERIALISE_ELEMENT(indirectBufferOffset).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    static const uint64_t IndirectArgumentSize = sizeof(uint32_t) * 4;
    MTL::Buffer *realBuffer = Unwrap(indirectBuffer);
    if(realBuffer == NULL || (indirectBufferOffset & 3) != 0 ||
       indirectBufferOffset > realBuffer->length() ||
       IndirectArgumentSize > realBuffer->length() - indirectBufferOffset ||
       realBuffer->contents() == NULL)
    {
      RDCERR("Invalid Metal drawPrimitives indirect argument buffer or offset");
      return false;
    }

    const uint32_t *arguments = (const uint32_t *)((const byte *)realBuffer->contents() +
                                                   indirectBufferOffset);
    const uint32_t vertexCount = arguments[0];
    const uint32_t instanceCount = arguments[1];
    const uint32_t vertexStart = arguments[2];
    const uint32_t baseInstance = arguments[3];
    MetalReplay *replay = m_Device->GetReplay();
    replay->SetPrimitiveTopology(primitiveType);
    replay->SetIndirectBuffer(GetResID(indirectBuffer), indirectBufferOffset,
                              IndirectArgumentSize);
    Unwrap(RenderCommandEncoder)
        ->drawPrimitives(primitiveType, realBuffer, indirectBufferOffset);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawPrimitives(indirect, %u vertices, %u instances)",
                                             vertexCount, instanceCount);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indirect;
      if(instanceCount > 1 || baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = vertexCount;
      action.numInstances = instanceCount;
      action.vertexOffset = vertexStart;
      action.instanceOffset = baseInstance;
      replay->SetActionOutputs(action);
      AddAction(action);
      replay->AddUsage(GetResID(indirectBuffer), ResourceUsage::Indirect);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType,
                                                    WrappedMTLBuffer *indirectBuffer,
                                                    NS::UInteger indirectBufferOffset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawPrimitives(primitiveType, Unwrap(indirectBuffer),
                                                   indirectBufferOffset));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawPrimitives_indirect);
    Serialise_drawPrimitives(ser, primitiveType, indirectBuffer, indirectBufferOffset);
    MetalResourceRecord *commandBufferRecord = GetRecord(m_CommandBuffer);
    commandBufferRecord->AddChunk(scope.Get());
    commandBufferRecord->MarkResourceFrameReferenced(GetResID(indirectBuffer), eFrameRef_Read);
  }
}

void WrappedMTLRenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType,
                                                    NS::UInteger vertexStart,
                                                    NS::UInteger vertexCount,
                                                    NS::UInteger instanceCount,
                                                    NS::UInteger baseInstance)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawPrimitives(primitiveType, vertexStart, vertexCount,
                                                   instanceCount, baseInstance));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawPrimitives_instanced);
      Serialise_drawPrimitives(ser, primitiveType, vertexStart, vertexCount, instanceCount,
                               baseInstance);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawIndexedPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitiveType, NS::UInteger indexCount,
    MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(primitiveType);
  SERIALISE_ELEMENT(indexCount).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexBuffer).Important();
  SERIALISE_ELEMENT(indexBufferOffset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Device->GetReplay()->SetPrimitiveTopology(primitiveType);
    m_Device->GetReplay()->SetIndirectBuffer(ResourceId(), 0, 0);
    m_Device->GetReplay()->BindIndexBuffer(GetResID(indexBuffer), indexBufferOffset, indexType);
    Unwrap(RenderCommandEncoder)
        ->drawIndexedPrimitives(primitiveType, indexCount, indexType, Unwrap(indexBuffer),
                                indexBufferOffset);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawIndexedPrimitives(%llu)", (uint64_t)indexCount);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indexed;
      action.numIndices = (uint32_t)indexCount;
      action.numInstances = 1;
      action.indexOffset = (uint32_t)(indexBufferOffset /
                                      (indexType == MTL::IndexTypeUInt16 ? 2 : 4));
      m_Device->GetReplay()->SetActionOutputs(action);
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawIndexedPrimitives(
    MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType,
    WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawIndexedPrimitives(
      primitiveType, indexCount, indexType, Unwrap(indexBuffer), indexBufferOffset));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives);
      Serialise_drawIndexedPrimitives(ser, primitiveType, indexCount, indexType, indexBuffer,
                                      indexBufferOffset);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(indexBuffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawIndexedPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitiveType, NS::UInteger indexCount,
    MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset,
    NS::UInteger instanceCount, NS::Integer baseVertex, NS::UInteger baseInstance)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(primitiveType);
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
    MTL::Buffer *realBuffer = Unwrap(indexBuffer);
    const uint64_t indexStride = indexType == MTL::IndexTypeUInt16 ? 2 : 4;
    if(realBuffer == NULL ||
       (indexType != MTL::IndexTypeUInt16 && indexType != MTL::IndexTypeUInt32) ||
       indexBufferOffset % indexStride != 0 || indexBufferOffset > realBuffer->length() ||
       indexCount > (realBuffer->length() - indexBufferOffset) / indexStride ||
       indexCount > UINT32_MAX || instanceCount > UINT32_MAX || baseInstance > UINT32_MAX ||
       baseVertex < INT32_MIN || baseVertex > INT32_MAX)
    {
      RDCERR("Invalid Metal indexed instancing buffer range or draw arguments");
      return false;
    }

    MetalReplay *replay = m_Device->GetReplay();
    replay->SetPrimitiveTopology(primitiveType);
    replay->SetIndirectBuffer(ResourceId(), 0, 0);
    replay->BindIndexBuffer(GetResID(indexBuffer), indexBufferOffset, indexType, indexCount);
    Unwrap(RenderCommandEncoder)
        ->drawIndexedPrimitives(primitiveType, indexCount, indexType, realBuffer,
                                indexBufferOffset, instanceCount, baseVertex, baseInstance);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawIndexedPrimitives(%llu, %llu instances, baseVertex %lld, baseInstance %llu)",
                                            (uint64_t)indexCount, (uint64_t)instanceCount,
                                            (int64_t)baseVertex, (uint64_t)baseInstance);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indexed;
      if(instanceCount > 1 || baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = (uint32_t)indexCount;
      action.numInstances = (uint32_t)instanceCount;
      action.baseVertex = (int32_t)baseVertex;
      action.instanceOffset = (uint32_t)baseInstance;
      // The index binding starts at indexBufferOffset, so indexOffset is relative to that binding.
      action.indexOffset = 0;
      replay->SetActionOutputs(action);
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawIndexedPrimitives(
    MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType,
    WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset, NS::UInteger instanceCount,
    NS::Integer baseVertex, NS::UInteger baseInstance)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawIndexedPrimitives(
      primitiveType, indexCount, indexType, Unwrap(indexBuffer), indexBufferOffset,
      instanceCount, baseVertex, baseInstance));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced_base);
    Serialise_drawIndexedPrimitives(ser, primitiveType, indexCount, indexType, indexBuffer,
                                    indexBufferOffset, instanceCount, baseVertex, baseInstance);
    MetalResourceRecord *commandBufferRecord = GetRecord(m_CommandBuffer);
    commandBufferRecord->AddChunk(scope.Get());
    commandBufferRecord->MarkResourceFrameReferenced(GetResID(indexBuffer), eFrameRef_Read);
  }
}

void WrappedMTLRenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType,
                                                    NS::UInteger vertexStart,
                                                    NS::UInteger vertexCount)
{
  drawPrimitives(primitiveType, vertexStart, vertexCount, 1, 0);
}

void WrappedMTLRenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType,
                                                    NS::UInteger vertexStart,
                                                    NS::UInteger vertexCount,
                                                    NS::UInteger instanceCount)
{
  drawPrimitives(primitiveType, vertexStart, vertexCount, instanceCount, 0);
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_endEncoding(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    Unwrap(RenderCommandEncoder)->endEncoding();
    m_Device->SetReplayRenderCommandEncoder(NULL);

    ActionDescription action;
    if(IsLoading(m_State))
    {
      action.customName = StringFormat::Fmt(
          "End Metal Render Pass (%s)",
          RDMTL::RenderPassOpString(m_Device->GetReplay()->GetRenderPassDescriptor(), true).c_str());
      action.flags = ActionFlags::PassBoundary | ActionFlags::EndPass;
      m_Device->GetReplay()->SetActionOutputs(action);
    }

    m_Device->GetReplay()->EndRenderPass();

    if(IsLoading(m_State))
    {
      AddEvent();
      AddAction(action);
      m_Device->GetReplay()->AddRenderPassStoreUsage(
          m_Device->GetReplay()->GetRenderPassDescriptor());
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::endEncoding()
{
  SERIALISE_TIME_CALL(Unwrap(this)->endEncoding());

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_endEncoding);
      Serialise_endEncoding(ser);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, endEncoding);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setRenderPipelineState,
                                WrappedMTLRenderPipelineState *pipelineState);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexBuffer,
                                WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexTexture,
                                WrappedMTLTexture *texture, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexTextures,
                                rdcarray<WrappedMTLTexture *> textures, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexSamplerState,
                                WrappedMTLSamplerState *sampler, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexSamplerStates,
                                rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentBuffer,
                                WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentBufferOffset,
                                NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentTexture,
                                WrappedMTLTexture *texture, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentTextures,
                                rdcarray<WrappedMTLTexture *> textures, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentSamplerState,
                                WrappedMTLSamplerState *sampler, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentSamplerStates,
                                rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, useResource,
                                WrappedMTLResource *resource, MTL::ResourceUsage usage);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setViewport,
                                MTL::Viewport &viewport);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setScissorRect,
                                MTL::ScissorRect &rect);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFrontFacingWinding,
                                MTL::Winding winding);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setCullMode,
                                MTL::CullMode cullMode);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setDepthStencilState,
                                WrappedMTLDepthStencilState *depthStencilState);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setStencilReferenceValue,
                                uint32_t referenceValue);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setStencilReferenceValues,
                                uint32_t frontReferenceValue, uint32_t backReferenceValue);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawPrimitives,
                                MTL::PrimitiveType primitiveType, NS::UInteger vertexStart,
                                NS::UInteger vertexCount, NS::UInteger instanceCount,
                                NS::UInteger baseInstance);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawPrimitives,
                                MTL::PrimitiveType primitiveType,
                                WrappedMTLBuffer *indirectBuffer,
                                NS::UInteger indirectBufferOffset);
template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawIndexedPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitiveType, MTL::IndexType indexType,
    WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset,
    WrappedMTLBuffer *indirectBuffer, NS::UInteger indirectBufferOffset)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(primitiveType);
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexBuffer).Important();
  SERIALISE_ELEMENT(indexBufferOffset).Important();
  SERIALISE_ELEMENT(indirectBuffer).Important();
  SERIALISE_ELEMENT(indirectBufferOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    MTL::Buffer *realIndex = Unwrap(indexBuffer);
    MTL::Buffer *realIndirect = Unwrap(indirectBuffer);
    const uint64_t indexStride = indexType == MTL::IndexTypeUInt16 ? 2 : 4;
    const uint64_t argumentSize = sizeof(MTL::DrawIndexedPrimitivesIndirectArguments);
    if(!RenderCommandEncoder || !realIndex || !realIndirect ||
       (indexType != MTL::IndexTypeUInt16 && indexType != MTL::IndexTypeUInt32) ||
       indexBufferOffset % indexStride != 0 || indexBufferOffset > realIndex->length() ||
       (indirectBufferOffset & 3) != 0 || indirectBufferOffset > realIndirect->length() ||
       argumentSize > realIndirect->length() - indirectBufferOffset ||
       realIndirect->contents() == NULL)
    {
      RDCERR("Invalid Metal indexed indirect buffers, alignment or argument offset");
      return false;
    }

    MTL::DrawIndexedPrimitivesIndirectArguments args;
    memcpy(&args, (const byte *)realIndirect->contents() + indirectBufferOffset, sizeof(args));
    if(!args.indexCount || !args.instanceCount ||
       args.indexStart > (realIndex->length() - indexBufferOffset) / indexStride ||
       args.indexCount > (realIndex->length() - indexBufferOffset) / indexStride - args.indexStart)
    {
      RDCERR("Invalid Metal indexed indirect indexStart/indexCount range");
      return false;
    }

    const uint64_t selectedOffset = indexBufferOffset + uint64_t(args.indexStart) * indexStride;
    MetalReplay *replay = m_Device->GetReplay();
    replay->SetPrimitiveTopology(primitiveType);
    replay->BindIndexBuffer(GetResID(indexBuffer), selectedOffset, indexType, args.indexCount);
    replay->SetIndirectBuffer(GetResID(indirectBuffer), indirectBufferOffset, argumentSize);
    Unwrap(RenderCommandEncoder)
        ->drawIndexedPrimitives(primitiveType, indexType, realIndex, indexBufferOffset,
                                realIndirect, indirectBufferOffset);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt(
          "drawIndexedPrimitives(indirect, %u indices, %u instances, indexStart %u, baseVertex %d, baseInstance %u)",
          args.indexCount, args.instanceCount, args.indexStart, args.baseVertex,
          args.baseInstance);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indexed | ActionFlags::Indirect;
      if(args.instanceCount > 1 || args.baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = args.indexCount;
      action.numInstances = args.instanceCount;
      action.indexOffset = 0;
      action.baseVertex = args.baseVertex;
      action.instanceOffset = args.baseInstance;
      replay->SetActionOutputs(action);
      AddAction(action);
      replay->AddUsage(GetResID(indirectBuffer), ResourceUsage::Indirect);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawIndexedPrimitives(
    MTL::PrimitiveType primitiveType, MTL::IndexType indexType,
    WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset,
    WrappedMTLBuffer *indirectBuffer, NS::UInteger indirectBufferOffset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawIndexedPrimitives(
      primitiveType, indexType, Unwrap(indexBuffer), indexBufferOffset,
      Unwrap(indirectBuffer), indirectBufferOffset));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_indirect);
    Serialise_drawIndexedPrimitives(ser, primitiveType, indexType, indexBuffer,
                                    indexBufferOffset, indirectBuffer, indirectBufferOffset);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(indexBuffer), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetResID(indirectBuffer), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawIndexedPrimitives,
                                MTL::PrimitiveType, MTL::IndexType, WrappedMTLBuffer *,
                                NS::UInteger, WrappedMTLBuffer *, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_executeCommandsMarker(
    SerialiserType &ser, WrappedMTLIndirectCommandBuffer *icb, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(icb).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || !icb || !range.length || range.location >= icb->Count() ||
       range.length > icb->Count() - range.location)
    {
      RDCERR("Invalid Metal ICB execute marker range or resource");
      return false;
    }
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription execute;
      execute.customName = StringFormat::Fmt("executeCommandsInBuffer(location=%llu, length=%llu)",
                                             (uint64_t)range.location, (uint64_t)range.length);
      execute.flags = ActionFlags::MultiAction | ActionFlags::PushMarker;
      AddAction(execute);
      m_Device->GetReplay()->BeginMultiAction((uint32_t)range.length);
    }
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_executeCommandsInBuffer(
    SerialiserType &ser, WrappedMTLIndirectCommandBuffer *icb, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(icb).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || !icb || !icb->SupportedDescriptor() ||
       range.length != 1 || range.location >= icb->Count())
    {
      RDCERR("Invalid Metal ICB execute range, object or descriptor");
      return false;
    }
    const MetalIndirectDraw &draw = icb->Draw(range.location);
    WrappedMTLRenderPipelineState *pipeline =
        icb->InheritPipelineState() ? m_EncoderPipeline : draw.pipeline;
    WrappedMTLBuffer *vertexBuffers[2] = {};
    NS::UInteger vertexOffsets[2] = {};
    for(unsigned slot = 0; slot < 2; ++slot)
    {
      vertexBuffers[slot] = icb->InheritBuffers() ? m_EncoderVertexBuffers[slot]
                                                   : draw.vertexBuffers[slot];
      vertexOffsets[slot] = icb->InheritBuffers() ? m_EncoderVertexOffsets[slot]
                                                  : draw.vertexBufferOffsets[slot];
    }
    if(!draw.encoded || !pipeline ||
       (icb->InheritPipelineState() && draw.pipeline) ||
       (icb->InheritBuffers() && (draw.vertexBuffers[0] || draw.vertexBuffers[1])) ||
       !vertexBuffers[0] || vertexOffsets[0] >= Unwrap(vertexBuffers[0])->length() ||
       !draw.instanceCount ||
       (draw.indexed &&
        (!vertexBuffers[1] ||
         vertexOffsets[1] >= Unwrap(vertexBuffers[1])->length() ||
         !draw.indexBuffer || !draw.indexCount)) ||
       (!draw.indexed && !draw.vertexCount))
    {
      RDCERR("Metal ICB command lacks pipeline, vertex buffer or draw arguments");
      return false;
    }
    MetalReplay *replay = m_Device->GetReplay();
    replay->BindRenderPipeline(GetResID(pipeline));
    replay->BindVertexBuffer(0, GetResID(vertexBuffers[0]), vertexOffsets[0]);
    if(draw.indexed)
    {
      replay->BindVertexBuffer(1, GetResID(vertexBuffers[1]), vertexOffsets[1]);
      replay->BindIndexBuffer(GetResID(draw.indexBuffer), draw.indexBufferOffset,
                              draw.indexType, draw.indexCount);
    }
    replay->SetPrimitiveTopology(draw.primitive);
    replay->SetIndirectBuffer(ResourceId(), 0, 0);
    Unwrap(RenderCommandEncoder)->executeCommandsInBuffer(Unwrap(icb), range);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = draw.indexed
                              ? StringFormat::Fmt("ICB[%llu] drawIndexedPrimitives(%llu) instances=%llu",
                                                  (uint64_t)range.location,
                                                  (uint64_t)draw.indexCount,
                                                  (uint64_t)draw.instanceCount)
                              : StringFormat::Fmt("ICB[%llu] drawPrimitives(%llu) instances=%llu",
                                                  (uint64_t)range.location,
                                                  (uint64_t)draw.vertexCount,
                                                  (uint64_t)draw.instanceCount);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indirect;
      if(draw.indexed)
        action.flags |= ActionFlags::Indexed;
      if(draw.instanceCount > 1 || draw.baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = (uint32_t)(draw.indexed ? draw.indexCount : draw.vertexCount);
      action.numInstances = (uint32_t)draw.instanceCount;
      action.vertexOffset = (uint32_t)draw.vertexStart;
      action.baseVertex = (int32_t)draw.baseVertex;
      action.instanceOffset = (uint32_t)draw.baseInstance;
      replay->SetActionOutputs(action);
      AddAction(action);
      replay->AddUsage(GetResID(icb), ResourceUsage::Indirect);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::executeCommandsInBuffer(WrappedMTLIndirectCommandBuffer *icb,
                                                               NS::Range range)
{
  SERIALISE_TIME_CALL(Unwrap(this)->executeCommandsInBuffer(Unwrap(icb), range));
  if(IsCaptureMode(m_State))
  {
    if(!icb || !range.length || range.location >= icb->Count() ||
       range.length > icb->Count() - range.location)
    {
      RDCERR("Invalid Metal ICB capture execute range or object");
      return;
    }
    CACHE_THREAD_SERIALISER();
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    {
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_executeCommandsMarker);
      Serialise_executeCommandsMarker(ser, icb, range);
      record->AddChunk(scope.Get());
    }
    for(NS::UInteger index = range.location; index < range.location + range.length; ++index)
    {
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_executeCommandsInBuffer);
      Serialise_executeCommandsInBuffer(ser, icb, NS::Range::Make(index, 1));
      record->AddChunk(scope.Get());
    }
    record->MarkResourceFrameReferenced(GetResID(icb), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_executeCommandsMarker(
    ReadSerialiser &, WrappedMTLIndirectCommandBuffer *, NS::Range);
template bool WrappedMTLRenderCommandEncoder::Serialise_executeCommandsMarker(
    WriteSerialiser &, WrappedMTLIndirectCommandBuffer *, NS::Range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, executeCommandsInBuffer,
                                WrappedMTLIndirectCommandBuffer *, NS::Range);

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawIndexedPrimitives,
                                MTL::PrimitiveType primitiveType, NS::UInteger indexCount,
                                MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer,
                                NS::UInteger indexBufferOffset);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawIndexedPrimitives,
                                MTL::PrimitiveType primitiveType, NS::UInteger indexCount,
                                MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer,
                                NS::UInteger indexBufferOffset, NS::UInteger instanceCount,
                                NS::Integer baseVertex, NS::UInteger baseInstance);
