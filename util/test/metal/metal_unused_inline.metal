// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
kernel void ordinary_direct(const device uint *input [[buffer(0)]],
    device uint *output [[buffer(1)]], constant uint *parameters [[buffer(2)]],
    uint tid [[thread_position_in_grid]])
{
  if(tid < parameters[0])
    output[parameters[1] + (tid * 17) % parameters[0]] =
        input[(tid * 13) % parameters[0]] + parameters[2];
}
// This variant actually consumes an inline GPU address. A capture without its
// identity/layout must fail restoration, even though Native execution succeeds.
kernel void active_address(constant ulong &address [[buffer(0)]],
    device uint *output [[buffer(1)]], constant uint *parameters [[buffer(2)]],
    uint tid [[thread_position_in_grid]])
{
  const device uint *input = reinterpret_cast<const device uint *>(address);
  if(tid < parameters[0])
    output[parameters[1] + (tid * 17) % parameters[0]] =
        input[(tid * 13) % parameters[0]] + parameters[2];
}
