// SPDX-License-Identifier: MIT
// Read back a tiny captured blit on both sides of a seek across terminal Empty.
#include <cstdio>
#include "renderdoc/api/replay/renderdoc_replay.h"

REPLAY_PROGRAM_MARKER()

static void CollectCopies(const rdcarray<ActionDescription> &actions,
                          rdcarray<const ActionDescription *> &copies)
{
  for(const ActionDescription &action : actions)
  {
    if(action.flags & ActionFlags::Copy)
      copies.push_back(&action);
    CollectCopies(action.children, copies);
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
  rdcarray<const ActionDescription *> copies;
  CollectCopies(controller->GetRootActions(), copies);
  if(copies.size() != 1 || copies[0]->copyDestination == ResourceId()) return 5;
  for(int i = 0; i < 2; i++)
  {
    controller->SetFrameEvent(copies[0]->eventId, true);
    bytebuf bytes = controller->GetBufferData(copies[0]->copyDestination, 0, 4096);
    if(bytes.size() != 4096) return 6;
    for(byte value : bytes)
      if(value != 0x5a) return 7;
  }
  puts("terminal Empty: blit bytes and seek PASS");
  controller->Shutdown();
  RENDERDOC_ShutdownReplay();
  return 0;
}
