// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
kernel void produce(device atomic_uint *count [[buffer(0)]],device uint *records [[buffer(1)]],device uint *map [[buffer(2)]],device const uint *salt [[buffer(4)]],uint i [[thread_position_in_grid]]) {
 uint position=atomic_fetch_add_explicit(count,1u,memory_order_relaxed); records[position]=i ^ (*salt-1);map[i]=position;
}
kernel void consume(device const uint *records [[buffer(1)]],device const uint *map [[buffer(2)]],device uint *out [[buffer(3)]],device const uint *salt [[buffer(4)]],uint i [[thread_position_in_grid]]) {out[i]=records[map[i]]==i?*salt:0u;}
