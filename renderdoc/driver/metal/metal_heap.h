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

private:
  struct PlacementRange
  {
    uint64_t begin, end;
    ResourceId resource;
    bool frameResource;
  };
  rdcarray<PlacementRange> m_PlacementRanges;
};
