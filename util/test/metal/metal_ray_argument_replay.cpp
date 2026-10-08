// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
REPLAY_PROGRAM_MARKER()
static rdcarray<APIEvent> events;
static uint32_t dispatch;
static void Walk(const rdcarray<ActionDescription> &actions)
{
  for(const auto &action : actions)
  {
    events.append(action.events);
    if(action.flags & ActionFlags::Dispatch) dispatch=action.eventId;
    Walk(action.children);
  }
}
int main(int argc, char **argv)
{
  if(argc!=2) return 1;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
  IReplayController *controller=nullptr;
  if(result.OK()) rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);
  file->Shutdown();if(!result.OK() || !controller) return 2;
  Walk(controller->GetRootActions());ResourceId output;
  for(const auto &resource:controller->GetResources())
    if(resource.name=="Ray device argument output") output=resource.resourceId;
  bool ok=dispatch && output!=ResourceId();
  std::sort(events.begin(),events.end(),[](const APIEvent &a,const APIEvent &b){return a.eventId<b.eventId;});
  uint64_t dispatchOffset=0;for(const auto &event:events)if(event.eventId==dispatch)dispatchOffset=event.fileOffset;
  for(unsigned cycle=0;cycle<3 && ok;cycle++)
    for(size_t i=0;i<events.size() && ok;i++)
    {
      const auto &event=events[cycle==1?events.size()-1-i:i];
      controller->SetFrameEvent(0,true);controller->SetFrameEvent(event.eventId,true);
      auto bytes=controller->GetBufferData(output,0,4);unsigned value=0;
      if(bytes.size()==4) memcpy(&value,bytes.data(),4);
      const auto &name=controller->GetStructuredFile().chunks[event.chunkIndex]->name;
      const bool executed=name.beginsWith("MTLComputeCommandEncoder::")?
        event.eventId>=dispatch:event.fileOffset>=dispatchOffset;
      ok &= controller->GetFatalErrorStatus().OK() && bytes.size()==4 && value==(executed?7:99);
      printf("%s cycle=%u EID=%u output=%u\n",ok?"PASS":"FAIL",cycle,event.eventId,value);
    }
  controller->Shutdown();RENDERDOC_ShutdownReplay();return ok?0:3;
}
