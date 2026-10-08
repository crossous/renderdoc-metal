// SPDX-License-Identifier: MIT
// Mixed converted compute roots: 4/5 CBVs and the static sampler table.
cbuffer SceneControls : register(b0) { uint sceneSlot; uint rowWidth; uint queryCount; uint textureSlot; };
cbuffer OutputControls : register(b1) { uint outputSlot; uint resultStride; uint secondarySlot; };
cbuffer OriginControls : register(b2) { float originZ; float directionZ; float missX; float secondHitX; };
cbuffer RangeControls : register(b3) { float fullRange; float shortRange; uint hitBias; };
#if CBV_ROOT_COUNT == 5
cbuffer ExtraControls : register(b4) { uint extraBias; };
#endif
SamplerState s0 : register(s0); SamplerState s1 : register(s1);
SamplerState s2 : register(s2); SamplerState s3 : register(s3);
SamplerState s4 : register(s4); SamplerState s5 : register(s5);
[numthreads(4, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
  const uint index=id.x+rowWidth*id.y;
  if(index>=queryCount) return;
  const uint lane=index&3;
  RaytracingAccelerationStructure scene=ResourceDescriptorHeap[sceneSlot];
#if TEXTURE_UAV == 1
  RWTexture2D<uint> results=ResourceDescriptorHeap[outputSlot];
#elif TEXTURE_UAV == 2
  RWTexture2D<float4> results=ResourceDescriptorHeap[outputSlot];
#elif TEXTURE_UAV == 3
  RWBuffer<uint> results=ResourceDescriptorHeap[outputSlot];
  Buffer<uint> typedInput=ResourceDescriptorHeap[4];
  uint inputCount;typedInput.GetDimensions(inputCount);
  if(inputCount!=4)return;
#else
  RWByteAddressBuffer results=ResourceDescriptorHeap[outputSlot];
#endif
  Texture2D<float4> texture=ResourceDescriptorHeap[textureSlot];
#ifdef TEXTURE_DIMENSIONS
  uint textureWidth,textureHeight;texture.GetDimensions(textureWidth,textureHeight);
  if(!textureWidth || !textureHeight)return;
#endif
#ifdef MULTI_UAV
  RWTexture2D<float4> secondary=ResourceDescriptorHeap[secondarySlot];
  float2 uv=float2(0.125,2.0/queryCount);
#else
  float2 uv=float2(0.25,0.75);
#endif
  uint sampled=uint(round(255.0*(texture.SampleLevel(s0,uv,0).r+texture.SampleLevel(s1,uv,0).r+
      texture.SampleLevel(s2,uv,0).r+texture.SampleLevel(s3,uv,0).r+
      texture.SampleLevel(s4,uv,0).r+texture.SampleLevel(s5,uv,0).r)));
  RayDesc ray;
  ray.Origin=float3(lane==2?missX:lane==1?secondHitX:0.0,0.0,originZ);
  ray.Direction=float3(0.0,0.0,directionZ);ray.TMin=0.0;ray.TMax=lane==3?shortRange:fullRange;
  RayQuery<RAY_FLAG_FORCE_OPAQUE> query;query.TraceRayInline(scene,0,0xff,ray);
  while(query.Proceed()) {}
  uint bias=hitBias+sampled;
#if TEXTURE_UAV == 3
  bias+=typedInput.Load(lane);
#endif
#if CBV_ROOT_COUNT == 5
  bias+=extraBias;
#endif
  uint value=query.CommittedStatus()==COMMITTED_TRIANGLE_HIT?
      uint(query.CommittedRayT()*1000.0)+query.CommittedPrimitiveIndex()+
      query.CommittedInstanceID()*17+query.CommittedGeometryIndex()*13+bias:0;
#if TEXTURE_UAV == 3
  results[index]=value;
#elif TEXTURE_UAV == 1
  results[uint2(index&3,index>>2)]=value;
#elif TEXTURE_UAV == 2
  results[uint2(index&3,index>>2)]=float4(value,value+1,value+2,1);
#else
  results.Store(index*resultStride,value);
#endif
#ifdef MULTI_UAV
  secondary[uint2(index&3,index>>2)]=float4(value?64.0/255.0:32.0/255.0,24.0/255.0,36.0/255.0,1);
#endif
}
