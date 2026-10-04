// SPDX-License-Identifier: MIT
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Last(const rdcarray<ActionDescription> &actions, uint32_t &event)
{
  for(const auto &a : actions)
  {
    event = event > a.eventId ? event : a.eventId;
    Last(a.children, event);
  }
}
static ResourceId ID(uint64_t value)
{
  ResourceId id;
  memcpy(&id, &value, sizeof(id));
  return id;
}
static bool Near(float a, float b)
{
  return std::fabs(a - b) < .003f;
}
int main(int argc, char **argv)
{
  if(argc != 2 && argc != 5)
    return 2;
  GlobalEnvironment env;
  env.enumerateGPUs = false;
  rdcarray<rdcstr> args;
  args.push_back(argv[0]);
  RENDERDOC_InitialiseReplay(env, args);
  auto *file = RENDERDOC_OpenCaptureFile();
  auto result = file->OpenFile(argv[1], "rdc", nullptr);
  IReplayController *r = nullptr;
  if(!result.OK())
    return 3;
  rdctie(result, r) = file->OpenCapture(ReplayOptions(), nullptr);
  file->Shutdown();
  if(!result.OK() || !r)
  {
    fprintf(stderr, "Open failed: %s\n",
            result.internal_msg ? result.internal_msg->c_str() : "unknown");
    return 4;
  }
  uint32_t event = 0;
  Last(r->GetRootActions(), event);
  const bool ue = argc == 5;
  if(ue)
    event = uint32_t(strtoul(argv[2], nullptr, 10));
  ResourceId ids[10];
  for(const auto &resource : r->GetResources())
    for(unsigned i = 0; i < 10; i++)
    {
      char name[64];
      snprintf(name, sizeof(name), "Texture inspection %u", i);
      if(resource.name == name)
        ids[i] = resource.resourceId;
    }
  if(ue)
    ids[0] = ID(strtoull(argv[3], nullptr, 10));
  for(unsigned i = 0; i < (ue ? 1U : 10U); i++)
    if(ids[i] == ResourceId())
    {
      fprintf(stderr, "missing inspection texture %u\n", i);
      return 5;
    }
  IReplayOutput *output =
      r->CreateOutput(CreateHeadlessWindowingData(32, 16), ReplayOutputType::Texture);
  auto show = [&](TextureDisplay display) {
    output->SetTextureDisplay(display);
    output->Display();
    return output->ReadbackOutputTexture();
  };
  auto rgb = [&](const bytebuf &b, unsigned x, unsigned y, float red, float green, float blue) {
    if(b.size() != 32 * 16 * 3)
      return false;
    const byte *p = b.data() + (y * 32 + x) * 3;
    bool ok = Near(p[0] / 255.0f, red) && Near(p[1] / 255.0f, green) && Near(p[2] / 255.0f, blue);
    if(!ok)
      fprintf(stderr, "pixel %u,%u got=%u,%u,%u wanted=%.3f,%.3f,%.3f\n", x, y, p[0], p[1], p[2],
              red, green, blue);
    return ok;
  };
  for(unsigned cycle = 0; cycle < 2; cycle++)
  {
    r->SetFrameEvent(event, true);
    if(!r->GetFatalErrorStatus().OK())
      return 6;
    const bytebuf before = r->GetTextureData(ids[0], {0, 0, 0});
    PixelValue lo, hi;
    rdctie(lo, hi) = r->GetMinMax(ids[0], {0, 0, 0}, CompType::Typeless);
    auto histogram = r->GetHistogram(ids[0], {0, 0, 0}, CompType::Typeless, ue ? 0.0f : -.5f,
                                     ue ? 1.0f : 1.25f, {true, false, false, false});
    uint64_t sum = 0;
    for(uint32_t bucket : histogram)
      sum += bucket;
    if(histogram.size() != 256 || sum != (ue ? 320U * 240U : 30U))
      return 7;
    if(!ue)
    {
      if(!Near(lo.floatValue[0], -.5f) || !std::isinf(hi.floatValue[0]) ||
         !Near(lo.floatValue[1], 0) || !Near(hi.floatValue[1], .75f))
        return 8;
      rdctie(lo, hi) = r->GetMinMax(ids[2], {0, 0, 0}, CompType::Typeless);
      if(lo.uintValue[0] != 0 || hi.uintValue[0] != 93)
        return 9;
      rdctie(lo, hi) = r->GetMinMax(ids[3], {0, 0, 0}, CompType::Typeless);
      if(lo.intValue[0] != -16 || hi.intValue[0] != 15)
        return 10;
      auto signedHist = r->GetHistogram(ids[3], {0, 0, 0}, CompType::Typeless, -16, 15,
                                        {true, false, false, false});
      uint64_t signedSum = 0;
      for(auto count : signedHist)
        signedSum += count;
      if(signedSum != 32 || signedHist[0] != 1 || signedHist[255] != 1)
        return 11;
      if(!r->GetHistogram(ids[0], {0, 0, 0}, CompType::Typeless, 1, 0, {true, false, false, false})
              .empty())
        return 12;
      auto emptyHist =
          r->GetHistogram(ids[0], {0, 0, 0}, CompType::Typeless, 0, 1, {false, false, false, false});
      uint64_t emptySum = 0;
      for(auto count : emptyHist)
        emptySum += count;
      if(emptyHist.size() != 256 || emptySum)
        return 13;
    }
    TextureDisplay display;
    display.resourceId = ids[0];
    display.scale = 4;
    display.linearDisplayAsGamma = true;
    display.overlay = DebugOverlay::NaN;
    auto pixels = show(display);
    if(!ue && (!rgb(pixels, 30, 2, 1, 0, 0) || !rgb(pixels, 30, 6, 0, 1, 0) ||
               !rgb(pixels, 2, 2, 0, 0, 1) || !rgb(pixels, 18, 10, .5, .5, .5)))
      return 14;
    display.red = false;
    pixels = show(display);
    if(!ue && !rgb(pixels, 30, 2, .0361f, .0361f, .0361f))
      return 15;
    display.red = true;
    display.overlay = DebugOverlay::Clipping;
    display.rangeMin = .25f;
    display.rangeMax = .75f;
    pixels = show(display);
    if(!ue && (!rgb(pixels, 2, 2, 1, 0, 0) || !rgb(pixels, 26, 10, 0, 1, 0) ||
               !rgb(pixels, 18, 10, .5, .5, .5)))
      return 16;
    display.overlay = DebugOverlay::NoOverlay;
    display.rangeMin = 0;
    display.rangeMax = 1;
    display.green = false;
    display.blue = false;
    pixels = show(display);
    if(!ue && !rgb(pixels, 18, 10, .5, .5, .5))
      return 17;
    display.rangeMin = .25f;
    display.rangeMax = 1.25f;
    pixels = show(display);
    if(!ue && !rgb(pixels, 18, 10, .25, .25, .25))
      return 18;
    display.rangeMin = 0;
    display.rangeMax = 1;
    display.linearDisplayAsGamma = false;
    pixels = show(display);
    if(!ue && !rgb(pixels, 18, 10, .735356f, .735356f, .735356f))
      return 19;
    display.linearDisplayAsGamma = true;
    display.green = display.blue = true;
    display.flipY = true;
    pixels = show(display);
    if(!ue && !rgb(pixels, 18, 2, .5, .75, .5))
      return 20;
    display.flipY = false;
    display.hdrMultiplier = 2;
    pixels = show(display);
    if(!ue && !rgb(pixels, 18, 10, .5, .5, .5))
      return 21;
    if(!ue)
    {
      display.hdrMultiplier = -1;
      display.resourceId = ids[1];
      pixels = show(display);
      if(!rgb(pixels, 18, 10, 128.0f / 255.0f, 160.0f / 255.0f, 64.0f / 255.0f))
        return 22;
      rdctie(lo, hi) = r->GetMinMax(ids[1], {0, 0, 0}, CompType::Typeless);
      if(!Near(lo.floatValue[0], .21586f) || !Near(hi.floatValue[0], .21586f))
        return 23;
      display.resourceId = ids[2];
      display.rangeMax = 93;
      display.green = display.blue = false;
      pixels = show(display);
      if(!rgb(pixels, 18, 10, 60.0f / 93.0f, 60.0f / 93.0f, 60.0f / 93.0f))
        return 26;
      display.resourceId = ids[3];
      display.rangeMin = -16;
      display.rangeMax = 15;
      pixels = show(display);
      if(!rgb(pixels, 18, 10, 20.0f / 31.0f, 20.0f / 31.0f, 20.0f / 31.0f))
        return 27;
      display.resourceId = ids[8];
      display.rangeMin = 0;
      display.rangeMax = 1;
      pixels = show(display);
      if(!rgb(pixels, 18, 10, .375, .375, .375))
        return 28;
      display.red = false;
      display.green = true;
      pixels = show(display);
      if(!rgb(pixels, 18, 10, 128.0f / 255, 128.0f / 255, 128.0f / 255))
        return 29;
      rdctie(lo, hi) = r->GetMinMax(ids[8], {0, 0, 0}, CompType::Typeless);
      if(!Near(lo.floatValue[0], .375) || !Near(hi.floatValue[1], 128.0f / 255))
        return 30;
      display.red = display.blue = true;
      for(unsigned t = 4; t < 8; t++)
      {
        const unsigned slices = t == 4 ? 3 : t == 5 ? 6 : t == 6 ? 4 : 12;
        for(unsigned slice = 0; slice < slices; slice++)
        {
          display.resourceId = ids[t];
          display.subresource = {0, slice, 0};
          pixels = show(display);
          if(!rgb(pixels, 2, 2, .04f * (slice + 1), 0, 0) ||
             !rgb(pixels, 26, 10, .04f * (slice + 1), .75, .25))
            return 31;
          rdctie(lo, hi) = r->GetMinMax(ids[t], display.subresource, CompType::Typeless);
          if(!Near(lo.floatValue[0], .04f * (slice + 1)) || !Near(hi.floatValue[1], .875))
            return 32;
          auto hist = r->GetHistogram(ids[t], display.subresource, CompType::Typeless, 0, 1,
                                      {true, true, true, false});
          uint64_t total = 0;
          for(auto n : hist)
            total += n;
          if(total != 64 * 3)
            return 33;
        }
        display.subresource = {1, 1, 0};
        pixels = show(display);
        if(!rgb(pixels, 2, 2, .09f, 0, 0) || !rgb(pixels, 26, 14, .09f, .75, .25))
          return 34;
      }
      display = TextureDisplay();
      display.resourceId = ids[9];
      display.scale = 4;
      display.subresource = {1, 0, 0};
      display.rangeMax = 200;
      display.green = display.blue = false;
      pixels = show(display);
      if(!rgb(pixels, 26, 14, .56, .56, .56))
        return 42;
      rdctie(lo, hi) = r->GetMinMax(ids[9], {1, 0, 0}, CompType::Typeless);
      if(lo.intValue[0] != 100 || hi.intValue[0] != 112)
        return 43;
      display = TextureDisplay();
      display.resourceId = ids[0];
      display.scale = 4;
      display.alpha = true;
      display.backgroundColor = FloatVector(.25, .25, .25, 1);
      pixels = show(display);
      if(!rgb(pixels, 18, 10, .399414f, .399414f, .399414f))
        return 35;
      display.alpha = false;
      display.red = display.green = display.blue = false;
      display.alpha = true;
      pixels = show(display);
      if(!rgb(pixels, 18, 10, .5, .5, .5))
        return 36;
      display = TextureDisplay();
      display.resourceId = ids[0];
      display.scale = 4;
      display.decodeYUV = true;
      pixels = show(display);
      if(!rgb(pixels, 18, 10, .5, .5, .5))
        return 37;
      display.decodeYUV = false;
      display.green = display.blue = false;
      display.rangeMin = 1;
      display.rangeMax = 0;
      pixels = show(display);
      if(!rgb(pixels, 14, 10, .75, .75, .75))
        return 39;
      display.rangeMin = display.rangeMax = .5;
      pixels = show(display);
      if(!rgb(pixels, 14, 10, 0, 0, 0) || !rgb(pixels, 22, 10, 1, 1, 1))
        return 40;
      display.rangeMin = 0;
      display.rangeMax = 1;
      display.xOffset = 4;
      display.yOffset = 4;
      display.backgroundColor = FloatVector(.25, .25, .25, 1);
      pixels = show(display);
      if(!rgb(pixels, 2, 2, .25, .25, .25))
        return 41;
      auto *background =
          r->CreateOutput(CreateHeadlessWindowingData(128, 128), ReplayOutputType::Texture);
      display = TextureDisplay();
      display.resourceId = ids[0];
      display.xOffset = 200;
      background->SetTextureDisplay(display);
      background->Display();
      auto checks = background->ReadbackOutputTexture();
      if(checks.size() != 128 * 128 * 3 || !Near(checks[(10 * 128 + 10) * 3] / 255.0f, .57) ||
         !Near(checks[(10 * 128 + 74) * 3] / 255.0f, .81))
        return 38;
      background->Shutdown();
    }
    else
    {
      for(unsigned i = 0; i < 256; i++)
        printf("UE histogram[%u]=%u\n", i, histogram[i]);
      if(argv[4][0])
      {
        std::ofstream data(argv[4], std::ios::binary);
        data.write((const char *)before.data(), before.size());
      }
      printf(
          "PASS UE inspection resource=%s event=%u cycle=%u histogram_sum=%llu minR=%g maxR=%g\n",
          argv[3], event, cycle, (unsigned long long)sum, lo.floatValue[0], hi.floatValue[0]);
    }
    if(before != r->GetTextureData(ids[0], {0, 0, 0}))
      return 24;
    r->SetFrameEvent(0, true);
    if(!r->GetFatalErrorStatus().OK())
      return 25;
    printf(
        "PASS texture inspection cycle=%u statistics/overlays/range/gamma/native bytes unchanged\n",
        cycle);
  }
  output->Shutdown();
  r->Shutdown();
  RENDERDOC_ShutdownReplay();
  return 0;
}
