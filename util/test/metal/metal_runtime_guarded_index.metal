// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct Entry { ulong buffer, texture, length; };
#if TEXTURE_DIMENSIONS
#if TEXTURE_BUFFER_VIEW
struct TextureEntry { ulong buffer; texture_buffer<uint,access::read_write> texture; ulong length; };
#elif TEXTURE_ATOMICS
struct TextureEntry { ulong buffer; texture2d<uint,access::read_write> texture; ulong length; };
#else
struct TextureEntry { ulong buffer; texture2d<uint,access::read> texture; ulong length; };
#endif
#endif
struct Roots { ulong constants, samplers; };
#if NATIVE_SAMPLER_HEAP
struct SamplerEntry { sampler state; ulong padding, length; };
struct SamplingEntry { ulong buffer; texture2d<uint,access::sample> texture; ulong length; };
#endif
#if TEXTURE_ATOMICS
kernel void runtime_texture_producer(constant Entry *heap [[buffer(0)]], constant Roots &roots [[buffer(2)]],
                                     uint local [[thread_index_in_threadgroup]])
{
  auto image=((constant TextureEntry *)heap)[4].texture;
  image.atomic_store(uint4(0x12345+local,0,0,0),uint2(local%8,local/8));
}
#endif
kernel void runtime_counter(constant Entry *heap [[buffer(0)]], constant Roots &roots [[buffer(2)]])
{
#if CALL_EFFECTS
  atomic_store_explicit((device atomic_uint *)heap[3].buffer,0,memory_order_relaxed);
#else
  ((device uint *)heap[3].buffer)[0]=0;
#endif
}
kernel void runtime_clear(constant Entry *heap [[buffer(0)]], constant Roots &roots [[buffer(2)]],
#if NATIVE_SAMPLER_HEAP
                          constant SamplerEntry *samplerHeap [[buffer(1)]],
#endif
                          uint local [[thread_index_in_threadgroup]])
{
  constant uint *settings=(constant uint *)roots.constants;
#if TEXTURE_DIMENSIONS
  auto dimensions=((constant TextureEntry *)heap)[4].texture;
#if TEXTURE_BUFFER_VIEW
  if(dimensions.get_width()!=64)return;
#if TEXTURE_PIXEL_READ
#if PARTIAL_VIEW_INIT
  // Only four texels have an explicit pre-dispatch upload. Each reader writes
  // its own texel afterwards, so no cross-invocation read/write race is needed.
  if(local<4)
  {
    const uint expected=settings[0]==1?0x09090909U:0x12346U+local;
    if(dimensions.read(local).x!=expected)return;
  }
#else
  if(dimensions.read(local).x==0xffffffff)return;
#endif
#endif
  dimensions.write(uint4(0x12345+local+settings[0],0,0,0),local);
#else
  if(dimensions.get_width()!=8 || dimensions.get_height()!=8)return;
#if TEXTURE_ATOMICS
  const uint imageIndex=dimensions.atomic_load(uint2(local%8,local/8)).x-(0x12345+local);
  dimensions.atomic_fetch_add(uint2(local%8,local/8),uint4(1,0,0,0));
#if NATIVE_SAMPLER_HEAP
  threadgroup_barrier(mem_flags::mem_texture);
  const uint gathered=((constant SamplingEntry *)heap)[4].texture.gather(
      samplerHeap[settings[0]-1].state,float2(0)).x;
  if(gathered!=0x12346U)return;
#endif
#endif
#if TEXTURE_PIXEL_READ
  if(dimensions.read(uint2(0)).x==0xffffffff)return;
#endif
#endif
#endif
  const uint q=local/settings[3];
#if CALL_EFFECTS
  threadgroup uint counters[64];
#if NULLABLE_SOURCE
  const uint counterIndex=
#if TEXTURE_ATOMICS
      imageIndex;
#else
      ((device uint *)heap[3].buffer)[0];
#endif
  device uint *counter=counterIndex<heap[3].length/4?(device uint *)heap[3].buffer:nullptr;
  counters[local]=counter[counterIndex];
#else
  counters[local]=((device uint *)heap[3].buffer)[0];
#endif
  threadgroup_barrier(mem_flags::mem_threadgroup);
  simdgroup_barrier(mem_flags::mem_threadgroup);
#if THREADGROUP_ATOMICS
  threadgroup atomic_uint allocator;
  if(local==0)atomic_store_explicit(&allocator,counters[0],memory_order_relaxed);
  threadgroup_barrier(mem_flags::mem_threadgroup);
  counters[local]=atomic_fetch_add_explicit(&allocator,0u,memory_order_relaxed);
  threadgroup_barrier(mem_flags::mem_threadgroup);
#endif
#endif
#if NATIVE_LANES
  const uint laneIndex=simd_broadcast_first(counters[local]);
  const uint active=popcount(uint(simd_vote::vote_t(simd_ballot(true))));
  const bool first=simd_is_first();
#endif
  if(q<1)
  {
#if CALL_EFFECTS
    const uint index=
#if NATIVE_LANES
#if REGISTER_BIT_EFFECTS
        // The fixture's actual GPU counter is 0 then 1. Native bit counting
        // maps both to their original index, without a CPU shader evaluator.
        31u-clz(laneIndex+1u);
#else
        laneIndex;
#endif
    if(!first || !active)return;
#else
        counters[local];
#endif
#else
    const uint index=((device uint *)heap[3].buffer)[q];
#endif
    device uint *output=(device uint *)heap[settings[0]].buffer;
    const uint offset=settings[1]+local+settings[3]*index;
#if POINTER_SELECT
    // The Native valid path is data-dependent; CPU index/coverage analysis
    // cannot determine this predicate. This literal is not a captured GPU VA.
    device uint *target=offset<heap[settings[0]].length/4?output+offset:(device uint *)1024;
#if CALL_EFFECTS
    const uint result=uint(max(abs(float(settings[2])-1024.0f),0.0f))
#if READ_MODIFY_WRITE
        + (output[settings[1]] & 3u)
#endif
        ;
#if READ_MODIFY_WRITE
    // One invocation owns this cell. Preserve a real plain read/modify/write
    // as well as the dynamic atomic target; neither numeric result is an ABI.
    output[settings[1]]=result;
#endif
    atomic_exchange_explicit((device atomic_uint *)target,result,memory_order_relaxed);
#if LITERAL_PROJECTION
    target[1]=result+1;
    target[2]=result+2;
    target[3]=result+3;
#endif
#else
    *target=settings[2];
#endif
#else
    output[offset]=settings[2];
#endif
  }
}
