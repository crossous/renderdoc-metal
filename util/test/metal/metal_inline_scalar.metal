// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct Packet { device const uint *input; ulong unused; ulong bytes; };
struct Scalar { ulong ordinary; ulong salt; uint offset; uint count; };
struct Wide { ulong ordinary; ulong salt; uint offset; uint count; ulong extra; ulong tail; };
kernel void scalar(constant Packet &packet [[buffer(30)]], constant Scalar &p [[buffer(2)]],
                   device uint *out [[buffer(9)]], uint i [[thread_position_in_grid]]) {
  if(i<p.count)out[p.offset+i]=packet.input[i]*7+i+uint(p.ordinary)^uint(p.salt>>32);
}
kernel void wide_scalar(constant Packet &packet [[buffer(30)]], constant Wide &p [[buffer(2)]],
                        device uint *out [[buffer(9)]], uint i [[thread_position_in_grid]]) {
  if(i<p.count)out[p.offset+i]=(packet.input[i]*11+i+uint(p.ordinary))^uint(p.salt)^uint(p.extra>>32)^uint(p.tail);
}

struct Pointer { device const uint *input; };
kernel void pointer_scalar(constant Pointer &p [[buffer(2)]], device uint *out [[buffer(9)]],
                           uint i [[thread_position_in_grid]]) {
  out[88+i]=p.input[i]*19+i;
}
