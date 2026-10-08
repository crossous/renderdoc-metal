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
  if(argc!=3)return 1;const bool blind=getenv("RENDERDOC_METAL_DEPTH_INDEPENDENT")!=nullptr;
  const unsigned offset=blind?96:32,tableOffset=blind?48:24,faces=blind?1:6;
  const unsigned width=blind?4:6,height=blind?3:6,stride=blind?2:4;
  bytebuf baseline;baseline.resize(3*faces*width*height*stride);
  FILE *nativePixels=fopen(argv[2],"rb");if(!nativePixels)return 2;
  const bool complete=fread(baseline.data(),1,baseline.size(),nativePixels)==baseline.size() && fgetc(nativePixels)==EOF;
  fclose(nativePixels);if(!complete)return 3;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
  IReplayController *c=nullptr;if(result.OK())rdctie(result,c)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK()||!c)return 4;
  ResourceId output,view,table;for(const auto &r:c->GetResources()){
    if(r.name=="Compare output")output=r.resourceId;if(r.name=="Compare depth view")view=r.resourceId;
    if(r.name=="Compare texture table")table=r.resourceId;
  }
  if(output==ResourceId()||view==ResourceId()||table==ResourceId())return 5;
  rdcarray<uint32_t> events;Dispatches(c->GetRootActions(),events);if(events.size()!=2)return 6;
  unsigned pixelsChecked=0;
  for(uint32_t eid:{events[0],events[1],events[0],0u,events[1],0u}){
    c->SetFrameEvent(eid,true);if(!c->GetFatalErrorStatus().OK())return 7;
    const auto bytes=c->GetBufferData(output,offset,32);if(bytes.size()!=32)return 8;
    for(unsigned stage=0;stage<2;stage++)for(unsigned i=0;i<4;i++){
      uint32_t actual=0;memcpy(&actual,bytes.data()+4*(stage*4+i),4);
      const uint32_t expected=eid && (!stage || eid==events[1])?(stage || !(i%2))+stage*10+i*2:0;
      if(actual!=expected){fprintf(stderr,"eid=%u stage=%u word=%u actual=%u expected=%u\n",eid,stage,i,actual,expected);return 9;}
    }
    const unsigned state=!eid?0:eid==events[0]?1:2;
    for(unsigned face=0;face<faces;face++){
      const auto pixels=c->GetTextureData(view,{0,face,0});if(pixels.size()!=width*height*stride)return 10;
      for(unsigned i=0;i<width*height;i++){
        const auto expectedPixel=baseline.data()+((state*faces+face)*width*height+i)*stride;
        if(memcmp(pixels.data()+i*stride,expectedPixel,stride)){
          uint32_t actual=0,expected=0;memcpy(&actual,pixels.data()+i*stride,stride);
          memcpy(&expected,expectedPixel,stride);
          fprintf(stderr,"depth eid=%u face=%u pixel=%u actual=0x%x expected=0x%x\n",eid,face,i,actual,expected);return 11;
        }
        pixelsChecked++;
      }
    }
    if(eid){DescriptorRange range;range.offset=tableOffset;range.count=1;range.descriptorSize=24;range.type=DescriptorType::Image;
      const auto values=c->GetDescriptors(table,{range});if(values.size()!=1||values[0].resource!=view)return 12;
    }
  }
  c->Shutdown();RENDERDOC_ShutdownReplay();
  printf("PASS depth comparison replay: all eight words, %u depth pixels, six event/EID0 selections\n",pixelsChecked);
}
