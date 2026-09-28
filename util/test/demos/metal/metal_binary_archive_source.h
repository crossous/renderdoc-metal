// SPDX-License-Identifier: MIT
// Identical MSL is used to prepare an archive and to exercise it under capture.
#pragma once

static constexpr const char *kMetalBinaryArchiveSource = R"(
#include <metal_stdlib>
using namespace metal;
[[stitchable]] float archive_visible(float x) { return x * 2.0; }
kernel void archive_probe(device uint *out [[buffer(0)]],
                          uint i [[thread_position_in_grid]]) { out[i] = i + 17; }
vertex float4 archive_vs(uint i [[vertex_id]]) {
  float2 p[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[i],0,1);
}
fragment float4 archive_fs() { return float4(0.2,0.7,0.3,1); }
)";
