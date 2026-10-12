// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <fstream>
#include <iterator>
#include <vector>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
int main(int argc,char **argv){
 if(argc!=4)return 1;unsigned side=strtoul(argv[3],nullptr,10);std::ifstream in(argv[2],std::ios::binary);std::vector<unsigned char> expected((std::istreambuf_iterator<char>(in)),{});if(expected.size()!=side*side*4+4096)return 2;
 GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});auto f=RENDERDOC_OpenCaptureFile();if(!f->OpenFile(argv[1],"rdc",{}).OK())return 3;
 auto opened=f->OpenCapture(ReplayOptions(),{});f->Shutdown();if(!opened.first.OK()||!opened.second){fprintf(stderr,"OpenCapture failed: %s\n",opened.first.internal_msg?opened.first.internal_msg->c_str():"unknown");RENDERDOC_ShutdownReplay();return 4;}
 auto c=opened.second;ResourceId image,result;for(const auto &b:c->GetResources()) {auto n=b.name;if(n=="Native texture before alias")image=b.resourceId;if(n=="Native buffer after alias")result=b.resourceId;}
 uint32_t last=0;std::function<void(const rdcarray<ActionDescription>&)> visit=[&](const auto &as){for(const auto&a:as){last=std::max(last,a.eventId);visit(a.children);}};visit(c->GetRootActions());bool good=image!=ResourceId()&&result!=ResourceId();unsigned checks=0;
 for(unsigned cycle=0;cycle<4;cycle++)for(uint32_t event:{last,0U,last}) {c->SetFrameEvent(event,true);auto a=c->GetBufferData(image,0,side*side*4);auto b=c->GetBufferData(result,0,4096);good &= a.size()+b.size()==expected.size();size_t at=0;for(byte v:a)good &= v==(event==0?0x6a:expected[at++]);for(byte v:b)good &= v==(event==0?0x6a:expected[at++]);good &= c->GetFatalErrorStatus().OK();checks++;}
 c->Shutdown();RENDERDOC_ShutdownReplay();printf("complete Native/EID0 checks=%u %s\n",checks,good?"MATCH":"FAIL");return good?0:6;
}
