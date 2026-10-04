// SPDX-License-Identifier: MIT
#include "metal_parallel_render_command_encoder.h"
#include "metal_command_buffer.h"
#include "metal_render_command_encoder.h"
#include "metal_replay.h"

static bool ValidParallelStoreAction(MTL::StoreAction action)
{
  return action >= MTL::StoreActionDontCare &&
         action <= MTL::StoreActionCustomSampleDepthStore && action != MTL::StoreActionUnknown;
}

static bool ValidParallelStoreActionOptions(MTL::StoreActionOptions options)
{
  return ((uint64_t)options & ~(uint64_t)MTL::StoreActionOptionValidMask) == 0;
}

WrappedMTLParallelRenderCommandEncoder::WrappedMTLParallelRenderCommandEncoder(
    MTL::ParallelRenderCommandEncoder *real, ResourceId id, WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

void WrappedMTLParallelRenderCommandEncoder::ResolveDeferredStoreActions()
{
  if(!m_DeferredStoreActions || !m_Real)
    return;
  MTL::ParallelRenderCommandEncoder *real = Unwrap(this);
  for(uint32_t i = 0; i < 8; i++)
    if(m_DeferredStoreActions & (1U << i))
    {
      real->setColorStoreAction(MTL::StoreActionStore, i);
      m_Device->GetReplay()->SetRenderPassStoreAction(i, MTL::StoreActionStore);
    }
  if(m_DeferredStoreActions & (1U << 8))
  {
    real->setDepthStoreAction(MTL::StoreActionStore);
    m_Device->GetReplay()->SetRenderPassStoreAction(8, MTL::StoreActionStore);
  }
  if(m_DeferredStoreActions & (1U << 9))
  {
    real->setStencilStoreAction(MTL::StoreActionStore);
    m_Device->GetReplay()->SetRenderPassStoreAction(9, MTL::StoreActionStore);
  }
  m_DeferredStoreActions = 0;
}

template <typename SerialiserType>
bool WrappedMTLParallelRenderCommandEncoder::Serialise_renderCommandEncoder(
    SerialiserType &ser, WrappedMTLRenderCommandEncoder *encoder)
{
  SERIALISE_ELEMENT_LOCAL(ParallelRenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, GetResID(encoder))
      .TypedAs("MTLRenderCommandEncoder"_lit).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ParallelRenderCommandEncoder ||
       ParallelRenderCommandEncoder->m_Type != eResParallelRenderCommandEncoder ||
       ParallelRenderCommandEncoder != m_Device->GetReplayParallelRenderCommandEncoder(ParallelRenderCommandEncoder) ||
       !Unwrap(ParallelRenderCommandEncoder) ||
       m_Device->GetReplayRenderCommandEncoder() || RenderCommandEncoder == ResourceId())
    {
      RDCERR("Invalid Metal parallel child encoder or parent identity");
      return false;
    }
    WrappedMTLObject *existing = GetResourceManager()->GetResource(RenderCommandEncoder, true);
    if(existing && existing->m_Type != eResRenderCommandEncoder)
    {
      RDCERR("Invalid Metal parallel child encoder resource identity");
      return false;
    }
    MTL::RenderCommandEncoder *real = Unwrap(ParallelRenderCommandEncoder)->renderCommandEncoder();
    if(!real) return false;
    WrappedMTLRenderCommandEncoder *wrapped = (WrappedMTLRenderCommandEncoder *)existing;
    if(wrapped)
      GetResourceManager()->ReplaceRealResource(wrapped, real);
    else
      GetResourceManager()->WrapResource(RenderCommandEncoder, real, wrapped);
    wrapped->SetCommandBuffer(ParallelRenderCommandEncoder->GetCommandBuffer());
    wrapped->SetParallelParent(ParallelRenderCommandEncoder);
    m_Device->SetReplayRenderCommandEncoder(wrapped);
    m_Device->GetReplay()->BeginRenderPass(
        m_Device->GetReplay()->GetRenderPassDescriptor());
    if(IsLoading(m_State))
    {
      m_Device->AddResource(RenderCommandEncoder, ResourceType::CommandBuffer,
                            "Parallel Render Child Encoder");
      m_Device->DerivedResource(ParallelRenderCommandEncoder, RenderCommandEncoder);
      AddEvent();
      ActionDescription action;
      action.customName = "Begin Metal Parallel Render Child";
      action.flags = ActionFlags::PassBoundary | ActionFlags::BeginPass;
      AddAction(action);
    }
  }
  return true;
}

