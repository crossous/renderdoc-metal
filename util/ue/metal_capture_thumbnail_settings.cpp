// SPDX-License-Identifier: MIT
// Test-only preload helper. Use the public in-memory setting; never flush the
// user's config or change RT/device capability predicates.
#include "renderdoc/api/replay/renderdoc_replay.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

__attribute__((constructor)) static void LosslessCaptureThumbnail()
{
  const char *requested = getenv("RENDERDOC_CAPTURE_LOSSLESS_THUMBNAIL");
  if(!requested || strcmp(requested, "1")) return;
  SDObject *setting = RENDERDOC_SetConfigSetting("Capture.IncludeExtendedThumbnail");
  if(!setting || setting->type.basetype != SDBasic::Boolean)
  {
    fprintf(stderr, "Lossless capture setting unavailable\n");
    return;
  }
  setting->data.basic.b = true;
  fprintf(stderr, "Lossless capture thumbnail enabled in memory; user config not flushed\n");
}
