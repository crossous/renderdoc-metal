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
       size < 4096 || size > 576ULL * 1024 * 1024 ||
       (storageMode != MTL::StorageModePrivate &&
        !(storageMode == MTL::StorageModeShared && type == MTL::HeapTypePlacement)) ||
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

void WrappedMTLHeap::ResetFramePlacementRanges()
{
  for(size_t i = m_PlacementRanges.size(); i > 0; --i)
    if(m_PlacementRanges[i - 1].frameResource)
      m_PlacementRanges.erase(i - 1);
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
       Buffer == ResourceId() ||
       (GetResourceManager()->HasResource(Buffer) &&
        !(IsActiveReplaying(m_State) && m_Device->IsFramePlacementResource(Buffer) &&
          !GetResourceManager()->GetResource(Buffer)->m_Real)) ||
       length == 0 || length > 128ULL * 1024 * 1024 ||
       (options != MTL::ResourceStorageModePrivate &&
        options != (MTL::ResourceStorageModePrivate | MTL::ResourceHazardTrackingModeTracked) &&
        !(Unwrap(Heap)->storageMode() == MTL::StorageModeShared &&
          (options == MTL::ResourceStorageModeShared ||
           options == (MTL::ResourceStorageModeShared | MTL::ResourceHazardTrackingModeTracked)))))
    {
      RDCERR("Invalid or unsupported Metal placement heap buffer identity or options");
      fprintf(stderr, "Metal placement buffer rejected: heap=%d type=%llu buffer=%s exists=%d frame=%d length=%llu options=%llu\n",
              Heap && Heap->m_Type == eResHeap && Heap->m_Real ? 1 : 0,
              Heap && Heap->m_Type == eResHeap && Heap->m_Real ? (uint64_t)Unwrap(Heap)->type() : 0,
              ToStr(Buffer).c_str(), GetResourceManager()->HasResource(Buffer) ? 1 : 0,
              m_Device->IsFramePlacementResource(Buffer) ? 1 : 0,
              (uint64_t)length, (uint64_t)options);
      return false;
    }
    const MTL::SizeAndAlign layout =
        Unwrap(m_Device)->heapBufferSizeAndAlign(length, options);
    const uint64_t heapSize = Unwrap(Heap)->size();
    if(layout.size == 0 || layout.align == 0 || offset % layout.align != 0 ||
       offset > heapSize || layout.size > heapSize - offset)
    {
      RDCERR("Invalid Metal placement heap buffer alignment or range");
      fprintf(stderr, "Metal placement buffer range: offset=%llu size=%llu align=%llu heap=%llu\n",
              (uint64_t)offset, (uint64_t)layout.size, (uint64_t)layout.align, heapSize);
      return false;
    }
    const uint64_t end = uint64_t(offset) + layout.size;
    for(const PlacementRange &range : Heap->m_PlacementRanges)
      if(uint64_t(offset) < range.end && range.begin < end)
      {
        // Older captures moved frame-created resources into the initial resource list.
        // They cannot prove which contents occupied an aliased range at frame start.
        if(!ser.VersionAtLeast(0xF) || m_Device->GetReplayEpoch() == 0)
        {
          RDCERR("Overlapping placement resource in a capture without frame creation order");
          return false;
        }
        WrappedMTLObject *existing = GetResourceManager()->GetResource(range.resource, true);
        bool aliasable = false;
        if(existing && existing->m_Real)
        {
          if(existing->m_Type == eResBuffer)
            aliasable = Unwrap((WrappedMTLBuffer *)existing)->isAliasable();
          else if(existing->m_Type == eResTexture)
            aliasable = Unwrap((WrappedMTLTexture *)existing)->isAliasable();
        }
        if(!aliasable)
        {
          RDCERR("Overlapping Metal placement heap resource is not aliasable");
          fprintf(stderr, "Metal placement buffer overlap: offset=%llu end=%llu existing=%llu..%llu resource=%s\n",
                  (uint64_t)offset, end, range.begin, range.end,
                  ToStr(range.resource).c_str());
          return false;
        }
      }
    MTL::Buffer *real = Unwrap(Heap)->newBuffer(length, options, offset);
    if(!real || real->heapOffset() != offset)
    {
      if(real) real->release();
      RDCERR("Metal failed to recreate placement heap buffer at captured offset");
      fprintf(stderr, "Metal placement buffer native creation failed: offset=%llu length=%llu\n",
              (uint64_t)offset, (uint64_t)length);
      return false;
    }
    const bool frameResource = m_Device->GetReplayEpoch() != 0;
    Heap->m_PlacementRanges.push_back({uint64_t(offset), end, Buffer, frameResource});
    if(frameResource && IsLoading(m_State))
      m_Device->RegisterFramePlacementResource(Buffer, Heap);
    if(IsActiveReplaying(m_State) && frameResource)
      GetResourceManager()->ReplaceRealResource(GetResourceManager()->GetResource(Buffer), real, true);
    else
    {
      WrappedMTLBuffer *wrapped = NULL;
      GetResourceManager()->WrapResource(Buffer, real, wrapped, true);
      m_Device->AddResource(Buffer, ResourceType::Buffer, "Placement Heap Buffer");
      m_Device->GetReplay()->AddBuffer(Buffer, length);
      m_Device->DerivedResource(Heap, Buffer);
    }
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
    Chunk *creation = scope.Get();
    record->AddChunk(creation);
    if(IsActiveCapturing(m_State))
    {
      m_Device->AddFrameCaptureRecordChunk(creation->Duplicate());
      m_Device->RegisterCapturedFrameResource(id);
    }
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

