// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()

static void Find(const rdcarray<ActionDescription> &actions, uint32_t &dispatch, uint32_t &last)
{
  for(const auto &a : actions)
  {
    last = last > a.eventId ? last : a.eventId;
    if(a.flags & ActionFlags::Dispatch) dispatch = a.eventId;
    Find(a.children, dispatch, last);
  }
}

int main(int argc, char **argv)
{
  if(argc != 5) return 2;
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
    rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr);
    file->Shutdown();
    if(!result.OK() || !controller)
    {
      fprintf(stderr, "OpenCapture failed: %s\n", result.internal_msg ? result.internal_msg->c_str() : "unknown");
      return 5;
    }
    ResourceId table, output, backbuffer;
    for(const auto &b : controller->GetBuffers())
    {
      if(b.length == 48) table = b.resourceId;
      if(b.length == 32) output = b.resourceId;
    }
    for(const auto &t : controller->GetTextures())
      if(t.width == 2 && t.height == 2) backbuffer = t.resourceId;
    uint32_t dispatch = 0, last = 0; Find(controller->GetRootActions(), dispatch, last);
    if(!dispatch || table == ResourceId() || output == ResourceId() || backbuffer == ResourceId()) return 6;
    for(int cycle = 0; cycle < 4; cycle++)
    {
      controller->SetFrameEvent(dispatch, true);
      auto resultBytes = controller->GetBufferData(output, 0, 32);
      auto tableBytes = controller->GetBufferData(table, 0, 48);
      uint64_t resultWords[4] = {}, tableWords[6] = {};
      if(resultBytes.size() != 32 || tableBytes.size() != 48) return 7;
      memcpy(resultWords, resultBytes.data(), 32); memcpy(tableWords, tableBytes.data(), 48);
      if(resultWords[0] != (0xdeadbeefULL << 32 | 80) ||
         !resultWords[1] || resultWords[1] != tableWords[0] || resultWords[1] != resultWords[3] ||
         !resultWords[2] || tableWords[1] || tableWords[2] != 0x123456789abcdef0ULL ||
         tableWords[3] || tableWords[5] != 0x123456789abcdef0ULL ||
         resultWords[1] == strtoull(argv[3], nullptr, 10) ||
         resultWords[2] == strtoull(argv[4], nullptr, 10)) return 8;
      printf("GPU PASS cycle=%d EID=%u result=80 sourceVA=%llu tableVA=%llu\n", cycle, dispatch,
          (unsigned long long)resultWords[1], (unsigned long long)resultWords[2]);
      controller->SetFrameEvent(0, true);
      tableBytes = controller->GetBufferData(table, 0, 48);
      if(tableBytes.size() != 48) return 9;
      memcpy(tableWords, tableBytes.data(), 48);
      if(!tableWords[0] || tableWords[0] == strtoull(argv[2], nullptr, 10) || tableWords[3] ||
         tableWords[2] != 0x123456789abcdef0ULL || tableWords[5] != 0x123456789abcdef0ULL) return 10;
      printf("reset PASS cycle=%d initial sourceVA=%llu retiredPointer=0\n", cycle, (unsigned long long)tableWords[0]);
    }
    controller->SetFrameEvent(last, true);
    const auto pixels = controller->GetTextureData(backbuffer, {0,0,0});
    if(pixels.size() != 16) return 11;
    for(size_t i=0;i<pixels.size();i+=4)
      if(pixels[i]!=128 || pixels[i+1]!=64 || pixels[i+2]!=32 || pixels[i+3]!=255) return 12;
    controller->Shutdown(); RENDERDOC_ShutdownReplay();
    puts("PASS sourced slot shadow: actual GPU 80/sentinel, inline and table VAs changed, retired pointer zero, ordinary bytes and repeated initial state restores");
  }
  return 0;
}
