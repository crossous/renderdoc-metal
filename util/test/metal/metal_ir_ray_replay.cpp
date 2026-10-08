// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include "renderdoc/driver/metal/official/metal-cpp.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
REPLAY_PROGRAM_MARKER()
static rdcarray<APIEvent> events;
static rdcarray<uint32_t> dispatches;
static void Walk(const rdcarray<ActionDescription> &actions)
{
  for(const auto &a : actions)
  { events.append(a.events); if(a.flags & ActionFlags::Dispatch) dispatches.push_back(a.eventId); Walk(a.children); }
}
int main(int argc,char **argv)
{
  if(argc < 2 || argc > 3) return 1;
  const char *mode = argc == 3 ? argv[2] : "default";
  const bool local = strstr(mode,"local-root");
  const bool global = strstr(mode,"global-");
  const bool heaps = strstr(mode,"descriptor-heaps");
  const bool heapAS = strstr(mode,"heap-as");
  const bool heapOnly = strstr(mode,"heap-as-only");
  const bool indirect = strstr(mode,"indirect-tlas");
  const bool frameBuild = strstr(mode,"frame-build");
  const bool geometry = strstr(mode,"geometry");
  const bool multiGeometry = geometry && strstr(mode,"multi");
  const uint32_t geometryBias = multiGeometry && !strstr(mode,"geometry-order-control") ? 17*(strstr(mode,"multi64")?63:1) : 0;
  const bool geometryFlipped = geometry && !strstr(mode,"geometry-original-control");
  const uint32_t indirectUserID = strstr(mode,"-user75") ? 75 : (frameBuild || strstr(mode,"-user74")) ? 74 : 73;
  const bool inactive = indirect && (strstr(mode,"-empty") || strstr(mode,"-masked"));
  const uint64_t recordStride = local ? 96 : 32;
  const bool nullHit = strstr(mode,"null-hit") || strstr(mode,"null-both") || strstr(mode,"no-closest");
  const bool nullMiss = strstr(mode,"null-miss") || strstr(mode,"null-both");
  const bool any = strstr(mode,"any-hit");
  const uint64_t pad = strstr(mode,"ue-") ? ~0ULL : !strcmp(mode,"pattern-pad") ? 0xa5a5d00d98761234ULL : 0;
  const unsigned baseMiss = nullMiss ? 7 : local ? 243 : 11, baseHit = any ? baseMiss : nullHit ? 7 : (local ? 131 : 73)+(indirect?indirectUserID:0)+geometryBias;
  const unsigned wantMiss = (inactive&&!frameBuild?baseMiss:(heapAS!=geometryFlipped)?baseHit:baseMiss)+(global?1211:0)+(heaps?37:0), wantHit = (inactive&&!frameBuild?baseMiss:(heapAS!=geometryFlipped)?baseMiss:baseHit)+(global?1211:0)+(heaps?23:0);
  GlobalEnvironment env; env.enumerateGPUs = false; RENDERDOC_InitialiseReplay(env,{argv[0]});
  // Keep native allocations alive across replay to change both VA and AS-ID
  // ordering. Correct pixels must not depend on the capture process's integers.
  auto nativeDevice = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
  auto padding = NS::TransferPtr(nativeDevice->newBuffer(1048576, MTL::ResourceStorageModeShared));
  auto unusedAS = NS::TransferPtr(nativeDevice->newAccelerationStructure(4096));
  // Empty/masked TLAS captures can omit the unused BLAS from their initial
  // state. Reserve a second AS ID to still force a different live TLAS ID.
  NS::SharedPtr<MTL::AccelerationStructure> unusedInactiveAS;
  if(inactive)unusedInactiveAS=NS::TransferPtr(nativeDevice->newAccelerationStructure(4096));
  if(!padding || !unusedAS || (inactive && !unusedInactiveAS)) return 4;
  NS::SharedPtr<MTL::Texture> unusedTexture;
  if(local || global || heaps)
  {
    auto td=NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    td->setTextureType(MTL::TextureType2D);td->setPixelFormat(MTL::PixelFormatR32Float);
    td->setWidth(4);td->setHeight(1);td->setStorageMode(MTL::StorageModeShared);td->setUsage(MTL::TextureUsageShaderRead);
    unusedTexture=NS::TransferPtr(nativeDevice->newTexture(td.get()));
    if(!unusedTexture || !unusedTexture->gpuResourceID()._impl) return 5;
  }
  auto file = RENDERDOC_OpenCaptureFile(); auto result = file->OpenFile(argv[1],"rdc",nullptr);
  std::map<ResourceId,bytebuf> capturedInitial;
  if(result.OK())
  {
    const auto &structured=file->GetStructuredData();
    for(auto chunk:structured.chunks)
    {
      const SDObject *blob=nullptr;ResourceId id;
      if(chunk->name=="Internal::Initial Contents" && chunk->FindChild("Contents"))
      {blob=chunk->FindChild("Contents");id=chunk->FindChild("id")->AsResourceId();}
      else if(chunk->name=="MTLDevice::newBufferWithBytes")
      {blob=chunk->FindChild("initialData");id=chunk->FindChild("Buffer")->AsResourceId();}
      if(blob && blob->data.basic.u<structured.buffers.size() && structured.buffers[blob->data.basic.u])
        capturedInitial[id]=*structured.buffers[blob->data.basic.u];
    }
  }
  IReplayController *c = nullptr; if(result.OK()) rdctie(result,c) = file->OpenCapture(ReplayOptions(),nullptr);
  file->Shutdown(); if(!result.OK() || !c) { fprintf(stderr,"Cannot open IR capture: code %u\n",(unsigned)result.code); return 2; }
  Walk(c->GetRootActions()); std::sort(events.begin(),events.end(),[](const APIEvent &a,const APIEvent &b){return a.eventId<b.eventId;});
  std::map<rdcstr,ResourceId> resources;
  for(const auto &r : c->GetResources()) resources[r.name] = r.resourceId;
  ResourceId output = resources["IR ray results"], packet = resources["IR dispatch packet"];
  bool ok = output != ResourceId() && packet != ResourceId() && dispatches.size() == (frameBuild?2U:1U);
  ResourceId beforeOutput = resources["IR before ray results"];
  ResourceId beforePacket = resources["IR before dispatch packet"];
  uint64_t beforeOffset = 0;
  for(const auto &e:events)if(frameBuild && e.eventId==dispatches.front())beforeOffset=e.fileOffset;
  uint64_t dispatchOffset = 0;
  for(const auto &e : events) if(!dispatches.empty() && e.eventId == dispatches.back()) dispatchOffset = e.fileOffset;
  for(const auto &name : {"IR global root","IR shader records","IR AS header","IR instance contributions","IR dispatch packet"})
  {
    if(heapOnly && (rdcstr(name)=="IR AS header" || rdcstr(name)=="IR instance contributions")) continue;
    if(strstr(mode,"-empty") && !heapAS && rdcstr(name)=="IR instance contributions")
    {
      auto h=capturedInitial[resources["IR AS header"]];uint64_t ptr=0;
      if(h.size()==64)memcpy(&ptr,h.data()+8,8);if(!ptr)continue;
    }
    bool used = false;
    for(const auto &u : c->GetUsage(resources[name])) used |= !dispatches.empty() && u.eventId == dispatches.back() && u.usage == ResourceUsage::CS_Resource;
    if(!used) fprintf(stderr,"Missing IR read usage: %s\n",name);
    ok &= used;
  }
  if(local) for(const auto &name : {"IR local CBV","IR local SRV","IR local texture table",
                                  "IR local static sampler table","IR local texture"})
  {
    bool used = false;
    for(const auto &u : c->GetUsage(resources[name])) used |= u.eventId == dispatches.back() && u.usage == ResourceUsage::CS_Resource;
    if(!used) fprintf(stderr,"Missing local-root read usage: %s\n",name);
    ok &= used;
  }
  if(global) for(const auto &name : {"IR global CBV","IR global SRV","IR global texture table",
                                   "IR global static sampler table","IR global texture"})
  {
    bool used=false;
    for(const auto &u:c->GetUsage(resources[name])) used |= u.eventId==dispatches.back() && u.usage==ResourceUsage::CS_Resource;
    if(!used) fprintf(stderr,"Missing global-root read usage: %s\n",name);
    ok &= used;
  }
  if(heaps) for(const auto &name : {"IR heap SRV","IR heap texture","IR resource descriptor heap","IR sampler descriptor heap"})
  {
    bool used=false;
    for(const auto &u:c->GetUsage(resources[name])) used |= u.eventId==dispatches.back() && u.usage==ResourceUsage::CS_Resource;
    if(!used) fprintf(stderr,"Missing descriptor-heap read usage: %s\n",name);
    ok &= used;
  }
  if(heapAS) for(const auto &name : {"IR heap AS header","IR heap instance contributions"})
  {
    if(strstr(mode,"-empty") && rdcstr(name)=="IR heap instance contributions")
    {
      auto h=capturedInitial[resources["IR heap AS header"]];uint64_t ptr=0;
      if(h.size()==64)memcpy(&ptr,h.data()+8,8);if(!ptr)continue;
    }
    bool used=false;
    for(const auto &u:c->GetUsage(resources[name])) used |= u.eventId==dispatches.back() && u.usage==ResourceUsage::CS_Resource;
    if(!used) fprintf(stderr,"Missing heap-AS read usage: %s\n",name);
    ok &= used;
  }
  uint64_t capturedUnusedAS=0;
  auto unusedHeader=capturedInitial[resources["IR AS header"]];
  if(unusedHeader.size()==64)memcpy(&capturedUnusedAS,unusedHeader.data(),8);
  ResourceId selectedAS;
  auto selectedHeader=capturedInitial[resources[heapAS?"IR heap AS header":"IR AS header"]];
  uint64_t selectedGPU=0;if(selectedHeader.size()==64)memcpy(&selectedGPU,selectedHeader.data(),8);
  for(const auto chunk:c->GetStructuredFile().chunks)
    if(chunk->name=="MTLAccelerationStructure::CaptureGPUIdentity" && chunk->FindChild("value")->data.basic.u==selectedGPU)
      selectedAS=chunk->FindChild("resource")->AsResourceId();
  ok &= selectedAS!=ResourceId();
  ResourceId beforePrimitive, afterPrimitive;
  const bool newTarget = geometry && strstr(mode,"new-target");
  if(newTarget)
  {
    for(auto chunk:c->GetStructuredFile().chunks)
    {
      if(chunk->name=="Internal::Initial Contents" && chunk->FindChild("kind") &&
         chunk->FindChild("id")->AsResourceId()==selectedAS && chunk->FindChild("children") &&
         chunk->FindChild("children")->NumChildren()==1)
        beforePrimitive=chunk->FindChild("children")->GetChild(0)->AsResourceId();
      if(chunk->name=="MTLAccelerationStructureCommandEncoder::buildFrozenTriangles" || chunk->name=="MTLAccelerationStructureCommandEncoder::buildFrozenMultiIndexed")
        afterPrimitive=chunk->FindChild("structure")->AsResourceId();
    }
    ok &= beforePrimitive!=ResourceId() && afterPrimitive!=ResourceId() && beforePrimitive!=afterPrimitive;
    for(const auto &pair:{rdcpair<ResourceId,uint32_t>(beforePrimitive,dispatches.front()),
                         rdcpair<ResourceId,uint32_t>(afterPrimitive,dispatches.back())})
    {
      bool expected=false,wrong=false;
      for(const auto &u:c->GetUsage(pair.first))if(u.usage==ResourceUsage::CS_Resource)
      {expected |= u.eventId==pair.second;wrong |= u.eventId==(pair.second==dispatches.front()?dispatches.back():dispatches.front());}
      ok &= expected && !wrong;
    }
  }
  // Namespaces have equal numeric GPU IDs in the actual fixture. Prove their
  // distinct ResourceIds, and the otherwise unbound BLAS, appear as read usage.
  for(const auto chunk : c->GetStructuredFile().chunks)
  {
    ResourceId resource;
    if(heapOnly && chunk->name=="MTLAccelerationStructure::CaptureGPUIdentity" && chunk->FindChild("value")->data.basic.u==capturedUnusedAS) continue;
    if(chunk->name == "MTLAccelerationStructure::CaptureGPUIdentity" ||
       chunk->name == "MTLFunctionTable::CaptureGPUIdentity")
      resource = chunk->FindChild("resource")->AsResourceId();
    if(chunk->name == "Internal::Initial Contents" && chunk->FindChild("kind") &&
       chunk->FindChild("kind")->data.basic.u == 1)
      resource = chunk->FindChild("id")->AsResourceId();
    if(inactive && !frameBuild && (!heapAS || heapOnly) && resource != selectedAS &&
       (chunk->name=="MTLAccelerationStructure::CaptureGPUIdentity" || chunk->name=="Internal::Initial Contents")) continue;
    if(resource != ResourceId())
    {
      bool read = false;
      const uint32_t expectedDispatch = newTarget && resource==beforePrimitive ? dispatches.front() : dispatches.back();
      for(const auto &u : c->GetUsage(resource)) read |= !dispatches.empty() && u.eventId == expectedDispatch && u.usage == ResourceUsage::CS_Resource;
      if(!read)
        for(const auto &r:c->GetResources())if(r.resourceId==resource)
          fprintf(stderr,"Missing AS/function-table read usage: chunk=%s resource=%s\n",chunk->name.c_str(),r.name.c_str());
      ok &= read;
    }
  }
  uint64_t capturedAS = 0, capturedSBT = 0;
  ResourceId sbt = resources["IR shader records"];
  for(const auto chunk : c->GetStructuredFile().chunks)
  {
    if(chunk->name == "MTLResource::CaptureGPUIdentity" && chunk->FindChild("resource")->AsResourceId() == sbt)
      capturedSBT = chunk->FindChild("value")->data.basic.u;
  }
  auto capturedHeader=capturedInitial[resources["IR AS header"]];
  if(capturedHeader.size()==64) memcpy(&capturedAS,capturedHeader.data(),8);
  c->SetFrameEvent(0,true);
  auto liveHeader = c->GetBufferData(resources["IR AS header"],0,8);
  auto livePacket = c->GetBufferData(packet,0,8);
  uint64_t liveAS = 0, liveSBT = 0;
  if(liveHeader.size() == 8) memcpy(&liveAS,liveHeader.data(),8);
  if(livePacket.size() == 8) memcpy(&liveSBT,livePacket.data(),8);
  ok &= capturedAS && capturedSBT && liveAS && liveSBT && liveAS != capturedAS && liveSBT != capturedSBT;
  printf("%s forced allocation-order relocation AS=%llu->%llu SBT=%llu->%llu\n",ok?"PASS":"FAIL",capturedAS,liveAS,capturedSBT,liveSBT);
  if(heapAS)
  {
    auto captured=capturedInitial[resources["IR heap AS header"]];
    auto current=c->GetBufferData(resources["IR heap AS header"],0,64);
    uint64_t oldID=0,newID=0;
    if(captured.size()==64) memcpy(&oldID,captured.data(),8);
    if(current.size()==64) memcpy(&newID,current.data(),8);
    ok &= oldID && newID && oldID!=capturedAS && newID!=liveAS && oldID!=newID;
    printf("%s heap-AS relocation %llu->%llu; distinct TLAS from unused fixture/direct GRS AS\n",ok?"PASS":"FAIL",oldID,newID);
  }
  for(const char *scope : {"local","global","heap"})
  {
    if(!strcmp(scope,"local") ? !local : !strcmp(scope,"global") ? !global : !heaps) continue;
    rdcstr prefix=rdcstr("IR ")+scope;
    uint64_t capturedTexture=0,capturedSampler=0,liveTexture=0,liveSampler=0;
    for(const auto chunk : c->GetStructuredFile().chunks)
      if(chunk->name == "MTLResource::CaptureGPUIdentity")
      {
        uint64_t kind=chunk->FindChild("kind")->data.basic.u;
        if(kind == 1 && chunk->FindChild("resource")->AsResourceId() == resources[prefix+" texture"]) capturedTexture=chunk->FindChild("value")->data.basic.u;
        if(kind == 2) capturedSampler=chunk->FindChild("value")->data.basic.u;
        if(kind == 1 || kind == 2)
        {
          bool used=false;
          for(const auto &u:c->GetUsage(chunk->FindChild("resource")->AsResourceId()))
            used |= u.eventId == dispatches.back() && u.usage == ResourceUsage::CS_Resource;
          ok &= used;
        }
      }
    auto textureTable=c->GetBufferData(resources[!strcmp(scope,"heap")?rdcstr("IR resource descriptor heap"):prefix+" texture table"],!strcmp(scope,"heap")?24:0,24);
    const bool six = !strcmp(scope,"global") || (!strcmp(scope,"local") && strstr(mode,"six-samplers"));
    auto samplerTable=c->GetBufferData(resources[!strcmp(scope,"heap")?rdcstr("IR sampler descriptor heap"):prefix+" static sampler table"],!strcmp(scope,"heap")?24:0,six?144:24);
    if(textureTable.size()==24) memcpy(&liveTexture,textureTable.data()+8,8);
    if(samplerTable.size()==(six?144:24)) memcpy(&liveSampler,samplerTable.data()+(six?72:0),8);
    if(six) for(unsigned entry=0;entry<6;entry++)
    {
      uint64_t words[3];memcpy(words,samplerTable.data()+entry*24,24);
      ok &= words[0] && !words[1] && !words[2];
    }
    ok &= capturedTexture && capturedSampler && liveTexture && liveSampler && liveTexture != capturedTexture;
    if(six) printf("%s %s resource translation texture=%llu->%llu six samplers bound, selected slot3=%llu\n",ok?"PASS":"FAIL",scope,capturedTexture,liveTexture,liveSampler);
    else printf("%s %s resource translation texture=%llu->%llu sampler=%llu->%llu\n",ok?"PASS":"FAIL",scope,
        capturedTexture,liveTexture,capturedSampler,liveSampler);
  }
  if(global)
  {
    auto roots=c->GetBufferData(resources["IR global root"],0,64);
    ok &= roots.size()==64;
    if(roots.size()==64)
    {
      uint64_t words[8];memcpy(words,roots.data(),64);
      unsigned constants[4];memcpy(constants,roots.data()+24,16);
      ok &= words[0] && words[1] && words[2] && words[5] && words[6] && words[7] &&
          constants[0]==200 && !constants[1] && !constants[2] && constants[3]==1;
    }
  }
  if(heaps)
  {
    auto entries=c->GetBufferData(resources["IR resource descriptor heap"],0,96);
    auto samplers=c->GetBufferData(resources["IR sampler descriptor heap"],0,48);
    ok &= entries.size()==96 && samplers.size()==48;
    if(entries.size()==96 && samplers.size()==48)
    {
      uint64_t words[12],swords[6];memcpy(words,entries.data(),96);memcpy(swords,samplers.data(),48);
      ok &= words[0] && !words[1] && words[2]==4 && !words[3] && words[4] && !words[5] &&
          (heapAS?words[6]!=0:words[6]==0) && !words[7] && !words[8] && words[9]==words[0]+8 && !words[10] && words[11]==4 &&
          !swords[0] && !swords[1] && !swords[2] && swords[3] && !swords[4] && !swords[5];
    }
  }
  bool written = false;
  for(const auto &u : c->GetUsage(output)) written |= !dispatches.empty() && u.eventId == dispatches.back() && u.usage == ResourceUsage::CS_RWResource;
  ok &= written;
  auto records = c->GetBufferData(resources["IR shader records"],0,recordStride*3);
  if(records.size() != recordStride*3) ok = false;
  else for(unsigned i = 0; i < 3; i++)
  {
    uint64_t words[4]; memcpy(words,records.data()+i*recordStride,32);
    ok &= words[0] == ((i == 2 && any) ? 4 : 0) &&
        words[1] == ((i == 1 && nullMiss) || (i == 2 && nullHit) ? 0 : i+1) &&
        ((local && i && (words[0] || words[1])) ? words[2] != 0 : words[2] == 0) && words[3] == pad;
    if(local && i)
    {
      uint64_t cbv, srv, table; unsigned constants[4];
      memcpy(&cbv,records.data()+i*recordStride+32,8);
      memcpy(constants,records.data()+i*recordStride+40,16);
      memcpy(&srv,records.data()+i*recordStride+56,8);memcpy(&table,records.data()+i*recordStride+64,8);
      if(words[0] || words[1]) ok &= cbv && srv && table && constants[0] == (i==2 ? 20 : 30) &&
          !constants[1] && !constants[2] && constants[3] == 1;
      else ok &= !cbv && !srv && !table && !constants[0] && !constants[1] && !constants[2] && !constants[3];
      for(unsigned at=72;at<96;at++) ok &= records[i*recordStride+at] == 0;
    }
  }
  const unsigned initialHit = any?baseMiss:nullHit?7:(local?131:73)+(indirect?73:0);
  const unsigned beforeHit=(inactive?baseMiss:heapAS?baseMiss:initialHit)+(global?1211:0)+(heaps?23:0);
  const unsigned beforeMiss=(inactive?baseMiss:heapAS?initialHit:baseMiss)+(global?1211:0)+(heaps?37:0);
  if(frameBuild)
  {
    ok &= beforeOutput!=ResourceId() && beforePacket!=ResourceId();
    for(const auto &name:{"IR before global root","IR before dispatch packet"})
    {
      bool read=false;for(const auto &u:c->GetUsage(resources[name]))
        read |= u.eventId==dispatches.front() && u.usage==ResourceUsage::CS_Resource;
      ok &= read;
    }
    bool written=false;for(const auto &u:c->GetUsage(beforeOutput))
      written |= u.eventId==dispatches.front() && u.usage==ResourceUsage::CS_RWResource;
    ok &= written;
  }
  auto scratchGuard = [&]() {
    if(!strstr(mode,"scratch-offset") || strstr(mode,"scratch-zero-control"))return true;
    auto guard=c->GetBufferData(resources["IR scratch guard readback"],0,256);
    if(guard.size()!=256)return false;
    for(byte value:guard)if(value!=0xa5)return false;
    return true;
  };
  for(unsigned cycle = 0; cycle < 3 && ok; cycle++) for(size_t i = 0; i < events.size() && ok; i++)
  {
    const auto &e = events[cycle == 1 ? events.size()-1-i : i];
    c->SetFrameEvent(0,true); auto reset = c->GetBufferData(output,0,8); unsigned values[2] = {};
    ok &= c->GetFatalErrorStatus().OK() && reset.size() == 8 && scratchGuard();
    if(reset.size() == 8) { memcpy(values,reset.data(),8); ok &= values[0] == 7 && values[1] == 7; }
    if(frameBuild)
    {
      auto before=c->GetBufferData(beforeOutput,0,8);unsigned data[2]={};
      if(before.size()==8)memcpy(data,before.data(),8);
      ok &= before.size()==8 && data[0]==7 && data[1]==7;
    }
    c->SetFrameEvent(e.eventId,true); auto bytes = c->GetBufferData(output,0,8);
    ok &= c->GetFatalErrorStatus().OK() && bytes.size() == 8 && scratchGuard();
    const bool gpu = c->GetStructuredFile().chunks[e.chunkIndex]->name.beginsWith("MTLComputeCommandEncoder::");
    bool after = gpu ? e.eventId >= dispatches.back() : e.fileOffset >= dispatchOffset;
    if(bytes.size() == 8)
    {
      memcpy(values,bytes.data(),8); ok &= values[0] == (after?wantHit:7) && values[1] == (after?wantMiss:7);
      if(!ok) fprintf(stderr,"EID %u values=%u/%u expected=%u/%u\n",e.eventId,values[0],values[1],after?wantHit:7,after?wantMiss:7);
    }
    if(frameBuild)
    {
      auto before=c->GetBufferData(beforeOutput,0,8);unsigned data[2]={};
      if(before.size()==8)memcpy(data,before.data(),8);
      const bool beforeRan=gpu?e.eventId>=dispatches.front():e.fileOffset>=beforeOffset;
      ok &= before.size()==8 && data[0]==(beforeRan?beforeHit:7) && data[1]==(beforeRan?beforeMiss:7);
      if(!ok)fprintf(stderr,"Before-output EID%u got%u/%u expected%u/%u\n",e.eventId,data[0],data[1],beforeRan?beforeHit:7,beforeRan?beforeMiss:7);
    }
    if(e.eventId == dispatches.back() || (frameBuild && e.eventId==dispatches.front()))
    {
      auto state = c->GetPipelineState().GetMetalPipelineState();
      ok &= state && state->computeBuffers.size() > 3 && state->computeBuffers[3].resourceId == (frameBuild && e.eventId==dispatches.front()?beforePacket:packet) && state->computeBuffers[3].byteOffset == 0;
    }
    printf("%s cycle %u EID %u IR TraceRay\n",ok?"PASS":"FAIL",cycle,e.eventId);
  }
  if(strstr(mode,"scratch-zero-control"))
  {
    c->SetFrameEvent(events.back().eventId,true);
    auto guard=c->GetBufferData(resources["IR scratch guard readback"],0,256);bool changed=false;
    for(byte value:guard)changed |= value!=0xa5;
    ok &= guard.size()==256 && changed && c->GetFatalErrorStatus().OK();
    if(!changed)fprintf(stderr,"Zero scratch offset control did not use the former prefix guard\n");
  }
  c->Shutdown(); RENDERDOC_ShutdownReplay();
  printf("%s IR TraceRay %u/%u mode=%s, SBT indices/padding/usage/binding, %zu events in three directions/EID0\n",ok?"PASS":"FAIL",wantHit,wantMiss,mode,events.size());
  return ok?0:3;
}