WrappedMTLRenderCommandEncoder *WrappedMTLParallelRenderCommandEncoder::renderCommandEncoder()
{
  MTL::RenderCommandEncoder *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->renderCommandEncoder());
  if(!real) return NULL;
  WrappedMTLRenderCommandEncoder *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->SetCommandBuffer(m_CommandBuffer);
  wrapped->SetParallelParent(this);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLParallelRenderCommandEncoder_renderCommandEncoder);
    Serialise_renderCommandEncoder(ser, wrapped);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
    GetResourceManager()->AddResourceRecord(wrapped);
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLParallelRenderCommandEncoder::Serialise_endEncoding(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(ParallelRenderCommandEncoder, this).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ParallelRenderCommandEncoder ||
       ParallelRenderCommandEncoder != m_Device->GetReplayParallelRenderCommandEncoder(ParallelRenderCommandEncoder) ||
       ParallelRenderCommandEncoder->m_Type != eResParallelRenderCommandEncoder ||
       !Unwrap(ParallelRenderCommandEncoder) || m_Device->GetReplayRenderCommandEncoder())
    {
      RDCERR("Invalid Metal parallel render pass end");
      return false;
    }
    ParallelRenderCommandEncoder->ResolveDeferredStoreActions();
    m_Device->FinaliseReplayStores(Unwrap(ParallelRenderCommandEncoder), false);
    Unwrap(ParallelRenderCommandEncoder)->endEncoding();
    m_Device->ApplyReplayStoreDiscards(Unwrap(ParallelRenderCommandEncoder->GetCommandBuffer()),
        m_Device->GetReplay()->GetRenderPassDescriptor());
    if(IsLoading(m_State) && !m_Device->GetReplay()->FlushRenderIndirectActions(
        GetResID(ParallelRenderCommandEncoder),Unwrap(ParallelRenderCommandEncoder->GetCommandBuffer())))
      return false;
    m_Device->SetReplayParallelRenderCommandEncoder(NULL);
    ActionDescription action;
    if(IsLoading(m_State))
    {
      action.customName = StringFormat::Fmt(
          "End Metal Parallel Render Pass (%s)",
          RDMTL::RenderPassOpString(m_Device->GetReplay()->GetRenderPassDescriptor(), true).c_str());
      action.flags = ActionFlags::PassBoundary | ActionFlags::EndPass;
      m_Device->GetReplay()->SetActionOutputs(action);
    }
    if(IsLoading(m_State))
    {
      AddEvent();
      AddAction(action);
      m_Device->GetReplay()->AddRenderPassStoreUsage(
          m_Device->GetReplay()->GetRenderPassDescriptor());
    }
    m_Device->GetReplay()->EndRenderPass();
  }
  return true;
}

