// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()

static void Flatten(const rdcarray<ActionDescription> &actions,
                    std::vector<const ActionDescription *> &flat)
{
  for(const auto &a : actions) { flat.push_back(&a); Flatten(a.children,flat); }
}
static ResourceId Field(const SDChunk *chunk, const char *name)
{
  auto f = chunk ? chunk->FindChild(name) : nullptr;
  return f ? f->AsResourceId() : ResourceId();
}
int main(int argc, char **argv)
{
  if(argc!=2 && argc!=3) return 2;
  setvbuf(stdout,nullptr,_IOLBF,0);
  GlobalEnvironment env; env.enumerateGPUs=false;
  RENDERDOC_InitialiseReplay(env,{argv[0]});
  auto file=RENDERDOC_OpenCaptureFile(); auto result=file->OpenFile(argv[1],"rdc",nullptr);
  if(!result.OK()) return 3;
  IReplayController *r=nullptr;
  rdctie(result,r)=file->OpenCapture(ReplayOptions(),nullptr); file->Shutdown();
  if(!result.OK() || !r) return 4;
  const SDFile &sd=r->GetStructuredFile();
  // Independent oracle: serialized identities, not mutable replay encoder state
  // or adjacency of actions. A parallel child factory carries both identities.
  std::map<ResourceId,ResourceId> roots;
  for(const SDChunk *chunk : sd.chunks)
  {
    auto child=Field(chunk,"RenderCommandEncoder"), parent=Field(chunk,"ParallelRenderCommandEncoder");
    if(child!=ResourceId() && parent!=ResourceId()) roots[child]=parent;
  }
  std::vector<const ActionDescription *> actions;
  Flatten(r->GetRootActions(),actions);
  std::sort(actions.begin(),actions.end(),[](auto a,auto b){return a->eventId<b->eventId;});
  auto owner=[&](const ActionDescription &a) {
    if(a.events.empty() || a.events.back().chunkIndex>=sd.chunks.size()) return ResourceId();
    const auto chunk=sd.chunks[a.events.back().chunkIndex];
    auto child=Field(chunk,"RenderCommandEncoder");
    if(child!=ResourceId()) return roots.count(child) ? roots[child] : child;
    return Field(chunk,"ParallelRenderCommandEncoder");
  };
  auto output=r->CreateOutput(CreateHeadlessWindowingData(16,16),ReplayOutputType::Texture);
  const uint32_t selected=argc==3 ? strtoul(argv[2],nullptr,10) : 0;
  unsigned checks=0;
  for(const auto a : actions)
  {
    if(a->IsFakeMarker() || (selected && a->eventId!=selected)) continue;
    printf("CHECK EID=%u events=",a->eventId);
    auto pass=owner(*a);
    if(pass!=ResourceId() && !(a->flags & ActionFlags::EndPass))
      for(const auto earlier : actions)
      {
        if(earlier->eventId>=a->eventId) break;
        if(!earlier->IsFakeMarker() && owner(*earlier)==pass &&
           (earlier->flags & (ActionFlags::Drawcall | ActionFlags::MeshDispatch | ActionFlags::PassBoundary)))
          printf("%u,",earlier->eventId);
      }
    printf("\n");
    r->SetFrameEvent(a->eventId,true);
    if(!r->GetFatalErrorStatus().OK()) return 5;
    checks++;
  }
  if(!checks) return 6;
  if(!selected)
    for(const auto &res : r->GetResources())
      if(res.name=="Interleaved pass shared target")
      {
        auto bytes=r->GetTextureData(res.resourceId,{0,0,0});
        if(bytes.size()!=16*16*4) return 10;
        for(unsigned y=0;y<16;y++) for(unsigned x=0;x<16;x++)
        {
          auto p=bytes.data()+(y*16+x)*4;
          if(p[0]!=(x<8?255:0) || p[1]!=(x<8?0:255) || p[2]!=0 || p[3]!=255) return 11;
        }
        puts("PASS replay final shared target matches Native left red/right green");
      }
  if(selected)
  {
    std::map<ResourceId,bytebuf> originals;
    for(uint64_t n : {12323ULL,12331ULL,12397ULL})
    {
      ResourceId id; memcpy(&id,&n,sizeof(id));
      originals[id]=r->GetTextureData(id,{0,0,0});
      if(originals[id].empty()) return 7;
    }
    for(unsigned cycle=0;cycle<2;cycle++)
    {
      r->SetFrameEvent(0,true); r->SetFrameEvent(selected,true);
      if(!r->GetFatalErrorStatus().OK()) return 8;
      for(const auto &original : originals)
        if(r->GetTextureData(original.first,{0,0,0})!=original.second) return 9;
    }
    puts("PASS same UE EID and two EID0 reset cycles: GBufferA/C/depth bytes unchanged");
  }
  output->Shutdown(); r->Shutdown(); RENDERDOC_ShutdownReplay();
  printf("PASS pass-event public output queries and seeks: %u checks\n",checks);
  return 0;
}
