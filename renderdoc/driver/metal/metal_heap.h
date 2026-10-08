// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#pragma once

#include "metal_common.h"

void MetalAssociateHeapProxy(MTL::Heap *real, WrappedMTLHeap *wrapped);

class WrappedMTLHeap : public WrappedMTLObject
{
public:
  WrappedMTLHeap(MTL::Heap *real, ResourceId id, WrappedMTLDevice *device);

  WrappedMTLBuffer *newBuffer(NS::UInteger length, MTL::ResourceOptions options);
  WrappedMTLAccelerationStructure *newAccelerationStructure(NS::UInteger size,
                                                            NS::UInteger offset, bool placement);
  template <typename SerialiserType>
  bool Serialise_newAccelerationStructure(SerialiserType &ser,
      WrappedMTLAccelerationStructure *structure, NS::UInteger size,
      NS::UInteger offset, bool placement);
  WrappedMTLBuffer *newBufferWithOffset(NS::UInteger length, MTL::ResourceOptions options,
                                        NS::UInteger offset);
  WrappedMTLTexture *newTexture(RDMTL::TextureDescriptor &descriptor);
  WrappedMTLTexture *newTextureWithOffset(RDMTL::TextureDescriptor &descriptor,
                                          NS::UInteger offset);
  template <typename SerialiserType>
  bool Serialise_newBuffer(SerialiserType &ser, WrappedMTLBuffer *buffer,
                           NS::UInteger length, MTL::ResourceOptions options);
  template <typename SerialiserType>
  bool Serialise_newBufferWithOffset(SerialiserType &ser, WrappedMTLBuffer *buffer,
                                     NS::UInteger length, MTL::ResourceOptions options,
                                     NS::UInteger offset);
  template <typename SerialiserType>
  bool Serialise_newTexture(SerialiserType &ser, WrappedMTLTexture *texture,
                            RDMTL::TextureDescriptor &descriptor);
  template <typename SerialiserType>
  bool Serialise_newTextureWithOffset(SerialiserType &ser, WrappedMTLTexture *texture,
                                      RDMTL::TextureDescriptor &descriptor,
                                      NS::UInteger offset);

  enum { TypeEnum = eResHeap };
  void ResetFramePlacementRanges();
  bool RecordCaptureAllocation(uint64_t offset, uint64_t size);
  bool CanImplicitlyAliasBuffers(uint64_t begin, uint64_t end, ResourceId after);
  bool HasOtherPlacementOverlap(uint64_t begin, uint64_t end, ResourceId after) const
  {
    for(const PlacementRange &range : m_PlacementRanges)
      if(range.resource != after && begin < range.end && range.begin < end)
        return true;
    return false;
  }
  bool HasPlacementOverlap(uint64_t begin, uint64_t end) const
  {
    for(const PlacementRange &range : m_PlacementRanges)
      if(begin < range.end && range.begin < end)
        return true;
    return false;
  }

private:
  struct PlacementRange
  {
    uint64_t begin, end;
    ResourceId resource;
    bool frameResource;
  };
  rdcarray<PlacementRange> m_PlacementRanges;
  // Allocation history belongs to the physical heap lifetime. Retiring an
  // object must not turn previously owned backing into a fresh allocation.
  Threading::CriticalSection m_CaptureAllocationLock;
  std::map<uint64_t, uint64_t> m_CaptureAllocatedRanges;
  bool m_CaptureAllocationHistoryComplete = true;
};
