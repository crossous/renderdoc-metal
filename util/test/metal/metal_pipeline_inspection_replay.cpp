// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
// clang-format off: the replay API declares the exported allocators used by pipestate types.
#include "renderdoc/api/replay/renderdoc_replay.h"
#include "renderdoc/api/replay/pipestate.h"
template <> rdcstr DoStringise(const uint32_t &value)
{
  char str[16];
  snprintf(str, sizeof(str), "%u", value);
  return str;
}
#include "renderdoc/api/replay/pipestate.inl"
// clang-format on
REPLAY_PROGRAM_MARKER()
static void Collect(const rdcarray<ActionDescription> &actions, rdcarray<uint32_t> &events)
{
  for(const auto &a : actions)
  {
    if(a.flags & ActionFlags::Drawcall)
      events.push_back(a.eventId);
    Collect(a.children, events);
  }
}
int main(int argc, char **argv)
{
  if(argc != 3)
    return 2;
  GlobalEnvironment env;
  env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {argv[0]});
  auto file = RENDERDOC_OpenCaptureFile();
  IReplayController *controller = nullptr;
  auto result = file->OpenFile(argv[1], "rdc", nullptr);
  if(result.OK())
    rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr);
  file->Shutdown();
  if(!result.OK() || !controller)
    return 3;
  rdcarray<uint32_t> events;
  Collect(controller->GetRootActions(), events);
  const bool dynamic = !strcmp(argv[2], "dynamic");
  const bool multiple = !strcmp(argv[2], "viewport-array");
  const bool reset = !strcmp(argv[2], "viewport-reset");
  bool ok = !events.empty();
  if((multiple || reset) && !events.empty())
    events = {events.back()};
  for(unsigned cycle = 0; cycle < 2; cycle++)
  {
    unsigned drawIndex = 0;
    for(auto eid : events)
    {
      controller->SetFrameEvent(0, true);
      controller->SetFrameEvent(eid, true);
      const auto &s = *controller->GetPipelineState().GetMetalPipelineState();
      if(multiple || reset)
      {
        const auto &pipe = controller->GetPipelineState();
        ok &= s.rasterizer.viewports.size() == (multiple ? 2U : 1U) &&
              s.rasterizer.scissors.size() == (multiple ? 2U : 1U) && pipe.GetViewport(0).x == 4 &&
              pipe.GetScissor(0).x == 12;
        if(multiple)
          ok &= pipe.GetViewport(1).enabled && pipe.GetViewport(1).width == 32 &&
                pipe.GetScissor(1).enabled && pipe.GetScissor(1).width == 32;
        else
          ok &= pipe.GetViewport(1).width == 0 && pipe.GetScissor(1).width == 0;
        printf("EID=%u cycle=%u viewports=%zu scissors=%zu reset=%d\n", eid, cycle,
               s.rasterizer.viewports.size(), s.rasterizer.scissors.size(), reset);
      }
      else if(dynamic)
      {
        ok &= !s.rasterizer.depthClip && s.rasterizer.fillMode == FillMode::Solid &&
              s.rasterizer.depthBias == 1.25f && s.rasterizer.slopeScaledDepthBias == 2.5f &&
              s.rasterizer.depthBiasClamp == 3.75f && s.rasterizer.viewports.size() == 1 &&
              s.rasterizer.scissors.size() == 1 && s.rasterizer.scissors[0].x == 64 &&
              s.rasterizer.scissors[0].y == 48;
        const float expected[] = {.2f, .4f, .6f, .8f};
        ok &= !memcmp(s.blendFactor, expected, sizeof(expected)) && s.patchControlPoints == 0;
        printf("EID=%u cycle=%u bias=%g slope=%g clamp=%g clip=%d blend=%g,%g,%g,%g\n", eid, cycle,
               s.rasterizer.depthBias, s.rasterizer.slopeScaledDepthBias,
               s.rasterizer.depthBiasClamp, s.rasterizer.depthClip, s.blendFactor[0],
               s.blendFactor[1], s.blendFactor[2], s.blendFactor[3]);
      }
      else
      {
        ok &= s.patchControlPoints == 3 && s.topology == Topology::PatchList_3CPs &&
              s.tessellationFactors.resourceId != ResourceId() &&
              s.tessellationFactors.byteOffset == 0 && s.tessellationFactors.byteSize == 256 &&
              s.tessellationInstanceStride == 0 && !s.tessellationPartitionMode.empty() &&
              !s.tessellationStepFunction.empty() && s.vertexShader.resourceId != ResourceId() &&
              s.maxTessellationFactor > 0 && s.rasterizer.viewports.size() == 1 &&
              s.rasterizer.scissors.size() == 1 && s.rasterizer.viewports[0].width == 400;
        if(events.size() == 1)
          ok &= s.rasterizer.scissors[0].x == 0 && s.rasterizer.scissors[0].width == 400;
        else
          ok &= s.rasterizer.scissors[0].x == int32_t(drawIndex * 133) &&
                s.rasterizer.scissors[0].width == (drawIndex == 2 ? 134 : 133);
        printf(
            "EID=%u cycle=%u controlPoints=%u factorBytes=%llu maxFactor=%u partition=%s step=%s\n",
            eid, cycle, s.patchControlPoints, (unsigned long long)s.tessellationFactors.byteSize,
            s.maxTessellationFactor, s.tessellationPartitionMode.c_str(),
            s.tessellationStepFunction.c_str());
      }
      ok &= controller->GetFatalErrorStatus().OK();
      drawIndex++;
    }
  }
  controller->Shutdown();
  RENDERDOC_ShutdownReplay();
  if(ok)
    puts("PASS captured pipeline inspection parameters after repeated event resets");
  return ok ? 0 : 4;
}
