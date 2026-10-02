// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void FindDraws(const rdcarray<ActionDescription> &actions,rdcarray<ActionDescription> &draws,unsigned &begins,unsigned &ends)
{for(const auto &a:actions){if(a.flags&ActionFlags::Drawcall)draws.push_back(a);if(a.flags&ActionFlags::BeginPass)begins++;if(a.flags&ActionFlags::EndPass)ends++;FindDraws(a.children,draws,begins,ends);}}

static void Find(const rdcarray<ActionDescription> &actions,rdcarray<uint32_t> &dispatches,uint32_t &last)
{for(const auto &a:actions){if(a.eventId>last)last=a.eventId;if(a.flags&ActionFlags::Dispatch)dispatches.push_back(a.eventId);Find(a.children,dispatches,last);}}
static bool HasMarker(const rdcarray<ActionDescription> &actions,const char *name)
{for(const auto &a:actions)if(a.customName==name || HasMarker(a.children,name))return true;return false;}
static void FindIndirect(const rdcarray<ActionDescription> &actions,rdcarray<ActionDescription> &indirect)
{for(const auto &a:actions){if((a.flags&ActionFlags::Dispatch)&&(a.flags&ActionFlags::Indirect))indirect.push_back(a);FindIndirect(a.children,indirect);}}
int main(int argc,char **argv)
{
    if(argc!=8 && argc!=9)return 2;
    const char *depthFormat=getenv("RENDERDOC_METAL_MRT_DEPTH_FORMAT");
    const bool depthPass=depthFormat!=nullptr; const bool stencilPass=depthPass && !strcmp(depthFormat,"d32s8");
    const bool depthOnlyPass=getenv("RENDERDOC_METAL_MRT_DEPTH_ONLY")!=nullptr;
    auto checkDepth=[&](IReplayController *replay,const MetalPipe::State *state,unsigned draw)->bool {
      if(!depthPass)return true;
      if(!state || state->depthTarget.resource==ResourceId() ||
         state->depthStencil.depthFunction!=(draw?CompareFunction::Equal:CompareFunction::LessEqual) ||
         state->depthStencil.depthWrites!=(draw==0) || (stencilPass && !state->depthStencil.stencilEnabled)) { fprintf(stderr,"Depth state draw=%u present=%d target=%d func=%u writes=%d stencil=%d expectedStencil=%d\n",draw,state!=nullptr,state && state->depthTarget.resource!=ResourceId(),state?unsigned(state->depthStencil.depthFunction):999,state?state->depthStencil.depthWrites:0,state?state->depthStencil.stencilEnabled:0,stencilPass);return false; }
      if(stencilPass && state->depthStencil.frontFace.reference!=(draw?8U:7U))return false;
      const auto bytes=replay->GetTextureData(state->depthTarget.resource,{0,0,0});
      const unsigned stride=stencilPass?8U:!strcmp(depthFormat,"d16")?2U:4U;
      if(bytes.size()!=4*stride){fprintf(stderr,"Depth bytes size=%zu expected=%u\n",bytes.size(),4*stride);return false;}
      for(unsigned i=0;i<bytes.size();i++)if(bytes[i]!=(stencilPass && i%stride==4?8U:0U)){fprintf(stderr,"Depth raw byte %u=%u\n",i,bytes[i]);return false;}
      return true;
    };
    const bool frameSources=argc==9 && !strcmp(argv[8],"frame");
    const uint64_t capturedFrameTexture=argc==9 && !strncmp(argv[8],"frame-texture:",14) ? strtoull(argv[8]+14,nullptr,10) : 0;
  @autoreleasepool
  {
    GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    id<MTLBuffer> padding=[device newBufferWithLength:1048576 options:MTLResourceStorageModeShared];
    auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:2 height:2 mipmapped:NO];
    descriptor.storageMode=MTLStorageModeShared;
    id<MTLTexture> texturePadding=[device newTextureWithDescriptor:descriptor];
    if(!padding||!texturePadding)return 3;
    NSMutableArray<id<MTLTexture>> *identityPadding=[NSMutableArray new];
    if(capturedFrameTexture)
      for(unsigned i=0;i<16;i++)
      {
        id<MTLTexture> texture=[device newTextureWithDescriptor:descriptor];
        if(!texture || !texture.gpuResourceID._impl)return 35;
        [identityPadding addObject:texture];
      }
    NSMutableArray<id<MTLBuffer>> *epochPadding=[NSMutableArray new];
    ICaptureFile *file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
    IReplayController *controller=nullptr;if(!result.OK())return 4;
    rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
    if(!result.OK()||!controller){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 5;}
    ResourceId bufferTable,textureTable,samplerTable,output,backbuffer,resolveTable,firstAliasOutput;
    ResourceId visibilityOutput;
    if(getenv("RENDERDOC_METAL_MRT_VISIBILITY"))
      for(const auto *chunk:controller->GetStructuredFile().chunks)
        if(chunk->name=="MTLCommandBuffer::renderCommandEncoderWithDescriptor" || chunk->name=="MTLCommandBuffer::parallelRenderCommandEncoderWithDescriptor") {
          const auto *descriptor=chunk->FindChild("descriptor");
          const auto *buffer=descriptor?descriptor->FindChild("visibilityResultBuffer"):nullptr;
          if(buffer && buffer->AsResourceId()!=ResourceId())visibilityOutput=buffer->AsResourceId();
        }
    auto checkVisibility=[&](uint64_t count,bool completed=false) {
      if(!getenv("RENDERDOC_METAL_MRT_VISIBILITY"))return true;
      if(visibilityOutput==ResourceId())return false;
      const auto data=controller->GetBufferData(visibilityOutput,0,32);uint64_t words[4]={};
      if(data.size()!=32)return false;memcpy(words,data.data(),32);
      fprintf(stderr,"Replay visibility requested=%llu values=%llu/%llu/%llu/%llu\n",(unsigned long long)count,(unsigned long long)words[0],(unsigned long long)words[1],(unsigned long long)words[2],(unsigned long long)words[3]);
      return words[0]==(completed?0:0x1234) && words[1]==count && words[2]==0x5678 && words[3]==0xdead;
    };
    for(const auto &resource:controller->GetResources())if(resource.name=="Resource MRT resolve")resolveTable=resource.resourceId;
    for(const auto &b:controller->GetBuffers())
    {
      if(b.length==8)output=b.resourceId;
      if(b.length==16)firstAliasOutput=b.resourceId;
      if(b.length!=24 && !(getenv("RENDERDOC_METAL_MRT_UNUSED_PRODUCER_SLOT") && b.length==48))continue;
      const auto bytes=controller->GetBufferData(b.resourceId,0,24);uint64_t words[3]={};
      if(bytes.size()!=24)return 6;memcpy(words,bytes.data(),24);
      if(words[2]==0x1111111111111111ULL)bufferTable=b.resourceId;
      // Both tables have the same packet metadata; this fixture creates the GPU
      // destination before the payload. Use the original captured identity order.
      if(words[2]==0x2222222222222222ULL)
      {
        if(textureTable==ResourceId() || b.resourceId<textureTable)textureTable=b.resourceId;
        // The resolve packet is created last among the texture packets in this fixture.
        if(resolveTable==ResourceId() || resolveTable<b.resourceId)resolveTable=b.resourceId;
      }

      if(words[2]==0x3333333333333333ULL)samplerTable=b.resourceId;
    }
    if(capturedFrameTexture && resolveTable==ResourceId())return 30;
    for(const auto &t:controller->GetTextures())if(t.width==2&&t.height==2)backbuffer=t.resourceId;
    rdcarray<uint32_t> dispatches;uint32_t last=0;Find(controller->GetRootActions(),dispatches,last);
    if(getenv("RENDERDOC_METAL_MRT_BLIT_MARKERS") &&
       (!HasMarker(controller->GetRootActions(),"Sourced tail copy") ||
        !HasMarker(controller->GetRootActions(),"Nested tail copy") ||
        !HasMarker(controller->GetRootActions(),"Tail source reset")))return 75;
    uint32_t emptyDispatch=0;
    if(getenv("RENDERDOC_METAL_MRT_ZERO_ARGUMENT_COMPUTE"))
    {
      if(dispatches.size()!=4)return 70;emptyDispatch=dispatches[2];dispatches.erase(2);
    }
    uint32_t deadSlotTableBorrowDispatch=0;
    if(getenv("RENDERDOC_METAL_MRT_DEAD_SLOT_TABLE_BORROW")) {
      if(dispatches.size()!=5)return 88;deadSlotTableBorrowDispatch=dispatches[3];dispatches.erase(3);
    }
    uint32_t reusedDispatch=0;
    if(getenv("RENDERDOC_METAL_MRT_SUBMITTED_GPU_REUSE")) {
      if(dispatches.size()!=4)return 82;reusedDispatch=dispatches.back();dispatches.pop_back();
    }
    if(dispatches.size()!=(getenv("RENDERDOC_METAL_MRT_ZERO_INDIRECT")?5:3)||output==ResourceId()||bufferTable==ResourceId()||textureTable==ResourceId()||samplerTable==ResourceId())
    {
      fprintf(stderr,"Missing replay fixture: dispatches=%zu output=%d buffer=%d texture=%d sampler=%d\n",dispatches.size(),output!=ResourceId(),bufferTable!=ResourceId(),textureTable!=ResourceId(),samplerTable!=ResourceId());
      for(const auto &r:controller->GetResources())fprintf(stderr,"resource name=%s\n",r.name.c_str());
      return 7;
    }
    rdcarray<ActionDescription> draws;unsigned begins=0,ends=0;FindDraws(controller->GetRootActions(),draws,begins,ends);
    if(getenv("RENDERDOC_METAL_MRT_COMPUTE_INDIRECT")) {
      rdcarray<ActionDescription> indirect;FindIndirect(controller->GetRootActions(),indirect);
      const unsigned count=getenv("RENDERDOC_METAL_MRT_ZERO_INDIRECT")?3:2;
      if(indirect.size()!=count)return 56;
      for(unsigned i=0;i<count;i++)if(indirect[i].dispatchDimension[0]!=(i==2?0:i+1) ||
          indirect[i].dispatchDimension[1]!=1 || indirect[i].dispatchDimension[2]!=1)return 57;
    }
    const bool renderIndirect=getenv("RENDERDOC_METAL_MRT_RENDER_INDIRECT")!=nullptr;
    const bool renderIndexed=getenv("RENDERDOC_METAL_MRT_RENDER_INDEXED")!=nullptr;
    const bool renderZero=getenv("RENDERDOC_METAL_MRT_RENDER_ZERO")!=nullptr;
    ActionDescription zeroRenderDraw;
    if(renderZero)
    {
      if(draws.empty())return 58;
      zeroRenderDraw=draws.back();draws.pop_back();
      if(!(zeroRenderDraw.flags&ActionFlags::Indirect) || zeroRenderDraw.numIndices!=0 ||
         zeroRenderDraw.numInstances!=1 || zeroRenderDraw.instanceOffset!=2)return 59;
    }
    ActionDescription depthOnlyDraw;
    if(depthOnlyPass) {
      if(draws.size()!=3 || draws[0].depthOut==ResourceId())return 51;
      depthOnlyDraw=draws[0];
      for(ResourceId output:depthOnlyDraw.outputs)if(output!=ResourceId())return 52;
      draws.erase(0); const unsigned extra=getenv("RENDERDOC_METAL_PARALLEL_MRT")?2:1;
      if(begins<extra || ends<extra)return 53; begins-=extra;ends-=extra;
    }
    if(renderIndirect) { if(!begins || !ends)return 65;begins--;ends--; } // tail-zero blit
    const bool fiveTargets=draws.size()==2 && draws[0].outputs[4]!=ResourceId();
    if(emptyDispatch) { if(!begins || !ends)return 74;begins--;ends--; } // extra zero-argument compute scope
    if(deadSlotTableBorrowDispatch) {if(!begins || !ends)return 89;begins--;ends--;}
    if(reusedDispatch) {if(!begins || !ends)return 83;begins--;ends--;}
    const bool parallelPass=begins==7;
    if(draws.size()!=2 || begins!=(parallelPass?7:5) || ends!=begins || draws[0].outputs[0]==ResourceId() || draws[0].outputs[1]==ResourceId() ||
       draws[1].outputs[0]!=draws[0].outputs[0] || draws[1].outputs[1]!=ResourceId())return 21;
    if(fiveTargets)for(unsigned slot=2;slot<5;slot++)
      if(draws[0].outputs[slot]==ResourceId() || draws[1].outputs[slot]!=ResourceId())return 28;
    if(renderIndirect)
      for(unsigned i=0;i<2;i++)
        if(!(draws[i].flags&ActionFlags::Indirect) || draws[i].numIndices!=(i?6U:3U) ||
           draws[i].numInstances!=1 || draws[i].instanceOffset!=2 ||
           bool(draws[i].flags&ActionFlags::Indexed)!=renderIndexed)return 60;
    ResourceId renderArguments;
    backbuffer=draws[1].outputs[0];const ResourceId intermediate=draws[0].outputs[1];
    uint64_t previousVA=0;bool frameVAChanged=false;
    for(int cycle=0;cycle<4;cycle++)
    {
      controller->SetFrameEvent(dispatches[0],true);
      auto before=controller->GetBufferData(output,0,8);uint32_t oldResult=0;
      if(before.size()!=8)return 16;memcpy(&oldResult,before.data(),4);
      if(oldResult!=strtoul(argv[6],nullptr,10))return 17;
      if(firstAliasOutput!=ResourceId())
      {
        const auto saved=controller->GetBufferData(firstAliasOutput,0,16);uint32_t first[4]={};
        if(saved.size()!=16)return 33;memcpy(first,saved.data(),16);
        if(first[0]!=oldResult||first[1]!=0xdeadbeefU)return 34;
        printf("CROSS first GPU buffer read PASS cycle=%d value=%u\n",cycle,first[0]);
      }
      controller->SetFrameEvent(dispatches[1],true);
      const auto written=controller->GetBufferData(textureTable,0,24);uint64_t produced[3]={};
      if(written.size()!=24)return 18;memcpy(produced,written.data(),24);
      if(produced[0]||!produced[1]||produced[2]!=0x2222222222222222ULL)return 19;
      if(emptyDispatch)
      {
        controller->SetFrameEvent(emptyDispatch,true);
        const auto unchanged=controller->GetBufferData(output,0,8);uint32_t value=0;
        if(unchanged.size()!=8)return 71;memcpy(&value,unchanged.data(),4);
        if(value!=strtoul(argv[6],nullptr,10))return 72;
        const auto *emptyState=controller->GetPipelineState().GetMetalPipelineState();
        if(!emptyState || !emptyState->computeShader.reflection ||
           !emptyState->computeShader.reflection->constantBlocks.empty() ||
           !emptyState->computeShader.reflection->readOnlyResources.empty() ||
           !emptyState->computeShader.reflection->readWriteResources.empty())return 73;
      }
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
      if(capturedFrameTexture)
      {
        const auto resolved=controller->GetBufferData(resolveTable,0,24);uint64_t packet[3]={};
        if(resolved.size()!=24)return 31;memcpy(packet,resolved.data(),24);
        if(packet[0]||!packet[1]||packet[1]==capturedFrameTexture||packet[2]!=0x2222222222222222ULL)return 32;
        printf("FRAME texture ID PASS cycle=%d captured=%llu replay=%llu\n",cycle,(unsigned long long)capturedFrameTexture,(unsigned long long)packet[1]);
      }
      if(depthOnlyPass) {
        controller->SetFrameEvent(depthOnlyDraw.eventId,true);
        const auto *depthOnlyState=controller->GetPipelineState().GetMetalPipelineState();
        if(!depthOnlyState || depthOnlyState->fragmentShader.resourceId!=ResourceId() ||
           depthOnlyState->vertexShader.resourceId==ResourceId() ||
           depthOnlyState->depthTarget.resource!=depthOnlyDraw.depthOut || !checkDepth(controller,depthOnlyState,0))return 54;
        for(const auto &target:depthOnlyState->colorTargets)if(target.resource!=ResourceId())return 55;
      }
      controller->SetFrameEvent(draws[0].eventId,true);
      if(!checkVisibility(4))return 76;
      const auto *state=controller->GetPipelineState().GetMetalPipelineState();
      if(!state || state->colorTargets.size()<2 || state->colorTargets[0].resource!=backbuffer || state->colorTargets[1].resource!=intermediate ||
         state->fragmentBuffers.empty() || state->fragmentBuffers[0].byteSize!=48 || state->vertexStorageBuffers.empty() || state->vertexStorageBuffers[0].byteSize!=48)return 22;
      if(!checkDepth(controller,state,0))return 36;
      if(renderIndirect)
      {
        const auto *state=controller->GetPipelineState().GetMetalPipelineState();
        if(!state || state->indirectBuffer.resourceId==ResourceId() || state->indirectBuffer.byteOffset!=16)return 61;
        renderArguments=state->indirectBuffer.resourceId;
      }
      const auto firstPixels=controller->GetTextureData(intermediate,{0,0,0});
      if(firstPixels.size()!=16)return 23;
      for(size_t i=0;i<firstPixels.size();i+=4)if(firstPixels[i]!=strtoul(argv[7],nullptr,10)||firstPixels[i+1]!=64||firstPixels[i+2]!=128||firstPixels[i+3]!=255)return 24;
      if(fiveTargets)for(unsigned slot=2;slot<5;slot++)
      {
        if(state->colorTargets.size()<=slot || state->colorTargets[slot].resource!=draws[0].outputs[slot])return 29;
        const auto extra=controller->GetTextureData(draws[0].outputs[slot],{0,0,0});
        if(extra.size()!=16)return 30;
        for(size_t i=0;i<extra.size();i+=4)if(extra[i]!=strtoul(argv[7],nullptr,10)||extra[i+1]!=64||extra[i+2]!=128||extra[i+3]!=255)return 31;
      }
      controller->SetFrameEvent(draws[1].eventId,true);
      if(!checkVisibility(4,true))return 78;
      state=controller->GetPipelineState().GetMetalPipelineState();
      if(!state || state->colorTargets.empty() || state->colorTargets[0].resource!=backbuffer ||
          (state->colorTargets.size()>1 && state->colorTargets[1].resource!=ResourceId()))return 25;
      if(!checkDepth(controller,state,1))return 37;
      if(renderIndirect)
      {
        const auto *state=controller->GetPipelineState().GetMetalPipelineState();
        if(!state || state->indirectBuffer.resourceId!=renderArguments || state->indirectBuffer.byteOffset!=64)return 62;
      }
      if(renderZero)controller->SetFrameEvent(zeroRenderDraw.eventId,true);
      const auto finalPixels=controller->GetTextureData(backbuffer,{0,0,0});
      if(finalPixels.size()!=16)return 26;
      for(size_t i=0;i<finalPixels.size();i+=4)if(finalPixels[i]!=128||finalPixels[i+1]!=64||finalPixels[i+2]!=strtoul(argv[7],nullptr,10)||finalPixels[i+3]!=255)return 27;
      if(getenv("RENDERDOC_METAL_MRT_LOGICAL_GPU_RETIREMENT")) {
        const auto preserved=controller->GetBufferData(textureTable,0,24);uint64_t retired[3]={};
        if(preserved.size()!=24)return 80;memcpy(retired,preserved.data(),24);
        if(retired[0] || !retired[1] || retired[2]!=0x2222222222222222ULL)return 81;
      }
      if(deadSlotTableBorrowDispatch) {
        controller->SetFrameEvent(deadSlotTableBorrowDispatch,true);
        auto data=controller->GetBufferData(output,0,8);uint32_t value=0;if(data.size()!=8)return 90;memcpy(&value,data.data(),4);
        if(value!=strtoul(argv[7],nullptr,10))return 91;
      }
      if(reusedDispatch) {
        controller->SetFrameEvent(reusedDispatch,true);auto result=controller->GetBufferData(output,0,8);uint32_t reuse[2]={};
        if(result.size()!=8)return 84;memcpy(reuse,result.data(),8);
        if(reuse[0]!=strtoul(argv[7],nullptr,10)||reuse[1]!=0xdeadbeefU)return 85;
        auto data=controller->GetBufferData(textureTable,0,24);uint64_t native[3]={};
        if(data.size()!=24)return 86;memcpy(native,data.data(),24);
        if(native[0]||!native[1]||native[2]!=0x2222222222222222ULL)return 87;
      }
      controller->SetFrameEvent(0,true);
      if(!checkVisibility(0))return 77;
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
    if(!checkVisibility(4,true))return 79;
    if(renderIndirect)
    {
      const auto bytes=controller->GetBufferData(renderArguments,0,128);if(bytes.size()!=128)return 63;
      for(auto value:bytes)if(value)return 64;
    }
    const auto pixels=controller->GetTextureData(backbuffer,{0,0,0});
    if(pixels.size()!=16)return 14;
    for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]!=128||pixels[i+1]!=64||pixels[i+2]!=strtoul(argv[7],nullptr,10)||pixels[i+3]!=255)return 15;
    controller->Shutdown();RENDERDOC_ShutdownReplay();
    printf("PASS sourced MRT/cross-pass: targets=%u parallel=%u scopes=%u/%u GPU sampled 122/186, ordinary bytes, seeks and every attachment pixel\n",fiveTargets?5U:2U,parallelPass?1U:0U,begins,ends);
  }
  return 0;
}
