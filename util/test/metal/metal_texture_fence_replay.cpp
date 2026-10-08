// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
REPLAY_PROGRAM_MARKER()
static void Dispatches(const rdcarray<ActionDescription> &actions,rdcarray<uint32_t> &events)
{for(const auto &a:actions){if(a.flags&ActionFlags::Dispatch)events.push_back(a.eventId);Dispatches(a.children,events);}}
int main(int argc,char **argv)
{
  if(argc!=3)return 1;const bool array=!strcmp(argv[2],"array");
  const bool integerBits=getenv("RENDERDOC_METAL_FENCE_INTEGER_BITS")!=nullptr;
  const bool variant=getenv("RENDERDOC_METAL_FENCE_INTEGER_VARIANT")!=nullptr;
  const bool constantTable=getenv("RENDERDOC_METAL_FENCE_CONSTANT_TABLE")!=nullptr;
  const bool constantVariant=getenv("RENDERDOC_METAL_FENCE_CONSTANT_VARIANT")!=nullptr;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",NULL);
  IReplayController *c=NULL;if(result.OK())rdctie(result,c)=file->OpenCapture(ReplayOptions(),NULL);file->Shutdown();
  if(!result.OK()||!c){fprintf(stderr,"Open failed\n");return 2;}
  ResourceId output,view,parent;for(const auto &r:c->GetResources()){
    if(r.name=="Fence output")output=r.resourceId;if(r.name=="Fence view")view=r.resourceId;
    if(r.name=="Fence parent")parent=r.resourceId;
  }
  rdcarray<uint32_t> events;Dispatches(c->GetRootActions(),events);
  if(events.size()!=2||output==ResourceId()||view==ResourceId()||parent==ResourceId())return 3;
  for(uint32_t eid:{events[0],events[1],events[0],0u,events[1],0u}){
    c->SetFrameEvent(eid,true);const auto bytes=c->GetBufferData(output,0,32);if(bytes.size()!=32)return 4;
    for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<4;i++){
      uint32_t word=0;memcpy(&word,bytes.data()+4*(4*stage+i),4);
      uint32_t expected=eid && (stage==0||eid==events[1])?stage*100+i+31+
          (integerBits?0x80000000u:0u):0;
      const uint32_t key[]={11,13,17,19,23,29},blind[]={41,43,47,53,59,61,67,71,73,79,83};
      if(expected && constantTable)expected+=(constantVariant?blind:key)[(expected^(stage*13))%(constantVariant?11:6)];
      if(word!=expected){fprintf(stderr,"EID %u stage %u word %u: %u != %u\n",eid,stage,i,word,expected);return 5;}
    }
    if(!eid){if(!c->GetTextureData(view,{0,0,0}).empty())return 6;continue;}
    for(unsigned slice=0;slice<(array?2u:1u);slice++){
      const auto pixels=c->GetTextureData(view,{0,slice,0});
      const size_t width=array?13u:variant?23u:17u,height=array?9u:variant?13u:11u;
      const size_t pixelBytes=array?16u:variant?8u:4u;
      if(pixels.size()!=width*height*pixelBytes)return 7;
      for(size_t y=0;y<height;y++)for(size_t x=0;x<width;x++)for(unsigned component=0;component<(array?4u:variant?2u:1u);component++){
        const uint32_t expected=y==0&&x<4&&(!array||x%2==slice)?(eid==events[1]?100:0)+uint32_t(x)+31+
            (integerBits?0x80000000u:0u):7;
        const byte *at=pixels.data()+(y*width+x)*pixelBytes+component*4;
        if(array){float value;memcpy(&value,at,4);if(value!=float(expected))return 8;}
        else{uint32_t value;memcpy(&value,at,4);if(value!=expected)return 8;}
      }
    }

  }
  c->Shutdown();RENDERDOC_ShutdownReplay();puts("PASS texture fence GPU output, full writable view pixels, six event selections including EID0");return 0;
}
