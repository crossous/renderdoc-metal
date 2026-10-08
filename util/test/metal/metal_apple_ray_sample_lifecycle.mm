// SPDX-License-Identifier: MIT
// Reuse the official sample's full pixel/event oracle across successful and
// failed controller lifetimes in one process. Names belong only to the fixture.
#define main AppleSampleReplay
#include "metal_apple_ray_sample_replay.cpp"
#undef main
#include "renderdoc/driver/metal/official/metal-cpp.h"
#include <mach/mach.h>

static uint64_t ResidentBytes()
{
  mach_task_basic_info_data_t info = {};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  return task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS
             ? info.resident_size : 0;
}
static bool RejectAndClose(const char *path)
{
  GlobalEnvironment env; env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {});
  auto file = RENDERDOC_OpenCaptureFile();
  auto result = file->OpenFile(path, "rdc", nullptr);
  IReplayController *controller = nullptr;
  bool validContainer = result.OK();
  if(validContainer) rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr);
  file->Shutdown();
  const bool rejected = validContainer && result.code == ResultCode::APIReplayFailed && !controller;
  if(controller) controller->Shutdown();
  RENDERDOC_ShutdownReplay();
  return rejected;
}
int main(int argc, char **argv)
{
  if(argc != 5) return 2;
  int iterations = atoi(argv[4]);
  if(iterations < 10 || iterations > 32) return 2;
  uint64_t baseline = 0;
  for(int i = 0; i < iterations; i++)
  {
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    if(!RejectAndClose(argv[3])) return 3;
    events.clear(); dispatch = copy = last = dispatches = draws = 0;
    char *arguments[] = {argv[0], argv[1], argv[2]};
    if(AppleSampleReplay(3, arguments)) return 4;
    pool.reset();
    if(i == 1) baseline = ResidentBytes();
  }
  const uint64_t resident = ResidentBytes();
  const uint64_t growth = resident > baseline ? resident - baseline : 0;
  const bool ok = baseline && growth <= 64ULL * 1024ULL * 1024ULL;
  printf("%s official sample failed/successful lifetimes=%d growth=%llu bytes; full Native pixels and all original events/EID0\n",
         ok ? "PASS" : "FAIL", iterations, (unsigned long long)growth);
  return ok ? 0 : 5;
}
