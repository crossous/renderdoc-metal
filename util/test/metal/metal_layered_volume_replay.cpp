// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Last(const rdcarray<ActionDescription>&as,uint32_t&last){for(const auto&a:as){last=last>a.eventId?last:a.eventId;Last(a.children,last);}}
int main(int argc,char**argv){if(argc!=2)return 2; GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr>args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
 auto*f=RENDERDOC_OpenCaptureFile();auto r=f->OpenFile(argv[1],"rdc",nullptr);IReplayController*c=nullptr;if(!r.OK())return 4;rdctie(r,c)=f->OpenCapture(ReplayOptions(),nullptr);f->Shutdown();if(!r.OK()||!c){fprintf(stderr,"OpenCapture failed: %s\n",r.internal_msg?r.internal_msg->c_str():"unknown");return 5;}
 c->SetFrameEvent(0,true);rdcarray<ResourceId> images;for(const auto&t:c->GetTextures())if(t.width==4&&t.height==4&&t.depth==4&&c->GetTextureData(t.resourceId,{0,0,0}).empty())images.push_back(t.resourceId);if(images.size()!=2)return 6;
 uint32_t last=0;Last(c->GetRootActions(),last);if(!last)return 7;
 const uint16_t levels[]={0x3c00,0x4000,0x4200,0x4400};
 auto check=[&](unsigned target){auto data=c->GetTextureData(images[target],{0,0,0});if(data.size()!=4*4*4*8){fprintf(stderr,"volume readback bytes=%zu\n",data.size());return false;}
  for(unsigned z=0;z<4;z++)for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++) {
   uint16_t expected[]={0x3400,0x3800,0x3a00,0x3c00};expected[target]=levels[z];
   if(memcmp(data.data()+((z*4+y)*4+x)*8,expected,8)){fprintf(stderr,"volume mismatch target=%u layer=%u\n",target,z);return false;}
  }return true;};
 for(unsigned i=0;i<4;i++){c->SetFrameEvent(last,true);if(!check(0)||!check(1))return 8;c->SetFrameEvent(0,true);for(auto image:images)if(!c->GetTextureData(image,{0,0,0}).empty())return 9;printf("PASS layered MRT cycle=%u all128voxels and frame birth removal\n",i);}
 c->Shutdown();RENDERDOC_ShutdownReplay();return 0;
}
