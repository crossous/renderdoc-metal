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

#include "metal_command_buffer.h"
#include <cmath>
#include "metal_buffer.h"
#include "metal_blit_command_encoder.h"
#include "metal_acceleration_structure_command_encoder.h"
#include "metal_compute_command_encoder.h"
#include "metal_device.h"
#include "metal_replay.h"
#include "metal_rate_map.h"
#include "metal_counter_sample_buffer.h"
#include "metal_render_command_encoder.h"
#include "metal_resources.h"
#include "metal_texture.h"

WrappedMTLCommandBuffer::WrappedMTLCommandBuffer(MTL::CommandBuffer *realMTLCommandBuffer,
                                                 ResourceId objId, WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLCommandBuffer, objId, wrappedMTLDevice, wrappedMTLDevice->GetStateRef())
{
  if(realMTLCommandBuffer && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

bool WrappedMTLCommandBuffer::ReplayComputeCommandEncoder(
    ResourceId id, MTL::ComputeCommandEncoder *realEncoder)
{
  if(!realEncoder)
    return false;

  WrappedMTLComputeCommandEncoder *wrappedEncoder =
      (WrappedMTLComputeCommandEncoder *)GetResourceManager()->GetResource(id, true);
  if(wrappedEncoder)
    GetResourceManager()->ReplaceRealResource(wrappedEncoder, realEncoder);
  else
    GetResourceManager()->WrapResource(id, realEncoder, wrappedEncoder);
  wrappedEncoder->SetCommandBuffer(this);
  m_Device->SetReplayComputeCommandEncoder(wrappedEncoder);
  m_Device->GetReplay()->BeginComputePass();
  if(IsLoading(m_State))
  {
    m_Device->AddResource(id, ResourceType::CommandBuffer, "Compute Encoder");
    m_Device->DerivedResource(GetResID(this), id);
    AddEvent();
    ActionDescription action;
    action.customName = "Begin Metal Compute Pass";
    action.flags = ActionFlags::PassBoundary | ActionFlags::BeginPass;
    AddAction(action);
  }
  return true;
}

bool WrappedMTLCommandBuffer::ReplayBlitCommandEncoder(ResourceId id,
                                                       MTL::BlitCommandEncoder *realEncoder)
{
  if(!realEncoder || id == ResourceId())
    return false;
  WrappedMTLBlitCommandEncoder *wrappedEncoder =
      (WrappedMTLBlitCommandEncoder *)GetResourceManager()->GetResource(id, true);
  if(wrappedEncoder)
    GetResourceManager()->ReplaceRealResource(wrappedEncoder, realEncoder);
  else
    GetResourceManager()->WrapResource(id, realEncoder, wrappedEncoder);
  wrappedEncoder->SetCommandBuffer(this);
  m_Device->SetReplayBlitCommandEncoder(wrappedEncoder);
  if(IsLoading(m_State))
  {
    m_Device->AddResource(id, ResourceType::CommandBuffer, "Blit Encoder");
    m_Device->DerivedResource(GetResID(this), id);
    AddEvent();
    ActionDescription action;
    action.customName = "Begin Metal Blit Pass";
    action.flags = ActionFlags::PassBoundary | ActionFlags::BeginPass;
    AddAction(action);
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_accelerationStructureCommandEncoder(
    SerialiserType &ser, WrappedMTLAccelerationStructureCommandEncoder *encoder)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT_LOCAL(Encoder, GetResID(encoder))
      .TypedAs("MTLAccelerationStructureCommandEncoder"_lit);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!CommandBuffer || Encoder == ResourceId())
      return false;
    MTL::AccelerationStructureCommandEncoder *real =
        Unwrap(CommandBuffer)->accelerationStructureCommandEncoder();
    if(!real)
      return false;
    WrappedMTLAccelerationStructureCommandEncoder *wrapped =
        (WrappedMTLAccelerationStructureCommandEncoder *)
            GetResourceManager()->GetResource(Encoder, true);
    if(wrapped)
      GetResourceManager()->ReplaceRealResource(wrapped, real);
    else
      GetResourceManager()->WrapResource(Encoder, real, wrapped);
    wrapped->SetCommandBuffer(CommandBuffer);
    m_Device->SetReplayAccelerationStructureCommandEncoder(wrapped);
    if(IsLoading(m_State))
    {
      m_Device->AddResource(Encoder, ResourceType::CommandBuffer, "Acceleration Structure Encoder");
      m_Device->DerivedResource(CommandBuffer, Encoder);
      AddEvent();
      ActionDescription action;
      action.customName = "Begin Metal Acceleration Structure Pass";
      action.flags = ActionFlags::PassBoundary | ActionFlags::BeginPass;
      AddAction(action);
    }
  }
  return true;
}

WrappedMTLAccelerationStructureCommandEncoder *
WrappedMTLCommandBuffer::accelerationStructureCommandEncoder()
{
  MTL::AccelerationStructureCommandEncoder *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->accelerationStructureCommandEncoder());
  if(!real)
    return NULL;
  WrappedMTLAccelerationStructureCommandEncoder *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->SetCommandBuffer(this);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_accelerationStructureCommandEncoder);
    Serialise_accelerationStructureCommandEncoder(ser, wrapped);
    GetRecord(this)->AddChunk(scope.Get());
    GetResourceManager()->AddResourceRecord(wrapped);
  }
  return wrapped;
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(
    WrappedMTLCommandBuffer, WrappedMTLAccelerationStructureCommandEncoder *,
    accelerationStructureCommandEncoder);

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_accelerationStructureCommandEncoderWithDescriptor(
    SerialiserType &ser, WrappedMTLAccelerationStructureCommandEncoder *encoder,
    bool hasSampleBuffers)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT_LOCAL(Encoder, GetResID(encoder))
      .TypedAs("MTLAccelerationStructureCommandEncoder"_lit);
  SERIALISE_ELEMENT(hasSampleBuffers).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!CommandBuffer || CommandBuffer->m_Type != eResCommandBuffer ||
       !CommandBuffer->m_Real || !m_Device->CanEncodeReplayEvent(CommandBuffer) ||
       Encoder == ResourceId() || hasSampleBuffers)
    {
      RDCERR("Invalid Metal acceleration structure pass descriptor or command buffer");
      return false;
    }
    WrappedMTLObject *existing = GetResourceManager()->GetResource(Encoder, true);
    if(existing && existing->m_Type != eResAccelerationStructureCommandEncoder)
    {
      RDCERR("Invalid Metal acceleration structure encoder identity");
      return false;
    }
    MTL::AccelerationStructurePassDescriptor *descriptor =
        MTL::AccelerationStructurePassDescriptor::accelerationStructurePassDescriptor();
    MTL::AccelerationStructureCommandEncoder *real =
        Unwrap(CommandBuffer)->accelerationStructureCommandEncoder(descriptor);
    if(!real) return false;
    WrappedMTLAccelerationStructureCommandEncoder *wrapped =
        (WrappedMTLAccelerationStructureCommandEncoder *)existing;
    if(wrapped)
      GetResourceManager()->ReplaceRealResource(wrapped, real);
    else
      GetResourceManager()->WrapResource(Encoder, real, wrapped);
    wrapped->SetCommandBuffer(CommandBuffer);
    m_Device->SetReplayAccelerationStructureCommandEncoder(wrapped);
    if(IsLoading(m_State))
    {
      m_Device->AddResource(Encoder, ResourceType::CommandBuffer,
                            "Acceleration Structure Encoder");
      m_Device->DerivedResource(CommandBuffer, Encoder);
      AddEvent();
      ActionDescription action;
      action.customName = "Begin Metal Acceleration Structure Pass";
      action.flags = ActionFlags::PassBoundary | ActionFlags::BeginPass;
      AddAction(action);
    }
  }
  return true;
}

