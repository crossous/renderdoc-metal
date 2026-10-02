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
struct Packet
{
  const device uint *input [[id(0)]];
  texture_buffer<uint> image [[id(1)]];
  uint bias [[id(2)]];
};
kernel void frame_pointer(const device Packet *table [[buffer(0)]], device uint2 *output [[buffer(1)]])
{
  output[0] = uint2(table->input[0] + table->image.read(0u).x + table->bias, 0xdeadbeefU);
}
)MSL" options:nil error:&error];
    if(!library) { fprintf(stderr, "%s\n", error.localizedDescription.UTF8String); return 2; }
    id<MTLFunction> function = [library newFunctionWithName:@"frame_pointer"];
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
    const MTLResourceOptions options = MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeTracked;
    MTLSizeAndAlign a = [device heapBufferSizeAndAlignWithLength:512 options:options];
    MTLSizeAndAlign b = [device heapBufferSizeAndAlignWithLength:512 options:options];
    if(!a.align || !b.align) return 3;
    const NSUInteger secondOffset = (a.size + b.align - 1) / b.align * b.align;
    MTLHeapDescriptor *heapDescriptor = [MTLHeapDescriptor new];
    heapDescriptor.type = MTLHeapTypePlacement;
    heapDescriptor.storageMode = MTLStorageModePrivate;
    heapDescriptor.hazardTrackingMode = MTLHazardTrackingModeTracked;
    heapDescriptor.size = MAX(65536UL, secondOffset + b.size);
    id<MTLHeap> heap = [device newHeapWithDescriptor:heapDescriptor];
    uint32_t sourceA[128] = {}, sourceB[128] = {};
    sourceA[64] = 41; sourceB[64] = 80;
    id<MTLBuffer> staging[2] = {
        [device newBufferWithBytes:sourceA length:sizeof(sourceA) options:MTLResourceStorageModeShared],
        [device newBufferWithBytes:sourceB length:sizeof(sourceB) options:MTLResourceStorageModeShared]};
    const uint64_t initial[] = {0xabcdef0123456789ULL, 0, 0, 7, 0xfeedfacec001d00dULL, 0x123456789abcdef0ULL};
    id<MTLBuffer> table = [device newBufferWithBytes:initial length:sizeof(initial) options:MTLResourceStorageModeShared];
    id<MTLBuffer> output = [device newBufferWithLength:16 options:MTLResourceStorageModeShared];
    if(!heap || !pipeline || !table || !output || !staging[0] || !staging[1]) return 4;
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto getAPI = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!getAPI || getAPI(eRENDERDOC_API_Version_1_7_0, (void **)&api) != 1) return 5;
      RENDERDOC_AnnotationValue layout = {};
      layout.vector.uint64[0] = 1; layout.vector.uint64[1] = 8;
      layout.vector.uint64[2] = 1; layout.vector.uint64[3] = 24;
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
    uint64_t addresses[2] = {}, textures[2] = {};
    for(int capture = 0; capture < (api ? 2 : 1); capture++)
    {
      @autoreleasepool
      {
        memcpy(table.contents, initial, sizeof(initial));
        if(api) api->StartFrameCapture(nullptr, nullptr);
        id<MTLBuffer> inputs[2];
        id<MTLTexture> views[2];
        for(int stage = 0; stage < 2; stage++)
        {
          inputs[stage] = [heap newBufferWithLength:512 options:options offset:stage ? secondOffset : 0];
          if(!inputs[stage]) return 7;
          MTLTextureDescriptor *view = [MTLTextureDescriptor new];
          view.textureType = MTLTextureTypeTextureBuffer;
          view.pixelFormat = MTLPixelFormatR32Uint; view.width = 1; view.height = 1;
          view.storageMode = MTLStorageModePrivate;
          view.resourceOptions = MTLResourceStorageModePrivate;
          view.hazardTrackingMode = MTLHazardTrackingModeDefault;
          view.allowGPUOptimizedContents = NO; view.usage = MTLTextureUsageShaderRead;
          views[stage] = [inputs[stage] newTextureWithDescriptor:view offset:256 bytesPerRow:256];
          if(!views[stage]) return 11;
          addresses[stage] = inputs[stage].gpuAddress + 256;
          textures[stage] = views[stage].gpuResourceID._impl;
          memcpy((char *)table.contents + 8, addresses + stage, 8);
          memcpy((char *)table.contents + 16, textures + stage, 8);
          id<MTLCommandBuffer> command = [queue commandBuffer];
          id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
          [blit copyFromBuffer:staging[stage] sourceOffset:0 toBuffer:inputs[stage]
              destinationOffset:0 size:512];
          [blit endEncoding];
          id<MTLComputeCommandEncoder> compute = [command computeCommandEncoder];
          [compute setComputePipelineState:pipeline];
          [compute setBuffer:table offset:8 atIndex:0];
          [compute setBuffer:output offset:0 atIndex:1];
          [compute useResource:inputs[stage] usage:MTLResourceUsageRead];
          [compute useResource:views[stage] usage:MTLResourceUsageRead];
          [compute dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
          [compute endEncoding]; [command commit]; [command waitUntilCompleted];
          const uint32_t *result = (const uint32_t *)output.contents;
          if(command.error || result[0] != (stage ? 167U : 89U) || result[1] != 0xdeadbeefU) return 8;
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
    printf("frame view native PASS first=89 second=167 VA_A=%llu VA_B=%llu captures=%d TEX_A=%llu TEX_B=%llu\n",
        (unsigned long long)addresses[0], (unsigned long long)addresses[1], api ? 2 : 0, (unsigned long long)textures[0], (unsigned long long)textures[1]);
  }
  return 0;
}
