// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"

REPLAY_PROGRAM_MARKER()

static void Events(const rdcarray<ActionDescription> &actions, rdcarray<uint32_t> &dispatches,
                   uint32_t &last)
{
  for(const ActionDescription &action : actions)
  {
    last = last > action.eventId ? last : action.eventId;
    if(action.flags & ActionFlags::Dispatch) dispatches.push_back(action.eventId);
    Events(action.children, dispatches, last);
  }
}

int main(int argc, char **argv)
{
  if(argc != 3) return 2;
  @autoreleasepool
  {
    GlobalEnvironment env;
    env.enumerateGPUs = false;
    rdcarray<rdcstr> arguments; arguments.push_back(argv[0]);
    RENDERDOC_InitialiseReplay(env, arguments);
    // Force a different allocation order so unchanged capture-process pointers cannot pass.
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLBuffer> padding = [device newBufferWithLength:1048576 options:MTLResourceStorageModeShared];
    if(!padding) return 3;
    ICaptureFile *file = RENDERDOC_OpenCaptureFile();
    ResultDetails result = file->OpenFile(argv[1], "rdc", nullptr);
    if(!result.OK()) return 4;
    IReplayController *controller = nullptr;
    rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr);
    file->Shutdown();
    if(!result.OK() || !controller)
    {
      fprintf(stderr, "OpenCapture failed: %s\n", result.internal_msg ? result.internal_msg->c_str() : "no detail");
      return 5;
    }
    ResourceId output, table, backbuffer;
    for(const BufferDescription &buffer : controller->GetBuffers())
    {
      if(buffer.length == 4) output = buffer.resourceId;
      if(buffer.length == 96) table = buffer.resourceId;
    }
    for(const TextureDescription &texture : controller->GetTextures())
      if(texture.width == 2 && texture.height == 2) backbuffer = texture.resourceId;
    rdcarray<uint32_t> dispatches;
    uint32_t last = 0;
    Events(controller->GetRootActions(), dispatches, last);
    if(dispatches.size() != 2 || output == ResourceId() || table == ResourceId() ||
       backbuffer == ResourceId()) return 6;
    const uint64_t capturedAddress = strtoull(argv[2], nullptr, 10);
    for(int cycle = 0; cycle < 3; cycle++)
    {
      for(int stage : {0, 1, 0, 1})
      {
        controller->SetFrameEvent(dispatches[stage], true);
        bytebuf bytes = controller->GetBufferData(output, 0, 4);
        uint32_t value = 0;
        if(bytes.size() == 4) memcpy(&value, bytes.data(), 4);
        if(value != (stage ? 161U : 122U))
        { fprintf(stderr, "seek stage=%d expected=%u actual=%u\n", stage, stage ? 161U : 122U, value); return 7; }
        bytebuf packet = controller->GetBufferData(table, 0, 96);
        if(packet.size() != 96) return 8;
        uint64_t words[12]; memcpy(words, packet.data(), sizeof(words));
        if(words[0] != capturedAddress || words[7] != ((uint64_t(0xdeadbeefU) << 32) | 17)) return 9;
        for(int i : {1, 2, 3, 8, 9, 10, 11})
          if(words[i] != 0xabcdef0123456789ULL) return 10;
        if(!words[4] || !words[5] || !words[6] || words[4] == capturedAddress) return 11;
        printf("seek PASS cycle=%d event=%u result=%u replayVA=%llu capturedVA=%llu\n",
            cycle, dispatches[stage], value, (unsigned long long)words[4],
            (unsigned long long)capturedAddress);
      }
    }
    controller->SetFrameEvent(last, true);
    bytebuf pixels = controller->GetTextureData(backbuffer, {0, 0, 0});
    if(pixels.size() != 16) return 12;
    for(size_t i = 0; i < pixels.size(); i += 4)
      if(pixels[i] != 128 || pixels[i+1] != 64 || pixels[i+2] != 32 || pixels[i+3] != 255) return 13;
    controller->Shutdown();
    RENDERDOC_ShutdownReplay();
    printf("descriptor replay PASS: GPU 122/161, offsets, partial CPU update, constants, 12 seeks, BGRA pixels\n");
  }
  return 0;
}
