// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
REPLAY_PROGRAM_MARKER()
static void Dispatches(const rdcarray<ActionDescription> &actions,rdcarray<uint32_t> &events)
{for(const auto &a:actions){if(a.flags&ActionFlags::Dispatch)events.push_back(a.eventId);Dispatches(a.children,events);}}
int main(int argc,char **argv)
{
  if(argc!=3)return 1;const bool array=!strcmp(argv[2],"array");
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",NULL);
  IReplayController *c=NULL;if(result.OK())rdctie(result,c)=file->OpenCapture(ReplayOptions(),NULL);file->Shutdown();
  if(!result.OK()||!c){fprintf(stderr,"Open failed\n");return 2;}
  ResourceId output,view,parent;for(const auto &r:c->GetResources()){
    if(r.name=="Aspect output")output=r.resourceId;if(r.name=="Aspect view")view=r.resourceId;
    if(r.name=="Aspect parent")parent=r.resourceId;
  }
  rdcarray<uint32_t> events;Dispatches(c->GetRootActions(),events);
  if(events.size()!=2||output==ResourceId()||view==ResourceId()||parent==ResourceId())return 3;
  for(uint32_t eid:{events[0],events[1],events[0],0u,events[1],0u}){
    c->SetFrameEvent(eid,true);const auto bytes=c->GetBufferData(output,0,32);if(bytes.size()!=32)return 4;
    for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<4;i++){
      uint32_t word=0;memcpy(&word,bytes.data()+4*(4*stage+i),4);
      const uint32_t expected=eid && (stage==0||eid==events[1])?(stage?203u:17u)+(array?2+2*(i%2):0):0;
      if(word!=expected){fprintf(stderr,"EID %u stage %u word %u: %u != %u\n",eid,stage,i,word,expected);return 5;}
    }
    if(!eid){if(!c->GetTextureData(view,{0,0,0}).empty())return 6;continue;}
    for(unsigned mip=0;mip<(array?2u:1u);mip++)for(unsigned slice=0;slice<(array?2u:1u);slice++){
      const auto pixels=c->GetTextureData(view,{mip,slice,0});
      const size_t width=array?127u>>(1+mip):320,height=array?73u>>(1+mip):240;
      if(pixels.size()!=width*height)return 7;
      const byte expected=byte((eid==events[1]?203:17)+(array?2+mip+slice:0));
      for(byte pixel:pixels)if(pixel!=expected)return 8;
    }
  }
  c->Shutdown();RENDERDOC_ShutdownReplay();puts("PASS aspect GPU output, full projected stencil pixels, six event selections including EID0");return 0;
}
