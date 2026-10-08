// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_heap.h"
#include "metal_buffer.h"
#include "metal_device.h"
#include "metal_manager.h"
#include "metal_replay.h"
#include "metal_texture.h"
#include "metal_acceleration_structure.h"

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
    // Placement heaps preserve captured offsets. Overlaps are validated at each child birth.
    // Sparse mappings and implicit texture aliases still require separate lifetime models.
    if(Heap == ResourceId() || GetResourceManager()->HasResource(Heap) ||
       !size || size > 576ULL * 1024 * 1024 ||
       (storageMode != MTL::StorageModePrivate &&
        !(storageMode == MTL::StorageModeShared && type == MTL::HeapTypePlacement)) ||
       cacheMode != MTL::CPUCacheModeDefaultCache ||
       hazardMode > MTL::HazardTrackingModeTracked ||
       (type != MTL::HeapTypeAutomatic && type != MTL::HeapTypePlacement))
    {
      RDCERR("Invalid or unsupported Metal heap descriptor or identity");
      return false;
    }
    MTL::HeapDescriptor *descriptor = MTL::HeapDescriptor::alloc()->init();
    descriptor->setSize(size);
    descriptor->setStorageMode(storageMode);
    descriptor->setCpuCacheMode(cacheMode);
    // Heap children inherit tracking from their heap, so strengthen it at the
    // allocation boundary as for standalone replay textures. Captured fences
    // and the placement/alias lifetime checks remain in force.
    descriptor->setHazardTrackingMode(MTL::HazardTrackingModeTracked);
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
bool WrappedMTLHeap::Serialise_newAccelerationStructure(SerialiserType &ser,
    WrappedMTLAccelerationStructure *structure, NS::UInteger size,
    NS::UInteger offset, bool placement)
{
  SERIALISE_ELEMENT_LOCAL(Heap, this).Important();
  SERIALISE_ELEMENT_LOCAL(Structure, GetResID(structure))
      .TypedAs("MTLAccelerationStructure"_lit).Important();
  SERIALISE_ELEMENT(size).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(placement).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    WrappedMTLObject *existing = Structure == ResourceId() ? NULL :
        GetResourceManager()->GetResource(Structure, true);
    const bool frameResource = m_Device->GetReplayEpoch() != 0;
    if(!Heap || Heap->m_Type != eResHeap || !Heap->m_Real ||
       Heap->m_Device != m_Device || Unwrap(Heap)->storageMode() != MTL::StorageModePrivate ||
       Unwrap(Heap)->type() != (placement ? MTL::HeapTypePlacement : MTL::HeapTypeAutomatic) ||
       Structure == ResourceId() || !size || size > 1024ULL*1024*1024 ||
       (!placement && offset) ||
       (existing && !(IsActiveReplaying(m_State) && frameResource &&
          m_Device->IsFramePlacementResource(Structure) && !existing->m_Real &&
          existing->m_Type == eResAccelerationStructure)))
    {
      RDCERR("Invalid Metal heap acceleration structure identity/type/size");
      return false;
    }
    const MTL::SizeAndAlign layout = Unwrap(m_Device)->heapAccelerationStructureSizeAndAlign(size);
    if(!layout.size || !layout.align || layout.size > Unwrap(Heap)->size() ||
       (placement && (offset % layout.align || offset > Unwrap(Heap)->size() ||
                     layout.size > Unwrap(Heap)->size()-offset)))
    {
      RDCERR("Invalid Metal heap acceleration structure placement range");
      return false;
    }
    if(placement && Heap->HasPlacementOverlap(offset, offset+layout.size))
    {
      RDCERR("Overlapping Metal heap AS allocations need separate alias lifetime proof");
      return false;
    }
    MTL::AccelerationStructure *real = placement ?
        Unwrap(Heap)->newAccelerationStructure(size, offset) :
        Unwrap(Heap)->newAccelerationStructure(size);
    if(!real || real->size() != size || real->heap() != Unwrap(Heap) ||
       (placement && real->heapOffset() != offset))
    {
      if(real) real->release();
      RDCERR("Metal failed to recreate captured heap acceleration structure");
      return false;
    }
    WrappedMTLAccelerationStructure *wrapped = (WrappedMTLAccelerationStructure *)existing;
    if(existing)
      GetResourceManager()->ReplaceRealResource(wrapped, real, true);
    else
    {
      GetResourceManager()->WrapResource(Structure, real, wrapped, true);
      m_Device->AddResource(Structure, ResourceType::AccelerationStructure,
                            "Heap Acceleration Structure");
      m_Device->DerivedResource(Heap, Structure);
    }
    wrapped->m_Size = size;
    wrapped->m_LastBuildKind = 0;
    wrapped->m_LastTriangleCount = 0;
    wrapped->m_LastBuildCommandBuffer = ResourceId();
    wrapped->m_LastCompactedSizeBuffer = ResourceId();
    wrapped->m_LastCompactedSizeOffset = 0;
    wrapped->m_LastCompactedSizeType = MTL::DataTypeNone;
    wrapped->m_LastCompactedWriteCommandBuffer = ResourceId();
    wrapped->m_LastInitialCompactedSize = 0;
    if(placement)
      Heap->m_PlacementRanges.push_back({uint64_t(offset), uint64_t(offset)+layout.size,
                                         Structure, frameResource});
    if(frameResource && IsLoading(m_State))
      m_Device->RegisterFramePlacementResource(Structure, Heap);
  }
  return true;
}

