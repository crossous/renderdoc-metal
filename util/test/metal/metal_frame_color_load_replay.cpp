// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Last(const rdcarray<ActionDescription>&as,uint32_t&last){for(const auto&a:as){last=last>a.eventId?last:a.eventId;Last(a.children,last);}}
int main(int argc,char**argv){if(argc!=4)return 2;bytebuf expected,initial;for(const char*s:{argv[2],argv[3]}){bytebuf&b=s==argv[2]?expected:initial;for(size_t i=0;i<strlen(s);i+=2){char h[3]={s[i],s[i+1],0};b.push_back((byte)strtoul(h,nullptr,16));}}if(expected.empty()||expected.size()!=initial.size())return 3;
 GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr>args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
 auto*f=RENDERDOC_OpenCaptureFile();auto r=f->OpenFile(argv[1],"rdc",nullptr);IReplayController*c=nullptr;if(!r.OK())return 4;rdctie(r,c)=f->OpenCapture(ReplayOptions(),nullptr);f->Shutdown();if(!r.OK()||!c){fprintf(stderr,"OpenCapture failed: %s\n",r.internal_msg?r.internal_msg->c_str():"unknown");return 5;}
 ResourceId image;for(const auto&t:c->GetTextures())if(t.width==8&&t.height==4)image=t.resourceId;if(image==ResourceId())return 6;uint32_t last=0;Last(c->GetRootActions(),last);if(!last)return 7;
 auto check=[&](const bytebuf&pixel){auto data=c->GetTextureData(image,{0,0,0});if(data.size()!=32*pixel.size())return false;for(size_t i=0;i<data.size();i+=pixel.size())if(memcmp(data.data()+i,pixel.data(),pixel.size()))return false;return true;};
 for(unsigned i=0;i<4;i++){c->SetFrameEvent(last,true);if(!check(expected))return 8;c->SetFrameEvent(0,true);auto data=c->GetTextureData(image,{0,0,0});if(!data.empty())return 9;printf("PASS frame color Load after Native compute cycle=%u full32pixels current/initial\n",i);}
 c->Shutdown();RENDERDOC_ShutdownReplay();return 0;
}
