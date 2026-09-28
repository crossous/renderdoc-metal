// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include <metal_stdlib>
using namespace metal;
struct BinaryParams { uint bias; };
kernel void cs_binary(device uint *result [[buffer(0)]],
                      constant BinaryParams &p [[buffer(1)]], uint id [[thread_position_in_grid]])
{
  result[id] = p.bias + id;
}
vertex float4 vs_binary(uint id [[vertex_id]])
{
  const float2 positions[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(positions[id], 0, 1);
}
fragment float4 fs_binary(const device uint *result [[buffer(0)]])
{
  return float4(result[0], result[32], result[64], 255) / 255.0;
}
