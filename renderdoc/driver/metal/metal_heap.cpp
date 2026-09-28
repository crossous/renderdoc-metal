// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_heap.h"
#include "metal_buffer.h"
#include "metal_device.h"
#include "metal_manager.h"
#include "metal_replay.h"
#include "metal_texture.h"

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newHeap(SerialiserType &ser, WrappedMTLHeap *heap,
                                         NS::UInteger size, MTL::StorageMode storageMode,
                                         MTL::CPUCacheMode cacheMode,
                                         MTL::HazardTrackingMode hazardMode, MTL::HeapType type)
{
  SERIALISE_ELEMENT_LOCAL(Heap, GetResID(heap)).TypedAs("MTLHeap"_lit).Important();
  SERIALISE_ELEMENT(size).Important();
  SERIALISE_ELEMENT(storageMode).Important();
  SERIALISE_ELEMENT(cacheMode);
  SERIALISE_ELEMENT(hazardMode);
  uint32_t heapType = (uint32_t)type;
  SERIALISE_ELEMENT(heapType).Named("type"_lit).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    type = (MTL::HeapType)heapType;
    // Placement heaps are admitted only with disjoint, explicitly aligned child buffers.
    // Aliasing, sparse mappings and placement textures still require separate lifetime models.
    if(Heap == ResourceId() || GetResourceManager()->HasResource(Heap) ||
       size < 4096 || size > 512 * 1024 * 1024 ||
       storageMode != MTL::StorageModePrivate ||
       cacheMode != MTL::CPUCacheModeDefaultCache ||
       hazardMode != MTL::HazardTrackingModeTracked ||
       (type != MTL::HeapTypeAutomatic && type != MTL::HeapTypePlacement))
    {
      RDCERR("Invalid or unsupported Metal heap descriptor or identity");
      return false;
    }
    MTL::HeapDescriptor *descriptor = MTL::HeapDescriptor::alloc()->init();
    descriptor->setSize(size);
    descriptor->setStorageMode(storageMode);
    descriptor->setCpuCacheMode(cacheMode);
    descriptor->setHazardTrackingMode(hazardMode);
    descriptor->setType(type);
    MTL::Heap *real = Unwrap(this)->newHeap(descriptor);
    descriptor->release();
    if(!real)
    {
      RDCERR("Metal failed to recreate captured heap");
      return false;
    }
    WrappedMTLHeap *wrapped = NULL;
    GetResourceManager()->WrapResource(Heap, real, wrapped, true);
    AddResource(Heap, ResourceType::Pool, "Heap");
    DerivedResource(this, Heap);
  }
  return true;
}

WrappedMTLHeap *WrappedMTLDevice::WrapNewHeap(MTL::Heap *real, NS::UInteger size,
                                              MTL::StorageMode storageMode,
                                              MTL::CPUCacheMode cacheMode,
                                              MTL::HazardTrackingMode hazardMode,
                                              MTL::HeapType type)
{
  if(!real) return NULL;
  WrappedMTLHeap *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newHeapWithDescriptor);
    Serialise_newHeap(ser, wrapped, size, storageMode, cacheMode, hazardMode, type);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newHeap(ReadSerialiser &, WrappedMTLHeap *,
    NS::UInteger, MTL::StorageMode, MTL::CPUCacheMode, MTL::HazardTrackingMode, MTL::HeapType);
template bool WrappedMTLDevice::Serialise_newHeap(WriteSerialiser &, WrappedMTLHeap *,
    NS::UInteger, MTL::StorageMode, MTL::CPUCacheMode, MTL::HazardTrackingMode, MTL::HeapType);

WrappedMTLHeap::WrappedMTLHeap(MTL::Heap *real, ResourceId id, WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
  {
    AllocateObjCBridge(this);
    MetalAssociateHeapProxy(real, this);
  }
}

template <typename SerialiserType>
bool WrappedMTLHeap::Serialise_newBuffer(SerialiserType &ser, WrappedMTLBuffer *buffer,
                                          NS::UInteger length, MTL::ResourceOptions options)
{
  SERIALISE_ELEMENT_LOCAL(Heap, this).Important();
  SERIALISE_ELEMENT_LOCAL(Buffer, GetResID(buffer)).TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(length).Important();
  SERIALISE_ELEMENT(options).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Heap || Heap->m_Type != eResHeap || !Heap->m_Real ||
       Buffer == ResourceId() || GetResourceManager()->HasResource(Buffer) ||
       length == 0 || length > 64 * 1024 * 1024 ||
       options != MTL::ResourceStorageModePrivate)
    {
      RDCERR("Invalid or unsupported Metal heap buffer identity, length or options");
      return false;
    }
    MTL::Buffer *real = Unwrap(Heap)->newBuffer(length, options);
    if(!real)
    {
      RDCERR("Metal failed to recreate heap-backed buffer");
      return false;
    }
    WrappedMTLBuffer *wrapped = NULL;
    GetResourceManager()->WrapResource(Buffer, real, wrapped, true);
    m_Device->AddResource(Buffer, ResourceType::Buffer, "Heap Buffer");
    m_Device->GetReplay()->AddBuffer(Buffer, length);
    m_Device->DerivedResource(Heap, Buffer);
  }
  return true;
}

