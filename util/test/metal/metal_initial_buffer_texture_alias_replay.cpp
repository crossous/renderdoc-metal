// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <fstream>
#include <iterator>
REPLAY_PROGRAM_MARKER()
int main(int argc,char **argv)
{
  if(argc!=3)return 1;
  std::ifstream f(argv[2],std::ios::binary);std::vector<char> native((std::istreambuf_iterator<char>(f)),{});
  if(native.empty()||native.size()>1024*1024)return 2;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
  if(!result.OK()){file->Shutdown();RENDERDOC_ShutdownReplay();return 3;}
  IReplayController *controller=nullptr;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK()||!controller){fprintf(stderr,"FRAME_NOT_RUN: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");RENDERDOC_ShutdownReplay();return 4;}
  ResourceId output;for(const auto &resource:controller->GetResources())if(resource.name=="Native heap extent output")output=resource.resourceId;
  uint32_t last=0;std::function<void(const rdcarray<ActionDescription>&)> visit=[&](const auto &actions){for(const auto &a:actions){last=std::max(last,a.eventId);visit(a.children);}};visit(controller->GetRootActions());
  bool good=output!=ResourceId();unsigned checks=0;
  for(unsigned cycle=0;good&&cycle<2;cycle++)for(uint32_t event:{last,0U,last,0U,last})
  {controller->SetFrameEvent(event,true);auto bytes=controller->GetBufferData(output,0,native.size());
   good&=bytes.size()==native.size();if(good){if(event)good&=!memcmp(bytes.data(),native.data(),native.size());else for(byte v:bytes)good&=v==0x6a;}
   good&=controller->GetFatalErrorStatus().OK();checks++;}
  controller->Shutdown();RENDERDOC_ShutdownReplay();printf("GPU_COMPLETED; full Native output/padding %s; %u event/EID0 checks\n",good?"MATCH":"FAIL",checks);return good?0:6;
}