WrappedMTLAccelerationStructureCommandEncoder *
WrappedMTLCommandBuffer::accelerationStructureCommandEncoderWithDescriptor(
    MTL::AccelerationStructurePassDescriptor *descriptor)
{
  if(!descriptor) return NULL;
  bool hasSampleBuffers = false;
  for(uint32_t i = 0; i < MAX_ACCELERATION_STRUCTURE_PASS_SAMPLE_BUFFER_ATTACHMENTS; i++)
  {
    MTL::AccelerationStructurePassSampleBufferAttachmentDescriptor *attachment =
        descriptor->sampleBufferAttachments()->object(i);
    hasSampleBuffers |= attachment && attachment->sampleBuffer() != NULL;
  }
  if(hasSampleBuffers)
  {
    RDCERR("Metal acceleration structure pass counter attachments are unsupported");
    return NULL;
  }
  MTL::AccelerationStructureCommandEncoder *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->accelerationStructureCommandEncoder(descriptor));
  if(!real) return NULL;
  WrappedMTLAccelerationStructureCommandEncoder *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->SetCommandBuffer(this);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(
        MetalChunk::MTLCommandBuffer_accelerationStructureCommandEncoderWithDescriptor);
    Serialise_accelerationStructureCommandEncoderWithDescriptor(ser, wrapped, hasSampleBuffers);
    GetRecord(this)->AddChunk(scope.Get());
    GetResourceManager()->AddResourceRecord(wrapped);
  }
  return wrapped;
}

template bool WrappedMTLCommandBuffer::Serialise_accelerationStructureCommandEncoderWithDescriptor(
    ReadSerialiser &, WrappedMTLAccelerationStructureCommandEncoder *, bool);