WrappedMTLAccelerationStructure *WrappedMTLHeap::newAccelerationStructure(
    NS::UInteger size, NS::UInteger offset, bool placement)
{
  SCOPED_READLOCK(m_Device->GetCaptureTransitionLock());
  SCOPED_LOCK(m_CaptureAllocationLock);
  MTL::AccelerationStructure *real = NULL;
  SERIALISE_TIME_CALL(real = placement ? Unwrap(this)->newAccelerationStructure(size, offset) :
                                        Unwrap(this)->newAccelerationStructure(size));
  if(!real) return NULL;
  WrappedMTLAccelerationStructure *wrapped = NULL;
  const ResourceId id = GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->m_Size = size;
  if(IsCaptureMode(m_State))
  {
    if(Unwrap(this)->type() == MTL::HeapTypePlacement)
      RecordCaptureAllocation(real->heapOffset(), Unwrap(m_Device)->heapAccelerationStructureSizeAndAlign(size).size);
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLHeap_newAccelerationStructure);
    Serialise_newAccelerationStructure(ser, wrapped, size, offset, placement);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    Chunk *creation = scope.Get();
    record->AddChunk(creation);
    if(IsActiveCapturing(m_State))
    {
      m_Device->AddFrameCaptureRecordChunk(creation->Duplicate());
      m_Device->RegisterCapturedFrameResource(id);
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(this), eFrameRef_Read);
    }
  }
  return wrapped;
}

template bool WrappedMTLHeap::Serialise_newAccelerationStructure(ReadSerialiser &,
    WrappedMTLAccelerationStructure *, NS::UInteger, NS::UInteger, bool);
template bool WrappedMTLHeap::Serialise_newAccelerationStructure(WriteSerialiser &,
    WrappedMTLAccelerationStructure *, NS::UInteger, NS::UInteger, bool);

bool WrappedMTLHeap::RecordCaptureAllocation(uint64_t offset, uint64_t size)
{
  if(!size || offset > Unwrap(this)->size() || size > Unwrap(this)->size() - offset)
  {
    m_CaptureAllocationHistoryComplete = false;
    return false;
  }
  uint64_t begin = offset, end = offset + size;
  bool fresh = m_CaptureAllocationHistoryComplete;
  auto it = m_CaptureAllocatedRanges.lower_bound(begin);
  if(it != m_CaptureAllocatedRanges.begin())
  {
    auto previous = it; --previous;
    if(previous->second >= begin) it = previous;
  }
  while(it != m_CaptureAllocatedRanges.end() && it->first <= end)
  {
    fresh &= !(offset < it->second && it->first < offset + size);
    begin = RDCMIN(begin, it->first); end = RDCMAX(end, it->second);
    it = m_CaptureAllocatedRanges.erase(it);
  }
  m_CaptureAllocatedRanges[begin] = end;
  return fresh;
}

