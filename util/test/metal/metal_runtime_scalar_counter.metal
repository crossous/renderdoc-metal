// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct Entry { ulong buffer, texture, length; };
struct Roots { ulong constants, samplers; };
kernel void runtime_counter(constant Entry *heap [[buffer(0)]], constant Roots &roots [[buffer(2)]])
{
  device uint *counter=(device uint *)heap[3].buffer;
  counter[0]=0;
}
kernel void runtime_clear(constant Entry *heap [[buffer(0)]], constant Roots &roots [[buffer(2)]])
{
  constant uint *settings=(constant uint *)roots.constants;
  device uint *counter=(device uint *)heap[3].buffer;
  device uint *result=(device uint *)heap[settings[0]].buffer;
  result[settings[1]+counter[0]]=settings[2];
}
kernel void runtime_counter_conditional(constant Entry *heap [[buffer(0)]], constant Roots &roots [[buffer(2)]])
{
  constant uint *settings=(constant uint *)roots.constants;
  device uint *counter=(device uint *)heap[3].buffer;
  if(settings[3]==0)counter[0]=0;
}