template bool WrappedMTLCommandBuffer::Serialise_accelerationStructureCommandEncoderWithDescriptor(
    WriteSerialiser &, WrappedMTLAccelerationStructureCommandEncoder *, bool);

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_blitCommandEncoder(SerialiserType &ser,
                                                           WrappedMTLBlitCommandEncoder *encoder)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, GetResID(encoder))
      .TypedAs("MTLBlitCommandEncoder"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!CommandBuffer || BlitCommandEncoder == ResourceId())
      return false;
    return CommandBuffer->ReplayBlitCommandEncoder(BlitCommandEncoder,
                                                    Unwrap(CommandBuffer)->blitCommandEncoder());
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_blitCommandEncoderWithDescriptor(
    SerialiserType &ser, WrappedMTLBlitCommandEncoder *encoder, bool hasSampleBuffers)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, GetResID(encoder))
      .TypedAs("MTLBlitCommandEncoder"_lit);
  SERIALISE_ELEMENT(hasSampleBuffers).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!CommandBuffer || BlitCommandEncoder == ResourceId() || hasSampleBuffers)
    {
      RDCERR("Invalid Metal blit encoder or unsupported counter sample attachments");
      return false;
    }
    MTL::BlitPassDescriptor *descriptor = MTL::BlitPassDescriptor::blitPassDescriptor();
    return CommandBuffer->ReplayBlitCommandEncoder(
        BlitCommandEncoder, Unwrap(CommandBuffer)->blitCommandEncoder(descriptor));
  }
  return true;
}

WrappedMTLBlitCommandEncoder *WrappedMTLCommandBuffer::blitCommandEncoderWithDescriptor(
    MTL::BlitPassDescriptor *descriptor)
{
  if(!descriptor)
    return NULL;
  MTL::BlitCommandEncoder *realEncoder = NULL;
  SERIALISE_TIME_CALL(realEncoder = Unwrap(this)->blitCommandEncoder(descriptor));
  if(!realEncoder)
    return NULL;
  WrappedMTLBlitCommandEncoder *wrappedEncoder = NULL;
  GetResourceManager()->WrapResource(ResourceId(), realEncoder, wrappedEncoder);
  wrappedEncoder->SetCommandBuffer(this);
  if(IsCaptureMode(m_State))
  {
    bool hasSampleBuffers = false;
    for(uint32_t i = 0; i < MAX_BLIT_PASS_SAMPLE_BUFFER_ATTACHMENTS; i++)
    {
      MTL::BlitPassSampleBufferAttachmentDescriptor *attachment =
          descriptor->sampleBufferAttachments()->object(i);
      hasSampleBuffers |= attachment && attachment->sampleBuffer() != NULL;
    }
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_blitCommandEncoderWithDescriptor);
    Serialise_blitCommandEncoderWithDescriptor(ser, wrappedEncoder, hasSampleBuffers);
    GetRecord(this)->AddChunk(scope.Get());
    GetResourceManager()->AddResourceRecord(wrappedEncoder);
  }
  return wrappedEncoder;
}

WrappedMTLBlitCommandEncoder *WrappedMTLCommandBuffer::blitCommandEncoder()
{
  MTL::BlitCommandEncoder *realMTLBlitCommandEncoder;
  SERIALISE_TIME_CALL(realMTLBlitCommandEncoder = Unwrap(this)->blitCommandEncoder());
  WrappedMTLBlitCommandEncoder *wrappedMTLBlitCommandEncoder;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), realMTLBlitCommandEncoder,
                                                     wrappedMTLBlitCommandEncoder);
  wrappedMTLBlitCommandEncoder->SetCommandBuffer(this);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_blitCommandEncoder);
      Serialise_blitCommandEncoder(ser, wrappedMTLBlitCommandEncoder);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(this);
    bufferRecord->AddChunk(chunk);

    MetalResourceRecord *encoderRecord =
        GetResourceManager()->AddResourceRecord(wrappedMTLBlitCommandEncoder);
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, *wrappedMTLLibrary);
  }
  return wrappedMTLBlitCommandEncoder;
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_computeCommandEncoder(
    SerialiserType &ser, WrappedMTLComputeCommandEncoder *encoder)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, GetResID(encoder))
      .TypedAs("MTLComputeCommandEncoder"_lit);
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    MTL::ComputeCommandEncoder *realEncoder = Unwrap(CommandBuffer)->computeCommandEncoder();
    if(!realEncoder)
      return false;
    WrappedMTLComputeCommandEncoder *wrappedEncoder =
        (WrappedMTLComputeCommandEncoder *)GetResourceManager()->GetResource(
            ComputeCommandEncoder, true);
    if(wrappedEncoder)
      GetResourceManager()->ReplaceRealResource(wrappedEncoder, realEncoder);
    else
      GetResourceManager()->WrapResource(ComputeCommandEncoder, realEncoder, wrappedEncoder);
    wrappedEncoder->SetCommandBuffer(CommandBuffer);
    m_Device->SetReplayComputeCommandEncoder(wrappedEncoder);
    m_Device->GetReplay()->BeginComputePass();
    if(IsLoading(m_State))
    {
      m_Device->AddResource(ComputeCommandEncoder, ResourceType::CommandBuffer, "Compute Encoder");
      m_Device->DerivedResource(CommandBuffer, ComputeCommandEncoder);
      AddEvent();
      ActionDescription action;
      action.customName = "Begin Metal Compute Pass";
      action.flags = ActionFlags::PassBoundary | ActionFlags::BeginPass;
      AddAction(action);
    }
  }
  return true;
}

