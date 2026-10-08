// SPDX-License-Identifier: MIT
// Test-only resource timeline; usage entries locate producer candidates, never
// establish actual shader access or authorize a backend capability.
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <map>
REPLAY_PROGRAM_MARKER()
static uint64_t Number(ResourceId id)
{
  uint64_t value = 0; static_assert(sizeof(id) == sizeof(value), "ResourceId representation");
  memcpy(&value, &id, sizeof(value)); return value;
}
static void Events(const rdcarray<ActionDescription> &actions, const SDFile &file,
                   std::map<uint32_t, const SDChunk *> &chunks)
{
  for(const auto &action : actions)
  {
    for(const auto &event : action.events)
      if(event.chunkIndex < file.chunks.size()) chunks[event.eventId] = file.chunks[event.chunkIndex];
    Events(action.children, file, chunks);
  }
}
int main(int argc, char **argv)
{
  if(argc != 4) return 2;
  FILE *input = fopen(argv[3], "r"); if(!input) return 2;
  rdcarray<ResourceId> resources; unsigned long long raw;
  while(fscanf(input, "%llu", &raw) == 1)
  { ResourceId id; memcpy(&id, &raw, sizeof(raw)); resources.push_back(id); }
  fclose(input); if(resources.empty() || resources.size() > 32) return 2;
  GlobalEnvironment env; env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {argv[0]});
  auto capture = RENDERDOC_OpenCaptureFile(); auto result = capture->OpenFile(argv[1], "rdc", nullptr);
  IReplayController *controller = nullptr;
  if(result.OK()) rdctie(result, controller) = capture->OpenCapture(ReplayOptions(), nullptr);
  capture->Shutdown();
  if(!result.OK() || !controller) { RENDERDOC_ShutdownReplay(); return 4; }
  FILE *out = fopen(argv[2], "w"); if(!out) { controller->Shutdown(); RENDERDOC_ShutdownReplay(); return 5; }
  std::map<uint32_t, const SDChunk *> events;
  const auto &file = controller->GetStructuredFile(); Events(controller->GetRootActions(), file, events);
  std::map<ResourceId, ResourceId> pipelineForEncoder;
  std::map<const SDChunk *, ResourceId> pipelineForChunk;
  for(const SDChunk *chunk : file.chunks)
  {
    auto encoder = chunk->FindChild("ComputeCommandEncoder");
    if(!encoder) encoder = chunk->FindChild("RenderCommandEncoder");
    if(!encoder) continue;
    const ResourceId id = encoder->AsResourceId();
    if(chunk->name == "MTLComputeCommandEncoder::setComputePipelineState")
      pipelineForEncoder[id] = chunk->FindChild("pipeline")->AsResourceId();
    if(chunk->name == "MTLRenderCommandEncoder::setRenderPipelineState")
      pipelineForEncoder[id] = chunk->FindChild("pipelineState")->AsResourceId();
    pipelineForChunk[chunk] = pipelineForEncoder[id];
  }
  for(ResourceId resource : resources)
    for(const auto &usage : controller->GetUsage(resource))
    {
      const auto found = events.find(usage.eventId);
      const SDChunk *chunk = found != events.end() ? found->second : nullptr;
      fprintf(out, "USAGE resource=%llu event=%u usage=%u pipeline=%llu chunk=%s\n",
              (unsigned long long)Number(resource), usage.eventId, unsigned(usage.usage),
              (unsigned long long)Number(chunk ? pipelineForChunk[chunk] : ResourceId()),
              chunk ? chunk->name.c_str() : "unknown");
    }
  fclose(out); controller->Shutdown(); RENDERDOC_ShutdownReplay();
  puts("PASS recorded usage timeline; candidates require API/AIR dataflow confirmation"); return 0;
}
