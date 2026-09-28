// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_event.h"
#include "metal_device.h"
#include "metal_command_buffer.h"
#include <objc/runtime.h>

WrappedMTLEvent::WrappedMTLEvent(MTL::Event *real, ResourceId id, WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  m_IsSharedEvent = real && class_respondsToSelector(
      object_getClass((struct objc_object *)real), sel_registerName("signaledValue"));
  if(real && id != ResourceId() && IsCaptureMode(m_State))
  {
    AllocateObjCBridge(this);
    if(m_IsSharedEvent)
    {
      Class sharedClass = objc_lookUpClass("ObjCBridgeMTLSharedEvent");
      RDCASSERT(sharedClass && class_getInstanceSize(sharedClass) ==
                                 class_getInstanceSize(objc_lookUpClass("ObjCBridgeMTLEvent")));
      object_setClass((struct objc_object *)this, sharedClass);
    }
  }
}

bool WrappedMTLEvent::PrepareReplay(uint64_t epoch)
{
  if(!epoch) return false;
  if(m_Epoch == epoch) return true;
  // Native event values cannot be reset. A new replay must not inherit signalled values from a
  // later event selection. ReplayLog finishes the previous submission before advancing its epoch.
  MTL::Event *real = NULL;
  if(m_AliasSource)
  {
    WrappedMTLEvent *root = AliasRoot();
    if(!root->PrepareReplay(epoch)) return false;
    MTL::SharedEventHandle *handle = ((MTL::SharedEvent *)Unwrap(root))->newSharedEventHandle();
    if(!handle) return false;
    real = (MTL::Event *)Unwrap(m_Device)->newSharedEvent(handle);
    handle->release();
  }
  else
    real = m_IsSharedEvent ? (MTL::Event *)Unwrap(m_Device)->newSharedEvent() :
                             Unwrap(m_Device)->newEvent();
  if(!real) return false;
  if(!m_AliasSource && m_IsSharedEvent && m_InitialHostValue)
    ((MTL::SharedEvent *)real)->setSignaledValue(m_InitialHostValue);
  GetResourceManager()->ReplaceRealResource(this, real, true);
  m_Epoch = epoch;
  lastSignal = m_AliasSource ? AliasRoot()->lastSignal :
               m_IsSharedEvent ? m_InitialHostValue : 0;
  return true;
}

bool WrappedMTLEvent::SetInitialHostValue(uint64_t value)
{
  if(!m_IsSharedEvent || m_AliasSource || value < m_InitialHostValue)
    return false;
  m_InitialHostValue = value;
  return true;
}

void WrappedMTLEvent::SetHostSignaledValue(uint64_t value)
{
  ((MTL::SharedEvent *)Unwrap(this))->setSignaledValue(value);
  if(!IsCaptureMode(m_State)) return;
  if(m_AliasSource || m_GPUEventEncoded)
  {
    MarkUnsupportedHostMutation();
    return;
  }
  m_InitialHostValue = value;
  CACHE_THREAD_SERIALISER();
  SCOPED_SERIALISE_CHUNK(MetalChunk::MTLSharedEvent_setInitialSignaledValue);
  m_Device->Serialise_setSharedEventInitialValue(ser, this, value);
  GetRecord(this)->AddChunk(scope.Get());
}

void WrappedMTLEvent::MarkUnsupportedHostMutation()
{
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLSharedEvent_unsupportedHostMutation);
    GetRecord(this)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newEvent(SerialiserType &ser, WrappedMTLEvent *event)
{
  SERIALISE_ELEMENT_LOCAL(Event, GetResID(event)).TypedAs("MTLEvent"_lit);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(Event == ResourceId() || GetResourceManager()->HasResource(Event))
    {
      RDCERR("Invalid Metal event identity");
      return false;
    }
    MTL::Event *real = Unwrap(this)->newEvent();
    if(!real) return false;
    WrappedMTLEvent *wrapped = NULL;
    GetResourceManager()->WrapResource(Event, real, wrapped, true);
    AddResource(Event, ResourceType::Sync, "Event");
    DerivedResource(this, Event);
  }
  return true;
}

WrappedMTLEvent *WrappedMTLDevice::newEvent()
{
  MTL::Event *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newEvent());
  if(!real) return NULL;
  WrappedMTLEvent *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newEvent);
    Serialise_newEvent(ser, wrapped);
    GetResourceManager()->AddResourceRecord(wrapped)->AddChunk(scope.Get());
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newSharedEvent(SerialiserType &ser, WrappedMTLEvent *event)
{
  SERIALISE_ELEMENT_LOCAL(Event, GetResID(event)).TypedAs("MTLSharedEvent"_lit);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(Event == ResourceId() || GetResourceManager()->HasResource(Event))
    {
      RDCERR("Invalid Metal shared-event identity");
      return false;
    }
    MTL::SharedEvent *real = Unwrap(this)->newSharedEvent();
    if(!real) return false;
    WrappedMTLEvent *wrapped = NULL;
    GetResourceManager()->WrapResource(Event, (MTL::Event *)real, wrapped, true);
    AddResource(Event, ResourceType::Sync, "Shared Event");
    DerivedResource(this, Event);
  }
  return true;
}

