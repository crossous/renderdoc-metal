// SPDX-License-Identifier: MIT
// CPU-only layout evidence from the actual UE-bundled Apache-2.0 IR header.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstddef>
#include <cstdio>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>
#include <metal_irconverter_runtime/ir_raytracing.h>
int main()
{
  printf("{\"dispatch_argument_size\":%zu,\"descriptor_size\":%zu,\"shader_identifier_size\":%zu,",
         sizeof(IRDispatchRaysArgument),sizeof(IRDispatchRaysDescriptor),sizeof(IRShaderIdentifier));
  printf("\"offsets\":{\"raygen\":%zu,\"miss\":%zu,\"hit\":%zu,\"callable\":%zu,\"width\":%zu,",
         offsetof(IRDispatchRaysDescriptor,RayGenerationShaderRecord),offsetof(IRDispatchRaysDescriptor,MissShaderTable),
         offsetof(IRDispatchRaysDescriptor,HitGroupTable),offsetof(IRDispatchRaysDescriptor,CallableShaderTable),offsetof(IRDispatchRaysDescriptor,Width));
  printf("\"GRS\":%zu,\"ResDescHeap\":%zu,\"SmpDescHeap\":%zu,\"VFT\":%zu,\"IFT\":%zu,\"IFTs\":%zu},",
         offsetof(IRDispatchRaysArgument,GRS),offsetof(IRDispatchRaysArgument,ResDescHeap),offsetof(IRDispatchRaysArgument,SmpDescHeap),
         offsetof(IRDispatchRaysArgument,VisibleFunctionTable),offsetof(IRDispatchRaysArgument,IntersectionFunctionTable),offsetof(IRDispatchRaysArgument,IntersectionFunctionTables));
  printf("\"shader_record_sampler_offset\":%zu}\n",offsetof(IRShaderIdentifier,localRootSignatureSamplersBuffer));
}
