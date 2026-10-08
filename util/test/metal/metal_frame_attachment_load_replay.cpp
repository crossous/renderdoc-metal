// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include "metal_frame_attachment_load_cases.h"
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Draws(const rdcarray<ActionDescription>&as,rdcarray<uint32_t>&ds,uint32_t&last){for(const auto&a:as){last=last>a.eventId?last:a.eventId;if(a.flags&ActionFlags::Drawcall)ds.push_back(a.eventId);Draws(a.children,ds,last);}}
int main(int argc,char**argv){if(argc!=2&&argc!=3)return 2;const FrameColorCase *initialColor=argc==3?FindFrameColorCase(argv[2]):nullptr;GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr>args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
 auto*f=RENDERDOC_OpenCaptureFile();auto r=f->OpenFile(argv[1],"rdc",nullptr);IReplayController*c=nullptr;if(!r.OK())return 3;rdctie(r,c)=f->OpenCapture(ReplayOptions(),nullptr);f->Shutdown();if(!r.OK()||!c){fprintf(stderr,"OpenCapture failed: %s\n",r.internal_msg?r.internal_msg->c_str():"unknown");return 4;}
 ResourceId images[4];for(const auto&res:c->GetResources())for(unsigned i=0;i<4;i++){char name[64];snprintf(name,sizeof(name),"Frame Load target %u",i);if(res.name==name)images[i]=res.resourceId;}for(auto id:images)if(id==ResourceId())return 5;
 rdcarray<uint32_t>ds;uint32_t last=0;Draws(c->GetRootActions(),ds,last);if(ds.size()!=2)return 6;
 const uint8_t colors[2][2][4]={{{64,128,191,255},{191,64,128,255}},{{191,128,64,255},{128,191,64,255}}};const uint32_t packed[2]={(14U<<6)|(((14U<<6)|32U)<<11)|((15U<<5)<<22),(15U<<6)|((14U<<6)<<11)|((13U<<5)<<22)};
 const uint32_t events[]={ds[0],ds[1],ds[0],last,0,last,ds[1],0};
 for(unsigned cycle=0;cycle<4;cycle++)for(uint32_t event:events){c->SetFrameEvent(event,true);const unsigned frame=event==ds[0]?0:1;
  for(unsigned i=0;i<4;i++){auto data=c->GetTextureData(images[i],{0,0,0});if(!event){if(initialColor&&i==0){if(data.size()!=256*initialColor->stride)return 7;for(byte value:data)if(value)return 7;}else if(!data.empty())return 7;continue;}const unsigned stride=i==3?8:initialColor&&i==0?initialColor->stride:4;if(data.size()!=256*stride){fprintf(stderr,"size mismatch event=%u image=%u bytes=%zu\n",event,i,data.size());return 8;}for(unsigned p=0;p<256;p++){const byte*pixel=data.data()+p*stride;float depth=0;if(stride>=4)memcpy(&depth,pixel,4);bool good=initialColor&&i==0?!memcmp(pixel,frame?initialColor->second:initialColor->first,initialColor->stride):i<2?!memcmp(pixel,colors[frame][i],4):i==2?!memcmp(pixel,&packed[frame],4):depth==(frame?.75f:.25f)&&pixel[4]==(frame?42:7)&&pixel[5]==0&&pixel[6]==0&&pixel[7]==0;if(!good){fprintf(stderr,"pixel mismatch event=%u image=%u pixel=%u bits=%08x stencil=%u\n",event,i,p,uint32_t(pixel[0]),i==3?pixel[4]:0);return 9;}}
  }
 }
 c->Shutdown();RENDERDOC_ShutdownReplay();printf("PASS frame MRT/depth/stencil first/second full256pixels, 32 event selections including EID0\n");return 0;
}
