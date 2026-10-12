// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <functional>
REPLAY_PROGRAM_MARKER()
int main(int argc,char **argv){if(argc!=3)return 1;std::ifstream input(argv[2],std::ios::binary);std::vector<char> native((std::istreambuf_iterator<char>(input)),{});if(native.empty())return 2;GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});auto f=RENDERDOC_OpenCaptureFile();auto s=f->OpenFile(argv[1],"rdc",{});if(!s.OK())return 3;auto o=f->OpenCapture(ReplayOptions(),{});f->Shutdown();if(!o.first.OK()||!o.second)return 4;auto c=o.second;ResourceId out;for(auto &r:c->GetResources())if(r.name=="Shared publication invariant")out=r.resourceId;uint32_t last=0;std::vector<uint32_t> events;std::function<void(const rdcarray<ActionDescription>&)> visit=[&](const auto &as){for(auto &a:as){last=std::max(last,a.eventId);if(a.flags&ActionFlags::Dispatch)events.push_back(a.eventId);visit(a.children);}};visit(c->GetRootActions());bool good=out!=ResourceId()&&events.size()==2;unsigned checks=0;for(unsigned cycle=0;good&&cycle<3;cycle++)for(auto event:{last,0U,events[0],events[1],0U,last}){c->SetFrameEvent(event,true);auto b=c->GetBufferData(out,0,native.size());good&=b.size()==native.size();for(size_t i=0;good&&i<b.size();i++)good&=b[i]==(event>=events[1]?byte(native[i]):byte(0));good&=c->GetFatalErrorStatus().OK();checks++;}c->Shutdown();RENDERDOC_ShutdownReplay();printf("Full Native shared correlation/persistent CPU input %s; %u event/EID0 checks; bytes=%zu\n",good?"MATCH":"FAIL",checks,native.size());return good?0:7;}
