// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cstdio>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()

static void Collect(const rdcarray<ActionDescription> &actions,
                    rdcarray<const ActionDescription *> &out, rdcarray<APIEvent> &events)
{
  for(const auto &a : actions)
  {
    out.push_back(&a); events.append(a.events); Collect(a.children, out, events);
  }
}

int main(int argc, char **argv)
{
  if(argc != 2) return 1;
  GlobalEnvironment env; env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {argv[0]});
  auto file = RENDERDOC_OpenCaptureFile();
  auto result = file->OpenFile(argv[1], "rdc", nullptr);
  IReplayController *c = nullptr;
  if(result.OK()) rdctie(result, c) = file->OpenCapture(ReplayOptions(), nullptr);
  file->Shutdown();
  if(!result.OK() || !c) {fprintf(stderr, "Open failed\n"); return 2;}
  rdcarray<const ActionDescription *> actions;
  rdcarray<APIEvent> apiEvents;
  Collect(c->GetRootActions(), actions, apiEvents);
  ResourceId source, destination, target;
  uint32_t firstDraw = 0, secondDraw = 0, copy = 0, sourceBirth = 0, destinationBirth = 0;
  uint64_t firstDrawOffset = 0, copyOffset = 0, sourceBirthOffset = 0, destinationBirthOffset = 0;
  for(const auto &a : actions)
  {
    if(a->flags & ActionFlags::Drawcall)
    {
      if(!firstDraw) firstDraw = a->eventId; else secondDraw = a->eventId;
    }
    if(a->flags & ActionFlags::Copy)
    {
      copy = a->eventId; source = a->copySource; destination = a->copyDestination;
    }
  }
  for(const auto &resource : c->GetResources())
    if(resource.name == "Future Shared regression target") target = resource.resourceId;
  rdcarray<uint32_t> events;
  const auto &sd = c->GetStructuredFile();
  for(const auto &event : apiEvents)
  {
    if(!events.contains(event.eventId)) events.push_back(event.eventId);
    if(event.eventId == firstDraw) firstDrawOffset = event.fileOffset;
    if(event.eventId == copy) copyOffset = event.fileOffset;
    if(event.chunkIndex >= sd.chunks.size()) continue;
    const auto chunk = sd.chunks[event.chunkIndex];
    if(!chunk->name.beginsWith("MTLDevice::newBuffer")) continue;
    const auto buffer = chunk->FindChild("Buffer");
    if(buffer && buffer->AsResourceId() == source) {sourceBirth = event.eventId; sourceBirthOffset = event.fileOffset;}
    if(buffer && buffer->AsResourceId() == destination) {destinationBirth = event.eventId; destinationBirthOffset = event.fileOffset;}
  }
  std::sort(events.begin(), events.end());
  // Public EIDs bake CPU calls separately from GPU submissions. Prove the creation
  // ordering with captured file offsets, rather than comparing those reordered EIDs.
  bool ok = firstDraw && secondDraw && copy && target != ResourceId() &&
            firstDrawOffset < sourceBirthOffset && sourceBirthOffset < destinationBirthOffset &&
            destinationBirthOffset < copyOffset;
  printf("Draws %u/%u, births %u/%u, copy %u, API events %zu\n",
         firstDraw, secondDraw, sourceBirth, destinationBirth, copy, events.size());
  for(unsigned cycle = 0; cycle < 3 && ok; cycle++)
    for(size_t i = 0; i < events.size() && ok; i++)
    {
      uint32_t eid = events[cycle == 1 ? events.size() - 1 - i : i];
      uint64_t offset = 0;
      for(const auto &event : apiEvents) if(event.eventId == eid) offset = event.fileOffset;
      c->SetFrameEvent(0, true);
      ok &= c->GetBufferData(source, 0, 0).empty() &&
            c->GetBufferData(destination, 0, 0).empty();
      c->SetFrameEvent(eid, true);
      ok &= c->GetFatalErrorStatus().OK();
      if(!ok) {fprintf(stderr, "Event %u failed\n", eid); break;}
      for(unsigned slot = 0; slot < 2; slot++)
      {
        const auto bytes = c->GetBufferData(slot ? destination : source, 0, 0);
        const auto birth = slot ? destinationBirthOffset : sourceBirthOffset;
        if(offset < birth) {ok &= bytes.empty(); continue;}
        ok &= bytes.size() == 256;
        for(size_t b = 0; b < bytes.size(); b++)
        {
          byte expected = b >= 64 && b < 128 ? 0x3c : 0x11;
          if(slot && (offset < copyOffset || b < 32 || b >= 192)) expected = 0xa5;
          ok &= bytes[b] == expected;
        }
      }
      if(eid == firstDraw || eid == secondDraw)
      {
        const auto pixels = c->GetTextureData(target, {0, 0, 0});
        ok &= pixels.size() == 32 * 32 * 4;
        for(size_t p = 0; p + 3 < pixels.size(); p += 4)
          ok &= pixels[p] == 64 && pixels[p+1] == 128 && pixels[p+2] == 191 && pixels[p+3] == 255;
      }
      printf("%s cycle=%u EID=%u: resource birth, CPU bytes and GPU prefix\n", ok ? "PASS" : "FAIL", cycle, eid);
    }
  bool copySrc = false, copyDst = false;
  for(auto u : c->GetUsage(source)) copySrc |= u.eventId == copy && u.usage == ResourceUsage::CopySrc;
  for(auto u : c->GetUsage(destination)) copyDst |= u.eventId == copy && u.usage == ResourceUsage::CopyDst;
  ok &= copySrc && copyDst && c->GetFatalErrorStatus().OK();
  c->Shutdown(); RENDERDOC_ShutdownReplay();
  return ok ? 0 : 3;
}
