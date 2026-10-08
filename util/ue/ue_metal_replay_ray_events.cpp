// SPDX-License-Identifier: MIT
// Test-only: locate bound query PSOs from a separately audited AIR inventory.
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <algorithm>
#include <string>
REPLAY_PROGRAM_MARKER()

static uint64_t Number(ResourceId id)
{ uint64_t value=0; static_assert(sizeof(id)==sizeof(value), "ResourceId representation"); memcpy(&value,&id,sizeof(value)); return value; }
struct Target { ResourceId pipeline,function,arguments; uint64_t offset=0; uint32_t rootSlot=0,rootBytes=0; };
struct Event { uint32_t id; Target target; };
// Optional test-only readback of displayed writable candidates. This is not
// shader feedback: absence here cannot certify that a resource was untouched.
static const char *readbackFolder = nullptr;
static uint32_t readbackSelection = 0;
static bool SaveWritableCandidates(IReplayController *controller, uint32_t event,
                                   const rdcarray<DescriptorAccess> &accesses)
{
  if(!readbackFolder) return true;
  const uint32_t selection = readbackSelection++;
  uint64_t total = 0;
  std::map<ResourceId, bool> seen;
  const auto textures = controller->GetTextures();
  const auto buffers = controller->GetBuffers();
  for(const auto &access : accesses)
  {
    if(access.staticallyUnused || !IsReadWriteDescriptor(access.type)) continue;
    const auto descriptors = controller->GetDescriptors(access.descriptorStore, {DescriptorRange(access)});
    for(const auto &descriptor : descriptors)
    {
      if(descriptor.resource == ResourceId() || seen.count(descriptor.resource)) continue;
      seen[descriptor.resource] = true;
      bytebuf bytes;
      const char *kind = nullptr;
      for(const auto &texture : textures)
        if(texture.resourceId == descriptor.resource && texture.byteSize <= 8 * 1024 * 1024)
        {
          bytes = controller->GetTextureData(descriptor.resource,
                                             {descriptor.firstMip, descriptor.firstSlice, 0});
          kind = "texture";
          break;
        }
      if(!kind)
        for(const auto &buffer : buffers)
          if(buffer.resourceId == descriptor.resource && buffer.length <= 8 * 1024 * 1024)
          {
            bytes = controller->GetBufferData(descriptor.resource, 0, buffer.length);
            kind = "buffer";
            break;
          }
      if(!kind) continue;
      // Diagnostic readbacks have their own bounded budget, not a replay
      // eligibility limit. Only the requested subresource is observed.
      if(bytes.empty() || bytes.size() > 8 * 1024 * 1024 || total + bytes.size() > 32 * 1024 * 1024)
        return false;
      total += bytes.size();
      char name[4096];
      snprintf(name, sizeof(name), "%s/selection-%u-event-%u-%s-%llu.bin", readbackFolder,
               selection, event, kind, (unsigned long long)Number(descriptor.resource));
      FILE *output = fopen(name, "wb");
      if(!output) return false;
      bool ok = fwrite(bytes.data(), 1, bytes.size(), output) == bytes.size();
      fclose(output);
      if(!ok) return false;
      printf("query-candidate-readback selection=%u event=%u resource=%llu kind=%s bytes=%zu\n",
             selection, event, (unsigned long long)Number(descriptor.resource), kind, bytes.size());
    }
  }
  return controller->GetFatalErrorStatus().OK();
}
static void Collect(const rdcarray<ActionDescription> &actions,const std::map<uint32_t,Target> &chunks,rdcarray<Event> &events)
{
  for(const auto &a:actions)
  {
    if(!a.IsFakeMarker() && (a.flags&ActionFlags::Dispatch) && a.dispatchDimension[0] && a.dispatchDimension[1] && a.dispatchDimension[2])
      for(const auto &e:a.events)
      {
        auto target=chunks.find(e.chunkIndex);
        if(e.eventId==a.eventId && target!=chunks.end()) events.push_back({a.eventId,target->second});
      }
    Collect(a.children,chunks,events);
  }
}
static bool Snapshot(IReplayController *controller,const Event &event,rdcstr &snapshot)
{
  controller->SetFrameEvent(event.id,true);
  if(!controller->GetFatalErrorStatus().OK()) return false;
  const auto *state=controller->GetPipelineState().GetMetalPipelineState();
  if(!state || state->computePipelineResourceId!=event.target.pipeline ||
     state->computeShader.resourceId!=event.target.function || state->computeBuffers.empty() ||
     state->computeBuffers[0].resourceId==ResourceId()) return false;
  if(state->computeBuffers.size()<=event.target.rootSlot ||
     state->computeBuffers[event.target.rootSlot].byteSize!=event.target.rootBytes) return false;
  if(event.target.arguments!=ResourceId() && (state->indirectBuffer.resourceId!=event.target.arguments ||
     state->indirectBuffer.byteOffset!=event.target.offset || state->indirectBuffer.byteSize!=12)) return false;
  snapshot=rdcstr(std::to_string(Number(state->computePipelineResourceId)).c_str());
  for(const auto &binding:state->computeBuffers)
  {
    snapshot += rdcstr(("|"+std::to_string(Number(binding.resourceId))+":"+std::to_string(binding.byteOffset)+":"+std::to_string(binding.byteSize)).c_str());
  }
  snapshot += rdcstr(("|indirect:"+std::to_string(Number(state->indirectBuffer.resourceId))+":"+std::to_string(state->indirectBuffer.byteOffset)).c_str());
  // Access display may be partial. It must not invalidate restored Native state.
  const auto access=controller->GetDescriptorAccess();
  if(!SaveWritableCandidates(controller, event.id, access)) return false;
  printf("query-event %u pipeline=%llu function=%llu indirect=%llu+%llu accessEntries=%zu\n",event.id,
      (unsigned long long)Number(state->computePipelineResourceId),(unsigned long long)Number(state->computeShader.resourceId),
      (unsigned long long)Number(state->indirectBuffer.resourceId),(unsigned long long)state->indirectBuffer.byteOffset,access.size());
  return controller->GetFatalErrorStatus().OK();
}
int main(int argc,char **argv)
{
  if(argc!=3 && argc!=4) return 2;
  if(argc==4) readbackFolder=argv[3];
  // Numeric IDs only locate test events; they never change backend eligibility.
  struct Expected { uint64_t function; uint32_t rootSlot,rootBytes; };
  std::map<uint64_t,Expected> wanted;
  FILE *input=fopen(argv[2],"r");if(!input)return 2;
  unsigned long long pipeline,function;unsigned slot,root;
  while(fscanf(input,"%llu %llu %u %u",&pipeline,&function,&slot,&root)==4)wanted[pipeline]={function,slot,root};
  fclose(input);if(wanted.empty())return 2;
  GlobalEnvironment environment;environment.enumerateGPUs=false;
  RENDERDOC_InitialiseReplay(environment,{argv[0]});auto file=RENDERDOC_OpenCaptureFile();
  auto result=file->OpenFile(argv[1],"rdc",nullptr);if(!result.OK())return 3;
  IReplayController *controller=nullptr;rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);file->Shutdown();
  if(!result.OK() || !controller)return 4;
  std::map<ResourceId,ResourceId> bound;std::map<uint32_t,Target> targets;
  const auto &chunks=controller->GetStructuredFile().chunks;
  for(uint32_t i=0;i<chunks.size();i++)
  {
    const auto *chunk=chunks[i];const auto *encoder=chunk->FindChild("ComputeCommandEncoder");
    if(!encoder)continue;ResourceId id=encoder->AsResourceId();
    if(chunk->name=="MTLComputeCommandEncoder::setComputePipelineState")bound[id]=chunk->FindChild("pipeline")->AsResourceId();
    if(chunk->name=="MTLComputeCommandEncoder::dispatchThreadgroups" || chunk->name=="MTLComputeCommandEncoder::dispatchThreadgroups(indirect)")
    {
      auto w=wanted.find(Number(bound[id]));if(w==wanted.end())continue;
      Target target;target.pipeline=bound[id];const uint64_t functionID=w->second.function;memcpy(&target.function,&functionID,8);target.rootSlot=w->second.rootSlot;target.rootBytes=w->second.rootBytes;
      const auto *buffer=chunk->FindChild("indirectBuffer");
      if(buffer){target.arguments=buffer->AsResourceId();target.offset=chunk->FindChild("indirectBufferOffset")->AsUInt64();}
      targets[i]=target;
    }
  }
  rdcarray<Event> events;Collect(controller->GetRootActions(),targets,events);
  printf("AIR-audited query targets=%zu nonzero actions=%zu\n",wanted.size(),events.size());
  if(events.empty())return 5;
  std::map<uint32_t,rdcstr> snapshots;
  for(const auto &event:events){rdcstr data;if(!Snapshot(controller,event,data))return 6;snapshots[event.id]=data;}
  for(bool reverse:{true,false})
  {
    controller->SetFrameEvent(0,true);if(!controller->GetFatalErrorStatus().OK())return 7;
    if(reverse)std::reverse(events.begin(),events.end());
    for(const auto &event:events)
    {
      controller->SetFrameEvent(event.id-1,true);if(!controller->GetFatalErrorStatus().OK())return 8;
      rdcstr data;if(!Snapshot(controller,event,data) || data!=snapshots[event.id])return 9;
    }
    if(reverse)std::reverse(events.begin(),events.end());
  }
  controller->Shutdown();RENDERDOC_ShutdownReplay();
  puts("PASS AIR-audited Native query events: forward/reverse/before/after/EID0, original PSO/function and indirect/root buffer identity");return 0;
}
