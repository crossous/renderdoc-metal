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
    if(argc<2 || argc>5) return 1;
    const uint32_t hit=argc==3?strtoul(argv[2],NULL,10):1000;
    const uint32_t queryGroups=argc>=4?strtoul(argv[3],NULL,10):0;
    const uint32_t expectedHit=argc>=4?strtoul(argv[2],NULL,10):hit;
    const bool sameEncoder=argc==5 && strtoul(argv[4],NULL,10)==1;
    ResourceId queryEncoder;
    bool textureOutput=false,floatTextureOutput=false,texelOutput=false;ResourceId texelParent,texelRead,texelReadParent;bytebuf texelReadInitial;uint64_t texelOffset=0;bytebuf texelInitial;uint64_t capturedOutputID=0;
    bool mixedRoots=false;uint64_t samplerOffset=0;ResourceId sampledTexture;unsigned changedSamplers=0,changedSamplerMask=0;
    unsigned cbvCount=0,bufferAPIChecks=0;rdcarray<ResourceId> cbvs;rdcarray<bytebuf> cbvInitial;rdcarray<bool> cbvFrame;
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
    for(unsigned i=0;i<16;i++)
    {
      auto sd=[MTLSamplerDescriptor new];sd.supportArgumentBuffers=YES;
      sd.lodMinClamp=i+1;sd.lodMaxClamp=1000;
      auto sampler=[native newSamplerStateWithDescriptor:sd];if(!sampler)return 2;[padding addObject:sampler];
    }
    for(unsigned i=0;i<16;i++)
    {
      auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Uint width:4 height:4 mipmapped:NO];
      td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
      auto texture=[native newTextureWithDescriptor:td];if(!texture)return 2;[padding addObject:texture];
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
    ResourceId frameSource,arguments;bytebuf frozenInstances;uint64_t frameCount=~0ULL;unsigned expectedBuilds=1;
    uint64_t argumentOffset=0;unsigned indirectRecords=0;uint32_t gridHeight=1;
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
    {
      if(c->FindChild("descriptor") && c->FindChild("Texture") && c->FindChild("Texture")->AsResourceId()==output)
      {
        textureOutput=true;
        texelOutput=c->FindChild("descriptor")->FindChild("textureType")->data.basic.u==MTLTextureTypeTextureBuffer;
        if(texelOutput){texelParent=c->FindChild("Buffer")->AsResourceId();texelOffset=c->FindChild("offset")->data.basic.u;}
        floatTextureOutput=c->FindChild("descriptor")->FindChild("pixelFormat")->data.basic.u==MTLPixelFormatRGBA32Float;
      }
      if(c->name=="MTLResource::CaptureGPUIdentity" && c->FindChild("resource")->AsResourceId()==output)
        capturedOutputID=c->FindChild("value")->data.basic.u;
    }
    if(texelOutput)
    {
      for(const auto *c:input.chunks)
        if(c->name=="Internal::Initial Contents" && c->FindChild("id")->AsResourceId()==texelParent)
          if(!blob(c->FindChild("Contents"),texelInitial))return 5;
      if(texelParent==ResourceId() || texelOffset!=256 || texelInitial.empty())return 5;
      for(const auto *c:input.chunks)
        if(c->name=="MTLBuffer::DescriptorSlotBinding" && c->FindChild("buffer")->AsResourceId()==heap &&
           c->FindChild("offset")->data.basic.u==96 && c->FindChild("kind")->data.basic.u==1)
          texelRead=c->FindChild("resource")->AsResourceId();
      for(const auto *c:input.chunks)
        if(c->name=="MTLBuffer::newTextureWithDescriptor" && c->FindChild("Texture")->AsResourceId()==texelRead)
        {texelReadParent=c->FindChild("Buffer")->AsResourceId();if(c->FindChild("offset")->data.basic.u!=texelOffset)return 5;}
      for(const auto *c:input.chunks)
        if(c->name=="Internal::Initial Contents" && c->FindChild("id")->AsResourceId()==texelReadParent)
          if(!blob(c->FindChild("Contents"),texelReadInitial))return 5;
      if(texelRead==ResourceId() || texelReadParent==ResourceId() || texelReadInitial.empty())return 5;

    }
    for(const auto *c:input.chunks)
      if(c->name=="MTLComputePipelineState::DeclareIRComputeRoot" && c->FindChild("kind")->data.basic.u==3)
        mixedRoots=true;
      else if(c->name=="MTLBuffer::DescriptorSlotBinding" && c->FindChild("buffer")->AsResourceId()==heap &&
              c->FindChild("offset")->data.basic.u==72 && c->FindChild("kind")->data.basic.u==1)
        sampledTexture=c->FindChild("resource")->AsResourceId();
    for(const auto *c:input.chunks)
      if(c->name=="MTLComputePipelineState::DeclareRayQueryHeapCBVRoot")
      {
        const unsigned count=c->FindChild("rootCount")->data.basic.u;
        if((count!=5 && count!=6) || (cbvCount && cbvCount!=count) ||
           c->FindChild("pipeline")->AsResourceId()!=pipeline ||
           c->FindChild("bytes")->data.basic.u!=(mixedRoots && c->FindChild("offset")->data.basic.u==(count-1)*8?144:16) || c->FindChild("outputSlot")->data.basic.u!=48) return 5;
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
            if(c->name=="MTLDevice::newBufferWithLength" && c->FindChild("Buffer")->AsResourceId()==cbvs[i])
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
              if(!capacity || capacity>8*1024*1024 || dst>capacity || bytes>capacity-dst)return 5;
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
        if(mixedRoots && i==cbvCount-1 ? cbvInitial[i].size()!=224 : (cbvInitial[i].size()!=80 && cbvInitial[i].size()!=2097152))return 5;
        if(mixedRoots && i==cbvCount-1)
          for(const auto *c:input.chunks)
            if(c->name=="MTLBuffer::DeclareDescriptorTable" && c->FindChild("buffer")->AsResourceId()==cbvs[i])
              samplerOffset=c->FindChild("offset")->data.basic.u;

      }
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
      if(c->name=="MTLComputeCommandEncoder::CaptureIndirectArguments")
      {
        if(!queryGroups || indirectRecords>=3) return 5;
        const auto id=c->FindChild("buffer")->AsResourceId();
        if(sameEncoder)
        {
          auto encoder=c->FindChild("encoder")->AsResourceId();
          if(indirectRecords && encoder!=queryEncoder) return 5;
          queryEncoder=encoder;
        }
        if(c->FindChild("ordinal")->data.basic.u!=(sameEncoder?indirectRecords:0)) return 5;
        const auto offset=c->FindChild("offset")->data.basic.u;
        if(indirectRecords && (id!=arguments || offset!=argumentOffset)) return 5;
        arguments=id;argumentOffset=offset;
        const auto *groups=c->FindChild("groups");
        if(!groups || groups->NumChildren()!=3)return 5;
        const uint32_t height=groups->GetChild(1)->data.basic.u;
        if(height!=1 && (!cbvCount || height!=66))return 5;
        if(sameEncoder || indirectRecords>0)gridHeight=height;
        const uint32_t total=sameEncoder?queryGroups:indirectRecords==0?1U:indirectRecords==1?queryGroups:0U;
        if(groups->GetChild(0)->data.basic.u*height!=total || groups->GetChild(2)->data.basic.u!=1)return 5;
        indirectRecords++;
      }
    if(queryGroups && (indirectRecords!=3 || arguments==ResourceId() || argumentOffset>52)) return 5;
    for(const auto *c:input.chunks)
      if(c->name=="Internal::Initial Contents")
      {
        const auto id=c->FindChild("id")->AsResourceId();
        if(id==header && !blob(c->FindChild("Contents"),initialHeader)) return 5;
        if(id==contribution && !blob(c->FindChild("Contents"),initialContribution)) return 5;
      }
    for(const auto *c:input.chunks)
      if(c->name=="MTLAccelerationStructureCommandEncoder::buildIndirectInstances" &&
         c->FindChild("structure")->AsResourceId()==structure)
      {
        if(frameSource!=ResourceId() || !blob(c->FindChild("descriptorBytes"),frozenInstances)) return 5;
        frameSource=c->FindChild("instances")->AsResourceId();
        const auto *parameters=c->FindChild("parameters");
        if(!parameters || parameters->NumChildren()!=8) return 5;
        frameCount=parameters->GetChild(3)->data.basic.u;
        if(frozenInstances.size()!=frameCount*72 || frameCount>64) return 5;
      }
    if(pipeline==ResourceId() || header==ResourceId() || structure==ResourceId() || contribution==ResourceId() ||
       !initialHeader.empty() || headerOffset || frameSource==ResourceId() || frameCount==~0ULL ||
       initialContribution.size()<contributionOffset+4) return 5;
    for(const auto *c:input.chunks)
      if(c->name=="MTLAccelerationStructureCommandEncoder::buildFrozenTriangles" ||
         c->name=="MTLAccelerationStructureCommandEncoder::buildFrozenMultiIndexed") ++expectedBuilds;
    if(expectedBuilds>2) return 5;
    IReplayController *replay=NULL; rdctie(result,replay)=file->OpenCapture(ReplayOptions(),{});
    file->Shutdown();
    if(!result.OK() || !replay) return 4;
    bool good=true; uint32_t dispatch=0,last=0,build=0;rdcarray<uint32_t> builds,dispatches;
    std::function<void(const rdcarray<ActionDescription>&)> visit=[&](const auto &actions) {
      for(const auto &a:actions)
      {
        if(a.eventId>last) last=a.eventId;
        if(a.flags&ActionFlags::Dispatch)
        {
          if(queryGroups)
          {
            const unsigned step=unsigned(dispatches.size());
            good &= step<3 && (a.flags&ActionFlags::Indirect) && a.dispatchDimension[0]==(sameEncoder?queryGroups/gridHeight:step==0?1U:step==1?queryGroups/gridHeight:0U) &&
                a.dispatchDimension[1]==(sameEncoder || step>0?gridHeight:1) && a.dispatchDimension[2]==1;
          }
          else if(dispatch) good=false;
          dispatch=a.eventId;dispatches.push_back(a.eventId);
        }
        if(a.flags&ActionFlags::BuildAccStruct) { builds.push_back(a.eventId);build=a.eventId; }
        visit(a.children);
      }
    };
    visit(replay->GetRootActions()); good &= build>0 && dispatches.size()==(queryGroups?3U:1U) && dispatches[0]>build && builds.size()==expectedBuilds;
    rdcarray<uint32_t> choices={last,0U};
    for(uint32_t event:builds){choices.push_back(event-1);choices.push_back(event);}
    for(uint32_t event:dispatches){choices.push_back(event-1);choices.push_back(event);}
    choices.push_back(0);choices.push_back(last);
    auto usage=[&](ResourceId id,ResourceUsage expected) {
      for(uint32_t event:dispatches)
      {
        bool found=false;for(const auto &u:replay->GetUsage(id)) if(u.eventId==event && u.usage==expected) found=true;
        if(!found) return false;
      }
      return true;
    };
    for(ResourceId id:{heap,header,contribution,structure}) good &= usage(id,ResourceUsage::CS_Resource);
    for(ResourceId id:{heap,header,contribution,structure})
      for(const auto &u:replay->GetUsage(id))
        for(uint32_t event:dispatches)
          if(u.eventId==event && u.usage==ResourceUsage::CS_RWResource)good=false;
    for(ResourceId id:cbvs)
    {
      good &= usage(id,ResourceUsage::CS_Resource);
      for(const auto &u:replay->GetUsage(id)) if(u.usage==ResourceUsage::CS_RWResource)good=false;
    }
    if(mixedRoots)
    {
      if(sampledTexture==ResourceId())good=false;
      else good &= usage(sampledTexture,ResourceUsage::CS_Resource);
    }
    good &= usage(output,ResourceUsage::CS_RWResource);
    if(texelOutput)
    {
      good &= usage(texelParent,ResourceUsage::CS_RWResource) && usage(texelRead,ResourceUsage::CS_Resource) && usage(texelReadParent,ResourceUsage::CS_Resource);
      for(ResourceId resource:{texelRead,texelReadParent})for(const auto &u:replay->GetUsage(resource))
        if(u.usage==ResourceUsage::CS_RWResource)good=false;
    }
    if(queryGroups) good &= usage(arguments,ResourceUsage::Indirect);
    uint64_t replayHeader[8]={};
    for(unsigned cycle=0;cycle<4 && good;cycle++)
      for(uint32_t event:choices)
      {
        replay->SetFrameEvent(event,true);
        for(unsigned i=0;i<cbvs.size();i++)
        {
          auto current=replay->GetBufferData(cbvs[i],0,cbvInitial[i].size());
          if(cbvFrame[i] && event==0){good &= current.empty();continue;}
          // Frame CPU bytes belong to the first using submission. Before it,
          // no snapshot promises allocation padding or unobserved CPU data.
          if(cbvFrame[i] && event<dispatches[0])continue;
          if(mixedRoots && i==cbvCount-1)
          {
            if(current.size()!=224){good=false;continue;}
            // The immutable table has six identities, guarded by full padding.
            const uint64_t at=samplerOffset;
            if(at!=16 && at!=64){good=false;continue;}
            for(unsigned j=0;j<6;j++)
            {
              uint64_t captured=0,actual=0;
              memcpy(&captured,cbvInitial[i].data()+at+j*24,8);memcpy(&actual,current.data()+at+j*24,8);
              good &= actual!=0;if(actual!=captured){changedSamplers++;changedSamplerMask|=1U<<j;}
              memcpy(current.data()+at+j*24,&captured,8);
            }
          }
          good &= current==cbvInitial[i];
        }
        if(mixedRoots)
        {
          auto pixels=replay->GetTextureData(sampledTexture,{0,0,0});
          const byte expected[]={12,24,36,255};
          good &= pixels.size()==16;
          for(size_t i=0;i<pixels.size();i++)good &= pixels[i]==expected[i%4];
        }
        const uint32_t resultWords=queryGroups?queryGroups*4:4;
        auto bytes=textureOutput?replay->GetTextureData(output,{0,0,0}):replay->GetBufferData(output,0,resultWords*4);
        const uint32_t components=floatTextureOutput?4:1;
        if(bytes.size()!=resultWords*4*components) { fprintf(stderr,"output byte count event=%u actual=%zu expected=%u texture=%d\n",event,bytes.size(),resultWords*4*components,textureOutput);good=false;break; }
        for(uint32_t i=0;i<resultWords;i++)
        {
          const bool written=event>=dispatches[0] && (sameEncoder || i<4 || (queryGroups && event>=dispatches[1]));
          const uint32_t expected=written?(i%4<2?expectedHit:0U):7U;
          if(floatTextureOutput)
          {
            float rgba[4];memcpy(rgba,bytes.data()+i*16,16);
            const float wanted[]={float(expected),written?float(expected+1):7.0f,written?float(expected+2):7.0f,written?1.0f:7.0f};
            for(unsigned j=0;j<4;j++)good &= rgba[j]==wanted[j];
          }
          else {uint32_t value=0;memcpy(&value,bytes.data()+i*4,4);
            if(value!=expected)fprintf(stderr,"query pixel mismatch event=%u index=%u actual=%u expected=%u\n",event,i,value,expected);
            good &= value==expected;}
        }
        if(!textureOutput)
        {
          bool selectedDispatch=false;for(uint32_t d:dispatches)selectedDispatch |= event==d;
          if(selectedDispatch)
          {
            unsigned found=0;
            for(const auto &access:replay->GetDescriptorAccess())
              if(access.stage==ShaderStage::Compute && access.descriptorStore==heap &&
                 access.byteOffset==48 && access.byteSize==24 && access.type==DescriptorType::ReadWriteBuffer)
              {
                DescriptorRange range;range.offset=48;range.count=1;range.descriptorSize=24;range.type=access.type;
                const auto descriptors=replay->GetDescriptors(heap,{range});
                BufferDescription description;
                for(const auto &buffer:replay->GetBuffers())if(buffer.resourceId==output)description=buffer;
                if(description.resourceId!=output || descriptors.size()!=1 || descriptors[0].resource!=output || descriptors[0].type!=access.type ||
                   descriptors[0].byteOffset || descriptors[0].byteSize!=description.length)
                {fprintf(stderr,"buffer descriptor API mismatch event=%u\n",event);good=false;}
                found++;
                bufferAPIChecks++;
              }
            if(found!=1){fprintf(stderr,"buffer descriptor API access count event=%u count=%u\n",event,found);good=false;}
          }
        }
        if(texelOutput)
        {
          good &= replay->GetBufferData(texelReadParent,0,texelReadInitial.size())==texelReadInitial;
          const auto readPixels=replay->GetTextureData(texelRead,{0,0,0});good &= readPixels.size()==16;
          for(size_t i=0;i+4<=readPixels.size();i+=4){uint32_t value;memcpy(&value,readPixels.data()+i,4);good &= value==5;}
          auto parent=replay->GetBufferData(texelParent,0,texelInitial.size());
          good &= parent.size()==texelInitial.size();
          for(size_t i=0;i<parent.size();i++)if(i<texelOffset || i>=texelOffset+resultWords*4)
            good &= parent[i]==texelInitial[i];
          bool selected=false;for(uint32_t d:dispatches)selected |= event==d;
          unsigned found=0;
          for(const auto &access:replay->GetDescriptorAccess())
            if(access.stage==ShaderStage::Compute && access.descriptorStore==heap && (access.byteOffset==48 || access.byteOffset==96))
            {
              if(!event){good=false;continue;}
              if(!selected)continue;
              const bool readonly=access.byteOffset==96;
              DescriptorRange range;range.offset=access.byteOffset;range.count=1;range.descriptorSize=24;range.type=access.type;
              const auto descriptors=replay->GetDescriptors(heap,{range});
              const auto locations=replay->GetDescriptorLocations(heap,{range});
              good &= access.type==(readonly?DescriptorType::TypedBuffer:DescriptorType::ReadWriteTypedBuffer) && descriptors.size()==1 &&
                  descriptors[0].resource==(readonly?texelReadParent:texelParent) && descriptors[0].view==(readonly?texelRead:output) &&
                  descriptors[0].byteOffset==texelOffset && descriptors[0].byteSize==(readonly?16U:resultWords*4) &&
                  descriptors[0].textureType==TextureType::Buffer && descriptors[0].format.compType==CompType::UInt &&
                  locations.size()==1 && locations[0].category==(readonly?DescriptorCategory::ReadOnlyResource:DescriptorCategory::ReadWriteResource);
              found++;bufferAPIChecks++;
            }
          if(selected && found!=2)good=false;
        }
        if(textureOutput && event)
        {
          auto slot=replay->GetBufferData(heap,48,24);uint64_t words[3]={};
          if(slot.size()!=24){good=false;break;}memcpy(words,slot.data(),24);
          if(words[0] || !words[1] || words[1]==capturedOutputID || words[2])
            fprintf(stderr,"texture descriptor mismatch event=%u captured=%llu actual=%llu/%llu/%llu\n",event,(unsigned long long)capturedOutputID,(unsigned long long)words[0],(unsigned long long)words[1],(unsigned long long)words[2]);
          good &= !words[0] && words[1] && words[1]!=capturedOutputID && !words[2];
        }
        if(queryGroups)
        {
          auto raw=replay->GetBufferData(arguments,0,64);if(raw.size()!=64){good=false;break;}
          uint32_t actual[3]={};memcpy(actual,raw.data()+argumentOffset,12);
          if(event==0) good &= actual[0]==0 && actual[1]==0 && actual[2]==0;
          for(unsigned step=0;step<dispatches.size();step++)
            if(event==dispatches[step] || event==dispatches[step]-1)
              good &= actual[0]==(sameEncoder?queryGroups/gridHeight:step==0?1U:step==1?queryGroups/gridHeight:0U) && actual[1]==(sameEncoder || step>0?gridHeight:1) && actual[2]==1;
          for(unsigned i=0;i<64;i++)if(i<argumentOffset || i>=argumentOffset+12)good &= raw[i]==0xcd;
        }
        auto h=replay->GetBufferData(header,0,64);
        auto c=replay->GetBufferData(contribution,0,initialContribution.size());
        auto slot=replay->GetBufferData(heap,slotOffset,24);
        if(c!=initialContribution || slot.size()!=24) { good=false; break; }
        if(event==0)
        {
          good &= h.empty();
          for(byte value:slot)good &= value==0;
          continue;
        }
        if(h.size()!=64) { fprintf(stderr,"header missing event=%u build=%u query=%u\n",event,build,dispatch);good=false;break; }
        uint64_t words[8],descriptor[3]; memcpy(words,h.data()+headerOffset,64); memcpy(descriptor,slot.data(),24);
        good &= words[0]!=0 && words[0]!=capturedHeader[0] && words[1]!=0 && words[1]!=capturedHeader[1];
        good &= capturedDescriptor!=0 && descriptor[0]!=0 && descriptor[0]!=capturedDescriptor && descriptor[1]==0 && descriptor[2]==0;
        if(cycle==0 && event==last) memcpy(replayHeader,words,64);
        good &= memcmp(words,replayHeader,64)==0;
        for(unsigned i=2;i<8;i++) good &= words[i]==0;
        if(event>=dispatch)
        {
          auto source=replay->GetBufferData(frameSource,0,0);
          good &= !source.empty();for(byte value:source)good &= value==0;
        }
      }
    if(mixedRoots)good &= changedSamplerMask==63;
    if(!textureOutput || texelOutput)good &= bufferAPIChecks==dispatches.size()*4*(texelOutput?2:1);
    replay->Shutdown(); RENDERDOC_ShutdownReplay();
    if(!good) { fprintf(stderr,"FAIL heap-query frame Header/build/output/descriptor/usage/reset\n"); return 6; }
    printf("PASS converted heap RayQuery %u/%u/0/0; typed kind3, changed AS ID/VA, immutable contribution, usage, Private source cleared, %zu event choices/EID0, frame-born Header/build\n",expectedHit,expectedHit,choices.size()*4);
    if(textureOutput)printf("PASS texture UAV %s: all texels/channels, initial data, identity relocation, CS_RW usage and event/EID0 reset verified\n",floatTextureOutput?"RGBA32Float":"R32Uint");
    if(texelOutput)printf("PASS %u typed buffer API checks: parent/view identity, format, nonzero offset/range and padding, writable location and EID0\n",bufferAPIChecks);
    else if(!textureOutput)printf("PASS %u buffer API checks: current ReadWriteBuffer identity, offset and full bound range\n",bufferAPIChecks);
    if(mixedRoots){printf("Mixed sampler identity changes=%u; texture pixels and all table scalar/padding bytes verified\n",changedSamplers);}
    if(mixedRoots)printf("PASS %u mixed roots: %u CBVs plus six static samplers; read-only resources/table metadata/padding stable\n",cbvCount,cbvCount-1);
    else if(cbvCount)printf("PASS %u CBV roots; all opaque data/padding/read-only usage stable across all event choices\n",cbvCount);
    if(queryGroups)printf("PASS per-use indirect heap-query %s; %u uint results, argument values/padding and Indirect usage, %zu event choices/EID0\n",sameEncoder?"same encoder ordinals0/1/2":"GPU groups1/large/0",queryGroups*4,choices.size()*4);
    return 0;
  }
}
