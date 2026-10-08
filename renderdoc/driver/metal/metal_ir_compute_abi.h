// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>

// Binding facts surviving conversion, not a declaration of shader accesses.
// Resource eligibility still requires current provenance and submission proofs.
struct MetalIRComputeRuntimeABI
{
  uint32_t rootBindPoint = 0, resourceBindPoint = 0, samplerBindPoint = 0;
  uint32_t cbvCount = 0, staticSamplerCount = 0, rootBytes = 0;
  uint32_t threads[3] = {};
};
enum class MetalIRComputeABIResult { Invalid, OtherMetadata, RuntimeBindings };
MetalIRComputeABIResult ParseMetalIRComputeRuntimeABI(
    const char *text, size_t length, MetalIRComputeRuntimeABI &abi);
