// SPDX-License-Identifier: MIT
#pragma once
#include "metal_resources.h"

MTL::StitchedLibraryDescriptor *MetalMakeSingleStitchedDescriptor(
    MTL::Function *function, const rdcstr &graphName, const rdcstr &functionName,
    uint32_t inputIndex);
