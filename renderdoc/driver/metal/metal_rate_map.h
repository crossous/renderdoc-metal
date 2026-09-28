// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#pragma once

#include "metal_common.h"

class WrappedMTLRasterizationRateMap : public WrappedMTLObject
{
public:
  WrappedMTLRasterizationRateMap(MTL::RasterizationRateMap *real, ResourceId id,
                                 WrappedMTLDevice *device);

  DECLARE_FUNCTION_SERIALISED(void, copyParameterDataToBuffer, WrappedMTLBuffer *buffer,
                              NS::UInteger offset);

  enum { TypeEnum = eResRasterizationRateMap };
};
