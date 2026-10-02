// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#include <cstdio>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Find(const rdcarray<ActionDescription> &actions,uint32_t &dispatch)
{for(const auto &a:actions){if(a.flags&ActionFlags::Dispatch)dispatch=a.eventId;Find(a.children,dispatch);}}
int main(int argc,char **argv)
{
  if(argc!=3)return 2;
  @autoreleasepool
  {
    NSString *folder=[NSString stringWithUTF8String:argv[2]];
    auto data=[&](NSString *name){return [NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:name]];};
    NSDictionary *manifest=[NSJSONSerialization JSONObjectWithData:data(@"manifest.json") options:0 error:nil];
    NSData *payload=data(@"payload.bin"),*indices=data(@"indices.bin");
    NSUInteger count=[manifest[@"count"] unsignedIntegerValue],size=[manifest[@"destination_bytes"] unsignedIntegerValue];
    if(!count||count>8||size>65536||size<24||payload.length!=count*24||indices.length!=count*4)return 3;
    NSMutableData *expected=[NSMutableData dataWithLength:size];memset(expected.mutableBytes,0xcd,size);
    const uint32_t *offsets=(const uint32_t *)indices.bytes;
    for(NSUInteger i=0;i<count;i++){if(offsets[i]>=size/24)return 4;memcpy((uint8_t *)expected.mutableBytes+offsets[i]*24,(const uint8_t *)payload.bytes+i*24,24);}
    GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);RENDERDOC_InitialiseReplay(env,args);
    ICaptureFile *file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);IReplayController *controller=nullptr;
    if(!result.OK())return 5;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
    if(!result.OK()||!controller){fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 6;}
    ResourceId output;for(const auto &b:controller->GetBuffers())if(b.length==size)output=b.resourceId;
    uint32_t dispatch=0;Find(controller->GetRootActions(),dispatch);if(!dispatch||output==ResourceId())return 7;
    for(int cycle=0;cycle<4;cycle++)
    {
      controller->SetFrameEvent(dispatch,true);const auto bytes=controller->GetBufferData(output,0,size);
      if(bytes.size()!=size||memcmp(bytes.data(),expected.bytes,size))return 8;
      controller->SetFrameEvent(0,true);const auto reset=controller->GetBufferData(output,0,size);
      if(reset.size()!=size)return 9;
      for(uint8_t byte:reset)if(byte!=0xcd)return 10;
      printf("PASS actual UE opaque scatter cycle=%d count=%lu targetBytes=%lu\n",cycle,(unsigned long)count,(unsigned long)size);
    }
    controller->Shutdown();RENDERDOC_ShutdownReplay();puts("PASS actual captured UE shader replay; copied payload exact, sentinel/reset preserved, four seeks; no descriptor consumer");
  }
  return 0;
}
