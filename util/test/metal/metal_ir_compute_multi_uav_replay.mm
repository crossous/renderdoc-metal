// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <vector>
#include "renderdoc/api/replay/renderdoc_replay.h"

REPLAY_PROGRAM_MARKER()

int main(int argc, char **argv)
{
  if(argc!=4)return 1;
  const uint32_t hit=strtoul(argv[2],NULL,10),groups=strtoul(argv[3],NULL,10);
  if(!hit || groups!=132)return 1;
  @autoreleasepool
  {
    __attribute__((objc_precise_lifetime)) id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    __attribute__((objc_precise_lifetime)) NSMutableArray *padding=[NSMutableArray new];
    __attribute__((objc_precise_lifetime)) id<MTLBuffer> addressPadding=[device newBufferWithLength:1024*1024 options:MTLResourceStorageModeShared];
    if(!addressPadding)return 2;
    for(unsigned i=0;i<16;i++)
    {auto as=[device newAccelerationStructureWithSize:4096];if(!as)return 2;[padding addObject:as];}
    for(unsigned i=0;i<16;i++)
    {auto d=[MTLSamplerDescriptor new];d.supportArgumentBuffers=YES;d.lodMinClamp=i+1;d.lodMaxClamp=1000;
     auto sampler=[device newSamplerStateWithDescriptor:d];if(!sampler)return 2;[padding addObject:sampler];}
    for(unsigned i=0;i<32;i++)
    {
      auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:i%2?MTLPixelFormatRGBA8Unorm:MTLPixelFormatR32Uint width:4 height:132 mipmapped:NO];
      d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
      auto t=[device newTextureWithDescriptor:d];if(!t)return 2;[padding addObject:t];
    }
    GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args={argv[0]};
    RENDERDOC_InitialiseReplay(env,args);
    auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",{});
    if(!result.OK()){file->Shutdown();return 3;}
    const auto &input=file->GetStructuredData();
    ResourceId pipeline,heap,header,structure,contribution,arguments;
    uint64_t argumentOffset=0,capturedHeader[8]={};
    std::map<uint64_t,std::vector<ResourceId>> bindings;
    std::map<uint64_t,ResourceId> samplerBindings;
    std::map<ResourceId,uint64_t> capturedIDs;
    for(const auto *c:input.chunks)
    {
      if(c->name=="MTLComputePipelineState::DeclareRayQueryHeapDispatch")
      {pipeline=c->FindChild("pipeline")->AsResourceId();heap=c->FindChild("heap")->AsResourceId();}
      if(c->name=="MTLResource::CaptureGPUIdentity" && c->FindChild("kind")->data.basic.u==1)
        capturedIDs[c->FindChild("resource")->AsResourceId()]=c->FindChild("value")->data.basic.u;
      if(c->name=="MTLBuffer::DeclareRayASHeader")
      {
        header=c->FindChild("buffer")->AsResourceId();structure=c->FindChild("structure")->AsResourceId();
        contribution=c->FindChild("contributions")->AsResourceId();
        const auto *value=c->FindChild("bytes");
        if(value->data.basic.u>=input.buffers.size() || !input.buffers[value->data.basic.u] || input.buffers[value->data.basic.u]->size()!=64)return 5;
        memcpy(capturedHeader,input.buffers[value->data.basic.u]->data(),64);
      }
      if(c->name=="MTLComputeCommandEncoder::dispatchThreadgroups(indirect)")
      {arguments=c->FindChild("indirectBuffer")->AsResourceId();argumentOffset=c->FindChild("indirectBufferOffset")->data.basic.u;}
    }
    for(const auto *c:input.chunks)
      if(c->name=="MTLBuffer::DescriptorSlotBinding" && c->FindChild("buffer")->AsResourceId()==heap &&
         c->FindChild("kind")->data.basic.u==1)
        bindings[c->FindChild("offset")->data.basic.u].push_back(c->FindChild("resource")->AsResourceId());
    if(pipeline==ResourceId() || header==ResourceId() || structure==ResourceId() || arguments==ResourceId() ||
       bindings[48].size()!=3 || bindings[72].size()!=3 || bindings[96].size()!=3)return 5;
    const auto primary=bindings[48],secondary=bindings[96],reads=bindings[72];
    std::set<ResourceId> unique(primary.begin(),primary.end());unique.insert(secondary.begin(),secondary.end());
    if(unique.size()!=5 || secondary[0]!=secondary[2] || reads[1]!=secondary[0] || reads[2]!=secondary[1])return 5;
    unsigned cbvCount=0,changedSamplerMask=0;uint64_t samplerOffset=0;
    rdcarray<ResourceId> cbvs;rdcarray<bytebuf> cbvInitial;rdcarray<bool> cbvFrame;
    auto blob=[&](const SDObject *value,bytebuf &bytes) {
      if(!value || value->data.basic.u>=input.buffers.size() || !input.buffers[value->data.basic.u])return false;
      const auto *raw=input.buffers[value->data.basic.u];bytes.assign(raw->data(),raw->size());return true;
    };
    for(const auto *c:input.chunks)
      if(c->name=="MTLComputePipelineState::DeclareRayQueryHeapCBVRoot")
      {
        const unsigned count=c->FindChild("rootCount")->data.basic.u;
        if((count!=5 && count!=6) || (cbvCount && cbvCount!=count) ||
           c->FindChild("pipeline")->AsResourceId()!=pipeline ||
           c->FindChild("bytes")->data.basic.u!=(c->FindChild("offset")->data.basic.u==(count-1)*8?144:16) || c->FindChild("outputSlot")->data.basic.u!=48) return 5;
        cbvCount=count;
      }
    if(cbvCount)
    {
      cbvs.resize(cbvCount);cbvInitial.resize(cbvCount);cbvFrame.resize(cbvCount);
      for(const auto *c:input.chunks)
        if(c->name=="MTLCommandEncoder::DescriptorInlineBinding" && c->FindChild("index")->data.basic.u==2)
        {
          const uint64_t offset=c->FindChild("entry")->data.basic.u*8;
          if(offset%8 || offset/8>=cbvCount) return 5;
          const auto id=c->FindChild("resource")->AsResourceId();
          if(cbvs[offset/8]!=ResourceId() && cbvs[offset/8]!=id)return 5;
          cbvs[offset/8]=id;
        }
      for(unsigned i=0;i<cbvCount;i++)
      {
        if(cbvs[i]==ResourceId())return 5;
        for(const auto *c:input.chunks)
          if(c->name=="Internal::Initial Contents" && c->FindChild("id")->AsResourceId()==cbvs[i])
            { if(!blob(c->FindChild("Contents"),cbvInitial[i]))return 5; }
          else if(c->name=="MTLDevice::newBufferWithBytes" && c->FindChild("Buffer")->AsResourceId()==cbvs[i])
            { if(!blob(c->FindChild("initialData"),cbvInitial[i]))return 5; }
        if(cbvInitial[i].empty())
        {
          cbvFrame[i]=true;
          uint64_t capacity=0;
          for(const auto *c:input.chunks)
            if((c->name=="MTLDevice::newBufferWithLength" || c->name=="MTLHeap::newBuffer(offset)") &&
               c->FindChild("Buffer")->AsResourceId()==cbvs[i])
              capacity=c->FindChild("length")->data.basic.u;
          for(const auto *c:input.chunks)
            if(c->name=="Internal_MTLBufferModifyCPUContents" && c->FindChild("Buffer")->AsResourceId()==cbvs[i] &&
               !c->FindChild("start")->data.basic.u && c->FindChild("size")->data.basic.u==capacity)
              {if(!blob(c->FindChild("data"),cbvInitial[i]))return 5;}
          for(const auto *c:input.chunks)
            if(c->name=="MTLBlitCommandEncoder::copyFromBuffer" && c->FindChild("destinationBuffer")->AsResourceId()==cbvs[i])
            {
              const uint64_t src=c->FindChild("sourceOffset")->data.basic.u,dst=c->FindChild("destinationOffset")->data.basic.u,
                  bytes=c->FindChild("size")->data.basic.u;
              if(!capacity || dst>capacity || bytes>capacity-dst)return 5;
              const ResourceId source=c->FindChild("sourceBuffer")->AsResourceId();bytebuf uploaded;
              for(const auto *s:input.chunks)
                if(s->name=="Internal::Initial Contents" && s->FindChild("id")->AsResourceId()==source)
                  {if(!blob(s->FindChild("Contents"),uploaded))return 5;}
                else if(s->name=="MTLDevice::newBufferWithBytes" && s->FindChild("Buffer")->AsResourceId()==source)
                  {if(!blob(s->FindChild("initialData"),uploaded))return 5;}
              if(src>uploaded.size() || bytes>uploaded.size()-src)return 5;
              cbvInitial[i].resize(capacity);memcpy(cbvInitial[i].data()+dst,uploaded.data()+src,bytes);
            }
        }
        if(i==cbvCount-1 ? cbvInitial[i].size()!=224 : (cbvInitial[i].size()!=80 && cbvInitial[i].size()!=2097152))return 5;
        if(i==cbvCount-1)
          for(const auto *c:input.chunks)
            if(c->name=="MTLBuffer::DeclareDescriptorTable" && c->FindChild("buffer")->AsResourceId()==cbvs[i])
              samplerOffset=c->FindChild("offset")->data.basic.u;

      }
    }
    for(const auto *c:input.chunks)
      if(c->name=="MTLBuffer::DescriptorSlotBinding" && c->FindChild("buffer")->AsResourceId()==cbvs.back() &&
         c->FindChild("kind")->data.basic.u==2)
        samplerBindings[c->FindChild("offset")->data.basic.u]=c->FindChild("resource")->AsResourceId();
    if(samplerBindings.size()!=6)return 5;
    // Copy all metadata before closing the capture file; SD objects belong to it.
    IReplayController *replay=NULL;rdctie(result,replay)=file->OpenCapture(ReplayOptions(),{});file->Shutdown();
    if(!result.OK() || !replay)return 4;
    bool good=true;uint32_t last=0;std::vector<uint32_t> dispatches,builds;
    std::function<void(const rdcarray<ActionDescription>&)> visit=[&](const auto &actions) {
      for(const auto &a:actions)
      {
        if(a.eventId>last)last=a.eventId;
        if(a.flags&ActionFlags::Dispatch)
        {
          unsigned step=dispatches.size();
          good &= step<3 && bool(a.flags&ActionFlags::Indirect) && a.dispatchDimension[0]==(step==0?1U:step==1?2U:0U) &&
              a.dispatchDimension[1]==(step?66U:1U) && a.dispatchDimension[2]==1;
          dispatches.push_back(a.eventId);
        }
        if(a.flags&ActionFlags::BuildAccStruct)builds.push_back(a.eventId);
        visit(a.children);
      }
    };
    visit(replay->GetRootActions());
    good &= dispatches.size()==3 && builds.size()==1 && builds[0]<dispatches[0];
    auto used=[&](ResourceId id,unsigned step,ResourceUsage role) {
      for(const auto &u:replay->GetUsage(id))if(u.eventId==dispatches[step] && u.usage==role)return true;
      fprintf(stderr,"missing usage step=%u role=%u\n",step,unsigned(role));return false;
    };
    if(good)for(unsigned step=0;step<3;step++)
    {
      good &= used(primary[step],step,ResourceUsage::CS_RWResource) && used(secondary[step],step,ResourceUsage::CS_RWResource) &&
          used(reads[step],step,ResourceUsage::CS_Resource) && used(arguments,step,ResourceUsage::Indirect);
      for(ResourceId id:{heap,header,contribution,structure})good &= used(id,step,ResourceUsage::CS_Resource);
      for(ResourceId id:cbvs)good &= used(id,step,ResourceUsage::CS_Resource);
      for(ResourceId id:unique)
        for(const auto &u:replay->GetUsage(id))
          if(u.eventId==dispatches[step] && u.usage==ResourceUsage::CS_RWResource && id!=primary[step] && id!=secondary[step])good=false;
    }
    for(ResourceId id:cbvs)
      for(const auto &u:replay->GetUsage(id))if(u.usage==ResourceUsage::CS_RWResource)good=false;
    std::vector<uint32_t> choices={last,0};
    for(uint32_t b:builds){choices.push_back(b-1);choices.push_back(b);}
    for(uint32_t d:dispatches){choices.push_back(d-1);choices.push_back(d);}
    choices.push_back(0);choices.push_back(last);
    std::map<ResourceId,uint64_t> replayIDs;
    unsigned selections=0,identityChecks=0,descriptorChecks=0,samplerChecks=0;
    for(unsigned cycle=0;cycle<4 && good;cycle++)for(uint32_t event:choices)
    {
      replay->SetFrameEvent(event,true);selections++;
      for(unsigned i=0;i<cbvs.size();i++)
        {
          auto current=replay->GetBufferData(cbvs[i],0,cbvInitial[i].size());
          if(cbvFrame[i] && event==0){good &= current.empty();continue;}
          // Frame CPU bytes belong to the first using submission. Before it,
          // no snapshot promises allocation padding or unobserved CPU data.
          if(cbvFrame[i] && event<dispatches[0])continue;
          if(i==cbvCount-1)
          {
            if(current.size()!=224){good=false;continue;}
            // The immutable table has six identities, guarded by full padding.
            const uint64_t at=samplerOffset;
            if(at!=16 && at!=64){good=false;continue;}
            for(unsigned j=0;j<6;j++)
            {
              uint64_t captured=0,actual=0;
              memcpy(&captured,cbvInitial[i].data()+at+j*24,8);memcpy(&actual,current.data()+at+j*24,8);
              good &= actual!=0;if(actual!=captured){changedSamplerMask|=1U<<j;}
              memcpy(current.data()+at+j*24,&captured,8);
            }
          }
          good &= current==cbvInitial[i];
        }
      const ResourceId textures[]={primary[0],primary[1],primary[2],secondary[0],secondary[1]};
      for(unsigned t=0;t<5;t++)
      {
        auto bytes=replay->GetTextureData(textures[t],{0,0,0});
        if(bytes.size()!=groups*16){good=false;break;}
        for(unsigned i=0;i<groups*4;i++)
        {
          const bool written=t==0 || t==3?event>=dispatches[0] && i<4:(t==1 || t==4) && event>=dispatches[1];
          const uint32_t want=t<3?(written?(i%4<2?hit+(t==1?312:0):0):7):
              (written?(i%4<2?0xff241840:0xff241820):0xff24180c);
          uint32_t actual;memcpy(&actual,bytes.data()+i*4,4);
          if(actual!=want){fprintf(stderr,"multi-UAV event=%u texture=%u texel=%u actual=%u want=%u\n",event,t,i,actual,want);good=false;break;}
        }
      }
      unsigned step=0;
      if(event>=dispatches[1]-1)step=1;
      if(event>=dispatches[2]-1)step=2;
      for(unsigned slot=0;slot<3;slot++)
      {
        auto bytes=replay->GetBufferData(heap,48+slot*24,24);uint64_t words[3]={};
        if(bytes.size()!=24){good=false;break;}memcpy(words,bytes.data(),24);
        ResourceId id=slot==0?primary[step]:slot==1?reads[step]:secondary[step];
        if(words[0] || !words[1] || words[2] || words[1]==capturedIDs[id])
          fprintf(stderr,"descriptor event=%u slot=%u words=%llu/%llu/%llu captured=%llu\n",event,slot,(unsigned long long)words[0],(unsigned long long)words[1],(unsigned long long)words[2],(unsigned long long)capturedIDs[id]);
        good &= !words[0] && words[1] && !words[2] && words[1]!=capturedIDs[id];
        if(replayIDs.count(id))
        {
          if(replayIDs[id]!=words[1])fprintf(stderr,"descriptor event=%u slot=%u unstable ID expected=%llu actual=%llu\n",event,slot,(unsigned long long)replayIDs[id],(unsigned long long)words[1]);
          good &= replayIDs[id]==words[1];
        }
        else replayIDs[id]=words[1];
        identityChecks++;
      }
      bool atDispatch=false;for(uint32_t d:dispatches)atDispatch |= event==d;
      if(!event)
      {
        DescriptorRange range;range.offset=uint32_t(samplerOffset);range.count=6;
        range.descriptorSize=24;range.type=DescriptorType::Sampler;
        for(const auto &sampler:replay->GetSamplerDescriptors(cbvs.back(),{range}))
          if(sampler.object!=ResourceId()) {fprintf(stderr,"stale sampler after EID0 reset\n");good=false;}
      }
      if(atDispatch)
      {
        const auto &accesses=replay->GetDescriptorAccess();
        for(unsigned slot=0;slot<3;slot++)
        {
          const ResourceId wanted=slot==0?primary[step]:slot==1?reads[step]:secondary[step];
          const auto type=slot==1?DescriptorType::Image:DescriptorType::ReadWriteImage;
          unsigned found=0;
          for(const auto &access:accesses)
            if(access.descriptorStore==heap && access.byteOffset==48+slot*24 &&
               access.stage==ShaderStage::Compute && access.type==type && access.byteSize==24)
            {
              DescriptorRange range;range.offset=access.byteOffset;range.count=1;range.descriptorSize=24;range.type=type;
              auto descriptors=replay->GetDescriptors(heap,{range});
              if(descriptors.size()!=1 || descriptors[0].resource!=wanted || descriptors[0].type!=type)
              {fprintf(stderr,"descriptor API identity mismatch event=%u slot=%u\n",event,slot);good=false;}
              found++;descriptorChecks++;
            }
          if(found!=1){fprintf(stderr,"descriptor API access count event=%u slot=%u count=%u\n",event,slot,found);good=false;}
        }
        unsigned structures=0;
        for(const auto &access:accesses)
          if(access.descriptorStore==heap && access.byteOffset==24 &&
             access.stage==ShaderStage::Compute && access.type==DescriptorType::AccelerationStructure &&
             access.byteSize==24)
          {
            DescriptorRange range;range.offset=24;range.count=1;range.descriptorSize=24;
            range.type=DescriptorType::AccelerationStructure;
            auto descriptors=replay->GetDescriptors(heap,{range});
            if(descriptors.size()!=1 || descriptors[0].resource!=structure ||
               descriptors[0].type!=DescriptorType::AccelerationStructure)
            {fprintf(stderr,"AS descriptor API identity mismatch event=%u\n",event);good=false;}
            structures++;descriptorChecks++;
          }
        if(structures!=1)
        {fprintf(stderr,"AS descriptor API access count event=%u count=%u\n",event,structures);good=false;}
        for(unsigned i=0;i<6;i++)
        {
          const uint64_t offset=samplerOffset+i*24;unsigned found=0;
          for(const auto &access:accesses)
            if(access.descriptorStore==cbvs.back() && access.byteOffset==offset &&
               access.stage==ShaderStage::Compute && access.type==DescriptorType::Sampler && access.byteSize==24)
            {
              DescriptorRange range;range.offset=uint32_t(offset);range.count=1;range.descriptorSize=24;range.type=DescriptorType::Sampler;
              const auto samplers=replay->GetSamplerDescriptors(cbvs.back(),{range});
              const auto images=replay->GetDescriptors(cbvs.back(),{range});
              if(samplers.size()!=1 || images.size()!=1 || samplers[0].object!=samplerBindings[offset] ||
                 samplers[0].type!=DescriptorType::Sampler || images[0].resource!=samplers[0].object ||
                 images[0].type!=DescriptorType::Sampler || samplers[0].maxLOD!=1000 || samplers[0].minLOD!=0 ||
                 samplers[0].filter.minify!=(i<2?FilterMode::Point:FilterMode::Linear) ||
                 samplers[0].filter.magnify!=(i<2?FilterMode::Point:FilterMode::Linear) ||
                 samplers[0].filter.mip!=(i<4?FilterMode::Point:FilterMode::Linear) ||
                 samplers[0].addressU!=(i%2?AddressMode::ClampEdge:AddressMode::Wrap) ||
                 samplers[0].addressV!=samplers[0].addressU || samplers[0].addressW!=samplers[0].addressU ||
                 samplers[0].unnormalized)
              {fprintf(stderr,"sampler descriptor API mismatch event=%u slot=%u\n",event,i);good=false;}
              found++;samplerChecks++;
            }
          if(found!=1){fprintf(stderr,"sampler descriptor API access count event=%u slot=%u count=%u\n",event,i,found);good=false;}
        }
      }
      auto raw=replay->GetBufferData(arguments,0,64);
      if(raw.size()!=64){good=false;break;}
      uint32_t dims[3]={};memcpy(dims,raw.data()+argumentOffset,12);
      if(!event)good &= !dims[0] && !dims[1] && !dims[2];
      for(unsigned s=0;s<3;s++)if(event==dispatches[s] || event==dispatches[s]-1)
        good &= dims[0]==(s==0?1U:s==1?2U:0U) && dims[1]==(s?66U:1U) && dims[2]==1;
      for(unsigned i=0;i<64;i++)if(i<argumentOffset || i>=argumentOffset+12)good &= raw[i]==0xcd;
      auto h=replay->GetBufferData(header,0,64);
      if(!event){if(!h.empty())fprintf(stderr,"Header still present at EID0\n");good &= h.empty();}
      else
      {
        if(h.size()!=64){good=false;break;}uint64_t words[8];memcpy(words,h.data(),64);
        if(!words[0] || words[0]==capturedHeader[0] || !words[1] || words[1]==capturedHeader[1])fprintf(stderr,"Header not relocated event=%u AS=%llu/%llu VA=%llu/%llu\n",event,(unsigned long long)words[0],(unsigned long long)capturedHeader[0],(unsigned long long)words[1],(unsigned long long)capturedHeader[1]);
        good &= words[0] && words[0]!=capturedHeader[0] && words[1] && words[1]!=capturedHeader[1];
        for(unsigned i=2;i<8;i++)good &= words[i]==capturedHeader[i];
      }
      if(!good)fprintf(stderr,"multi-UAV oracle failed at event=%u cycle=%u\n",event,cycle);
    }
    good &= changedSamplerMask==63;
    replay->Shutdown();RENDERDOC_ShutdownReplay();
    if(!good)return 6;
    printf("PASS multi-UAV five textures/%u texels each, hit=%u/%u, current-slot roles and GPU-produced SRV; %u event choices/EID0, %u descriptor identity checks, %u common API descriptor checks, %u sampler API checks, indirect/AS Header relocation/usage, all CBV bytes/padding and six sampler identities\n",groups*4,hit,hit+312,selections,identityChecks,descriptorChecks,samplerChecks);
    return 0;
  }
}
