// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Draw(const rdcarray<ActionDescription> &actions,uint32_t &event)
{
  for(const auto &a:actions)
  {
    if(a.flags&ActionFlags::Drawcall)event=a.eventId;
    Draw(a.children,event);
  }
}
int main(int argc,char **argv)
{
  if(argc!=2)return 2;
  GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr>args;args.push_back(argv[0]);
  RENDERDOC_InitialiseReplay(env,args);
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
  IReplayController *c=nullptr;if(!result.OK())return 3;
  rdctie(result,c)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK()||!c){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
  c->SetFrameEvent(0,true);rdcarray<ResourceId>volumes;
  for(const auto&t:c->GetTextures())
    if(t.width==4&&t.height==4&&t.depth==4&&c->GetTextureData(t.resourceId,{0,0,0}).empty())volumes.push_back(t.resourceId);
  if(volumes.size()!=2)return 5;
  uint32_t draw=0;Draw(c->GetRootActions(),draw);if(!draw)return 6;
  auto output=c->CreateOutput(CreateHeadlessWindowingData(4,4),ReplayOutputType::Texture);if(!output)return 7;
  for(unsigned cycle=0;cycle<2;cycle++)
  {
    c->SetFrameEvent(draw,true);
    for(unsigned target=0;target<2;target++)for(unsigned slice=0;slice<4;slice++)
    {
      TextureDisplay cfg;cfg.resourceId=volumes[target];cfg.subresource={0,slice,0};
      cfg.scale=1.0f;cfg.xOffset=cfg.yOffset=0;cfg.red=cfg.green=cfg.blue=true;cfg.alpha=false;
      cfg.rangeMin=0;cfg.rangeMax=4;cfg.rawOutput=false;
      output->SetTextureDisplay(cfg);output->Display();auto pixels=output->ReadbackOutputTexture();
      if(pixels.size()!=4*4*3)return 8;
      float source[]={0.25f,0.5f,0.75f};source[target]=float(slice+1);
      for(unsigned p=0;p<16;p++)for(unsigned channel=0;channel<3;channel++)
      {
        const float expected=source[channel]/4*255;
        if(fabsf(float(pixels[p*3+channel])-expected)>1.0f)
        {
          fprintf(stderr,"Volume display mismatch cycle=%u target=%u slice=%u pixel=%u channel=%u got=%u expected=%f\n",cycle,target,slice,p,channel,pixels[p*3+channel],expected);
          return 9;
        }
      }
      printf("PASS Native volume display cycle=%u target=%u slice=%u RGB=%u,%u,%u all16pixels\n",cycle,target,slice,pixels[0],pixels[1],pixels[2]);
    }
    c->SetFrameEvent(0,true);
  }
  output->Shutdown();c->Shutdown();RENDERDOC_ShutdownReplay();return 0;
}
