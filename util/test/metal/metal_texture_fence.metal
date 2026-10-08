// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
#if CONSTANT_TABLE
#if CONSTANT_VARIANT
constant uint Palette[]={41,43,47,53,59,61,67,71,73,79,83};
#else
constant uint Palette[]={11,13,17,19,23,29};
#endif
#endif
static inline uint PaletteOutput(uint gpuValue,uint stage)
{
#if CONSTANT_TABLE
  // Index comes from a Native texture read; restoration must not calculate it.
  return gpuValue+Palette[(gpuValue^(stage*13))%(sizeof(Palette)/sizeof(Palette[0]))];
#else
  return gpuValue;
#endif
}
#if INTEGER_BIT_PATTERN
using Word=int;
#else
using Word=uint;
#endif
struct Entry2D { ulong zero; texture2d<Word,access::read_write> image; ulong metadata; };
struct EntryArray { ulong zero; texture2d_array<float,access::read_write> image; ulong metadata; };
kernel void fence_2d(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]], uint tid [[thread_position_in_grid]])
{
  auto image=((const device Entry2D *)root[0])->image;
  const uint value=uint(root[1])*100+tid+31
#if INTEGER_BIT_PATTERN
      +0x80000000u
#endif
      ;
  image.write(vec<Word,4>(value),uint2(tid,0));
  image.fence();
  output[root[1]*4+tid]=PaletteOutput(uint(image.read(uint2(tid,0)).x),uint(root[1]));
}
kernel void fence_array(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]], uint tid [[thread_position_in_grid]])
{
  auto image=((const device EntryArray *)root[0])->image;
  const uint slice=tid%image.get_array_size();
  const uint value=uint(root[1])*100+tid+31;
  image.write(float4(value),uint2(tid,0),slice);
  image.fence();
  output[root[1]*4+tid]=PaletteOutput(uint(image.read(uint2(tid,0),slice).x),uint(root[1]));
}
