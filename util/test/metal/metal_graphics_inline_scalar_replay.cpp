// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
REPLAY_PROGRAM_MARKER()
int main(int argc,char**argv){if(argc!=3)return 1;std::vector<std::vector<char>> reference;
 for(unsigned i=0;i<3;i++){std::ifstream f(std::string(argv[2])+"."+std::to_string(i)+".bin",std::ios::binary);reference.emplace_back(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());}
 GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});auto f=RENDERDOC_OpenCaptureFile();if(!f->OpenFile(argv[1],"rdc",{}).OK())return 3;
 auto opened=f->OpenCapture(ReplayOptions(),{});f->Shutdown();if(!opened.first.OK()||!opened.second){RENDERDOC_ShutdownReplay();return 4;}auto c=opened.second;ResourceId image;
 for(auto &r:c->GetResources())if(r.name=="Graphics inline full output")image=r.resourceId;
 std::vector<uint32_t> draws;uint32_t last=0;std::function<void(const rdcarray<ActionDescription>&)>visit=[&](auto &as){for(auto &a:as){last=std::max(last,a.eventId);if(a.flags&ActionFlags::Drawcall)draws.push_back(a.eventId);visit(a.children);}};visit(c->GetRootActions());bool good=image!=ResourceId()&&draws.size()==2&&!reference[0].empty();unsigned checks=0;
 for(unsigned cycle=0;good&&cycle<3;cycle++)for(uint32_t event:{last,0U,draws[0],draws[1],0U,last}){c->SetFrameEvent(event,true);auto bytes=c->GetTextureData(image,{0,0,0});unsigned step=event<draws[0]?0:event<draws[1]?1:2;good &= bytes.size()==reference[step].size()&&memcmp(bytes.data(),reference[step].data(),bytes.size())==0;good &=c->GetFatalErrorStatus().OK();checks++;}
 c->Shutdown();RENDERDOC_ShutdownReplay();printf("Native complete image %s; %u event/EID0 checks\n",good?"MATCH":"FAIL",checks);return good?0:6;}