WrappedMTLBuffer *WrappedMTLHeap::newBuffer(NS::UInteger length, MTL::ResourceOptions options)
{
  MTL::Buffer *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newBuffer(length, options));
  if(!real) return NULL;
  WrappedMTLBuffer *wrapped = NULL;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLHeap_newBuffer);
    Serialise_newBuffer(ser, wrapped, length, options);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
    MTL::StorageMode mode = real->storageMode();
    record->bufInfo = new MetalBufferInfo(mode);
    if(mode == MTL::StorageModeShared)
    {
      record->bufInfo->data = (byte *)real->contents();
      record->bufInfo->length = real->length();
    }
    else if(mode == MTL::StorageModePrivate)
      GetResourceManager()->MarkDirtyResource(id);
  }
  return wrapped;
}

template bool WrappedMTLHeap::Serialise_newBuffer(ReadSerialiser &, WrappedMTLBuffer *,
                                                   NS::UInteger, MTL::ResourceOptions);
template bool WrappedMTLHeap::Serialise_newBuffer(WriteSerialiser &, WrappedMTLBuffer *,
                                                   NS::UInteger, MTL::ResourceOptions);

template <typename SerialiserType>
bool WrappedMTLHeap::Serialise_newBufferWithOffset(SerialiserType &ser,
                                                   WrappedMTLBuffer *buffer,
                                                   NS::UInteger length,
                                                   MTL::ResourceOptions options,
                                                   NS::UInteger offset)
{
  SERIALISE_ELEMENT_LOCAL(Heap, this).Important();
  SERIALISE_ELEMENT_LOCAL(Buffer, GetResID(buffer)).TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(length).Important();
  SERIALISE_ELEMENT(options).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Heap || Heap->m_Type != eResHeap || !Heap->m_Real ||
       Unwrap(Heap)->type() != MTL::HeapTypePlacement ||
       Buffer == ResourceId() || GetResourceManager()->HasResource(Buffer) ||
       length == 0 || length > 64 * 1024 * 1024 ||
       options != MTL::ResourceStorageModePrivate)
    {
      RDCERR("Invalid or unsupported Metal placement heap buffer identity or options");
      return false;
    }
    const MTL::SizeAndAlign layout =
        Unwrap(m_Device)->heapBufferSizeAndAlign(length, options);
    const uint64_t heapSize = Unwrap(Heap)->size();
    if(layout.size == 0 || layout.align == 0 || offset % layout.align != 0 ||
       offset > heapSize || layout.size > heapSize - offset)
    {
      RDCERR("Invalid Metal placement heap buffer alignment or range");
      return false;
    }
    const uint64_t end = uint64_t(offset) + layout.size;
    for(const PlacementRange &range : Heap->m_PlacementRanges)
      if(uint64_t(offset) < range.end && range.begin < end)
      {
        RDCERR("Overlapping Metal placement heap buffers require alias lifetime replay");
        return false;
      }
    MTL::Buffer *real = Unwrap(Heap)->newBuffer(length, options, offset);
    if(!real || real->heapOffset() != offset)
    {
      if(real) real->release();
      RDCERR("Metal failed to recreate placement heap buffer at captured offset");
      return false;
    }
    Heap->m_PlacementRanges.push_back({uint64_t(offset), end});
    WrappedMTLBuffer *wrapped = NULL;
    GetResourceManager()->WrapResource(Buffer, real, wrapped, true);
    m_Device->AddResource(Buffer, ResourceType::Buffer, "Placement Heap Buffer");
    m_Device->GetReplay()->AddBuffer(Buffer, length);
    m_Device->DerivedResource(Heap, Buffer);
  }
  return true;
}

