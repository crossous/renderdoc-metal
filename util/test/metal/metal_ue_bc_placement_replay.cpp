// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()

static const ActionDescription *FindDraw(const rdcarray<ActionDescription> &actions)
{
  for(const ActionDescription &action : actions)
  {
    if(action.flags & ActionFlags::Drawcall) return &action;
    if(const ActionDescription *child = FindDraw(action.children)) return child;
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
  unsigned bc = 0;
  ResourceId bc1, bc1srgb, bc5;
  for(const TextureDescription &desc : renderer->GetTextures())
  {
    if(desc.creationFlags & TextureCategory::SwapBuffer) swap = desc.resourceId;
    if(desc.format.type == ResourceFormatType::BC1 || desc.format.type == ResourceFormatType::BC5)
    {
      bc++;
      if(desc.format.type == ResourceFormatType::BC5) bc5 = desc.resourceId;
      else if(desc.format.compType == CompType::UNormSRGB) bc1srgb = desc.resourceId;
      else bc1 = desc.resourceId;
    }
  }
  if(!draw || swap == ResourceId() || bc != 3 || bc1 == ResourceId() ||
     bc1srgb == ResourceId() || bc5 == ResourceId()) return 5;
  const byte red[8] = {0,0xf8,0,0,0,0,0,0};
  const byte blue[8] = {0x1f,0,0,0,0,0,0,0};
  const byte rg[16] = {255,0,0,0,0,0,0,0,128,0,0,0,0,0,0,0};
  for(uint32_t event : {draw->eventId, 10000000U, draw->eventId})
  {
    renderer->SetFrameEvent(event, true);
    const bytebuf a = renderer->GetTextureData(bc1, {0,0,0});
    const bytebuf b = renderer->GetTextureData(bc1srgb, {0,0,0});
    const bytebuf c = renderer->GetTextureData(bc5, {0,0,0});
    if(a.size() != 32 || b.size() != 32 || c.size() != 64 ||
       memcmp(a.data(),red,8) || memcmp(b.data(),blue,8) || memcmp(c.data(),rg,16))
      return 7;
    for(uint32_t mip = 1; mip < 4; mip++)
    {
      const bytebuf ma = renderer->GetTextureData(bc1, {mip,0,0});
      const bytebuf mb = renderer->GetTextureData(bc1srgb, {mip,0,0});
      const bytebuf mc = renderer->GetTextureData(bc5, {mip,0,0});
      if(ma.size() != 8 || mb.size() != 8 || mc.size() != 16 ||
         memcmp(ma.data(),red,8) || memcmp(mb.data(),blue,8) || memcmp(mc.data(),rg,16))
        return 8;
    }
    const PixelValue pixel = renderer->PickPixel(swap, 1, 1, {0,0,0}, CompType::Typeless);
    if(pixel.floatValue[0] < 0.49f || pixel.floatValue[0] > 0.51f ||
       pixel.floatValue[1] < 0.49f || pixel.floatValue[1] > 0.51f ||
       pixel.floatValue[2] < 0.99f || pixel.floatValue[3] < 0.99f)
    {
      fprintf(stderr,"BC replay pixel at %u: %.4f %.4f %.4f %.4f\n",event,
              pixel.floatValue[0],pixel.floatValue[1],pixel.floatValue[2],pixel.floatValue[3]);
      return 6;
    }
  }
  puts("BC placement replay: three resources and forward/back seek pixel OK");
  renderer->Shutdown();
  RENDERDOC_ShutdownReplay();
  return 0;
}
