// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cassert>
#include "renderdoc/driver/metal/metal_replay_budget.h"
int main()
{
  const uint64_t largeGroups[] = {160, 180, 1}, ordinaryThreads[] = {8, 8, 1};
  assert(MetalComputeDispatchExtentFits(largeGroups, ordinaryThreads));
  const uint64_t zeroGroups[] = {0, UINT64_MAX, 1};
  assert(MetalComputeDispatchExtentFits(zeroGroups, ordinaryThreads));
  const uint64_t invalidThreads[] = {8, 0, 1}, overflowGroups[] = {UINT64_MAX, 2, 1};
  assert(!MetalComputeDispatchExtentFits(largeGroups, invalidThreads));
  assert(!MetalComputeDispatchExtentFits(overflowGroups, ordinaryThreads));
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
  // Several individually bounded copies share the frame's memory budget;
  // the per-copy staging boundary is not a cumulative one-copy allowance.
  MetalReplayPreflightBudget frame;
  assert(frame.Consume(mib) && frame.Consume(mib));
  assert(frame.Consume(257/8+1)); // sparse field-proof storage shares this budget
  const uint64_t beforeFailure=frame.bytes;
  assert(!frame.Consume(UINT64_MAX) && frame.bytes==beforeFailure);
  MetalReplayPreflightBudget exact;
  assert(exact.Consume(128*mib-4096));
  assert(!exact.Consume(0));
  MetalReplayPreflightBudget aggregate;
  unsigned copies=0;
  while(aggregate.Consume(mib))copies++;
  assert(copies>2 && copies<128 && aggregate.bytes<=128*mib);
  puts("PASS allocation budget: actual backing/initial costs, no duplicate placement charge, device/headroom/CPU/snapshot bounds, overflow, exact boundary, hard cap; no GPU work");
  puts("PASS unified preflight budget: multiple copies + sparse proof storage, aggregate rejection, overflow and exact boundary; no GPU work");
}
