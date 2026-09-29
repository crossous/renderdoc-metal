// SPDX-License-Identifier: MIT
#include <cstdio>
#include "renderdoc/api/replay/renderdoc_replay.h"

REPLAY_PROGRAM_MARKER()

static void CollectDispatches(const rdcarray<ActionDescription> &actions,
                              rdcarray<uint32_t> &events)
{
  for(const ActionDescription &action : actions)
  {
    if(action.flags & ActionFlags::Dispatch)
      events.push_back(action.eventId);
    CollectDispatches(action.children, events);
  }
}

int main(int argc, char **argv)
{
  if(argc != 2) return 2;
  GlobalEnvironment env;
  env.enumerateGPUs = false;
  rdcarray<rdcstr> args;
  args.push_back(argv[0]);
  RENDERDOC_InitialiseReplay(env, args);
  ICaptureFile *file = RENDERDOC_OpenCaptureFile();
  ResultDetails result = file->OpenFile(argv[1], "rdc", NULL);
  if(!result.OK()) return 3;
  IReplayController *controller = NULL;
  rdctie(result, controller) = file->OpenCapture(ReplayOptions(), NULL);
  file->Shutdown();
  if(!result.OK() || !controller) return 4;
  ResourceId buffer;
  for(const SDChunk *chunk : controller->GetStructuredFile().chunks)
    if(chunk->name == "MTLHeap::newBuffer(offset)")
    {
      const SDObject *field = chunk->FindChild("Buffer");
      if(field) buffer = field->AsResourceId();
    }
  rdcarray<uint32_t> dispatches;
  CollectDispatches(controller->GetRootActions(), dispatches);
  if(buffer == ResourceId() || dispatches.size() != 2) return 5;
  const uint32_t steps[] = {0, 1, 0, 1};
  for(uint32_t step : steps)
  {
    controller->SetFrameEvent(dispatches[step], true);
    bytebuf bytes = controller->GetBufferData(buffer, 0, 8);
    if(bytes.size() != 8) return 6;
    const uint32_t *values = (const uint32_t *)bytes.data();
    if(values[0] != 8 + step || values[1] != 12 + step)
    {
      fprintf(stderr, "event %u: %u %u\n", dispatches[step], values[0], values[1]);
      return 7;
    }
  }
  printf("frame Shared placement: CPU initial, GPU updates and event seek OK\n");
  controller->Shutdown();
  RENDERDOC_ShutdownReplay();
  return 0;
}
