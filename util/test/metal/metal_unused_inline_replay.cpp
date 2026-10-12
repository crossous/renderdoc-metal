// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
REPLAY_PROGRAM_MARKER()
int main(int argc,char **argv)
{
  if(argc!=4)return 1;
  const unsigned count=atoi(argv[2]),offset=atoi(argv[3]);
  if((count!=64 && count!=1024) || !offset)return 1;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
  if(!result.OK()){file->Shutdown();RENDERDOC_ShutdownReplay();return 3;}
  IReplayController *controller=nullptr;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK() || !controller){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");RENDERDOC_ShutdownReplay();return 4;}
  ResourceId output;for(const auto &resource:controller->GetResources())
    if(resource.name=="Unused inline output")output=resource.resourceId;
  uint32_t dispatch=0,last=0;unsigned dispatches=0;
  std::function<void(const rdcarray<ActionDescription> &)> visit=[&](const auto &actions) {
    for(const auto &action:actions){last=std::max(last,action.eventId);
      if(action.flags&ActionFlags::Dispatch){dispatch=action.eventId;dispatches++;}visit(action.children);}
  };visit(controller->GetRootActions());
  bool good=output!=ResourceId() && dispatches==1 && dispatch;unsigned checks=0;
  for(unsigned cycle=0;good && cycle<2;cycle++)for(uint32_t event:{last,0U,dispatch-1,dispatch,0U,last})
  {
    controller->SetFrameEvent(event,true);bytebuf expected;expected.resize((count+offset+16)*4);
    for(unsigned i=0;i<count+offset+16;i++){const uint32_t value=0x6a6a6a6aU;memcpy(expected.data()+i*4,&value,4);}
    if(event>=dispatch)for(unsigned t=0;t<count;t++)
    {const uint32_t value=((t*13)%count)*23+108;memcpy(expected.data()+(offset+(t*17)%count)*4,&value,4);}
    good &= controller->GetBufferData(output,0,expected.size())==expected && controller->GetFatalErrorStatus().OK();checks++;
  }
  controller->Shutdown();RENDERDOC_ShutdownReplay();
  printf("GPU_COMPLETED; output %s; %u full-output/event/EID0 checks\n",good?"MATCH":"FAIL",checks);
  return good?0:6;
}
