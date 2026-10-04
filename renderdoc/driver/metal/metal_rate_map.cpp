// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_rate_map.h"
#include "metal_buffer.h"
#include "metal_device.h"
#include "metal_manager.h"

WrappedMTLRasterizationRateMap::WrappedMTLRasterizationRateMap(
    MTL::RasterizationRateMap *real, ResourceId id, WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newRasterizationRateMap(
    SerialiserType &ser, WrappedMTLRasterizationRateMap *rateMap, MTL::Size screenSize,
    rdcarray<float> horizontal, rdcarray<float> vertical, bool supported,
    rdcarray<rdcarray<float>> extraHorizontal, rdcarray<rdcarray<float>> extraVertical)
{
  SERIALISE_ELEMENT_LOCAL(RateMap, GetResID(rateMap))
      .TypedAs("MTLRasterizationRateMap"_lit).Important();
  SERIALISE_ELEMENT(screenSize).Important();
  SERIALISE_ELEMENT(horizontal).Important();
  SERIALISE_ELEMENT(vertical).Important();
  SERIALISE_ELEMENT(supported).Important();
  if(ser.VersionAtLeast(0x3))
  {
    SERIALISE_ELEMENT(extraHorizontal).Important();
    SERIALISE_ELEMENT(extraVertical).Important();
  }
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    const size_t layerCount = 1 + extraHorizontal.size();
    if(!supported || RateMap == ResourceId() || GetResourceManager()->HasResource(RateMap) ||
       !screenSize.width || !screenSize.height || screenSize.width > 8192 ||
       screenSize.height > 8192 || horizontal.size() < 2 || vertical.size() < 2 ||
       horizontal.size() > 64 || vertical.size() > 64 ||
       extraHorizontal.size() != extraVertical.size() || layerCount > 4 ||
       !Unwrap(this)->supportsRasterizationRateMap(layerCount))
    {
      RDCERR("Invalid or unsupported Metal rasterization rate map descriptor");
      return false;
    }
    for(float sample : horizontal)
      if(!(sample >= 0.0f && sample <= 1.0f)) return false;
    for(float sample : vertical)
      if(!(sample >= 0.0f && sample <= 1.0f)) return false;
    rdcarray<MTL::RasterizationRateLayerDescriptor *> layers;
    MTL::RasterizationRateLayerDescriptor *layer =
        MTL::RasterizationRateLayerDescriptor::alloc()->init(
            MTL::Size::Make(horizontal.size(), vertical.size(), 0),
            horizontal.data(), vertical.data());
    if(!layer) return false;
    layers.push_back(layer);
    for(size_t i = 0; i < extraHorizontal.size(); i++)
    {
      const rdcarray<float> &h = extraHorizontal[i];
      const rdcarray<float> &v = extraVertical[i];
      if(h.size() < 2 || h.size() > 64 || v.size() < 2 || v.size() > 64)
      {
        for(auto *entry : layers) entry->release();
        return false;
      }
      for(float sample : h)
        if(!(sample >= 0.0f && sample <= 1.0f))
        {
          for(auto *entry : layers) entry->release();
          return false;
        }
      for(float sample : v)
        if(!(sample >= 0.0f && sample <= 1.0f))
        {
          for(auto *entry : layers) entry->release();
          return false;
        }
      MTL::RasterizationRateLayerDescriptor *extra =
          MTL::RasterizationRateLayerDescriptor::alloc()->init(
              MTL::Size::Make(h.size(), v.size(), 0), h.data(), v.data());
      if(!extra)
      {
        for(auto *entry : layers) entry->release();
        return false;
      }
      layers.push_back(extra);
    }
    rdcarray<const MTL::RasterizationRateLayerDescriptor *> layerPointers;
    for(auto *entry : layers) layerPointers.push_back(entry);
    MTL::RasterizationRateMapDescriptor *descriptor =
        MTL::RasterizationRateMapDescriptor::rasterizationRateMapDescriptor(
            MTL::Size::Make(screenSize.width, screenSize.height, 0),
            layerPointers.size(), layerPointers.data());
    MTL::RasterizationRateMap *real = Unwrap(this)->newRasterizationRateMap(descriptor);
    for(auto *entry : layers) entry->release();
    if(!real)
    {
      RDCERR("Metal failed to recreate rasterization rate map");
      return false;
    }
    WrappedMTLRasterizationRateMap *wrapped = NULL;
    GetResourceManager()->WrapResource(RateMap, real, wrapped, true);
    wrapped->horizontal.push_back(horizontal);
    wrapped->vertical.push_back(vertical);
    wrapped->horizontal.append(extraHorizontal);
    wrapped->vertical.append(extraVertical);
    AddResource(RateMap, ResourceType::StateObject, "Rasterization Rate Map");
    DerivedResource(this, RateMap);
  }
  return true;
}

