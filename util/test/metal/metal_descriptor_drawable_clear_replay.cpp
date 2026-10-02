// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Last(const rdcarray<ActionDescription>&as,uint32_t&last){for(const auto&a:as){last=last>a.eventId?last:a.eventId;Last(a.children,last);}}
int main(int argc,char**argv){if(argc!=2)return 2;bytebuf expected={191,128,64,255};
 GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr>args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
 auto*f=RENDERDOC_OpenCaptureFile();auto r=f->OpenFile(argv[1],"rdc",nullptr);IReplayController*c=nullptr;if(!r.OK())return 4;rdctie(r,c)=f->OpenCapture(ReplayOptions(),nullptr);f->Shutdown();if(!r.OK()||!c){fprintf(stderr,"OpenCapture failed: %s\n",r.internal_msg?r.internal_msg->c_str():"unknown");return 5;}
 ResourceId image;for(const auto&t:c->GetTextures())if(t.width==2&&t.height==2)image=t.resourceId;if(image==ResourceId())return 6;uint32_t last=0;Last(c->GetRootActions(),last);if(!last)return 7;
 auto check=[&](const bytebuf&pixel){auto data=c->GetTextureData(image,{0,0,0});if(data.size()!=4*pixel.size())return false;for(size_t i=0;i<data.size();i+=pixel.size())if(memcmp(data.data()+i,pixel.data(),pixel.size()))return false;return true;};
 ResourceId output;for(const auto&b:c->GetBuffers())if(b.length==8)output=b.resourceId;if(output==ResourceId())return 10;
 for(unsigned i=0;i<4;i++){
  c->SetFrameEvent(last,true);if(!check(expected))return 8;
  const auto bytes=c->GetBufferData(output,0,8);uint32_t result[2]={};if(bytes.size()!=8)return 11;memcpy(result,bytes.data(),8);if(result[0]!=638||result[1]!=0xdeadbeefU)return 12;
  c->SetFrameEvent(0,true);printf("PASS drawable binding-before-Clear and GPU SRV read-after-Clear cycle=%u\n",i);
 }
 c->Shutdown();RENDERDOC_ShutdownReplay();return 0;
}
