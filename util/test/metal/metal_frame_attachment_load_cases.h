// SPDX-License-Identifier: MIT
#pragma once
#include <cstring>
#include <cstdint>
// Independent expected bytes for the fixed .25/.5/.75/1 shader output.
struct FrameColorCase {const char *name;unsigned format,stride;uint8_t first[16],second[16];};
static const FrameColorCase frameColorCases[]={
 {"initial-r16",20,2,{0,64},{255,191}},
 {"initial-rg8",30,2,{64,128},{191,128}},
 {"initial-r16float",25,2,{0,52},{0,58}},
 {"initial-rg16float",65,4,{0,52,0,56},{0,58,0,56}},
 {"initial-r32float",55,4,{0,0,128,62},{0,0,64,63}},
 {"initial-rg32float",105,8,{0,0,128,62,0,0,0,63},{0,0,64,63,0,0,0,63}},
 {"initial-rgba32float",125,16,{0,0,128,62,0,0,0,63,0,0,64,63,0,0,128,63},{0,0,64,63,0,0,0,63,0,0,128,62,0,0,128,63}},
 {"initial-r8snorm",12,1,{32},{95}},
 {"initial-r16snorm",22,2,{0,32},{255,95}},
 {"initial-rg16unorm",60,4,{0,64,0,128},{255,191,0,128}},
};
static const FrameColorCase *FindFrameColorCase(const char *mode){for(const auto &c:frameColorCases)if(!strcmp(c.name,mode))return &c;return nullptr;}
