// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Actions(const rdcarray<ActionDescription> &as, rdcarray<uint32_t> &dispatches)
{
  for(const auto &a : as) { if(a.flags & ActionFlags::Dispatch) dispatches.push_back(a.eventId); Actions(a.children, dispatches); }
}
int main(int argc, char **argv)
{
  if(argc != 3) return 2;
  @autoreleasepool
  {
    id<MTLDevice> native = MTLCreateSystemDefaultDevice(); NSMutableArray<id<MTLBuffer>> *padding = [NSMutableArray new];
    for(unsigned i = 0; i < 32; i++) [padding addObject:[native newBufferWithLength:131072 options:MTLResourceStorageModeShared]];
    GlobalEnvironment env; env.enumerateGPUs = false; rdcarray<rdcstr> args; args.push_back(argv[0]); RENDERDOC_InitialiseReplay(env, args);
    auto file = RENDERDOC_OpenCaptureFile(); auto result = file->OpenFile(argv[1], "rdc", nullptr); if(!result.OK()) return 3;
    IReplayController *controller = nullptr; rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr); file->Shutdown();
    if(!result.OK() || !controller) { fprintf(stderr, "OpenCapture failed: %s\n", result.internal_msg ? result.internal_msg->c_str() : "unknown"); return 4; }
    ResourceId table, samplers, output;
    for(const auto &b : controller->GetBuffers())
    { if(b.length == 786432 * 24) table = b.resourceId; if(b.length == 4096 * 24) samplers = b.resourceId; }
    // Both input and output have 8 bytes: identify the ordinary output through the dispatch binding.
    for(const auto *c : controller->GetStructuredFile().chunks)
      if(c->name == "MTLComputeCommandEncoder::setBuffer" && c->FindChild("index")->AsUInt64() == 1) output = c->FindChild("buffer")->AsResourceId();
    rdcarray<uint32_t> dispatches; Actions(controller->GetRootActions(), dispatches);
    if(dispatches.size() != 2 || table == ResourceId() || samplers == ResourceId() || output == ResourceId()) return 5;
    uint64_t replayBase = 0, samplerIdentities[2] = {};
    auto tables = [&](unsigned phase) {
      auto data = controller->GetBufferData(table, 0, 0); if(data.size() != 786432 * 24) return false;
      for(size_t i = 0; i < 786430; i++)
      { uint64_t words[3]; memcpy(words, data.data() + i * 24, 24); if(words[0] || words[1] || words[2] != 0xababababababababULL) return false; }
      uint64_t b[3], t[3]; memcpy(b, data.data() + 786430 * 24, 24); memcpy(t, data.data() + 786431 * 24, 24);
      if(!b[0] || b[0] == strtoull(argv[2], nullptr, 10) + phase * 4 || b[1] || b[2] != 0x1111111111111111ULL || t[0] || !t[1] || t[2] != 0x2222222222222222ULL) return false;
      if(!replayBase) replayBase = b[0] - phase * 4; if(b[0] != replayBase + phase * 4) return false;
      data = controller->GetBufferData(samplers, 0, 0); if(data.size() != 4096 * 24) return false;
      for(size_t i = 0; i < 4095; i++)
      { uint64_t words[3]; memcpy(words, data.data() + i * 24, 24); if(words[0] || words[1] != 0xefefefefefefefefULL || words[2] != 0xcdcdcdcdcdcdcdcdULL) return false; }
      uint64_t s[3]; memcpy(s, data.data() + 4095 * 24, 24);
      if(!s[0] || s[1] != 0x123456789abcdef0ULL || s[2] != 0x3333333333333333ULL) return false;
      if(!samplerIdentities[phase]) samplerIdentities[phase] = s[0];
      return s[0] == samplerIdentities[phase] && (!samplerIdentities[1 - phase] || s[0] != samplerIdentities[1 - phase]);
    };
    for(unsigned cycle = 0; cycle < 4; cycle++)
    {
      for(unsigned phase : {1u, 0u})
      {
        controller->SetFrameEvent(dispatches[phase], true);
        const auto data = controller->GetBufferData(output, 0, 8); uint32_t words[2] = {};
        if(data.size() != 8) return 6; memcpy(words, data.data(), 8);
        if(words[0] != (phase ? 161u : 122u) || words[1] != 0xdeadbeef || !tables(phase)) return 7;
        printf("GPU large table PASS cycle=%u phase=%u value=%u\n", cycle, phase, words[0]);
      }
      controller->SetFrameEvent(0, true); if(!tables(0)) return 8;
    }
    controller->Shutdown(); RENDERDOC_ShutdownReplay();
    puts("PASS large typed tables: 786432 resource slots, 4096 sampler slots, all empty metadata, tail identities and EID0 restoration");
  }
  return 0;
}