WrappedMTLComputeCommandEncoder *WrappedMTLCommandBuffer::computeCommandEncoder()
{
  MTL::ComputeCommandEncoder *realEncoder;
  SERIALISE_TIME_CALL(realEncoder = Unwrap(this)->computeCommandEncoder());
  if(!realEncoder)
    return NULL;
  WrappedMTLComputeCommandEncoder *wrappedEncoder;
  GetResourceManager()->WrapResource(ResourceId(), realEncoder, wrappedEncoder);
  wrappedEncoder->SetCommandBuffer(this);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_computeCommandEncoder);
    Serialise_computeCommandEncoder(ser, wrappedEncoder);
    GetRecord(this)->AddChunk(scope.Get());
    GetResourceManager()->AddResourceRecord(wrappedEncoder);
  }
  return wrappedEncoder;
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_computeCommandEncoderWithDescriptor(
    SerialiserType &ser, WrappedMTLComputeCommandEncoder *encoder,
    MTL::DispatchType dispatchType)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, GetResID(encoder))
      .TypedAs("MTLComputeCommandEncoder"_lit);
  SERIALISE_ELEMENT(dispatchType).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(dispatchType != MTL::DispatchTypeSerial && dispatchType != MTL::DispatchTypeConcurrent)
    {
      RDCERR("Invalid Metal compute dispatch type %u", (uint32_t)dispatchType);
      return false;
    }
    MTL::ComputePassDescriptor *descriptor = MTL::ComputePassDescriptor::computePassDescriptor();
    descriptor->setDispatchType(dispatchType);
    return ReplayComputeCommandEncoder(
        ComputeCommandEncoder, Unwrap(CommandBuffer)->computeCommandEncoder(descriptor));
  }
  return true;
}

WrappedMTLComputeCommandEncoder *WrappedMTLCommandBuffer::computeCommandEncoderWithDescriptor(
    MTL::ComputePassDescriptor *descriptor)
{
  if(!descriptor)
    return NULL;
  MTL::ComputeCommandEncoder *realEncoder = NULL;
  SERIALISE_TIME_CALL(realEncoder = Unwrap(this)->computeCommandEncoder(descriptor));
  if(!realEncoder)
    return NULL;

  WrappedMTLComputeCommandEncoder *wrappedEncoder = NULL;
  GetResourceManager()->WrapResource(ResourceId(), realEncoder, wrappedEncoder);
  wrappedEncoder->SetCommandBuffer(this);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDescriptor);
    Serialise_computeCommandEncoderWithDescriptor(ser, wrappedEncoder,
                                                  descriptor->dispatchType());
    GetRecord(this)->AddChunk(scope.Get());
    GetResourceManager()->AddResourceRecord(wrappedEncoder);
  }
  return wrappedEncoder;
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_computeCommandEncoder(
    SerialiserType &ser, WrappedMTLComputeCommandEncoder *encoder,
    MTL::DispatchType dispatchType)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, GetResID(encoder))
      .TypedAs("MTLComputeCommandEncoder"_lit);
  SERIALISE_ELEMENT(dispatchType).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(dispatchType != MTL::DispatchTypeSerial && dispatchType != MTL::DispatchTypeConcurrent)
    {
      RDCERR("Invalid Metal compute dispatch type %u", (uint32_t)dispatchType);
      return false;
    }
    return ReplayComputeCommandEncoder(
        ComputeCommandEncoder, Unwrap(CommandBuffer)->computeCommandEncoder(dispatchType));
  }
  return true;
}

