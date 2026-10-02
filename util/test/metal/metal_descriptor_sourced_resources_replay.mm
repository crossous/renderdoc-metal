// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Find(const rdcarray<ActionDescription> &actions,rdcarray<uint32_t> &dispatches,uint32_t &last)
{for(const auto &a:actions){if(a.eventId>last)last=a.eventId;if(a.flags&ActionFlags::Dispatch)dispatches.push_back(a.eventId);Find(a.children,dispatches,last);}}
int main(int argc,char **argv)
{
    if(argc!=8 && argc!=9)return 2;
    const bool frameSources=argc==9 && !strcmp(argv[8],"frame");
  @autoreleasepool
  {
    GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    id<MTLBuffer> padding=[device newBufferWithLength:1048576 options:MTLResourceStorageModeShared];
    auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:2 height:2 mipmapped:NO];
    descriptor.storageMode=MTLStorageModeShared;
    id<MTLTexture> texturePadding=[device newTextureWithDescriptor:descriptor];
    if(!padding||!texturePadding)return 3;
    NSMutableArray<id<MTLBuffer>> *epochPadding=[NSMutableArray new];
    ICaptureFile *file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
    IReplayController *controller=nullptr;if(!result.OK())return 4;
    rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
    if(!result.OK()||!controller){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 5;}
    ResourceId bufferTable,textureTable,samplerTable,output,backbuffer;
    for(const auto &b:controller->GetBuffers())
    {
      if(b.length==8)output=b.resourceId;
      if(b.length!=24)continue;
      const auto bytes=controller->GetBufferData(b.resourceId,0,24);uint64_t words[3]={};
      if(bytes.size()!=24)return 6;memcpy(words,bytes.data(),24);
      if(words[2]==0x1111111111111111ULL)bufferTable=b.resourceId;
      // Both tables have the same packet metadata; this fixture creates the GPU
      // destination before the payload. Use the original captured identity order.
      if(words[2]==0x2222222222222222ULL &&
         (textureTable==ResourceId() || b.resourceId<textureTable))textureTable=b.resourceId;

      if(words[2]==0x3333333333333333ULL)samplerTable=b.resourceId;
    }
    for(const auto &t:controller->GetTextures())if(t.width==2&&t.height==2)backbuffer=t.resourceId;
    rdcarray<uint32_t> dispatches;uint32_t last=0;Find(controller->GetRootActions(),dispatches,last);
    if(dispatches.size()!=3||output==ResourceId()||bufferTable==ResourceId()||textureTable==ResourceId()||samplerTable==ResourceId())
    {
      fprintf(stderr,"Missing replay fixture: dispatches=%zu output=%d buffer=%d texture=%d sampler=%d\n",dispatches.size(),output!=ResourceId(),bufferTable!=ResourceId(),textureTable!=ResourceId(),samplerTable!=ResourceId());
      for(const auto &r:controller->GetResources())fprintf(stderr,"resource name=%s\n",r.name.c_str());
      return 7;
    }
    uint64_t previousVA=0;bool frameVAChanged=false;
    for(int cycle=0;cycle<4;cycle++)
    {
      controller->SetFrameEvent(dispatches[0],true);
      auto before=controller->GetBufferData(output,0,8);uint32_t oldResult=0;
      if(before.size()!=8)return 16;memcpy(&oldResult,before.data(),4);
      if(oldResult!=strtoul(argv[6],nullptr,10))return 17;
      controller->SetFrameEvent(dispatches[1],true);
      const auto written=controller->GetBufferData(textureTable,0,24);uint64_t produced[3]={};
      if(written.size()!=24)return 18;memcpy(produced,written.data(),24);
      if(produced[0]||!produced[1]||produced[2]!=0x2222222222222222ULL)return 19;
      controller->SetFrameEvent(dispatches[2],true);
      const auto bytes=controller->GetBufferData(output,0,8);uint32_t words[2]={};
      if(bytes.size()!=8)return 8;memcpy(words,bytes.data(),8);
      if(words[0]!=strtoul(argv[7],nullptr,10)||words[1]!=0xdeadbeefU)return 9;
      uint64_t packet[3]={};const auto b=controller->GetBufferData(bufferTable,0,24);memcpy(packet,b.data(),24);
      if(packet[0]==strtoull(argv[2],nullptr,10)||packet[1]||packet[2]!=0x1111111111111111ULL)return 10;
      const uint64_t newVA=packet[0];
      if(previousVA && newVA!=previousVA)frameVAChanged=true;
      previousVA=newVA;
      const auto t=controller->GetBufferData(textureTable,0,24);memcpy(packet,t.data(),24);
      if(packet[0]||!packet[1]||packet[1]==strtoull(argv[4],nullptr,10)||packet[2]!=0x2222222222222222ULL)return 11;
      const uint64_t newTexture=packet[1];
      const auto s=controller->GetBufferData(samplerTable,0,24);memcpy(packet,s.data(),24);
      if(!packet[0]||packet[1]!=0x123456789abcdef0ULL||packet[2]!=0x3333333333333333ULL)return 12;
      printf("GPU PASS cycle=%d before=%u after=%u newVA=%llu textureID=%llu samplerID=%llu capturedSamplerID=%s\n",cycle,oldResult,words[0],(unsigned long long)newVA,(unsigned long long)newTexture,(unsigned long long)packet[0],argv[5]);
      controller->SetFrameEvent(0,true);
      const auto reset=controller->GetBufferData(textureTable,0,24);memcpy(packet,reset.data(),24);
      const auto restored=controller->GetBufferData(output,0,8);
      if(restored.size()!=8)return 13;
      if(frameSources)
      {
        // Consume the tiny allocations just released by EID0. Later frame creations
        // must re-encode their native VA rather than reuse the loading pass's bytes.
        [epochPadding addObject:[device newBufferWithLength:24 options:MTLResourceStorageModeShared]];
        [epochPadding addObject:[device newBufferWithLength:12 options:MTLResourceStorageModeShared]];
      }
    }
    if(frameSources && !frameVAChanged)return 20;
    controller->SetFrameEvent(last,true);const auto pixels=controller->GetTextureData(backbuffer,{0,0,0});
    if(pixels.size()!=16)return 14;
    for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]!=128||pixels[i+1]!=64||pixels[i+2]!=32||pixels[i+3]!=255)return 15;
    controller->Shutdown();RENDERDOC_ShutdownReplay();
    puts("PASS sourced Texture4 compute copy: GPU sampled 122/186, expected/source associations, ordinary bytes, seeks and pixels");
  }
  return 0;
}
