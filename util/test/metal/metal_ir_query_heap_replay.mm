// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <functional>
#include "renderdoc/api/replay/renderdoc_replay.h"

REPLAY_PROGRAM_MARKER()

int main(int argc, char **argv)
{
  if(argc!=2 && argc!=3) return 1;
  const uint32_t hit=argc==3?strtoul(argv[2],NULL,10):1000;
  @autoreleasepool
  {
    __attribute__((objc_precise_lifetime)) id<MTLDevice> native=MTLCreateSystemDefaultDevice();
    __attribute__((objc_precise_lifetime)) NSMutableArray *padding=[NSMutableArray new];
    __attribute__((objc_precise_lifetime)) id<MTLBuffer> addressPadding=[native newBufferWithLength:1024*1024 options:MTLResourceStorageModeShared];
    if(!addressPadding) return 2;
    for(unsigned i=0;i<16;i++)
    {
      auto as=[native newAccelerationStructureWithSize:4096];
      if(!as) return 2;
      [padding addObject:as];
    }
    GlobalEnvironment env; env.enumerateGPUs=false;
    rdcarray<rdcstr> args; args.push_back(argv[0]);
    RENDERDOC_InitialiseReplay(env,args);
    auto file=RENDERDOC_OpenCaptureFile();
    auto result=file->OpenFile(argv[1],"rdc",{});
    if(!result.OK()) { file->Shutdown(); return 3; }
    const auto &input=file->GetStructuredData();
    ResourceId pipeline,heap,output,header,contribution,structure;
    uint64_t slotOffset=0,headerOffset=0,contributionOffset=0,capturedHeader[8]={},capturedDescriptor=0;
    bytebuf initialHeader,initialContribution;
    auto blob=[&](const SDObject *value,bytebuf &bytes) {
      if(!value || value->data.basic.u>=input.buffers.size() || !input.buffers[value->data.basic.u]) return false;
      auto raw=input.buffers[value->data.basic.u]; bytes.assign(raw->data(),raw->size()); return true;
    };
    for(const auto *c:input.chunks)
      if(c->name=="MTLComputePipelineState::DeclareRayQueryHeapDispatch")
      {
        if(pipeline!=ResourceId()) return 5;
        pipeline=c->FindChild("pipeline")->AsResourceId(); heap=c->FindChild("heap")->AsResourceId();
        output=c->FindChild("output")->AsResourceId(); slotOffset=c->FindChild("slotOffset")->data.basic.u;
      }
    for(const auto *c:input.chunks)
      if(c->name=="MTLBuffer::DescriptorSlotEvent" && c->FindChild("buffer")->AsResourceId()==heap &&
         c->FindChild("offset")->data.basic.u==slotOffset && c->FindChild("event")->data.basic.u==2)
      {
        bytebuf raw; if(!blob(c->FindChild("data"),raw) || raw.size()!=24) return 5;
        memcpy(&capturedDescriptor,raw.data(),8);
      }
    for(const auto *c:input.chunks)
      if(c->name=="MTLBuffer::DescriptorSlotBinding" && c->FindChild("buffer")->AsResourceId()==heap &&
         c->FindChild("offset")->data.basic.u==slotOffset && c->FindChild("kind")->data.basic.u==3)
      {
        header=c->FindChild("resource")->AsResourceId(); headerOffset=c->FindChild("memberOffset")->data.basic.u;
      }
    for(const auto *c:input.chunks)
      if(c->name=="MTLBuffer::DeclareRayASHeader" && c->FindChild("buffer")->AsResourceId()==header &&
         c->FindChild("offset")->data.basic.u==headerOffset)
      {
        bytebuf raw; if(!blob(c->FindChild("bytes"),raw) || raw.size()!=64) return 5;
        memcpy(capturedHeader,raw.data(),64); structure=c->FindChild("structure")->AsResourceId();
        contribution=c->FindChild("contributions")->AsResourceId(); contributionOffset=c->FindChild("contributionOffset")->data.basic.u;
      }
    for(const auto *c:input.chunks)
      if(c->name=="Internal::Initial Contents")
      {
        const auto id=c->FindChild("id")->AsResourceId();
        if(id==header && !blob(c->FindChild("Contents"),initialHeader)) return 5;
        if(id==contribution && !blob(c->FindChild("Contents"),initialContribution)) return 5;
      }
    if(pipeline==ResourceId() || header==ResourceId() || structure==ResourceId() || contribution==ResourceId() ||
       initialHeader.size()<headerOffset+64 || initialContribution.size()<contributionOffset+4) return 5;
    IReplayController *replay=NULL; rdctie(result,replay)=file->OpenCapture(ReplayOptions(),{});
    file->Shutdown();
    if(!result.OK() || !replay) return 4;
    bool good=true; uint32_t dispatch=0,last=0;
    std::function<void(const rdcarray<ActionDescription>&)> visit=[&](const auto &actions) {
      for(const auto &a:actions)
      {
        if(a.eventId>last) last=a.eventId;
        if(a.flags&ActionFlags::Dispatch) { if(dispatch) good=false; dispatch=a.eventId; }
        visit(a.children);
      }
    };
    visit(replay->GetRootActions()); good &= dispatch>0;
    auto usage=[&](ResourceId id,ResourceUsage expected) {
      for(const auto &u:replay->GetUsage(id)) if(u.eventId==dispatch && u.usage==expected) return true;
      return false;
    };
    for(ResourceId id:{heap,header,contribution,structure}) good &= usage(id,ResourceUsage::CS_Resource);
    good &= usage(output,ResourceUsage::CS_RWResource);
    uint64_t replayHeader[8]={};
    for(unsigned cycle=0;cycle<4 && good;cycle++)
      for(uint32_t event:{last,0U,dispatch-1,dispatch})
      {
        replay->SetFrameEvent(event,true);
        auto bytes=replay->GetBufferData(output,0,16);
        uint32_t values[4]={}; if(bytes.size()!=16) { good=false; break; }
        memcpy(values,bytes.data(),16);
        const uint32_t expected[4]={event>=dispatch?hit:7U,event>=dispatch?hit:7U,event>=dispatch?0U:7U,event>=dispatch?0U:7U};
        good &= memcmp(values,expected,16)==0;
        auto h=replay->GetBufferData(header,0,initialHeader.size());
        auto c=replay->GetBufferData(contribution,0,initialContribution.size());
        auto slot=replay->GetBufferData(heap,slotOffset,24);
        if(h.size()!=initialHeader.size() || c!=initialContribution || slot.size()!=24) { good=false; break; }
        uint64_t words[8],descriptor[3]; memcpy(words,h.data()+headerOffset,64); memcpy(descriptor,slot.data(),24);
        good &= words[0]!=0 && words[0]!=capturedHeader[0] && words[1]!=0 && words[1]!=capturedHeader[1];
        good &= capturedDescriptor!=0 && descriptor[0]!=0 && descriptor[0]!=capturedDescriptor && descriptor[1]==0 && descriptor[2]==0;
        if(cycle==0 && event==last) memcpy(replayHeader,words,64);
        good &= memcmp(words,replayHeader,64)==0;
        for(unsigned i=2;i<8;i++) good &= words[i]==0;
        for(size_t i=0;i<h.size();i++)
          if(i<headerOffset || i>=headerOffset+64) good &= h[i]==initialHeader[i];
      }
    replay->Shutdown(); RENDERDOC_ShutdownReplay();
    if(!good) { fprintf(stderr,"FAIL heap-query output/header/descriptor/usage/reset\n"); return 6; }
    printf("PASS converted heap RayQuery %u/%u/0/0; typed kind3, changed AS ID/VA, immutable contribution, usage, 16 event choices/EID0\n",hit,hit);
    return 0;
  }
}