WrappedMTLComputeCommandEncoder *WrappedMTLCommandBuffer::computeCommandEncoder(
    MTL::DispatchType dispatchType)
{
  MTL::ComputeCommandEncoder *realEncoder = NULL;
  SERIALISE_TIME_CALL(realEncoder = Unwrap(this)->computeCommandEncoder(dispatchType));
  if(!realEncoder)
    return NULL;

  WrappedMTLComputeCommandEncoder *wrappedEncoder = NULL;
  GetResourceManager()->WrapResource(ResourceId(), realEncoder, wrappedEncoder);
  wrappedEncoder->SetCommandBuffer(this);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDispatchType);
    Serialise_computeCommandEncoder(ser, wrappedEncoder, dispatchType);
    GetRecord(this)->AddChunk(scope.Get());
    GetResourceManager()->AddResourceRecord(wrappedEncoder);
  }
  return wrappedEncoder;
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_renderCommandEncoderWithDescriptor(
    SerialiserType &ser, WrappedMTLRenderCommandEncoder *encoder,
    RDMTL::RenderPassDescriptor &descriptor)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, GetResID(encoder))
      .TypedAs("MTLRenderCommandEncoder"_lit);
  SERIALISE_ELEMENT(descriptor).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if((descriptor.rasterizationRateMapId != ResourceId() &&
        (!descriptor.rasterizationRateMap ||
         descriptor.rasterizationRateMap->m_Type != eResRasterizationRateMap ||
         !descriptor.rasterizationRateMap->m_Real ||
         GetResID(descriptor.rasterizationRateMap) != descriptor.rasterizationRateMapId)) ||
       (descriptor.rasterizationRateMapId == ResourceId() &&
        descriptor.rasterizationRateMap))
    {
      RDCERR("Invalid Metal render pass rasterization rate map identity");
      return false;
    }
    if(descriptor.rasterizationRateMap &&
       Unwrap(descriptor.rasterizationRateMap)->layerCount() !=
           RDCMAX((uint64_t)1, (uint64_t)descriptor.renderTargetArrayLength))
    {
      RDCERR("Metal render pass rasterization rate map layer count does not match target array length");
      return false;
    }
    for(const auto &attachment : descriptor.sampleBufferAttachments)
    {
      if((attachment.sampleBufferId != ResourceId() &&
          (!attachment.sampleBuffer ||
           GetResID(attachment.sampleBuffer) != attachment.sampleBufferId)) ||
         (attachment.sampleBufferId == ResourceId() && attachment.sampleBuffer))
      {
        RDCERR("Invalid Metal render pass counter sample buffer identity");
        return false;
      }
      if(!attachment.sampleBuffer)
        continue;
      if(attachment.sampleBuffer->m_Type != eResCounterSampleBuffer ||
         !attachment.sampleBuffer->m_Real)
      {
        RDCERR("Invalid Metal render pass counter sample buffer identity");
        return false;
      }
      const uint64_t count = Unwrap(attachment.sampleBuffer)->sampleCount();
      for(uint64_t index : {uint64_t(attachment.startOfVertexSampleIndex),
                            uint64_t(attachment.endOfVertexSampleIndex),
                            uint64_t(attachment.startOfFragmentSampleIndex),
                            uint64_t(attachment.endOfFragmentSampleIndex)})
        if(index != MTLCounterDontSample && index >= count)
        {
          RDCERR("Invalid Metal render pass counter sample index");
          return false;
        }
    }
    MTL::RenderPassDescriptor *mtlDescriptor(descriptor);
    // Keep deferred store actions unknown so the application's encoder setters remain legal.
    // Only unattached slots can be finalised immediately; attached slots are resolved at
    // endEncoding if a partial event replay stops before their setter chunks.
    uint16_t deferredStoreActions = 0;
    for(NS::UInteger i = 0; i < descriptor.colorAttachments.size(); i++)
      if(descriptor.colorAttachments[i].storeAction == MTL::StoreActionUnknown)
      {
        if(descriptor.colorAttachments[i].texture)
          deferredStoreActions |= uint16_t(1U << i);
        else
          mtlDescriptor->colorAttachments()->object(i)->setStoreAction(MTL::StoreActionStore);
      }
    if(descriptor.depthAttachment.storeAction == MTL::StoreActionUnknown)
    {
      if(descriptor.depthAttachment.texture)
        deferredStoreActions |= uint16_t(1U << 8);
      else
        mtlDescriptor->depthAttachment()->setStoreAction(MTL::StoreActionStore);
    }
    if(descriptor.stencilAttachment.storeAction == MTL::StoreActionUnknown)
    {
      if(descriptor.stencilAttachment.texture)
        deferredStoreActions |= uint16_t(1U << 9);
      else
        mtlDescriptor->stencilAttachment()->setStoreAction(MTL::StoreActionStore);
    }
    MTL::RenderCommandEncoder *realEncoder =
        Unwrap(CommandBuffer)->renderCommandEncoder(mtlDescriptor);
    mtlDescriptor->release();
    if(!realEncoder)
      return false;

    WrappedMTLRenderCommandEncoder *wrappedEncoder =
        (WrappedMTLRenderCommandEncoder *)GetResourceManager()->GetResource(RenderCommandEncoder,
                                                                            true);
    if(wrappedEncoder)
      GetResourceManager()->ReplaceRealResource(wrappedEncoder, realEncoder);
    else
      GetResourceManager()->WrapResource(RenderCommandEncoder, realEncoder, wrappedEncoder);
    wrappedEncoder->SetCommandBuffer(CommandBuffer);
    wrappedEncoder->SetDeferredStoreActions(deferredStoreActions);
    m_Device->SetReplayRenderCommandEncoder(wrappedEncoder);
    m_Device->GetReplay()->BeginRenderPass(descriptor);
    if(IsLoading(m_State))
    {
      m_Device->AddResource(RenderCommandEncoder, ResourceType::CommandBuffer, "Render Encoder");
      m_Device->DerivedResource(CommandBuffer, RenderCommandEncoder);
    }

    ResourceId colorTarget;
    if(!descriptor.colorAttachments.empty() && descriptor.colorAttachments[0].texture)
      colorTarget = GetResID(descriptor.colorAttachments[0].texture);
    m_Device->SetReplayRenderTarget(colorTarget);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("Begin Metal Render Pass (%s)",
                                            RDMTL::RenderPassOpString(descriptor, false).c_str());
      action.flags = ActionFlags::PassBoundary | ActionFlags::BeginPass;
      bool clearsColor = false;
      for(const RDMTL::RenderPassColorAttachmentDescriptor &attachment : descriptor.colorAttachments)
        clearsColor |= attachment.texture && attachment.loadAction == MTL::LoadActionClear;
      if(clearsColor)
      {
        action.flags |= ActionFlags::Clear | ActionFlags::ClearColor;
      }
      const bool clearsDepth = descriptor.depthAttachment.texture &&
                               descriptor.depthAttachment.loadAction == MTL::LoadActionClear;
      const bool clearsStencil = descriptor.stencilAttachment.texture &&
                                 descriptor.stencilAttachment.loadAction == MTL::LoadActionClear;
      if(clearsDepth || clearsStencil)
      {
        action.flags |= ActionFlags::Clear | ActionFlags::ClearDepthStencil;
      }
      m_Device->GetReplay()->SetActionOutputs(action);
      AddAction(action);
      m_Device->GetReplay()->AddRenderPassLoadUsage(descriptor);
    }
  }
  return true;
}

