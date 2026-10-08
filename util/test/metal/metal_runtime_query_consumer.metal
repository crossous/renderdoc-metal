// SPDX-License-Identifier: MIT
#include <metal_stdlib>
#include <metal_raytracing>
using namespace metal;
using namespace metal::raytracing;
struct Entry { ulong buffer, texture, length; };
struct Roots { ulong constants, samplers; };
// Public runtime header ABI: Native AS identity and metadata buffer pointer.
struct PublicASHeader { instance_acceleration_structure structure; constant uint *contributions; };
#ifdef QUERY_CONTRIBUTION_PRODUCER
kernel void runtime_contribution_producer(device uint *out [[buffer(QUERY_CONTRIBUTION_SLOT)]],
                                         constant uint &count [[buffer(QUERY_CONTRIBUTION_COUNT_SLOT)]],
                                         uint id [[thread_position_in_grid]])
{
  // Ordinary Native data, not a GPU address publication or CPU result fact.
  if(id<count)out[id]=clz(1u<<id)+id-31u;
}
#endif
#ifdef QUERY_NULL_ROWS
kernel void runtime_null_unqualified_writer(device Entry *heap [[buffer(0)]])
{
  heap[3].buffer=0;
}
#endif
#ifdef QUERY_PARTIAL_HEAP
kernel void runtime_partial_gpu_producer(constant Roots &roots [[buffer(QUERY_PRODUCER_SLOT)]],
                                        uint3 group [[threadgroup_position_in_grid]])
{
  // The original GPU computes this value. Capture/replay must restore the
  // written byte interval without a CPU implementation of the intrinsic.
  device uint *output=(device uint *)roots.constants;
  output[QUERY_PRODUCER_BYTE_OFFSET/4u]=3033u+clz(group.x+1u)-31u;
}
#endif
kernel void runtime_clear(constant Entry *heap [[buffer(0)]], constant Roots &roots [[buffer(2)]]
#ifdef QUERY_GROUP_INDICES
                          , uint3 group [[threadgroup_position_in_grid]]
#endif
                          )
{
  constant uint *settings=(constant uint *)roots.constants;
  device PublicASHeader *header=(device PublicASHeader *)heap[0].buffer;
  constant uint * volatile metadata=header->contributions;
#ifdef QUERY_POINTER_BRANCHES
  // Both stores carry restored pointer origins; the GPU chooses the live one.
  if(settings[0]==1)metadata=header->contributions+1;
#endif
  ray r; r.origin=float3(settings[0]==1?0.0f:10.0f,0.0f,1.0f);
  r.direction=float3(0,0,-1);r.min_distance=0;r.max_distance=2;
  intersection_query<triangle_data,instancing> query;
  query.reset(r,header->structure);
#ifdef QUERY_REGISTER_API
  bool attributes=true;
  bool candidateFacing=false;
#endif
  while(query.next())
    if(query.get_candidate_intersection_type()==intersection_type::triangle)
    {
#ifdef QUERY_REGISTER_API
      const float2 bary=query.get_candidate_triangle_barycentric_coord();
      const float4x3 world=query.get_candidate_object_to_world_transform();
      const float4x3 object=query.get_candidate_world_to_object_transform();
      attributes &= query.get_candidate_geometry_id()==0 && query.get_candidate_primitive_id()==0 &&
          query.get_candidate_user_instance_id()==0 && query.get_candidate_instance_id()<3 &&
          abs(query.get_candidate_triangle_distance()-1.0f)<0.0001f &&
          all(bary>=0.0f) && bary.x+bary.y<=1.0f &&
          distance(world*float4(query.get_candidate_ray_origin(),1.0f),r.origin)<0.0001f &&
          distance(object*float4(r.direction,0.0f),query.get_candidate_ray_direction())<0.0001f;
      candidateFacing=query.is_candidate_triangle_front_facing();
#endif
      query.commit_triangle_intersection();
    }
  const bool hit=query.get_committed_intersection_type()==intersection_type::triangle;
  bool validHit=hit && abs(query.get_committed_distance()-1.0f)<0.0001f &&
      metadata[query.get_committed_instance_id()]==0;
#ifdef QUERY_DYNAMIC_HEAP
  // The real GPU query result selects a buffer descriptor. No CPU fact can
  // select this row; both possible buffers require valid restored contents.
  if(hit)
  {
#ifdef QUERY_GROUP_INDICES
    const uint row=1+group.x;
#elif defined(QUERY_NULL_ROWS)
    const uint row=3+query.get_committed_instance_id()%QUERY_NULL_ROWS;
#elif defined(QUERY_PARTIAL_HEAP)
    const uint row=1+query.get_committed_instance_id()%3u;
#else
    const uint row=1+(query.get_committed_instance_id()&1u);
#endif
#ifdef QUERY_SWAPPED_INITIAL
    const uint expectedWord=row==2?1011u:1011u*row;
#else
#if defined(QUERY_CREATION_ZERO)
    const uint expectedWord=row==3?0u:1011u*row;
#else
    const uint expectedWord=1011u*row;
#endif
#endif
#ifdef QUERY_NULL_ROWS
    device uint *selected=(device uint *)heap[row].buffer;
    validHit &= selected?selected[63]==expectedWord:row>=3;
#elif defined(QUERY_PARTIAL_HEAP)
    const uint word=row==3?QUERY_PARTIAL_OFFSET/4u:63u;
#ifdef QUERY_GROUP_INDICES
    // Separate narrow and wide reads exercise independent restoration widths.
    validHit &= ((volatile device ushort *)heap[row].buffer)[word*2u]==ushort(expectedWord);
#endif
    validHit &= ((device uint *)heap[row].buffer)[word]==expectedWord;
#else
    validHit &= ((device uint *)heap[row].buffer)[63]==expectedWord;
#endif
  }
#endif
#ifdef QUERY_REGISTER_API
  if(hit)
  {
    const float2 bary=query.get_committed_triangle_barycentric_coord();
    const float4x3 world=query.get_committed_object_to_world_transform();
    const float4x3 object=query.get_committed_world_to_object_transform();
    validHit &= attributes && query.get_committed_geometry_id()==0 && query.get_committed_primitive_id()==0 &&
        query.get_committed_user_instance_id()==0 && query.is_committed_triangle_front_facing()==candidateFacing &&
        all(bary>=0.0f) && bary.x+bary.y<=1.0f &&
        distance(world*float4(query.get_committed_ray_origin(),1.0f),r.origin)<0.0001f &&
        distance(object*float4(r.direction,0.0f),query.get_committed_ray_direction())<0.0001f &&
        distance(query.get_world_space_ray_origin(),r.origin)<0.0001f &&
        distance(query.get_world_space_ray_direction(),r.direction)<0.0001f && query.get_ray_min_distance()==0.0f;
  }
#endif
  // The GPU output depends on a real hit/distance/instance metadata lookup.
  // A missed expected hit or a hit on the missed ray corrupts the full oracle.
  ((device uint *)heap[settings[0]].buffer)[settings[1]
#ifdef QUERY_GROUP_INDICES
      +group.x
#endif
      ]=settings[2]+
      ((settings[0]==1?validHit:!hit)?0u:100000u);
}
