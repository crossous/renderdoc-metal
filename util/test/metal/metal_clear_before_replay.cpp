// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Find(const rdcarray<ActionDescription> &a,uint32_t &eid)
{for(const auto &x:a){if(x.flags & ActionFlags::Drawcall)eid=x.eventId;Find(x.children,eid);}}
static ResourceId ID(uint64_t n){ResourceId id;memcpy(&id,&n,sizeof(id));return id;}
static bytebuf Read(const std::string &path)
{
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  bytebuf data;
  if(!f || f.tellg() < 0) return data;
  data.resize(size_t(f.tellg()));
  f.seekg(0);
  if(!f.read((char *)data.data(), data.size())) data.clear();
  return data;
}
int main(int argc,char **argv)
{
  if(argc!=3)return 2;setvbuf(stdout,nullptr,_IOLBF,0);
  const bool ueDepth=!strncmp(argv[2],"ue-depth:",9);
  const bool ue=ueDepth || (argv[2][0]>='0' && argv[2][0]<='9');
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
  if(!result.OK())return 3;IReplayController *r=nullptr;
  rdctie(result,r)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();if(!result.OK() || !r)return 4;
  uint32_t eid=0;Find(r->GetRootActions(),eid);if(ue)eid=strtoul(argv[2]+(ueDepth?9:0),nullptr,10);
  r->SetFrameEvent(eid,true);ResourceId colors[2],depth;
  if(ue)
  {
    const auto pipe=r->GetPipelineState().GetMetalPipelineState();
    colors[0]=ueDepth ? pipe->depthTarget.resource : pipe->colorTargets[0].resource;
    if(ueDepth && pipe->depthStencil.depthFunction!=CompareFunction::AlwaysTrue) return 17;
  }
  else for(const auto &res:r->GetResources())
  {
    if(res.name=="Clear original MRT 0")colors[0]=res.resourceId;
    if(res.name=="Clear original MRT 1")colors[1]=res.resourceId;
    if(res.name=="Clear original depth")depth=res.resourceId;
  }
  if(colors[0]==ResourceId() || (!ue && depth==ResourceId()))return 5;
  std::map<ResourceId,bytebuf> baseline,inputs;
  for(auto id:colors)if(id!=ResourceId())baseline[id]=r->GetTextureData(id,{0,0,0});
  if(ue && !ueDepth)for(uint64_t n:{12323ULL,12331ULL,12397ULL})inputs[ID(n)]=r->GetTextureData(ID(n),{0,0,0});
  if(!ue) for(unsigned i=0;i<2;i++)
    if(baseline[colors[i]]!=Read(std::string(argv[2])+"/original-"+std::to_string(i)+".bin"))return 6;
  auto output=r->CreateOutput(CreateHeadlessWindowingData(32,16),ReplayOutputType::Texture);
  for(unsigned cycle=0;cycle<2;cycle++)
    for(bool depthSelected:{false,true})
    {
      if(ue && depthSelected)continue;
      for(DebugOverlay overlay:{DebugOverlay::ClearBeforeDraw,DebugOverlay::ClearBeforePass})
      {
        TextureDisplay display;display.resourceId=depthSelected?depth:colors[0];display.overlay=overlay;
        display.backgroundColor={.2f,.3f,.4f,1};
        output->SetTextureDisplay(display);
        ResourceId image=output->GetDebugOverlayTexID();
        if(image==ResourceId() || !r->GetFatalErrorStatus().OK())return 7;
        output->Display(); // the shared viewer displays the modified ORIGINAL target
        auto mask=r->GetTextureData(image,{0,0,0});
        if(mask.empty())return 8;for(auto b:mask)if(b)return 9;
        const std::string kind=std::string(overlay==DebugOverlay::ClearBeforeDraw ? "draw" : "pass")+
                              (depthSelected ? "-depth" : "-colour");
        for(unsigned i=0;i<(ue?1U:2U);i++)
        {
          auto pixels=r->GetTextureData(colors[i],{0,0,0});if(pixels.empty())return 10;
          if(!ue && pixels!=Read(std::string(argv[2])+"/"+kind+"-"+std::to_string(i)+".bin"))
          {fprintf(stderr,"Native mismatch %s MRT %u\n",kind.c_str(),i);return 11;}
          if(ue && !ueDepth && pixels==baseline[colors[i]])return 12;
          // This depth-only UE event uses Always and covers the entire target.
          // Vulkan intentionally skips depth clear for Always; both modes
          // must reproduce exactly the same original depth output.
          if(ueDepth && pixels!=baseline[colors[i]])return 18;
          if(ue)if(const char *dir=getenv("RENDERDOC_METAL_CLEAR_DUMP_DIR"))
          {std::ofstream f(std::string(dir)+"/ue-"+kind+"-"+std::to_string(cycle)+".bin",std::ios::binary);f.write((const char *)pixels.data(),pixels.size());}
        }
        for(const auto &input:inputs)if(input.second.empty() || r->GetTextureData(input.first,{0,0,0})!=input.second)return 13;
        display.overlay=DebugOverlay::NoOverlay;display.resourceId=colors[0];
        output->SetTextureDisplay(display);output->Display(); // common ForceOverlayRefresh, no manual seek
        for(const auto &original:baseline)if(r->GetTextureData(original.first,{0,0,0})!=original.second)return 14;
        for(const auto &input:inputs)if(r->GetTextureData(input.first,{0,0,0})!=input.second)return 15;
        if(!r->GetFatalErrorStatus().OK())return 16;
        printf("PASS cycle=%u EID=%u %s original-shader MRT result %s; transparent overlay; None restores original bytes\n",
          cycle,eid,kind.c_str(),ue ? (ueDepth ? "equals original Always depth; no colour attachments" : "changed with GBuffer/depth unchanged") : "equals Native reference");
      }
    }
  output->Shutdown();r->Shutdown();RENDERDOC_ShutdownReplay();return 0;
}
