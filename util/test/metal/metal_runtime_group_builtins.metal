// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct Entry { ulong buffer,texture,length; };
struct Roots { ulong constants,samplers; };
kernel void runtime_clear(constant Entry *heap [[buffer(0)]],constant Roots &roots [[buffer(2)]],
                          uint local [[thread_index_in_threadgroup]],uint3 group [[threadgroup_position_in_grid]])
{
 constant uint *settings=(constant uint *)roots.constants;
 const uint linear=((group.z*2+group.y)*2+group.x)*(settings[3]/8)+local;
 const uint quotient=linear/settings[3];
 const uint remainder=linear%settings[3];
 if(remainder==0){device uint *output=(device uint *)heap[settings[0]].buffer;output[settings[1]+quotient]=settings[2];}
}
