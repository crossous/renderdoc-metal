// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
kernel void exact_grid(const device uint *input [[buffer(4)]],
    device uint *output [[buffer(9)]], constant uint4 &shape [[buffer(2)]],
    uint3 tid [[thread_position_in_grid]])
{
  // The API dispatches exactly this grid. Padding remains a canary: rounding
  // to full groups would change it rather than create an out-of-bounds access.
  uint index = tid.x + shape.x * (tid.y + shape.y * tid.z);
  output[shape.w + index] = input[index] * 7 + index * 11 + 101;
}

// Dynamic allocations are encoder state. Their contents and the local barrier
// execute on the GPU, including non-uniform edge groups.
kernel void dynamic_grid(const device uint *input [[buffer(4)]],
    device uint *output [[buffer(9)]], constant uint4 &extent [[buffer(2)]],
    threadgroup uint *scratch [[threadgroup(3)]],
    threadgroup uint *mirror [[threadgroup(7)]],
    uint3 tid [[thread_position_in_grid]], uint3 local [[thread_position_in_threadgroup]],
    uint3 actual [[threads_per_threadgroup]])
{
  threadgroup uint sharedBias;
  uint i=tid.x+extent.x*(tid.y+extent.y*tid.z);
  uint lane=local.x+actual.x*(local.y+actual.y*local.z);
  scratch[lane]=input[i];mirror[lane]=input[i]+101;
  if(lane==0)sharedBias=input[i]%13;
  threadgroup_barrier(mem_flags::mem_threadgroup);
  uint next=(lane+1)%(actual.x*actual.y*actual.z);
  output[extent.w+i]=scratch[next]*7+i*11+mirror[lane]+sharedBias;
}
