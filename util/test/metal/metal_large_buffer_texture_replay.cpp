// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
int main(int argc,char **argv)
{
  if(argc!=2)return 2;GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);if(!result.OK())return 3;
  IReplayController *replay=nullptr;rdctie(result,replay)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK()||!replay){fprintf(stderr,"Open failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
  ResourceId output;for(const auto &buffer:replay->GetBuffers())if(buffer.length==12)output=buffer.resourceId;
  uint32_t event=0;rdcarray<const rdcarray<ActionDescription> *> lists={&replay->GetRootActions()};
  while(!lists.empty()){auto list=lists.back();lists.pop_back();for(const auto &a:*list){if(a.flags&ActionFlags::Dispatch)event=a.eventId;if(!a.children.empty())lists.push_back(&a.children);}}
  if(output==ResourceId()||!event)return 5;
  for(unsigned iteration=0;iteration<4;iteration++) {
    replay->SetFrameEvent(0,true);replay->SetFrameEvent(event,true);auto bytes=replay->GetBufferData(output,0,12);
    uint32_t values[3]={};if(bytes.size()!=12)return 6;memcpy(values,bytes.data(),12);
    if(values[0]!=0x12345678||values[1]!=0x31415926||values[2]!=0xabcdef01)return 7;
  }
  replay->Shutdown();RENDERDOC_ShutdownReplay();puts("PASS large TextureBuffer replay: first/middle/last exact sentinels through four reset/seeks, original three-thread dispatch");
}
