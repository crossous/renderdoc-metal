// SPDX-License-Identifier: MIT
#ifdef HEAP_AS
cbuffer QueryControls : register(b7, space3) { uint sceneSlot; uint reserved; };
#else
RaytracingAccelerationStructure scene : register(t0);
#endif
RWStructuredBuffer<uint> results : register(u0);

[numthreads(4, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
  const uint lane = id.x & 3;
#ifdef HEAP_AS
  RaytracingAccelerationStructure scene = ResourceDescriptorHeap[sceneSlot];
#endif
  RayDesc ray;
  ray.Origin = float3(lane == 2 ? 3.0 : lane == 1 ? 0.25 : 0.0, 0.0, -1.0);
  ray.Direction = float3(0.0, 0.0, 1.0);
  ray.TMin = 0.0;
  ray.TMax = lane == 3 ? 0.5 : 2.0;
  RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
  query.TraceRayInline(scene, 0, 0xff, ray);
  while(query.Proceed()) {}
  results[id.x] = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT ?
      uint(query.CommittedRayT() * 1000.0) + query.CommittedPrimitiveIndex() +
      query.CommittedInstanceID() * 17 + query.CommittedGeometryIndex() * 13 : 0;
}
