// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <fstream>
#include <iterator>
REPLAY_PROGRAM_MARKER()
int main(int argc,char **argv)
{
  if(argc!=5)return 1;
  const bool volume=atoi(argv[3])!=0;const unsigned offset=atoi(argv[4]);
  const bool groups=getenv("RENDERDOC_METAL_TEST_GROUPS")!=nullptr;
  const unsigned width=volume?(groups?8:5):(groups?64:67),height=volume?(groups?6:7):1,
      depth=volume?(groups?4:3):1;
  std::ifstream raw(argv[2],std::ios::binary);std::vector<char> native((std::istreambuf_iterator<char>(raw)),{});
  if(native.size()!=(512+offset+16)*4)return 2;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
  if(!result.OK()){file->Shutdown();RENDERDOC_ShutdownReplay();return 3;}
  IReplayController *controller=nullptr;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK() || !controller){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");RENDERDOC_ShutdownReplay();return 4;}
  ResourceId output;for(const auto &resource:controller->GetResources())if(resource.name=="Exact grid output")output=resource.resourceId;
  uint32_t dispatch=0,last=0;bool gridOK=true;unsigned dispatches=0;
  std::function<void(const rdcarray<ActionDescription> &)> visit=[&](const auto &actions) {
    for(const auto &action:actions){last=std::max(last,action.eventId);
      if(action.flags&ActionFlags::Dispatch){if(!dispatch)dispatch=action.eventId;dispatches++;
        gridOK &= action.dispatchDimension[0]==(groups?width/(volume?4:32):width) &&
          action.dispatchDimension[1]==(groups?height/(volume?3:1):height) &&
          action.dispatchDimension[2]==(groups?depth/(volume?2:1):depth) &&
          action.dispatchThreadsDimension[0]==(volume?4:32) && action.dispatchThreadsDimension[1]==(volume?3:1) && action.dispatchThreadsDimension[2]==(volume?2:1);}
      visit(action.children);}
  };visit(controller->GetRootActions());
  const unsigned expectedDispatches=getenv("RENDERDOC_METAL_TEST_TWO_ENCODERS")?2:1;
  bool good=output!=ResourceId() && dispatches==expectedDispatches && dispatch && gridOK;unsigned checks=0;
  for(unsigned cycle=0;good && cycle<2;cycle++)for(uint32_t event:{last,0U,dispatch-1,dispatch,0U,last})
  {
    controller->SetFrameEvent(event,true);auto bytes=controller->GetBufferData(output,0,native.size());
    if(event>=dispatch)good &= bytes.size()==native.size() && !memcmp(bytes.data(),native.data(),native.size());
    else {bytebuf initial;initial.resize(native.size());for(size_t i=0;i<initial.size();i+=4){uint32_t seed=0x6a6a6a6aU;memcpy(initial.data()+i,&seed,4);}good &= bytes==initial;}
    good &= controller->GetFatalErrorStatus().OK();checks++;
  }
  controller->Shutdown();RENDERDOC_ShutdownReplay();
  printf("GPU_COMPLETED; Native full-output/padding %s; exact 3D action dimensions; %u event/EID0 checks\n",good?"MATCH":"FAIL",checks);
  return good?0:6;
}
