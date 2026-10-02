#include <cstdio>

#include "renderdoc/api/replay/renderdoc_replay.h"

REPLAY_PROGRAM_MARKER()

int main(int argc, char **argv)
{
  if(argc != 2)
    return 2;

  GlobalEnvironment env;
  env.enumerateGPUs = false;
  rdcarray<rdcstr> args;
  args.push_back(argv[0]);
  RENDERDOC_InitialiseReplay(env, args);

  ICaptureFile *file = RENDERDOC_OpenCaptureFile();
  ResultDetails result = file->OpenFile(argv[1], "rdc", NULL);
  if(!result.OK())
  {
    fprintf(stderr, "OpenFile failed (code=%u): %s\n", (uint32_t)result.code,
            result.internal_msg ? result.internal_msg->c_str() : "no detail");
    file->Shutdown();
    RENDERDOC_ShutdownReplay();
    return 3;
  }

  fprintf(stderr, "OpenFile OK; beginning OpenCapture\n");
  fflush(stderr);

  IReplayController *controller = NULL;
  rdctie(result, controller) = file->OpenCapture(ReplayOptions(), NULL);
  file->Shutdown();
  if(!result.OK() || controller == NULL)
  {
    fprintf(stderr, "OpenCapture failed (code=%u): %s\n", (uint32_t)result.code,
            result.internal_msg ? result.internal_msg->c_str() : "no detail");
    RENDERDOC_ShutdownReplay();
    return 4;
  }

  fprintf(stdout, "OpenCapture OK: root_actions=%zu textures=%zu buffers=%zu\n",
          controller->GetRootActions().size(), controller->GetTextures().size(),
          controller->GetBuffers().size());
  controller->Shutdown();
  RENDERDOC_ShutdownReplay();
  return 0;
}