bool WrappedMTLHeap::CanImplicitlyAliasBuffers(uint64_t begin, uint64_t end, ResourceId after)
{
  if(!m_Real || (Unwrap(this)->storageMode() != MTL::StorageModeShared &&
      !(m_Device->SupportsPrivateDescriptorSources() && Unwrap(this)->storageMode() == MTL::StorageModePrivate)) ||
     Unwrap(this)->hazardTrackingMode() != MTL::HazardTrackingModeTracked) return false;
  for(const PlacementRange &range : m_PlacementRanges)
    if(begin < range.end && range.begin < end)
    {
      WrappedMTLObject *old = GetResourceManager()->GetResource(range.resource, true);
      if(!old || !old->m_Real) return false;
      const bool buffer = old->m_Type == eResBuffer &&
          Unwrap((WrappedMTLBuffer *)old)->length() <= m_Device->DescriptorPlacementBufferAliasLimit() &&
          Unwrap((WrappedMTLBuffer *)old)->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
          m_Device->CanReplayImplicitBufferAlias(range.resource, after);
      const bool texture = old->m_Type == eResTexture &&
          Unwrap((WrappedMTLTexture *)old)->storageMode() == MTL::StorageModePrivate &&
          Unwrap((WrappedMTLTexture *)old)->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
          range.end - range.begin <= m_Device->DescriptorPlacementAliasLimit() &&
          m_Device->CanReplayRetiredTextureAlias(range.resource, after);
      if(!buffer && !texture) return false;
    }
  return true;
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
  SCOPED_READLOCK(m_Device->GetCaptureTransitionLock());
  SCOPED_LOCK(m_CaptureAllocationLock);
  MTL::Buffer *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newBuffer(length, options));
  if(!real) return NULL;
  WrappedMTLBuffer *wrapped = NULL;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    if(Unwrap(this)->type() == MTL::HeapTypePlacement)
      RecordCaptureAllocation(real->heapOffset(), Unwrap(m_Device)->heapBufferSizeAndAlign(length, options).size);
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
       (Unwrap(Heap)->storageMode() != ((options & MTL::ResourceStorageModePrivate) ?
          MTL::StorageModePrivate : MTL::StorageModeShared)) ||
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
        // isAliasable does not prove logical retirement. Explicit retirement invalidates
        // old descriptor sources; bounded implicit placement sharing keeps both objects live.
        const bool aliasable = m_Device->IsReplayResourceAliasable(range.resource);
        WrappedMTLObject *previous = GetResourceManager()->GetResource(range.resource, true);
        const bool implicit = previous && previous->m_Type == eResBuffer &&
            previous->m_Real && Unwrap((WrappedMTLBuffer *)previous)->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
            (Unwrap(Heap)->storageMode() == MTL::StorageModeShared ||
             (m_Device->SupportsPrivateDescriptorSources() && Unwrap(Heap)->storageMode() == MTL::StorageModePrivate)) &&
            Unwrap(Heap)->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
            length <= m_Device->DescriptorPlacementBufferAliasLimit() &&
            Unwrap((WrappedMTLBuffer *)previous)->length() <= m_Device->DescriptorPlacementBufferAliasLimit() &&
            m_Device->CanReplayImplicitBufferAlias(range.resource, Buffer);
        // A validated frame AS packet owns independent staging. A later exact
        // tracked alias cannot change this build input; both native objects stay
        // live and the heap orders later GPU writes across their shared range.
        const bool frozenASInput = previous && previous->m_Type == eResBuffer && previous->m_Real &&
            uint64_t(offset) == range.begin && end == range.end &&
            Unwrap(Heap)->storageMode() == MTL::StorageModePrivate &&
            Unwrap(Heap)->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
            Unwrap((WrappedMTLBuffer *)previous)->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
            length <= 64*1024*1024 && Unwrap((WrappedMTLBuffer *)previous)->length() <= 64*1024*1024 &&
            m_Device->CanReplayFrozenASInputAlias(range.resource, Buffer);
        const bool retiredTexture = previous && previous->m_Type == eResTexture && previous->m_Real &&
            Unwrap(Heap)->storageMode() == MTL::StorageModePrivate &&
            Unwrap(Heap)->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
            Unwrap((WrappedMTLTexture *)previous)->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
            length <= m_Device->DescriptorPlacementBufferAliasLimit() &&
            range.end - range.begin <= m_Device->DescriptorPlacementAliasLimit() &&
            m_Device->CanReplayRetiredTextureAlias(range.resource, Buffer);
        if(!aliasable && !implicit && !retiredTexture && !frozenASInput)
        {
          RDCERR("Overlapping Metal placement heap resource lacks supported aliasing/completion");
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
  SCOPED_READLOCK(m_Device->GetCaptureTransitionLock());
  SCOPED_LOCK(m_CaptureAllocationLock);
  MTL::Buffer *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newBuffer(length, options, offset));
  if(!real) return NULL;
  WrappedMTLBuffer *wrapped = NULL;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    const auto layout = Unwrap(m_Device)->heapBufferSizeAndAlign(length, options);
    const bool fresh = RecordCaptureAllocation(real->heapOffset(), layout.size);
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
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(this), eFrameRef_Read);
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
    m_Device->CaptureHeapBufferBirth(wrapped,this,fresh);
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
       (descriptor.textureType != MTL::TextureType2D &&
        descriptor.textureType != MTL::TextureType2DMultisample) ||
       (descriptor.pixelFormat != MTL::PixelFormatRGBA8Unorm &&
        descriptor.pixelFormat != MTL::PixelFormatBGRA8Unorm) ||
       !descriptor.width || !descriptor.height || descriptor.width > 8192 ||
       descriptor.height > 8192 || descriptor.depth != 1 ||
       descriptor.mipmapLevelCount != 1 || descriptor.arrayLength != 1 ||
       (descriptor.textureType == MTL::TextureType2D ? descriptor.sampleCount != 1 :
          (descriptor.sampleCount != 2 && descriptor.sampleCount != 4 && descriptor.sampleCount != 8)) ||
       !Unwrap(m_Device)->supportsTextureSampleCount(descriptor.sampleCount) ||
       (descriptor.resourceOptions != MTL::ResourceStorageModePrivate &&
        descriptor.resourceOptions != (MTL::ResourceStorageModePrivate | MTL::ResourceHazardTrackingModeUntracked) &&
        descriptor.resourceOptions != (MTL::ResourceStorageModePrivate | MTL::ResourceHazardTrackingModeTracked)) ||
       descriptor.cpuCacheMode != MTL::CPUCacheModeDefaultCache ||
       (uint64_t(descriptor.usage) & ~uint64_t(7)) != 0 ||
       descriptor.hazardTrackingMode > MTL::HazardTrackingModeTracked ||
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
    native->setHazardTrackingMode(MTL::HazardTrackingModeTracked);
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
  SCOPED_READLOCK(m_Device->GetCaptureTransitionLock());
  SCOPED_LOCK(m_CaptureAllocationLock);
  MTL::TextureDescriptor *native(descriptor);
  MTL::Texture *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newTexture(native));
  native->release();
  if(!real) return NULL;
  WrappedMTLTexture *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    if(Unwrap(this)->type() == MTL::HeapTypePlacement)
    {
      MTL::TextureDescriptor *historyDescriptor(descriptor);
      RecordCaptureAllocation(real->heapOffset(), Unwrap(m_Device)->heapTextureSizeAndAlign(historyDescriptor).size);
      historyDescriptor->release();
    }
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLHeap_newTexture);
    Serialise_newTexture(ser, wrapped, descriptor);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
    if(descriptor.storageMode == MTL::StorageModePrivate ||
       descriptor.storageMode == MTL::StorageModeShared)
      GetResourceManager()->MarkDirtyResource(GetResID(wrapped));
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
  uint32_t blockWidth = 0, blockHeight = 0, blockBytes = 0;
  if(GetTextureDataBlockShape(format, blockWidth, blockHeight, blockBytes)) return true;
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
  if(descriptor.sampleCount == 1)
  {
    uint64_t logicalBytes = 0;
    return MetalTextureReplayLayout(descriptor, logicalBytes);
  }

  if(descriptor.textureType != MTL::TextureType2D &&
     descriptor.textureType != MTL::TextureType2DArray &&
     descriptor.textureType != MTL::TextureTypeCube &&
     descriptor.textureType != MTL::TextureTypeCubeArray &&
     descriptor.textureType != MTL::TextureType3D &&
     descriptor.textureType != MTL::TextureType2DMultisample &&
     descriptor.textureType != MTL::TextureType2DMultisampleArray)
    return false;
  if(!descriptor.width || !descriptor.height || !descriptor.depth ||
     descriptor.width > 16384 || descriptor.height > 16384 || descriptor.depth > 256 ||
     !descriptor.arrayLength || descriptor.arrayLength > 16 ||
     !descriptor.mipmapLevelCount)
    return false;
  const bool msaa = descriptor.textureType == MTL::TextureType2DMultisample ||
                    descriptor.textureType == MTL::TextureType2DMultisampleArray;
  if(msaa ? ((descriptor.sampleCount != 2 && descriptor.sampleCount != 4 && descriptor.sampleCount != 8) ||
              descriptor.mipmapLevelCount != 1) : descriptor.sampleCount != 1)
    return false;
  if(descriptor.textureType == MTL::TextureType3D)
  {
    if(descriptor.arrayLength != 1) return false;
  }
  else if(descriptor.depth != 1)
    return false;
  if(descriptor.textureType != MTL::TextureType2DArray &&
     descriptor.textureType != MTL::TextureType2DMultisampleArray &&
     descriptor.textureType != MTL::TextureTypeCubeArray && descriptor.arrayLength != 1)
    return false;
  if((descriptor.textureType == MTL::TextureTypeCube || descriptor.textureType == MTL::TextureTypeCubeArray) &&
     descriptor.width != descriptor.height)
    return false;
  return ValidTextureMipCount(descriptor.width, descriptor.height, descriptor.depth,
                               descriptor.mipmapLevelCount);
}

