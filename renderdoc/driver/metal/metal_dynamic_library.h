// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#pragma once

#include "metal_resources.h"

class WrappedMTLDynamicLibrary : public WrappedMTLObject
{
public:
  WrappedMTLDynamicLibrary(MTL::DynamicLibrary *real, ResourceId id, WrappedMTLDevice *device);
  ~WrappedMTLDynamicLibrary();
  rdcstr m_TemporaryPath;
  rdcstr m_TemporaryDirectory;
  enum { TypeEnum = eResDynamicLibrary };
};

bool MetalDynamicLibraryIsWrapped(MTL::DynamicLibrary *library);
