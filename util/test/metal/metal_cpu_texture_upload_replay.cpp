// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Find(const rdcarray<ActionDescription> &actions,uint32_t &dispatch)
{for(const auto &a:actions){if(a.flags&ActionFlags::Dispatch)dispatch=a.eventId;Find(a.children,dispatch);}}
int main(int argc,char **argv)
{
  if(argc!=2)return 2;
  GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
  ICaptureFile *file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);IReplayController *controller=nullptr;
  if(!result.OK())return 3;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK()||!controller){fprintf(stderr,"Open failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
  ResourceId output;for(const auto &b:controller->GetBuffers())if(b.length==8)output=b.resourceId;
  uint32_t dispatch=0;Find(controller->GetRootActions(),dispatch);if(!dispatch||output==ResourceId())return 5;
  for(int cycle=0;cycle<4;cycle++)
  {controller->SetFrameEvent(dispatch,true);const auto bytes=controller->GetBufferData(output,0,8);uint32_t words[2]={};
    if(bytes.size()!=8)return 6;memcpy(words,bytes.data(),8);if(words[0]!=407||words[1]!=0xdeadbeefU)return 7;
    controller->SetFrameEvent(0,true);}
  controller->Shutdown();RENDERDOC_ShutdownReplay();puts("PASS replay 3D last image and BC1 compressed upload, GPU=407/DEADBEEF, four seeks");return 0;
}
