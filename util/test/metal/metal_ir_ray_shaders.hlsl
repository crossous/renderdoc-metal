// SPDX-License-Identifier: MIT
// Small DXR pipeline to exercise the same IR runtime ABI used by MetalRHI.
RaytracingAccelerationStructure scene : register(t0);
RWByteAddressBuffer output : register(u0);
struct Payload { uint value; };

#ifdef GLOBAL_ROOT
cbuffer GlobalValues : register(b0,space2) { uint globalBase; };
cbuffer GlobalConstants : register(b1,space2) { uint globalAdd; uint globalX; uint globalY; uint globalZ; };
ByteAddressBuffer globalData : register(t1,space2);
Texture2D<float> globalTexture : register(t2,space2);
SamplerState globalSampler : register(s3,space2);
uint GlobalResult()
{
  return globalBase+globalAdd+globalData.Load(0)+
      (uint)round(globalTexture.SampleLevel(globalSampler,float2(0.25,0.5),0));
}
#endif

[shader("raygeneration")]
void raygen()
{
  uint index = DispatchRaysIndex().x;
  RayDesc ray;
  ray.Origin = float3(index == 0 ? 0 : 100, 0, -2);
  ray.Direction = float3(0, 0, 1);
  ray.TMin = 0;
  ray.TMax = 1000;
  const uint geometryMultiplier =
#ifdef MULTI_GEOMETRY
    0;
#else
    1;
#endif
  Payload payload;
  payload.value = 7;
#ifdef HEAP_AS
  RaytracingAccelerationStructure heapScene = ResourceDescriptorHeap[2];
  TraceRay(heapScene, RAY_FLAG_NONE, 0xff, 0, geometryMultiplier, 0, ray, payload);
#else
  TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, geometryMultiplier, 0, ray, payload);
#endif
#ifdef DIRECT_HEAPS
  ByteAddressBuffer heapData = ResourceDescriptorHeap[index == 0 ? 0 : 3];
  Texture2D<float> heapTexture = ResourceDescriptorHeap[1];
  SamplerState heapSampler = SamplerDescriptorHeap[1];
  uint heapValue=heapData.Load(0)+(uint)round(heapTexture.SampleLevel(heapSampler,float2(0.25,0.5),0));
#ifdef GLOBAL_ROOT
  heapValue += GlobalResult();
#endif
  output.Store(index*4,payload.value+heapValue);
#elif defined(GLOBAL_ROOT)
  output.Store(index*4,payload.value+GlobalResult());
#else
  output.Store(index * 4, payload.value);
#endif
}

#ifdef LOCAL_ROOT
cbuffer LocalValues : register(b0, space1) { uint localBase; };
cbuffer LocalConstants : register(b1, space1) { uint localAdd; uint localX; uint localY; uint localIgnore; };
ByteAddressBuffer localData : register(t1, space1);
Texture2D<float> localTexture : register(t2, space1);
#ifdef STATIC_SIX
SamplerState localSampler : register(s3, space1);
#else
SamplerState localSampler : register(s0, space1);
#endif
uint LocalResult()
{
  return localBase + localAdd + localData.Load(0) +
      (uint)round(localTexture.SampleLevel(localSampler,float2(0.25,0.5),0));
}
#endif

[shader("closesthit")]
void closest_hit(inout Payload payload, BuiltInTriangleIntersectionAttributes attributes)
{
#ifdef LOCAL_ROOT
  payload.value = LocalResult();
#else
  payload.value = 73;
#endif
#ifdef INDIRECT_AS
  payload.value += InstanceID();
#endif
#ifdef MULTI_GEOMETRY
  payload.value += GeometryIndex()*17;
#endif
}

[shader("miss")]
void miss(inout Payload payload)
{
#ifdef LOCAL_ROOT
  payload.value = LocalResult();
#else
  payload.value = 11;
#endif
}

[shader("anyhit")]
void any_hit(inout Payload payload, BuiltInTriangleIntersectionAttributes attributes)
{
#ifdef LOCAL_ROOT
  if(localIgnore && LocalResult() == 131) IgnoreHit();
#else
  IgnoreHit();
#endif
}
