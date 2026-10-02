// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Find(const rdcarray<ActionDescription> &actions,uint32_t &copy)
{for(const auto &a:actions){if(a.flags&ActionFlags::Copy)copy=a.eventId;Find(a.children,copy);}}
int main(int argc,char **argv)
{
  if(argc!=3)return 2;
  GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
  ICaptureFile *file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);IReplayController *controller=nullptr;
  if(!result.OK())return 3;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK()||!controller){fprintf(stderr,"Open failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
  ResourceId output,texture;for(const auto &b:controller->GetBuffers())if(b.length==512)output=b.resourceId;
  for(const auto &t:controller->GetTextures())if(t.width==2&&t.height==2&&t.format.compType==CompType::UNorm&&t.format.BGRAOrder()==false)texture=t.resourceId;
  uint32_t copy=0;Find(controller->GetRootActions(),copy);if(!copy||output==ResourceId()||texture==ResourceId())return 5;
  const int red=atoi(argv[2]);
  for(int cycle=0;cycle<6;cycle++)
  {
    controller->SetFrameEvent(0,true);
    controller->SetFrameEvent(copy,true);const auto bytes=controller->GetBufferData(output,0,512);
    if(bytes.size()!=512)return 6;
    for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++)
    {const auto p=bytes.data()+y*256+x*4;if(p[0]!=red||p[1]!=64||p[2]!=128||p[3]!=255)return 7;}
    const auto pixels=controller->GetTextureData(texture,{0,0,0});if(pixels.size()!=16)return 8;
    for(unsigned i=0;i<16;i+=4)if(pixels[i]!=red||pixels[i+1]!=64||pixels[i+2]!=128||pixels[i+3]!=255)return 9;
  }
  controller->Shutdown();RENDERDOC_ShutdownReplay();puts("PASS frame texture birth/recreation, exact heap offset, GPU clear/copy pixels, six seeks");return 0;
}