WrappedMTLBuffer *WrappedMTLHeap::newBufferWithOffset(NS::UInteger length,
                                                      MTL::ResourceOptions options,
                                                      NS::UInteger offset)
{
  MTL::Buffer *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newBuffer(length, options, offset));
  if(!real) return NULL;
  WrappedMTLBuffer *wrapped = NULL;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLHeap_newBufferWithOffset);
    Serialise_newBufferWithOffset(ser, wrapped, length, options, offset);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
    MTL::StorageMode mode = real->storageMode();
    record->bufInfo = new MetalBufferInfo(mode);
    if(mode == MTL::StorageModeShared)
    {
      record->bufInfo->data = (byte *)real->contents();
      record->bufInfo->length = real->length();
    }
    else if(mode == MTL::StorageModePrivate)
      GetResourceManager()->MarkDirtyResource(id);
  }
  return wrapped;
}

template bool WrappedMTLHeap::Serialise_newBufferWithOffset(ReadSerialiser &,
    WrappedMTLBuffer *, NS::UInteger, MTL::ResourceOptions, NS::UInteger);
template bool WrappedMTLHeap::Serialise_newBufferWithOffset(WriteSerialiser &,
    WrappedMTLBuffer *, NS::UInteger, MTL::ResourceOptions, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLHeap::Serialise_newTexture(SerialiserType &ser, WrappedMTLTexture *texture,
                                           RDMTL::TextureDescriptor &descriptor)
{
  SERIALISE_ELEMENT_LOCAL(Heap, this).Important();
  SERIALISE_ELEMENT_LOCAL(Texture, GetResID(texture)).TypedAs("MTLTexture"_lit).Important();
  SERIALISE_ELEMENT(descriptor).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Heap || Heap->m_Type != eResHeap || !Heap->m_Real ||
       Texture == ResourceId() || GetResourceManager()->HasResource(Texture) ||
       descriptor.storageMode != MTL::StorageModePrivate ||
       descriptor.textureType != MTL::TextureType2D ||
       (descriptor.pixelFormat != MTL::PixelFormatRGBA8Unorm &&
        descriptor.pixelFormat != MTL::PixelFormatBGRA8Unorm) ||
       !descriptor.width || !descriptor.height || descriptor.width > 8192 ||
       descriptor.height > 8192 || descriptor.depth != 1 ||
       descriptor.mipmapLevelCount != 1 || descriptor.arrayLength != 1 ||
       descriptor.sampleCount != 1 ||
       descriptor.resourceOptions != MTL::ResourceStorageModePrivate ||
       descriptor.cpuCacheMode != MTL::CPUCacheModeDefaultCache ||
       (uint64_t(descriptor.usage) & ~uint64_t(7)) != 0 ||
       descriptor.hazardTrackingMode != MTL::HazardTrackingModeDefault ||
       descriptor.swizzle.red != MTL::TextureSwizzleRed ||
       descriptor.swizzle.green != MTL::TextureSwizzleGreen ||
       descriptor.swizzle.blue != MTL::TextureSwizzleBlue ||
       descriptor.swizzle.alpha != MTL::TextureSwizzleAlpha)
    {
      RDCERR("Invalid or unsupported Metal heap texture identity or descriptor");
      return false;
    }
    if(descriptor.usage != MTL::TextureUsageUnknown)
      descriptor.usage = (MTL::TextureUsage)(descriptor.usage | MTL::TextureUsageShaderRead);
    MTL::TextureDescriptor *native(descriptor);
    MTL::Texture *real = Unwrap(Heap)->newTexture(native);
    native->release();
    if(!real)
    {
      RDCERR("Metal failed to recreate heap-backed texture");
      return false;
    }
    WrappedMTLTexture *wrapped = NULL;
    GetResourceManager()->WrapResource(Texture, real, wrapped, true);
    m_Device->AddResource(Texture, ResourceType::Texture, "Heap Texture");
    m_Device->GetReplay()->AddTexture(Texture, real, false);
    m_Device->DerivedResource(Heap, Texture);
  }
  return true;
}

WrappedMTLTexture *WrappedMTLHeap::newTexture(RDMTL::TextureDescriptor &descriptor)
{
  MTL::TextureDescriptor *native(descriptor);
  MTL::Texture *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newTexture(native));
  native->release();
  if(!real) return NULL;
  WrappedMTLTexture *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLHeap_newTexture);
    Serialise_newTexture(ser, wrapped, descriptor);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLHeap::Serialise_newTexture(ReadSerialiser &, WrappedMTLTexture *,
                                                    RDMTL::TextureDescriptor &);
template bool WrappedMTLHeap::Serialise_newTexture(WriteSerialiser &, WrappedMTLTexture *,
                                                    RDMTL::TextureDescriptor &);

