// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>
#include "metal_texture_initial_spec.h"
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Find(const rdcarray<ActionDescription> &actions,uint32_t &dispatch,uint32_t &last)
{for(const auto &a:actions){if(a.flags&ActionFlags::Dispatch)dispatch=a.eventId;last=std::max(last,a.eventId);Find(a.children,dispatch,last);}}
int main(int argc,char **argv)
{
  if(argc!=2)return 2;
  const bool sourced=getenv("RENDERDOC_METAL_SOURCED_TEXTURE_INITIAL")!=nullptr;
  GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
  ICaptureFile *file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);IReplayController *controller=nullptr;
  if(!result.OK())return 3;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK()||!controller){fprintf(stderr,"Open failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
  const auto specs=InitialTextureSpecs(getenv("RENDERDOC_METAL_SHARED_TEXTURE_INITIAL")!=nullptr);std::vector<ResourceId> images;
  for(const auto &s:specs)
  {
    char name[64];snprintf(name,sizeof(name),"initial-%u-%u",s.format,s.type);ResourceId id;
    for(const auto *chunk:controller->GetStructuredFile().chunks)
      if(chunk->name=="MTLDevice::newTextureWithDescriptor"||chunk->name=="MTLHeap::newTexture(offset)")
      {
        const auto *desc=chunk->FindChild("descriptor");const auto *resource=chunk->FindChild("Texture");
        if(desc&&resource&&desc->FindChild("pixelFormat")->AsUInt64()==s.format&&
           desc->FindChild("textureType")->AsUInt64()==s.type&&desc->FindChild("width")->AsUInt64()==s.width&&
           desc->FindChild("height")->AsUInt64()==s.height)id=resource->AsResourceId();
      }
    if(id==ResourceId()){fprintf(stderr,"Missing %s\n",name);return 5;}images.push_back(id);
  }
  ResourceId output,table,stencilView;
  for(const auto &b:controller->GetBuffers())
  {if(b.length==8)output=b.resourceId;if(sourced&&b.length==159*24)table=b.resourceId;}
  if(sourced)
  {
    for(const auto *c:controller->GetStructuredFile().chunks)
      if(c->name=="MTLTexture::newTextureViewWithPixelFormat"&&c->FindChild("format")->AsUInt64()==261)
        stencilView=c->FindChild("View")->AsResourceId();
    if(table==ResourceId()||stencilView==ResourceId())return 16;
  }
  uint32_t dispatch=0,last=0;Find(controller->GetRootActions(),dispatch,last);
  if(!dispatch||output==ResourceId())return 6;
  size_t checked=0;
  for(unsigned cycle=0;cycle<4;cycle++)
  {
    controller->SetFrameEvent(last,true);auto overwritten=controller->GetTextureData(images[0],{0,0,0});
    if(overwritten.size()!=60||overwritten[0]!=192){fprintf(stderr,"Overwrite failed\n");return 7;}
    for(size_t i=0;i<specs.size();i++)if(!sourced&&specs[i].format==260&&specs[i].type==2)
    {
      const auto data=controller->GetTextureData(images[i],{0,0,0});float depth=0;
      if(data.size()==120)memcpy(&depth,data.data(),4);
      if(depth!=0.875f||data[4]!=99){fprintf(stderr,"Depth/stencil overwrite failed\n");return 13;}
    }
    controller->SetFrameEvent(dispatch,true);auto bytes=controller->GetBufferData(output,0,8);
    uint32_t words[2]={};if(bytes.size()==8)memcpy(words,bytes.data(),8);
    if(words[0]!=(sourced?309u:135u)||words[1]!=0xdeadbeef){fprintf(stderr,"GPU result=%u/%x\n",words[0],words[1]);return 8;}
    controller->SetFrameEvent(0,true);
    for(size_t i=0;i<specs.size();i++)
    {
      const auto &s=specs[i];
      for(unsigned slice=0;slice<InitialTextureSlices(s);slice++)for(unsigned mip=0;mip<s.mips;mip++)
      {
        const unsigned w=std::max(1u,s.width>>mip),h=std::max(1u,s.height>>mip),d=std::max(1u,s.depth>>mip);
        const unsigned rows=(h+s.block-1)/s.block,rowbytes=((w+s.block-1)/s.block)*s.bytes;
        const auto data=controller->GetTextureData(images[i],{mip,slice,0});
        if(data.size()!=size_t(rowbytes)*rows*d){fprintf(stderr,"Size fmt=%u type=%u mip=%u slice=%u actual=%zu expected=%u\n",s.format,s.type,mip,slice,data.size(),rowbytes*rows*d);return 9;}
        for(unsigned z=0;z<d;z++)for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<rowbytes;x++)
          if(data[(z*rows+y)*rowbytes+x]!=InitialTextureByte(s,mip,slice,z,y,x))
          {fprintf(stderr,"Bytes fmt=%u type=%u mip=%u slice=%u z=%u row=%u byte=%u got=%u expected=%u\n",s.format,s.type,mip,slice,z,y,x,data[(z*rows+y)*rowbytes+x],InitialTextureByte(s,mip,slice,z,y,x));return 10;}
        checked++;
      }
      if(s.format==13||s.format==23||s.format==53||s.format==73||s.format==103||s.format==123)
      {
        const auto p=controller->PickPixel(images[i],0,0,{0,0,0},CompType::Typeless);
        if(p.uintValue[0]!=17){fprintf(stderr,"UInt pixel=%u fmt=%u\n",p.uintValue[0],s.format);return 11;}
      }
      if(s.format==20)
      {
        const auto p=controller->PickPixel(images[i],0,0,{0,0,0},CompType::Typeless);
        if(std::abs(p.floatValue[0]-17.0f/65535.0f)>0.0000001f)return 15;
      }
      if(s.format==25||s.format==55||s.format==65||s.format==105||s.format==115||s.format==125)
      {
        const unsigned z=s.type==7?1:0;
        const auto p=controller->PickPixel(images[i],0,0,{0,z,0},CompType::Typeless);
        if(std::abs(p.floatValue[0]-(0.25f+z*0.0625f))>0.0001f){fprintf(stderr,"Float pixel=%f fmt=%u type=%u\n",p.floatValue[0],s.format,s.type);return 12;}
      }
      if(s.format>=250)
      {
        const auto p=controller->PickPixel(images[i],0,0,{0,0,0},CompType::Typeless);
        if(std::abs(p.floatValue[0]-0.25f)>0.00002f||(s.format==260&&std::abs(p.floatValue[1]-17.0f/255.0f)>0.00001f))
        {fprintf(stderr,"Depth pixel=%f/%f fmt=%u\n",p.floatValue[0],p.floatValue[1],s.format);return 14;}
      }
    }
    if(sourced)
    {
      const auto contents=controller->GetBufferData(table,0,159*24);
      if(contents.size()!=159*24)return 17;
      for(size_t i=0;i<159;i++)
      {
        uint64_t packet[3]={};memcpy(packet,contents.data()+i*24,24);
        const uint64_t metadata=i<specs.size()?(uint64_t(specs[i].format)<<32)|specs[i].type:(261ULL<<32)|2;
        if(packet[0]||!packet[1]||packet[2]!=metadata)return 18;
      }
      for(unsigned mip=0;mip<3;mip++)
      {
        const unsigned w=std::max(1u,3u>>mip),h=std::max(1u,5u>>mip);
        const auto data=controller->GetTextureData(stencilView,{mip,0,0});
        if(data.size()!=w*h)return 19;
        for(uint8_t value:data)if(value!=17+mip)return 20;
        const auto pixel=controller->PickPixel(stencilView,0,0,{mip,0,0},CompType::Typeless);
        // RenderDoc's shared S8 decoder exposes stencil in G, matching D32S8.
        if(pixel.floatValue[0]!=0.0f||std::abs(pixel.floatValue[1]-(17+mip)/255.0f)>0.00001f)
        {fprintf(stderr,"Stencil pixel mip=%u R=%g G=%g\n",mip,pixel.floatValue[0],pixel.floatValue[1]);return 21;}
      }
    }
  }
  controller->Shutdown();RENDERDOC_ShutdownReplay();printf("PASS %zu textures, %zu subresource checks, four overwrite/dispatch/reset seeks, UInt/float pixel decoding, sourced=%d GPU=%u\n",specs.size(),checked,sourced,sourced?309:135);return 0;
}