// UE's placement heaps use 2D, array, cube and volume textures. Keep this list limited to
// formats observed in the captured frame and checked with native Metal size/alignment probing.
static bool ValidPlacementTextureFormat(MTL::PixelFormat format)
{
  switch(format)
  {
    case MTL::PixelFormatR8Unorm:
    case MTL::PixelFormatR8Unorm_sRGB:
    case MTL::PixelFormatR8Uint:
    case MTL::PixelFormatR16Unorm:
    case MTL::PixelFormatR16Uint:
    case MTL::PixelFormatR16Float:
    case MTL::PixelFormatRG8Unorm:
    case MTL::PixelFormatRG8Uint:
    case MTL::PixelFormatR32Uint:
    case MTL::PixelFormatR32Float:
    case MTL::PixelFormatRG16Unorm:
    case MTL::PixelFormatRG16Uint:
    case MTL::PixelFormatRG16Float:
    case MTL::PixelFormatRGBA8Unorm:
    case MTL::PixelFormatBGRA8Unorm:
    case MTL::PixelFormatBGRA8Unorm_sRGB:
    case MTL::PixelFormatRGB10A2Unorm:
    case MTL::PixelFormatRG11B10Float:
    case MTL::PixelFormatRG32Uint:
    case MTL::PixelFormatRG32Float:
    case MTL::PixelFormatRGBA16Unorm:
    case MTL::PixelFormatRGBA16Float:
    case MTL::PixelFormatRGBA32Float:
    case MTL::PixelFormatBC1_RGBA:
    case MTL::PixelFormatBC1_RGBA_sRGB:
    case MTL::PixelFormatBC5_RGUnorm:
    case MTL::PixelFormatDepth32Float_Stencil8: return true;
    default: return false;
  }
}

