// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
int main(int argc,char **argv)
{
  if(argc!=2)return 2;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
  if(!result.OK())return 3;
  IReplayController *controller=nullptr;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK() || !controller){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
  rdcarray<ActionDescription> draws;uint32_t last=0;
  rdcarray<const rdcarray<ActionDescription> *> lists={&controller->GetRootActions()};
  while(!lists.empty())
  {
    auto list=lists.back();lists.pop_back();
    for(const auto &action:*list)
    {
      if(action.eventId>last)last=action.eventId;
      if((action.flags&ActionFlags::Drawcall)&&(action.flags&ActionFlags::Indirect))draws.push_back(action);
      if(!action.children.empty())lists.push_back(&action.children);
    }
  }
  if(draws.size()!=2)return 5;
  for(unsigned i=0;i<2;i++)
    if(draws[i].numIndices!=(i?6U:3U)||draws[i].numInstances!=1||draws[i].instanceOffset!=2 || !(draws[i].flags&ActionFlags::Instanced))
    {fprintf(stderr,"draw %u metadata %u/%u/%u\n",i,draws[i].numIndices,draws[i].numInstances,draws[i].instanceOffset);return 6;}
  ResourceId arguments;
  for(unsigned step:{1U,0U,1U,0U,1U,0U,1U})
  {
    controller->SetFrameEvent(0,true);controller->SetFrameEvent(draws[step].eventId,true);
    auto metal=controller->GetPipelineState().GetMetalPipelineState();
    if(!metal || metal->indirectBuffer.resourceId==ResourceId() || metal->indirectBuffer.byteOffset!=(step?64U:16U))return 7;
    arguments=metal->indirectBuffer.resourceId;
    auto pixels=controller->GetTextureData(draws[step].outputs[0],{0,0,0});
    if(pixels.size()!=16){fprintf(stderr,"pixel bytes %zu\n",pixels.size());return 8;}
    for(unsigned i=0;i<16;i+=4)
      if(pixels[i]!=128||pixels[i+1]!=64||pixels[i+2]!=255||pixels[i+3]!=255){fprintf(stderr,"pixel %u=%u/%u/%u/%u\n",i,pixels[i],pixels[i+1],pixels[i+2],pixels[i+3]);return 9;}
  }
  controller->SetFrameEvent(last,true);auto bytes=controller->GetBufferData(arguments,0,96);
  if(bytes.size()!=96)return 10;for(auto value:bytes)if(value)return 11;
  controller->Shutdown();RENDERDOC_ShutdownReplay();
  puts("PASS render indirect per-use counts 3/6, baseInstance2, seven reset/seeks, every BGRA pixel and final-zero Private source");return 0;
}
