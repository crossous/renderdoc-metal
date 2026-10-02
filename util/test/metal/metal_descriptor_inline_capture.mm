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
kernel void frame_pointer(const device ulong *table [[buffer(0)]], device uint4 *output [[buffer(1)]])
{
  const device uint *input = reinterpret_cast<const device uint *>(table[1]);
  const bool valid = table[0] == 0xabcdef0123456789ul && table[2] == 0xfeedfacec001d00dul;
  output[0] = uint4(valid ? input[0] : 0xbadU, 0xdeadbeefU, uint(table[1]), uint(table[1] >> 32));
}
)MSL" options:nil error:&error];
    if(!library) { fprintf(stderr, "%s\n", error.localizedDescription.UTF8String); return 2; }
    id<MTLFunction> function = [library newFunctionWithName:@"frame_pointer"];
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
    const char *privateSetting = getenv("RENDERDOC_METAL_DESCRIPTOR_PRIVATE");
    const bool privateInputs = privateSetting && !strcmp(privateSetting, "1");
    const MTLResourceOptions options = (privateInputs ? MTLResourceStorageModePrivate : MTLResourceStorageModeShared) |
        MTLResourceHazardTrackingModeTracked;
    MTLSizeAndAlign a = [device heapBufferSizeAndAlignWithLength:8 options:options];
    MTLSizeAndAlign b = [device heapBufferSizeAndAlignWithLength:12 options:options];
    if(!a.align || !b.align) return 3;
    const NSUInteger secondOffset = (a.size + b.align - 1) / b.align * b.align;
    MTLHeapDescriptor *heapDescriptor = [MTLHeapDescriptor new];
    heapDescriptor.type = MTLHeapTypePlacement;
    heapDescriptor.storageMode = privateInputs ? MTLStorageModePrivate : MTLStorageModeShared;
    heapDescriptor.hazardTrackingMode = MTLHazardTrackingModeTracked;
    heapDescriptor.size = MAX(65536UL, secondOffset + b.size);
    id<MTLHeap> heap = [device newHeapWithDescriptor:heapDescriptor];
    const uint32_t sourceA[] = {0xbadU, 41U}, sourceB[] = {0xbadU, 0xbadU, 80U};
    id<MTLBuffer> staging[2] = {
        [device newBufferWithBytes:sourceA length:sizeof(sourceA) options:MTLResourceStorageModeShared],
        [device newBufferWithBytes:sourceB length:sizeof(sourceB) options:MTLResourceStorageModeShared]};
    const uint64_t initial[] = {0xabcdef0123456789ULL, 0, 0xfeedfacec001d00dULL};
    id<MTLBuffer> table = [device newBufferWithBytes:initial length:sizeof(initial) options:MTLResourceStorageModeShared];
    id<MTLBuffer> output = [device newBufferWithLength:16 options:MTLResourceStorageModeShared];
    if(!heap || !pipeline || !table || !output || !staging[0] || !staging[1]) return 4;
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto getAPI = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!getAPI || getAPI(eRENDERDOC_API_Version_1_7_0, (void **)&api) != 1) return 5;
      RENDERDOC_AnnotationValue layout = {};
      layout.vector.uint64[0] = 0; layout.vector.uint64[1] = 8;
      layout.vector.uint64[2] = 1; layout.vector.uint64[3] = 8;
      RENDERDOC_AnnotationValue coverage = {}; coverage.uint32 = 3;
      if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)table,
          "metal.descriptorTable", eRENDERDOC_UInt64, 4, &layout) ||
         api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device,
          "metal.descriptorCoverage", eRENDERDOC_UInt32, 0, &coverage)) return 6;
      api->SetCaptureFilePathTemplate(path);
    }
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = device; layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO; layer.drawableSize = CGSizeMake(2, 2);
    id<MTLCommandQueue> queue = [device newCommandQueue];
    uint64_t addresses[2] = {};
    for(int capture = 0; capture < (api ? 2 : 1); capture++)
    {
      @autoreleasepool
      {
        memcpy(table.contents, initial, sizeof(initial));
        if(api) api->StartFrameCapture(nullptr, nullptr);
        id<MTLBuffer> inputs[2];
        for(int stage = 0; stage < 2; stage++)
        {
          inputs[stage] = [heap newBufferWithLength:stage ? 12 : 8 options:options offset:stage ? secondOffset : 0];
          if(!inputs[stage]) return 7;
          if(!privateInputs)
            memcpy(inputs[stage].contents, staging[stage].contents, stage ? 12 : 8);
          addresses[stage] = inputs[stage].gpuAddress + (stage ? 8 : 4);
          memcpy((char *)table.contents + 8, addresses + stage, 8);
          id<MTLCommandBuffer> command = [queue commandBuffer];
          if(privateInputs)
          {
            id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
            [blit copyFromBuffer:staging[stage] sourceOffset:0 toBuffer:inputs[stage]
                destinationOffset:0 size:stage ? 12 : 8];
            [blit endEncoding];
          }
          id<MTLComputeCommandEncoder> compute = [command computeCommandEncoder];
          [compute setComputePipelineState:pipeline];
          if(api)
          {
            RENDERDOC_AnnotationValue bytesLayout = {};
            bytesLayout.vector.uint64[0] = 0; bytesLayout.vector.uint64[1] = 8;
            bytesLayout.vector.uint64[2] = 1; bytesLayout.vector.uint64[3] = 8;
            if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)compute,
                "metal.descriptorBytes", eRENDERDOC_UInt64, 4, &bytesLayout)) return 11;
          }
          [compute setBytes:table.contents length:24 atIndex:0];
          [compute setBuffer:output offset:0 atIndex:1];
          [compute useResource:inputs[stage] usage:MTLResourceUsageRead];
          [compute dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
          [compute endEncoding]; [command commit]; [command waitUntilCompleted];
          const uint32_t *result = (const uint32_t *)output.contents;
          if(command.error || result[0] != (stage ? 80U : 41U) || result[1] != 0xdeadbeefU) return 8;
        }
        if(api)
        {
          id<CAMetalDrawable> drawable = [layer nextDrawable];
          if(!drawable) return 9;
          MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
          pass.colorAttachments[0].texture = drawable.texture;
          pass.colorAttachments[0].loadAction = MTLLoadActionClear;
          pass.colorAttachments[0].storeAction = MTLStoreActionStore;
          pass.colorAttachments[0].clearColor = MTLClearColorMake(0.125, 0.25, 0.5, 1);
          id<MTLCommandBuffer> command = [queue commandBuffer];
          id<MTLRenderCommandEncoder> render = [command renderCommandEncoderWithDescriptor:pass];
          [render endEncoding]; [command presentDrawable:drawable];
          [command commit]; [command waitUntilCompleted];
          if(command.error || !api->EndFrameCapture(nullptr, nullptr)) return 10;
        }
      }
    }
    printf("inline descriptor native PASS first=41 second=80 VA_A=%llu VA_B=%llu captures=%d\n",
        (unsigned long long)addresses[0], (unsigned long long)addresses[1], api ? 2 : 0);
  }
  return 0;
}
