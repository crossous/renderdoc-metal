// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
REPLAY_PROGRAM_MARKER()
static rdcarray<APIEvent> events;
static rdcarray<uint32_t> dispatches;
static void Walk(const rdcarray<ActionDescription> &actions)
{
  for(const auto &a:actions){events.append(a.events);if(a.flags&ActionFlags::Dispatch)dispatches.push_back(a.eventId);Walk(a.children);}
}
int main(int argc,char **argv)
{
  if(argc!=2)return 1;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
  IReplayController *c=nullptr;if(result.OK())rdctie(result,c)=file->OpenCapture(ReplayOptions(),nullptr);
  file->Shutdown();if(!result.OK()||!c)return 2;
  Walk(c->GetRootActions());std::sort(events.begin(),events.end(),[](const APIEvent &a,const APIEvent &b){return a.eventId<b.eventId;});
  ResourceId output,structure;for(const auto &r:c->GetResources()){
    if(r.name=="Six compile API ray results")output=r.resourceId;
    if(r.name=="Compile API triangle AS")structure=r.resourceId;
  }
  bool ok=output!=ResourceId()&&dispatches.size()==6;
  // Two legacy synchronous APIs share a display name. Chunk IDs identify APIs.
  std::set<uint32_t> creationAPIs;std::set<ResourceId> pipelines;
  for(const auto chunk:c->GetStructuredFile().chunks)
  {
    if(chunk->name=="MTLComputeCommandEncoder::setAccelerationStructure")
    {
      auto bound=chunk->FindChild("structure")->AsResourceId();
      ok &= structure==ResourceId()||structure==bound;
      structure=bound;
    }
    if(chunk->name.beginsWith("MTLDevice::newComputePipelineStateWith")){
      ok &= creationAPIs.insert(chunk->metadata.chunkID).second;
      ok &= pipelines.insert(chunk->FindChild("ComputePipelineState")->AsResourceId()).second;
    }
  }
  ok &= structure!=ResourceId()&&creationAPIs.size()==6&&pipelines.size()==6;
  uint64_t offsets[6]={};ResourceId boundPipelines[6],lastPipeline;
  unsigned slot=0;
  for(const auto &e:events){
    const auto chunk=c->GetStructuredFile().chunks[e.chunkIndex];
    if(chunk->name=="MTLComputeCommandEncoder::setComputePipelineState")lastPipeline=chunk->FindChild("pipeline")->AsResourceId();
    if(slot<6&&e.eventId==dispatches[slot]){offsets[slot]=e.fileOffset;boundPipelines[slot]=lastPipeline;ok&=pipelines.count(lastPipeline)>0;slot++;}
  }
  for(unsigned cycle=0;cycle<3&&ok;cycle++)for(size_t i=0;i<events.size()&&ok;i++){
    const auto &e=events[cycle==1?events.size()-1-i:i];
    c->SetFrameEvent(0,true);
    auto reset=c->GetBufferData(output,0,48);ok&=reset.size()==48;
    if(reset.size()==48)for(unsigned s=0;s<12;s++){unsigned value;memcpy(&value,reset.data()+s*4,4);ok&=value==7;}
    c->SetFrameEvent(e.eventId,true);ok&=c->GetFatalErrorStatus().OK();
    auto bytes=c->GetBufferData(output,0,48);ok&=bytes.size()==48;
    const bool gpu=c->GetStructuredFile().chunks[e.chunkIndex]->name.beginsWith("MTLComputeCommandEncoder::");
    if(bytes.size()==48)for(unsigned s=0;s<6;s++){
      const bool executed=gpu?e.eventId>=dispatches[s]:e.fileOffset>=offsets[s];
      for(unsigned ray=0;ray<2;ray++){unsigned value;memcpy(&value,bytes.data()+(s*2+ray)*4,4);
        const unsigned want=executed?(ray?0:1):7;
        if(value!=want)fprintf(stderr,"EID %u API %u ray %u got %u expected %u\n",e.eventId,s,ray,value,want);
        ok&=value==want;
      }
      if(e.eventId==dispatches[s]){
        auto state=c->GetPipelineState().GetMetalPipelineState();
        ok&=state&&state->computePipelineResourceId==boundPipelines[s]&&
            state->computeAccelerationStructures.size()>0&&state->computeAccelerationStructures[0]==structure;
      }
    }
    printf("%s cycle %u EID %u six API ray prefix\n",ok?"PASS":"FAIL",cycle,e.eventId);
  }
  c->Shutdown();RENDERDOC_ShutdownReplay();
  printf("%s six creation APIs/six real ray dispatches, %zu events in three directions/EID0\n",ok?"PASS":"FAIL",events.size());
  return ok?0:3;
}
