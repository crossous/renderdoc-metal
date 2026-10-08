// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct Entry { ulong buffer, texture, length; };
struct Roots { ulong constants, samplers; };
kernel void runtime_clear(const device Entry *heap [[buffer(0)]],
                          constant Roots &roots [[buffer(2)]])
{
  const constant uint *settings=(const constant uint *)roots.constants;
  device uint *target=(device uint *)heap[settings[0]].buffer;
  const device uint *indices=(const device uint *)heap[settings[4]].buffer;
  const device uint *source=(const device uint *)heap[settings[5]].buffer;
  #pragma clang loop unroll(disable)
  for(uint i=0;i<settings[3];i++)
    target[indices[i]]=source[i]+settings[2];
}
