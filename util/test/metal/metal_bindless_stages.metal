// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct Entry { ulong zero [[id(0)]]; texture2d<float, access::read_write> image [[id(1)]]; ulong metadata [[id(2)]]; };
struct Root { constant uint *index [[id(0)]]; };
struct Vertex { float4 position [[position]]; };
struct Primitive { float4 color; };
struct Payload { float4 color; };
using Triangle = metal::mesh<Vertex, Primitive, 3, 1, metal::topology::triangle>;
[[mesh]] void mesh_only(Triangle output, constant Entry *table [[buffer(0)]], constant Root &root [[buffer(2)]], uint tid [[thread_index_in_threadgroup]]) {
  float4 color = table[root.index[0]].image.read(uint2(0));
  if(tid == 0) { table[root.index[0]].image.write(color + float4(0.25), uint2(0)); Primitive p; p.color = color; output.set_primitive(0, p); output.set_primitive_count(1); }
  if(tid < 3) { const float2 pos[] = {float2(-1,-1),float2(3,-1),float2(-1,3)}; Vertex v; v.position = float4(pos[tid],0,1); output.set_vertex(tid,v); output.set_index(tid,tid); }
}
[[object]] void object_main(object_data Payload *payload [[payload]], constant Entry *table [[buffer(0)]], constant Root &root [[buffer(2)]], mesh_grid_properties grid, uint tid [[thread_index_in_threadgroup]]) {
  if(tid == 0) { float4 color = table[root.index[0]].image.read(uint2(0)); table[root.index[0]].image.write(color + float4(0.5),uint2(0)); payload->color = color; grid.set_threadgroups_per_grid(uint3(1,1,1)); }
}
[[mesh]] void mesh_payload(Triangle output, const object_data Payload *payload [[payload]], constant Entry *table [[buffer(0)]], constant Root &root [[buffer(2)]], uint tid [[thread_index_in_threadgroup]]) {
  float4 color = table[root.index[0]].image.read(uint2(0));
  if(tid == 0) { Primitive p; p.color = color + payload->color; output.set_primitive(0,p); output.set_primitive_count(1); }
  if(tid < 3) { const float2 pos[] = {float2(-1,-1),float2(3,-1),float2(-1,3)}; Vertex v; v.position = float4(pos[tid],0,1); output.set_vertex(tid,v); output.set_index(tid,tid); }
}
fragment float4 stage_fs(Primitive in [[stage_in]]) { return in.color; }
