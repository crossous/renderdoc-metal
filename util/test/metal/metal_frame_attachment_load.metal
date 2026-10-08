// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct MRT {float4 a [[color(0)]];float4 b [[color(1)]];float4 c [[color(2)]];};
vertex float4 load_first(uint id [[vertex_id]]) {
  const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],.25,1);
}
vertex float4 load_second(uint id [[vertex_id]]) {
  const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],.75,1);
}
fragment MRT color_first(){return {float4(.25,.5,.75,1),float4(.75,.25,.5,1),float4(.5,.75,1,1)};}
fragment MRT color_second(){return {float4(.75,.5,.25,1),float4(.5,.75,.25,1),float4(1,.5,.25,1)};}
