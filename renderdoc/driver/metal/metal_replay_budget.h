// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <initializer_list>

// Native dispatch extents are execution arguments, not a CPU analysis budget.
// Keep checked arithmetic; pipeline/device threadgroup limits are validated by
// ValidateComputeThreadgroupSnapshot before Native execution. A zero grid is
// a no-op, while a zero threadgroup dimension is invalid.
inline bool MetalComputeDispatchExtentFits(const uint64_t groups[3], const uint64_t threads[3])
{
  uint64_t work = 1;
  for(unsigned i = 0; i < 3; ++i)
    if(!threads[i]) return false;
  if(!groups[0] || !groups[1] || !groups[2]) return true;
  for(unsigned i = 0; i < 3; ++i)
    for(uint64_t axis : {groups[i], threads[i]})
    {
      if(work > UINT64_MAX / axis) return false;
      work *= axis;
    }
  return true;
}

// Bound retained preflight bookkeeping for the whole frame, independently of
// dispatch/command kind and capture protocol. Shader work has a separate budget.
struct MetalReplayPreflightBudget
{
  uint64_t bytes = 0;
  bool Consume(uint64_t encodedBytes)
  {
    const uint64_t limit = 128ULL * 1024 * 1024;
    const uint64_t recordReserve = 4096;
    if(bytes > limit - recordReserve || encodedBytes > limit - recordReserve - bytes)
      return false;
    bytes += recordReserve + encodedBytes;
    return true;
  }
};

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
