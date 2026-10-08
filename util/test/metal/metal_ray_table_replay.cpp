// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <set>
#include <map>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()

static rdcarray<APIEvent> events;
static rdcarray<uint32_t> dispatches;
static unsigned groups = 0, signs = 0, builds = 0, writes = 0;
static uint32_t buildEvent = 0, writeEvent = 0;
static bool background = false, frameTLAS = false;
static bool Walk(const rdcarray<ActionDescription> &actions, rdcstr owner)
{
  bool ok = true;
  for(const auto &a : actions)
  {
    events.append(a.events);
    printf("EID %u: %s, owner %s\n",a.eventId,a.customName.c_str(),owner.c_str());
    if(a.customName == "Ray AS outer") {ok &= bool(a.flags & ActionFlags::PushMarker);groups++;}
    if(a.customName == "Ray AS build" || a.customName == "Ray AS size")
    {ok &= owner == "Ray AS outer" && bool(a.flags & ActionFlags::PushMarker);groups++;}
    if(a.customName == "Before BLAS") {ok &= owner == "Ray AS outer" && bool(a.flags & ActionFlags::SetMarker);signs++;}
    if(a.customName == "After size write") {ok &= owner == "Ray AS size" && bool(a.flags & ActionFlags::SetMarker);signs++;}
    if(a.flags & ActionFlags::BuildAccStruct) {ok &= owner == (frameTLAS?"":"Ray AS build");builds++;buildEvent=a.eventId;}
    if(a.customName == "Write Metal Compacted Acceleration Structure Size")
    {ok &= owner == "Ray AS size";writes++;writeEvent=a.eventId;}
    if(a.flags & ActionFlags::Dispatch) dispatches.push_back(a.eventId);
    ok &= !a.IsFakeMarker();
    ok &= Walk(a.children, (a.flags & ActionFlags::PushMarker) ? a.customName : owner);
  }
  return ok;
}

