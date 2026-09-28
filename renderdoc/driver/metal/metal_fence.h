// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#pragma once

#include "metal_common.h"

class WrappedMTLFence : public WrappedMTLObject
{
public:
  WrappedMTLFence(MTL::Fence *real, ResourceId id, WrappedMTLDevice *device);
  enum { TypeEnum = eResFence };

  void Updated(uint64_t epoch, ResourceId encoder)
  {
    m_UpdateEpoch = epoch;
    m_UpdateEncoder = encoder;
  }
  bool CanWait(uint64_t epoch, ResourceId encoder) const
  {
    // No initial fence state can be imported from before the capture. In particular, an old
    // replay/seek must not make an otherwise unsignalled wait appear safe.
    return epoch != 0 && m_UpdateEpoch == epoch && m_UpdateEncoder != encoder;
  }

private:
  uint64_t m_UpdateEpoch = 0;
  ResourceId m_UpdateEncoder;
};

inline bool ValidMetalFence(WrappedMTLFence *fence)
{
  return fence && fence->m_Type == eResFence && fence->m_Real;
}
