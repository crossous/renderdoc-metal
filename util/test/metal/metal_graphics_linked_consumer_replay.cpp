// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
REPLAY_PROGRAM_MARKER()
static bool Actions(const rdcarray<ActionDescription> &actions,rdcarray<uint32_t> &draws,uint32_t &last)
{
  for(const auto &a:actions)
  {
    last=std::max(last,a.eventId);
    if(a.flags&ActionFlags::Drawcall)
    {if(!(a.flags&ActionFlags::Indexed) || a.numIndices!=3 || a.numInstances!=1 || a.instanceOffset!=draws.size())return false;draws.push_back(a.eventId);}
    if(!Actions(a.children,draws,last))return false;
  }
  return true;
}
int main(int argc,char **argv)
{
  if(argc!=2)return 1;
  const bool gpuIndices=getenv("RENDERDOC_METAL_GRAPHICS_GPU_INDICES")!=nullptr;
  const bool early=getenv("RENDERDOC_METAL_GRAPHICS_EARLY_FRAGMENT")!=nullptr;
  const bool independent=getenv("RENDERDOC_METAL_GRAPHICS_FRAGMENT_INDEPENDENT")!=nullptr;
  const unsigned width=independent?37:16,height=independent?23:16;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
  IReplayController *controller=nullptr;
  if(result.OK())rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);
  file->Shutdown();
  if(!result.OK() || !controller){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
  ResourceId texture,table,data,color,indices;
  for(const auto &r:controller->GetResources())
  {
    if(r.name=="Linked graphics indices")indices=r.resourceId;
    if(r.name=="Linked graphics depth output")texture=r.resourceId;
    if(r.name=="Linked graphics resource heap")table=r.resourceId;
    if(r.name=="Linked graphics depth values")data=r.resourceId;
    if(r.name=="Linked graphics fragment output")color=r.resourceId;
  }
  rdcarray<uint32_t> draws;uint32_t last=0;
  if(!Actions(controller->GetRootActions(),draws,last) || draws.size()!=2 || texture==ResourceId() || table==ResourceId() || data==ResourceId() || (early && color==ResourceId()))return 5;
  unsigned events=0,descriptors=0;
  for(unsigned cycle=0;cycle<4;cycle++)
    for(uint32_t event:rdcarray<uint32_t>{draws[0],draws[1],draws[0],last,0,last,draws[1],0})
    {
      controller->SetFrameEvent(event,true);events++;
      const auto pixels=controller->GetTextureData(texture,{0,0,0});
      if(pixels.size()!=width*height*4)return 6;
      const float expected=event==0?1.0f:event==draws[0]?.25f:.75f;
      for(unsigned i=0;i<width*height;i++)
      {float value=0;memcpy(&value,pixels.data()+i*4,4);if(value!=expected){fprintf(stderr,"depth event=%u pixel=%u got=%f expected=%f\n",event,i,value,expected);return 7;}}
      if(early){const auto colors=controller->GetTextureData(color,{0,0,0});const unsigned stride=independent?8:4;
        if(colors.size()!=width*height*stride){fprintf(stderr,"color bytes event=%u actual=%zu expected=%u\n",event,colors.size(),width*height*stride);return 12;}
        for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){
          const uint8_t bgra[]={191,128,uint8_t(x%2?191:64),255};
          const uint16_t half[]={uint16_t(x%2?0x3a00:0x3400),0x3800,0x3a00,0x3c00};
          const auto pixel=colors.data()+(y*width+x)*stride;
          if(!event){for(unsigned j=0;j<stride;j++)if(pixel[j]){fprintf(stderr,"color initial event=0 pixel=%u/%u byte=%u value=%u\n",x,y,j,unsigned(pixel[j]));return 12;}}
          else if(memcmp(pixel,independent?(const void *)half:(const void *)bgra,stride)){fprintf(stderr,"color event=%u pixel=%u/%u firstbytes=%u,%u,%u,%u\n",event,x,y,unsigned(pixel[0]),unsigned(pixel[1]),unsigned(pixel[2]),unsigned(pixel[3]));return 12;}
        }
      }
      if(gpuIndices) {
        const unsigned offset=independent?16:0,stride=independent?4:2,length=independent?32:6;
        const auto bytes=controller->GetBufferData(indices,0,length);if(bytes.size()!=length)return 13;
        for(unsigned i=0;i<length;i++) {
          const bool payload=i>=offset && i<offset+stride*3;
          const unsigned ordinal=payload?(i-offset)/stride:0;
          const uint32_t expected=event?(1+ordinal+(independent?1:0))%3:ordinal;
          const unsigned char byte=payload?((expected>>(((i-offset)%stride)*8))&255):0;
          if(bytes[i]!=byte){fprintf(stderr,"index event=%u byte=%u actual=%u expected=%u\n",event,i,unsigned(bytes[i]),unsigned(byte));return 13;}
        }
      }
      if(!event)
        for(const auto &access:controller->GetDescriptorAccess())
          if(access.descriptorStore==table)return 10;
      if(event==draws[0] || event==draws[1])
      {
        const auto accesses=controller->GetDescriptorAccess();
        unsigned used=0;
        for(const auto &access:accesses)
          used+=access.stage==ShaderStage::Vertex && access.descriptorStore==table &&
              access.byteOffset==24 && access.type==DescriptorType::Buffer;
        if(used!=1)return 9;
        DescriptorRange range;range.offset=24;range.count=1;range.descriptorSize=24;range.type=DescriptorType::Buffer;
        const auto values=controller->GetDescriptors(table,{range});
        const auto locations=controller->GetDescriptorLocations(table,{range});
        if(values.size()!=1 || values[0].type!=DescriptorType::Buffer || values[0].resource!=data || values[0].byteOffset || values[0].byteSize!=16)return 8;
        if(locations.size()!=1 || locations[0].category!=DescriptorCategory::ReadOnlyResource)return 11;
        descriptors++;
      }
    }
  controller->Shutdown();RENDERDOC_ShutdownReplay();
  printf("PASS linked graphics replay: %u event/EID0 selections, %u descriptor identities, all %u depth pixels per event, color pixels=%u, fragment=%d; no RT dispatch\n",events,descriptors,width*height,early?width*height:0,int(early));
}
