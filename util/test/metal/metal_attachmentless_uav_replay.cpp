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
  if(!result.OK()||!replay){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
  ResourceId output,arguments;for(const auto &r:replay->GetResources())
  {if(r.name=="Attachmentless vertex UAV")output=r.resourceId;if(r.name=="Attachmentless indirect")arguments=r.resourceId;}
  for(const auto &buffer:replay->GetBuffers())if(buffer.length==16)output=buffer.resourceId;
  rdcarray<ActionDescription> draws;uint32_t last=0;rdcarray<const rdcarray<ActionDescription> *> lists={&replay->GetRootActions()};
  while(!lists.empty()){auto list=lists.back();lists.pop_back();for(const auto &a:*list){if(a.eventId>last)last=a.eventId;if((a.flags&ActionFlags::Drawcall)&&a.numIndices&&a.numInstances)draws.push_back(a);if(!a.children.empty())lists.push_back(&a.children);}}
  if(!draws.empty()) { replay->SetFrameEvent(draws[0].eventId,true);
    const auto *state=replay->GetPipelineState().GetMetalPipelineState();if(state&&(draws[0].flags&ActionFlags::Indirect))arguments=state->indirectBuffer.resourceId; }
  if(draws.size()!=2||output==ResourceId()||arguments==ResourceId()){
    fprintf(stderr,"draws=%zu output=%d arguments=%d\n",draws.size(),output!=ResourceId(),arguments!=ResourceId());
    for(const auto &r:replay->GetResources())fprintf(stderr,"resource %s\n",r.name.c_str());return 5;}
  for(unsigned i=0;i<2;i++){if(draws[i].numIndices!=(i?6U:3U)||draws[i].numInstances!=1||draws[i].instanceOffset!=2||draws[i].depthOut!=ResourceId())return 6;for(auto target:draws[i].outputs)if(target!=ResourceId())return 7;}
  for(unsigned step:{1U,0U,1U,0U,1U,0U,1U})
  {
    replay->SetFrameEvent(0,true);replay->SetFrameEvent(draws[step].eventId,true);
    const auto bytes=replay->GetBufferData(output,0,16);uint32_t values[4]={};if(bytes.size()!=16)return 8;memcpy(values,bytes.data(),16);
    if(values[0]!=(step?9U:3U)||values[1]!=(step?27U:6U)||values[2]!=0x13572468||values[3]!=0x24681357){fprintf(stderr,"UAV step=%u values=%u/%u/%x/%x\n",step,values[0],values[1],values[2],values[3]);return 9;}
    const auto *state=replay->GetPipelineState().GetMetalPipelineState();if(!state||state->vertexShader.resourceId==ResourceId()||state->fragmentShader.resourceId==ResourceId()||((draws[step].flags&ActionFlags::Indirect)&&state->indirectBuffer.resourceId!=arguments))return 10;
    for(const auto &target:state->colorTargets)if(target.resource!=ResourceId())return 11;
  }
  replay->SetFrameEvent(last,true);const auto bytes=replay->GetBufferData(arguments,0,96);if(bytes.size()!=96)return 12;for(auto v:bytes)if(v)return 13;
  replay->Shutdown();RENDERDOC_ShutdownReplay();puts("PASS attachmentless replay: per-use 3/6, actual vertex UAV3/9 and sums6/27, sentinels, seven reset/seeks, no targets, fragment present, final-zero source");
}
