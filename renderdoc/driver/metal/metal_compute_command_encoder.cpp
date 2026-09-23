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
    if(index > 1 || !texture)
      return false;
    Unwrap(ComputeCommandEncoder)->setTexture(Unwrap(texture), index);
    m_Device->GetReplay()->SetComputeTexture((uint32_t)index, GetResID(texture));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setTexture(WrappedMTLTexture *texture, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTexture(Unwrap(texture), index));
  if(index < 2)
    m_Textures[index] = texture;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setTexture);
    Serialise_setTexture(ser, texture, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(texture),
                                        index == 0 ? eFrameRef_Read : eFrameRef_PartialWrite);
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
    const TextureDescription source = replay->GetTexture(replay->GetComputeTexture(0));
    const TextureDescription destination = replay->GetTexture(replay->GetComputeTexture(1));
    if(source.resourceId == ResourceId() || destination.resourceId == ResourceId() ||
       source.type != TextureType::Texture2D || destination.type != TextureType::Texture2D ||
       source.width != destination.width || source.height != destination.height ||
       source.format != destination.format || groups.width == 0 || groups.height == 0 ||
       groups.depth != 1 || threadsPerGroup.width == 0 || threadsPerGroup.height == 0 ||
       threadsPerGroup.depth != 1 || groups.width > UINT32_MAX || groups.height > UINT32_MAX ||
       threadsPerGroup.width > UINT32_MAX || threadsPerGroup.height > UINT32_MAX ||
       threadsPerGroup.width * threadsPerGroup.height > 1024 ||
       groups.width < (destination.width + threadsPerGroup.width - 1) / threadsPerGroup.width ||
       groups.height < (destination.height + threadsPerGroup.height - 1) / threadsPerGroup.height)
    {
      RDCERR("Invalid Metal compute texture binding or dispatch grid");
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
      replay->AddUsage(replay->GetComputeTexture(0), ResourceUsage::CS_Resource);
      replay->AddUsage(replay->GetComputeTexture(1), ResourceUsage::CS_RWResource);
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

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, endEncoding);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setComputePipelineState,
                                WrappedMTLComputePipelineState *pipeline);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setTexture,
                                WrappedMTLTexture *texture, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, dispatchThreadgroups,
                                MTL::Size &groups, MTL::Size &threadsPerGroup);
