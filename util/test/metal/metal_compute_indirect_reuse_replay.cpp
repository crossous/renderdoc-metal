// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
int main(int argc,char **argv)
{
  if(argc!=3) return 2;
  GlobalEnvironment env; env.enumerateGPUs=false;
  RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();
  auto result=file->OpenFile(argv[1],"rdc",nullptr); if(!result.OK()) return 3;
  IReplayController *controller=nullptr;
  rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr); file->Shutdown();
  if(!result.OK() || !controller) return 4;
  ResourceId output,arguments;
  rdcarray<ActionDescription> indirect; uint32_t lastEvent=0;
  rdcarray<const rdcarray<ActionDescription> *> lists={&controller->GetRootActions()};
  while(!lists.empty())
  {
    auto list=lists.back(); lists.pop_back();
    for(const auto &action:*list)
    {
      lastEvent=lastEvent>action.eventId?lastEvent:action.eventId;
      if((action.flags & ActionFlags::Indirect) && (action.flags & ActionFlags::Dispatch)) indirect.push_back(action);
      if(!action.children.empty()) lists.push_back(&action.children);
    }
  }
  if(indirect.size()!=3) return 5;
  controller->SetFrameEvent(indirect[0].eventId,true);
  const auto *firstState=controller->GetPipelineState().GetMetalPipelineState();
  if(!firstState || firstState->computeBuffers.empty()) return 5;
  output=firstState->computeBuffers[0].resourceId; arguments=firstState->indirectBuffer.resourceId;
  if(output==ResourceId() || arguments==ResourceId()) return 5;
  const uint32_t counts[]={1,3,2};
  for(unsigned i=0;i<3;i++)
    if(indirect[i].dispatchDimension[0]!=counts[i] || indirect[i].dispatchDimension[1]!=1 || indirect[i].dispatchDimension[2]!=1)
    { fprintf(stderr,"Indirect %u EID %u groups=%u/%u/%u expected=%u/1/1\n",i,indirect[i].eventId,indirect[i].dispatchDimension[0],indirect[i].dispatchDimension[1],indirect[i].dispatchDimension[2],counts[i]); return 6; }
  controller->SetFrameEvent(lastEvent,true);
  auto finalArgs=controller->GetBufferData(arguments,16,12); uint32_t args[3]={};
  if(finalArgs.size()!=12) return 7;
  memcpy(args,finalArgs.data(),12); if(args[0]!=0 || args[1]!=1 || args[2]!=1) return 8;
  const bool bufferWeight=atoi(argv[2])!=0;
  const uint32_t totals[]={10,70,130},completed[]={1,4,6};
  for(unsigned step:{2U,0U,1U,0U,2U,1U,2U})
  {
    controller->SetFrameEvent(0,true); controller->SetFrameEvent(indirect[step].eventId,true);
    auto data=controller->GetBufferData(output,0,64); if(data.size()!=64) return 9;
    uint32_t values[16];memcpy(values,data.data(),64);
    if(values[4]!=(bufferWeight?7*completed[step]:totals[step]) || values[5]!=completed[step]) return 10;
    for(unsigned i=0;i<16;i++) if(i!=4 && i!=5 && values[i]!=0x13572468) return 11;
    const auto *metal=controller->GetPipelineState().GetMetalPipelineState();
    if(!metal) return 12; const auto &state=*metal;
    if(state.computeBuffers.size()<2 || state.computeBuffers[0].resourceId!=output || state.computeBuffers[0].byteOffset!=16) return 12;
  }
  controller->Shutdown(); RENDERDOC_ShutdownReplay();
  puts("PASS per-use indirect groups 1/3/2 despite final zero; 7 reset/seeks; output, inline/buffer binding restoration and sentinels");
  return 0;
}