static bool PlacementTextureBlockFormat(MTL::PixelFormat format)
{
  uint32_t width = 0, height = 0, bytes = 0;
  return GetTextureDataBlockShape(format, width, height, bytes) && (width > 1 || height > 1);
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
       Texture == ResourceId() ||
       (GetResourceManager()->HasResource(Texture) &&
        !(IsActiveReplaying(m_State) && m_Device->IsFramePlacementResource(Texture) &&
          !GetResourceManager()->GetResource(Texture)->m_Real)) ||
       (descriptor.storageMode != MTL::StorageModePrivate && descriptor.storageMode != MTL::StorageModeShared) ||
       descriptor.storageMode != Unwrap(Heap)->storageMode() ||
       !ValidPlacementTextureShape(descriptor) ||
       !Unwrap(m_Device)->supportsTextureSampleCount(descriptor.sampleCount) ||
       !ValidPlacementTextureFormat(descriptor.pixelFormat) ||
       (PlacementTextureBlockFormat(descriptor.pixelFormat) &&
        !ser.VersionAtLeast(0x10)) ||
       (descriptor.resourceOptions != (MTL::ResourceOptions)(uint64_t(descriptor.storageMode) << 4) &&
        descriptor.resourceOptions != ((MTL::ResourceOptions)(uint64_t(descriptor.storageMode) << 4) |
                                       MTL::ResourceHazardTrackingModeTracked) &&
        descriptor.resourceOptions != ((MTL::ResourceOptions)(uint64_t(descriptor.storageMode) << 4) |
                                       MTL::ResourceHazardTrackingModeUntracked)) ||
       descriptor.cpuCacheMode != MTL::CPUCacheModeDefaultCache ||
       (uint64_t(descriptor.usage) & ~uint64_t(55)) != 0 ||
       descriptor.hazardTrackingMode > MTL::HazardTrackingModeTracked ||
       descriptor.swizzle.red > MTL::TextureSwizzleAlpha ||
       descriptor.swizzle.green > MTL::TextureSwizzleAlpha ||
       descriptor.swizzle.blue > MTL::TextureSwizzleAlpha ||
       descriptor.swizzle.alpha > MTL::TextureSwizzleAlpha)
    {
      RDCERR("Invalid or unsupported Metal placement heap texture identity or descriptor");
      fprintf(stderr, "Metal placement texture checks: shape=%d format=%d heapStorage=%llu\n",
              ValidPlacementTextureShape(descriptor) ? 1 : 0,
              ValidPlacementTextureFormat(descriptor.pixelFormat) ? 1 : 0,
              Heap && Heap->m_Real ? (uint64_t)Unwrap(Heap)->storageMode() : 0);
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
    native->setHazardTrackingMode(MTL::HazardTrackingModeTracked);
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
        if(ser.VersionAtLeast(0xF) && m_Device->GetReplayEpoch() != 0 &&
           m_Device->SupportsTrackedAliasCreationWhileEncoding() &&
           Heap->CanImplicitlyAliasBuffers(offset, end, Texture)) continue;
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
    const bool frameResource = m_Device->GetReplayEpoch() != 0;
    if(getenv("RENDERDOC_METAL_TRACE_REPLAY_WAITS"))
      fprintf(stderr, "Metal frame texture birth: id=%s epoch=%llu loading=%d active=%d\n",
              ToStr(Texture).c_str(), m_Device->GetReplayEpoch(), IsLoading(m_State), IsActiveReplaying(m_State));
    Heap->m_PlacementRanges.push_back({uint64_t(offset), end, Texture, frameResource});
    if(frameResource && IsLoading(m_State))
      m_Device->RegisterFramePlacementResource(Texture, Heap);
    if(IsActiveReplaying(m_State) && frameResource)
      GetResourceManager()->ReplaceRealResource(GetResourceManager()->GetResource(Texture), real, true);
    else
    {
      WrappedMTLTexture *wrapped = NULL;
      GetResourceManager()->WrapResource(Texture, real, wrapped, true);
      m_Device->AddResource(Texture, ResourceType::Texture, "Placement Heap Texture");
      m_Device->GetReplay()->AddTexture(Texture, real, false);
      m_Device->DerivedResource(Heap, Texture);
    }
  }
  return true;
}

