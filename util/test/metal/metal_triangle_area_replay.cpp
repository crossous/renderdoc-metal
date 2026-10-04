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
  ResourceId target=pipe->colorTargets[0].resource;
  if(target==ResourceId()) return 5;
  TextureDescription td;for(const auto &t:r->GetTextures()) if(t.resourceId==target) td=t;
  auto output=r->CreateOutput(CreateHeadlessWindowingData(td.width,td.height),ReplayOutputType::Texture);
  std::vector<std::pair<ResourceId,bytebuf>> originals;
  originals.push_back({target,r->GetTextureData(target,{0,0,0})});
  if(!ue && pipe->depthTarget.resource!=ResourceId())
    originals.push_back({pipe->depthTarget.resource,r->GetTextureData(pipe->depthTarget.resource,{0,0,0})});
  if(ue) for(uint64_t id:{12323ULL,12331ULL,12397ULL})
  {ResourceId input;memcpy(&input,&id,sizeof(id));originals.push_back({input,r->GetTextureData(input,{0,0,0})});}
  for(const auto &v:originals) if(v.second.empty()) return 6;
  for(unsigned cycle=0;cycle<2;cycle++)
  {
    r->SetFrameEvent(event,true);
    bytebuf draw;
    for(DebugOverlay kind:{DebugOverlay::TriangleSizeDraw,DebugOverlay::TriangleSizePass})
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
        if(!std::isfinite(value) || value<0) return 9;
        if(p[0]!=p[1] || p[0]!=p[2] || (p[3]!=0 && p[3]!=0x3c00)) return 10;
        if(!ue)
        {
          const auto original=(const float *)(originals[0].second.data()+(y*td.width+x)*16);
          bool covered=original[3]!=0;
          if(mode==7 && kind==DebugOverlay::TriangleSizeDraw && original[0]!=2) covered=false;
          float expected=covered?original[0]:0;
          if(std::fabs(value-expected)>expected*.001+1e-4 || bool(p[3])!=covered)
          {fprintf(stderr,"triangle case%u kind%u xy%u,%u got%g alpha%g expected%g covered%d\n",mode,unsigned(kind),x,y,value,Half(p[3]),expected,covered);return 12;}
        }
        if(ue && kind==DebugOverlay::TriangleSizePass)
        {
          const auto d=(const uint16_t *)(draw.data()+(y*td.width+x)*8);
          if(d[3] && !p[3]) return 13;
        }
        positive+=value>0;largest=std::fmax(largest,value);
      }
      if(ue && !positive) return 14;
      output->Display();
      auto shown=output->ReadbackOutputTexture();
      if(shown.size()!=size_t(td.width)*td.height*3) return 17;
      // Compare shared trisize heatmap RGB bytes where known Native pixels draw.
      if(!ue)
        for(unsigned i=0;i<td.width*td.height;i++)
        {
          const auto p=(const uint16_t *)(pixels.data()+i*8);
          if(!p[3]) continue;
          bool small=Half(p[0])==2;
          if(std::abs(int(shown[i*3])-64)>1 || std::abs(int(shown[i*3+1])-int(small?255:0))>1 ||
             std::abs(int(shown[i*3+2])-64)>1)
          {fprintf(stderr,"triangle heatmap case%u kind%u pixel%u got%u,%u,%u area%g\n",mode,unsigned(kind),i,shown[i*3],shown[i*3+1],shown[i*3+2],Half(p[0]));return 18;}
        }
      if(kind==DebugOverlay::TriangleSizeDraw) draw=pixels;
      for(const auto &v:originals) if(r->GetTextureData(v.first,{0,0,0})!=v.second) return 15;
      display.overlay=DebugOverlay::NoOverlay;output->SetTextureDisplay(display);output->Display();
      for(const auto &v:originals) if(r->GetTextureData(v.first,{0,0,0})!=v.second) return 16;
      printf("PASS triangle cycle=%u EID=%u kind=%u positive=%u max=%g projected-area/heatmap/original bytes/None restore\n",cycle,event,unsigned(kind),positive,largest);
    }
  }
  output->Shutdown();r->Shutdown();RENDERDOC_ShutdownReplay();return 0;
}
