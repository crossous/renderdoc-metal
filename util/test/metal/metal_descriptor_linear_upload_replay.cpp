// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Last(const rdcarray<ActionDescription>&as,uint32_t&last){for(const auto&a:as){last=last>a.eventId?last:a.eventId;Last(a.children,last);}}
int main(int argc,char**argv){if(argc!=4)return 2;const unsigned bpp=strtoul(argv[2],nullptr,10),frame=strtoul(argv[3],nullptr,10);if((bpp!=1&&bpp!=4)||frame>1)return 3;
 GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr>args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
 auto*f=RENDERDOC_OpenCaptureFile();auto r=f->OpenFile(argv[1],"rdc",nullptr);IReplayController*c=nullptr;if(!r.OK())return 4;rdctie(r,c)=f->OpenCapture(ReplayOptions(),nullptr);f->Shutdown();if(!r.OK()||!c){fprintf(stderr,"OpenCapture failed: %s\n",r.internal_msg?r.internal_msg->c_str():"unknown");return 5;}
 ResourceId image;for(const auto&t:c->GetTextures())if(t.width==8&&t.height==4)image=t.resourceId;if(image==ResourceId())return 6;uint32_t last=0;Last(c->GetRootActions(),last);if(!last)return 7;
 rdcarray<uint32_t>copies;auto collect=[&](auto&&self,const rdcarray<ActionDescription>&as)->void{for(const auto&a:as){if(a.copyDestination==image)copies.push_back(a.eventId);self(self,a.children);}};collect(collect,c->GetRootActions());if(copies.size()!=2)return 10;
 auto check=[&](unsigned base,unsigned patch,bool patched){auto data=c->GetTextureData(image,{0,0,0});if(data.size()!=32*bpp)return false;for(unsigned y=0;y<4;y++)for(unsigned x=0;x<8;x++)for(unsigned component=0;component<bpp;component++){
  unsigned expected=patched&&x>=1&&x<3&&y>=1&&y<3?patch:base;
  if(data[(y*8+x)*bpp+component]!=expected)return false;
 }return true;};
 for(unsigned i=0;i<4;i++){
  c->SetFrameEvent(copies[0],true);if(!check(0x40+frame*16,0,false))return 11;
  c->SetFrameEvent(last,true);if(!check(0x40+frame*16,0x90+frame*16,true))return 8;
  c->SetFrameEvent(0,true);if(!check(frame?0x40:0,frame?0x90:0,frame))return 9;
  printf("PASS linear texture upload cycle=%u all32pixels/firstcopy/partialpatch/initial\n",i);
 }
 c->Shutdown();RENDERDOC_ShutdownReplay();return 0;
}
