// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
template <> rdcstr DoStringise(const uint32_t &value) { char s[16]; snprintf(s,sizeof(s),"%u",value); return s; }
#include "renderdoc/api/replay/pipestate.inl"
REPLAY_PROGRAM_MARKER()
static void Events(const rdcarray<ActionDescription> &actions, rdcarray<uint32_t> &events) {
  for(const auto &a : actions) { if(a.flags & (ActionFlags::Drawcall | ActionFlags::MeshDispatch | ActionFlags::Dispatch)) events.push_back(a.eventId); Events(a.children,events); }
}
static const ActionDescription *Find(const rdcarray<ActionDescription> &actions, uint32_t eid) {
  for(const auto &a : actions) { if(a.eventId == eid) return &a; if(auto child = Find(a.children, eid)) return child; }
  return nullptr;
}
int main(int argc,char **argv) {
  if(argc != 3) return 2;
  const bool bindless = !strcmp(argv[2],"bindless"), ray = !strcmp(argv[2],"ray");
  GlobalEnvironment env; env.enumerateGPUs=false; RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile(); IReplayController *c=nullptr;
  auto result=file->OpenFile(argv[1],"rdc",nullptr); if(result.OK()) rdctie(result,c)=file->OpenCapture(ReplayOptions(),nullptr);
  file->Shutdown(); if(!result.OK() || !c) { fprintf(stderr,"Open failed: %s\n",result.internal_msg ? result.internal_msg->c_str() : "unknown"); return 3; }
  bool ok=true; rdcarray<uint32_t> events; Events(c->GetRootActions(),events); ResourceId images[3];
  if(bindless) {
    ok &= events.size()==2;
    for(const auto &r:c->GetResources()) for(unsigned i=0;i<3;i++) if(r.name==rdcstr("Stage image ")+char('0'+i)) images[i]=r.resourceId;
    for(unsigned i=0;i<3;i++) { auto uses=c->GetUsage(images[i]); printf("pre-visit image=%u uses=%zu\n",i,uses.size()); ok &= images[i]!=ResourceId() && uses.size()==(i==0?2U:i==1?1U:0U);
      for(const auto &use:uses) { printf("  eid=%u usage=%u\n",use.eventId,unsigned(use.usage)); ok &= i==0 ? (use.eventId==events[0] && use.usage==ResourceUsage::MS_RWResource)||(use.eventId==events[1] && use.usage==ResourceUsage::MS_Resource) : use.eventId==events[1] && use.usage==ResourceUsage::TS_RWResource; }
    }
  }
  unsigned meshShaders=0,taskShaders=0,asInputs=0,resources=0,samplers=0;
  for(unsigned cycle=0;cycle<2 && ok;cycle++) for(size_t event=0;event<events.size() && ok;event++) {
    c->SetFrameEvent(0,true); c->SetFrameEvent(events[event],true); ok &= c->GetFatalErrorStatus().OK(); const auto &pipe=c->GetPipelineState();
    for(ShaderStage stage:{ShaderStage::Task,ShaderStage::Mesh,ShaderStage::Compute,ShaderStage::Vertex,ShaderStage::Fragment}) {
      if(pipe.GetShader(stage)==ResourceId()) continue;
      auto reflection=pipe.GetShaderReflection(stage); ok &= reflection && reflection->stage==stage && !pipe.GetShaderEntryPoint(stage).empty();
      if(stage==ShaderStage::Mesh) meshShaders++; if(stage==ShaderStage::Task) taskShaders++;
      for(const auto &read:pipe.GetReadOnlyResources(stage,true)) { resources++; if(read.access.type==DescriptorType::AccelerationStructure) { asInputs++; bool used=false; for(const auto &use:c->GetUsage(read.descriptor.resource)) used |= use.eventId==events[event] && use.usage==ResUsage(uint32_t(stage)); ok &= read.descriptor.resource!=ResourceId() && used; } }
      samplers+=pipe.GetSamplers(stage,true).size();
    }
    if(bindless) {
      const auto writes=pipe.GetReadWriteResources(event ? ShaderStage::Task : ShaderStage::Mesh,true);
      unsigned matching=0; for(const auto &w:writes) if(w.access.type==DescriptorType::ReadWriteImage) { matching++; ok &= w.descriptor.resource==images[event]; }
      ok &= matching==1;
      if(event) { unsigned reads=0; for(const auto &r:pipe.GetReadOnlyResources(ShaderStage::Mesh,true)) if(r.access.type==DescriptorType::Image) { reads++; ok &= r.descriptor.resource==images[0]; } ok &= reads==1; }
      const auto action = Find(c->GetRootActions(), events[event]);
      if(!action || action->outputs[0] == ResourceId()) ok = false;
      else {
        const auto pixels = c->GetTextureData(action->outputs[0], {0,0,0});
        const byte expected = event ? 128 : 32;
        ok &= pixels.size() == 16;
        for(byte v : pixels) ok &= v == expected;
      }
      for(unsigned i=0;i<3;i++) { const auto data=c->GetTextureData(images[i],{0,0,0}); const float expected=i==0?0.375f:i==1 && event?0.625f:0.125f; float values[4]={}; if(data.size()==16) memcpy(values,data.data(),16); ok &= data.size()==16; for(float v:values) ok &= v==expected; }
    }
    printf("event=%u cycle=%u ok=%d\n",events[event],cycle,ok);
  }
  if(ray) ok &= asInputs>0; else ok &= meshShaders>0;
  printf("MeshShaders=%u TaskShaders=%u ASInputs=%u Resources=%u Samplers=%u\n",meshShaders,taskShaders,asInputs,resources,samplers);
  c->Shutdown(); RENDERDOC_ShutdownReplay(); if(ok) puts("PASS public shader stages, descriptors, usage and repeated event resets"); return ok?0:4;
}