void WrappedMTLParallelRenderCommandEncoder::endEncoding()
{
  SERIALISE_TIME_CALL(Unwrap(this)->endEncoding());
  m_Device->FlushRenderIndirectCaptures(m_CommandBuffer,m_ID);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLParallelRenderCommandEncoder_endEncoding);
    Serialise_endEncoding(ser);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLParallelRenderCommandEncoder::Serialise_setStoreAction(
    SerialiserType &ser, MTL::StoreAction action, NS::UInteger index, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(ParallelRenderCommandEncoder, this).Important();
  uint64_t actionValue = (uint64_t)action;
  SERIALISE_ELEMENT(actionValue).Named("storeAction"_lit).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ParallelRenderCommandEncoder ||
       ParallelRenderCommandEncoder != m_Device->GetReplayParallelRenderCommandEncoder(ParallelRenderCommandEncoder) ||
       ParallelRenderCommandEncoder->m_Type != eResParallelRenderCommandEncoder ||
       !Unwrap(ParallelRenderCommandEncoder) || variant > 2 ||
       !ValidParallelStoreAction((MTL::StoreAction)actionValue) ||
       (variant == 0 && index >= 8) || (variant != 0 && index != 0))
    {
      RDCERR("Invalid Metal parallel render store action");
      return false;
    }
    action = (MTL::StoreAction)actionValue;
    if(variant == 0) Unwrap(ParallelRenderCommandEncoder)->setColorStoreAction(action, index);
    if(variant == 1) Unwrap(ParallelRenderCommandEncoder)->setDepthStoreAction(action);
    if(variant == 2) Unwrap(ParallelRenderCommandEncoder)->setStencilStoreAction(action);
    const uint32_t attachment = variant == 0 ? (uint32_t)index : variant == 1 ? 8 : 9;
    ParallelRenderCommandEncoder->m_DeferredStoreActions &= ~(1U << attachment);
    m_Device->GetReplay()->SetRenderPassStoreAction(attachment, action);
  }
  return true;
}

void WrappedMTLParallelRenderCommandEncoder::setStoreAction(MTL::StoreAction action,
                                                              NS::UInteger index,
                                                              uint32_t variant)
{
  if(variant == 0)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setColorStoreAction(action, index));
  }
  else if(variant == 1)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setDepthStoreAction(action));
  }
  else
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setStencilStoreAction(action));
  }
  if(IsCaptureMode(m_State))
  {
    const MetalChunk chunk = variant == 0 ? MetalChunk::MTLParallelRenderCommandEncoder_setColorStoreAction :
                             variant == 1 ? MetalChunk::MTLParallelRenderCommandEncoder_setDepthStoreAction :
                                            MetalChunk::MTLParallelRenderCommandEncoder_setStencilStoreAction;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_setStoreAction(ser, action, index, variant);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLParallelRenderCommandEncoder::Serialise_setStoreActionOptions(
    SerialiserType &ser, MTL::StoreActionOptions options, NS::UInteger index, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(ParallelRenderCommandEncoder, this).Important();
  uint64_t optionsValue = (uint64_t)options;
  SERIALISE_ELEMENT(optionsValue).Named("storeActionOptions"_lit).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ParallelRenderCommandEncoder ||
       ParallelRenderCommandEncoder != m_Device->GetReplayParallelRenderCommandEncoder(ParallelRenderCommandEncoder) ||
       ParallelRenderCommandEncoder->m_Type != eResParallelRenderCommandEncoder ||
       !Unwrap(ParallelRenderCommandEncoder) || variant > 2 ||
       !ValidParallelStoreActionOptions((MTL::StoreActionOptions)optionsValue) ||
       (variant == 0 && index >= 8) || (variant != 0 && index != 0))
    {
      RDCERR("Invalid Metal parallel render store action options");
      return false;
    }
    options = (MTL::StoreActionOptions)optionsValue;
    if(variant == 0) Unwrap(ParallelRenderCommandEncoder)->setColorStoreActionOptions(options, index);
    if(variant == 1) Unwrap(ParallelRenderCommandEncoder)->setDepthStoreActionOptions(options);
    if(variant == 2) Unwrap(ParallelRenderCommandEncoder)->setStencilStoreActionOptions(options);
    m_Device->GetReplay()->SetRenderPassStoreOptions(
        variant == 0 ? (uint32_t)index : variant == 1 ? 8 : 9, options);
  }
  return true;
}

