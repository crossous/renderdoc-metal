// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <functional>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
int main(int argc,char **argv) {
  if(argc!=3)return 1;uint64_t unit=strtoull(argv[2],nullptr,10);
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",{});
  if(!result.OK())return 3;
  rdcarray<ResourceId> reads;
  for(auto chunk:file->GetStructuredData().chunks) if(chunk->name=="MTLBlitCommandEncoder::copyFromBuffer")
    reads.push_back(chunk->FindChild("destinationBuffer")->AsResourceId());
  auto opened=file->OpenCapture(ReplayOptions(),{});file->Shutdown();
  if(!opened.first.OK() || !opened.second){fprintf(stderr,"OpenCapture failed\n");RENDERDOC_ShutdownReplay();return 4;}
  auto replay=opened.second;uint32_t last=0;
  std::function<void(const rdcarray<ActionDescription>&)> visit=[&](const auto &as){for(auto &a:as){last=std::max(last,a.eventId);visit(a.children);}};visit(replay->GetRootActions());
  bool good=reads.size()==3;unsigned checks=0;
  for(unsigned cycle=0;cycle<4;cycle++) for(uint32_t event:{last,0U,last}) {
    replay->SetFrameEvent(event,true);
    for(unsigned k=0;k<reads.size();k++) {
      auto bytes=replay->GetBufferData(reads[k],0,4*unit);good &= bytes.size()==4*unit;
      for(byte value:bytes)good &= value==(event==0?0:k==0?0x31:k==1?0x52:0x73);
    }good &= replay->GetFatalErrorStatus().OK();checks++;
  }
  replay->Shutdown();RENDERDOC_ShutdownReplay();if(!good)return 6;
  printf("PASS retired backing chain Native three full outputs/padding %u event/EID0 checks\n",checks);return 0;
}
