// SPDX-License-Identifier: MIT
// Full invariant output comparison for the Native heap-creation contract.
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <vector>
REPLAY_PROGRAM_MARKER()
int main(int argc, char **argv)
{
  if(argc != 3) return 1;
  FILE *input = fopen(argv[2],"rb"); if(!input) return 2;
  std::vector<byte> expected(16448);
  const bool inputGood = fread(expected.data(),1,expected.size(),input) == expected.size() && fgetc(input) == EOF;
  fclose(input); if(!inputGood) return 2;
  GlobalEnvironment environment; environment.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(environment,{argv[0]});
  auto capture = RENDERDOC_OpenCaptureFile(); auto status = capture->OpenFile(argv[1],"rdc",nullptr);
  IReplayController *controller = nullptr;
  if(status.OK()) rdctie(status,controller) = capture->OpenCapture(ReplayOptions(),nullptr);
  capture->Shutdown();
  if(!status.OK() || !controller)
  {
    fprintf(stderr,"OpenCapture failed: %s\n",status.internal_msg ? status.internal_msg->c_str() : "unknown");
    RENDERDOC_ShutdownReplay(); return 4;
  }
  ResourceId result; for(const auto &resource : controller->GetResources())
    if(resource.name == "Heap creation coherence output") result = resource.resourceId;
  bool matchingLength = false;
  for(const auto &buffer : controller->GetBuffers())
    matchingLength |= buffer.resourceId == result && buffer.length == expected.size();
  if(!matchingLength) result = ResourceId();
  uint32_t last = 0;
  std::function<void(const rdcarray<ActionDescription> &)> visit = [&](const auto &actions) {
    for(const auto &action : actions) { last = std::max(last,action.eventId); visit(action.children); }
  }; visit(controller->GetRootActions());
  bool good = result != ResourceId() && last;
  unsigned checks = 0;
  unsigned initialChecks = 0;
  for(unsigned cycle = 0; cycle < 7 && result != ResourceId(); cycle++)
  {
    if(cycle)
    {
      controller->SetFrameEvent(0,true);
      const auto initial = controller->GetBufferData(result,0,expected.size());
      unsigned differences = 0;
      for(byte value : initial) differences += value != 0xa7;
      good = good && initial.size() == expected.size() && !differences && controller->GetFatalErrorStatus().OK();
      printf("EID0 initial payload cycle=%u bytes=%zu mismatchedBytes=%u\n",cycle,initial.size(),differences);
      initialChecks++;
      controller->SetFrameEvent(last,true);
    }
    const auto bytes = controller->GetBufferData(result,0,expected.size());
    if(bytes.size() != expected.size() || !controller->GetFatalErrorStatus().OK()) { good = false; break; }
    unsigned mismatchedWords = 0, mismatchedBytes = 0;
    for(size_t i = 0;i<expected.size();i++) mismatchedBytes += bytes[i] != expected[i];
    for(size_t i = 0;i<16384;i+=4) mismatchedWords += bytes[i] || bytes[i+1] || bytes[i+2] || bytes[i+3];
    printf("Complete Native output cycle=%u bytes=%zu mismatchedBytes=%u coherenceErrors=%u\n",
      cycle,bytes.size(),mismatchedBytes,mismatchedWords); fflush(stdout);
    good = good && !mismatchedBytes; checks++;
  }
  const bool completed = controller->GetFatalErrorStatus().OK();
  controller->Shutdown(); RENDERDOC_ShutdownReplay();
  printf("GPU execution %s; output %s; complete checks=%u EID0=%u; no unordered intermediate-byte oracle\n",
    completed ? "COMPLETED" : "FAIL",good ? "MATCH" : "FAIL",checks,initialChecks);
  return completed && good && checks == 7 && initialChecks == 6 ? 0 : 6;
}
