// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#pragma once

#include "metal_replay.h"

// Residency declarations and barriers share the same directly wrapped resource subset.
// Check the replay registry before invoking MTLResource methods on an untrusted capture ID.
inline bool ValidMetalCommandResource(WrappedMTLDevice *device, WrappedMTLResource *resource)
{
  if(!resource)
    return false;
  const ResourceId id = GetResID(resource);
  return device->GetReplay()->GetBuffer(id).resourceId != ResourceId() ||
         device->GetReplay()->GetTexture(id).resourceId != ResourceId();
}

// Function tables are MTLResources and may be declared resident before a draw/dispatch.
// Barriers keep the smaller buffer/texture subset above; validate table identity against
// the replay resource manager before passing an untrusted capture object to Metal.
inline bool ValidMetalResidencyResource(WrappedMTLDevice *device, WrappedMTLResource *resource)
{
  if(ValidMetalCommandResource(device, resource)) return true;
  if(!resource) return false;
  WrappedMTLObject *obj = (WrappedMTLObject *)resource;
  if((obj->m_Type != eResVisibleFunctionTable &&
      obj->m_Type != eResIntersectionFunctionTable) ||
     !obj->m_Real || obj->m_Device != device)
    return false;
  MetalResourceManager *manager = device->GetResourceManager();
  const ResourceId id = GetResID(obj);
  return manager->HasResource(id) && manager->GetResource(id) == obj;
}

inline bool ValidMetalResidencyResources(WrappedMTLDevice *device,
                                         const rdcarray<WrappedMTLResource *> &resources)
{
  for(WrappedMTLResource *resource : resources)
    if(!ValidMetalResidencyResource(device, resource)) return false;
  return true;
}

inline bool ValidMetalResourceUsage(uint64_t usage)
{
  return usage != 0 && (usage & ~uint64_t(MTL::ResourceUsageRead | MTL::ResourceUsageWrite |
                                         MTL::ResourceUsageSample)) == 0;
}

inline bool ValidMetalGraphicsStages(uint64_t stages)
{
  // Tile/mesh/object stages require their own replay implementations.
  return stages != 0 && (stages & ~uint64_t(MTL::RenderStageVertex | MTL::RenderStageFragment)) == 0;
}

inline rdcarray<const MTL::Resource *> UnwrapMetalResources(
    const rdcarray<WrappedMTLResource *> &resources)
{
  rdcarray<const MTL::Resource *> real;
  real.reserve(resources.size());
  for(WrappedMTLResource *resource : resources)
    real.push_back(Unwrap(resource));
  return real;
}

inline bool ValidMetalCommandResources(WrappedMTLDevice *device,
                                        const rdcarray<WrappedMTLResource *> &resources)
{
  for(WrappedMTLResource *resource : resources)
    if(!ValidMetalCommandResource(device, resource))
      return false;
  return true;
}

inline void ReferenceMetalCommandResources(MetalResourceRecord *record,
                                            const rdcarray<WrappedMTLResource *> &resources,
                                            bool write)
{
  for(WrappedMTLResource *resource : resources)
    record->MarkResourceFrameReferenced(GetResID(resource),
                                        write ? eFrameRef_ReadBeforeWrite : eFrameRef_Read);
}
