// SPDX-License-Identifier: MIT
// Test-only: compare native texture observations along explicit capture targets.
// Encoder/resource numbers select diagnostics, never backend eligibility.
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <algorithm>
REPLAY_PROGRAM_MARKER()

static uint64_t Number(ResourceId id)
{
  uint64_t value = 0;
  static_assert(sizeof(id) == sizeof(value), "ResourceId representation");
  memcpy(&value, &id, sizeof(value));
  return value;
}
struct Target
{
  uint64_t encoder = 0;
  ResourceId texture;
  uint32_t mip = 0, slice = 0, event = 0;
  uint32_t requestedEvent = 0;
};
static bool inspectInputs = false;
static bool computeTargets = false;
static bool explicitEvents = false;
static std::map<ResourceId, rdcarray<EventUsage>> targetUsages;
struct InputRange
{
  uint64_t encoder = 0;
  ResourceId buffer;
  uint64_t offset = 0, bytes = 0;
};
static rdcarray<InputRange> inputRanges;
static bool ObserveBuffers(IReplayController *controller, const Target &target,
                           const char *folder, uint32_t cycle)
{
  for(const auto &range : inputRanges)
  {
    if(range.encoder != target.encoder) continue;
    const auto bytes = controller->GetBufferData(range.buffer, range.offset, range.bytes);
    if(bytes.size() != range.bytes || !controller->GetFatalErrorStatus().OK()) return false;
    char name[4096];
    snprintf(name, sizeof(name), "%s/input-cycle-%u-event-%u-buffer-%llu-offset-%llu.bin",
             folder, cycle, target.event, (unsigned long long)Number(range.buffer),
             (unsigned long long)range.offset);
    FILE *file = fopen(name, "wb");
    if(!file) return false;
    const bool saved = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    fclose(file);
    if(!saved) return false;
    printf("INPUT_RANGE cycle=%u event=%u buffer=%llu offset=%llu bytes=%llu\n",
           cycle, target.event, (unsigned long long)Number(range.buffer),
           (unsigned long long)range.offset, (unsigned long long)range.bytes);
  }
  return true;
}
static void Locate(const rdcarray<ActionDescription> &actions, const SDFile &structured,
                   rdcarray<Target> &targets, uint32_t &last)
{
  for(const auto &action : actions)
  {
    if(!action.IsFakeMarker()) last = std::max(last, action.eventId);
    if(!action.IsFakeMarker() &&
       (action.flags & (computeTargets ? ActionFlags::Dispatch : ActionFlags::Drawcall)))
      for(const auto &event : action.events)
      {
        if(event.eventId != action.eventId || event.chunkIndex >= structured.chunks.size()) continue;
        const auto *encoder = structured.chunks[event.chunkIndex]->FindChild(
            computeTargets ? "ComputeCommandEncoder" : "RenderCommandEncoder");
        if(!encoder) continue;
        for(auto &target : targets)
          if(Number(encoder->AsResourceId()) == target.encoder &&
             (!target.requestedEvent || target.requestedEvent == action.eventId))
          {
            bool output = action.outputs.contains(target.texture);
            if(computeTargets)
              for(const auto &usage : targetUsages[target.texture])
                output |= usage.eventId == action.eventId && usage.usage == ResourceUsage::CS_RWResource;
            if(output) target.event = std::max(target.event, action.eventId);
          }
      }
    Locate(action.children, structured, targets, last);
  }
}
static bool Observe(IReplayController *controller, const Target &target,
                    const char *folder, uint32_t cycle, bool completeFrame,
                    bool eventSelected = false)
{
  if(!completeFrame && !eventSelected) controller->SetFrameEvent(target.event, true);
  if(!controller->GetFatalErrorStatus().OK()) return false;
  const auto *state = controller->GetPipelineState().GetMetalPipelineState();
  if(!state) return false;
  bool bound = false;
  if(state)
    for(const auto &output : state->colorTargets)
      bound |= output.resource == target.texture && output.firstMip == target.mip && output.firstSlice == target.slice;
  // Compute targets are located from API dispatch + resource usage; this is a
  // diagnostic candidate, not shader feedback or proof of an actual store.
  if(!completeFrame && !computeTargets && !bound) return false;
  const auto textures = controller->GetTextures();
  const auto buffers = inspectInputs ? controller->GetBuffers() : rdcarray<BufferDescription>();
  const TextureDescription *description = nullptr;
  for(const auto &texture : textures)
    if(texture.resourceId == target.texture) description = &texture;
  // Readback budget only; does not reject any capture/API capability.
  if(!description || description->byteSize > 8 * 1024 * 1024) return false;
  const auto bytes = controller->GetTextureData(target.texture, {target.mip, target.slice, 0});
  if(bytes.empty() || bytes.size() > 8 * 1024 * 1024 || !controller->GetFatalErrorStatus().OK()) return false;
  char name[4096];
  snprintf(name, sizeof(name), "%s/%scycle-%u-event-%u-texture-%llu.bin", folder,
           completeFrame ? "whole-" : "", cycle, target.event,
           (unsigned long long)Number(target.texture));
  if(target.mip || target.slice)
    snprintf(name, sizeof(name), "%s/%scycle-%u-event-%u-texture-%llu-mip-%u-slice-%u.bin", folder,
             completeFrame ? "whole-" : "", cycle, target.event,
             (unsigned long long)Number(target.texture), target.mip, target.slice);
  FILE *file = fopen(name, "wb");
  if(!file) return false;
  const bool saved = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
  fclose(file);
  if(!saved) return false;
  printf("OBSERVE scope=%s cycle=%u event=%u encoder=%llu texture=%llu mip=%u slice=%u width=%u height=%u format=%s bytes=%zu pso=%llu shader=%llu\n",
         completeFrame ? "whole" : "checkpoint",
         cycle, target.event, (unsigned long long)target.encoder, (unsigned long long)Number(target.texture),
         target.mip, target.slice, description->width, description->height, description->format.Name().c_str(),
         bytes.size(), (unsigned long long)Number(computeTargets ? state->computePipelineResourceId : state->pipelineResourceId),
         (unsigned long long)Number(computeTargets ? state->computeShader.resourceId : state->fragmentShader.resourceId));
  for(const auto &access : controller->GetDescriptorAccess())
  {
    if(access.staticallyUnused) continue;
    const auto descriptors = controller->GetDescriptors(access.descriptorStore, {DescriptorRange(access)});
    for(const auto &descriptor : descriptors)
    {
      printf("CANDIDATE cycle=%u event=%u stage=%u type=%u store=%llu row=%u resource=%llu offset=%llu bytes=%llu mip=%u slice=%u\n",
             cycle, target.event, unsigned(access.stage), unsigned(access.type),
             (unsigned long long)Number(access.descriptorStore), access.byteOffset,
             (unsigned long long)Number(descriptor.resource), (unsigned long long)descriptor.byteOffset,
             (unsigned long long)descriptor.byteSize, descriptor.firstMip, descriptor.firstSlice);
      if(inspectInputs && CategoryForDescriptorType(access.type) == DescriptorCategory::ReadOnlyResource)
      {
        for(const auto &texture : textures)
          if(texture.resourceId == descriptor.resource && texture.byteSize <= 8 * 1024 * 1024)
          {
            const auto input = controller->GetTextureData(descriptor.resource,
                                      {descriptor.firstMip, descriptor.firstSlice, 0});
            if(input.empty() || input.size() > 8 * 1024 * 1024) return false;
            snprintf(name, sizeof(name), "%s/input-cycle-%u-event-%u-texture-%llu-mip-%u-slice-%u.bin",
                     folder, cycle, target.event, (unsigned long long)Number(descriptor.resource),
                     descriptor.firstMip, descriptor.firstSlice);
            FILE *output = fopen(name, "wb");
            if(!output) return false;
            const bool savedInput = fwrite(input.data(), 1, input.size(), output) == input.size();
            fclose(output);
            if(!savedInput) return false;
            printf("INPUT cycle=%u event=%u texture=%llu width=%u height=%u format=%s mip=%u slice=%u bytes=%zu\n",
                   cycle, target.event, (unsigned long long)Number(descriptor.resource), texture.width,
                   texture.height, texture.format.Name().c_str(), descriptor.firstMip,
                   descriptor.firstSlice, input.size());
            break;
          }
        for(const auto &buffer : buffers)
          if(buffer.resourceId == descriptor.resource && descriptor.byteOffset < buffer.length)
          {
            const uint64_t available = buffer.length - descriptor.byteOffset;
            const uint64_t count = descriptor.byteSize ? std::min(descriptor.byteSize, available) : available;
            // Diagnostic observation, including potentially undefined padding.
            // This limit and the observed bytes never decide replay eligibility.
            if(count > 1024 * 1024) break;
            const auto input = controller->GetBufferData(descriptor.resource, descriptor.byteOffset, count);
            if(input.size() != count || !controller->GetFatalErrorStatus().OK()) return false;
            snprintf(name, sizeof(name), "%s/input-candidate-cycle-%u-event-%u-buffer-%llu-offset-%llu-bytes-%llu.bin",
                     folder, cycle, target.event, (unsigned long long)Number(descriptor.resource),
                     (unsigned long long)descriptor.byteOffset, (unsigned long long)count);
            FILE *output = fopen(name, "wb");
            if(!output) return false;
            const bool savedInput = fwrite(input.data(), 1, input.size(), output) == input.size();
            fclose(output);
            if(!savedInput) return false;
            printf("INPUT_BUFFER_CANDIDATE cycle=%u event=%u buffer=%llu offset=%llu bytes=%llu\n",
                   cycle, target.event, (unsigned long long)Number(descriptor.resource),
                   (unsigned long long)descriptor.byteOffset, (unsigned long long)count);
            break;
          }
      }
    }
  }
  return controller->GetFatalErrorStatus().OK();
}
int main(int argc, char **argv)
{
  explicitEvents = (argc == 5 || argc == 6) && !strcmp(argv[4], "--compute-events-after");
  computeTargets = explicitEvents ||
      ((argc == 5 || argc == 6) && !strcmp(argv[4], "--compute-inputs-after"));
  const bool afterOnly = computeTargets ||
      ((argc == 5 || argc == 6) && !strcmp(argv[4], "--inputs-after"));
  if(argc != 4 && !afterOnly &&
     !(argc == 5 && (!strcmp(argv[4], "--whole-only") || !strcmp(argv[4], "--inputs")))) return 2;
  const bool wholeOnly = argc == 5 && !strcmp(argv[4], "--whole-only");
  inspectInputs = afterOnly || (argc == 5 && !strcmp(argv[4], "--inputs"));
  if(argc == 6)
  {
    FILE *ranges = fopen(argv[5], "r");
    if(!ranges) return 2;
    unsigned long long encoder, resource, offset, bytes;
    uint64_t total = 0;
    while(fscanf(ranges, "%llu %llu %llu %llu", &encoder, &resource, &offset, &bytes) == 4)
    {
      // Test readback budget only, never a backend support or recovery limit.
      if(!resource || !bytes || bytes > 1024 * 1024 || offset > UINT64_MAX - bytes ||
         inputRanges.size() >= 64 || total > 16 * 1024 * 1024 - bytes)
      { fclose(ranges); return 2; }
      InputRange range; range.encoder = encoder; range.offset = offset; range.bytes = bytes;
      memcpy(&range.buffer, &resource, sizeof(resource)); inputRanges.push_back(range); total += bytes;
    }
    fclose(ranges);
    if(inputRanges.empty()) return 2;
  }
  rdcarray<Target> targets;
  FILE *input = fopen(argv[3], "r");
  if(!input) return 2;
  unsigned long long encoder, resource;
  unsigned mip, slice, requestedEvent = 0;
  while(explicitEvents ?
            fscanf(input, "%llu %llu %u %u %u", &encoder, &resource, &mip, &slice, &requestedEvent) == 5 :
            fscanf(input, "%llu %llu %u %u", &encoder, &resource, &mip, &slice) == 4)
  {
    if(explicitEvents && !requestedEvent) { fclose(input); return 2; }
    Target target;
    target.encoder = encoder; memcpy(&target.texture, &resource, sizeof(resource));
    target.mip = mip; target.slice = slice; target.requestedEvent = requestedEvent;
    targets.push_back(target);
  }
  fclose(input);
  if(targets.empty() || targets.size() > 16) return 2;
  setvbuf(stdout, nullptr, _IONBF, 0);
  GlobalEnvironment env; env.enumerateGPUs = false;
  RENDERDOC_InitialiseReplay(env, {argv[0]});
  auto file = RENDERDOC_OpenCaptureFile();
  auto result = file->OpenFile(argv[1], "rdc", nullptr);
  if(!result.OK()) { file->Shutdown(); RENDERDOC_ShutdownReplay(); return 3; }
  IReplayController *controller = nullptr;
  rdctie(result, controller) = file->OpenCapture(ReplayOptions(), nullptr);
  file->Shutdown();
  if(!result.OK() || !controller) { RENDERDOC_ShutdownReplay(); return 4; }
  uint32_t last = 0;
  if(computeTargets)
    for(const auto &target : targets) targetUsages[target.texture] = controller->GetUsage(target.texture);
  Locate(controller->GetRootActions(), controller->GetStructuredFile(), targets, last);
  // Whole-frame resource observations need no draw/dispatch owner: a legal
  // transfer-only frame may produce the selected texture. Zero encoder is an
  // explicit diagnostic locator, never a production replay qualification.
  if(wholeOnly)
    for(auto &target : targets)
      if(target.encoder == 0) target.event = last;
  std::sort(targets.begin(), targets.end(), [](const Target &a, const Target &b) { return a.event < b.event; });
  int code = 0;
  for(const auto &target : targets)
    if(!target.event) { code = 5; break; }
  for(uint32_t cycle = 0; cycle < 3 && !code; cycle++)
  {
    // Normal OpenCapture already completes the first whole frame. Subsequent
    // cycles replay the original full sequence without intermediate readbacks.
    if(!wholeOnly || cycle) controller->SetFrameEvent(0, true);
    if(wholeOnly && cycle) controller->SetFrameEvent(last, true);
    if(!controller->GetFatalErrorStatus().OK()) { code = 6; break; }
    uint32_t selectedEvent = 0;
    for(const auto &target : targets)
    {
      if(afterOnly && selectedEvent != target.event)
      {
        controller->SetFrameEvent(target.event, true);
        selectedEvent = target.event;
      }
      if(inspectInputs && !afterOnly)
      {
        Target before = target; --before.event;
        if(!Observe(controller, before, argv[2], cycle, false)) { code = 7; break; }
      }
      if(!Observe(controller, target, argv[2], cycle, wholeOnly, afterOnly) ||
         (inspectInputs && !ObserveBuffers(controller, target, argv[2], cycle))) { code = 7; break; }
    }
  }
  rdcstr contents; RENDERDOC_GetLogFileContents(0, contents);
  char name[4096]; snprintf(name, sizeof(name), "%s/renderdoc-final.log", argv[2]);
  if(FILE *log = fopen(name, "wb")) { fwrite(contents.data(), 1, contents.size(), log); fclose(log); }
  controller->Shutdown(); RENDERDOC_ShutdownReplay();
  if(!code) puts("GPU EXECUTION COMPLETE; observations collected; output comparison and overall acceptance pending");
  return code;
}
