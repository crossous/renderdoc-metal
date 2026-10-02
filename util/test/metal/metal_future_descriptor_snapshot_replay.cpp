// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()
static void Dispatches(const rdcarray<ActionDescription> &actions, rdcarray<uint32_t> &events)
{
  for(const auto &a : actions) { if(a.flags & ActionFlags::Dispatch) events.push_back(a.eventId); Dispatches(a.children, events); }
}
int main(int argc, char **argv)
{
  if(argc != 2) return 2;
  GlobalEnvironment env; env.enumerateGPUs = false;
  rdcarray<rdcstr> args; args.push_back(argv[0]); RENDERDOC_InitialiseReplay(env, args);
  auto file = RENDERDOC_OpenCaptureFile(); auto status = file->OpenFile(argv[1], "rdc", nullptr);
  if(!status.OK()) return 3;
  IReplayController *c = nullptr; rdctie(status, c) = file->OpenCapture(ReplayOptions(), nullptr); file->Shutdown();
  if(!status.OK() || !c) return 4;
  ResourceId output;
  rdcarray<uint32_t> events; Dispatches(c->GetRootActions(), events);
  if(events.size() != 3) return 5;
  int result = 0;
  for(unsigned cycle = 0; cycle < 4 && !result; cycle++)
    for(unsigned event = 0; event < 3 && !result; event++)
    {
      c->SetFrameEvent(0, true); c->SetFrameEvent(events[event], true);
      if(!c->GetFatalErrorStatus().OK()) { result = 6; break; }
      const auto &bindings = c->GetPipelineState().GetMetalPipelineState()->computeBuffers;
      if(bindings.size() <= 1 || bindings[1].resourceId == ResourceId()) { result = 5; break; }
      if(output == ResourceId()) output = bindings[1].resourceId;
      if(output != bindings[1].resourceId) { result = 5; break; }
      auto data = c->GetBufferData(output, 0, 24); uint32_t words[6] = {};
      if(data.size() != 24) { result = 7; break; } memcpy(words, data.data(), 24);
      for(unsigned index = 0; index < 3; index++)
        if(words[index * 2] != (index <= event ? (index + 1) * 17 : 0) ||
           words[index * 2 + 1] != (index <= event ? 0xdeadbeefU : 0)) result = 8;
      printf("cycle=%u EID=%u Native output=%u/%u/%u status=%d\n", cycle, events[event], words[0], words[2], words[4], result);
    }
  c->Shutdown(); RENDERDOC_ShutdownReplay();
  if(!result) puts("PASS future descriptor snapshot: four EID0 reset cycles, first/middle/future Native values and markers");
  return result;
}
