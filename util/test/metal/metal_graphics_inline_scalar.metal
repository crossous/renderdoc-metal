// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;
struct Payload { ulong ordinary, salt; uint offset,count; };
struct Wide { ulong ordinary, salt; uint offset,count; ulong extra,tail; };
struct Out { float4 position [[position]]; };
vertex Out scalar_v(uint id [[vertex_id]], constant Payload &p [[buffer(2)]]) {
 float2 pts[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
 Out o;o.position=float4(pts[(id+p.offset)%3],0,1);return o;
}
struct Pointer { device const float2 *pts; };
vertex Out pointer_v(uint id [[vertex_id]],constant Pointer &p [[buffer(2)]]) {
 Out o;o.position=float4(p.pts[id],0,1);return o;
}
fragment float4 scalar_f(constant Payload &p [[buffer(7)]]) {
 return float4(float((p.ordinary^p.salt)&255)/255.0, float(p.count)/255.0,
 float((p.salt>>8)&255)/255.0,1);
}

vertex Out wide_v(uint id [[vertex_id]],constant Wide &p [[buffer(2)]]) { float2 pts[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};Out o;o.position=float4(pts[(id+p.offset)%3],0,1);return o; }
fragment float4 wide_f(constant Wide &p [[buffer(7)]]) {return float4(float((p.ordinary^p.salt)&255)/255.0,float(p.count)/255.0,float((p.extra+p.tail)&255)/255.0,1);}
