// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static float Half(uint16_t h)
{
  unsigned exp=(h>>10)&31, mant=h&1023;
  return std::ldexp(exp ? float(mant+1024) : float(mant), exp ? int(exp)-25 : -24)*(h&0x8000 ? -1 : 1);
}
static void LastDraw(const rdcarray<ActionDescription> &actions,uint32_t &eid)
{
  for(const auto &a:actions) {if(a.flags&ActionFlags::Drawcall) eid=a.eventId;LastDraw(a.children,eid);}
}
int main(int argc,char **argv)
{
  if(argc!=3) return 2;
  setvbuf(stdout,nullptr,_IOLBF,0);
  const bool ue=!strncmp(argv[2],"ue:",3);
  unsigned mode=ue?0:unsigned(strtoul(argv[2],nullptr,10));
  if(!ue && mode>10) return 2;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto status=file->OpenFile(argv[1],"rdc",nullptr);
  if(!status.OK()) return 3;
  IReplayController *r=nullptr;rdctie(status,r)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!status.OK() || !r) return 4;
  uint32_t event=0;LastDraw(r->GetRootActions(),event);if(ue) event=strtoul(argv[2]+3,nullptr,10);
  r->SetFrameEvent(event,true);
  auto pipe=r->GetPipelineState().GetMetalPipelineState();
  ResourceId target=ue?pipe->colorTargets[0].resource:pipe->depthTarget.resource;
  if(target==ResourceId()) return 5;
  TextureDescription td;for(const auto &t:r->GetTextures()) if(t.resourceId==target) td=t;
  auto output=r->CreateOutput(CreateHeadlessWindowingData(td.width,td.height),ReplayOutputType::Texture);
  std::vector<std::pair<ResourceId,bytebuf>> originals;
  originals.push_back({target,r->GetTextureData(target,{0,0,0})});
  if(ue) for(uint64_t id:{12323ULL,12331ULL,12397ULL})
  {ResourceId input;memcpy(&input,&id,sizeof(id));originals.push_back({input,r->GetTextureData(input,{0,0,0})});}
  for(const auto &v:originals) if(v.second.empty()) return 6;
  for(unsigned cycle=0;cycle<2;cycle++)
  {
    r->SetFrameEvent(event,true);
    bytebuf draw;
    for(DebugOverlay kind:{DebugOverlay::QuadOverdrawDraw,DebugOverlay::QuadOverdrawPass})
    {
      TextureDisplay display;display.resourceId=target;display.overlay=kind;output->SetTextureDisplay(display);
      auto image=output->GetDebugOverlayTexID();
      if(image==ResourceId() || !r->GetFatalErrorStatus().OK()) return 7;
      auto pixels=r->GetTextureData(image,{0,0,0});
      if(pixels.size()!=size_t(td.width)*td.height*8) return 8;
      unsigned positive=0;float largest=0;
      for(unsigned y=0;y<td.height;y++) for(unsigned x=0;x<td.width;x++)
      {
        const auto p=(const uint16_t *)(pixels.data()+(y*td.width+x)*8);
        float value=Half(p[0]);
        if(!std::isfinite(value) || value<0 || value!=std::floor(value)) return 9;
        for(unsigned c=1;c<4;c++) if(p[c]!=p[0]) return 10;
        const auto q=(const uint16_t *)(pixels.data()+((y&~1U)*td.width+(x&~1U))*8);
        if(p[0]!=q[0]) return 11;
        if(!ue)
        {
          float expected=mode==8?0:((mode==4 || mode==5) && kind==DebugOverlay::QuadOverdrawPass?2:1);
          if(value!=expected) {fprintf(stderr,"case%u kind%u xy%u,%u got%g expected%g\n",mode,unsigned(kind),x,y,value,expected);return 12;}
        }
        if(ue && kind==DebugOverlay::QuadOverdrawPass)
        {
          const auto d=(const uint16_t *)(draw.data()+(y*td.width+x)*8);
          if(value<Half(d[0])) return 13;
        }
        positive+=value>0;largest=std::fmax(largest,value);
      }
      if(ue && !positive) return 14;
      output->Display();
      auto shown=output->ReadbackOutputTexture();
      if(shown.size()!=size_t(td.width)*td.height*3) return 17;
      // Shared colorRamp buckets 1 and 2, preserving Vulkan UI colour bytes.
      if(!ue && mode!=8)
      {
        unsigned count=(mode==4 || mode==5) && kind==DebugOverlay::QuadOverdrawPass?2:1;
        for(unsigned i=0;i<td.width*td.height;i++)
          if(std::abs(int(shown[i*3])-64)>1 || shown[i*3+1]!=0 ||
             std::abs(int(shown[i*3+2])-int(count==1?64:192))>1)
          {fprintf(stderr,"heatmap case%u kind%u xy%u got%u,%u,%u count%u\n",mode,unsigned(kind),i,shown[i*3],shown[i*3+1],shown[i*3+2],count);return 18;}
      }
      if(kind==DebugOverlay::QuadOverdrawDraw) draw=pixels;
      for(const auto &v:originals) if(r->GetTextureData(v.first,{0,0,0})!=v.second) return 15;
      display.overlay=DebugOverlay::NoOverlay;output->SetTextureDisplay(display);output->Display();
      for(const auto &v:originals) if(r->GetTextureData(v.first,{0,0,0})!=v.second) return 16;
      printf("PASS quad cycle=%u EID=%u kind=%u positive=%u max=%g raw-count/quad uniformity/original bytes/None restore\n",cycle,event,unsigned(kind),positive,largest);
    }
  }
  output->Shutdown();r->Shutdown();RENDERDOC_ShutdownReplay();return 0;
}
