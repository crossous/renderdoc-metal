// SPDX-License-Identifier: MIT
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
REPLAY_PROGRAM_MARKER()
static rdcarray<APIEvent> events;
static rdcarray<uint32_t> dispatches;
static void Walk(const rdcarray<ActionDescription> &actions)
{for(const auto &a:actions){events.append(a.events);if(a.flags&ActionFlags::Dispatch)dispatches.push_back(a.eventId);Walk(a.children);}}
int main(int argc,char **argv)
{
  if(argc!=2)return 1;
  GlobalEnvironment env;env.enumerateGPUs=false;RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile();auto result=file->OpenFile(argv[1],"rdc",nullptr);IReplayController *c=nullptr;
  if(result.OK())rdctie(result,c)=file->OpenCapture(ReplayOptions(),nullptr);
  file->Shutdown();if(!result.OK()||!c){fprintf(stderr,"Cannot open ray argument capture\n");return 2;}
  Walk(c->GetRootActions());std::sort(events.begin(),events.end(),[](const APIEvent&a,const APIEvent&b){return a.eventId<b.eventId;});
  ResourceId output,packet;
  for(const auto &r:c->GetResources()){
    if(r.name=="Ray argument packet results")output=r.resourceId;
    if(r.name=="Ray AS IFT VFT argument packets")packet=r.resourceId;
  }
  std::map<uint64_t,rdcarray<ResourceId>> fields;uint64_t selected=0;
  for(const auto chunk:c->GetStructuredFile().chunks){
    if(chunk->name=="MTLArgumentEncoder::setArgumentBuffer")selected=chunk->FindChild("offset")->data.basic.u;
    if(chunk->name=="MTLArgumentEncoder::setAccelerationStructure"||chunk->name=="MTLArgumentEncoder::setIntersectionFunctionTable"||chunk->name=="MTLArgumentEncoder::setVisibleFunctionTable"){
      auto resource=chunk->FindChild(chunk->name=="MTLArgumentEncoder::setVisibleFunctionTable"?"table":"resource")->AsResourceId();
      auto index=chunk->FindChild("index")->data.basic.u;fields[selected].resize_for_index(index);fields[selected][index]=resource;
    }
  }
  bool ok=output!=ResourceId()&&packet!=ResourceId()&&dispatches.size()==4&&fields.size()==5;
  uint64_t offsets[4]={},packetOffsets[4]={},lastPacketOffset=0;unsigned next=0;
  for(const auto &e:events){
    auto chunk=c->GetStructuredFile().chunks[e.chunkIndex];
    if(chunk->name=="MTLComputeCommandEncoder::setBuffer"&&chunk->FindChild("index")->data.basic.u==0){
      ok &= chunk->FindChild("buffer")->AsResourceId()==packet;lastPacketOffset=chunk->FindChild("offset")->data.basic.u;
    }
    if(next<4&&e.eventId==dispatches[next]){offsets[next]=e.fileOffset;packetOffsets[next]=lastPacketOffset;next++;}
  }
  for(const auto &entry:fields)ok&=entry.second.size()==3;
  if(ok){
    auto nil=fields.rbegin()->second;for(auto resource:nil)ok&=resource==ResourceId();
    for(unsigned s=0;s<4;s++)for(auto resource:fields[packetOffsets[s]]){
      bool found=false;for(const auto &u:c->GetUsage(resource))found|=u.eventId==dispatches[s]&&u.usage==ResourceUsage::CS_Resource;
      ok&=found;
    }
  }
  const unsigned expected[]={3,0,0,9};
  for(unsigned cycle=0;cycle<3&&ok;cycle++)for(size_t i=0;i<events.size()&&ok;i++){
    const auto &e=events[cycle==1?events.size()-1-i:i];c->SetFrameEvent(0,true);
    auto reset=c->GetBufferData(output,0,16);ok&=reset.size()==16;
    if(reset.size()==16)for(unsigned s=0;s<4;s++){unsigned value;memcpy(&value,reset.data()+s*4,4);ok&=value==7;}
    // Restore resource fields while preserving unrelated bytes before the packets.
    auto prefix=c->GetBufferData(packet,0,fields.begin()->first);for(auto value:prefix)ok&=value==0xa5;
    ok&=prefix.size()==fields.begin()->first;
    c->SetFrameEvent(e.eventId,true);ok&=c->GetFatalErrorStatus().OK();
    auto bytes=c->GetBufferData(output,0,16);ok&=bytes.size()==16;
    const bool gpu=c->GetStructuredFile().chunks[e.chunkIndex]->name.beginsWith("MTLComputeCommandEncoder::");
    if(bytes.size()==16)for(unsigned s=0;s<4;s++){
      unsigned value;memcpy(&value,bytes.data()+s*4,4);const unsigned want=(gpu?e.eventId>=dispatches[s]:e.fileOffset>=offsets[s])?expected[s]:7;
      if(value!=want)fprintf(stderr,"EID %u result %u=%u expected %u\n",e.eventId,s,value,want);ok&=value==want;
      if(e.eventId==dispatches[s]){
        auto state=c->GetPipelineState().GetMetalPipelineState();ok&=state&&state->computeBuffers.size()>0&&
          state->computeBuffers[0].resourceId==packet&&state->computeBuffers[0].byteOffset==packetOffsets[s];
      }
    }
    printf("%s cycle %u EID %u typed AS/IFT/VFT packet\n",ok?"PASS":"FAIL",cycle,e.eventId);
  }
  c->Shutdown();RENDERDOC_ShutdownReplay();
  printf("%s ray packets, typed usage/null/padding, %zu events in three directions/EID0\n",ok?"PASS":"FAIL",events.size());return ok?0:3;
}