WrappedMTLTexture *WrappedMTLHeap::newTextureWithOffset(RDMTL::TextureDescriptor &descriptor,
                                                        NS::UInteger offset)
{
  SCOPED_READLOCK(m_Device->GetCaptureTransitionLock());
  SCOPED_LOCK(m_CaptureAllocationLock);
  MTL::TextureDescriptor *native(descriptor);
  MTL::Texture *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newTexture(native, offset));
  native->release();
  if(!real) return NULL;
  WrappedMTLTexture *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    MTL::TextureDescriptor *historyDescriptor(descriptor);
    const auto layout = Unwrap(m_Device)->heapTextureSizeAndAlign(historyDescriptor);
    historyDescriptor->release();
    RecordCaptureAllocation(real->heapOffset(), layout.size);
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLHeap_newTextureWithOffset);
    Serialise_newTextureWithOffset(ser, wrapped, descriptor, offset);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    Chunk *creation = scope.Get();
    record->AddChunk(creation);
    if(IsActiveCapturing(m_State))
    {
      m_Device->AddFrameCaptureRecordChunk(creation->Duplicate());
      m_Device->RegisterCapturedFrameResource(GetResID(wrapped));
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(this), eFrameRef_Read);
    }
    if(descriptor.storageMode == MTL::StorageModePrivate ||
       descriptor.storageMode == MTL::StorageModeShared)
      GetResourceManager()->MarkDirtyResource(GetResID(wrapped));
  }
  return wrapped;
}

template bool WrappedMTLHeap::Serialise_newTextureWithOffset(ReadSerialiser &,
    WrappedMTLTexture *, RDMTL::TextureDescriptor &, NS::UInteger);
template bool WrappedMTLHeap::Serialise_newTextureWithOffset(WriteSerialiser &,
    WrappedMTLTexture *, RDMTL::TextureDescriptor &, NS::UInteger);
