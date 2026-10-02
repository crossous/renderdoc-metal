// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Find(const rdcarray<ActionDescription> &actions,uint32_t &dispatch,uint32_t &last)
{for(const auto &a:actions){if(a.eventId>last)last=a.eventId;if(a.flags&ActionFlags::Dispatch)dispatch=a.eventId;Find(a.children,dispatch,last);}}
int main(int argc,char **argv)
{
  if(argc!=6 && argc!=7)return 2;
  const bool privateTexture=argc==7;
  @autoreleasepool
  {
    GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    id<MTLBuffer> padding=[device newBufferWithLength:1048576 options:MTLResourceStorageModeShared];
    auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:2 height:2 mipmapped:NO];
    descriptor.storageMode=MTLStorageModeShared;
    id<MTLTexture> texturePadding=[device newTextureWithDescriptor:descriptor];
    if(!padding||!texturePadding)return 3;
    ICaptureFile *file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
    IReplayController *controller=nullptr;if(!result.OK())return 4;
    rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
    if(!result.OK()||!controller){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 5;}
    const bool frameBufferView=getenv("RENDERDOC_METAL_DESCRIPTOR_FRAME_BUFFER_VIEW")!=nullptr;
    ResourceId bufferTable,textureTable,samplerTable,output,backbuffer,initialImage,mipImage;
    for(const auto &b:controller->GetBuffers())
    {
      if(b.length==8)output=b.resourceId;
      if(b.length!=24)continue;
      const auto bytes=controller->GetBufferData(b.resourceId,0,24);uint64_t words[3]={};
      if(bytes.size()!=24)return 6;memcpy(words,bytes.data(),24);
      if(words[2]==0x1111111111111111ULL)bufferTable=b.resourceId;
      if(words[2]==0x2222222222222222ULL)textureTable=b.resourceId;
      if(words[2]==0x3333333333333333ULL)samplerTable=b.resourceId;
    }
    bool bufferTexture=false,r11Texture=false,r16Texture=false;
    for(const auto &t:controller->GetTextures())
    {
      if(t.width==2&&t.height==2)backbuffer=t.resourceId;
      if(t.width==1&&t.height==1 && (!frameBufferView || initialImage==ResourceId() || t.resourceId<initialImage)){initialImage=t.resourceId;bufferTexture=t.type==TextureType::Buffer;r11Texture=t.format.type==ResourceFormatType::R11G11B10;}
      if(t.width==192&&t.height==104){initialImage=t.resourceId;r16Texture=true;}
      if(t.width==3&&t.height==5)mipImage=t.resourceId;
    }
    uint32_t dispatch=0,last=0;Find(controller->GetRootActions(),dispatch,last);
    if(!dispatch||output==ResourceId()||bufferTable==ResourceId()||textureTable==ResourceId()||samplerTable==ResourceId())return 7;
    uint64_t initialTexture=0;
    if(frameBufferView)
    {
      controller->SetFrameEvent(0,true);
      const auto initial=controller->GetBufferData(textureTable,0,24);
      if(initial.size()!=24)return 31;memcpy(&initialTexture,initial.data()+8,8);
    }
    for(int cycle=0;cycle<4;cycle++)
    {
      if(privateTexture)
      {
        controller->SetFrameEvent(last,true);
        const auto altered=controller->GetTextureData(initialImage,{0,0,0});
        if(r16Texture)
        {
          if(altered.size()!=192*104*2)return 26;
          for(size_t i=0;i<altered.size();i+=2)
          {uint16_t value=0;memcpy(&value,altered.data()+i,2);if(value!=0x3a00)return 27;}
        }
        else if(r11Texture)
        {
          uint32_t actual=0;if(altered.size()==4)memcpy(&actual,altered.data(),4);
          if(actual!=(0x3a0u|(0x340u<<11)|(0x1c0u<<22)))return 23;
        }
        else if(altered.size()!=4 || (altered[0]!=192 && altered[2]!=192))return 16;
      }
      controller->SetFrameEvent(dispatch,true);
      const auto bytes=controller->GetBufferData(output,0,8);uint32_t words[2]={};
      if(bytes.size()!=8)return 8;memcpy(words,bytes.data(),8);
      if(words[0]!=122||words[1]!=0xdeadbeefU)return 9;
      uint64_t packet[3]={};const auto b=controller->GetBufferData(bufferTable,0,24);memcpy(packet,b.data(),24);
      if(packet[0]==strtoull(argv[2],nullptr,10)||packet[1]||packet[2]!=0x1111111111111111ULL)return 10;
      const uint64_t newVA=packet[0];
      const auto t=controller->GetBufferData(textureTable,0,24);memcpy(packet,t.data(),24);
      if(packet[0]||!packet[1]||packet[1]==strtoull(argv[4],nullptr,10)||packet[2]!=0x2222222222222222ULL)return 11;
      const uint64_t newTexture=packet[1];
      const auto s=controller->GetBufferData(samplerTable,0,24);memcpy(packet,s.data(),24);
      if(!packet[0]||packet[1]!=0x123456789abcdef0ULL||packet[2]!=0x3333333333333333ULL)return 12;
      printf("GPU PASS cycle=%d result=122 newVA=%llu textureID=%llu samplerID=%llu capturedSamplerID=%s\n",cycle,(unsigned long long)newVA,(unsigned long long)newTexture,(unsigned long long)packet[0],argv[5]);
      controller->SetFrameEvent(0,true);
      const auto reset=controller->GetBufferData(textureTable,0,24);memcpy(packet,reset.data(),24);
      if(frameBufferView ? (packet[1]!=initialTexture || packet[1]==newTexture) : packet[1]!=newTexture)return 13;
      if(privateTexture)
      {
        if(mipImage==ResourceId())return 18;
        for(unsigned mip=0;mip<3;mip++)
        {
          const unsigned width=std::max(1u,3u>>mip),height=std::max(1u,5u>>mip);
          const auto rows=controller->GetTextureData(mipImage,{mip,0,0});
          if(rows.size()!=width*height*4)return 19;
          for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)
          {
            const uint8_t *p=rows.data()+4*(y*width+x);
            if(p[0]!=17+19*mip || p[1]!=31+y || p[2]!=53+x || p[3]!=255)return 20;
          }
        }
        if(bufferTexture)
        {
          const auto picked=controller->PickPixel(initialImage,0,0,{0,0,0},CompType::Typeless);
          if(fabs(picked.floatValue[0]-64.0f/255.0f)>0.00001f || fabs(picked.floatValue[1]-128.0f/255.0f)>0.00001f ||
             fabs(picked.floatValue[2]-192.0f/255.0f)>0.00001f || picked.floatValue[3]!=1.0f)return 21;
          for(const Subresource invalid : {Subresource{1,0,0},Subresource{0,1,0},Subresource{0,0,1}})
            if(!controller->GetTextureData(initialImage,invalid).empty())return 22;
        }
        const auto original=controller->GetTextureData(initialImage,{0,0,0});
        if(r16Texture)
        {
          if(original.size()!=192*104*2)return 28;
          for(size_t i=0;i<original.size();i+=2)
          {uint16_t value=0;memcpy(&value,original.data()+i,2);if(value!=0x3400)return 29;}
          const unsigned locations[][2]={{0,0},{96,52},{191,103}};
          for(const auto &location : locations)
          {
            const auto pixel=controller->PickPixel(initialImage,location[0],location[1],{0,0,0},CompType::Typeless);
            if(pixel.floatValue[0]!=0.25f||pixel.floatValue[1]!=0.0f||pixel.floatValue[2]!=0.0f||pixel.floatValue[3]!=1.0f)return 30;
          }
        }
        else if(r11Texture)
        {
          uint32_t actual=0;if(original.size()==4)memcpy(&actual,original.data(),4);
          if(actual!=(0x340u|(0x380u<<11)|(0x1d0u<<22)))return 24;
          const auto pixel=controller->PickPixel(initialImage,0,0,{0,0,0},CompType::Typeless);
          if(pixel.floatValue[0]!=0.25f||pixel.floatValue[1]!=0.5f||pixel.floatValue[2]!=0.75f)return 25;
        }
        else if(original.size()!=4 || original[1]!=128 || original[3]!=255 ||
           !((original[0]==64 && original[2]==192)||(original[0]==192 && original[2]==64)))return 17;
      }
    }
    controller->SetFrameEvent(last,true);const auto pixels=controller->GetTextureData(backbuffer,{0,0,0});
    if(pixels.size()!=16)return 14;
    for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]!=128||pixels[i+1]!=64||pixels[i+2]!=32||pixels[i+3]!=255)return 15;
    controller->Shutdown();RENDERDOC_ShutdownReplay();
    puts("PASS UE enum Buffer0/Texture4/Sampler7: native sample result, source associations, ordinary bytes, seeks and pixels");
  }
  return 0;
}
