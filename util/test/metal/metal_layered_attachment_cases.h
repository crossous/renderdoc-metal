// SPDX-License-Identifier: MIT
#pragma once
#include <cstring>
struct LayeredAttachmentCase
{
  const char *name; bool volume, frame; unsigned format, stride, width, height, depth, mips;
  unsigned level, firstLayer, layers;
};
static const LayeredAttachmentCase LayeredAttachmentCases[] = {
  {"volume", true, false, 115, 8, 16, 12, 8, 2, 0, 0, 8},
  {"array-mip", false, true, 70, 4, 29, 17, 5, 3, 1, 1, 2},
  {"volume-plane", true, false, 55, 4, 21, 13, 10, 3, 1, 1, 3},
};
static const LayeredAttachmentCase *FindLayeredAttachmentCase(const char *name)
{ for(const auto &c : LayeredAttachmentCases) if(!strcmp(c.name, name)) return &c; return nullptr; }
