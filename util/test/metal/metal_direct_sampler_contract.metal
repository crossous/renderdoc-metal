// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
kernel void direct_sampler_contract(texture2d<float> image [[texture(6)]],
    sampler state [[sampler(3)]], device float4 *output [[buffer(9)]],
    constant uint2 &parameters [[buffer(2)]], uint index [[thread_position_in_grid]])
{
  if(index < 8)
    output[parameters.x + index] = image.sample(state,
        float2(float(index % 4) + 0.75f, float(index / 4) + 0.75f) / float2(4, 2),
        level(float(parameters.y)));
}
