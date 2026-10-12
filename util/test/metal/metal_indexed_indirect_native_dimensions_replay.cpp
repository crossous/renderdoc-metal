// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>
#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#endif
REPLAY_PROGRAM_MARKER()
int main(int argc,char **argv)
{
  if(argc!=3 && argc!=4)return 2;
#if defined(__OBJC__)
  @autoreleasepool {
  // An unrelated live Native allocation is a legal replay process condition,
  // not an input replacement or a captured address substituted into the shader.
  id<MTLBuffer> nativePrefix=nil;
  if(argc==4){
    unsigned bytes=unsigned(atoi(argv[3]));if(!bytes || bytes>1024*1024)return 2;
    auto device=MTLCreateSystemDefaultDevice();nativePrefix=[device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    if(!nativePrefix || !nativePrefix.gpuAddress)return 2;
    printf("Unrelated Native allocation bytes=%u address=%llu\n",bytes,(unsigned long long)nativePrefix.gpuAddress);
  }
#else
  if(argc==4)return 2;
#endif
  FILE *f=fopen(argv[2],"rb");if(!f)return 2;
  std::vector<unsigned char> expected(256*17);
  bool valid=fread(expected.data(),1,expected.size(),f)==expected.size() && fgetc(f)==EOF;fclose(f);
  if(!valid)return 2;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto status=file->OpenFile(argv[1],"rdc",nullptr);
  IReplayController *controller=nullptr;if(status.OK())rdctie(status,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!status.OK() || !controller){fprintf(stderr,"OpenCapture failed\n");RENDERDOC_ShutdownReplay();return 4;}
  ResourceId image,args,padded,indices;
  for(const auto &r:controller->GetResources()){
    if(r.name=="Native indirect invariant image")image=r.resourceId;
    if(r.name=="Native indirect current arguments")args=r.resourceId;
    if(r.name=="Native indirect padded copy")padded=r.resourceId;
    if(r.name=="Native indirect index input")indices=r.resourceId;
  }
  uint64_t indexBytes=0;for(const auto &b:controller->GetBuffers())if(b.resourceId==indices)indexBytes=b.length;
  uint32_t last=0,drawEvent=0;ActionDescription draw;
  std::function<void(const rdcarray<ActionDescription>&)> visit=[&](const auto &list){for(const auto &a:list){
    last=std::max(last,a.eventId);
    if((a.flags & ActionFlags::Drawcall) && (a.flags & ActionFlags::Indexed) && (a.flags & ActionFlags::Indirect))
    {drawEvent=a.eventId;draw=a;}
    visit(a.children);
  }};visit(controller->GetRootActions());
  unsigned failures=0,checks=0;
  for(unsigned cycle=0;cycle<4;cycle++){
    if(cycle){
      controller->SetFrameEvent(0,true);auto initial=controller->GetBufferData(args,0,20);
      unsigned initialErrors=initial.size()!=20;
      for(unsigned char value:initial)initialErrors+=value!=0;
      printf("EID0 initial bytes=%zu errors=%u\n",initial.size(),initialErrors);
      failures+=initialErrors!=0;controller->SetFrameEvent(last,true);
    }
    auto pixels=controller->GetTextureData(image,{0,0,0});auto words=controller->GetBufferData(args,0,20);
    auto copy=controller->GetBufferData(padded,0,expected.size());
    unsigned errors=0;if(pixels.size()!=31*17*4 || words.size()!=20 || copy.size()!=expected.size()){failures++;continue;}
    for(size_t i=0;i<copy.size();i++)errors+=copy[i]!=expected[i];
    for(unsigned y=0;y<17;y++)for(unsigned x=0;x<124;x++)errors+=pixels[y*124+x]!=expected[y*256+x];
    uint32_t actual[5]={};memcpy(actual,words.data(),20);
    const bool bounded=((actual[0]==3 && actual[1]==1 && actual[2]==3 && int32_t(actual[3])==-3) || (actual[0]==6 && actual[1]==2 && actual[2]==0 && actual[3]==0)) && !actual[4];
    const bool actionMatch=drawEvent && draw.numIndices==actual[0] && draw.numInstances==actual[1] &&
        draw.indexOffset==actual[2] && draw.baseVertex==int32_t(actual[3]) && draw.instanceOffset==actual[4];
    controller->SetFrameEvent(drawEvent,true);
    const auto *state=controller->GetPipelineState().GetMetalPipelineState();
    const bool bindingMatch=state && state->indexBuffer.resourceId==indices &&
        state->indexBuffer.byteOffset==0 && state->indexBuffer.byteSize==indexBytes &&
        (state->indexBuffer.byteStride==2 || state->indexBuffer.byteStride==4) &&
        indexBytes==6*state->indexBuffer.byteStride;
    controller->SetFrameEvent(last,true);
    failures+=errors!=0 || !bounded || !actionMatch || !bindingMatch || !controller->GetFatalErrorStatus().OK();checks++;
    printf("Action and current index binding cycle=%u action=%d binding=%d bytes=%llu\n",cycle,actionMatch,bindingMatch,(unsigned long long)indexBytes);
    printf("Complete image cycle=%u bytes=%zu errors=%u current-indices=%u instances=%u firstIndex=%u baseVertex=%d bounded=%d\n",cycle,pixels.size(),errors,actual[0],actual[1],actual[2],int32_t(actual[3]),bounded);
  }
  controller->Shutdown();RENDERDOC_ShutdownReplay();printf("GPU %s complete-output checks=%u\n",failures?"FAIL":"COMPLETED",checks);
#if defined(__OBJC__)
  if(nativePrefix && !nativePrefix.length)return 6;
#endif
  return failures?6:0;
#if defined(__OBJC__)
  }
#endif
}
