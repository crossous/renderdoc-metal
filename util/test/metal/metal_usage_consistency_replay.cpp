// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
template <> rdcstr DoStringise(const uint32_t &value)
{ char text[16]; snprintf(text, sizeof(text), "%u", value); return text; }
#include "renderdoc/api/replay/pipestate.inl"
REPLAY_PROGRAM_MARKER()

int main(int argc, char **argv)
{
  if(argc != 3 && argc != 4) return 2;
  setvbuf(stdout, nullptr, _IOLBF, 0);
  uint64_t value = strtoull(argv[2], nullptr, 10); ResourceId resource;
  memcpy(&resource, &value, sizeof(resource));
  GlobalEnvironment env; env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {argv[0]});
  auto file = RENDERDOC_OpenCaptureFile(); IReplayController *controller = nullptr;
  auto result = file->OpenFile(argv[1], "rdc", nullptr);
  if(result.OK()) rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr);
  file->Shutdown(); if(!result.OK() || !controller) return 3;
  const auto initial = controller->GetUsage(resource);
  bool ok = !initial.empty();
  unsigned checked = 0;
  for(const auto &use : initial)
  {
    ShaderStage stage; bool write = false;
    switch(use.usage)
    {
      case ResourceUsage::CS_RWResource: write = true; stage = ShaderStage::Compute; break;
      case ResourceUsage::CS_Resource: stage = ShaderStage::Compute; break;
      case ResourceUsage::PS_RWResource: write = true; stage = ShaderStage::Fragment; break;
      case ResourceUsage::PS_Resource: stage = ShaderStage::Fragment; break;
      case ResourceUsage::VS_RWResource: write = true; stage = ShaderStage::Vertex; break;
      case ResourceUsage::VS_Resource: stage = ShaderStage::Vertex; break;
      default: continue;
    }
    controller->SetFrameEvent(0, true); controller->SetFrameEvent(use.eventId, true);
    const auto &pipe = controller->GetPipelineState();
    const auto descriptors = write ? pipe.GetReadWriteResources(stage, true)
                                   : pipe.GetReadOnlyResources(stage, true);
    bool found = false;
    for(const auto &binding : descriptors) found |= binding.descriptor.resource == resource;
    ok &= found && controller->GetFatalErrorStatus().OK(); checked++;
    printf("usage EID=%u stage=%u write=%d selected-descriptor-agrees=%d\n",
           use.eventId, unsigned(stage), write, found);
  }
  if(argc == 4)
  {
    const uint32_t excluded = strtoul(argv[3], nullptr, 10);
    controller->SetFrameEvent(0, true); controller->SetFrameEvent(excluded, true);
    bool found = false;
    for(ShaderStage stage : {ShaderStage::Compute, ShaderStage::Vertex, ShaderStage::Fragment})
    {
      for(const auto &binding : controller->GetPipelineState().GetReadOnlyResources(stage, true))
        found |= binding.descriptor.resource == resource;
      for(const auto &binding : controller->GetPipelineState().GetReadWriteResources(stage, true))
        found |= binding.descriptor.resource == resource;
    }
    for(const auto &use : controller->GetUsage(resource)) found |= use.eventId == excluded;
    ok &= !found && controller->GetFatalErrorStatus().OK();
    printf("excluded EID=%u absent-from-descriptors-and-usage=%d\n", excluded, !found);
  }
  const bool stable = controller->GetUsage(resource) == initial;
  ok &= stable;
  printf("%s whole-frame Usage/selected shader descriptor consistency: usages=%zu shader-events=%u stable-after-visits=%d\n",
         ok ? "PASS" : "FAIL", initial.size(), checked, stable);
  controller->Shutdown(); RENDERDOC_ShutdownReplay();
  return ok ? 0 : 4;
}
