// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()

static rdcarray<APIEvent> events;
static rdcarray<uint32_t> dispatches;
static unsigned refits=0;
static bool Walk(const rdcarray<ActionDescription> &actions)
{
  bool ok=true;
  for(const auto &a:actions)
  {
    events.append(a.events);
    if(a.flags & ActionFlags::Dispatch) dispatches.push_back(a.eventId);
    if(a.flags & ActionFlags::BuildAccStruct)
    {refits++; ok &= a.customName.beginsWith("Refit Metal");}
    ok &= !a.IsFakeMarker() && Walk(a.children);
  }
  return ok;
}
int main(int argc,char **argv)
{
  if(argc!=3) return 1;
  const bool initialRefit=strstr(argv[2],"initial-refit");
  const bool separate=strstr(argv[2],"separate");
  GlobalEnvironment env;env.enumerateGPUs=false;
  RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);
  IReplayController *c=nullptr;if(result.OK()) rdctie(result,c)=file->OpenCapture(ReplayOptions(),nullptr);
  file->Shutdown();if(!result.OK() || !c) return 2;
  bool ok=Walk(c->GetRootActions()) && dispatches.size()==3 && refits==1;
  ResourceId output;for(auto r:c->GetResources())if(r.name=="Refit ray results")output=r.resourceId;
  ok &= output!=ResourceId();
  unsigned initials=0;rdcarray<ResourceId> bindings;
  for(auto chunk:c->GetStructuredFile().chunks)
  {
    if(chunk->name=="Internal::Initial Contents" && chunk->FindChild("parameters"))
    {initials++;ok &= chunk->FindChild("parameters")->GetChild(7)->AsUInt64()==1;}
    if(chunk->name=="MTLComputeCommandEncoder::setAccelerationStructure")bindings.push_back(chunk->FindChild("structure")->AsResourceId());
  }
  ok &= initials==1 && bindings.size()==3;
  if(bindings.size()==3) ok &= (separate?bindings[0]!=bindings[1]:bindings[0]==bindings[1]) && bindings[1]==bindings[2];
  uint64_t offsets[3]={};
  for(auto e:events)for(unsigned slot=0;slot<3;slot++)if(e.eventId==dispatches[slot])offsets[slot]=e.fileOffset;
  const unsigned expected[3]={initialRefit?0U:1U,initialRefit?1U:0U,initialRefit?1U:0U};
  std::set<uint32_t> ids;for(auto e:events) ok &= ids.insert(e.eventId).second;
  rdcarray<uint32_t> selections;for(uint32_t id:ids)selections.push_back(id);
  for(unsigned cycle=0;cycle<3 && ok;cycle++)for(size_t i=0;i<selections.size() && ok;i++)
  {
    auto eid=selections[cycle==1?selections.size()-1-i:i];const APIEvent *selected=nullptr;
    for(const auto &e:events)if(e.eventId==eid)selected=&e;
    const auto name=c->GetStructuredFile().chunks[selected->chunkIndex]->name;
    const bool gpu=name.beginsWith("MTLComputeCommandEncoder::") || name.beginsWith("MTLAccelerationStructureCommandEncoder::");
    c->SetFrameEvent(0,true);c->SetFrameEvent(eid,true);
    if(!c->GetFatalErrorStatus().OK())fprintf(stderr,"Fatal at EID %u %s: %s\n",eid,name.c_str(),c->GetFatalErrorStatus().internal_msg?c->GetFatalErrorStatus().internal_msg->c_str():"unknown");
    ok &= c->GetFatalErrorStatus().OK();
    auto bytes=c->GetBufferData(output,0,12);ok &= bytes.size()==12;
    if(bytes.size()==12)for(unsigned slot=0;slot<3;slot++)
    {
      unsigned v=0;memcpy(&v,bytes.data()+4*slot,4);
      const unsigned want=(gpu?eid>=dispatches[slot]:selected->fileOffset>=offsets[slot])?expected[slot]:7;
      if(v!=want)fprintf(stderr,"EID %u slot %u got %u expected %u\n",eid,slot,v,want);
      ok &= v==want;
    }
    for(unsigned slot=0;slot<3;slot++)if(eid==dispatches[slot])
    {auto state=c->GetPipelineState().GetMetalPipelineState();ok &= state && state->computeAccelerationStructures.size()>0 && state->computeAccelerationStructures[0]==bindings[slot];
      if(state && state->computeAccelerationStructures.size()>0 && state->computeAccelerationStructures[0]!=bindings[slot])fprintf(stderr,"EID %u AS binding mismatch\n",eid);}
    printf("%s cycle %u EID %u refit ray prefix\n",ok?"PASS":"FAIL",cycle,eid);
  }
  c->Shutdown();RENDERDOC_ShutdownReplay();
  printf("%s %s, %zu events in three directions\n",ok?"PASS":"FAIL",argv[2],selections.size());return ok?0:3;
}
