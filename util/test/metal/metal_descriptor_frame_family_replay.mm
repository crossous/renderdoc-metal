// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Actions(const rdcarray<ActionDescription> &as,uint32_t &dispatch,uint32_t &last)
{for(const auto &a:as){last=last>a.eventId?last:a.eventId;if(a.flags&ActionFlags::Dispatch){static unsigned count=0;count++;if(count==2)dispatch=a.eventId;}Actions(a.children,dispatch,last);}}
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
 ResourceId table,output,image;
 for(const auto &r:controller->GetResources()) {if(r.name=="Frame color table")table=r.resourceId;if(r.name=="Frame color output")output=r.resourceId;}
 for(const auto &b:controller->GetBuffers()) {
  if(b.length==16)output=b.resourceId;
  if(b.length==24){const auto data=controller->GetBufferData(b.resourceId,0,24);uint64_t packet[3]={};
   if(data.size()==24){memcpy(packet,data.data(),24);if(packet[2]==0x4141414141414141ULL)table=b.resourceId;}}
 }
 for(const auto *c:controller->GetStructuredFile().chunks)if(c->name=="MTLBuffer::DescriptorSlotBinding" && c->FindChild("kind")->AsUInt64()==1) {
  image=c->FindChild("resource")->AsResourceId();
 }
 TextureDescription imageDesc;
 for(const auto &t:controller->GetTextures())if(t.resourceId==image)imageDesc=t;
 const bool array=imageDesc.type==TextureType::Texture2DArray;
 const bool half=imageDesc.format.compByteWidth==2;
 const bool integer=imageDesc.format.compType==CompType::UInt;
 const bool unorm8=imageDesc.format.compType==CompType::UNorm && imageDesc.format.type==ResourceFormatType::Regular && imageDesc.format.compByteWidth==1;
 const bool packed10=imageDesc.format.type==ResourceFormatType::R10G10B10A2;
 const bool scalar=imageDesc.format.compCount==1;
 uint32_t dispatch=0,last=0;Actions(controller->GetRootActions(),dispatch,last);
 if(!dispatch||table==ResourceId()||output==ResourceId()||image==ResourceId()){fprintf(stderr,"Missing dispatch=%u table=%d output=%d image=%d chunks=%zu\n",dispatch,table!=ResourceId(),output!=ResourceId(),image!=ResourceId(),controller->GetStructuredFile().chunks.size());for(const auto &r:controller->GetResources())fprintf(stderr,"resource %s\n",r.name.c_str());return 5;}
 unsigned writes=0;
 for(const auto &use:controller->GetUsage(image))if(use.usage==ResourceUsage::CS_RWResource)writes++;
 if(writes<2){fprintf(stderr,"Missing typed source usage: writes=%u\n",writes);return 14;}
 printf("PASS typed array/volume CS_RW usages=%u\n",writes);
 auto checkImage=[&](bool overwrite) {
  const size_t bytes=imageDesc.format.compByteWidth*imageDesc.format.compCount;
  for(unsigned slice=0;slice<(array?imageDesc.arraysize:1u);slice++) {
   const auto data=controller->GetTextureData(image,{0,slice,0});
   if(data.size()!=size_t(imageDesc.width)*imageDesc.height*bytes*(array?1:imageDesc.depth))return false;
   const float values[]={overwrite?0.75f:0.25f,overwrite?0.25f:0.5f,overwrite?0.5f:0.75f,1.f};
   const uint16_t halves[]={uint16_t(overwrite?0x3a00:0x3400),uint16_t(overwrite?0x3400:0x3800),uint16_t(overwrite?0x3800:0x3a00),0x3c00};
   for(size_t i=0;i<data.size();i+=bytes) {
    if(packed10) {uint32_t v=0;memcpy(&v,data.data()+i,4);const uint32_t expected=(overwrite?767u:256u)|((overwrite?256u:512u)<<10)|((overwrite?512u:767u)<<20)|(3u<<30);if(v!=expected)return false;continue;}
    if(unorm8) {if(data[i]!=(overwrite?191:64))return false;continue;}
    if(integer) {uint32_t v=0;memcpy(&v,data.data()+i,4);if(v!=(overwrite?192u:64u))return false;continue;}
    for(unsigned c=0;c<imageDesc.format.compCount;c++) {
    if(half){uint16_t v=0;memcpy(&v,data.data()+i+2*c,2);if(v!=halves[c])return false;}
    else {float v=0;memcpy(&v,data.data()+i+4*c,4);if(v!=values[c])return false;}
   }}
  }
  const float values[]={overwrite?0.75f:0.25f,overwrite?0.25f:0.5f,overwrite?0.5f:0.75f,1.f};
  const unsigned layers=array?imageDesc.arraysize:imageDesc.depth;
  for(unsigned slice : {0u,layers-1})for(const auto &location:rdcarray<rdcpair<uint32_t,uint32_t>>{{0,0},{imageDesc.width/2,imageDesc.height/2},{imageDesc.width-1,imageDesc.height-1}}) {
   const auto pixel=controller->PickPixel(image,location.first,location.second,{0,slice,0},CompType::Typeless);
   for(unsigned c=0;c<4;c++) {
    if(integer){const uint32_t expected=c==0?(overwrite?192:64):0;if(pixel.uintValue[c]!=expected)return false;}
    else if(unorm8){const float expected=c==0?float(overwrite?191:64)/255.f:c==3?1.f:0.f;if(pixel.floatValue[c]!=expected)return false;}
    else if(packed10){const float expected=c==0?float(overwrite?767:256)/1023.f:c==1?float(overwrite?256:512)/1023.f:c==2?float(overwrite?512:767)/1023.f:1.f;if(pixel.floatValue[c]!=expected)return false;}
    else if(pixel.floatValue[c]!=(scalar&&c>0&&c<3?0.f:values[c]))return false;
   }
  }
  return true;
 };
 for(unsigned cycle=0;cycle<4;cycle++) {
  controller->SetFrameEvent(last,true);if(!checkImage(true))return 6;
  controller->SetFrameEvent(dispatch,true);if(!checkImage(false))return 7;
  auto data=controller->GetBufferData(output,0,16);uint32_t words[4]={};if(data.size()!=16)return 8;memcpy(words,data.data(),16);
  if(words[0]!=64||words[1]!=(scalar?0:128)||words[2]!=(scalar?0:192)||words[3]!=0xdeadbeef)return 9;
  data=controller->GetBufferData(table,0,24);uint64_t packet[3]={};if(data.size()!=24)return 10;memcpy(packet,data.data(),24);
  if(packet[0]||!packet[1]||packet[1]==strtoull(argv[2],nullptr,10)||packet[2]!=0x4141414141414141ULL)return 11;
  printf("GPU frame family PASS cycle=%u pixels=%llu result=%u/%u/%u NativeID=%llu\n",cycle,(unsigned long long)imageDesc.width*imageDesc.height*(array?imageDesc.arraysize:imageDesc.depth),words[0],words[1],words[2],(unsigned long long)packet[1]);
  controller->SetFrameEvent(0,true);data=controller->GetBufferData(table,0,24);memcpy(packet,data.data(),24);
  if(packet[0]||packet[1]||packet[2])return 12;
 }
 controller->Shutdown();RENDERDOC_ShutdownReplay();puts("PASS frame family full pixels, PickPixel, typed GPU ID and zero-at-EID0");return 0;
 }
}
