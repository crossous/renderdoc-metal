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
  if(argc != 6) return 2;
  @autoreleasepool
  {
    GlobalEnvironment env;
    env.enumerateGPUs = false;
    rdcarray<rdcstr> arguments; arguments.push_back(argv[0]);
    RENDERDOC_InitialiseReplay(env, arguments);
    // Force a different allocation order so unchanged capture-process pointers cannot pass.
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLBuffer> padding = [device newBufferWithLength:1048576 options:MTLResourceStorageModeShared];
    MTLTextureDescriptor *paddingDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
        MTLPixelFormatR32Uint width:4 height:4 mipmapped:NO];
    id<MTLTexture> paddingTexture = [device newTextureWithDescriptor:paddingDescriptor];
    if(!padding || !paddingTexture) return 3;
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
      if(buffer.length == 16) output = buffer.resourceId;
      if(buffer.length == 48) table = buffer.resourceId;
    }
    for(const TextureDescription &texture : controller->GetTextures())
      if(texture.width == 2 && texture.height == 2) backbuffer = texture.resourceId;
    rdcarray<uint32_t> dispatches;
    uint32_t last = 0;
    Events(controller->GetRootActions(), dispatches, last);
    if(dispatches.size() != 2 || output == ResourceId() || table == ResourceId() ||
       backbuffer == ResourceId()) return 6;
    for(int cycle = 0; cycle < 3; cycle++)
    {
      for(int stage : {0, 1, 0, 1})
      {
        controller->SetFrameEvent(dispatches[stage], true);
        bytebuf bytes = controller->GetBufferData(output, 0, 16);
        uint32_t value = 0;
        if(bytes.size() == 16) memcpy(&value, bytes.data(), 4);
        if(value != (stage ? 167U : 89U))
        { fprintf(stderr, "seek stage=%d expected=%u actual=%u\n", stage, stage ? 167U : 89U, value); return 7; }
        bytebuf packet = controller->GetBufferData(table, 0, 48);
        if(packet.size() != 48 || bytes.size() != 16) return 8;
        uint64_t words[6]; memcpy(words, packet.data(), sizeof(words));
        uint32_t sentinel = 0; memcpy(&sentinel, bytes.data() + 4, 4);
        if(words[0] != 0xabcdef0123456789ULL || words[3] != 7 || words[4] != 0xfeedfacec001d00dULL || words[5] != 0x123456789abcdef0ULL ||
           sentinel != 0xdeadbeefU) return 9;
        if(!words[1] || words[1] == strtoull(argv[stage ? 3 : 2], nullptr, 10)) return 11;
        if(!words[2] || words[2] == strtoull(argv[stage ? 5 : 4], nullptr, 10)) return 16;
        printf("view identity PASS replayID=%llu capturedID=%llu\n", (unsigned long long)words[2],
            (unsigned long long)strtoull(argv[stage ? 5 : 4], nullptr, 10));
        printf("seek PASS cycle=%d event=%u result=%u replayVA=%llu capturedVA=%llu\n",
            cycle, dispatches[stage], value, (unsigned long long)words[1],
            (unsigned long long)strtoull(argv[stage ? 3 : 2], nullptr, 10));
      }
      controller->SetFrameEvent(0, true);
      bytebuf initialTable = controller->GetBufferData(table, 0, 48);
      uint64_t initialWords[6] = {};
      if(initialTable.size() != 48) return 14;
      memcpy(initialWords, initialTable.data(), sizeof(initialWords));
      if(initialWords[0] != 0xabcdef0123456789ULL || initialWords[1] ||
         initialWords[2] || initialWords[3] != 7 || initialWords[4] != 0xfeedfacec001d00dULL || initialWords[5] != 0x123456789abcdef0ULL) return 15;
      printf("frame-start PASS cycle=%d pointer=0 before resource creation\n", cycle);
    }
    controller->SetFrameEvent(last, true);
    bytebuf pixels = controller->GetTextureData(backbuffer, {0, 0, 0});
    if(pixels.size() != 16) return 12;
    for(size_t i = 0; i < pixels.size(); i += 4)
      if(pixels[i] != 128 || pixels[i+1] != 64 || pixels[i+2] != 32 || pixels[i+3] != 255) return 13;
    controller->Shutdown();
    RENDERDOC_ShutdownReplay();
    printf("frame view replay PASS: GPU 89/167, frame placement/view recreation, VA and texture ID relocation, constants, 12 seeks, BGRA pixels\n");
  }
  return 0;
}
