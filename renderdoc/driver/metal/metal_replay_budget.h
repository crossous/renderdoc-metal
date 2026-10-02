// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <initializer_list>

// Count backing allocations once, as D3D12 heaps and Vulkan device memory do.
// These limits are a replay policy, not a claim that recommended working set
// represents currently free memory. Reserve uploader/readback/driver headroom.
struct MetalReplayAllocationBudget
{
  uint64_t nativeBytes = 0;
  uint64_t initialBytes = 0;
  uint64_t snapshotBytes = 0;
  bool valid = true;

  void Add(uint64_t &total, uint64_t bytes)
  {
    if(bytes > UINT64_MAX - total) valid = false;
    else total += bytes;
  }

  bool Fits(uint64_t recommended, uint64_t alreadyAllocated) const
  {
    const uint64_t mib = 1024ULL * 1024;
    const uint64_t gpuLimit = recommended / 4 < 3072 * mib ? recommended / 4 : 3072 * mib;
    const uint64_t totalLimit = recommended / 2 < 6144 * mib ? recommended / 2 : 6144 * mib;
    const uint64_t gpuReserve = 128 * mib;
    const uint64_t cpuReserve = 64 * mib;
    if(!valid || gpuLimit < gpuReserve || alreadyAllocated > gpuLimit - gpuReserve ||
       nativeBytes > gpuLimit - gpuReserve - alreadyAllocated || totalLimit < gpuReserve + cpuReserve)
      return false;
    uint64_t left = totalLimit - gpuReserve - cpuReserve;
    for(uint64_t cost : {nativeBytes, alreadyAllocated, initialBytes, initialBytes, snapshotBytes})
    {
      if(cost > left) return false;
      left -= cost;
    }
    return true;
  }
};
