// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static bool Actions(const rdcarray<ActionDescription> &actions, uint32_t &last)
{
  for(const auto &action : actions)
  {
    last = std::max(last, action.eventId);
    if(action.flags & ActionFlags::Dispatch) return false;
    if(!Actions(action.children, last)) return false;
  }
  return true;
}
int main(int argc, char **argv)
{
  if(argc != 2 && argc != 3) return 2;
  GlobalEnvironment env; env.enumerateGPUs = false;
  rdcarray<rdcstr> args; args.push_back(argv[0]); RENDERDOC_InitialiseReplay(env, args);
  ICaptureFile *file = RENDERDOC_OpenCaptureFile();
  auto result = file->OpenFile(argv[1], "rdc", nullptr);
  IReplayController *controller = nullptr;
  if(!result.OK()) return 3;
  rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr);
  file->Shutdown();
  if(!result.OK() || !controller) return 4;
  uint32_t last = 0;
  if(!Actions(controller->GetRootActions(), last) || !last) return 5;
  for(uint32_t event : {0U, last, 0U, last})
  {
    controller->SetFrameEvent(event, true);
    rdcarray<uint32_t> values;
    for(const auto &buffer : controller->GetBuffers())
    {
      if(buffer.length != 4) return 6;
      const auto bytes = controller->GetBufferData(buffer.resourceId, 0, 4);
      if(bytes.size() != 4) return 7;
      uint32_t value = 0; memcpy(&value, bytes.data(), 4); values.push_back(value);
    }
    std::sort(values.begin(), values.end());
    if(argc == 2 && (values.size() != 2 || values[0] != 111 || values[1] != 222)) return 8;
    printf("PASS EID=%u initial values=%zu%s\n", event, values.size(), argc == 2 ? " [111,222]" : " (automatic capture)");
  }
  const auto textures = controller->GetTextures();
  if(textures.size() != 1 || textures[0].width != 2 || textures[0].height != 2) return 9;
  if(controller->GetTextureData(textures[0].resourceId, {0,0,0}).size() != 16) return 10;
  controller->Shutdown(); RENDERDOC_ShutdownReplay();
  puts("PASS replay: only captured frame commands; completed pre-capture GPU data survives seeks");
  return 0;
}
