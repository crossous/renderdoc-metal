// SPDX-License-Identifier: MIT
#pragma once
#include "metal_indirect_readback.h"

// Native footprints are queried from live objects, never captured GPU addresses.
// Texture views and buffer-backed textures resolve to their allocation owner.
struct MetalIndirectWriteFootprint
{
  NS::SharedPtr<MTL::Resource> owner;
  NS::SharedPtr<MTL::Heap> heap;
  uint64_t begin=0, end=0;
  bool buffer=false, valid=false;
};
MetalIndirectWriteFootprint GetMetalIndirectWriteFootprint(MTL::Resource *resource, bool texture);
bool MetalIndirectSourceDisjoint(MTL::Buffer *source,
    const rdcarray<MetalIndirectWriteFootprint> &writes);

// Call after the Native render pass (parallel parent included) has ended.
// The caller must separately prove that its declarations include all shader writes.
// Explicit retirement/alias history is also the caller's responsibility; placement
// resources can report isAliasable immediately after creation.
bool CopyMetalRenderIndirectArguments(MTL::Device *device, MTL::CommandBuffer *command,
    MTL::Buffer *source, uint64_t offset, uint32_t wordCount,
    const rdcarray<MetalIndirectWriteFootprint> &writes, MetalIndirectReadback &result);
