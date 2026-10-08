// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <algorithm>
REPLAY_PROGRAM_MARKER()

int main(int argc,char **argv)
{
  if(argc<2 || argc>4)return 1;
  const char *mode=argc>=3?argv[2]:"default";
  const unsigned instanceCount=argc==4?strtoul(argv[3],nullptr,10):1;
  const bool frame=strstr(mode,"frame");
  const bool geometry=strstr(mode,"geometry"),newTarget=strstr(mode,"new-target"),dynamicHeader=strstr(mode,"dynamic-header");
  const unsigned geometryCount=strstr(mode,"multi64")?64:strstr(mode,"multi")?2:1;
  @autoreleasepool
  {
    // Displace AS identities without dispatching unrelated GPU work.
    __attribute__((objc_precise_lifetime)) id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    __attribute__((objc_precise_lifetime)) NSMutableArray *padding=[NSMutableArray array];
    __attribute__((objc_precise_lifetime)) id<MTLBuffer> addressPadding=[device newBufferWithLength:1024*1024 options:MTLResourceStorageModeShared];
    if(!addressPadding)return 2;
    for(unsigned i=0;i<16;i++) {
      auto as=[device newAccelerationStructureWithSize:4096];if(!as)return 2;[padding addObject:as];
    }
    GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);
    RENDERDOC_InitialiseReplay(env,args);
    ICaptureFile *file=RENDERDOC_OpenCaptureFile();auto opened=file->OpenFile(argv[1],"rdc",{});
    if(opened.code!=ResultCode::Succeeded){fprintf(stderr,"OpenFile %s\n",opened.internal_msg?opened.internal_msg->c_str():"unknown");file->Shutdown();return 3;}
    ResourceId contributions; uint64_t contributionOffset=0,capturedContribution=0;
    rdcarray<byte> contributionInitial;size_t headerWriteChunk=0;
    ResourceId output,header,roots,as,beforeOutput,beforeRoots,child,instanceSource,geometryVertices,geometryIndices,beforeHeader,beforeAS;uint64_t capturedAS=0,capturedBeforeAS=0,headerOffset=0,rootOffset=0,capturedRoots[2]={},capturedBeforeRoots[2]={};
    const auto &input=file->GetStructuredData();
    for(const auto *chunk:input.chunks)if(chunk->name=="MTLComputePipelineState::DeclareRayQueryDispatch") {
      if(roots!=ResourceId()) {
        beforeRoots=chunk->FindChild("roots")->AsResourceId();beforeOutput=chunk->FindChild("output")->AsResourceId();beforeHeader=chunk->FindChild("header")->AsResourceId();continue;
      }
      roots=chunk->FindChild("roots")->AsResourceId();header=chunk->FindChild("header")->AsResourceId();
      output=chunk->FindChild("output")->AsResourceId();
      rootOffset=chunk->FindChild("offset")->data.basic.u;
    }
    for(const auto *chunk:input.chunks)if(chunk->name=="MTLBuffer::DeclareRayASHeader" && chunk->FindChild("buffer")->AsResourceId()==header) {
      contributions=chunk->FindChild("contributions")->AsResourceId();
      contributionOffset=chunk->FindChild("contributionOffset")->data.basic.u;
      auto blob=chunk->FindChild("bytes");
      if(!blob || blob->data.basic.u>=input.buffers.size() || !input.buffers[blob->data.basic.u] || input.buffers[blob->data.basic.u]->size()!=64)return 6;
      memcpy(&capturedContribution,input.buffers[blob->data.basic.u]->data()+8,8);
    }
    for(const auto *chunk:input.chunks)if(chunk->name=="MTLDevice::newBufferWithBytes" &&
        chunk->FindChild("Buffer")->AsResourceId()==roots) {
      auto blob=chunk->FindChild("initialData");
      if(blob && blob->data.basic.u<input.buffers.size() && input.buffers[blob->data.basic.u] &&
         rootOffset<=input.buffers[blob->data.basic.u]->size() &&
         input.buffers[blob->data.basic.u]->size()-rootOffset>=16)
        memcpy(capturedRoots,input.buffers[blob->data.basic.u]->data()+rootOffset,16);
    }
    for(const auto *chunk:input.chunks)if(chunk->name=="MTLBuffer::DeclareDescriptorTable" &&
        chunk->FindChild("buffer")->AsResourceId()==header && chunk->FindChild("schema")->data.basic.u==4)
      headerOffset=chunk->FindChild("offset")->data.basic.u;
    for(const auto *chunk:input.chunks)if(chunk->name=="Internal::Initial Contents" &&
        chunk->FindChild("id")->AsResourceId()==header) {
      auto blob=chunk->FindChild("Contents");
      if(blob && blob->data.basic.u<input.buffers.size() && input.buffers[blob->data.basic.u] &&
         input.buffers[blob->data.basic.u]->size()>=headerOffset+64)
        memcpy(&capturedAS,input.buffers[blob->data.basic.u]->data()+headerOffset,8);
    }
    for(const auto *chunk:input.chunks)if(chunk->name=="Internal::Initial Contents" &&
        chunk->FindChild("id")->AsResourceId()==beforeRoots && beforeRoots!=ResourceId()) {
      auto blob=chunk->FindChild("Contents");
      if(blob && blob->data.basic.u<input.buffers.size() && input.buffers[blob->data.basic.u] &&
         input.buffers[blob->data.basic.u]->size()>=rootOffset+16)
        memcpy(capturedBeforeRoots,input.buffers[blob->data.basic.u]->data()+rootOffset,16);
    }
    for(const auto *chunk:input.chunks)if(chunk->name=="Internal::Initial Contents" &&
        beforeHeader!=ResourceId() && chunk->FindChild("id")->AsResourceId()==beforeHeader) {
      auto blob=chunk->FindChild("Contents");
      if(blob && blob->data.basic.u<input.buffers.size() && input.buffers[blob->data.basic.u] &&
         input.buffers[blob->data.basic.u]->size()>=headerOffset+64)
        memcpy(&capturedBeforeAS,input.buffers[blob->data.basic.u]->data()+headerOffset,8);
    }
    if(contributions!=ResourceId())for(const auto *chunk:input.chunks)
      if(chunk->name=="Internal::Initial Contents" && chunk->FindChild("id")->AsResourceId()==contributions) {
        auto blob=chunk->FindChild("Contents");
        if(blob && blob->data.basic.u<input.buffers.size() && input.buffers[blob->data.basic.u])
          contributionInitial.assign(input.buffers[blob->data.basic.u]->data(),input.buffers[blob->data.basic.u]->size());
      }
    if(dynamicHeader) {
      capturedBeforeAS=capturedAS;
      for(size_t i=0;i<input.chunks.size();i++) {
        const auto *chunk=input.chunks[i];
        if(chunk->name=="MTLBuffer::DeclareRayASHeader" && chunk->FindChild("buffer")->AsResourceId()==header) {
          auto blob=chunk->FindChild("bytes");
          if(!blob || blob->data.basic.u>=input.buffers.size() || !input.buffers[blob->data.basic.u] || input.buffers[blob->data.basic.u]->size()!=64)return 6;
          memcpy(&capturedAS,input.buffers[blob->data.basic.u]->data(),8);
          if(capturedAS!=capturedBeforeAS && !headerWriteChunk)headerWriteChunk=i;
        }
      }
      if(capturedAS==capturedBeforeAS)return 6;
    }
    for(const auto *chunk:input.chunks)if(chunk->name=="MTLAccelerationStructure::CaptureGPUIdentity") {
      const auto identity=chunk->FindChild("value")->data.basic.u;
      if(identity==capturedAS)as=chunk->FindChild("resource")->AsResourceId();
      if(identity==capturedBeforeAS)beforeAS=chunk->FindChild("resource")->AsResourceId();
    }
    if(newTarget) {
      if(as==ResourceId() || beforeAS==ResourceId() || as==beforeAS || (!dynamicHeader && header==beforeHeader))return 6;
      if(!strstr(mode,"reused"))for(const auto *chunk:input.chunks)if(chunk->name=="Internal::Initial Contents" &&
          chunk->FindChild("id")->AsResourceId()==as)return 6;
    }
    for(const auto *chunk:input.chunks)if(chunk->name=="MTLAccelerationStructureCommandEncoder::buildIndirectInstances") {
      auto children=chunk->FindChild("children");if(children && children->NumChildren())child=children->GetChild(0)->AsResourceId();
      instanceSource=chunk->FindChild("instances")->AsResourceId();
    }
    for(const auto *chunk:input.chunks)if(chunk->name=="MTLAccelerationStructureCommandEncoder::buildFrozenTriangles" ||
        chunk->name=="MTLAccelerationStructureCommandEncoder::buildFrozenMultiIndexed") {
      geometryVertices=chunk->FindChild("vertices")->AsResourceId();geometryIndices=chunk->FindChild("indices")->AsResourceId();
    }
    auto replay=file->OpenCapture({},nullptr);IReplayController *controller=replay.second;
    if(replay.first.code!=ResultCode::Succeeded){fprintf(stderr,"OpenCapture %s\n",replay.first.internal_msg?replay.first.internal_msg->c_str():"unknown");file->Shutdown();return 4;}
    for(const auto &resource:controller->GetResources()) {
      if(resource.name=="IR query results")output=resource.resourceId;
      if(resource.name=="IR query AS header")header=resource.resourceId;
      if(resource.name=="IR query roots")roots=resource.resourceId;
      if(resource.name=="IR query before results")beforeOutput=resource.resourceId;
      if(resource.name=="IR query before roots")beforeRoots=resource.resourceId;
      if(resource.name=="IR query TLAS" && !dynamicHeader)as=resource.resourceId;
    }
    uint32_t dispatch=0,first=0,last=0,headerWriteEvent=0;
    uint64_t liveBeforeHeader=0,liveAfterHeader=0,beforeContributionVA=0,afterContributionVA=0;
    rdcarray<uint32_t> dispatches;
    std::function<void(const rdcarray<ActionDescription>&)> scan=[&](const auto &actions) {
      for(const auto &action:actions) {
        if(dynamicHeader)for(const auto &event:action.events)if(event.chunkIndex==headerWriteChunk)headerWriteEvent=event.eventId;
        if(!first)first=action.eventId;last=std::max(last,action.eventId);
        if(action.flags&ActionFlags::Dispatch){dispatch=action.eventId;dispatches.push_back(dispatch);}scan(action.children);}
    };scan(controller->GetRootActions());
    bool passed=output!=ResourceId() && roots!=ResourceId() && header!=ResourceId() && as!=ResourceId() && dispatch && last>=dispatch;
    passed &=dispatches.size()==(frame?2U:1U) && (!frame || (beforeOutput!=ResourceId() && beforeRoots!=ResourceId()));
    const uint32_t hit=frame?(geometry?1250:1000)+17*((strstr(mode,"user75")?75:74)+instanceCount-1)+13*(strstr(mode,"geometry-order-control")?0:geometryCount-1):strstr(mode,"empty") || strstr(mode,"masked")?0:strcmp(mode,"default")?1000+17*(73+instanceCount-1):1000;
    const uint32_t result[]={hit,hit,0,0},initial[]={7,7,7,7};
    const uint32_t beforeHit=strstr(mode,"empty") || strstr(mode,"masked")?0:1000+17*(73+instanceCount-1);
    const uint32_t beforeResult[]={beforeHit,beforeHit,0,0};
    rdcarray<uint32_t> choices={last,dispatch,dispatch-1};
    if(frame && !dispatches.empty()){choices.push_back(dispatches[0]);choices.push_back(dispatches[0]-1);}
    if(dynamicHeader) { if(!headerWriteEvent)passed=false;choices.push_back(headerWriteEvent);choices.push_back(headerWriteEvent-1); }
    choices.push_back(0);
    for(unsigned cycle=0;passed && cycle<4;cycle++) {
      for(auto event:choices) {
        controller->SetFrameEvent(event,true);auto bytes=controller->GetBufferData(output,0,16);
        const bool after=event>=dispatch;
        if(bytes.size()!=16 || memcmp(bytes.data(),after?result:initial,16)) {passed=false;break;}
        if(frame) {
          auto beforeBytes=controller->GetBufferData(beforeOutput,0,16);
          if(beforeBytes.size()!=16 || memcmp(beforeBytes.data(),event>=dispatches[0]?beforeResult:initial,16)){passed=false;break;}
        }
        auto headerBytes=controller->GetBufferData(header,0,0);uint64_t live=0;
        if(headerBytes.size()<headerOffset+64){passed=false;break;}memcpy(&live,headerBytes.data()+headerOffset,8);
        if(headerOffset)for(size_t at=0;at<headerBytes.size();at++)if((at<headerOffset || at>=headerOffset+64) && headerBytes[at]!=0xa5)passed=false;
        if(!live || !capturedAS || live==capturedAS){passed=false;break;}
        if(contributions!=ResourceId()) {
          uint64_t address=0;memcpy(&address,headerBytes.data()+headerOffset+8,8);
          auto bytes=controller->GetBufferData(contributions,0,0);
          if(!address || address==capturedContribution || contributionInitial.empty() || bytes!=contributionInitial){passed=false;break;}
          if(dynamicHeader) {
            if(event>=headerWriteEvent){liveAfterHeader=live;afterContributionVA=address;}
            else {liveBeforeHeader=live;beforeContributionVA=address;}
            if(liveBeforeHeader && liveAfterHeader && liveBeforeHeader==liveAfterHeader)passed=false;
            if(beforeContributionVA && afterContributionVA && afterContributionVA!=beforeContributionVA+16)passed=false;
          }
          for(size_t i=headerOffset+16;i<headerOffset+64;i++)if(headerBytes[i])passed=false;
        }
        auto rootBytes=controller->GetBufferData(roots,0,0);uint64_t liveRoots[2]={};
        if(rootBytes.size()<rootOffset+16){passed=false;break;}
        memcpy(liveRoots,rootBytes.data()+rootOffset,16);
        if(!liveRoots[0] || !liveRoots[1] || liveRoots[0]==capturedRoots[0] || liveRoots[1]==capturedRoots[1]){passed=false;break;}
        if(rootOffset==8) {
          uint64_t prefix=0,suffix=0;if(rootBytes.size()!=32){passed=false;break;}
          memcpy(&prefix,rootBytes.data(),8);memcpy(&suffix,rootBytes.data()+24,8);
          if(prefix!=0xa5a5d00d12345678ULL || suffix!=0x8877665544332211ULL){passed=false;break;}
        }
        if(frame) {
          auto sourceBytes=controller->GetBufferData(instanceSource,0,0);
          if(sourceBytes.size()!=instanceCount*80+8 ||
             std::any_of(sourceBytes.begin(),sourceBytes.end(),[](byte value){return value!=0;})){passed=false;break;}
          if(geometry && event>=dispatch) {
            auto vertices=controller->GetBufferData(geometryVertices,0,0);
            if(vertices.size()!=(geometryCount>1?256U:72U) ||
               std::any_of(vertices.begin(),vertices.end(),[](byte value){return value!=0;})){passed=false;break;}
            if(geometryIndices!=ResourceId()) {
              auto indices=controller->GetBufferData(geometryIndices,0,0);
              if(indices.size()!=(geometryCount>1?64U:24U) ||
                 std::any_of(indices.begin(),indices.end(),[](byte value){return value!=0;})){passed=false;break;}
            }
          }
          auto beforeBytes=controller->GetBufferData(beforeRoots,0,0);uint64_t beforeValues[2]={};
          if(beforeBytes.size()<rootOffset+16){passed=false;break;}
          memcpy(beforeValues,beforeBytes.data()+rootOffset,16);
          if((newTarget && !dynamicHeader?beforeValues[0]==liveRoots[0]:beforeValues[0]!=liveRoots[0]) || !beforeValues[1] || beforeValues[1]==liveRoots[1] ||
             !capturedBeforeRoots[0] || !capturedBeforeRoots[1] || beforeValues[0]==capturedBeforeRoots[0] ||
             beforeValues[1]==capturedBeforeRoots[1]){passed=false;break;}
          if(newTarget && !dynamicHeader) {
            auto beforeHeaderBytes=controller->GetBufferData(beforeHeader,0,0);uint64_t beforeLive=0;
            if(beforeHeaderBytes.size()<headerOffset+64){passed=false;break;}
            memcpy(&beforeLive,beforeHeaderBytes.data()+headerOffset,8);
            if(headerOffset)for(size_t at=0;at<beforeHeaderBytes.size();at++)if((at<headerOffset || at>=headerOffset+64) && beforeHeaderBytes[at]!=0xa5)passed=false;
            if(!beforeLive || beforeLive==live || beforeLive==capturedBeforeAS){passed=false;break;}
          }
          if(rootOffset==8) {
            uint64_t prefix=0,suffix=0;if(beforeBytes.size()!=32){passed=false;break;}
            memcpy(&prefix,beforeBytes.data(),8);memcpy(&suffix,beforeBytes.data()+24,8);
            if(prefix!=0xa5a5d00d12345678ULL || suffix!=0x8877665544332211ULL){passed=false;break;}
          }
        }
      }
    }
    for(auto id:{roots,header,as}) {
      bool read=false,write=false;for(const auto &use:controller->GetUsage(id))if(use.eventId==dispatch) {
        read|=use.usage==ResourceUsage::CS_Resource;write|=use.usage==ResourceUsage::CS_RWResource;
      }passed &=read && !write;
    }
    if(contributions!=ResourceId()) {
      bool read=false;for(const auto &use:controller->GetUsage(contributions))read|=use.eventId==dispatch && use.usage==ResourceUsage::CS_Resource;passed &=read;
    }
    bool outputWrite=false;for(const auto &use:controller->GetUsage(output))outputWrite|=use.eventId==dispatch && use.usage==ResourceUsage::CS_RWResource;
    passed &=outputWrite;
    if(frame) {
      bool beforeWrite=false,afterWrite=false;
      for(const auto &use:controller->GetUsage(beforeOutput))if(use.usage==ResourceUsage::CS_RWResource) {
        beforeWrite|=use.eventId==dispatches[0];afterWrite|=use.eventId==dispatch;
      }
      passed &=beforeWrite && !afterWrite;
      bool mainBeforeWrite=false;for(const auto &use:controller->GetUsage(output))mainBeforeWrite|=use.eventId==dispatches[0] && use.usage==ResourceUsage::CS_RWResource;
      passed &=!mainBeforeWrite;
      for(auto id:{beforeRoots,newTarget?beforeHeader:header,newTarget?beforeAS:as}) {
        bool read=false;for(const auto &use:controller->GetUsage(id))read|=use.eventId==dispatches[0] && use.usage==ResourceUsage::CS_Resource;
        passed &=read;
      }
      if(newTarget)for(auto id:{as,header}) {
        if(dynamicHeader && id==header)continue;
        bool readBefore=false;for(const auto &use:controller->GetUsage(id))
          readBefore|=use.eventId==dispatches[0] && use.usage==ResourceUsage::CS_Resource;
        passed &=!readBefore;
      }
      bool childBefore=false,childAfter=false;
      for(const auto &use:controller->GetUsage(child))if(use.usage==ResourceUsage::CS_Resource) {
        childBefore|=use.eventId==dispatches[0];childAfter|=use.eventId==dispatch;
      }
      passed &=child!=ResourceId() && childAfter && (childBefore==(!strstr(mode,"empty") && !strstr(mode,"masked")));
    }
    printf("%s converted RayQuery replay %s four uint results=%u/%u/0/0, changed AS ID/VA, read/write usage, %zu event choices/EID0\n",passed?"PASS":"FAIL",mode,hit,hit,choices.size()*4);
    controller->Shutdown();file->Shutdown();RENDERDOC_ShutdownReplay();return passed?0:5;
  }
}
