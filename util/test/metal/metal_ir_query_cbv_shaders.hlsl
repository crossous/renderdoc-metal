// SPDX-License-Identifier: MIT
// Real converted CBV-root compute: every root affects ray or UAV data.
cbuffer SceneControls : register(b0) { uint sceneSlot; uint rowWidth; uint queryCount; };
cbuffer OutputControls : register(b1) { uint outputSlot; uint resultStride; };
cbuffer OriginControls : register(b2) { float originZ; float directionZ; };
cbuffer RangeControls : register(b3) { float fullRange; float shortRange; };
cbuffer HitControls : register(b4) { float missX; float secondHitX; uint hitBias; };
#if CBV_ROOT_COUNT == 6
cbuffer ExtraControls : register(b5) { uint extraBias; };
#endif
[numthreads(4, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
  const uint index = id.x + rowWidth * id.y;
  if(index >= queryCount) return;
  const uint lane = index & 3;
  RaytracingAccelerationStructure scene = ResourceDescriptorHeap[sceneSlot];
  RWByteAddressBuffer results = ResourceDescriptorHeap[outputSlot];
  RayDesc ray;
  ray.Origin = float3(lane == 2 ? missX : lane == 1 ? secondHitX : 0.0, 0.0, originZ);
  ray.Direction = float3(0.0, 0.0, directionZ);
  ray.TMin = 0.0;
  ray.TMax = lane == 3 ? shortRange : fullRange;
  RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
  query.TraceRayInline(scene, 0, 0xff, ray);
  while(query.Proceed()) {}
  uint bias = hitBias;
#if CBV_ROOT_COUNT == 6
  bias += extraBias;
#endif
  results.Store(index * resultStride, query.CommittedStatus() == COMMITTED_TRIANGLE_HIT ?
      uint(query.CommittedRayT() * 1000.0) + query.CommittedPrimitiveIndex() +
      query.CommittedInstanceID() * 17 + query.CommittedGeometryIndex() * 13 + bias : 0);
}
