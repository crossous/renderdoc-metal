// SPDX-License-Identifier: MIT
// Normal capture open followed by explicit UE event seeks and Native MRT readbacks.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()

static unsigned long long IDValue(ResourceId id)
{
  uint64_t value = 0; static_assert(sizeof(id) == sizeof(value), "ResourceId width");
  memcpy(&value, &id, sizeof(value)); return value;
}

static void Inventory(const rdcarray<ActionDescription> &actions,
                      const rdcarray<TextureDescription> &textures, const SDFile &structured)
{
  for(const auto &a : actions)
  {
    unsigned colorCount = 0;
    for(ResourceId output : a.outputs)
      if(output != ResourceId()) colorCount++;
    if(colorCount >= 3)
      printf("UE multiple-output action EID=%u targets=%u name=%s\n", a.eventId,
             colorCount, a.customName.c_str());
    for(const auto &t : textures)
      if(t.depth == 64 && t.width == 64 && t.height == 64 &&
         a.flags & ActionFlags::Drawcall && a.outputs.contains(t.resourceId))
        printf("UE volume MRT draw EID=%u resource=%llu instances=%u\n", a.eventId,
               IDValue(t.resourceId), a.numInstances);
    if((a.flags & ActionFlags::Dispatch) && (a.flags & ActionFlags::Indirect))
      for(const auto &event : a.events)
        if(event.chunkIndex < structured.chunks.size())
        {
          const auto *chunk = structured.chunks[event.chunkIndex];
          const auto *buffer = chunk->FindChild("indirectBuffer");
          const auto *offset = chunk->FindChild("indirectBufferOffset");
          if(buffer && offset)
            printf("UE indirect action EID=%u buffer=%llu offset=%llu groups=%u,%u,%u name=%s\n",
                   a.eventId, (unsigned long long)buffer->AsUInt64(),
                   (unsigned long long)offset->AsUInt64(), a.dispatchDimension[0],
                   a.dispatchDimension[1], a.dispatchDimension[2], a.customName.c_str());
        }
    Inventory(a.children, textures, structured);
  }
}

int main(int argc, char **argv)
{
  if(argc < 4) return 2;
  struct IndirectCheck { unsigned eid; unsigned long long buffer, offset; unsigned words[3]; };
  rdcarray<IndirectCheck> checks;
  for(int arg = 3; arg < argc; arg++)
    if(strncmp(argv[arg], "--indirect=", 11) == 0)
    {
      IndirectCheck check = {};
      if(sscanf(argv[arg] + 11, "%u,%llu,%llu,%u,%u,%u", &check.eid,
                &check.buffer, &check.offset, &check.words[0], &check.words[1],
                &check.words[2]) != 6) return 2;
      checks.push_back(check);
    }
  setvbuf(stdout, nullptr, _IONBF, 0);
  char debugPath[4096];
  snprintf(debugPath, sizeof(debugPath), "%s/renderdoc.log", argv[2]);
  RENDERDOC_SetDebugLogFile(debugPath);
  GlobalEnvironment env; env.enumerateGPUs = false;
  rdcarray<rdcstr> args; args.push_back(argv[0]); RENDERDOC_InitialiseReplay(env, args);
  auto file = RENDERDOC_OpenCaptureFile();
  auto status = file->OpenFile(argv[1], "rdc", nullptr);
  if(!status.OK()) return 3;
  IReplayController *c = nullptr;
  rdctie(status, c) = file->OpenCapture(ReplayOptions(), nullptr); file->Shutdown();
  if(!status.OK() || !c) return 4;
  puts("UE normal OpenCapture OK"); Inventory(c->GetRootActions(), c->GetTextures(), c->GetStructuredFile());
  int exitCode = 0;
  for(int arg = 3; arg < argc && !exitCode; arg++)
  {
    if(strncmp(argv[arg], "--indirect=", 11) == 0) continue;
    const uint32_t eid = uint32_t(strtoul(argv[arg], nullptr, 10));
    rdcarray<Descriptor> outputs;
    rdcarray<bytebuf> baseline;
    for(unsigned cycle = 0; cycle < 2 && !exitCode; cycle++)
    {
      c->SetFrameEvent(0, true); c->SetFrameEvent(eid, true);
      status = c->GetFatalErrorStatus();
      if(!status.OK())
      {
        fprintf(stderr, "UE event failed EID=%u cycle=%u: %s\n", eid, cycle,
                status.internal_msg ? status.internal_msg->c_str() : "unknown");
        exitCode = 5; break;
      }
      const auto current = c->GetPipelineState().GetMetalPipelineState()->colorTargets;
      if(!cycle) outputs = current;
      if(outputs != current) { exitCode = 6; break; }
      printf("UE event seek OK EID=%u cycle=%u targets=%zu\n", eid, cycle, current.size());
      for(const auto &check : checks)
        if(check.eid == eid)
        {
          ResourceId resource;
          uint64_t id = check.buffer; memcpy(&resource, &id, sizeof(id));
          const auto bytes = c->GetBufferData(resource, check.offset, 12);
          unsigned actual[3] = {};
          if(bytes.size() == 12) memcpy(actual, bytes.data(), 12);
          printf("UE Native indirect EID=%u cycle=%u buffer=%llu offset=%llu actual=%u,%u,%u expected=%u,%u,%u\n",
                 eid, cycle, check.buffer, check.offset, actual[0], actual[1], actual[2],
                 check.words[0], check.words[1], check.words[2]);
          if(bytes.size() != 12 || memcmp(actual, check.words, 12)) { exitCode = 11; break; }
        }
      if(exitCode) break;
      unsigned index = 0;
      for(const auto &target : current)
      {
        if(target.resource == ResourceId()) continue;
        const auto bytes = c->GetTextureData(target.resource,
                                           {target.firstMip, target.firstSlice, 0});
        if(bytes.empty()) { exitCode = 7; break; }
        if(!cycle) baseline.push_back(bytes);
        else if(index >= baseline.size() || baseline[index] != bytes)
        { fprintf(stderr, "UE MRT data mismatch EID=%u target=%u\n", eid, index); exitCode = 8; break; }
        char path[4096];
        snprintf(path, sizeof(path), "%s/eid-%u-cycle-%u-target-%u.bin", argv[2], eid, cycle, index);
        FILE *out = fopen(path, "wb");
        if(!out) { exitCode = 9; break; }
        const bool saved = fwrite(bytes.data(), 1, bytes.size(), out) == bytes.size(); fclose(out);
        if(!saved) { exitCode = 10; break; }
        printf("UE Native MRT EID=%u cycle=%u target=%u resource=%llu mip=%u slice=%u bytes=%zu\n",
               eid, cycle, index++, IDValue(target.resource),
               target.firstMip, target.firstSlice, bytes.size());
      }
    }
  }
  rdcstr debugContents;
  RENDERDOC_GetLogFileContents(0, debugContents);
  snprintf(debugPath, sizeof(debugPath), "%s/renderdoc-final.log", argv[2]);
  if(FILE *debugOutput = fopen(debugPath, "wb"))
  {
    fwrite(debugContents.c_str(), 1, debugContents.size(), debugOutput);
    fclose(debugOutput);
  }
  c->Shutdown(); RENDERDOC_ShutdownReplay();
  if(!exitCode) puts("PASS UE selected events, EID0 resets and identical Native MRT bytes");
  return exitCode;
}
