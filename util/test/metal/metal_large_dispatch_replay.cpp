// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
REPLAY_PROGRAM_MARKER()
int main(int argc, char **argv)
{
  if(argc != 3) return 2;
  std::ifstream input(argv[2], std::ios::binary);
  std::vector<uint8_t> expected((std::istreambuf_iterator<char>(input)), {});
  if(expected.empty() || expected.size() > 20 * 1024 * 1024) return 2;
  GlobalEnvironment env; env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {argv[0]});
  auto file = RENDERDOC_OpenCaptureFile();
  auto result = file->OpenFile(argv[1], "rdc", nullptr);
  IReplayController *controller = nullptr;
  if(result.OK()) rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr);
  file->Shutdown();
  if(!result.OK() || !controller) { RENDERDOC_ShutdownReplay(); return 3; }
  ResourceId output;
  for(const auto &resource : controller->GetResources())
    if(resource.name == "LargeDispatchOutput") output = resource.resourceId;
  uint32_t last = 0;
  rdcarray<const rdcarray<ActionDescription> *> lists = {&controller->GetRootActions()};
  while(!lists.empty())
  {
    const auto *list = lists.back(); lists.pop_back();
    for(const auto &action : *list)
    {
      if(!action.IsFakeMarker() && action.eventId > last) last = action.eventId;
      if(!action.children.empty()) lists.push_back(&action.children);
    }
  }
  int code = output == ResourceId() || !last ? 4 : 0;
  for(unsigned cycle = 0; cycle < 3 && !code; ++cycle)
  {
    if(cycle) { controller->SetFrameEvent(0, true); controller->SetFrameEvent(last, true); }
    auto bytes = controller->GetBufferData(output, 0, 0);
    if(!controller->GetFatalErrorStatus().OK() || bytes.size() != expected.size() ||
       memcmp(bytes.data(), expected.data(), expected.size())) code = 5;
  }
  controller->Shutdown(); RENDERDOC_ShutdownReplay();
  if(!code) puts("PASS normal OpenCapture + three full outputs/EID0 exact Native, including padding");
  return code;
}
