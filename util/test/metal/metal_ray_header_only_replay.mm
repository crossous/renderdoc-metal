// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstring>
#include <functional>
#include "renderdoc/api/replay/renderdoc_replay.h"

REPLAY_PROGRAM_MARKER()

// Public AS-header publication is independently replayable without a consuming PSO.
int main(int argc, char **argv)
{
  if(argc != 2) return 1;
  @autoreleasepool
  {
    __attribute__((objc_precise_lifetime)) id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    __attribute__((objc_precise_lifetime)) NSMutableArray *padding = [NSMutableArray new];
    for(unsigned i = 0; i < 16; i++)
    {
      auto structure = [device newAccelerationStructureWithSize:4096];
      auto buffer = [device newBufferWithLength:4096 options:MTLResourceStorageModeShared];
      if(!structure || !buffer) return 2;
      [padding addObject:structure]; [padding addObject:buffer];
    }
    GlobalEnvironment env; env.enumerateGPUs = false;
    rdcarray<rdcstr> args = {argv[0]};
    RENDERDOC_InitialiseReplay(env, args);
    auto file = RENDERDOC_OpenCaptureFile();
    auto result = file->OpenFile(argv[1], "rdc", {});
    if(!result.OK()) { file->Shutdown(); RENDERDOC_ShutdownReplay(); return 3; }
    const auto &input = file->GetStructuredData();
    ResourceId header, contribution;
    uint64_t captured[8] = {};
    bytebuf contributionInitial;
    unsigned declarations = 0;
    for(const auto *chunk : input.chunks)
      if(chunk->name == "MTLBuffer::DeclareRayASHeader")
      {
        declarations++;
        header = chunk->FindChild("buffer")->AsResourceId();
        contribution = chunk->FindChild("contributions")->AsResourceId();
        const auto *bytes = chunk->FindChild("bytes");
        if(!bytes || bytes->data.basic.u >= input.buffers.size() ||
           !input.buffers[bytes->data.basic.u] || input.buffers[bytes->data.basic.u]->size() != 64)
        { file->Shutdown(); RENDERDOC_ShutdownReplay(); return 5; }
        memcpy(captured, input.buffers[bytes->data.basic.u]->data(), 64);
      }
    for(const auto *chunk : input.chunks)
      if(chunk->name == "Internal::Initial Contents" &&
         chunk->FindChild("id")->AsResourceId() == contribution)
      {
        const auto *bytes = chunk->FindChild("Contents");
        if(bytes && bytes->data.basic.u < input.buffers.size() && input.buffers[bytes->data.basic.u])
          contributionInitial = *input.buffers[bytes->data.basic.u];
      }
    if(declarations != 1 || header == ResourceId() || contributionInitial.empty())
    { file->Shutdown(); RENDERDOC_ShutdownReplay(); return 5; }
    auto opened = file->OpenCapture(ReplayOptions(), {});
    result = opened.first;
    IReplayController *replay = opened.second;
    file->Shutdown();
    if(!result.OK()) { fprintf(stderr, "OpenCapture failed\n"); RENDERDOC_ShutdownReplay(); return 4; }
    uint32_t last = 0;
    unsigned dispatches = 0;
    rdcarray<uint32_t> builds;
    std::function<void(const rdcarray<ActionDescription> &)> visit = [&](const auto &actions) {
      for(const auto &action : actions)
      {
        last = std::max(last, action.eventId);
        if(action.flags & ActionFlags::Dispatch) dispatches++;
        if(action.flags & ActionFlags::BuildAccStruct) builds.push_back(action.eventId);
        visit(action.children);
      }
    };
    visit(replay->GetRootActions());
    bool good = last > 0 && !builds.empty() && dispatches == 0;
    if(!good) fprintf(stderr, "Unexpected actions last=%u builds=%zu dispatches=%u\n", last, builds.size(), dispatches);
    rdcarray<uint32_t> events = {last, 0};
    for(auto event : builds) { events.push_back(event - 1); events.push_back(event); }
    events.push_back(last); events.push_back(0); events.push_back(last);
    bytebuf relocated;
    unsigned checks = 0;
    for(unsigned cycle = 0; cycle < 4; cycle++)
      for(auto event : events)
      {
        replay->SetFrameEvent(event, true);
        auto bytes = replay->GetBufferData(header, 0, 64);
        auto actualContribution = replay->GetBufferData(contribution, 0, contributionInitial.size());
        if(actualContribution != contributionInitial)
          fprintf(stderr, "Contribution changed event=%u size=%zu/%zu\n", event, actualContribution.size(), contributionInitial.size());
        good &= actualContribution == contributionInitial;
        if(event == 0) good &= bytes.empty();
        else if(!bytes.empty())
        {
          good &= bytes.size() == 64;
          if(bytes.size() == 64)
          {
            uint64_t words[8]; memcpy(words, bytes.data(), 64);
            if(words[0] || words[1])
            {
              good &= words[0] != 0 && words[0] != captured[0] &&
                      words[1] != 0 && words[1] != captured[1];
              if(!words[0] || words[0] == captured[0] || !words[1] || words[1] == captured[1])
                fprintf(stderr, "Header relocation event=%u AS=%llu/%llu VA=%llu/%llu\n", event,
                        (unsigned long long)words[0], (unsigned long long)captured[0],
                        (unsigned long long)words[1], (unsigned long long)captured[1]);
              for(unsigned i = 2; i < 8; i++) good &= words[i] == 0;
            }
          }
        }
        if(event == last)
        {
          good &= bytes.size() == 64;
          if(relocated.empty()) relocated = bytes;
          good &= bytes == relocated;
          if(bytes.size() == 64) { uint64_t word; memcpy(&word, bytes.data(), 8); good &= word != 0; }
        }
        checks++;
      }
    replay->Shutdown(); RENDERDOC_ShutdownReplay();
    if(!good) { fprintf(stderr, "FAIL independent Header replay/reset\n"); return 6; }
    printf("PASS Header publication without query declarations: %u AS builds, zero compute dispatches, %u event/EID0 checks, relocated IDs/VA, immutable contributions\n", unsigned(builds.size()), checks);
    return 0;
  }
}
