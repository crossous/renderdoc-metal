#include <metal_stdlib>
using namespace metal;
struct Entry {
  ulong zero [[id(0)]];
  texture2d<float, access::read_write> image [[id(1)]];
  ulong metadata [[id(2)]];
};
struct Root { constant uint *index [[id(0)]]; };
kernel void modify(constant Entry *table [[buffer(0)]], constant Root &root [[buffer(2)]])
{
  uint index = root.index[0];
  float4 previous = table[index].image.read(uint2(0));
  table[index].image.write(previous + float4(0.25, 0.5, 0.75, 1), uint2(0));
}
