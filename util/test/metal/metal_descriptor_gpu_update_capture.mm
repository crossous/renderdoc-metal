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
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Entry { device const uint *value [[id(0)]]; ulong textureID [[id(1)]]; ulong metadata [[id(2)]]; };
struct Root {
  device const Entry *entries [[id(0)]]; ulong z0 [[id(1)]]; ulong m0 [[id(2)]];
  device const uint *indices [[id(3)]]; ulong z1 [[id(4)]]; ulong m1 [[id(5)]];
  device Entry *destination [[id(6)]]; ulong z2 [[id(7)]]; ulong m2 [[id(8)]];
};
kernel void copy_descriptor(device const Root &root [[buffer(0)]])
{ root.destination[root.indices[0]] = root.entries[0]; }
kernel void consume_descriptors(device const Entry *table [[buffer(0)]], device uint *output [[buffer(1)]])
{ output[0] = table[0].value[0] + (table[1].value ? table[1].value[0] : 0); }
)MSL" options:nil error:&error];
    if(!library) { fprintf(stderr, "%s\n", error.localizedDescription.UTF8String); return 2; }
    id<MTLComputePipelineState> copy = [device newComputePipelineStateWithFunction:
        [library newFunctionWithName:@"copy_descriptor"] error:&error];
    id<MTLComputePipelineState> consume = [device newComputePipelineStateWithFunction:
        [library newFunctionWithName:@"consume_descriptors"] error:&error];
    const uint32_t a = 41, b = 80, index = 0;
    id<MTLBuffer> inputA = [device newBufferWithBytes:&a length:4 options:MTLResourceStorageModeShared];
    id<MTLBuffer> inputB = [device newBufferWithBytes:&b length:4 options:MTLResourceStorageModeShared];
    id<MTLBuffer> indices = [device newBufferWithBytes:&index length:4 options:MTLResourceStorageModeShared];
    uint64_t entry[] = {inputA.gpuAddress, 0, 0xabcdef0123456789ULL};
    uint64_t entryB[] = {inputB.gpuAddress, 0, 0x12345678abcdef09ULL};
    id<MTLBuffer> source = [device newBufferWithBytes:entry length:24 options:MTLResourceStorageModeShared];
    id<MTLBuffer> destination = [device newBufferWithLength:48 options:MTLResourceStorageModeShared];
    uint64_t roots[] = {source.gpuAddress, 0, 17, indices.gpuAddress, 0, 18,
                         destination.gpuAddress, 0, 19};
    id<MTLBuffer> root = [device newBufferWithBytes:roots length:72 options:MTLResourceStorageModeShared];
    uint32_t out[] = {0, 0xdeadbeefU};
    id<MTLBuffer> output = [device newBufferWithBytes:out length:8 options:MTLResourceStorageModeShared];
    if(!copy || !consume || !source || !destination || !root || !output) return 3;
    RENDERDOC_API_1_7_0 *api = nullptr;
    const char *prefix = getenv("RENDERDOC_METAL_CAPTURE_PATH");
    if(prefix)
    {
      auto getAPI = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!getAPI || getAPI(eRENDERDOC_API_Version_1_7_0, (void **)&api) != 1) return 4;
      RENDERDOC_AnnotationValue coverage = {}; coverage.uint32 = 2;
      if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device,
           "metal.descriptorCoverage", eRENDERDOC_UInt32, 0, &coverage)) return 5;
      for(id<MTLBuffer> buffer in @[source, destination, root])
      {
        RENDERDOC_AnnotationValue layout = {};
        layout.vector.uint64[0] = 1; layout.vector.uint64[2] = buffer.length / 24;
        layout.vector.uint64[3] = 24;
        if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)buffer,
             "metal.descriptorTable", eRENDERDOC_UInt64, 4, &layout)) return 5;
      }
      RENDERDOC_AnnotationValue gpu = {}; gpu.uint32 = 1;
      if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)destination,
           "metal.descriptorGPUWrites", eRENDERDOC_UInt32, 0, &gpu)) return 5;
      api->SetCaptureFilePathTemplate(prefix);
    }
    id<MTLCommandQueue> queue = [device newCommandQueue];
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = device; layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO; layer.drawableSize = CGSizeMake(2, 2);
    for(int capture = 0; capture < (api ? 2 : 1); capture++)
    {
      memcpy(source.contents, entry, 24); memset(destination.contents, 0, 48);
      memcpy(output.contents, out, 8);
      if(api) api->StartFrameCapture(nullptr, nullptr);
      for(int stage = 0; stage < 3; stage++)
      {
        if(stage == 1)
        {
          memcpy((char *)destination.contents + 24, entryB, 24);
          if(api)
          {
            RENDERDOC_AnnotationValue write = {};
            write.vector.uint64[0] = 24; write.vector.uint64[1] = 24;
            if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)destination,
                 "metal.descriptorCPUWrite", eRENDERDOC_UInt64, 2, &write)) return 6;
          }
        }
        if(stage == 2) memcpy(source.contents, entryB, 8);
        id<MTLCommandBuffer> command = [queue commandBuffer];
        id<MTLComputeCommandEncoder> compute = [command computeCommandEncoder];
        for(id<MTLBuffer> buffer in @[source, indices, destination, inputA, inputB])
          [compute useResource:buffer usage:buffer == destination ?
              MTLResourceUsageRead | MTLResourceUsageWrite : MTLResourceUsageRead];
        if(stage != 1)
        {
          [compute setComputePipelineState:copy]; [compute setBuffer:root offset:0 atIndex:0];
          [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
          [compute memoryBarrierWithScope:MTLBarrierScopeBuffers];
        }
        [compute setComputePipelineState:consume];
        [compute setBuffer:destination offset:0 atIndex:0]; [compute setBuffer:output offset:0 atIndex:1];
        [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
        [compute endEncoding]; [command commit]; [command waitUntilCompleted];
        const uint32_t expected[] = {41, 121, 160};
        if(command.error || *(uint32_t *)output.contents != expected[stage] ||
           ((uint32_t *)output.contents)[1] != 0xdeadbeefU) return 7;
      }
      if(api)
      {
        id<CAMetalDrawable> drawable = [layer nextDrawable];
        if(!drawable) return 8;
        id<MTLCommandBuffer> command = [queue commandBuffer];
        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.125, 0.25, 0.5, 1);
        id<MTLRenderCommandEncoder> render = [command renderCommandEncoderWithDescriptor:pass];
        [render endEncoding]; [command presentDrawable:drawable];
        [command commit]; [command waitUntilCompleted];
        if(command.error || !api->EndFrameCapture(nullptr, nullptr)) return 9;
      }
    }
    printf("descriptor GPU-update native PASS 41/121/160 VA_A=%llu VA_B=%llu captures=%d\n",
           (unsigned long long)entry[0], (unsigned long long)entryB[0], api ? 2 : 0);
  }
  return 0;
}
