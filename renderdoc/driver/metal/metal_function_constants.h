// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#pragma once

#include "metal_common.h"

// Public constant setters have no corresponding getters. Preserve their ordered writes rather
// than inspect private Metal objects or substitute default values during replay.
struct MetalFunctionSnapshot
{
  bool supported = true;
  rdcstr name, specializedName;
  uint64_t options = 0;
  rdcarray<rdcstr> constantNames;
  rdcarray<uint64_t> constantIndices, constantTypes;
  rdcarray<rdcarray<byte>> constantValues;
};

inline uint32_t MetalFunctionConstantSize(uint64_t type)
{
  switch(type)
  {
    case MTL::DataTypeBool:
    case MTL::DataTypeChar:
    case MTL::DataTypeUChar: return 1;
    case MTL::DataTypeHalf:
    case MTL::DataTypeShort:
    case MTL::DataTypeUShort: return 2;
    case MTL::DataTypeFloat:
    case MTL::DataTypeInt:
    case MTL::DataTypeUInt: return 4;
    case MTL::DataTypeLong:
    case MTL::DataTypeULong: return 8;
    default: return 0;
  }
}

void RegisterMetalFunctionConstantHooks();
MetalFunctionSnapshot CaptureMetalFunctionSnapshot(NS::String *name,
                                                   MTL::FunctionConstantValues *values,
                                                   MTL::FunctionDescriptor *descriptor);
