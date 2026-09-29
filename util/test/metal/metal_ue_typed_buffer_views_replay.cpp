// SPDX-License-Identifier: MIT
#include <cstdio>

#include "renderdoc/api/replay/renderdoc_replay.h"

REPLAY_PROGRAM_MARKER()

static const ActionDescription *FindDraw(const rdcarray<ActionDescription> &actions)
{
  for(const ActionDescription &action : actions)
  {
    if(action.flags & ActionFlags::Drawcall)
      return &action;
    if(const ActionDescription *child = FindDraw(action.children))
      return child;
  }
  return NULL;
}

int main(int argc, char **argv)
{
  if(argc != 2) return 2;
  GlobalEnvironment env;
  env.enumerateGPUs = false;
  rdcarray<rdcstr> args;
  args.push_back(argv[0]);
  RENDERDOC_InitialiseReplay(env, args);
  ICaptureFile *file = RENDERDOC_OpenCaptureFile();
  ResultDetails result = file->OpenFile(argv[1], "rdc", NULL);
  if(!result.OK()) return 3;
  IReplayController *renderer = NULL;
  rdctie(result, renderer) = file->OpenCapture(ReplayOptions(), NULL);
  file->Shutdown();
  if(!result.OK() || !renderer) return 4;

  const ActionDescription *draw = FindDraw(renderer->GetRootActions());
  ResourceId swap;
  for(const TextureDescription &desc : renderer->GetTextures())
    if(desc.creationFlags & TextureCategory::SwapBuffer) swap = desc.resourceId;
  if(!draw || swap == ResourceId()) return 5;

  ResourceId parents[3] = {};
  ResourceId views[3] = {};
  uint32_t widths[3] = {64, 64, 32};
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.type == ResourceType::Texture && resource.parentResources.size() == 1)
      for(const TextureDescription &desc : renderer->GetTextures())
        if(desc.resourceId == resource.resourceId && desc.height == 1)
          for(int i = 0; i < 3; i++)
            if(desc.width == widths[i] && views[i] == ResourceId())
            {
              views[i] = resource.resourceId;
              parents[i] = resource.parentResources[0];
              break;
            }
  // The two 64-wide views are distinguished by their creation order; all three
  // must retain a different parent buffer. Texture-buffer GetTextureData is not
  // currently exposed by the replay API, so pixels below check actual GPU use.
  for(int i = 0; i < 3; i++)
  {
    if(views[i] == ResourceId() || parents[i] == ResourceId()) return 6;
  }
  if(parents[0] == parents[1] || parents[0] == parents[2] || parents[1] == parents[2]) return 7;

  for(uint32_t event : {draw->eventId, 10000000U, draw->eventId})
  {
    renderer->SetFrameEvent(event, true);
    const PixelValue pixel = renderer->PickPixel(swap, 1, 1, {0, 0, 0}, CompType::Typeless);
    if(pixel.floatValue[0] < 0.49f || pixel.floatValue[0] > 0.51f ||
       pixel.floatValue[1] < 0.99f || pixel.floatValue[2] < 0.49f ||
       pixel.floatValue[2] > 0.51f || pixel.floatValue[3] < 0.49f ||
       pixel.floatValue[3] > 0.51f)
    {
      fprintf(stderr, "typed view replay pixel at %u: %.4f %.4f %.4f %.4f\n", event,
              pixel.floatValue[0], pixel.floatValue[1], pixel.floatValue[2],
              pixel.floatValue[3]);
      return 8;
    }
  }
  puts("typed view replay: resource parents and seek pixels OK");
  renderer->Shutdown();
  RENDERDOC_ShutdownReplay();
  return 0;
}
