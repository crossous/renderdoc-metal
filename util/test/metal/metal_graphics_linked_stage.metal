// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct Input { device const uchar *buffer; uint length, stride; };
struct Result { float4 position; uint index; };
[[visible]] Result stage_input(device const Input *inputs,uint vertexIndex,uint relative,uint base)
{
  Result result;
  result.position=*reinterpret_cast<device const float4 *>(inputs[0].buffer+vertexIndex*inputs[0].stride);
  result.index=*reinterpret_cast<device const uint *>(inputs[1].buffer+(relative+base)*inputs[1].stride);
  return result;
}
