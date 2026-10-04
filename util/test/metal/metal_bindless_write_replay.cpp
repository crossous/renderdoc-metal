// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
template <> rdcstr DoStringise(const uint32_t &value)
{ char text[16]; snprintf(text, sizeof(text), "%u", value); return text; }
#include "renderdoc/api/replay/pipestate.inl"
REPLAY_PROGRAM_MARKER()
static void Dispatches(const rdcarray<ActionDescription> &actions, rdcarray<uint32_t> &events)
{
  for(const auto &a : actions) { if(a.flags & ActionFlags::Dispatch) events.push_back(a.eventId); Dispatches(a.children, events); }
}
int main(int argc, char **argv)
{
  if(argc != 2) return 2;
  GlobalEnvironment env; env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {argv[0]});
  auto file = RENDERDOC_OpenCaptureFile(); IReplayController *c = nullptr;
  auto result = file->OpenFile(argv[1], "rdc", nullptr);
  if(result.OK()) rdctie(result, c) = file->OpenCapture(ReplayOptions(), nullptr);
  file->Shutdown(); if(!result.OK() || !c) return 3;
  ResourceId images[3];
  for(const auto &resource : c->GetResources())
    for(unsigned i = 0; i < 3; i++)
      if(resource.name == rdcstr("Bindless output ") + char('0'+i)) images[i] = resource.resourceId;
  rdcarray<uint32_t> events; Dispatches(c->GetRootActions(), events);
  bool ok = events.size() == 2;
  // Check before visiting the dispatches. Residency of the unused image must not invent usage.
  for(unsigned i = 0; i < 3 && ok; i++) {
    ok &= images[i] != ResourceId();
    auto uses = c->GetUsage(images[i]);
    printf("pre-visit image=%u identified=%d usage-count=%zu\n", i, images[i] != ResourceId(), uses.size());
    for(const auto &use : uses) printf("  event=%u type=%u\n", use.eventId, unsigned(use.usage));
    ok &= uses.size() == (i < 2 ? 1U : 0U);
    if(i < 2 && uses.size() == 1)
      ok &= uses[0].eventId == events[i] && uses[0].usage == ResourceUsage::CS_RWResource;
  }
  for(unsigned cycle = 0; cycle < 3 && ok; cycle++)
    for(unsigned i = 0; i < 2 && ok; i++) {
      c->SetFrameEvent(0, true); c->SetFrameEvent(events[i], true);
      ok &= c->GetFatalErrorStatus().OK();
      const auto &pipe = c->GetPipelineState();
      auto writes = pipe.GetReadWriteResources(ShaderStage::Compute, true);
      unsigned textures = 0;
      for(const auto &write : writes)
        if(write.access.type == DescriptorType::ReadWriteImage) { textures++; ok &= write.descriptor.resource == images[i]; }
      ok &= textures == 1;
      for(const auto &read : pipe.GetReadOnlyResources(ShaderStage::Compute, true))
        ok &= read.descriptor.resource != images[i];
      const float expected[4] = {0.25f, 0.5f, 0.75f, 1.0f};
      for(unsigned image = 0; image < 3; image++) {
        const auto bytes = c->GetTextureData(images[image], {0,0,0}); const float zero[4] = {};
        ok &= bytes.size() == 16 && !memcmp(bytes.data(), image <= i ? expected : zero, 16);
      }
      printf("dispatch=%u cycle=%u indexed writable outputs=%u ok=%d\n", events[i], cycle, textures, ok);
    }
  c->Shutdown(); RENDERDOC_ShutdownReplay();
  if(ok) puts("PASS pre-visit usage, changed inline pointer, merged SRV/UAV access, unrelated resident texture, six resets and exact GPU bytes");
  return ok ? 0 : 4;
}