WrappedMTLEvent *WrappedMTLDevice::newSharedEvent()
{
  MTL::SharedEvent *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newSharedEvent());
  if(!real) return NULL;
  WrappedMTLEvent *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), (MTL::Event *)real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newSharedEvent);
    Serialise_newSharedEvent(ser, wrapped);
    GetResourceManager()->AddResourceRecord(wrapped)->AddChunk(scope.Get());
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_importSharedEventHandle(SerialiserType &ser,
                                                         WrappedMTLEvent *event,
                                                         WrappedMTLEvent *source)
{
  SERIALISE_ELEMENT_LOCAL(Event, GetResID(event)).TypedAs("MTLSharedEvent"_lit).Important();
  SERIALISE_ELEMENT_LOCAL(Source, GetResID(source)).TypedAs("MTLSharedEvent"_lit).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(Event == ResourceId() || GetResourceManager()->HasResource(Event) ||
       Source == ResourceId() || !GetResourceManager()->HasResource(Source))
    {
      RDCERR("Invalid or unsupported Metal shared-event handle import identity");
      return false;
    }
    source = (WrappedMTLEvent *)GetResourceManager()->GetResource(Source);
    if(!source || source->m_Type != eResEvent || !source->m_Real || !source->IsSharedEvent() ||
       source->m_Device != this)
    {
      RDCERR("Invalid or unsupported Metal shared-event handle source or import identity");
      return false;
    }
    MTL::SharedEventHandle *handle = ((MTL::SharedEvent *)Unwrap(source))->newSharedEventHandle();
    if(!handle) return false;
    MTL::SharedEvent *real = Unwrap(this)->newSharedEvent(handle);
    handle->release();
    if(!real) return false;
    WrappedMTLEvent *wrapped = NULL;
    GetResourceManager()->WrapResource(Event, (MTL::Event *)real, wrapped, true);
    wrapped->SetAliasSource(source);
    AddResource(Event, ResourceType::Sync, "Shared Event Handle Import");
    DerivedResource(source, Event);
  }
  return true;
}

WrappedMTLEvent *WrappedMTLDevice::ImportSharedEventHandle(MTL::SharedEvent *real,
                                                           WrappedMTLEvent *source)
{
  if(!real) return NULL;
  WrappedMTLEvent *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), (MTL::Event *)real, wrapped);
  if(source && source->m_Type == eResEvent && source->IsSharedEvent() &&
     source->m_Device == this)
    wrapped->SetAliasSource(source);
  else
    source = NULL;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newSharedEventWithHandle);
    Serialise_importSharedEventHandle(ser, wrapped, source);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    if(source) record->AddParent(GetRecord(source));
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_setSharedEventInitialValue(
    SerialiserType &ser, WrappedMTLEvent *event, uint64_t value)
{
  SERIALISE_ELEMENT(event).Important();
  SERIALISE_ELEMENT(value).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (!event || event->m_Type != eResEvent || !Unwrap(event) ||
      !event->SetInitialHostValue(value)))
  {
    RDCERR("Invalid Metal shared-event initial host value or resource");
    return false;
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLCommandBuffer::Serialise_encodeEvent(SerialiserType &ser, WrappedMTLEvent *event,
                                                   uint64_t value, bool signal)
{
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, this);
  SERIALISE_ELEMENT(event).Important();
  SERIALISE_ELEMENT(value).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!CommandBuffer || CommandBuffer->m_Type != eResCommandBuffer || !Unwrap(CommandBuffer) ||
       !m_Device->CanEncodeReplayEvent(CommandBuffer) || !event || event->m_Type != eResEvent ||
       !Unwrap(event) || !event->PrepareReplay(m_Device->GetReplayEpoch()))
    {
      RDCERR("Invalid/unsupported Metal event command: signal must increase, wait needs a prior captured signal");
      return false;
    }
    WrappedMTLEvent *timeline = event->AliasRoot();
    if(signal ? value <= timeline->lastSignal : value > timeline->lastSignal)
    {
      RDCERR("Invalid Metal shared-event timeline value");
      return false;
    }
    if(signal)
    {
      Unwrap(CommandBuffer)->encodeSignalEvent(Unwrap(event), value);
      timeline->lastSignal = value;
    }
    else
      Unwrap(CommandBuffer)->encodeWait(Unwrap(event), value);
  }
  return true;
}

void WrappedMTLCommandBuffer::encodeEvent(WrappedMTLEvent *event, uint64_t value, bool signal)
{
  if(signal)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->encodeSignalEvent(Unwrap(event), value));
  }
  else
  {
    SERIALISE_TIME_CALL(Unwrap(this)->encodeWait(Unwrap(event), value));
  }
  CaptureEvent(event, value, signal);
}

void WrappedMTLCommandBuffer::CaptureEvent(WrappedMTLEvent *event, uint64_t value, bool signal)
{
  if(IsCaptureMode(m_State))
  {
    if(event) event->MarkGPUEventEncoded();
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(signal ? MetalChunk::MTLCommandBuffer_encodeSignalEvent :
                                   MetalChunk::MTLCommandBuffer_encodeWaitForEvent);
    Serialise_encodeEvent(ser, event, value, signal);
    MetalResourceRecord *record = GetRecord(this);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(event), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLEvent *, newEvent);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLEvent *, newSharedEvent);
template bool WrappedMTLDevice::Serialise_setSharedEventInitialValue(
    ReadSerialiser &, WrappedMTLEvent *, uint64_t);
template bool WrappedMTLDevice::Serialise_setSharedEventInitialValue(
    WriteSerialiser &, WrappedMTLEvent *, uint64_t);
template bool WrappedMTLDevice::Serialise_importSharedEventHandle(
    ReadSerialiser &, WrappedMTLEvent *, WrappedMTLEvent *);
template bool WrappedMTLDevice::Serialise_importSharedEventHandle(
    WriteSerialiser &, WrappedMTLEvent *, WrappedMTLEvent *);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLCommandBuffer, void, encodeEvent, WrappedMTLEvent *, uint64_t, bool);
