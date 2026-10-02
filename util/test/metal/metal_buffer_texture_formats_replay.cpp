// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cmath>
#include "metal_texture_initial_spec.h"
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Find(const rdcarray<ActionDescription> &actions,uint32_t &dispatch,uint32_t &last)
{for(const auto &a:actions){if(a.flags&ActionFlags::Dispatch)dispatch=a.eventId;last=std::max(last,a.eventId);Find(a.children,dispatch,last);}}
int main(int argc,char **argv)
{
  if(argc!=2)return 2;GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
  ICaptureFile *file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);IReplayController *controller=nullptr;
  if(!result.OK())return 3;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK()||!controller){fprintf(stderr,"Open failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
  const auto specs=BufferTextureInitialSpecs();std::vector<ResourceId> images;
  for(const auto &s:specs)
  {
    ResourceId id;
    for(const auto *c:controller->GetStructuredFile().chunks)if(c->name=="MTLBuffer::newTextureWithDescriptor")
    {const auto *d=c->FindChild("descriptor");if(d->FindChild("pixelFormat")->AsUInt64()==s.format)id=c->FindChild("Texture")->AsResourceId();}
    if(id==ResourceId())return 5;images.push_back(id);
  }
  ResourceId output;for(const auto &b:controller->GetBuffers())if(b.length==8)output=b.resourceId;
  uint32_t dispatch=0,last=0;Find(controller->GetRootActions(),dispatch,last);if(!dispatch||output==ResourceId())return 6;
  size_t checks=0;
  for(unsigned cycle=0;cycle<4;cycle++)
  {
    controller->SetFrameEvent(last,true);auto altered=controller->GetTextureData(images[0],{0,0,0});uint32_t first=0;
    if(altered.size()!=28)return 7;memcpy(&first,altered.data(),4);if(first!=0x01020304)return 8;
    controller->SetFrameEvent(dispatch,true);auto outputBytes=controller->GetBufferData(output,0,8);uint32_t words[2]={};
    if(outputBytes.size()!=8)return 9;memcpy(words,outputBytes.data(),8);if(words[0]!=17||words[1]!=0xdeadbeef)return 10;
    controller->SetFrameEvent(0,true);
    for(size_t i=0;i<specs.size();i++)
    {
      const auto &s=specs[i];const auto data=controller->GetTextureData(images[i],{0,0,0});
      if(data.size()!=s.width*s.bytes){fprintf(stderr,"Readback size failed format=%u size=%zu\n",s.format,data.size());return 11;}
      for(unsigned x=0;x<data.size();x++)if(data[x]!=InitialTextureByte(s,0,0,0,0,x)){fprintf(stderr,"Raw bytes failed format=%u byte=%u\n",s.format,x);return 12;}
      auto pixel=controller->PickPixel(images[i],0,0,{0,0,0},CompType::Typeless);
      for(unsigned c=0;c<s.components;c++)
      {
        const unsigned raw=17+23*c;
        if(s.format==53||s.format==123||s.format==103||s.format==73||s.format==23||s.format==63)
        {if(pixel.uintValue[c]!=raw)return 13;}
        else if(s.format==54){if(pixel.intValue[c]!=-17)return 14;}
        else
        {
          const float expected=s.format==70&&c==3?1.0f:s.format==70||s.format==10?raw/255.0f:s.format==72?raw/127.0f:0.25f+0.125f*c;
          if(fabs(pixel.floatValue[c]-expected)>0.00001f){fprintf(stderr,"PickPixel failed format=%u c=%u value=%g expected=%g\n",s.format,c,pixel.floatValue[c],expected);return 15;}
        }
      }
      checks++;
    }
  }
  controller->Shutdown();RENDERDOC_ShutdownReplay();printf("PASS TextureBuffer16: %zu raw/pixel checks, 4 seek cycles, GPU17/DEADBEEF, exact parent offsets and overwrite restore\n",checks);return 0;
}
