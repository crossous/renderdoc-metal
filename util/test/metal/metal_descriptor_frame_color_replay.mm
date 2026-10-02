// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Actions(const rdcarray<ActionDescription> &as,uint32_t &dispatch,uint32_t &last)
{for(const auto &a:as){last=last>a.eventId?last:a.eventId;if(a.flags&ActionFlags::Dispatch)dispatch=a.eventId;Actions(a.children,dispatch,last);}}
int main(int argc,char **argv)
{
 if(argc!=3)return 2;
 @autoreleasepool {
 id<MTLDevice> native=MTLCreateSystemDefaultDevice();
 NSMutableArray<id<MTLTexture>> *padding=[NSMutableArray new];
 auto paddingDesc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:2 height:2 mipmapped:NO];
 paddingDesc.storageMode=MTLStorageModeShared;
 for(unsigned i=0;i<32;i++){id<MTLTexture> t=[native newTextureWithDescriptor:paddingDesc];if(!t||!t.gpuResourceID._impl)return 13;[padding addObject:t];}
 GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
 auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
 if(!result.OK())return 3;IReplayController *controller=nullptr;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
 if(!result.OK()||!controller){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
 ResourceId table,output,image,arena;
 for(const auto &r:controller->GetResources()) {if(r.name=="Frame color table")table=r.resourceId;if(r.name=="Frame color output")output=r.resourceId;if(r.name=="Retired descriptor uniform arena")arena=r.resourceId;}
 for(const auto &b:controller->GetBuffers()) {
  if(b.length==16)output=b.resourceId;
  if(b.length==24){const auto data=controller->GetBufferData(b.resourceId,0,24);uint64_t packet[3]={};
   if(data.size()==24){memcpy(packet,data.data(),24);if(packet[2]==0x4141414141414141ULL)table=b.resourceId;}}
 }
 for(const auto *c:controller->GetStructuredFile().chunks)if(c->name=="MTLBuffer::DescriptorSlotBinding" && c->FindChild("kind")->AsUInt64()==1) {
  image=c->FindChild("resource")->AsResourceId();
 }
 bool r16=false;for(const auto &t:controller->GetTextures())if(t.resourceId==image)r16=t.format.compCount==1;
 uint32_t dispatch=0,last=0;Actions(controller->GetRootActions(),dispatch,last);
 if(!dispatch||table==ResourceId()||output==ResourceId()||image==ResourceId()){fprintf(stderr,"Missing dispatch=%u table=%d output=%d image=%d chunks=%zu\n",dispatch,table!=ResourceId(),output!=ResourceId(),image!=ResourceId(),controller->GetStructuredFile().chunks.size());for(const auto &r:controller->GetResources())fprintf(stderr,"resource %s\n",r.name.c_str());return 5;}
 auto checkImage=[&](bool overwrite) {
  const auto data=controller->GetTextureData(image,{0,0,0});const size_t step=r16?2:4;
  if(data.size()!=192*104*step)return false;
  const uint32_t packed=overwrite?(0x3a0u|(0x340u<<11)|(0x1c0u<<22)):(0x340u|(0x380u<<11)|(0x1d0u<<22));
  for(size_t i=0;i<data.size();i+=step) {
   uint32_t value=0;memcpy(&value,data.data()+i,step);if(value!=(r16?(overwrite?0x3a00u:0x3400u):packed))return false;
  }
  for(const auto &location:rdcarray<rdcpair<uint32_t,uint32_t>>{{0,0},{96,52},{191,103}}) {
   const auto pixel=controller->PickPixel(image,location.first,location.second,{0,0,0},CompType::Typeless);
   if(pixel.floatValue[0]!=(overwrite?0.75f:0.25f)||pixel.floatValue[1]!=(r16?0.f:overwrite?0.25f:0.5f)||pixel.floatValue[2]!=(r16?0.f:overwrite?0.5f:0.75f))return false;
  }
  return true;
 };
 for(unsigned cycle=0;cycle<4;cycle++) {
  controller->SetFrameEvent(last,true);if(!checkImage(true))return 6;
  controller->SetFrameEvent(dispatch,true);if(!checkImage(false))return 7;
  auto data=controller->GetBufferData(output,0,16);uint32_t words[4]={};if(data.size()!=16)return 8;memcpy(words,data.data(),16);
  if(words[0]!=64||words[1]!=(r16?0:128)||words[2]!=(r16?0:192)||words[3]!=0xdeadbeef) {
   fprintf(stderr,"GPU frame color result mismatch: %u/%u/%u marker=%08x\n",words[0],words[1],words[2],words[3]);return 9;
  }
  if(arena!=ResourceId()) {
   const uint32_t constants[]={13,17,19,23,29,31};
   const auto bytes=controller->GetBufferData(arena,0,24);
   if(bytes.size()!=sizeof(constants)||memcmp(bytes.data(),constants,sizeof(constants)))return 14;
  }
  data=controller->GetBufferData(table,0,24);uint64_t packet[3]={};if(data.size()!=24)return 10;memcpy(packet,data.data(),24);
  if(packet[0]||!packet[1]||packet[1]==strtoull(argv[2],nullptr,10)||packet[2]!=0x4141414141414141ULL)return 11;
  printf("GPU frame color PASS cycle=%u pixels=19968 result=%u/%u/%u NativeID=%llu\n",cycle,words[0],words[1],words[2],(unsigned long long)packet[1]);
  controller->SetFrameEvent(0,true);data=controller->GetBufferData(table,0,24);memcpy(packet,data.data(),24);
  if(packet[0]||packet[1]||packet[2])return 12;
  if(arena!=ResourceId()) {
   const uint32_t zero[6]={};const auto bytes=controller->GetBufferData(arena,0,24);
   if(bytes.size()!=sizeof(zero)||memcmp(bytes.data(),zero,sizeof(zero)))return 15;
  }
 }
 controller->Shutdown();RENDERDOC_ShutdownReplay();puts("PASS frame color full pixels, PickPixel, typed GPU ID and zero-at-EID0");return 0;
 }
}