template <typename SerialiserType>
bool WrappedMTLHeap::Serialise_newTextureWithOffset(SerialiserType &ser,
                                                     WrappedMTLTexture *texture,
                                                     RDMTL::TextureDescriptor &descriptor,
                                                     NS::UInteger offset)
{
  SERIALISE_ELEMENT_LOCAL(Heap, this).Important();
  SERIALISE_ELEMENT_LOCAL(Texture, GetResID(texture)).TypedAs("MTLTexture"_lit).Important();
  SERIALISE_ELEMENT(descriptor).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Heap || Heap->m_Type != eResHeap || !Heap->m_Real ||
       Unwrap(Heap)->type() != MTL::HeapTypePlacement ||
       Texture == ResourceId() || GetResourceManager()->HasResource(Texture) ||
       descriptor.storageMode != MTL::StorageModePrivate ||
       descriptor.textureType != MTL::TextureType2D ||
       (descriptor.pixelFormat != MTL::PixelFormatRGBA8Unorm &&
        descriptor.pixelFormat != MTL::PixelFormatBGRA8Unorm) ||
       !descriptor.width || !descriptor.height || descriptor.width > 8192 ||
       descriptor.height > 8192 || descriptor.depth != 1 ||
       descriptor.mipmapLevelCount != 1 || descriptor.arrayLength != 1 ||
       descriptor.sampleCount != 1 ||
       descriptor.resourceOptions != MTL::ResourceStorageModePrivate ||
       descriptor.cpuCacheMode != MTL::CPUCacheModeDefaultCache ||
       (uint64_t(descriptor.usage) & ~uint64_t(7)) != 0 ||
       descriptor.hazardTrackingMode != MTL::HazardTrackingModeDefault ||
       descriptor.swizzle.red != MTL::TextureSwizzleRed ||
       descriptor.swizzle.green != MTL::TextureSwizzleGreen ||
       descriptor.swizzle.blue != MTL::TextureSwizzleBlue ||
       descriptor.swizzle.alpha != MTL::TextureSwizzleAlpha)
    {
      RDCERR("Invalid or unsupported Metal placement heap texture identity or descriptor");
      return false;
    }
    if(descriptor.usage != MTL::TextureUsageUnknown)
      descriptor.usage = (MTL::TextureUsage)(descriptor.usage | MTL::TextureUsageShaderRead);
    MTL::TextureDescriptor *native(descriptor);
    const MTL::SizeAndAlign layout = Unwrap(m_Device)->heapTextureSizeAndAlign(native);
    const uint64_t heapSize = Unwrap(Heap)->size();
    if(layout.size == 0 || layout.align == 0 || offset % layout.align != 0 ||
       offset > heapSize || layout.size > heapSize - offset)
    {
      native->release();
      RDCERR("Invalid Metal placement heap texture alignment or range");
      return false;
    }
    const uint64_t end = uint64_t(offset) + layout.size;
    for(const PlacementRange &range : Heap->m_PlacementRanges)
      if(uint64_t(offset) < range.end && range.begin < end)
      {
        native->release();
        RDCERR("Overlapping Metal placement heap resources require alias lifetime replay");
        return false;
      }
    MTL::Texture *real = Unwrap(Heap)->newTexture(native, offset);
    native->release();
    if(!real || real->heapOffset() != offset)
    {
      if(real) real->release();
      RDCERR("Metal failed to recreate placement heap texture at captured offset");
      return false;
    }
    Heap->m_PlacementRanges.push_back({uint64_t(offset), end});
    WrappedMTLTexture *wrapped = NULL;
    GetResourceManager()->WrapResource(Texture, real, wrapped, true);
    m_Device->AddResource(Texture, ResourceType::Texture, "Placement Heap Texture");
    m_Device->GetReplay()->AddTexture(Texture, real, false);
    m_Device->DerivedResource(Heap, Texture);
  }
  return true;
}

WrappedMTLTexture *WrappedMTLHeap::newTextureWithOffset(RDMTL::TextureDescriptor &descriptor,
                                                        NS::UInteger offset)
{
  MTL::TextureDescriptor *native(descriptor);
  MTL::Texture *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newTexture(native, offset));
  native->release();
  if(!real) return NULL;
  WrappedMTLTexture *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLHeap_newTextureWithOffset);
    Serialise_newTextureWithOffset(ser, wrapped, descriptor, offset);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLHeap::Serialise_newTextureWithOffset(ReadSerialiser &,
    WrappedMTLTexture *, RDMTL::TextureDescriptor &, NS::UInteger);
template bool WrappedMTLHeap::Serialise_newTextureWithOffset(WriteSerialiser &,
    WrappedMTLTexture *, RDMTL::TextureDescriptor &, NS::UInteger);
