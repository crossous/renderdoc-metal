// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()

static void Events(const rdcarray<ActionDescription> &actions, rdcarray<uint32_t> &dispatches)
{
  for(const ActionDescription &action : actions)
  {
    if(action.flags & ActionFlags::Dispatch) dispatches.push_back(action.eventId);
    Events(action.children, dispatches);
  }
}

int main(int argc, char **argv)
{
  if(argc != 4) return 2;
  @autoreleasepool
  {
    GlobalEnvironment env; env.enumerateGPUs = false;
    rdcarray<rdcstr> arguments; arguments.push_back(argv[0]);
    RENDERDOC_InitialiseReplay(env, arguments);
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLBuffer> padding = [device newBufferWithLength:1048576 options:MTLResourceStorageModeShared];
    if(!padding) return 3;
    ICaptureFile *file = RENDERDOC_OpenCaptureFile();
    ResultDetails result = file->OpenFile(argv[1], "rdc", nullptr);
    if(!result.OK()) return 4;
    IReplayController *controller = nullptr;
    rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr); file->Shutdown();
    if(!result.OK() || !controller)
    {
      fprintf(stderr, "OpenCapture failed: %s\n", result.internal_msg ? result.internal_msg->c_str() : "no detail");
      return 5;
    }
    ResourceId output, destination;
    for(const BufferDescription &buffer : controller->GetBuffers())
    {
      if(buffer.length == 8) output = buffer.resourceId;
      if(buffer.length == 48) destination = buffer.resourceId;
    }
    rdcarray<uint32_t> dispatches; Events(controller->GetRootActions(), dispatches);
    if(dispatches.size() != 5 || output == ResourceId() || destination == ResourceId()) return 6;
    const uint64_t oldA = strtoull(argv[2], nullptr, 10), oldB = strtoull(argv[3], nullptr, 10);
    const uint32_t eventIndices[] = {1, 2, 4}, expected[] = {41, 121, 160};
    for(int cycle = 0; cycle < 3; cycle++)
      for(int stage : {0, 1, 2, 0, 2})
      {
        controller->SetFrameEvent(dispatches[eventIndices[stage]], true);
        bytebuf bytes = controller->GetBufferData(output, 0, 8);
        uint32_t values[2] = {}; if(bytes.size() == 8) memcpy(values, bytes.data(), 8);
        if(values[0] != expected[stage] || values[1] != 0xdeadbeefU)
        { fprintf(stderr, "stage=%d expected=%u actual=%u sentinel=%x\n", stage, expected[stage], values[0], values[1]); return 7; }
        bytebuf table = controller->GetBufferData(destination, 0, 48);
        if(table.size() != 48) return 8;
        uint64_t words[6]; memcpy(words, table.data(), 48);
        if(!words[0] || words[0] == oldA || words[0] == oldB || words[1] ||
           words[2] != 0xabcdef0123456789ULL || words[4]) return 9;
        if(stage == 0 && (words[3] || words[5])) return 10;
        if(stage != 0 && (!words[3] || words[3] == oldA || words[3] == oldB ||
                          words[5] != 0x12345678abcdef09ULL)) return 11;
        if(stage == 2 && words[0] != words[3]) return 12;
        printf("mixed seek PASS cycle=%d event=%u GPU=%u replayVA=%llu/%llu\n", cycle,
               dispatches[eventIndices[stage]], values[0], (unsigned long long)words[0],
               (unsigned long long)words[3]);
      }
    controller->Shutdown(); RENDERDOC_ShutdownReplay();
    printf("mixed descriptor replay PASS: GPU copy + explicit CPU entry, 41/121/160, 15 seeks\n");
  }
  return 0;
}
