// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
#if INDEPENDENT_DEPTH
#define DEPTH_TYPE depth2d<float>
#define SAMPLER_SLOT 6
#define OUTPUT_SLOT 9
#else
#define DEPTH_TYPE depthcube<float>
#define SAMPLER_SLOT 3
#define OUTPUT_SLOT 7
#endif
struct Entry { ulong buffer; DEPTH_TYPE depth; ulong bytes; };
struct Sampling { sampler compare; ulong unused, metadata; };
kernel void depth_compare(device const Entry *table [[buffer(0)]],
    device const Sampling *sampling [[buffer(SAMPLER_SLOT)]],
    device uint *output [[buffer(OUTPUT_SLOT)]], constant uint &stage [[buffer(2)]],
    uint id [[thread_position_in_grid]])
{
  const float reference=id%2?.75f:.25f;
#if INDEPENDENT_DEPTH
  float result=table[0].depth.sample_compare(sampling[0].compare,float2(.5),reference);
#else
  float result=table[0].depth.sample_compare(sampling[0].compare,float3(1,0,0),reference);
#endif
  // Native sampling, conversion and dynamic indexing produce ordinary words.
  output[stage*4+id]=uint(result)+stage*10+id*2;
}