int main(int argc, char **argv)
{
  if(argc != 2 && argc != 3) return 1;
  background = argc==3 && strncmp(argv[2],"background",10)==0;
  const bool late = argc==3 && strcmp(argv[2],"background-late")==0;
  const bool tlas = background && strstr(argv[2],"tlas");
  const bool userID=tlas && strstr(argv[2],"user-id");
  const bool indirect=tlas && strstr(argv[2],"indirect");
  const bool emptyIndirect=indirect && strstr(argv[2],"empty");
  const bool inactiveIndirect=indirect && strstr(argv[2],"inactive");
  const char *manyTag=indirect?strstr(argv[2],"many-"):nullptr;
  const unsigned manyChildren=manyTag?strtoul(manyTag+5,nullptr,10):0;
  const unsigned instanceUserID=(userID || indirect) && strstr(argv[2],"high")?0xf0000049U:73U;
  const bool repeated = tlas && strstr(argv[2],"repeated");
  frameTLAS=tlas && strstr(argv[2],"frame");
  const bool tlasLate = tlas && strstr(argv[2],"late");
  const bool indexLate = argc==3 && strstr(argv[2],"indexed") && strstr(argv[2],"late");
  const bool formatted=argc==3 && strstr(argv[2],"formatted");
  const bool multiIndexed=argc==3 && strstr(argv[2],"multi-indexed");
  const bool identity=argc==3 && strstr(argv[2],"identity");
  const bool tableIdentity=argc==3 && strstr(argv[2],"table-identity");
  const bool frameBornTables=tableIdentity && strstr(argv[2],"frame-born");
  const bool largeVisible=argc==3 && strstr(argv[2],"large-visible");
  const unsigned visibleCount=largeVisible?strtoul(strrchr(argv[2],'-')+1,nullptr,10):0;
  const bool mutated = argc==3 && (strcmp(argv[2],"background-mutated")==0 || strstr(argv[2],"compact-rebuilt") ||
      (tlas && strstr(argv[2],"mutated")));
  GlobalEnvironment env; env.enumerateGPUs=false;
  RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile(); auto result=file->OpenFile(argv[1],"rdc",nullptr);
  IReplayController *c=nullptr; if(result.OK()) rdctie(result,c)=file->OpenCapture(ReplayOptions(),nullptr);
  file->Shutdown(); if(!result.OK() || !c)
  {
    fprintf(stderr,"Could not open ray capture %s (code %u): %s\n",argv[1],
            unsigned(result.code),result.internal_msg ? result.internal_msg->c_str() : "no details");
    return 2;
  }
  bool ok=Walk(c->GetRootActions(),"") && groups==(background?0:3) && signs==(background?0:2) &&
      builds==((background && !frameTLAS)?0:strstr(argv[2],"frame-rebuild")?2:1) && writes==(background?0:1) && dispatches.size()==4;
  ResourceId output,compacted,structure,vertices,indices,instances;
  for(auto r:c->GetResources())
  {
    if(r.name=="Ray table clear/rebind results") output=r.resourceId;
    if(r.name=="Ray triangle vertices") vertices=r.resourceId;
    if(r.name=="Ray initial indices") indices=r.resourceId;
    if(r.name=="Ray indirect instances") instances=r.resourceId;
    if(r.name=="Ray compacted size") compacted=r.resourceId;

  }
  for(auto chunk:c->GetStructuredFile().chunks)
    if(chunk->name=="MTLComputeCommandEncoder::setAccelerationStructure")
    {
      auto bound=chunk->FindChild("structure")->AsResourceId();
      if(structure!=ResourceId()) ok &= bound==structure;
      structure=bound;
    }
  ok &= output!=ResourceId() && (background || compacted!=ResourceId()) && structure!=ResourceId();
  if(identity)
  {
    std::set<ResourceId> identities;
    for(auto chunk:c->GetStructuredFile().chunks)
      if(chunk->name=="MTLAccelerationStructure::CaptureGPUIdentity")
      {
        const auto resource=chunk->FindChild("resource")->AsResourceId();
        ok &= resource!=ResourceId() && chunk->FindChild("value")->data.basic.u>0;
        identities.insert(resource);
      }
    ok &= identities.count(structure)>0 &&
          identities.size()==((tlas || strstr(argv[2],"unused"))?2U:1U);
  }
  if(largeVisible)
  {
    unsigned tables=0,visibleUpdates=0,visibleNulls=0,lastUpdates=0;
    for(auto chunk:c->GetStructuredFile().chunks)
    {
      if(chunk->name=="MTLComputePipelineState::newVisibleFunctionTableWithDescriptor")
      { tables++; ok &= chunk->FindChild("count")->data.basic.u==visibleCount; }
      if(chunk->name=="MTLVisibleFunctionTable::setFunction")
      {
        visibleUpdates++;
        visibleNulls+=chunk->FindChild("function")->AsResourceId()==ResourceId();
        lastUpdates+=chunk->FindChild("index")->data.basic.u==visibleCount-1;
      }
    }
    ok &= tables==(tableIdentity && strstr(argv[2],"unused")?2U:1U) && visibleUpdates==5 && visibleNulls==2 && lastUpdates==3;
  }
  if(tableIdentity)
  {
    std::map<ResourceId,std::pair<unsigned,uint64_t>> identities;
    std::set<uint64_t> values[2];
    unsigned counts[2]={};
    for(auto chunk:c->GetStructuredFile().chunks)
      if(chunk->name=="MTLFunctionTable::CaptureGPUIdentity")
      {
        const auto resource=chunk->FindChild("resource")->AsResourceId();
        const unsigned kind=chunk->FindChild("kind")->data.basic.u;
        const uint64_t value=chunk->FindChild("value")->data.basic.u;
        ok &= resource!=ResourceId() && kind<=1 && value>0;
        if(kind>1) continue;
        const auto entry=std::make_pair(kind,value);
        if(identities.count(resource)) ok &= identities[resource]==entry;
        else {identities[resource]=entry; counts[kind]++; ok &= values[kind].insert(value).second;}
      }
    const bool unused=strstr(argv[2],"unused");
    ok &= counts[0]==(unused?2U:1U) && counts[1]==(unused?5U:4U);
  }
  std::set<uint32_t> ids;
  for(auto e:events) ok &= ids.insert(e.eventId).second;
  unsigned nulls=0, updates=0, fenceUpdates=0,fenceWaits=0;
  for(auto chunk:c->GetStructuredFile().chunks) {
    fenceUpdates+=chunk->name=="MTLAccelerationStructureCommandEncoder::updateFence";
    fenceWaits+=chunk->name=="MTLAccelerationStructureCommandEncoder::waitForFence";
  }
  ok &= (fenceUpdates==0 && fenceWaits==0) || (fenceUpdates==1 && fenceWaits==1);
  uint64_t dispatchOffsets[4]={}, writeOffset=0;
  uint64_t outputBirthOffset=0;
  for(auto event:events) {
    for(unsigned slot=0;slot<4;slot++)if(event.eventId==dispatches[slot])dispatchOffsets[slot]=event.fileOffset;
    if(event.eventId==writeEvent)writeOffset=event.fileOffset;
    const auto chunk=c->GetStructuredFile().chunks[event.chunkIndex];
    if(chunk->name=="MTLDevice::newBufferWithBytes" &&
       chunk->FindChild("Buffer")->AsResourceId()==output)
      outputBirthOffset=event.fileOffset;
  }
  ok &= !frameBornTables || outputBirthOffset!=0;
  for(auto chunk:c->GetStructuredFile().chunks)
    if(chunk->name=="MTLIntersectionFunctionTable::setFunction")
    {updates++; if(chunk->FindChild("function")->AsResourceId()==ResourceId()) nulls++;}
  ok &= nulls==5 && updates==17;
  const bool opaque=tlas && strstr(argv[2],"instance-opaque");
  const bool maskZero=tlas && strstr(argv[2],"mask-zero");
  const bool gpuMutated=argc==3 && strstr(argv[2],"gpu-mutated");
  const unsigned hitValue=(userID || indirect)?instanceUserID:largeVisible?3:1;
  const unsigned missing=opaque?hitValue:0, bound=(inactiveIndirect||emptyIndirect||late||tlasLate||indexLate||maskZero)?0:hitValue;
  const unsigned expected[4]={missing,bound,missing,strstr(argv[2],"frame-rebuild")?bound+1:bound};
  unsigned initialAS = 0;
  for(auto chunk:c->GetStructuredFile().chunks)
    if(chunk->name=="Internal::Initial Contents" && chunk->FindChild("parameters")) initialAS++;
  const unsigned expectedInitialAS=(emptyIndirect || inactiveIndirect?(frameTLAS?0U:1U):manyChildren?manyChildren+(frameTLAS?0:1):tlas?(frameTLAS?(repeated?2U:1U):(repeated?3U:2U)):
      (background?1U:0U))+(indirect && strstr(argv[2],"unused-built")?1U:0U);
  if(initialAS != expectedInitialAS)
    fprintf(stderr,"AS initial recipes %u expected %u\n",initialAS,expectedInitialAS);
  ok &= initialAS==expectedInitialAS;
  if(manyChildren)
  {
    std::set<ResourceId> children;
    std::set<uint64_t> gpuIdentities;
    unsigned indirectRecipes=0;
    for(auto chunk:c->GetStructuredFile().chunks)
    {
      if(chunk->name=="MTLAccelerationStructure::CaptureGPUIdentity")
      {
        auto resource=chunk->FindChild("resource"); auto value=chunk->FindChild("value");
        ok &= resource && value && value->data.basic.u &&
            children.insert(resource->AsResourceId()).second && gpuIdentities.insert(value->data.basic.u).second;
      }
      if(chunk->name=="Internal::Initial Contents" && chunk->FindChild("kind") &&
         chunk->FindChild("kind")->data.basic.u==9)
      {
        indirectRecipes++;
        auto dependencies=chunk->FindChild("children");
        ok &= dependencies && dependencies->NumChildren()==manyChildren;
      }
    }
    ok &= children.size()==manyChildren && gpuIdentities.size()==manyChildren &&
        indirectRecipes==(frameTLAS?0U:1U);
  }
  uint32_t sourceUploadEvent = 0, sourceEraseEvent = 0;
  uint64_t sourceUploadOffset = 0, sourceEraseOffset = 0;
  bytebuf initialSource, uploadedSource;
  if(frameTLAS && gpuMutated && indirect)
  {
    c->SetFrameEvent(0, true); initialSource = c->GetBufferData(instances, 0, 0);
    for(const auto &event : events)
    {
      auto chunk = c->GetStructuredFile().chunks[event.chunkIndex];
      if(chunk->name == "MTLBlitCommandEncoder::copyFromBuffer" &&
         chunk->FindChild("destinationBuffer")->AsResourceId() == instances)
      {
        sourceUploadEvent = event.eventId; sourceUploadOffset = event.fileOffset;
        uploadedSource = c->GetBufferData(chunk->FindChild("sourceBuffer")->AsResourceId(), 0, 0);
      }
      if(chunk->name == "MTLBlitCommandEncoder::fillBuffer")
      { sourceEraseEvent = event.eventId; sourceEraseOffset = event.fileOffset; }
    }
    ok &= sourceUploadEvent && sourceEraseEvent && !initialSource.empty() &&
        initialSource.size() == uploadedSource.size();
  }
  rdcarray<uint32_t> selections; for(uint32_t id:ids) selections.push_back(id);
  for(unsigned cycle=0;cycle<3 && ok;cycle++)
    for(size_t i=0;i<selections.size() && ok;i++)
    {
      auto eid=selections[cycle==1?selections.size()-1-i:i];
      const APIEvent *selected=nullptr;
      for(const auto &event:events)if(event.eventId==eid)selected=&event;
      const auto name=c->GetStructuredFile().chunks[selected->chunkIndex]->name;
      // GPU commands use the public submission prefix. CPU calls/snapshots use
      // their physical recording boundary, which may follow encoded GPU work.
      bool gpu=name.beginsWith("MTLBlitCommandEncoder::") ||
               name.beginsWith("MTLComputeCommandEncoder::") ||
               name.beginsWith("MTLAccelerationStructureCommandEncoder::");
      c->SetFrameEvent(0,true);
      if(frameBornTables) ok &= c->GetBufferData(output,0,16).empty();
      c->SetFrameEvent(eid,true); ok &= c->GetFatalErrorStatus().OK();
      if(indirect && (strstr(argv[2],"source-erased") || strstr(argv[2],"gpu-mutated")))
      {
        auto source=c->GetBufferData(instances,0,0);
        ok &= !source.empty();
        if(frameTLAS)
        {
          const bool erased = gpu ? eid >= sourceEraseEvent : selected->fileOffset >= sourceEraseOffset;
          const bool uploaded = gpu ? eid >= sourceUploadEvent : selected->fileOffset >= sourceUploadOffset;
          if(erased) {for(byte value:source) ok &= value == 0;}
          else ok &= source == (uploaded ? uploadedSource : initialSource);
        }
        else {for(byte value:source) ok &= value == 0;}
      }
      // Frame-born output has no live resource before its CPU creation chunk.
      // Check absence at EID 0 and the creation boundary, then its initial bytes
      // and every dispatch result, including backward and repeated seeks.
      auto bytes=c->GetBufferData(output,0,16);
      const bool outputBorn=!frameBornTables || selected->fileOffset>=outputBirthOffset;
      if(bytes.size()!=(outputBorn?16U:0U))
        fprintf(stderr,"EID %u output bytes %zu expected %u (birth offset %llu, selected %llu)\n",
                eid,bytes.size(),outputBorn?16U:0U,(unsigned long long)outputBirthOffset,
                (unsigned long long)selected->fileOffset);
      ok &= bytes.size()==(outputBorn?16U:0U);
      if(bytes.size()==16) for(unsigned slot=0;slot<4;slot++)
      {
        unsigned v=0;memcpy(&v,bytes.data()+slot*4,4);
        unsigned want=(gpu ? eid>=dispatches[slot] : selected->fileOffset>=dispatchOffsets[slot])?expected[slot]:7;
        if(v!=want) fprintf(stderr,"EID %u slot %u got %u expected %u\n",eid,slot,v,want);
        ok &= v==want;
      }
      if(!background && (gpu ? eid>=writeEvent : selected->fileOffset>=writeOffset))
      {auto size=c->GetBufferData(compacted,0,8);uint64_t v=0;if(size.size()==8)memcpy(&v,size.data(),8);if(!v || (!multiIndexed && v!=1280))fprintf(stderr,"EID %u compacted %llu\n",eid,(unsigned long long)v);ok &= v>0 && (multiIndexed || v==1280);}
      if((eid==buildEvent && fenceUpdates) || background)
      {
        const float shift=((mutated && !indirect) || late || manyChildren)?100:0;
        const float expectedVertices[9]={-1+shift,-1,0,1+shift,-1,0,shift,1,0};
        const float formattedVertices[19]={42,42,42,42,-1,-1,0,1,99,1,-1,0,1,99,0,1,0,1,99};
        const size_t size=formatted?sizeof(formattedVertices):sizeof(expectedVertices);
        auto data=c->GetBufferData(vertices,0,size);
        ok &= data.size()==size;
        if(gpuMutated && !indirect) {for(byte value:data) if(value) {fprintf(stderr,"GPU-mutated vertex byte %u\n",unsigned(value));ok=false;break;}}
        else ok &= !memcmp(data.data(),formatted?(const void *)formattedVertices:(const void *)expectedVertices,size);
      }
      if(indices!=ResourceId())
      {
        const unsigned shift=(indexLate || (argc==3 && strstr(argv[2],"indexed-mutated")))?3:0;
        const unsigned short want16[5]={65535,(unsigned short)shift,(unsigned short)(1+shift),(unsigned short)(2+shift),65535};
        const unsigned want32[5]={0xffffffff,shift,1+shift,2+shift,0xffffffff};
        const unsigned short multi16[8]={65535,3,4,5,0,1,2,65535};
        const unsigned multi32[8]={0xffffffff,3,4,5,0,1,2,0xffffffff};
        const bool index32=argc==3 && strstr(argv[2],"u32");
        const size_t count=multiIndexed?(index32?sizeof(multi32):sizeof(multi16)):
            (index32?sizeof(want32):sizeof(want16));
        auto data=c->GetBufferData(indices,0,count);
        ok &= data.size()==count;
        if(gpuMutated) {for(byte value:data) if(value) {fprintf(stderr,"GPU-mutated index byte %u\n",unsigned(value));ok=false;break;}}
        else ok &= !memcmp(data.data(),multiIndexed?(index32?(const void *)multi32:(const void *)multi16):
            (index32?(const void *)want32:(const void *)want16),data.size());
      }
      for(unsigned slot=0;slot<4;slot++) if(eid==dispatches[slot])
      {
        auto state=c->GetPipelineState().GetMetalPipelineState();
        ok &= state && state->computeAccelerationStructures.size()>0 &&
              state->computeAccelerationStructures[0]==structure;
      }
      printf("%s cycle %u EID %u GPU ray prefix\n",ok?"PASS":"FAIL",cycle,eid);
    }
  c->Shutdown();RENDERDOC_ShutdownReplay();
  printf("%s markers, %u table updates/%u nulls, %zu events in three directions\n",ok?"PASS":"FAIL",updates,nulls,selections.size());
  return ok?0:3;
}
