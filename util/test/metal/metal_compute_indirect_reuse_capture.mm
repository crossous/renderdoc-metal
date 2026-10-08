// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

int main()
{
  @autoreleasepool
  {
    const bool bufferWeight = getenv("RENDERDOC_METAL_INDIRECT_BUFFER_WEIGHT") != nullptr;
    const bool variant = getenv("RENDERDOC_METAL_INDIRECT_VARIANT") != nullptr;
    const NSUInteger argumentOffset=variant?28:16, outputOffset=variant?32:16;
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void write_args(device uint *args [[buffer(0)]], constant uint &count [[buffer(1)]])
{ args[0]=count<32?popcount((1u<<count)-1u):count; args[1]=1; args[2]=1; }
kernel void consume(device atomic_uint *output [[buffer(0)]], constant uint &weight [[buffer(1)]])
{ atomic_fetch_add_explicit(output, weight, memory_order_relaxed);
  atomic_fetch_add_explicit(output+1, 1u, memory_order_relaxed); }
)MSL" options:nil error:&error];
    if(!library) { fprintf(stderr,"%s\n",error.localizedDescription.UTF8String); return 2; }
    id<MTLComputePipelineState> writer = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"write_args"] error:&error];
    id<MTLComputePipelineState> consumer = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"consume"] error:&error];
    id<MTLBuffer> arguments = [device newBufferWithLength:64 options:MTLResourceStorageModePrivate];
    uint32_t initial[16]; for(auto &v:initial) v=0x13572468;
    initial[outputOffset/4]=initial[outputOffset/4+1]=0;
    id<MTLBuffer> output = [device newBufferWithBytes:initial length:sizeof(initial) options:MTLResourceStorageModeShared];
    const uint32_t weights[]={0x24681357,7,0x24681357};
    id<MTLBuffer> weightBuffer=[device newBufferWithBytes:weights length:sizeof(weights) options:MTLResourceStorageModeShared];
    arguments.label=@"Indirect reused arguments"; output.label=@"Indirect reused output";
    if(!writer || !consumer || !arguments || !output || !weightBuffer) return 3;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path);
    }
    id<MTLCommandQueue> queue=[device newCommandQueue];
    CAMetalLayer *layer=[CAMetalLayer layer]; layer.device=device;
    layer.pixelFormat=MTLPixelFormatBGRA8Unorm; layer.drawableSize=CGSizeMake(2,2);
    if(api) api->StartFrameCapture(nullptr,nullptr);
    id<MTLCommandBuffer> command=getenv("RENDERDOC_METAL_INDIRECT_UNRETAINED")?
        [queue commandBufferWithUnretainedReferences]:[queue commandBuffer];
    id<MTLComputeCommandEncoder> compute=getenv("RENDERDOC_METAL_INDIRECT_CONCURRENT")?
        [command computeCommandEncoderWithDispatchType:MTLDispatchTypeConcurrent]:[command computeCommandEncoder];
    const uint32_t counts[]={variant?3U:1U,variant?2U:3U,variant?4U:2U,0};
    for(unsigned step=0;step<4;step++)
    {
      [compute setComputePipelineState:writer];
      [compute setBuffer:arguments offset:argumentOffset atIndex:0];
      [compute setBytes:&counts[step] length:4 atIndex:1];
      [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      [compute memoryBarrierWithScope:MTLBarrierScopeBuffers];
      if(step==3) break;
      [compute setComputePipelineState:consumer];
      if(bufferWeight)
      {
        id<MTLBuffer> bindings[]={output,weightBuffer}; NSUInteger offsets[]={0,4};
        [compute setBuffers:bindings offsets:offsets withRange:NSMakeRange(0,2)];
        [compute setBufferOffset:outputOffset atIndex:0];
      }
      else
      {
        const uint32_t weight=variant?(step==0?17U:step==1?29U:43U):10*(step+1);
        [compute setBuffer:output offset:outputOffset atIndex:0];
        [compute setBytes:&weight length:4 atIndex:1];
      }
      [compute dispatchThreadgroupsWithIndirectBuffer:arguments indirectBufferOffset:argumentOffset threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      [compute memoryBarrierWithScope:MTLBarrierScopeBuffers];
    }
    [compute endEncoding];
    id<CAMetalDrawable> drawable=[layer nextDrawable];
    if(!drawable) return 5;
    auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=drawable.texture;
    pass.colorAttachments[0].loadAction=MTLLoadActionClear;
    pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    [[command renderCommandEncoderWithDescriptor:pass] endEncoding];
    [command presentDrawable:drawable]; [command commit]; [command waitUntilCompleted];
    const uint32_t *values=(const uint32_t *)output.contents;
    if(command.error || values[outputOffset/4]!=(bufferWeight?(variant?63U:42U):(variant?281U:130U)) || values[outputOffset/4+1]!=(variant?9U:6U)) return 6;
    for(unsigned i=0;i<16;i++) if(i!=outputOffset/4 && i!=outputOffset/4+1 && values[i]!=0x13572468) return 7;
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 8;
    printf("PASS native indirect reuse, groups=1/3/2, final args=0/1/1, weight=%s, total=%u count=%u\n",bufferWeight?"buffer":"inline",values[4],values[5]);
  }
  return 0;
}
