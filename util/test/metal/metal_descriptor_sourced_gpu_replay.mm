// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Find(const rdcarray<ActionDescription> &actions, rdcarray<uint32_t> &dispatches,
                 uint32_t &copy, uint32_t &last)
{
  for(const auto &action : actions)
  {
    if(action.eventId > last) last = action.eventId;
    if(action.flags & ActionFlags::Dispatch) dispatches.push_back(action.eventId);
    if(action.flags & ActionFlags::Copy) copy = action.eventId;
    Find(action.children, dispatches, copy, last);
  }
}
int main(int argc, char **argv)
{
  const bool compute = argc == 8 && !strcmp(argv[7], "compute");
  if(argc != 7 && !compute) return 2;
  @autoreleasepool
  {
    GlobalEnvironment env; env.enumerateGPUs = false;
    rdcarray<rdcstr> args; args.push_back(argv[0]); RENDERDOC_InitialiseReplay(env, args);
    id<MTLBuffer> padding = [MTLCreateSystemDefaultDevice() newBufferWithLength:1048576 options:MTLResourceStorageModeShared];
    if(!padding) return 3;
    ICaptureFile *file = RENDERDOC_OpenCaptureFile();
    auto result = file->OpenFile(argv[1], "rdc", nullptr);
    IReplayController *controller = nullptr;
    if(!result.OK()) return 4;
    rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr); file->Shutdown();
    if(!result.OK() || !controller)
    { fprintf(stderr, "OpenCapture failed: %s\n", result.internal_msg ? result.internal_msg->c_str() : "unknown"); return 5; }
    ResourceId table, payload, output, texture, independentTarget;
    for(const auto &buffer : controller->GetBuffers())
    {
      if(buffer.length == 48) table = buffer.resourceId;
      if(buffer.length == 24) payload = buffer.resourceId;
      if(buffer.length == 64) output = buffer.resourceId;
    }
    for(const auto &t : controller->GetTextures())
    {
      if(t.width == 2 && t.height == 2) texture=t.resourceId;
      if(t.width == 1 && t.height == 1) independentTarget=t.resourceId;
    }
    rdcarray<uint32_t> dispatches; uint32_t copy = 0, last = 0;
    Find(controller->GetRootActions(), dispatches, copy, last);
    if(compute && dispatches.size() == 3) copy = dispatches[1];
    if(dispatches.size() != (compute ? 3U : 2U) || !copy ||
       table==ResourceId() || output==ResourceId() || payload==ResourceId()) return 6;
    const uint32_t before = strtoul(argv[5],nullptr,10), after = strtoul(argv[6],nullptr,10);
    const uint64_t oldA = strtoull(argv[2],nullptr,10), oldB=strtoull(argv[3],nullptr,10), oldTable=strtoull(argv[4],nullptr,10);
    auto check = [&](uint32_t value, size_t offset) {
      const auto data=controller->GetBufferData(output,offset,32);
      uint64_t words[4] = {};
      if(data.size()!=32) return false;
      memcpy(words,data.data(),32);
      const auto tableData=controller->GetBufferData(table,0,48);
      uint64_t tableWords[6] = {};
      if(tableData.size()!=48) return false;
      memcpy(tableWords,tableData.data(),48);
      if(words[0] != (0xdeadbeefULL << 32 | value) || words[1]!=words[3] ||
         words[1]!=tableWords[0] || !words[1] || words[1]==oldA || words[1]==oldB ||
         !words[2] || words[2]==oldTable || tableWords[1] || tableWords[2]!=0x123456789abcdef0ULL || tableWords[3])
        return false;
      printf("GPU PASS value=%u sourceVA=%llu tableVA=%llu\n", value,(unsigned long long)words[1],(unsigned long long)words[2]);
      return true;
    };
    for(int cycle=0;cycle<4;cycle++)
    {
      controller->SetFrameEvent(dispatches[0],true); if(!check(before,0)) return 7;
      controller->SetFrameEvent(copy,true);
      const auto dest=controller->GetBufferData(table,0,24), source=controller->GetBufferData(payload,0,24);
      if(dest!=source) return 8;
      controller->SetFrameEvent(dispatches[compute ? 2 : 1],true); if(!check(after,32)) return 9;
      controller->SetFrameEvent(0,true);
      const auto reset=controller->GetBufferData(table,0,24);
      if(reset.size()!=24) return 10;
      uint64_t pointer=0; memcpy(&pointer,reset.data(),8);
      if(!pointer || pointer==oldA || pointer==oldB) return 11;
      printf("seek PASS cycle=%d before=%u after=%u resetVA=%llu\n",cycle,before,after,(unsigned long long)pointer);
    }
    controller->SetFrameEvent(last,true);
    const auto pixels=controller->GetTextureData(texture,{0,0,0});
    if(pixels.size()!=16) return 12;
    for(size_t i=0;i<pixels.size();i+=4)
      if(pixels[i]!=128 || pixels[i+1]!=64 || pixels[i+2]!=32 || pixels[i+3]!=255) return 13;
    if(getenv("RENDERDOC_METAL_INTERLEAVED_PRODUCER"))
    {
      const uint8_t expected[] = {128,64,191,255};
      for(unsigned cycle=0;cycle<4;cycle++)
      {
        controller->SetFrameEvent(0,true); controller->SetFrameEvent(last,true);
        if(!controller->GetFatalErrorStatus().OK()) return 14;
        const auto bytes=controller->GetTextureData(independentTarget,{0,0,0});
        const auto data=controller->GetBufferData(output,0,64);
        uint64_t words[8] = {};
        if(data.size()!=64) return 15;
        memcpy(words,data.data(),64);
        if(bytes.size()!=4 || memcmp(bytes.data(),expected,4) ||
           words[0]!=(0xdeadbeefULL<<32|before) || words[4]!=(0xdeadbeefULL<<32|after) ||
           words[1]!=words[3] || words[5]!=words[7] || words[1]==words[5] ||
           words[2]!=words[6] || !check(after,32)) return 15;
      }
      puts("PASS interleaved independent Native clear and producer/consumer bytes after four EID0 resets");
    }
    controller->Shutdown(); RENDERDOC_ShutdownReplay();
    puts("PASS sourced GPU update: old shader read, relocated payload GPU copy, new shader read, seeks and clear pixels");
  }
  return 0;
}
