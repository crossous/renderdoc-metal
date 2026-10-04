// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct V { float4 p [[position]]; };
struct C { half4 c [[color(0), raster_order_group(2)]]; };
struct MRT { half4 c [[color(0), raster_order_group(2)]]; half4 copy [[color(1)]]; };
vertex V vs(uint i [[vertex_id]]) {
  float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)}; return {float4(p[i],0,1)};
}
fragment C seed() { return {half4(.25h,.25h,.25h,1)}; }
fragment MRT fetch(V v [[stage_in]], C old, constant half4 &delta [[buffer(0)]]) {
  half4 c=old.c+delta; return {c,c};
}
fragment half4 plain() { return half4(.75h,.5h,.25h,1); }
// A real implicit imageblock kernel, plus buffer, inline constant and tile memory bindings.
kernel void tile_image(imageblock<C,imageblock_layout_implicit> img,
  ushort2 p [[thread_position_in_threadgroup]], device atomic_uint *counter [[buffer(0)]],
  constant half &delta [[buffer(1)]], threadgroup half *scratch [[threadgroup(0)]]) {
  if(all(p==ushort2(0))) scratch[0]=delta;
  threadgroup_barrier(mem_flags::mem_threadgroup);
  C c=img.read(p); c.c.x+=scratch[0]; img.write(c,p);
  if(all(p==ushort2(0))) atomic_fetch_add_explicit(counter,1u,memory_order_relaxed);
}
