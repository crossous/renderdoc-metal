// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct Input { device const uchar *buffer; uint length, stride; };
struct Result { float4 position; uint index; };
[[visible]] Result stage_input(device const Input *, uint, uint, uint);
struct Entry { device const float *buffer; ulong texture, length; };
struct Root { device const uint *settings; };
#if POINTER_SELECTION_ORIGINS
__attribute__((noinline)) static float guarded_depth(device const float *p, uint index, bool enabled)
{
  if(enabled)return p[index];
  return index==1?.25f:.75f;
}
#endif
vertex float4 linked_vertex(uint vertexIndex [[vertex_id]], uint instance [[instance_id]],
    uint base [[base_instance]], device const Input *inputs [[buffer(6)]],
    device const Root &root [[buffer(2)]], device const Entry *heap [[buffer(0)]])
{
  Result value=stage_input(inputs,vertexIndex,instance-base,base);
#if POINTER_SELECTION_ORIGINS
  const Entry selected=heap[root.settings[0]];
  const bool enabled=root.settings[1]<selected.length/sizeof(float);
  device const float *pointer=enabled?selected.buffer:nullptr;
  value.position.z=guarded_depth(pointer,value.index,enabled);
#else
  value.position.z=heap[root.settings[0]].buffer[value.index];
#endif
  return value.position;
}
#if EARLY_FRAGMENT
#if INDEPENDENT_FRAGMENT
#define FRAGMENT_SLOT 11
#else
#define FRAGMENT_SLOT 3
#endif
#if LOCAL_FRAGMENT_HELPER
__attribute__((noinline)) static void fragment_control(bool discard)
{
  if(discard)discard_fragment();
}
#endif
[[early_fragment_tests]]
fragment float4 linked_fragment(float4 position [[position]],
    device const Entry *heap [[buffer(FRAGMENT_SLOT)]])
{
  // Original Native interpolation/conversion/indexing selects ordinary data.
  // Its value need not be available to CPU access display.
#if LOCAL_FRAGMENT_HELPER
  fragment_control(position.x < -1.0f);
#endif
  uint index=1+(uint(position.x)%2);
  return float4(heap[1].buffer[index],.5,.75,1);
}
#endif

#if GPU_INDEX_PRODUCER
#if INDEPENDENT_FRAGMENT
#define GENERATED_INDEX uint
#define GENERATED_INDEX_SLOT 10
#else
#define GENERATED_INDEX ushort
#define GENERATED_INDEX_SLOT 8
#endif
// Ordinary GPU index values remain Native data, including register-only math.
kernel void native_indices(device GENERATED_INDEX *indices [[buffer(GENERATED_INDEX_SLOT)]],
    constant uint &rotation [[buffer(2)]], uint id [[thread_position_in_grid]])
{
  indices[id]=GENERATED_INDEX((popcount(1u<<id)+id+rotation)%3u);
}
#endif
