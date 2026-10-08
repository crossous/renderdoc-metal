// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include <functional>
#include "renderdoc/api/replay/renderdoc_replay.h"

REPLAY_PROGRAM_MARKER()

int main(int argc, char **argv)
{
  if(argc != 3) return 1;
  const uint64_t size = strtoull(argv[2], nullptr, 10);
  const uint64_t backing = 1024 * 1024, offset = 128 * 1024, prefix = 64 * 1024;
  if(size != 229376 && size != backing) return 1;
  GlobalEnvironment env; env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {argv[0]});
  auto file = RENDERDOC_OpenCaptureFile(); auto result = file->OpenFile(argv[1], "rdc", {});
  if(!result.OK()) { file->Shutdown(); RENDERDOC_ShutdownReplay(); return 3; }
  ResourceId original, alias, readOriginal, readAlias;
  const auto &input = file->GetStructuredData();
  for(const auto *chunk : input.chunks)
    if(chunk->name == "MTLHeap::newBuffer(offset)")
    {
      auto at = chunk->FindChild("offset")->data.basic.u;
      auto length = chunk->FindChild("length")->data.basic.u;
      if(at == 0 && length == backing) original = chunk->FindChild("Buffer")->AsResourceId();
      if(at == offset && length == size) alias = chunk->FindChild("Buffer")->AsResourceId();
    }
  for(const auto *chunk : input.chunks)
    if(chunk->name == "MTLBlitCommandEncoder::copyFromBuffer")
    {
      auto source = chunk->FindChild("sourceBuffer")->AsResourceId();
      if(source == original) readOriginal = chunk->FindChild("destinationBuffer")->AsResourceId();
      if(source == alias) readAlias = chunk->FindChild("destinationBuffer")->AsResourceId();
    }
  if(original == ResourceId() || alias == ResourceId() || readOriginal == ResourceId() || readAlias == ResourceId())
  { file->Shutdown(); RENDERDOC_ShutdownReplay(); return 5; }
  auto opened = file->OpenCapture(ReplayOptions(), {}); file->Shutdown();
  if(!opened.first.OK() || !opened.second) { RENDERDOC_ShutdownReplay(); return 4; }
  auto replay = opened.second;
  uint32_t first = 0, second = 0, copyA = 0, copyB = 0, last = 0;
  unsigned copies = 0;
  std::function<void(const rdcarray<ActionDescription> &)> visit = [&](const auto &actions) {
    for(const auto &action : actions)
    {
      last = std::max(last, action.eventId);
      if(action.flags & ActionFlags::Copy)
      {
        copies++;
        if(action.copyDestination == original) first = action.eventId;
        if(action.copyDestination == alias) second = action.eventId;
        if(action.copyDestination == readOriginal) copyA = action.eventId;
        if(action.copyDestination == readAlias) copyB = action.eventId;
      }
      visit(action.children);
    }
  };
  visit(replay->GetRootActions());
  bool good = copies == 4 && first && first < second && second < copyA && copyA < copyB;
  auto expectedA = [&](uint32_t event) {
    bytebuf bytes; bytes.resize(backing);
    for(uint64_t i = 0; i < backing; i++)
      bytes[i] = event >= second && i >= offset && i - offset < size ? 0x5a :
                 event >= first && i < prefix ? 0x22 : 0x11;
    return bytes;
  };
  rdcarray<uint32_t> events = {last, 0, first - 1, first, second - 1, second, copyA - 1, copyA, copyB - 1, copyB, 0, last};
  unsigned checks = 0;
  for(unsigned cycle = 0; cycle < 4; cycle++)
    for(auto event : events)
    {
      replay->SetFrameEvent(event, true);
      auto bytes = replay->GetBufferData(original, 0, backing);
      if(bytes != expectedA(event)) { fprintf(stderr, "Original alias bytes mismatch event=%u\n", event); good = false; }
      auto aliasBytes = replay->GetBufferData(alias, 0, size);
      if(event == 0) good &= aliasBytes.empty();
      if(event >= second)
      {
        good &= aliasBytes.size() == size;
        for(byte value : aliasBytes) good &= value == 0x5a;
      }
      if(event >= copyA) good &= replay->GetBufferData(readOriginal, 0, backing) == expectedA(event);
      if(event >= copyB) good &= replay->GetBufferData(readAlias, 0, size) == aliasBytes;
      checks++;
    }
  for(auto pair : {rdcpair<ResourceId, uint32_t>(original, first), rdcpair<ResourceId, uint32_t>(alias, second)})
  {
    bool found = false;
    for(const auto &use : replay->GetUsage(pair.first))
      found |= use.eventId == pair.second && use.usage == ResourceUsage::CopyDst;
    good &= found;
  }
  replay->Shutdown(); RENDERDOC_ShutdownReplay();
  if(!good) { fprintf(stderr, "FAIL physical placement alias replay/reset/usage\n"); return 6; }
  printf("PASS physical placement alias: original=%llu alias=%llu, four GPU copies, %u event/EID0 checks, overlap/padding/readbacks/usage\n",
         (unsigned long long)backing, (unsigned long long)size, checks);
  return 0;
}