WrappedMTLRenderCommandEncoder *WrappedMTLCommandBuffer::renderCommandEncoderWithDescriptor(
    RDMTL::RenderPassDescriptor &descriptor)
{
  MTL::RenderCommandEncoder *realMTLRenderCommandEncoder;
  MTL::RenderPassDescriptor *mtlDescriptor(descriptor);
  SERIALISE_TIME_CALL(realMTLRenderCommandEncoder =
                          Unwrap(this)->renderCommandEncoder(mtlDescriptor));
  mtlDescriptor->release();
  WrappedMTLRenderCommandEncoder *wrappedMTLRenderCommandEncoder;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), realMTLRenderCommandEncoder,
                                                     wrappedMTLRenderCommandEncoder);
  wrappedMTLRenderCommandEncoder->SetCommandBuffer(this);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_renderCommandEncoderWithDescriptor);
      Serialise_renderCommandEncoderWithDescriptor(ser, wrappedMTLRenderCommandEncoder, descriptor);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(this);
    bufferRecord->AddChunk(chunk);

    MetalResourceRecord *encoderRecord =
        GetResourceManager()->AddResourceRecord(wrappedMTLRenderCommandEncoder);

    auto referenceAttachment = [bufferRecord](const RDMTL::RenderPassAttachmentDescriptor &attachment) {
      if(attachment.texture)
        bufferRecord->MarkResourceFrameReferenced(GetResID(attachment.texture), eFrameRef_Read);
      if(attachment.resolveTexture)
        bufferRecord->MarkResourceFrameReferenced(GetResID(attachment.resolveTexture), eFrameRef_Read);
    };

    for(int i = 0; i < descriptor.colorAttachments.count(); ++i)
    {
      referenceAttachment(descriptor.colorAttachments[i]);
    }
    referenceAttachment(descriptor.depthAttachment);
    referenceAttachment(descriptor.stencilAttachment);
    if(descriptor.visibilityResultBuffer)
      bufferRecord->MarkResourceFrameReferenced(GetResID(descriptor.visibilityResultBuffer),
                                                eFrameRef_ReadBeforeWrite);
    if(descriptor.rasterizationRateMap)
      bufferRecord->MarkResourceFrameReferenced(GetResID(descriptor.rasterizationRateMap),
                                                eFrameRef_Read);
    for(const auto &attachment : descriptor.sampleBufferAttachments)
      if(attachment.sampleBuffer)
        bufferRecord->MarkResourceFrameReferenced(GetResID(attachment.sampleBuffer),
                                                  eFrameRef_PartialWrite);
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, *wrappedMTLLibrary);
  }
  return wrappedMTLRenderCommandEncoder;
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_presentDrawable(SerialiserType &ser,
                                                        WrappedMTLTexture *presentedImage)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT(presentedImage).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
    {
      AddEvent();

      ActionDescription action;
      ResourceId presentedImageId = GetResID(presentedImage);
      action.customName = StringFormat::Fmt("presentDrawable(%s)", ToStr(presentedImageId).c_str());
      action.flags |= ActionFlags::Present;
      action.copyDestination = presentedImageId;
      m_Device->SetLastPresentedIamge(presentedImageId);
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLCommandBuffer::presentDrawable(MTL::Drawable *drawable)
{
  SERIALISE_TIME_CALL(Unwrap(this)->presentDrawable(drawable));
  CapturePresent(drawable, MetalChunk::MTLCommandBuffer_presentDrawable, 0.0);
}

void WrappedMTLCommandBuffer::presentDrawable(MTL::Drawable *drawable, double time,
                                              bool minimumDuration)
{
  if(minimumDuration)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->presentDrawableAfterMinimumDuration(drawable, time));
  }
  else
  {
    SERIALISE_TIME_CALL(Unwrap(this)->presentDrawableAtTime(drawable, time));
  }
  CapturePresent(drawable, minimumDuration ? MetalChunk::MTLCommandBuffer_presentDrawable_afterMinimumDuration
                                         : MetalChunk::MTLCommandBuffer_presentDrawable_atTime, time);
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_presentDrawableTimed(
    SerialiserType &ser, WrappedMTLTexture *presentedImage, double time, bool minimumDuration)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT(presentedImage).Important();
  SERIALISE_ELEMENT(time).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!CommandBuffer || CommandBuffer->m_Type != eResCommandBuffer ||
       !presentedImage || presentedImage->m_Type != eResTexture ||
       !std::isfinite(time) || time < 0.0)
    {
      RDCERR("Invalid Metal timed present resource or time");
      return false;
    }
    // Offline replay has no drawable scheduling clock. Keep the captured timing as metadata.
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = minimumDuration ? "presentDrawable(afterMinimumDuration)"
                                         : "presentDrawable(atTime)";
      action.flags = ActionFlags::Present;
      action.copyDestination = GetResID(presentedImage);
      m_Device->SetLastPresentedIamge(action.copyDestination);
      AddAction(action);
    }
  }
  return true;
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, presentDrawableTimed,
                                WrappedMTLTexture *presentedImage, double time, bool minimumDuration);

