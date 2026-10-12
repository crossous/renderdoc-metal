// SPDX-License-Identifier: MIT
// Time normal API loading separately from output readback and event navigation.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <memory>
#if defined(RENDERDOC_PLATFORM_APPLE)
#include <objc/message.h>
#include <objc/runtime.h>
#endif
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
// A plain C++ command-line client has no application event-loop autorelease
// pool. Drain completed Native command buffers only after controller shutdown;
// each reopened device still performs all normal restoration checks.
struct NativeAutoreleaseScope
{
#if defined(RENDERDOC_PLATFORM_APPLE)
  id pool = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSAutoreleasePool"),
                                          sel_registerName("new"));
  ~NativeAutoreleaseScope()
  { ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("drain")); }
#endif
};
using Clock = std::chrono::steady_clock;
static Clock::time_point origin;
static unsigned cycle=0;
static double Now() { return std::chrono::duration<double>(Clock::now()-origin).count(); }
static void Stamp(const char *phase, const char *edge)
{ printf("{\"phase\":\"%s\",\"edge\":\"%s\",\"cycle\":%u,\"seconds\":%.9f}\n",phase,edge,cycle,Now()); fflush(stdout); }
static void Actions(const rdcarray<ActionDescription> &actions,uint32_t &last,rdcarray<ResourceId> &presented)
{
  for(const auto &a:actions)
  {
    if(!a.IsFakeMarker())last=std::max(last,a.eventId);
    if((a.flags & ActionFlags::Present) && !presented.contains(a.copyDestination))presented.push_back(a.copyDestination);
    Actions(a.children,last,presented);
  }
}
int main(int argc,char **argv)
{
  if(argc<3 || argc>6)return 2;
  const unsigned reopens=argc>=5?unsigned(strtoul(argv[4],nullptr,10)):0;
  // Repeated complete EID0 -> final-event observations distinguish first-load
  // state from a changing result on every reset. This is a diagnostic only;
  // no intermediate event readbacks or waits are inserted into the workload.
  const unsigned resets=argc==6?unsigned(strtoul(argv[5],nullptr,10)):1;
  if(reopens>8 || resets==0 || resets>8)return 2;
  // Read the completed output again without seeking/re-executing. This separates
  // a changing readback target/timing from a change caused by another frame replay.
  const bool repeatReadback = getenv("RENDERDOC_METAL_PROBE_REPEAT_READBACK") != nullptr;
  NativeAutoreleaseScope processPool;
  origin=Clock::now();Dl_info library={};dladdr((void *)&RENDERDOC_InitialiseReplay,&library);
  printf("Loaded backend: %s\n",library.dli_fname?library.dli_fname:"unknown");
  GlobalEnvironment env;env.enumerateGPUs=false;rdcarray<rdcstr> args;args.push_back(argv[0]);
  RENDERDOC_InitialiseReplay(env,args);RENDERDOC_SetDebugLogFile(rdcstr(argv[2])+"/renderdoc.log");
  std::unique_ptr<NativeAutoreleaseScope> devicePool(new NativeAutoreleaseScope);
  auto file=RENDERDOC_OpenCaptureFile();Stamp("OpenFile","begin");
  auto result=file->OpenFile(argv[1],"rdc",nullptr);Stamp("OpenFile","end");
  if(!result.OK()){file->Shutdown();return 3;}
  IReplayController *controller=nullptr;Stamp("OpenCapture","begin");
  float lastProgress=-1;
  rdctie(result,controller)=file->OpenCapture(ReplayOptions(),[&](float progress) {
    if(progress-lastProgress>=0.02f || (progress>=1.0f && lastProgress<1.0f))
    {printf("{\"progress\":%.6f,\"seconds\":%.9f}\n",progress,Now());fflush(stdout);lastProgress=progress;}
  });
  Stamp("OpenCapture","end");file->Shutdown();
  if(!result.OK() || !controller)
  {fprintf(stderr,"OpenCapture failed: %s\n",result.internal_msg?result.internal_msg->c_str():"unknown");return 4;}
  uint32_t last=0;rdcarray<ResourceId> presented;Stamp("RootActions","begin");
  Actions(controller->GetRootActions(),last,presented);Stamp("RootActions","end");
  Stamp("ResourceInventory","begin");auto textures=controller->GetTextures();auto buffers=controller->GetBuffers();Stamp("ResourceInventory","end");
  printf("Inventory: last=%u textures=%zu buffers=%zu presents=%zu\n",last,textures.size(),buffers.size(),presented.size());
  // Optional diagnostic target from the CLI, never backend support eligibility.
  auto selectedReadback=[&](const char *filename) {
    if(argc<4 || !strtoull(argv[3],nullptr,10))return true;
    const uint64_t wanted=strtoull(argv[3],nullptr,10);
    for(const auto &texture:textures)
    {
      uint64_t id=0;static_assert(sizeof(id)==sizeof(texture.resourceId),"ResourceId size");
      memcpy(&id,&texture.resourceId,sizeof(id));if(id!=wanted)continue;
      if(texture.byteSize>64*1024*1024)return false;
      auto data=controller->GetTextureData(texture.resourceId,{0,0,0});
      printf("Selected: id=%llu %ux%u format=%s bytes=%zu\n",(unsigned long long)wanted,
          texture.width,texture.height,texture.format.Name().c_str(),data.size());
      FILE *f=fopen((rdcstr(argv[2])+"/"+filename).c_str(),"wb");if(!f)return false;
      const bool saved=!data.empty() && fwrite(data.data(),1,data.size(),f)==data.size();fclose(f);return saved;
    }
    return false;
  };
  Stamp("SelectedReadback","begin");if(!selectedReadback("selected-first.bin"))return 5;
  Stamp("SelectedReadback","end");
  if(repeatReadback)
  {
    Stamp("UnchangedEventReadback","begin");
    if(!selectedReadback("selected-same-event.bin"))return 5;
    Stamp("UnchangedEventReadback","end");
  }
  Stamp("PresentedReadback","begin");unsigned index=0;
  for(auto id:presented)
  {
    TextureDescription desc;bool found=false;
    for(const auto &t:textures)if(t.resourceId==id){desc=t;found=true;break;}
    if(!found)return 5;
    auto data=controller->GetTextureData(id,{0,0,0});
    const rdcstr name=rdcstr(argv[2])+"/present-"+std::to_string(index++).c_str()+".bin";
    FILE *f=fopen(name.c_str(),"wb");if(!f)return 5;
    bool saved=fwrite(data.data(),1,data.size(),f)==data.size();fclose(f);if(!saved)return 5;
    printf("Presented: %ux%u bytes=%zu special=%u BGRA=%u\n",desc.width,desc.height,data.size(),unsigned(desc.format.Special()),unsigned(desc.format.BGRAOrder()));
  }
  Stamp("PresentedReadback","end");
  for(unsigned reset=1;reset<=resets;reset++)
  {
    printf("{\"reset\":%u}\n",reset);fflush(stdout);
    Stamp("EID0","begin");controller->SetFrameEvent(0,true);Stamp("EID0","end");
    Stamp("FinalEvent","begin");controller->SetFrameEvent(last,true);Stamp("FinalEvent","end");
    const std::string filename=reset==1?"selected-reset.bin":
        "selected-reset-"+std::to_string(reset)+".bin";
    Stamp("ResetSelectedReadback","begin");if(!selectedReadback(filename.c_str()))return 5;
    Stamp("ResetSelectedReadback","end");
    if(repeatReadback)
    {
      const std::string repeated="selected-reset-same-event-"+std::to_string(reset)+".bin";
      Stamp("ResetUnchangedEventReadback","begin");
      if(!selectedReadback(repeated.c_str()))return 5;
      Stamp("ResetUnchangedEventReadback","end");
    }
    if(!controller->GetFatalErrorStatus().OK())return 6;
  }
  result=controller->GetFatalErrorStatus();Stamp("Shutdown","begin");controller->Shutdown();Stamp("Shutdown","end");
  if(!result.OK())return 6;
  devicePool.reset();
  // Comparable closes/reopens on the same thread/process. Immutable syntax may
  // persist; Native resources and all dynamic restoration decisions are rebuilt.
  // Time OpenCapture separately, then export the same selected output from
  // each fresh device. This distinguishes an EID0-only reset defect from a
  // changing result on independent executions of the identical capture.
  for(cycle=1;cycle<=reopens;cycle++)
  {
    devicePool.reset(new NativeAutoreleaseScope);
    file=RENDERDOC_OpenCaptureFile();Stamp("OpenFile","begin");
    result=file->OpenFile(argv[1],"rdc",nullptr);Stamp("OpenFile","end");
    if(!result.OK()){file->Shutdown();return 3;}
    controller=nullptr;Stamp("OpenCapture","begin");
    rdctie(result,controller)=file->OpenCapture(ReplayOptions(),nullptr);
    Stamp("OpenCapture","end");file->Shutdown();
    if(!result.OK() || !controller)
    {
      fprintf(stderr,"OpenCapture reopen %u failed: %s\n",cycle,
          result.internal_msg?result.internal_msg->c_str():"unknown");
      return 4;
    }
    textures=controller->GetTextures();
    const std::string filename="selected-reopen-"+std::to_string(cycle)+".bin";
    Stamp("ReopenSelectedReadback","begin");
    if(!selectedReadback(filename.c_str()))return 5;
    Stamp("ReopenSelectedReadback","end");
    result=controller->GetFatalErrorStatus();Stamp("Shutdown","begin");controller->Shutdown();Stamp("Shutdown","end");
    if(!result.OK())return 6;
    devicePool.reset();
  }
  RENDERDOC_ShutdownReplay();
  puts("GPU_COMPLETED; output comparison pending; GUI not measured by this probe");return 0;
}
