// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Find(const rdcarray<ActionDescription> &actions, uint32_t &draw)
{
  for(const auto &a : actions) { if(a.flags & ActionFlags::Drawcall) draw=a.eventId; Find(a.children,draw); }
}
static ResourceId ID(uint64_t n) { ResourceId id; memcpy(&id,&n,sizeof(id)); return id; }
static float Half(uint16_t h)
{
  unsigned exp=(h>>10)&31, mant=h&1023;
  return std::ldexp(exp ? float(mant+1024) : float(mant), exp ? int(exp)-25 : -24)*(h&0x8000 ? -1 : 1);
}
int main(int argc,char **argv)
{
  if(argc != 2 && argc != 3) return 2;
  setvbuf(stdout,nullptr,_IOLBF,0);
  if(const char *log=getenv("RENDERDOC_METAL_TEST_LOG")) RENDERDOC_SetDebugLogFile(log);
  GlobalEnvironment env; env.enumerateGPUs=false;
  RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();
  auto result=file->OpenFile(argv[1],"rdc",nullptr);
  IReplayController *r=nullptr;
  if(!result.OK()) return 3;
  rdctie(result,r)=file->OpenCapture(ReplayOptions(),nullptr); file->Shutdown();
  if(!result.OK() || !r) return 4;
  uint32_t event=0; Find(r->GetRootActions(),event);
  const bool cull=argc==3 && !strcmp(argv[2],"cull");
  const bool depthExport=argc==3 && !strncmp(argv[2],"depth-export",12);
  const bool greaterDepth=depthExport && !strcmp(argv[2],"depth-export-greater");
  const bool mrtDepth=depthExport && !strcmp(argv[2],"depth-export-mrt");
  const bool viewport=argc==3 && !strncmp(argv[2],"viewport",8);
  const bool viewportArray=viewport && !strcmp(argv[2],"viewport-array");
  const bool ue=argc==3 && !cull && !depthExport && !viewport;
  const bool ueDepth=ue && !strncmp(argv[2],"ue-depth:",9);
  if(ue) event=strtoul(argv[2]+(ueDepth?9:0),nullptr,10);
  r->SetFrameEvent(event,true);
  ResourceId color;
  if(ue)
  {
    const auto pipe=r->GetPipelineState().GetMetalPipelineState();
    color=ueDepth ? pipe->depthTarget.resource : pipe->colorTargets[0].resource;
  }
  else for(const auto &res:r->GetResources()) if(res.name==(mrtDepth ? "Clear original MRT 0" : "Overlay original colour")) color=res.resourceId;
  if(color==ResourceId()) return 5;
  TextureDescription td;
  for(const auto &tex:r->GetTextures()) if(tex.resourceId==color) td=tex;
  auto output=r->CreateOutput(CreateHeadlessWindowingData(td.width,td.height),ReplayOutputType::Texture);
  const bytebuf baseline=r->GetTextureData(color,{0,0,0});
  std::map<uint64_t,bytebuf> inputs;
  if(mrtDepth)
    for(const auto &res:r->GetResources())
      if(res.name=="Clear original MRT 1" || res.name=="Clear original depth")
      {
        uint64_t n=0;memcpy(&n,&res.resourceId,sizeof(n));
        inputs[n]=r->GetTextureData(res.resourceId,{0,0,0});
        if(inputs[n].empty()) return 11;
      }
  if(ue && !ueDepth)
    for(uint64_t id:{12323ULL,12331ULL,12397ULL})
    {
      inputs[id]=r->GetTextureData(ID(id),{0,0,0});
      if(inputs[id].empty()) return 11;
      if(const char *dir=getenv("RENDERDOC_METAL_OVERLAY_DUMP_DIR"))
      {
        std::ofstream file(std::string(dir)+"/ue-"+std::to_string(id)+".bin",std::ios::binary);
        file.write((const char *)inputs[id].data(),inputs[id].size());
      }
    }
  const unsigned cycles=getenv("RENDERDOC_METAL_OVERLAY_ONE_CYCLE") ? 1 : 2;
  for(unsigned cycle=0;cycle<cycles;cycle++)
  {
    r->SetFrameEvent(event,true);
    const std::vector<DebugOverlay> overlays = (depthExport || ueDepth) ? std::vector<DebugOverlay>{DebugOverlay::Depth} : viewport ? std::vector<DebugOverlay>{DebugOverlay::ViewportScissor} :
      std::vector<DebugOverlay>{DebugOverlay::Depth,DebugOverlay::Stencil,DebugOverlay::Drawcall,
                               DebugOverlay::BackfaceCull,DebugOverlay::Wireframe,DebugOverlay::ViewportScissor};
    for(DebugOverlay overlay:overlays)
    {
      TextureDisplay display; display.resourceId=color; display.overlay=overlay;
      output->SetTextureDisplay(display);
      ResourceId image=output->GetDebugOverlayTexID();
      if(viewportArray)
      {
        if(image!=ResourceId() || !r->GetFatalErrorStatus().OK() || r->GetTextureData(color,{0,0,0})!=baseline) return 16;
        printf("PASS multi viewport/scissor rejected without using only element zero, original unchanged cycle=%u\n",cycle);
        continue;
      }
      if(!r->GetFatalErrorStatus().OK() || image==ResourceId())
      {
        const auto status=r->GetFatalErrorStatus();
        fprintf(stderr,"overlay %u failed: image=%d fatal=%s\n",unsigned(overlay),image!=ResourceId(),
          status.internal_msg ? status.internal_msg->c_str() : "none"); return 6;
      }
      auto pixels=r->GetTextureData(image,{0,0,0});
      if(pixels.size()!=size_t(td.width)*td.height*8) return 7;
      if(viewport)
      {
        // Known Native viewport/scissor coordinates: transparent outside,
        // grey viewport border, white scissor border, actual red/green coverage
        // under Vulkan's blue annotation. Verify RGB AND its alpha blend factors.
        auto check=[&](unsigned x,unsigned y,float red,float green,float blue,float alpha) {
          auto p=(const uint16_t *)(pixels.data()+(y*td.width+x)*8);
          const float expected[4]={red,green,blue,alpha};
          for(unsigned c=0;c<4;c++) if(std::fabs(Half(p[c])-expected[c])>.002f)
          { fprintf(stderr,"viewport xy=%u,%u channel=%u got=%g expected=%g\n",x,y,c,Half(p[c]),expected[c]);return false; }
          return true;
        };
        if(!check(0,0,0,0,0,0) || !check(4,2,.1f,.1f,.1f,1) ||
           !check(12,4,1,1,1,1) || !check(8,7,.68f,.08f,.36f,.76f) ||
           !check(15,7,.08f,.68f,.36f,.76f)) return 15;
      }
      unsigned red=0,green=0,transparent=0;
      for(unsigned y=0;y<td.height;y++) for(unsigned x=0;x<td.width;x++)
      {
        auto p=(const uint16_t *)(pixels.data()+(y*td.width+x)*8);
        red+=p[0]==0x3c00 && p[1]==0; green+=p[0]==0 && p[1]==0x3c00; transparent+=p[3]==0;
        if(overlay==DebugOverlay::Wireframe && p[3]!=0 &&
           (p[0]!=0x3a46 || p[1]!=0x3c00 || p[2]!=0 || p[3]!=0x3c00)) return 13;
        if(!ue && (overlay==DebugOverlay::Depth || overlay==DebugOverlay::Stencil || (cull && overlay==DebugOverlay::BackfaceCull)))
        {
          bool pass=cull ? baseline[(y*td.width+x)*4]==255 : overlay==DebugOverlay::Depth ? (depthExport ? (!greaterDepth && (!mrtDepth || x<24)) : x>=16) : x<16;
          if(p[0]!=(pass?0:0x3c00) || p[1]!=(pass?0x3c00:0) || p[2]!=0 || p[3]!=0x3c00)
          { fprintf(stderr,"overlay=%u xy=%u,%u got=%04x,%04x,%04x,%04x\n",unsigned(overlay),x,y,p[0],p[1],p[2],p[3]); return 8; }
        }
      }
      if(!ue && overlay==DebugOverlay::Wireframe && (transparent==0 || transparent==td.width*td.height)) return 10;
      if(!r->GetFatalErrorStatus().OK() || r->GetTextureData(color,{0,0,0})!=baseline) return 9;
      for(const auto &input:inputs)
        if(r->GetTextureData(ID(input.first),{0,0,0})!=input.second) return 12;
      uint64_t number=0; memcpy(&number,&color,sizeof(number));
      printf("PASS cycle=%u EID=%u output=%llu overlay=%u red=%u green=%u transparent=%u original unchanged\n",
        cycle,event,(unsigned long long)number, unsigned(overlay),red,green,transparent);
    }
  }
  output->Shutdown(); r->Shutdown(); RENDERDOC_ShutdownReplay();
  return 0;
}
