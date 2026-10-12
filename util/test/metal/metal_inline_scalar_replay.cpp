// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
REPLAY_PROGRAM_MARKER()
int main(int argc,char **argv) {
  if(argc!=3)return 1;std::ifstream f(argv[2],std::ios::binary);
  std::vector<char> native((std::istreambuf_iterator<char>(f)),{});if(native.size()!=640)return 2;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",{});
  if(!result.OK())return 3;auto opened=file->OpenCapture(ReplayOptions(),{});file->Shutdown();
  if(!opened.first.OK() || !opened.second){fprintf(stderr,"OpenCapture failed\n");RENDERDOC_ShutdownReplay();return 4;}
  auto replay=opened.second;ResourceId output;
  for(const auto &r:replay->GetResources())if(r.name=="Inline scalar full output")output=r.resourceId;
  uint32_t last=0;std::vector<uint32_t> dispatches;
  std::function<void(const rdcarray<ActionDescription>&)> visit=[&](const auto &as){for(const auto &a:as){last=std::max(last,a.eventId);if(a.flags&ActionFlags::Dispatch)dispatches.push_back(a.eventId);visit(a.children);}};
  visit(replay->GetRootActions());bool good=output!=ResourceId() && dispatches.size()==2;unsigned checks=0;
  for(unsigned cycle=0;good && cycle<3;cycle++)for(uint32_t event:{last,0U,dispatches[0],dispatches[1],0U,last}) {
    replay->SetFrameEvent(event,true);auto b=replay->GetBufferData(output,0,640);good &= b.size()==640;
    for(size_t i=0;good && i<640;i++) {
      const bool written=event>=dispatches[0] && i>=32 && i<288 || event>=dispatches[1] && i>=352 && i<608;
      good &= b[i]==(written?byte(native[i]):byte(0x72));
    }
    good &= replay->GetFatalErrorStatus().OK();checks++;
  }
  replay->Shutdown();RENDERDOC_ShutdownReplay();
  printf("Native complete output/padding %s; %u event/EID0 checks\n",good?"MATCH":"FAIL",checks);return good?0:6;
}
