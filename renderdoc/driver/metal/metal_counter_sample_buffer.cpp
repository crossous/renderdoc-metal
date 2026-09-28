// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_counter_sample_buffer.h"
#include "metal_device.h"
#include "metal_manager.h"

WrappedMTLCounterSampleBuffer::WrappedMTLCounterSampleBuffer(
    MTL::CounterSampleBuffer *real, ResourceId id, WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newCounterSampleBuffer(
    SerialiserType &ser, WrappedMTLCounterSampleBuffer *sampleBuffer,
    rdcstr counterSetName, uint64_t sampleCount, uint64_t storageMode, bool supported)
{
  SERIALISE_ELEMENT_LOCAL(CounterSampleBuffer, GetResID(sampleBuffer))
      .TypedAs("MTLCounterSampleBuffer"_lit).Important();
  SERIALISE_ELEMENT(counterSetName).Important();
  SERIALISE_ELEMENT(sampleCount).Important();
  SERIALISE_ELEMENT(storageMode).Important();
  SERIALISE_ELEMENT(supported).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!supported || CounterSampleBuffer == ResourceId() ||
       GetResourceManager()->HasResource(CounterSampleBuffer) ||
       counterSetName != "timestamp" || sampleCount < 4 || sampleCount > 64 ||
       storageMode != MTL::StorageModeShared ||
       !Unwrap(this)->supportsCounterSampling(MTL::CounterSamplingPointAtStageBoundary))
    {
      RDCERR("Invalid or unsupported Metal stage-boundary counter sample buffer");
      return false;
    }
    MTL::CounterSet *set = NULL;
    NS::Array *sets = Unwrap(this)->counterSets();
    for(NS::UInteger i = 0; sets && i < sets->count(); i++)
    {
      MTL::CounterSet *candidate = sets->object<MTL::CounterSet>(i);
      if(candidate && candidate->name() &&
         counterSetName == candidate->name()->utf8String())
      {
        set = candidate;
        break;
      }
    }
    if(!set) return false;
    MTL::CounterSampleBufferDescriptor *descriptor =
        MTL::CounterSampleBufferDescriptor::alloc()->init();
    descriptor->setCounterSet(set);
    descriptor->setStorageMode(MTL::StorageModeShared);
    descriptor->setSampleCount(sampleCount);
    NS::Error *error = NULL;
    MTL::CounterSampleBuffer *real =
        Unwrap(this)->newCounterSampleBuffer(descriptor, &error);
    descriptor->release();
    if(!real)
    {
      RDCERR("Failed to recreate Metal counter sample buffer: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    WrappedMTLCounterSampleBuffer *wrapped = NULL;
    GetResourceManager()->WrapResource(CounterSampleBuffer, real, wrapped, true);
    AddResource(CounterSampleBuffer, ResourceType::StateObject, "Counter Sample Buffer");
    DerivedResource(this, CounterSampleBuffer);
  }
  return true;
}

WrappedMTLCounterSampleBuffer *WrappedMTLDevice::newCounterSampleBuffer(
    MTL::CounterSampleBufferDescriptor *descriptor, NS::Error **error)
{
  if(!descriptor) return NULL;
  MTL::CounterSet *set = descriptor->counterSet();
  const rdcstr name = set && set->name() ? set->name()->utf8String() : "";
  const uint64_t count = descriptor->sampleCount();
  const uint64_t mode = descriptor->storageMode();
  const bool supported = name == "timestamp" && count >= 4 && count <= 64 &&
                         mode == MTL::StorageModeShared &&
                         Unwrap(this)->supportsCounterSampling(
                             MTL::CounterSamplingPointAtStageBoundary);
  MTL::CounterSampleBuffer *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newCounterSampleBuffer(descriptor, error));
  if(!real) return NULL;
  WrappedMTLCounterSampleBuffer *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newCounterSampleBufferWithDescriptor);
    Serialise_newCounterSampleBuffer(ser, wrapped, name, count, mode, supported);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newCounterSampleBuffer(
    ReadSerialiser &, WrappedMTLCounterSampleBuffer *, rdcstr, uint64_t, uint64_t, bool);
template bool WrappedMTLDevice::Serialise_newCounterSampleBuffer(
    WriteSerialiser &, WrappedMTLCounterSampleBuffer *, rdcstr, uint64_t, uint64_t, bool);
