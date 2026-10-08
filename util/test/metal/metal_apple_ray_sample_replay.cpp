// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
REPLAY_PROGRAM_MARKER()

static rdcarray<APIEvent> events;
static uint32_t dispatch = 0, copy = 0, last = 0;
static unsigned dispatches = 0, draws = 0;
static void Walk(const rdcarray<ActionDescription> &actions)
{
  for(const auto &action : actions)
  {
    events.append(action.events);
    last = std::max(last, action.eventId);
    if(action.flags & ActionFlags::Dispatch) {dispatch = action.eventId; dispatches++;}
    if(action.flags & ActionFlags::Drawcall) draws++;
    if(action.customName == "copyFromTexture(to buffer)")
      copy = action.eventId;
    Walk(action.children);
  }
}

int main(int argc, char **argv)
{
  if(argc != 3) return 1;
  std::ifstream input(argv[2], std::ios::binary);
  std::vector<char> original((std::istreambuf_iterator<char>(input)), {});
  if(!input || original.empty() || original.size() % 16) return 2;
  bytebuf expected((const byte *)original.data(), original.size());
  GlobalEnvironment environment;
  environment.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(environment, {argv[0]});
  auto file = RENDERDOC_OpenCaptureFile();
  auto result = file->OpenFile(argv[1], "rdc", nullptr);
  IReplayController *controller = nullptr;
  if(result.OK()) rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr);
  file->Shutdown();
  if(!result.OK() || !controller)
  { fprintf(stderr, "OpenCapture failed: %s\n", result.internal_msg ? result.internal_msg->c_str() : "no detail"); return 3; }
  Walk(controller->GetRootActions());
  ResourceId readback, texture;
  for(const auto &resource : controller->GetResources())
  {
    if(resource.name == "Apple ray sample readback") readback = resource.resourceId;
    if(resource.name == "Apple ray sample output") texture = resource.resourceId;
  }
  bool extentMatches = false;
  for(const auto &description : controller->GetTextures())
    if(description.resourceId == texture)
      extentMatches = uint64_t(description.width) * description.height * 16 == expected.size();
  bool ok = extentMatches && dispatches == 1 && draws == 1 && dispatch && last && copy &&
      readback != ResourceId() && texture != ResourceId();
  auto checkOutput = [&]() {
    auto pixels = controller->GetTextureData(texture, {0,0,0});
    if(pixels != expected)
    {
      size_t different = 0;
      for(size_t i = 0; i < std::min(pixels.size(), expected.size()); i++)
        different += pixels[i] != expected[i];
      fprintf(stderr, "Texture differs: bytes=%zu expected=%zu differing=%zu\n",
              pixels.size(), expected.size(), different);
      return false;
    }
    return true;
  };
  controller->SetFrameEvent(last, true);
  ok &= controller->GetFatalErrorStatus().OK() && checkOutput() &&
      controller->GetBufferData(readback, 0, expected.size()) == expected;
  controller->SetFrameEvent(dispatch, true);
  const auto *state = controller->GetPipelineState().GetMetalPipelineState();
  ok &= state && state->computeAccelerationStructures.size() > 4 &&
      state->computeAccelerationStructures[4] != ResourceId() && checkOutput();
  std::sort(events.begin(), events.end(),
            [](const APIEvent &a, const APIEvent &b) { return a.eventId < b.eventId; });
  const auto uniqueEnd = std::unique(events.begin(), events.end(),
                          [](const APIEvent &a, const APIEvent &b) { return a.eventId == b.eventId; });
  events.resize(size_t(uniqueEnd - events.begin()));
  for(unsigned cycle = 0; cycle < 3 && ok; cycle++)
    for(size_t i = 0; i < events.size() && ok; i++)
    {
      const auto &event = events[cycle == 1 ? events.size() - 1 - i : i];
      controller->SetFrameEvent(0, true);
      controller->SetFrameEvent(event.eventId, true);
      ok &= controller->GetFatalErrorStatus().OK();
      const auto &name = controller->GetStructuredFile().chunks[event.chunkIndex]->name;
      // Compute/render/blit GPU events follow the submitted execution prefix.
      if((name.beginsWith("MTLComputeCommandEncoder::") ||
          name.beginsWith("MTLRenderCommandEncoder::") ||
          name.beginsWith("MTLBlitCommandEncoder::")) && event.eventId >= dispatch)
        ok &= checkOutput();
      printf("%s cycle=%u EID=%u\n", ok ? "PASS" : "FAIL", cycle, event.eventId);
    }
  controller->Shutdown();
  RENDERDOC_ShutdownReplay();
  printf("%s official ray output, TLAS binding, %zu events in three directions\n",
         ok ? "PASS" : "FAIL", events.size());
  return ok ? 0 : 4;
}
