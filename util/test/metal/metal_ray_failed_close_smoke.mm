// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include "renderdoc/driver/metal/official/metal-cpp.h"
#include <mach/mach.h>
#include <sys/file.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
REPLAY_PROGRAM_MARKER()

static uint32_t Dispatch(const rdcarray<ActionDescription> &actions)
{
  for(const auto &action : actions)
  {
    if(action.flags & ActionFlags::Dispatch) return action.eventId;
    if(uint32_t event = Dispatch(action.children)) return event;
  }
  return 0;
}

static bool CloseCapture(const char *path, bool successExpected)
{
  auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
  ICaptureFile *file = RENDERDOC_OpenCaptureFile();
  ResultDetails result = file->OpenFile(path, "rdc", NULL);
  // Both fixtures have valid RDC containers: a parser/setup failure must occur
  // inside OpenCapture, and its partially constructed device must be closed.
  if(!result.OK()) { file->Shutdown(); return false; }
  if(successExpected)
  {
    bool hasList = false;
    for(const auto chunk : file->GetStructuredData().chunks)
      if(chunk->name == "Internal::List of Initial Contents Resources")
      {
        auto records = chunk->FindChild("NeededInitials");
        hasList = records && records->NumChildren() > 0;
      }
    if(!hasList) { file->Shutdown(); return false; }
  }
  IReplayController *controller = NULL;
  rdctie(result, controller) = file->OpenCapture(ReplayOptions(), NULL);
  file->Shutdown();
  bool ok = successExpected ? result.OK() && controller : !result.OK() && !controller;
  if(controller && successExpected)
  {
    ResourceId output, aliasOriginal, aliasReplacement;
    for(const auto &resource : controller->GetResources())
    {
      if(resource.name == "IR ray results") output = resource.resourceId;
      if(resource.name == "Placement alias original") aliasOriginal = resource.resourceId;
      if(resource.name == "Placement alias replacement") aliasReplacement = resource.resourceId;
    }
    const uint32_t dispatch = Dispatch(controller->GetRootActions());
    if(aliasOriginal != ResourceId() && aliasReplacement != ResourceId())
    {
      // Headless GPU-copy captures also exercise complete open/seek/reset/close
      // after a damaged resource plan has aborted a prior controller creation.
      ok &= dispatch == 0;
      for(uint32_t event : {0U, ~0U, 0U})
      {
        controller->SetFrameEvent(event, true);
        auto prefix = controller->GetBufferData(aliasOriginal, 0, 16);
        auto overlap = controller->GetBufferData(aliasOriginal, 128 * 1024, 16);
        ok &= prefix.size() == 16 && overlap.size() == 16;
        for(byte value : prefix) ok &= value == (event ? 0x22 : 0x11);
        for(byte value : overlap) ok &= value == (event ? 0x5a : 0x11);
        if(event == 0) ok &= controller->GetBufferData(aliasReplacement, 0, 16).empty();
      }
    }
    else
    {
      ok &= output != ResourceId() && dispatch != 0;
      for(uint32_t event : {0U, dispatch, 0U})
      {
        controller->SetFrameEvent(event, true);
        auto bytes = controller->GetBufferData(output, 0, 8);
        uint32_t values[2] = {};
        if(bytes.size() == sizeof(values)) memcpy(values, bytes.data(), sizeof(values));
        ok &= bytes.size() == sizeof(values) && values[0] == (event ? 146U : 7U) &&
              values[1] == (event ? 11U : 7U);
      }
    }
  }
  if(controller) controller->Shutdown();
  return ok;
}

static uint64_t ResidentBytes()
{
  mach_task_basic_info_data_t info = {};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  return task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) ==
      KERN_SUCCESS ? info.resident_size : 0;
}

int main(int argc, char **argv)
{
  // Share the same exclusive lifetime lock as native/capture/IR corruption gates.
  const char *temporary = "/tmp";
  for(const char *key : {"TMPDIR", "TEMP", "TMP"})
  {
    const char *candidate = getenv(key);
    if(candidate && candidate[0] && access(candidate, W_OK | X_OK) == 0)
    { temporary = candidate; break; }
  }
  const rdcstr lockPath = rdcstr(temporary) + "/renderdoc-metal-ir-gpu-tests.lock";
  FILE *gpuLock = fopen(lockPath.c_str(), "a");
  if(!gpuLock || flock(fileno(gpuLock), LOCK_EX)) return 4;
  if(argc < 4) return 1;
  const int iterations = atoi(argv[argc - 1]);
  if(iterations < 10 || iterations > 32) return 2;
  auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
  GlobalEnvironment env; env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {argv[0]});
  bool ok = true; uint64_t baseline = 0;
  for(int i = 0; i < iterations && ok; i++)
  {
    // Fail first, then prove the next complete open/seek/reset still works.
    for(int bad = 2; bad < argc - 1 && ok; bad++) ok = CloseCapture(argv[bad], false);
    if(ok) ok = CloseCapture(argv[1], true);
    if(i == 1) baseline = ResidentBytes();
    if(!ok) fprintf(stderr, "Failed-close lifecycle failed at iteration %d\n", i);
  }
  const uint64_t final = ResidentBytes();
  const uint64_t growth = final > baseline ? final - baseline : 0;
  RENDERDOC_ShutdownReplay();
  pool.reset();
  ok &= baseline && growth <= 64ULL * 1024ULL * 1024ULL;
  printf("%s failed-close lifecycle: %d failures + 1 success x %d iterations; "
         "initial-contents list, output/placement bytes, EID0/action/EID0; growth=%llu bytes\n",
         ok ? "PASS" : "FAIL", argc - 3, iterations, growth);
  fclose(gpuLock);
  return ok ? 0 : 3;
}
