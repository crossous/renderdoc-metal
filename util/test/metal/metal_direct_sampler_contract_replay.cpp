// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <fstream>
#include <iterator>
REPLAY_PROGRAM_MARKER()
int main(int argc, char **argv)
{
  if(argc != 3) return 1;
  std::ifstream raw(argv[2],std::ios::binary); std::vector<char> native((std::istreambuf_iterator<char>(raw)),{});
  if(native.size()!=576) return 2;
  GlobalEnvironment env; env.enumerateGPUs=false; RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile(); auto result=file->OpenFile(argv[1],"rdc",nullptr);
  if(!result.OK()) { file->Shutdown(); RENDERDOC_ShutdownReplay(); return 3; }
  IReplayController *controller=nullptr; rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK() || !controller) { fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown"); RENDERDOC_ShutdownReplay();return 4; }
  ResourceId output; for(const auto &resource:controller->GetResources())
    if(resource.name=="Direct sampler contract output") output=resource.resourceId;
  std::vector<uint32_t> events; uint32_t last=0;
  std::function<void(const rdcarray<ActionDescription> &)> visit=[&](const auto &actions) {
    for(const auto &action:actions) { last=std::max(last,action.eventId);
      if(action.flags&ActionFlags::Dispatch) events.push_back(action.eventId); visit(action.children); }
  }; visit(controller->GetRootActions());
  bool good=output!=ResourceId() && events.size()==4; unsigned checks=0, bindingChecks=0;
  ResourceId nearest, linear;
  std::vector<char> seed(576); for(size_t i=0;i<seed.size();i+=4) {uint32_t v=0x6a6a6a6aU;memcpy(seed.data()+i,&v,4);}
  for(unsigned cycle=0; good && cycle<2; cycle++) {
    std::vector<uint32_t> visits={last,0,events[0]-1,events[0],events[1],events[2],events[3],0,last};
    for(uint32_t event:visits) {
      auto expected=seed; for(size_t pass=0;pass<4;pass++) if(event>=events[pass])
        memcpy(expected.data()+32+pass*128,native.data()+32+pass*128,128);
      controller->SetFrameEvent(event,true); auto data=controller->GetBufferData(output,0,576);
      good &= data.size()==576 && !memcmp(data.data(),expected.data(),576) && controller->GetFatalErrorStatus().OK(); checks++;
      for(size_t pass=0;pass<4;pass++) if(event==events[pass]) {
        const auto *state=controller->GetPipelineState().GetMetalPipelineState();
        good &= state && state->computeSamplers.size()>3 && state->computeSamplers[3]!=ResourceId();
        if(state && state->computeSamplers.size()>3) {
          auto id=state->computeSamplers[3];
          if(pass==0) nearest=id; if(pass==1) linear=id;
          if(pass==2) good &= id==nearest;
          if(pass==3) good &= id==linear;
          if(pass>=1) good &= nearest!=linear && state->computeSamplers[2]==ResourceId();
          if(pass==3) good &= state->computeSamplers.size()>4 && state->computeSamplers[4]==ResourceId();
        }
        bindingChecks++;
      }
    }
  }
  controller->Shutdown(); RENDERDOC_ShutdownReplay();
  printf("Native direct sampler full output/padding %s; %u event/EID0 and %u binding checks\n",good?"MATCH":"FAIL",checks,bindingChecks);
  return good?0:6;
}
