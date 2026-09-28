// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#pragma once

#include "metal_common.h"

class WrappedMTLCounterSampleBuffer : public WrappedMTLObject
{
public:
  WrappedMTLCounterSampleBuffer(MTL::CounterSampleBuffer *real, ResourceId id,
                                WrappedMTLDevice *device);

  enum { TypeEnum = eResCounterSampleBuffer };
};