void WrappedMTLCommandBuffer::CapturePresent(MTL::Drawable *drawable, MetalChunk chunkType, double time)
{
  if(IsCaptureMode(m_State))
  {
    MetalDrawableInfo info = m_Device->UnregisterDrawableInfo(drawable);
    WrappedMTLTexture *presentedImage = info.texture;
    if(presentedImage)
    {
      Chunk *chunk = NULL;
      {
        CACHE_THREAD_SERIALISER();
        SCOPED_SERIALISE_CHUNK(chunkType);
        if(chunkType == MetalChunk::MTLCommandBuffer_presentDrawable)
          Serialise_presentDrawable(ser, presentedImage);
        else
          Serialise_presentDrawableTimed(ser, presentedImage, time,
              chunkType == MetalChunk::MTLCommandBuffer_presentDrawable_afterMinimumDuration);
        chunk = scope.Get();
      }
      MetalResourceRecord *bufferRecord = GetRecord(this);
      bufferRecord->AddChunk(chunk);
      bufferRecord->cmdInfo->presented = true;
      bufferRecord->cmdInfo->outputLayer = info.mtlLayer;
      bufferRecord->cmdInfo->backBuffer = presentedImage;
    }
    else
    {
      RDCERR("Ignoring presentDrawable on untracked MTLDrawable");
    }
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_commit(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    CommandBuffer->commit();
    m_Device->MarkReplayCommandBufferCommitted();
  }
  return true;
}

void WrappedMTLCommandBuffer::commit()
{
  MTL::CommandBuffer *mtlCommandBuffer = Unwrap(this);
  bool isCapture = IsCaptureMode(m_State);
  // During capture keep the real resource alive
  // It will be released when it is no longer required to be tracked
  if(isCapture)
  {
    mtlCommandBuffer->retain();
    // Snapshot before native commit can invoke callbacks or mutate shared memory on the GPU.
    m_Device->CaptureCmdBufCPUWrites(GetRecord(this));
  }
  SERIALISE_TIME_CALL(mtlCommandBuffer->commit());
  if(isCapture)
  {
    MetalResourceRecord *bufferRecord = GetRecord(this);
    m_Device->CaptureCmdBufCommit(bufferRecord);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_enqueue(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    CommandBuffer->waitUntilCompleted();
  }
  return true;
}

void WrappedMTLCommandBuffer::enqueue()
{
  SERIALISE_TIME_CALL(Unwrap(this)->enqueue());
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_enqueue);
      Serialise_enqueue(ser);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(this);
    bufferRecord->AddChunk(chunk);
    m_Device->CaptureCmdBufEnqueue(bufferRecord);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_pushDebugGroup(SerialiserType &ser, NS::String *string)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT(string).Important();
  SERIALISE_CHECK_READ_ERRORS();
  return true;
}

void WrappedMTLCommandBuffer::pushDebugGroup(NS::String *string)
{
  SERIALISE_TIME_CALL(Unwrap(this)->pushDebugGroup(string));
  if(IsCaptureMode(m_State) && IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_pushDebugGroup);
    Serialise_pushDebugGroup(ser, string);
    GetRecord(this)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_popDebugGroup(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_CHECK_READ_ERRORS();
  return true;
}

void WrappedMTLCommandBuffer::popDebugGroup()
{
  SERIALISE_TIME_CALL(Unwrap(this)->popDebugGroup());
  if(IsCaptureMode(m_State) && IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_popDebugGroup);
    Serialise_popDebugGroup(ser);
    GetRecord(this)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_handlerRegistration(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (!CommandBuffer || CommandBuffer->m_Type != eResCommandBuffer || !Unwrap(CommandBuffer)))
  {
    RDCERR("Invalid Metal command buffer handler registration");
    return false;
  }
  // Application code, pointers and callback timing are not part of a GPU capture. CPU writes
  // made by a callback are captured through the normal resource snapshot/update mechanism.
  return true;
}

void WrappedMTLCommandBuffer::CaptureHandlerRegistration(bool completed)
{
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(completed ? MetalChunk::MTLCommandBuffer_addCompletedHandler
                                    : MetalChunk::MTLCommandBuffer_addScheduledHandler);
    Serialise_handlerRegistration(ser);
    GetRecord(this)->AddChunk(scope.Get());
  }
}

template bool WrappedMTLCommandBuffer::Serialise_handlerRegistration(ReadSerialiser &ser);
template bool WrappedMTLCommandBuffer::Serialise_handlerRegistration(WriteSerialiser &ser);

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_waitUntilScheduled(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);

  SERIALISE_CHECK_READ_ERRORS();

  // The commit chunk submits replay work in-order. Waiting here would only stall capture loading;
  // command buffer completion is synchronised by the replay driver when its results are consumed.
  return true;
}

