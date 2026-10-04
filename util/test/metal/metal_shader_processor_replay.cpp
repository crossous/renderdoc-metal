// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <set>
#include "renderdoc/api/replay/renderdoc_replay.h"
template <> rdcstr DoStringise(const uint32_t &v) { char s[16]; snprintf(s,sizeof(s),"%u",v); return s; }
#include "renderdoc/api/replay/pipestate.inl"
REPLAY_PROGRAM_MARKER()
static uint32_t FirstDraw(const rdcarray<ActionDescription> &actions)
{
  for(const auto &a : actions)
  {
    if(a.flags & ActionFlags::Drawcall) return a.eventId;
    uint32_t event = FirstDraw(a.children); if(event) return event;
  }
  return 0;
}
static bytebuf Read(IReplayController *c, ShaderStage stage)
{
  std::set<ResourceId> resources;
  for(const auto &o : c->GetPipelineState().GetOutputTargets())
    if(o.resource != ResourceId()) resources.insert(o.resource);
  for(const auto &rw : c->GetPipelineState().GetReadWriteResources(stage, true))
    if(rw.descriptor.resource != ResourceId()) resources.insert(rw.descriptor.resource);
  bytebuf ret;
  for(auto id : resources)
  {
    for(const auto &texture : c->GetTextures()) if(texture.resourceId == id)
      ret.append(c->GetTextureData(id, {}));
    for(const auto &buffer : c->GetBuffers()) if(buffer.resourceId == id)
      ret.append(c->GetBufferData(id, 0, 64));
  }
  return ret;
}
int main(int argc, char **argv)
{
  if(argc < 4) return 2;
  setvbuf(stdout, nullptr, _IONBF, 0);
  GlobalEnvironment env; env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {argv[0]});
  IReplayController *c = nullptr;
  auto capture = RENDERDOC_OpenCaptureFile();
  printf("opening=%s\n", argv[1]);
  auto result = capture->OpenFile(argv[1], "rdc", nullptr);
  if(result.OK()) rdctie(result,c) = capture->OpenCapture(ReplayOptions(), nullptr);
  capture->Shutdown();
  if(!result.OK() || !c)
  {
    printf("OpenCapture failed: %s\n", result.internal_msg ? result.internal_msg->c_str() : "no controller");
    if(c) c->Shutdown();
    RENDERDOC_ShutdownReplay();
    return 3;
  }
  uint32_t event = argc > 4 ? atoi(argv[4]) : FirstDraw(c->GetRootActions());
  ShaderStage stage = argc > 5 && !strcmp(argv[5], "cs") ? ShaderStage::Compute : ShaderStage::Fragment;
  c->SetFrameEvent(event, true);
  ShaderReflection original = *c->GetPipelineState().GetShaderReflection(stage);
  if(argc > 6)
  {
    std::ofstream dump(argv[6], std::ios::binary);
    dump.write((const char *)original.rawBytes.data(), original.rawBytes.size());
  }
  bytebuf baseline = Read(c, stage);
  std::ifstream file(argv[2], std::ios::binary);
  std::vector<char> bytes((std::istreambuf_iterator<char>(file)), {});
  bytebuf binary; binary.assign((const byte *)bytes.data(), bytes.size());
  auto flags = original.debugInfo.compileFlags;
  ResourceId edited; rdcstr errors;
  rdctie(edited, errors) = c->BuildTargetShader(argv[3], ShaderEncoding::MetalLib, binary, flags, stage);
  bool ok = edited != ResourceId() && errors.empty() && !baseline.empty();
  printf("event=%u compiled=%d errors=%s baselineBytes=%zu\n", event, ok, errors.c_str(), baseline.size());
  for(unsigned cycle = 0; ok && cycle < 2; ++cycle)
  {
    c->ReplaceResource(original.resourceId, edited);
    c->SetFrameEvent(0, true); c->SetFrameEvent(event, true);
    bool applied = c->GetPipelineState().GetShaderReflection(stage)->resourceId == edited;
    bool same = applied && Read(c, stage) == baseline;
    c->RemoveReplacement(original.resourceId);
    c->SetFrameEvent(0, true); c->SetFrameEvent(event, true);
    bool restored = Read(c, stage) == baseline;
    ok = same && restored && c->GetFatalErrorStatus().OK();
    printf("cycle=%u identical=%d restored=%d fatal=%d\n", cycle, same, restored, !c->GetFatalErrorStatus().OK());
  }
  if(edited != ResourceId()) c->FreeTargetResource(edited);
  c->Shutdown(); RENDERDOC_ShutdownReplay();
  printf("%s bundled processor replay\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
