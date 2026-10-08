// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct Entry2D { ulong zero [[id(0)]]; texture2d<uint> stencil [[id(1)]]; ulong metadata [[id(2)]]; };
struct EntryArray { ulong zero [[id(0)]]; texture2d_array<uint> stencil [[id(1)]]; ulong metadata [[id(2)]]; };
kernel void read_2d(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]], uint tid [[thread_position_in_grid]])
{
  const device Entry2D *table=reinterpret_cast<const device Entry2D *>(root[0]);
  output[root[1]*4+tid]=table->stencil.read(uint2(tid,tid)).x;
}
kernel void read_array(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]], uint tid [[thread_position_in_grid]])
{
  const device EntryArray *table=reinterpret_cast<const device EntryArray *>(root[0]);
  const uint mip=tid%table->stencil.get_num_mip_levels();
  const uint slice=tid%table->stencil.get_array_size();
  output[root[1]*4+tid]=table->stencil.read(uint2(tid,tid),slice,mip).x;
}