void WrappedMTLCommandBuffer::waitUntilScheduled()
{
  SERIALISE_TIME_CALL(Unwrap(this)->waitUntilScheduled());
  if(IsCaptureMode(m_State) && IsActiveCapturing(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_waitUntilScheduled);
      Serialise_waitUntilScheduled(ser);
      chunk = scope.Get();
    }
    GetRecord(this)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_waitUntilCompleted(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLCommandBuffer::waitUntilCompleted()
{
  SERIALISE_TIME_CALL(Unwrap(this)->waitUntilCompleted());
  if(IsCaptureMode(m_State))
  {
    if(IsActiveCapturing(m_State))
    {
      Chunk *chunk = NULL;
      {
        CACHE_THREAD_SERIALISER();
        SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandBuffer_waitUntilCompleted);
        Serialise_waitUntilCompleted(ser);
        chunk = scope.Get();
      }
      MetalResourceRecord *bufferRecord = GetRecord(this);
      bufferRecord->AddChunk(chunk);
    }
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandBuffer,
                                            WrappedMTLBlitCommandEncoder *encoder,
                                            blitCommandEncoder);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandBuffer,
                                            WrappedMTLBlitCommandEncoder *encoder,
                                            blitCommandEncoderWithDescriptor,
                                            bool hasSampleBuffers);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandBuffer,
                                            WrappedMTLComputeCommandEncoder *encoder,
                                            computeCommandEncoder);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandBuffer,
                                            WrappedMTLComputeCommandEncoder *encoder,
                                            computeCommandEncoderWithDescriptor,
                                            MTL::DispatchType dispatchType);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandBuffer,
                                            WrappedMTLComputeCommandEncoder *encoder,
                                            computeCommandEncoder,
                                            MTL::DispatchType dispatchType);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandBuffer,
                                            WrappedMTLRenderCommandEncoder *encoder,
                                            renderCommandEncoderWithDescriptor,
                                            RDMTL::RenderPassDescriptor &descriptor);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, presentDrawable,
                                WrappedMTLTexture *presentedImage);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, commit);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, enqueue);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, pushDebugGroup,
                                NS::String *string);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, popDebugGroup);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, waitUntilScheduled);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, waitUntilCompleted);