void WrappedMTLParallelRenderCommandEncoder::setStoreActionOptions(
    MTL::StoreActionOptions options, NS::UInteger index, uint32_t variant)
{
  if(variant == 0)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setColorStoreActionOptions(options, index));
  }
  else if(variant == 1)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setDepthStoreActionOptions(options));
  }
  else
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setStencilStoreActionOptions(options));
  }
  if(IsCaptureMode(m_State))
  {
    const MetalChunk chunk = variant == 0 ? MetalChunk::MTLParallelRenderCommandEncoder_setColorStoreActionOptions :
                             variant == 1 ? MetalChunk::MTLParallelRenderCommandEncoder_setDepthStoreActionOptions :
                                            MetalChunk::MTLParallelRenderCommandEncoder_setStencilStoreActionOptions;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_setStoreActionOptions(ser, options, index, variant);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLParallelRenderCommandEncoder::Serialise_debugLabel(
    SerialiserType &ser, NS::String *label, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(ParallelRenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(label);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ParallelRenderCommandEncoder ||
       ParallelRenderCommandEncoder != m_Device->GetReplayParallelRenderCommandEncoder(ParallelRenderCommandEncoder) ||
       ParallelRenderCommandEncoder->m_Type != eResParallelRenderCommandEncoder ||
       !Unwrap(ParallelRenderCommandEncoder) || variant > 2)
      return false;
    if(variant == 0) Unwrap(ParallelRenderCommandEncoder)->pushDebugGroup(label);
    if(variant == 1) Unwrap(ParallelRenderCommandEncoder)->popDebugGroup();
    if(variant == 2) Unwrap(ParallelRenderCommandEncoder)->insertDebugSignpost(label);
  }
  if(IsLoading(m_State))
  {
    AddEvent();
    m_Device->GetReplay()->AddDebugGroup(label, variant == 0 ? ActionFlags::PushMarker :
                                                variant == 1 ? ActionFlags::PopMarker :
                                                               ActionFlags::SetMarker);
  }
  return true;
}

void WrappedMTLParallelRenderCommandEncoder::debugLabel(NS::String *label, uint32_t variant)
{
  if(variant == 0)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->pushDebugGroup(label));
  }
  else if(variant == 1)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->popDebugGroup());
  }
  else
  {
    SERIALISE_TIME_CALL(Unwrap(this)->insertDebugSignpost(label));
  }
  if(IsCaptureMode(m_State))
  {
    const MetalChunk chunk = variant == 0 ? MetalChunk::MTLParallelRenderCommandEncoder_pushDebugGroup :
                             variant == 1 ? MetalChunk::MTLParallelRenderCommandEncoder_popDebugGroup :
                                            MetalChunk::MTLParallelRenderCommandEncoder_insertDebugSignpost;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_debugLabel(ser, label, variant);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template bool WrappedMTLParallelRenderCommandEncoder::Serialise_renderCommandEncoder(
    ReadSerialiser &, WrappedMTLRenderCommandEncoder *);
template bool WrappedMTLParallelRenderCommandEncoder::Serialise_renderCommandEncoder(
    WriteSerialiser &, WrappedMTLRenderCommandEncoder *);
template bool WrappedMTLParallelRenderCommandEncoder::Serialise_endEncoding(ReadSerialiser &);
template bool WrappedMTLParallelRenderCommandEncoder::Serialise_endEncoding(WriteSerialiser &);
template bool WrappedMTLParallelRenderCommandEncoder::Serialise_setStoreAction(
    ReadSerialiser &, MTL::StoreAction, NS::UInteger, uint32_t);
template bool WrappedMTLParallelRenderCommandEncoder::Serialise_setStoreAction(
    WriteSerialiser &, MTL::StoreAction, NS::UInteger, uint32_t);
template bool WrappedMTLParallelRenderCommandEncoder::Serialise_setStoreActionOptions(
    ReadSerialiser &, MTL::StoreActionOptions, NS::UInteger, uint32_t);
template bool WrappedMTLParallelRenderCommandEncoder::Serialise_setStoreActionOptions(
    WriteSerialiser &, MTL::StoreActionOptions, NS::UInteger, uint32_t);
template bool WrappedMTLParallelRenderCommandEncoder::Serialise_debugLabel(
    ReadSerialiser &, NS::String *, uint32_t);
template bool WrappedMTLParallelRenderCommandEncoder::Serialise_debugLabel(
    WriteSerialiser &, NS::String *, uint32_t);
