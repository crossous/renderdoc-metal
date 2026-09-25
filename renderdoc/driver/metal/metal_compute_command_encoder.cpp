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

#include "metal_compute_command_encoder.h"
#include "metal_command_buffer.h"
#include "metal_compute_pipeline_state.h"
#include "metal_replay.h"
#include "metal_texture.h"
#include "metal_buffer.h"
#include "metal_sampler_state.h"

WrappedMTLComputeCommandEncoder::WrappedMTLComputeCommandEncoder(MTL::ComputeCommandEncoder *real,
                                                                 ResourceId id,
                                                                 WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_endEncoding(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(ComputeCommandEncoder)->endEncoding();
    m_Device->SetReplayComputeCommandEncoder(NULL);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "End Metal Compute Pass";
      action.flags = ActionFlags::PassBoundary | ActionFlags::EndPass;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::endEncoding()
{
  SERIALISE_TIME_CALL(Unwrap(this)->endEncoding());
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_endEncoding);
    Serialise_endEncoding(ser);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setComputePipelineState(
    SerialiserType &ser, WrappedMTLComputePipelineState *pipeline)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(pipeline).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!pipeline)
      return false;
    Unwrap(ComputeCommandEncoder)->setComputePipelineState(Unwrap(pipeline));
    m_Device->GetReplay()->SetComputePipeline(GetResID(pipeline));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setComputePipelineState(
    WrappedMTLComputePipelineState *pipeline)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setComputePipelineState(Unwrap(pipeline)));
  m_Pipeline = pipeline;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setComputePipelineState);
    Serialise_setComputePipelineState(ser, pipeline);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(pipeline), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setTexture(SerialiserType &ser,
                                                             WrappedMTLTexture *texture,
                                                             NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(index);
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(index >= 128)
      return false;
    Unwrap(ComputeCommandEncoder)->setTexture(Unwrap(texture), index);
    m_Device->GetReplay()->SetComputeTexture((uint32_t)index, GetResID(texture));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setTexture(WrappedMTLTexture *texture, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTexture(Unwrap(texture), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setTexture);
    Serialise_setTexture(ser, texture, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(texture)
      record->MarkResourceFrameReferenced(GetResID(texture), eFrameRef_ReadBeforeWrite);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setTextures(
    SerialiserType &ser, rdcarray<WrappedMTLTexture *> textures, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && (range.location >= 128 || range.length > 128 - range.location))
  {
    RDCERR("Invalid Metal compute texture batch range");
    return false;
  }
  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLTexture *resource : textures)
      bound.push_back(resource ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(textures).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || textures.size() != range.length || bound.size() != range.length)
      return false;
    rdcarray<const MTL::Texture *> real;
    for(size_t i = 0; i < textures.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0) != (textures[i] != NULL))
      {
        RDCERR("Invalid Metal compute texture batch resource");
        return false;
      }
      real.push_back(Unwrap(textures[i]));
    }
    Unwrap(ComputeCommandEncoder)->setTextures(real.data(), range);
    for(size_t i = 0; i < textures.size(); i++)
      m_Device->GetReplay()->SetComputeTexture((uint32_t)(range.location + i),
                                                GetResID(textures[i]));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setTextures(rdcarray<WrappedMTLTexture *> textures,
                                                    NS::Range range)
{
  rdcarray<const MTL::Texture *> real;
  for(WrappedMTLTexture *resource : textures)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setTextures(real.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setTextures);
    Serialise_setTextures(ser, textures, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(size_t i = 0; i < textures.size(); i++)
      if(textures[i])
        record->MarkResourceFrameReferenced(GetResID(textures[i]), eFrameRef_ReadBeforeWrite);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setSamplerState(SerialiserType &ser,
                                                                  WrappedMTLSamplerState *sampler,
                                                                  NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(sampler).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(index >= 16 || !sampler)
    {
      RDCERR("Invalid Metal compute sampler binding");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->setSamplerState(Unwrap(sampler), index);
    m_Device->GetReplay()->BindComputeSampler((uint32_t)index, GetResID(sampler));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setSamplerState(WrappedMTLSamplerState *sampler,
                                                        NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setSamplerState(Unwrap(sampler), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setSamplerState);
    Serialise_setSamplerState(ser, sampler, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setSamplerStates(
    SerialiserType &ser, rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && (range.location >= 16 || range.length > 16 - range.location))
  {
    RDCERR("Invalid Metal compute sampler batch range");
    return false;
  }
  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLSamplerState *resource : samplers)
      bound.push_back(resource ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(samplers).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || samplers.size() != range.length || bound.size() != range.length)
      return false;
    rdcarray<const MTL::SamplerState *> real;
    for(size_t i = 0; i < samplers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0) != (samplers[i] != NULL))
      {
        RDCERR("Invalid Metal compute sampler batch resource");
        return false;
      }
      real.push_back(Unwrap(samplers[i]));
    }
    Unwrap(ComputeCommandEncoder)->setSamplerStates(real.data(), range);
    for(size_t i = 0; i < samplers.size(); i++)
      m_Device->GetReplay()->BindComputeSampler((uint32_t)(range.location + i),
                                                 GetResID(samplers[i]));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setSamplerStates(
    rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range)
{
  rdcarray<const MTL::SamplerState *> real;
  for(WrappedMTLSamplerState *resource : samplers)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setSamplerStates(real.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setSamplerStates);
    Serialise_setSamplerStates(ser, samplers, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLSamplerState *resource : samplers)
      if(resource)
        record->MarkResourceFrameReferenced(GetResID(resource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setBuffer(SerialiserType &ser,
                                                            WrappedMTLBuffer *buffer,
                                                            NS::UInteger offset,
                                                            NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(index >= 31 || (buffer && offset >= Unwrap(buffer)->length()) ||
       (!buffer && offset != 0))
    {
      RDCERR("Invalid Metal compute buffer binding or offset");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->setBuffer(Unwrap(buffer), offset, index);
    m_Device->GetReplay()->BindComputeBuffer((uint32_t)index, GetResID(buffer), (uint64_t)offset);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setBuffer(WrappedMTLBuffer *buffer, NS::UInteger offset,
                                                  NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setBuffer(Unwrap(buffer), offset, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setBuffer);
    Serialise_setBuffer(ser, buffer, offset, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(buffer)
      record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_ReadBeforeWrite);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setBuffers(
    SerialiserType &ser, rdcarray<WrappedMTLBuffer *> buffers,
    rdcarray<NS::UInteger> offsets, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && (range.location >= 31 || range.length > 31 - range.location))
  {
    RDCERR("Invalid Metal compute buffer batch range");
    return false;
  }
  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLBuffer *resource : buffers)
      bound.push_back(resource ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(buffers).Important();
  SERIALISE_ELEMENT(offsets).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || buffers.size() != range.length ||
       offsets.size() != range.length || bound.size() != range.length)
      return false;
    rdcarray<const MTL::Buffer *> real;
    for(size_t i = 0; i < buffers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0) != (buffers[i] != NULL) ||
         (buffers[i] && offsets[i] >= Unwrap(buffers[i])->length()) ||
         (!buffers[i] && offsets[i] != 0))
      {
        RDCERR("Invalid Metal compute buffer batch resource or offset");
        return false;
      }
      real.push_back(Unwrap(buffers[i]));
    }
    Unwrap(ComputeCommandEncoder)->setBuffers(real.data(), offsets.data(), range);
    for(size_t i = 0; i < buffers.size(); i++)
      m_Device->GetReplay()->BindComputeBuffer((uint32_t)(range.location + i),
                                                GetResID(buffers[i]), offsets[i]);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setBuffers(rdcarray<WrappedMTLBuffer *> buffers,
                                                   rdcarray<NS::UInteger> offsets, NS::Range range)
{
  rdcarray<const MTL::Buffer *> real;
  for(WrappedMTLBuffer *resource : buffers)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setBuffers(real.data(), offsets.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setBuffers);
    Serialise_setBuffers(ser, buffers, offsets, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLBuffer *resource : buffers)
      if(resource)
        record->MarkResourceFrameReferenced(GetResID(resource), eFrameRef_ReadBeforeWrite);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_dispatchThreadgroups(SerialiserType &ser,
                                                                        MTL::Size &groups,
                                                                        MTL::Size &threadsPerGroup)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(groups).Important();
  SERIALISE_ELEMENT(threadsPerGroup).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    MTL::ComputeCommandEncoder *realEncoder = Unwrap(ComputeCommandEncoder);
    MetalReplay *replay = m_Device->GetReplay();
    const TextureDescription source = replay->GetTexture(replay->GetComputeTextureForAccess(false));
    const TextureDescription destination = replay->GetTexture(replay->GetComputeTextureForAccess(true));
    const MetalPipe::BufferBinding input = replay->GetComputeBufferForAccess(false);
    const MetalPipe::BufferBinding output = replay->GetComputeBufferForAccess(true);
    const bool bufferOnly = source.resourceId == ResourceId() &&
                            destination.resourceId == ResourceId() &&
                            input.resourceId == ResourceId() &&
                            output.resourceId != ResourceId();
    if((!bufferOnly &&
        (source.resourceId == ResourceId() || destination.resourceId == ResourceId() ||
         source.type != TextureType::Texture2D || destination.type != TextureType::Texture2D ||
         source.width != destination.width || source.height != destination.height ||
         source.format != destination.format)) ||
       groups.width == 0 || groups.height == 0 ||
       groups.depth != 1 || threadsPerGroup.width == 0 || threadsPerGroup.height == 0 ||
       threadsPerGroup.depth != 1 || groups.width > UINT32_MAX || groups.height > UINT32_MAX ||
       threadsPerGroup.width > UINT32_MAX || threadsPerGroup.height > UINT32_MAX ||
       threadsPerGroup.width * threadsPerGroup.height > 1024 ||
       (!bufferOnly &&
        (groups.width < (destination.width + threadsPerGroup.width - 1) / threadsPerGroup.width ||
         groups.height < (destination.height + threadsPerGroup.height - 1) / threadsPerGroup.height)))
    {
      RDCERR("Invalid Metal compute texture binding or dispatch grid");
      return false;
    }
    if(bufferOnly && output.byteSize < 12)
    {
      RDCERR("Invalid Metal compute output buffer range");
      return false;
    }
    if(!bufferOnly && (input.resourceId != ResourceId() || output.resourceId != ResourceId()) &&
       (input.resourceId == ResourceId() || output.resourceId == ResourceId() ||
        input.byteSize < (uint64_t)destination.width * destination.height * 4 ||
        output.byteSize < (uint64_t)destination.width * destination.height * 4))
    {
      RDCERR("Invalid Metal compute buffer range");
      return false;
    }
    realEncoder->dispatchThreadgroups(groups, threadsPerGroup);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("dispatchThreadgroups(%llux%llux1, %llux%llux1)",
                                             (uint64_t)groups.width, (uint64_t)groups.height,
                                             (uint64_t)threadsPerGroup.width,
                                             (uint64_t)threadsPerGroup.height);
      action.flags = ActionFlags::Dispatch;
      action.dispatchDimension[0] = (uint32_t)groups.width;
      action.dispatchDimension[1] = (uint32_t)groups.height;
      action.dispatchDimension[2] = (uint32_t)groups.depth;
      action.dispatchThreadsDimension[0] = (uint32_t)threadsPerGroup.width;
      action.dispatchThreadsDimension[1] = (uint32_t)threadsPerGroup.height;
      action.dispatchThreadsDimension[2] = (uint32_t)threadsPerGroup.depth;
      AddAction(action);
      if(!bufferOnly)
      {
        replay->AddUsage(source.resourceId, ResourceUsage::CS_Resource);
        replay->AddUsage(destination.resourceId, ResourceUsage::CS_RWResource);
      }
      else
      {
        replay->AddUsage(output.resourceId, ResourceUsage::CS_RWResource);
      }
      if(input.resourceId != ResourceId())
      {
        replay->AddUsage(input.resourceId, ResourceUsage::CS_Resource);
        replay->AddUsage(output.resourceId, ResourceUsage::CS_RWResource);
      }
    }
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::dispatchThreadgroups(MTL::Size &groups,
                                                              MTL::Size &threadsPerGroup)
{
  SERIALISE_TIME_CALL(Unwrap(this)->dispatchThreadgroups(groups, threadsPerGroup));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups);
    Serialise_dispatchThreadgroups(ser, groups, threadsPerGroup);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_dispatchThreadgroups(
    SerialiserType &ser, WrappedMTLBuffer *indirectBuffer,
    NS::UInteger indirectBufferOffset, MTL::Size &threadsPerGroup)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(indirectBuffer).Important();
  SERIALISE_ELEMENT(indirectBufferOffset).Important();
  SERIALISE_ELEMENT(threadsPerGroup).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    MTL::Buffer *realBuffer = Unwrap(indirectBuffer);
    const uint64_t argumentSize = sizeof(MTL::DispatchThreadgroupsIndirectArguments);
    if(realBuffer == NULL || (indirectBufferOffset & 3) != 0 ||
       indirectBufferOffset > realBuffer->length() ||
       argumentSize > realBuffer->length() - indirectBufferOffset ||
       threadsPerGroup.width == 0 || threadsPerGroup.height == 0 ||
       threadsPerGroup.depth != 1 || threadsPerGroup.width > UINT32_MAX ||
       threadsPerGroup.height > UINT32_MAX ||
       threadsPerGroup.width * threadsPerGroup.height > 1024)
    {
      RDCERR("Invalid Metal compute indirect argument buffer, offset, or threadgroup size");
      return false;
    }

    MetalReplay *replay = m_Device->GetReplay();
    const TextureDescription source = replay->GetTexture(replay->GetComputeTextureForAccess(false));
    const TextureDescription destination = replay->GetTexture(replay->GetComputeTextureForAccess(true));
    if(source.resourceId == ResourceId() || destination.resourceId == ResourceId() ||
       source.type != TextureType::Texture2D || destination.type != TextureType::Texture2D ||
       source.width != destination.width || source.height != destination.height ||
       source.format != destination.format)
    {
      RDCERR("Invalid Metal compute texture binding for indirect dispatch");
      return false;
    }

    const MetalPipe::BufferBinding input = replay->GetComputeBufferForAccess(false);
    const MetalPipe::BufferBinding output = replay->GetComputeBufferForAccess(true);
    if((input.resourceId != ResourceId() || output.resourceId != ResourceId()) &&
       (input.resourceId == ResourceId() || output.resourceId == ResourceId() ||
        input.byteSize < (uint64_t)destination.width * destination.height * 4 ||
        output.byteSize < (uint64_t)destination.width * destination.height * 4))
    {
      RDCERR("Invalid Metal compute buffer range for indirect dispatch");
      return false;
    }

    replay->SetIndirectBuffer(GetResID(indirectBuffer), indirectBufferOffset, argumentSize);
    Unwrap(ComputeCommandEncoder)
        ->dispatchThreadgroups(realBuffer, indirectBufferOffset, threadsPerGroup);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "dispatchThreadgroups(indirect, <?, ?, ?>)";
      action.flags = ActionFlags::Dispatch | ActionFlags::Indirect;
      action.dispatchThreadsDimension[0] = (uint32_t)threadsPerGroup.width;
      action.dispatchThreadsDimension[1] = (uint32_t)threadsPerGroup.height;
      action.dispatchThreadsDimension[2] = (uint32_t)threadsPerGroup.depth;
      AddAction(action);
      replay->RegisterComputeIndirectAction(replay->GetNextEventID() - 1,
                                            GetResID(indirectBuffer),
                                            indirectBufferOffset);
      replay->AddUsage(GetResID(indirectBuffer), ResourceUsage::Indirect);
      replay->AddUsage(source.resourceId, ResourceUsage::CS_Resource);
      replay->AddUsage(destination.resourceId, ResourceUsage::CS_RWResource);
      if(input.resourceId != ResourceId())
      {
        replay->AddUsage(input.resourceId, ResourceUsage::CS_Resource);
        replay->AddUsage(output.resourceId, ResourceUsage::CS_RWResource);
      }
    }
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::dispatchThreadgroups(
    WrappedMTLBuffer *indirectBuffer, NS::UInteger indirectBufferOffset,
    MTL::Size &threadsPerGroup)
{
  SERIALISE_TIME_CALL(Unwrap(this)->dispatchThreadgroups(
      Unwrap(indirectBuffer), indirectBufferOffset, threadsPerGroup));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups_indirect);
    Serialise_dispatchThreadgroups(ser, indirectBuffer, indirectBufferOffset, threadsPerGroup);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(indirectBuffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_dispatchThreads(SerialiserType &ser,
                                                                  MTL::Size &grid,
                                                                  MTL::Size &threadsPerGroup)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(grid).Important();
  SERIALISE_ELEMENT(threadsPerGroup).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    MetalReplay *replay = m_Device->GetReplay();
    const TextureDescription source = replay->GetTexture(replay->GetComputeTextureForAccess(false));
    const TextureDescription destination = replay->GetTexture(replay->GetComputeTextureForAccess(true));
    if(source.resourceId == ResourceId() || destination.resourceId == ResourceId() ||
       source.type != TextureType::Texture2D || destination.type != TextureType::Texture2D ||
       source.width != destination.width || source.height != destination.height ||
       source.format != destination.format || grid.width == 0 || grid.height == 0 ||
       grid.depth != 1 || grid.width > destination.width || grid.height > destination.height ||
       threadsPerGroup.width == 0 || threadsPerGroup.height == 0 ||
       threadsPerGroup.depth != 1 || threadsPerGroup.width > 1024 ||
       threadsPerGroup.height > 1024 ||
       threadsPerGroup.width * threadsPerGroup.height > 1024)
    {
      RDCERR("Invalid Metal compute texture binding or thread grid");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->dispatchThreads(grid, threadsPerGroup);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("dispatchThreads(%llux%llux1, %llux%llux1)",
                                             (uint64_t)grid.width, (uint64_t)grid.height,
                                             (uint64_t)threadsPerGroup.width,
                                             (uint64_t)threadsPerGroup.height);
      action.flags = ActionFlags::Dispatch;
      action.dispatchDimension[0] = (uint32_t)grid.width;
      action.dispatchDimension[1] = (uint32_t)grid.height;
      action.dispatchDimension[2] = 1;
      action.dispatchThreadsDimension[0] = (uint32_t)threadsPerGroup.width;
      action.dispatchThreadsDimension[1] = (uint32_t)threadsPerGroup.height;
      action.dispatchThreadsDimension[2] = 1;
      AddAction(action);
      replay->AddUsage(source.resourceId, ResourceUsage::CS_Resource);
      replay->AddUsage(destination.resourceId, ResourceUsage::CS_RWResource);
    }
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::dispatchThreads(MTL::Size &grid,
                                                        MTL::Size &threadsPerGroup)
{
  SERIALISE_TIME_CALL(Unwrap(this)->dispatchThreads(grid, threadsPerGroup));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_dispatchThreads);
    Serialise_dispatchThreads(ser, grid, threadsPerGroup);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, endEncoding);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setComputePipelineState,
                                WrappedMTLComputePipelineState *pipeline);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setTexture,
                                WrappedMTLTexture *texture, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setTextures,
                                rdcarray<WrappedMTLTexture *> textures, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setSamplerState,
                                WrappedMTLSamplerState *sampler, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setSamplerStates,
                                rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setBuffer,
                                WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setBuffers,
                                rdcarray<WrappedMTLBuffer *> buffers,
                                rdcarray<NS::UInteger> offsets, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, dispatchThreadgroups,
                                MTL::Size &groups, MTL::Size &threadsPerGroup);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, dispatchThreadgroups,
                                WrappedMTLBuffer *indirectBuffer,
                                NS::UInteger indirectBufferOffset, MTL::Size &threadsPerGroup);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, dispatchThreads,
                                MTL::Size &grid, MTL::Size &threadsPerGroup);
