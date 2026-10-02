// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cassert>
#include "renderdoc/driver/metal/metal_replay_budget.h"
int main()
{
  const uint64_t mib=1024ULL*1024, recommendation=12713115648ULL;
  MetalReplayAllocationBudget actual;
  actual.Add(actual.nativeBytes,2393702400ULL);
  actual.Add(actual.initialBytes,1282587160ULL);
  assert(actual.Fits(recommendation,0));
  // A placement child is not another backing. Charging the captured heap
  // buffer payload again would reject this otherwise bounded frame.
  auto duplicate=actual;duplicate.Add(duplicate.nativeBytes,664615760ULL);
  assert(!duplicate.Fits(recommendation,0));
  assert(!actual.Fits(0,0));
  assert(!actual.Fits(recommendation,1024*mib));
  auto tooManyHeaps=actual;tooManyHeaps.Add(tooManyHeaps.nativeBytes,2048*mib);
  assert(!tooManyHeaps.Fits(recommendation,0));
  auto cpu=actual;cpu.Add(cpu.initialBytes,2048*mib);
  assert(!cpu.Fits(recommendation,0));
  auto snapshots=actual;snapshots.Add(snapshots.snapshotBytes,4096*mib);
  assert(!snapshots.Fits(recommendation,0));
  MetalReplayAllocationBudget overflow;
  overflow.Add(overflow.nativeBytes,UINT64_MAX);overflow.Add(overflow.nativeBytes,1);
  assert(!overflow.Fits(recommendation,0));
  MetalReplayAllocationBudget boundary;
  boundary.nativeBytes=recommendation/4-128*mib;
  assert(boundary.Fits(recommendation,0));
  boundary.Add(boundary.nativeBytes,1);
  assert(!boundary.Fits(recommendation,0));
  MetalReplayAllocationBudget capped;capped.nativeBytes=3072*mib;
  assert(!capped.Fits(64ULL*1024*mib,0));
  puts("PASS allocation budget: actual backing/initial costs, no duplicate placement charge, device/headroom/CPU/snapshot bounds, overflow, exact boundary, hard cap; no GPU work");
}
