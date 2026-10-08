// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct Entry { ulong buffer, texture, length; };
struct Roots { ulong constants, samplers; };
kernel void runtime_clear(constant Entry *heap [[buffer(0)]], constant Roots &roots [[buffer(2)]])
{
  constant uint *settings = (constant uint *)roots.constants;
  device uint *result = (device uint *)heap[settings[0]].buffer;
  result[settings[1]] = settings[2];
}
