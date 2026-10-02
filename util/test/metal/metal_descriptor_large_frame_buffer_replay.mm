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
int main(int argc,char **argv) {
 if(argc!=3)return 2;
 @autoreleasepool {
  id<MTLDevice> native=MTLCreateSystemDefaultDevice();NSMutableArray<id<MTLBuffer>> *padding=[NSMutableArray new];
  for(unsigned i=0;i<32;i++){id<MTLBuffer> b=[native newBufferWithLength:131072 options:MTLResourceStorageModeShared];if(!b)return 3;[padding addObject:b];}
  GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);if(!result.OK())return 4;
  IReplayController *controller=nullptr;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK()||!controller){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 5;}
  ResourceId table,output,source;uint64_t length=0;bool privateSource=false;
  bool completeCopy=false;
  for(const auto &b:controller->GetBuffers()) {
   if(b.length==16)output=b.resourceId;
   if(b.length==24){const auto data=controller->GetBufferData(b.resourceId,0,24);uint64_t p[3]={};if(data.size()==24){memcpy(p,data.data(),24);if(p[2]==0x4141414141414141ULL)table=b.resourceId;}}
  }
  for(const auto *c:controller->GetStructuredFile().chunks)if(c->name=="MTLBuffer::DescriptorSlotBinding"&&c->FindChild("buffer")->AsResourceId()==table)source=c->FindChild("resource")->AsResourceId();
  for(const auto &b:controller->GetBuffers())if(b.resourceId==source)length=b.length;
  for(const auto *c:controller->GetStructuredFile().chunks)if(c->name=="MTLHeap::newBuffer(offset)"&&c->FindChild("Buffer")->AsResourceId()==source)privateSource=(c->FindChild("options")->AsUInt64()&0xf0)==32;
  for(const auto *c:controller->GetStructuredFile().chunks)if(c->name=="MTLBlitCommandEncoder::copyFromBuffer" && c->FindChild("size") && c->FindChild("size")->AsUInt64()>65536)completeCopy=true;
  uint32_t dispatch=0,last=0;Actions(controller->GetRootActions(),dispatch,last);
  if(!dispatch||table==ResourceId()||output==ResourceId()||source==ResourceId()||(length!=131072&&length!=786432&&length!=1024*1024&&length!=4*1024*1024))return 6;
  auto checkSource=[&](bool overwritten) {
   const auto data=controller->GetBufferData(source,length-12,12);uint32_t values[3]={};
   if(data.size()!=12)return false;memcpy(values,data.data(),12);
   if(values[0]!=0xbad||values[1]!=(overwritten?90:41)||values[2]!=(overwritten?120:80))return false;
   if(!privateSource||completeCopy){const auto all=controller->GetBufferData(source,0,0);if(all.size()!=length)return false;for(size_t i=0;i<all.size()-12;i++)if(all[i]!=0xee)return false;}
   return true;
  };
  for(unsigned cycle=0;cycle<4;cycle++) {
   controller->SetFrameEvent(last,true);if(!checkSource(true))return 7;
   controller->SetFrameEvent(dispatch,true);if(!checkSource(false))return 8;
   auto data=controller->GetBufferData(output,0,16);uint32_t words[4]={};if(data.size()!=16)return 9;memcpy(words,data.data(),16);
   if(words[0]!=54||words[1]!=80||words[2]!=0x12345678||words[3]!=0xdeadbeef)return 10;
   data=controller->GetBufferData(table,0,24);uint64_t packet[3]={};if(data.size()!=24)return 11;memcpy(packet,data.data(),24);
   if(!packet[0]||packet[0]==strtoull(argv[2],nullptr,10)||packet[1]||packet[2]!=0x4141414141414141ULL)return 12;
   printf("GPU large frame buffer PASS cycle=%u length=%llu private=%d GPU54/80 NativeVA=%llu\n",cycle,(unsigned long long)length,privateSource,(unsigned long long)packet[0]);
   controller->SetFrameEvent(0,true);data=controller->GetBufferData(table,0,24);if(data.size()!=24)return 13;memcpy(packet,data.data(),24);if(packet[0]||packet[1]||packet[2])return 14;
  }
  controller->Shutdown();RENDERDOC_ShutdownReplay();puts("PASS large frame sourced buffer: tail data, Shared complete bytes, partial/full replay, NativeVA and EID0 reset");
 }
 return 0;
}
