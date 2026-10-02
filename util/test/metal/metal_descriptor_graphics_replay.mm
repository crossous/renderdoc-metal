// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static bool CheckDraws(const rdcarray<ActionDescription> &actions,unsigned &count,unsigned indices,unsigned instances,int base,unsigned start,bool strip)
{
  for(const auto &a:actions) {
    if(a.flags&ActionFlags::Drawcall) {
      ++count;
      if(!(a.flags&ActionFlags::Indexed) || a.numIndices!=indices || a.numInstances!=instances ||
         a.baseVertex!=base || a.instanceOffset!=start || a.indexOffset!=0) {
        fprintf(stderr,"Draw metadata EID=%u count=%u instances=%u baseVertex=%d baseInstance=%u indexOffset=%u\n",a.eventId,a.numIndices,a.numInstances,a.baseVertex,a.instanceOffset,a.indexOffset);
        return false;
      }
    }
    if(!CheckDraws(a.children,count,indices,instances,base,start,strip))return false;
  }
  return true;
}
static uint32_t LastDraw(const rdcarray<ActionDescription> &actions)
{ uint32_t last=0;for(const auto &a:actions) { if(a.flags&ActionFlags::Drawcall)last=std::max(last,a.eventId);last=std::max(last,LastDraw(a.children)); } return last; }
static uint32_t FirstDraw(const rdcarray<ActionDescription> &actions)
{ for(const auto &a:actions) { if(a.flags&ActionFlags::Drawcall)return a.eventId; if(uint32_t child=FirstDraw(a.children))return child; } return 0; }
static void Find(const rdcarray<ActionDescription> &actions,rdcarray<uint32_t> &dispatches,uint32_t &last)
{for(const auto &a:actions){if(a.eventId>last)last=a.eventId;if(a.flags&ActionFlags::Dispatch)dispatches.push_back(a.eventId);Find(a.children,dispatches,last);}}
int main(int argc,char **argv)
{
    if(argc!=8 && argc!=9)return 2;
    const bool frameSources=argc==9 && !strcmp(argv[8],"frame");
    const bool privateFrame=argc==9 && !strcmp(argv[8],"private-frame");
    const bool tableAlias=argc==9 && !strcmp(argv[8],"table-alias");
    const bool futureCPU=argc==9 && !strcmp(argv[8],"future-alias-cpu");
    const unsigned updates=argc==9 && !strncmp(argv[8],"batch",5)?unsigned(strtoul(argv[8]+5,nullptr,10)):1;
    if(!updates||updates>256)return 27;
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
    if(getenv("RENDERDOC_METAL_DRAW_WORK")) {
      const bool strip=getenv("RENDERDOC_METAL_TRIANGLE_STRIP")!=nullptr;
      const char *batch=getenv("RENDERDOC_METAL_DRAW_BATCH_COUNT");
      unsigned count=0;
      if(!CheckDraws(controller->GetRootActions(),count,strip?4:6,2,strip?0:-3,7,strip) || count!=(batch?unsigned(atoi(batch)):256))return 29;
    }
    ResourceId baseTable,bufferTable,textureTable,samplerTable,output,backbuffer;
    for(const auto &b:controller->GetBuffers())
    {
      if(b.length==(futureCPU?16U:8U))output=b.resourceId;
      if(b.length!=24 && b.length!=updates*24)continue;
      const auto bytes=controller->GetBufferData(b.resourceId,0,24);uint64_t words[3]={};
      if(bytes.size()!=24)return 6;memcpy(words,bytes.data(),24);
      if(words[2]==0x1111111111111111ULL)
      {
        if(baseTable==ResourceId() || b.resourceId<baseTable)baseTable=b.resourceId;
        if(bufferTable==ResourceId() || bufferTable<b.resourceId)bufferTable=b.resourceId;
      }
      // Both tables have the same packet metadata; this fixture creates the GPU
      // destination before the payload. Use the original captured identity order.
      if(b.length==updates*24 && words[2]==0x2222222222222222ULL &&
         (textureTable==ResourceId() || b.resourceId<textureTable))textureTable=b.resourceId;

      if(words[2]==0x3333333333333333ULL)samplerTable=b.resourceId;
    }
    for(const auto &t:controller->GetTextures())if(t.width==2&&t.height==2)backbuffer=t.resourceId;
    rdcarray<uint32_t> dispatches;uint32_t last=0;Find(controller->GetRootActions(),dispatches,last);
    if(dispatches.size()!=(privateFrame?4U:3U)||output==ResourceId()||bufferTable==ResourceId()||textureTable==ResourceId()||samplerTable==ResourceId())
    {
      fprintf(stderr,"Missing replay fixture: dispatches=%zu output=%d buffer=%d texture=%d sampler=%d\n",dispatches.size(),output!=ResourceId(),bufferTable!=ResourceId(),textureTable!=ResourceId(),samplerTable!=ResourceId());
      for(const auto &r:controller->GetResources())fprintf(stderr,"resource name=%s\n",r.name.c_str());
      return 7;
    }
    if(privateFrame)dispatches.erase(0,1); // Three actual consumers follow the GPU initialization dispatch.
    uint64_t previousVA=0;bool frameVAChanged=false;
    if(tableAlias && baseTable==bufferTable)return 23;
    for(int cycle=0;cycle<4;cycle++)
    {
      controller->SetFrameEvent(dispatches[0],true);
      auto before=controller->GetBufferData(output,0,8);uint32_t oldResult=0;
      if(before.size()!=8)return 16;memcpy(&oldResult,before.data(),4);
      if(oldResult!=strtoul(argv[6],nullptr,10))
      {fprintf(stderr,"First descriptor dispatch result=%u expected=%s cycle=%d\n",oldResult,argv[6],cycle);return 17;}
      uint64_t firstVA=0;
      if(tableAlias)
      {
        auto original=controller->GetBufferData(baseTable,0,24);uint64_t packet[3]={};
        if(original.size()!=24)return 24;memcpy(packet,original.data(),24);
        if(!packet[0] || packet[1] || packet[2]!=0x1111111111111111ULL)return 25;
        firstVA=packet[0];
      }
      controller->SetFrameEvent(dispatches[1],true);
      const auto written=controller->GetBufferData(textureTable,0,updates*24);uint64_t produced[3]={};
      if(written.size()!=updates*24)return 18;
      uint64_t producerID=0;
      for(unsigned slot=0;slot<updates;slot++) {
       memcpy(produced,written.data()+slot*24,24);
       if(produced[0]||!produced[1]||produced[2]!=0x2222222222222222ULL)return 19;
       if(producerID && producerID!=produced[1])return 28;
       producerID=produced[1];
      }
      controller->SetFrameEvent(dispatches[2],true);
      const auto bytes=controller->GetBufferData(output,0,8);uint32_t words[2]={};
      if(bytes.size()!=8)return 8;memcpy(words,bytes.data(),8);
      if(words[0]!=strtoul(argv[7],nullptr,10)||words[1]!=0xdeadbeefU)return 9;
      if(futureCPU)
      {
        const auto saved=controller->GetBufferData(output,8,8);uint32_t first[2]={};
        if(saved.size()!=8)return 21;memcpy(first,saved.data(),8);
        if(first[0]!=strtoul(argv[6],nullptr,10)||first[1]!=1)return 22;
      }
      uint64_t packet[3]={};const auto b=controller->GetBufferData(bufferTable,0,24);memcpy(packet,b.data(),24);
      if(packet[0]==strtoull(argv[2],nullptr,10)||packet[1]||packet[2]!=0x1111111111111111ULL)return 10;
      const uint64_t newVA=packet[0];
      if(tableAlias && newVA!=firstVA+4)return 26;
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
    controller->SetFrameEvent(last,true);
    const auto pixels=controller->GetTextureData(backbuffer,{0,0,0});
    if(pixels.size()!=16)return 14;
    for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]!=128||pixels[i+1]!=64||pixels[i+2]!=strtoul(argv[7],nullptr,10)||pixels[i+3]!=255)return 15;
    if(getenv("RENDERDOC_METAL_POISON_INDEX_TAIL")) {
      ResourceId indexID;for(const auto &b:controller->GetBuffers())if(b.length==256 && (indexID==ResourceId() || b.resourceId<indexID))indexID=b.resourceId;
      if(indexID==ResourceId())return 33;
      const bool strip=getenv("RENDERDOC_METAL_TRIANGLE_STRIP")!=nullptr;
      const auto poisoned=controller->GetBufferData(indexID,0,strip?32:16);
      if(poisoned.size()!=(strip?32U:16U))return 33;
      for(uint8_t byte:poisoned)if(byte)return 33;
    }
    if(getenv("RENDERDOC_METAL_DRAW_WORK")) {
      controller->SetFrameEvent(LastDraw(controller->GetRootActions()),true);
      const bool strip=getenv("RENDERDOC_METAL_TRIANGLE_STRIP")!=nullptr;
      const auto &pipe=controller->GetPipelineState();const auto *metal=pipe.GetMetalPipelineState();
      if(!metal || metal->topology!=(strip?Topology::TriangleStrip:Topology::TriangleList) ||
         metal->indexBuffer.resourceId==ResourceId() || metal->indexBuffer.byteOffset!=(strip?4U:2U) ||
         metal->indexBuffer.byteStride!=(strip?4U:2U) || metal->indexBuffer.byteSize!=(strip?16U:12U)) {
        fprintf(stderr,"Indexed pipe metadata: present=%d topology=%u offset=%llu stride=%u bytes=%llu\n",metal!=nullptr,metal?unsigned(metal->topology):0,metal?(unsigned long long)metal->indexBuffer.byteOffset:0,metal?metal->indexBuffer.byteStride:0,metal?(unsigned long long)metal->indexBuffer.byteSize:0);return 30;
      }
    }
    if(getenv("RENDERDOC_METAL_LATE_INDEX_UPLOAD")) {
      const uint32_t draw=FirstDraw(controller->GetRootActions());if(!draw)return 31;
      const bool strip=getenv("RENDERDOC_METAL_TRIANGLE_STRIP")!=nullptr;
      for(unsigned cycle=0;cycle<4;cycle++) {
        controller->SetFrameEvent(0,true);controller->SetFrameEvent(draw,true);
        const auto partial=controller->GetTextureData(backbuffer,{0,0,0});
        if(partial.size()!=16)return 31;
        for(size_t i=0;i<partial.size();i+=4)if(partial[i]!=128||partial[i+1]!=64||partial[i+2]!=strtoul(argv[7],nullptr,10)||partial[i+3]!=255) {
          fprintf(stderr,"Late-upload draw seek failed EID=%u cycle=%u pixel=%u/%u/%u/%u\n",draw,cycle,partial[i],partial[i+1],partial[i+2],partial[i+3]);return 31;
        }
        const auto *metal=controller->GetPipelineState().GetMetalPipelineState();if(!metal)return 32;
        const auto index=controller->GetBufferData(metal->indexBuffer.resourceId,strip?4:2,strip?16:12);
        if(index.size()!=(strip?16U:12U))return 32;
        for(unsigned j=0;j<(strip?4U:6U);j++) {
          uint32_t value=0;memcpy(&value,index.data()+j*(strip?4:2),strip?4:2);
          if(value!=(strip?j:(j+2)%3+3))return 32;
        }
      }
      puts("PASS late-encoded/early-submitted index producer: four first-draw partial seeks and Native index bytes");
    }
    controller->Shutdown();RENDERDOC_ShutdownReplay();
    puts("PASS sourced Texture4 compute-to-graphics: GPU sampled 122/186, expected/source associations, ordinary bytes, seeks and pixels");
  }
  return 0;
}
