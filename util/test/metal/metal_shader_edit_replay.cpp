// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <set>
#include <map>
#include "renderdoc/api/replay/renderdoc_replay.h"
template <> rdcstr DoStringise(const uint32_t &v) { char s[16]; snprintf(s,sizeof(s),"%u",v); return s; }
#include "renderdoc/api/replay/pipestate.inl"
REPLAY_PROGRAM_MARKER()
static void Collect(const rdcarray<ActionDescription> &actions,rdcarray<const ActionDescription*> &out) {
  for(const auto &a:actions) { if(a.flags&(ActionFlags::Drawcall|ActionFlags::Dispatch|ActionFlags::MeshDispatch))out.push_back(&a); Collect(a.children,out); }
}
static std::map<ResourceId,bytebuf> readBaseline;
static unsigned readNumber=0;
static bytebuf Read(IReplayController *c,const ActionDescription *action,ShaderStage stage,bool inputs=false) {
  std::set<ResourceId> resources;
  for(auto target:action->outputs) if(target!=ResourceId()) resources.insert(target);
  for(const auto &rw:c->GetPipelineState().GetReadWriteResources(stage,true))
    if(rw.descriptor.resource!=ResourceId()) resources.insert(rw.descriptor.resource);
  // Include the real GBuffer inputs, not just the first colour output of a lighting draw.
  if(inputs) for(const auto &ro:c->GetPipelineState().GetReadOnlyResources(stage,true))
    for(const auto &t:c->GetTextures()) if(t.resourceId==ro.descriptor.resource)
      resources.insert(t.resourceId);
  bytebuf result; const unsigned sample=readNumber++;
  for(auto id:resources)
  {
    rdcstr name; for(const auto &r:c->GetResources())if(r.resourceId==id)name=r.name;
    for(const auto &b:c->GetBuffers()) if(b.resourceId==id)
    { auto data=c->GetBufferData(id,0,64); result.append(data);
      if(getenv("RENDERDOC_METAL_EDIT_TRACE"))
      { auto base=readBaseline.find(id); size_t diff=0; if(base!=readBaseline.end()){ while(diff<data.size() && diff<base->second.size() && data[diff]==base->second[diff])diff++; }
        printf("read sample=%u buffer=%s bytes=%zu firstDiff=%zu\n",sample,name.c_str(),data.size(),diff); if(base==readBaseline.end())readBaseline[id]=data; } }
    for(const auto &t:c->GetTextures()) if(t.resourceId==id)
    { auto data=c->GetTextureData(id,{0,0,0}); result.append(data);
      if(getenv("RENDERDOC_METAL_EDIT_TRACE"))
      { auto base=readBaseline.find(id); size_t diff=0; if(base!=readBaseline.end()){ while(diff<data.size() && diff<base->second.size() && data[diff]==base->second[diff])diff++; }
        printf("read sample=%u texture=%s bytes=%zu firstDiff=%zu\n",sample,name.c_str(),data.size(),diff);
        if(base!=readBaseline.end() && diff<data.size() && diff<base->second.size())
        { uint32_t before=0,after=0; size_t word=diff-diff%4; memcpy(&before,base->second.data()+word,4);memcpy(&after,data.data()+word,4);
          printf("  changed word offset=%zu before=%08x after=%08x\n",word,before,after); } if(base==readBaseline.end())readBaseline[id]=data; } }
  }
  return result;
}
int main(int argc,char **argv) {
  setvbuf(stdout,nullptr,_IONBF,0);
  if(argc<3)return 2; bool air=!strcmp(argv[2],"air"),ue=argc>3; bool ok=true;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto f=RENDERDOC_OpenCaptureFile();IReplayController *c=nullptr;
  auto result=f->OpenFile(argv[1],"rdc",nullptr);if(result.OK())rdctie(result,c)=f->OpenCapture(ReplayOptions(),nullptr);f->Shutdown();
  if(!result.OK()||!c)return 3;
  rdcarray<const ActionDescription*> actions;Collect(c->GetRootActions(),actions);unsigned tested=0;
  for(ShaderStage stage:{ShaderStage::Vertex,ShaderStage::Fragment,ShaderStage::Compute,ShaderStage::Task,ShaderStage::Mesh}) {
    if(ue && stage!=(atoi(argv[3])==3928?ShaderStage::Fragment:ShaderStage::Compute))continue;
    const ActionDescription *a=nullptr;
    for(auto action:actions) {
      if(ue && action->eventId!=unsigned(atoi(argv[3])))continue;
      c->SetFrameEvent(action->eventId,true);
      if(c->GetPipelineState().GetShader(stage)!=ResourceId()){a=action;break;}
    }
    if(!a)continue;
    auto refl=c->GetPipelineState().GetShaderReflection(stage); if(!refl){ok=false;break;}
    // The controller's pipeline pointers can change after ReplaceResource: own all editor inputs.
    ShaderReflection original=*refl; ResourceId id=original.resourceId;
    bytebuf baseline=Read(c,a,stage,!strcmp(argv[2],"control")); if(baseline.empty()){ok=false;break;}
    if(!strcmp(argv[2],"control"))
    {
      for(unsigned i=0;i<5;i++){ c->SetFrameEvent(0,true);c->SetFrameEvent(a->eventId,true);
        auto bytes=Read(c,a,stage,true); printf("control cycle=%u allResourcesIdentical=%d fatal=%d\n",i,bytes==baseline,c->GetFatalErrorStatus().OK()); }
      tested++;continue;
    }
    rdcstr text; ShaderEncoding encoding;
    if(air) {encoding=ShaderEncoding::MetalAIRAsm;text=c->DisassembleShader(ResourceId(),refl,"Metal AIR (editable)");}
    else {encoding=ShaderEncoding::MSL;if(original.debugInfo.files.empty()){ok=false;break;}text=original.debugInfo.files[original.debugInfo.editBaseFile<0?0:original.debugInfo.editBaseFile].contents;}
    if(ue) {char path[200];snprintf(path,sizeof(path),"build-macos-debug/metal-shader-edit/ue-%s-%s.ll",argv[3],stage==ShaderStage::Fragment?"fs":"cs"); if(FILE *o=fopen(path,"wb")){fwrite(text.data(),1,text.size(),o);fclose(o);} }
    const rdcstr entry=original.debugInfo.entrySourceName.empty()?original.entryPoint:original.debugInfo.entrySourceName;
    for(unsigned cycle=0;cycle<2 && ok;cycle++) {
      auto built=c->BuildTargetShader(entry,encoding,bytebuf((const byte*)text.data(),text.size()),original.debugInfo.compileFlags,stage);
      printf("build EID=%u stage=%u cycle=%u bytes=%zu success=%d errors=%s\n",a->eventId,unsigned(stage),cycle,text.size(),built.first!=ResourceId(),built.second.c_str());
      if(built.first==ResourceId()){ok=false;break;}
      c->ReplaceResource(id,built.first);c->SetFrameEvent(0,true);c->SetFrameEvent(a->eventId,true);
      ok &= c->GetFatalErrorStatus().OK() && Read(c,a,stage)==baseline &&
            c->GetPipelineState().GetShaderReflection(stage)->resourceId==built.first;
      printf("replace EID=%u stage=%u cycle=%u identical=%d\n",a->eventId,unsigned(stage),cycle,ok);
      // Invalid source must fail without disturbing the last successful replacement.
      auto bad=c->BuildTargetShader(entry,encoding,bytebuf((const byte*)"invalid shader",14),original.debugInfo.compileFlags,stage);
      ok &= bad.first==ResourceId() && !bad.second.empty() && c->GetFatalErrorStatus().OK();
      c->RemoveReplacement(id);c->SetFrameEvent(0,true);c->SetFrameEvent(a->eventId,true);
      ok &= c->GetFatalErrorStatus().OK() && Read(c,a,stage)==baseline && c->GetPipelineState().GetShader(stage)==id;
      c->FreeTargetResource(built.first);
    }
    tested++; if(!ok)break;
    if(!ue && air && stage==ShaderStage::Compute && text.contains("add i32 %1, 100"))
    {
      std::string edited(text.c_str());
      edited.replace(edited.find("add i32 %1, 100"),strlen("add i32 %1, 100"),"add i32 %1, 200");
      auto built=c->BuildTargetShader(entry,encoding,bytebuf((const byte*)edited.data(),edited.size()),original.debugInfo.compileFlags,stage);
      ok &= built.first!=ResourceId();
      if(built.first!=ResourceId())
      {
        c->ReplaceResource(id,built.first);
        auto bytes=Read(c,a,stage); uint32_t words[4]={}; if(bytes.size()==16)memcpy(words,bytes.data(),16);
        printf("AIR changed bytes=%zu words=%u,%u,%u,%u shaderApplied=%d fatal=%d\n",bytes.size(),words[0],words[1],words[2],words[3],c->GetPipelineState().GetShaderReflection(stage)->resourceId==built.first,c->GetFatalErrorStatus().OK());
        ok &= bytes.size()==16; for(unsigned i=0;i<4;i++)ok &= words[i]==200+i;
        c->FreeTargetResource(built.first); c->SetFrameEvent(a->eventId,true); ok &= Read(c,a,stage)==baseline;
        printf("edited AIR CS words 200..203 + restore=%d\n",ok);
      }
    }
    if(!ue && !strcmp(argv[2],"source") && stage==ShaderStage::Fragment) {
      const rdcstr changed="#include <metal_stdlib>\nusing namespace metal; fragment float4 fs() {return float4(1,0,0,1);}";
      auto replacement=c->BuildTargetShader("fs",ShaderEncoding::MSL,bytebuf((const byte*)changed.data(),changed.size()),original.debugInfo.compileFlags,stage);
      ok &= replacement.first!=ResourceId();
      if(replacement.first!=ResourceId()) {
        c->ReplaceResource(id,replacement.first);
        auto pixels=Read(c,a,stage); ok &= pixels!=baseline && pixels.size()==256;
        for(size_t i=0;i<pixels.size();i++)ok &= pixels[i]==(i%4==0||i%4==3?255:0);
        // Valid MSL that cannot link to the captured render-target layout must also
        // fail transactionally, after library/function compilation has succeeded.
        const ShaderReflection vertex=*c->GetPipelineState().GetShaderReflection(ShaderStage::Vertex);
        const rdcstr incompatible="#include <metal_stdlib>\nusing namespace metal; struct I {float4 p [[attribute(0)]];}; vertex float4 vs(I v [[stage_in]]){return v.p;}";
        auto failed=c->BuildTargetShader("vs",ShaderEncoding::MSL,
            bytebuf((const byte*)incompatible.data(),incompatible.size()),vertex.debugInfo.compileFlags,ShaderStage::Vertex);
        ok &= failed.first==ResourceId() && !failed.second.empty() && Read(c,a,stage)==pixels;
        printf("incompatible PSO preserves last good edit=%d errors=%s\n",ok,failed.second.c_str());
        // Both pipeline states using the captured function must change.
        for(auto other:actions)if(other->outputs[0]==a->outputs[0] && other->eventId>a->eventId) {
          c->SetFrameEvent(other->eventId,true);ok &= Read(c,other,stage)==pixels;
        }
        c->FreeTargetResource(replacement.first);c->SetFrameEvent(a->eventId,true);
        ok &= Read(c,a,stage)==baseline && c->GetPipelineState().GetShader(stage)==id;
        printf("edited pixels and all dependent PSOs + active free restores original=%d\n",ok);
      }
    }
  }
  printf("%s shader edit stages=%u mode=%s UE=%d\n",ok&&tested?"PASS":"FAIL",tested,argv[2],ue);
  c->Shutdown();RENDERDOC_ShutdownReplay();return ok&&tested?0:4;
}
