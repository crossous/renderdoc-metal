// SPDX-License-Identifier: MIT
// Normal OpenCapture and full event replay, with Native texture/output readback.
#include <cstdio>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER()

struct LogEvidence
{
  rdcstr path;
  void Save()
  {
    rdcstr contents;
    RENDERDOC_GetLogFileContents(0, contents);
    if(contents.empty()) return;
    if(FILE *output = fopen(path.c_str(), "wb"))
    {
      fwrite(contents.c_str(), 1, contents.size(), output);
      fclose(output);
    }
  }
  ~LogEvidence() { Save(); }
};

struct Inventory
{
  uint32_t last = 0, draws = 0, dispatches = 0, markers = 0;
  rdcarray<ResourceId> presented;
};

static void Inspect(const rdcarray<ActionDescription> &actions, Inventory &info)
{
  for(const auto &a : actions)
  {
    if(!a.IsFakeMarker() && a.eventId > info.last) info.last = a.eventId;
    if(a.flags & ActionFlags::Drawcall) info.draws++;
    if(a.flags & ActionFlags::Dispatch) info.dispatches++;
    if(a.flags & ActionFlags::PushMarker) info.markers++;
    if(a.flags & ActionFlags::Present && !info.presented.contains(a.copyDestination))
      info.presented.push_back(a.copyDestination);
    Inspect(a.children, info);
  }
}

static bool SavePresented(IReplayController *controller, const Inventory &info,
                          const char *folder, const char *phase)
{
  char name[4096];
  snprintf(name, sizeof(name), "%s/%s-presented.json", folder, phase);
  FILE *manifest = fopen(name, "wb");
  if(!manifest) return false;
  fprintf(manifest, "[\n");
  uint32_t index = 0;
  for(ResourceId id : info.presented)
  {
    const auto textures = controller->GetTextures();
    const TextureDescription *description = nullptr;
    for(const auto &texture : textures)
      if(texture.resourceId == id) { description = &texture; break; }
    if(!description) { fclose(manifest); return false; }
    const auto bytes = controller->GetTextureData(id, {0, 0, 0});
    if(bytes.size() != size_t(description->width) * description->height * 4)
    { fprintf(stderr, "Invalid presented texture readback size=%zu\n", bytes.size()); fclose(manifest); return false; }
    char basename[256];
    snprintf(basename, sizeof(basename), "%s-present-%u-%ux%u.bin", phase, index,
             description->width, description->height);
    snprintf(name, sizeof(name), "%s/%s", folder, basename);
    FILE *output = fopen(name, "wb");
    if(!output) { fclose(manifest); return false; }
    const bool saved = fwrite(bytes.data(), 1, bytes.size(), output) == bytes.size();
    fclose(output);
    if(!saved) { fclose(manifest); return false; }
    fprintf(manifest, "%s{\"file\":\"%s\",\"width\":%u,\"height\":%u,\"bytes\":%zu,\"bgra\":%s,\"special\":%s}",
            index ? ",\n" : "", basename, description->width, description->height,
            bytes.size(), description->format.BGRAOrder() ? "true" : "false",
            description->format.Special() ? "true" : "false");
    fprintf(stdout, "Native presented readback phase=%s index=%u %ux%u bytes=%zu\n",
            phase, index++, description->width, description->height, bytes.size());
  }
  fprintf(manifest, "\n]\n"); fclose(manifest);
  return !info.presented.empty();
}

int main(int argc, char **argv)
{
  if(argc != 3 && !(argc == 4 && !strcmp(argv[3], "--open-output-only"))) return 2;
  const bool openOutputOnly = argc == 4;
  GlobalEnvironment environment; environment.enumerateGPUs = false;
  rdcarray<rdcstr> args; args.push_back(argv[0]);
  RENDERDOC_InitialiseReplay(environment, args);
  char logfile[4096];
  snprintf(logfile, sizeof(logfile), "%s/renderdoc.log", argv[2]);
  RENDERDOC_SetDebugLogFile(logfile);
  LogEvidence evidence = {rdcstr(argv[2]) + "/renderdoc-final.log"};
  auto file = RENDERDOC_OpenCaptureFile();
  auto result = file->OpenFile(argv[1], "rdc", nullptr);
  if(!result.OK()) return 3;
  IReplayController *controller = nullptr;
  rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr);
  file->Shutdown();
  if(!result.OK() || !controller)
  { fprintf(stderr, "Normal OpenCapture failed: %s\n", result.internal_msg ? result.internal_msg->c_str() : "unknown"); return 4; }
  Inventory info; Inspect(controller->GetRootActions(), info);
  printf("Normal OpenCapture OK: last=%u draws=%u dispatches=%u markers=%u presents=%zu textures=%zu buffers=%zu\n",
         info.last, info.draws, info.dispatches, info.markers, info.presented.size(),
         controller->GetTextures().size(), controller->GetBuffers().size());
  if(!SavePresented(controller, info, argv[2], "loaded")) return 5;
  for(uint32_t cycle = 0; cycle < (openOutputOnly ? 0U : 2U); cycle++)
  {
    controller->SetFrameEvent(0, true);
    if(!controller->GetFatalErrorStatus().OK()) return 6;
    controller->SetFrameEvent(info.last, true);
    result = controller->GetFatalErrorStatus();
    if(!result.OK())
    { fprintf(stderr, "Full event replay failed: %s\n", result.internal_msg ? result.internal_msg->c_str() : "unknown"); return 7; }
    char phase[64]; snprintf(phase, sizeof(phase), "replay-%u", cycle);
    if(!SavePresented(controller, info, argv[2], phase)) return 8;
  }
  evidence.Save();
  controller->Shutdown(); RENDERDOC_ShutdownReplay();
  puts(openOutputOnly ? "GPU EXECUTION COMPLETE; presented texture readback saved; event resets not run; outputs unvalidated" :
       "GPU EXECUTION COMPLETE; two full EID0/reset replays and presented readbacks saved; outputs unvalidated");
  return 0;
}