WrappedMTLRasterizationRateMap *WrappedMTLDevice::newRasterizationRateMap(
    MTL::RasterizationRateMapDescriptor *descriptor)
{
  if(!descriptor) return NULL;
  MTL::Size screenSize = descriptor->screenSize();
  rdcarray<float> horizontal, vertical;
  rdcarray<rdcarray<float>> extraHorizontal, extraVertical;
  const NS::UInteger layerCount = descriptor->layerCount();
  bool supported = layerCount >= 1 && layerCount <= 4 &&
                   Unwrap(this)->supportsRasterizationRateMap(layerCount);
  for(NS::UInteger i = 0; supported && i < layerCount; i++)
  {
    MTL::RasterizationRateLayerDescriptor *layer = descriptor->layer(i);
    if(!layer) { supported = false; break; }
    const MTL::Size samples = layer->sampleCount();
    supported = samples.width >= 2 && samples.height >= 2 &&
                samples.width <= 64 && samples.height <= 64;
    if(supported)
    {
      rdcarray<float> h, v;
      h.assign(layer->horizontalSampleStorage(), samples.width);
      v.assign(layer->verticalSampleStorage(), samples.height);
      if(i == 0) { horizontal = h; vertical = v; }
      else { extraHorizontal.push_back(h); extraVertical.push_back(v); }
    }
  }
  MTL::RasterizationRateMap *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newRasterizationRateMap(descriptor));
  if(!real) return NULL;
  WrappedMTLRasterizationRateMap *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newRasterizationRateMapWithDescriptor);
    Serialise_newRasterizationRateMap(ser, wrapped, screenSize, horizontal, vertical, supported,
                                     extraHorizontal, extraVertical);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newRasterizationRateMap(
    ReadSerialiser &, WrappedMTLRasterizationRateMap *, MTL::Size,
    rdcarray<float>, rdcarray<float>, bool, rdcarray<rdcarray<float>>,
    rdcarray<rdcarray<float>>);
template bool WrappedMTLDevice::Serialise_newRasterizationRateMap(
    WriteSerialiser &, WrappedMTLRasterizationRateMap *, MTL::Size,
    rdcarray<float>, rdcarray<float>, bool, rdcarray<rdcarray<float>>,
    rdcarray<rdcarray<float>>);

template <typename SerialiserType>
bool WrappedMTLRasterizationRateMap::Serialise_copyParameterDataToBuffer(
    SerialiserType &ser, WrappedMTLBuffer *buffer, NS::UInteger offset)
{
  SERIALISE_ELEMENT_LOCAL(RateMap, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    MTL::RasterizationRateMap *realMap = Unwrap(RateMap);
    MTL::Buffer *realBuffer = Unwrap(buffer);
    const MTL::SizeAndAlign requirements =
        realMap ? realMap->parameterBufferSizeAndAlign() : MTL::SizeAndAlign{};
    if(!RateMap || RateMap->m_Type != eResRasterizationRateMap || !realMap ||
       !buffer || buffer->m_Type != eResBuffer || !realBuffer ||
       realBuffer->storageMode() != MTL::StorageModeShared ||
       !requirements.align || offset % requirements.align ||
       offset > realBuffer->length() ||
       requirements.size > realBuffer->length() - offset)
    {
      RDCERR("Invalid Metal rasterization map parameter-buffer copy");
      return false;
    }
    realMap->copyParameterDataToBuffer(realBuffer, offset);
  }
  return true;
}

void WrappedMTLRasterizationRateMap::copyParameterDataToBuffer(
    WrappedMTLBuffer *buffer, NS::UInteger offset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->copyParameterDataToBuffer(Unwrap(buffer), offset));
  if(IsCaptureMode(m_State) && buffer)
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRasterizationRateMap_copyParameterDataToBuffer);
    Serialise_copyParameterDataToBuffer(ser, buffer, offset);
    MetalResourceRecord *record = GetRecord(buffer);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
    GetResourceManager()->MarkDirtyResource(GetResID(buffer));
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRasterizationRateMap, void,
                                copyParameterDataToBuffer, WrappedMTLBuffer *buffer,
                                NS::UInteger offset);