static bool ValidPlacementTextureShape(const RDMTL::TextureDescriptor &descriptor)
{
  if(descriptor.textureType != MTL::TextureType2D &&
     descriptor.textureType != MTL::TextureType2DArray &&
     descriptor.textureType != MTL::TextureTypeCube &&
     descriptor.textureType != MTL::TextureType3D)
    return false;
  if(!descriptor.width || !descriptor.height || !descriptor.depth ||
     descriptor.width > 16384 || descriptor.height > 16384 || descriptor.depth > 256 ||
     !descriptor.arrayLength || descriptor.arrayLength > 16 ||
     descriptor.sampleCount != 1 || !descriptor.mipmapLevelCount)
    return false;
  if(descriptor.textureType == MTL::TextureType3D)
  {
    if(descriptor.arrayLength != 1) return false;
  }
  else if(descriptor.depth != 1)
    return false;
  if(descriptor.textureType != MTL::TextureType2DArray && descriptor.arrayLength != 1)
    return false;
  if(descriptor.textureType == MTL::TextureTypeCube && descriptor.width != descriptor.height)
    return false;
  uint64_t largest = RDCMAX(uint64_t(descriptor.width), uint64_t(descriptor.height));
  if(descriptor.textureType == MTL::TextureType3D)
    largest = RDCMAX(largest, uint64_t(descriptor.depth));
  uint32_t maxMips = 0;
  do
  {
    maxMips++;
    largest >>= 1;
  } while(largest);
  return descriptor.mipmapLevelCount <= maxMips;
}

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
       !ValidPlacementTextureShape(descriptor) ||
       !ValidPlacementTextureFormat(descriptor.pixelFormat) ||
       ((descriptor.pixelFormat == MTL::PixelFormatBC1_RGBA ||
         descriptor.pixelFormat == MTL::PixelFormatBC1_RGBA_sRGB ||
         descriptor.pixelFormat == MTL::PixelFormatBC5_RGUnorm) &&
        !ser.VersionAtLeast(0x10)) ||
       (descriptor.resourceOptions != MTL::ResourceStorageModePrivate &&
        descriptor.resourceOptions != (MTL::ResourceStorageModePrivate |
                                       MTL::ResourceHazardTrackingModeTracked)) ||
       descriptor.cpuCacheMode != MTL::CPUCacheModeDefaultCache ||
       (uint64_t(descriptor.usage) & ~uint64_t(55)) != 0 ||
       (descriptor.hazardTrackingMode != MTL::HazardTrackingModeDefault &&
        descriptor.hazardTrackingMode != MTL::HazardTrackingModeTracked) ||
       !descriptor.allowGPUOptimizedContents ||
       descriptor.swizzle.red != MTL::TextureSwizzleRed ||
       descriptor.swizzle.green != MTL::TextureSwizzleGreen ||
       descriptor.swizzle.blue != MTL::TextureSwizzleBlue ||
       descriptor.swizzle.alpha != MTL::TextureSwizzleAlpha)
    {
      RDCERR("Invalid or unsupported Metal placement heap texture identity or descriptor");
      fprintf(stderr,
              "Metal placement texture descriptor rejected: heap=%d heapType=%llu textureNull=%d exists=%d "
              "storage=%llu type=%llu format=%llu size=%llux%llux%llu mips=%llu array=%llu samples=%llu "
              "options=%llu cache=%llu usage=%llu hazard=%llu swizzle=%llu,%llu,%llu,%llu\n",
              Heap && Heap->m_Type == eResHeap && Heap->m_Real ? 1 : 0,
              Heap && Heap->m_Type == eResHeap && Heap->m_Real ? (uint64_t)Unwrap(Heap)->type() : 0,
              Texture == ResourceId() ? 1 : 0, GetResourceManager()->HasResource(Texture) ? 1 : 0,
              (uint64_t)descriptor.storageMode, (uint64_t)descriptor.textureType,
              (uint64_t)descriptor.pixelFormat, (uint64_t)descriptor.width,
              (uint64_t)descriptor.height, (uint64_t)descriptor.depth,
              (uint64_t)descriptor.mipmapLevelCount, (uint64_t)descriptor.arrayLength,
              (uint64_t)descriptor.sampleCount, (uint64_t)descriptor.resourceOptions,
              (uint64_t)descriptor.cpuCacheMode, (uint64_t)descriptor.usage,
              (uint64_t)descriptor.hazardTrackingMode, (uint64_t)descriptor.swizzle.red,
              (uint64_t)descriptor.swizzle.green, (uint64_t)descriptor.swizzle.blue,
              (uint64_t)descriptor.swizzle.alpha);
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
      fprintf(stderr, "Metal placement texture range rejected: offset=%llu align=%llu layout=%llu heap=%llu\n",
              (uint64_t)offset, (uint64_t)layout.align, (uint64_t)layout.size, heapSize);
      return false;
    }
    const uint64_t end = uint64_t(offset) + layout.size;
    for(const PlacementRange &range : Heap->m_PlacementRanges)
      if(uint64_t(offset) < range.end && range.begin < end)
      {
        native->release();
        RDCERR("Overlapping Metal placement heap texture requires separate lifetime validation");
        fprintf(stderr, "Metal placement texture overlap: offset=%llu end=%llu existing=%llu..%llu\n",
                (uint64_t)offset, end, range.begin, range.end);
        return false;
      }
    MTL::Texture *real = Unwrap(Heap)->newTexture(native, offset);
    native->release();
    if(!real || real->heapOffset() != offset)
    {
      if(real) real->release();
      RDCERR("Metal failed to recreate placement heap texture at captured offset");
      fprintf(stderr, "Metal placement texture native creation failed: offset=%llu format=%llu\n",
              (uint64_t)offset, (uint64_t)descriptor.pixelFormat);
      return false;
    }
    Heap->m_PlacementRanges.push_back({uint64_t(offset), end, Texture, false});
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
    if(descriptor.pixelFormat == MTL::PixelFormatBC1_RGBA ||
       descriptor.pixelFormat == MTL::PixelFormatBC1_RGBA_sRGB ||
       descriptor.pixelFormat == MTL::PixelFormatBC5_RGUnorm)
      GetResourceManager()->MarkDirtyResource(GetResID(wrapped));
  }
  return wrapped;
}

template bool WrappedMTLHeap::Serialise_newTextureWithOffset(ReadSerialiser &,
    WrappedMTLTexture *, RDMTL::TextureDescriptor &, NS::UInteger);
template bool WrappedMTLHeap::Serialise_newTextureWithOffset(WriteSerialiser &,
    WrappedMTLTexture *, RDMTL::TextureDescriptor &, NS::UInteger);
