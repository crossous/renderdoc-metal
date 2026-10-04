// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()

static void Collect(const rdcarray<ActionDescription> &actions, const SDFile &sd,
                    std::map<ResourceId, std::set<uint32_t>> &expected, unsigned &scopes)
{
  for(const auto &action : actions)
  {
    for(const auto &event : action.events)
    {
      if(event.chunkIndex >= sd.chunks.size()) continue;
      const SDChunk *chunk = sd.chunks[event.chunkIndex];
      if(strstr(chunk->name.c_str(), "memoryBarrierWithScope")) scopes++;
      if(!strstr(chunk->name.c_str(), "memoryBarrierWithResources")) continue;
      const SDObject *resources = chunk->FindChild("resources");
      if(!resources) continue;
      for(size_t i = 0; i < resources->NumChildren(); i++)
        expected[resources->GetChild(i)->AsResourceId()].insert(event.eventId);
    }
    Collect(action.children, sd, expected, scopes);
  }
}

int main(int argc, char **argv)
{
  if(argc != 2) return 2;
  GlobalEnvironment env; env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {argv[0]});
  auto file = RENDERDOC_OpenCaptureFile(); IReplayController *controller = nullptr;
  auto result = file->OpenFile(argv[1], "rdc", nullptr);
  if(result.OK()) rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr);
  file->Shutdown();
  if(!result.OK() || !controller) return 3;
  std::map<ResourceId, std::set<uint32_t>> expected;
  unsigned scopes = 0;
  Collect(controller->GetRootActions(), controller->GetStructuredFile(), expected, scopes);
  bool ok = !expected.empty() && scopes > 0;
  unsigned checked = 0;
  for(unsigned cycle = 0; cycle < 2; cycle++)
  {
    // Scope barriers must not invent usage for other resident resources.
    for(const auto &resource : controller->GetResources())
    {
      std::set<uint32_t> actual;
      for(const auto &use : controller->GetUsage(resource.resourceId))
        if(use.usage == ResourceUsage::Barrier) actual.insert(use.eventId);
      auto oracle = expected.find(resource.resourceId);
      ok &= actual == (oracle == expected.end() ? std::set<uint32_t>() : oracle->second);
    }
    for(const auto &resource : expected)
      for(uint32_t event : resource.second)
      {
        controller->SetFrameEvent(0, true);
        controller->SetFrameEvent(event, true);
        ok &= controller->GetFatalErrorStatus().OK();
        checked++;
      }
  }
  controller->Shutdown(); RENDERDOC_ShutdownReplay();
  printf("%s exact resource-barrier EIDs, scope exclusion and reset stability: resources=%zu scope-barriers=%u selections=%u\n",
         ok ? "PASS" : "FAIL", expected.size(), scopes, checked);
  return ok ? 0 : 4;
}
