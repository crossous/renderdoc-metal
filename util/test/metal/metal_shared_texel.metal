// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
kernel void linear_half(const device uint *input [[buffer(4)]],
    device uint *output [[buffer(9)]], constant uint4 &shape [[buffer(2)]],
    texture2d<half, access::read> texels [[texture(6)]], uint3 tid [[thread_position_in_grid]])
{
  uint i=tid.x+shape.x*(tid.y+shape.y*tid.z);
  output[shape.w+i]=input[i]*7+i*11+101+uint(float(texels.read(uint2(i%texels.get_width(),0)).x)*32.0f);
}
kernel void linear_uint(const device uint *input [[buffer(4)]],
    device uint *output [[buffer(9)]], constant uint4 &shape [[buffer(2)]],
    texture2d<uint, access::read> texels [[texture(6)]], uint3 tid [[thread_position_in_grid]])
{
  uint i=tid.x+shape.x*(tid.y+shape.y*tid.z);
  output[shape.w+i]=input[i]*7+i*11+101+texels.read(uint2(i%texels.get_width(),0)).x;
}

struct LinearPacket { const device uint *input; ulong reserved; ulong length; };
kernel void linear_half_table(constant LinearPacket &packet [[buffer(30)]],
    device uint *output [[buffer(9)]], constant uint4 &shape [[buffer(2)]],
    texture2d<half, access::read> texels [[texture(6)]], uint3 tid [[thread_position_in_grid]])
{
  uint i=tid.x+shape.x*(tid.y+shape.y*tid.z);
  output[shape.w+i]=packet.input[i]*7+i*11+101+uint(float(texels.read(uint2(i%texels.get_width(),0)).x)*32.0f);
}
kernel void linear_uint_table(constant LinearPacket &packet [[buffer(30)]],
    device uint *output [[buffer(9)]], constant uint4 &shape [[buffer(2)]],
    texture2d<uint, access::read> texels [[texture(6)]], uint3 tid [[thread_position_in_grid]])
{
  uint i=tid.x+shape.x*(tid.y+shape.y*tid.z);
  output[shape.w+i]=packet.input[i]*7+i*11+101+texels.read(uint2(i%texels.get_width(),0)).x;
}
