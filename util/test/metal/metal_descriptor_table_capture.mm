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
  device const uint *input [[id(0)]];
  texture2d<float> image [[id(1)]];
  sampler point [[id(2)]];
  uint bias [[id(3)]];
};
kernel void descriptor_probe(device const Packet &packet [[buffer(0)]], device uint *output [[buffer(1)]])
{
  output[0] = packet.input[0] + uint(packet.image.sample(packet.point, float2(0.5)).r * 255.0 + 0.5)
            + packet.bias;
}
)MSL" options:nil error:&error];
    if(!library) { fprintf(stderr, "%s\n", error.localizedDescription.UTF8String); return 2; }
    id<MTLFunction> function = [library newFunctionWithName:@"descriptor_probe"];
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
    const uint32_t a[] = {0xbadU, 41}, b[] = {0xbadU, 0xbadU, 80};
    id<MTLBuffer> inputA = [device newBufferWithBytes:a length:sizeof(a) options:MTLResourceStorageModeShared];
    id<MTLBuffer> inputB = [device newBufferWithBytes:b length:sizeof(b) options:MTLResourceStorageModeShared];
    const uint64_t addressA = inputA.gpuAddress + 4;
    uint64_t addressB = 0;
    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
        MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared;
    descriptor.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> image = [device newTextureWithDescriptor:descriptor];
    const unsigned char pixel[] = {64, 128, 192, 255};
    [image replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:pixel bytesPerRow:4];
    MTLSamplerDescriptor *sampling = [MTLSamplerDescriptor new];
    sampling.supportArgumentBuffers = YES;
    id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sampling];
    const uint64_t packet[] = {addressA, image.gpuResourceID._impl, sampler.gpuResourceID._impl,
                               (uint64_t(0xdeadbeefU) << 32) | 17};
    uint64_t contents[12];
    for(uint64_t &word : contents) word = 0xabcdef0123456789ULL;
    contents[0] = addressA; // Deliberately address-looking ordinary constant, outside the schema.
    memcpy(contents + 4, packet, sizeof(packet));
    id<MTLBuffer> table = [device newBufferWithBytes:contents length:sizeof(contents) options:MTLResourceStorageModeShared];
    table.label = @"Explicit Descriptor Table";
    id<MTLBuffer> output = [device newBufferWithLength:4 options:MTLResourceStorageModeShared];
    output.label = @"Descriptor Replay Output";
    if(!pipeline || !inputA || !inputB || !image || !sampler || !table || !output) return 3;
    RENDERDOC_API_1_7_0 *api = nullptr;
    const char *prefix = getenv("RENDERDOC_METAL_CAPTURE_PATH");
    if(prefix)
    {
      auto getAPI = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!getAPI || getAPI(eRENDERDOC_API_Version_1_7_0, (void **)&api) != 1) return 4;
      RENDERDOC_AnnotationValue layout = {};
      layout.vector.uint64[0] = 3; layout.vector.uint64[1] = 32;
      layout.vector.uint64[2] = 1; layout.vector.uint64[3] = 32;
      RENDERDOC_AnnotationValue coverage = {}; coverage.uint32 = 1;
      if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)table,
          "metal.descriptorTable", eRENDERDOC_UInt64, 4, &layout) ||
         api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device,
          "metal.descriptorCoverage", eRENDERDOC_UInt32, 0, &coverage)) return 5;
      api->SetCaptureFilePathTemplate(prefix);
    }
    id<MTLCommandQueue> queue = [device newCommandQueue];
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = device; layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO; layer.drawableSize = CGSizeMake(2, 2);
    for(int capture = 0; capture < (api ? 2 : 1); capture++)
    {
      memcpy(table.contents, contents, sizeof(contents));
      if(api)
      {
        api->StartFrameCapture(nullptr, nullptr);
        if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device,
             "metal.capturePresented", eRENDERDOC_Empty, 0, nullptr) != 2) return 10;
      }
      for(int stage = 0; stage < 2; stage++)
      {
        if(stage)
        {
          // The resource exists before capture, but its first identity query occurs
          // inside the frame. Capture emits both record metadata and a frame copy.
          addressB = inputB.gpuAddress + 8;
          memcpy((char *)table.contents + 32, &addressB, 8);
        }
        id<MTLCommandBuffer> command = [queue commandBuffer];
        id<MTLComputeCommandEncoder> compute = [command computeCommandEncoder];
        [compute setComputePipelineState:pipeline];
        [compute setBuffer:table offset:32 atIndex:0];
        [compute setBuffer:output offset:0 atIndex:1];
        [compute useResource:inputA usage:MTLResourceUsageRead];
        [compute useResource:inputB usage:MTLResourceUsageRead];
        [compute useResource:image usage:MTLResourceUsageRead];
        [compute dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
        [compute endEncoding]; [command commit]; [command waitUntilCompleted];
        if(command.error || *(uint32_t *)output.contents != (stage ? 161 : 122)) return 6;
      }
      if(api)
      {
        // More acquisitions than CAMetalLayer's drawable pool: capture must keep
        // the thumbnail acquisition alive without retaining every presented drawable.
        for(int present = 0; present < 6; present++)
        {
        @autoreleasepool
        {
        id<CAMetalDrawable> drawable = [layer nextDrawable];
        if(!drawable) return 7;
        id<MTLCommandBuffer> command = [queue commandBuffer];
        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.125, 0.25, 0.5, 1);
        id<MTLRenderCommandEncoder> render = [command renderCommandEncoderWithDescriptor:pass];
        [render endEncoding]; [command presentDrawable:drawable];
        [command commit]; [command waitUntilCompleted];
        if(command.error) return 8;
        }
        }
        // Drop all application drawable/command references before thumbnail readback.
        if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device,
             "metal.capturePresented", eRENDERDOC_Empty, 0, nullptr) ||
           !api->EndFrameCapture(nullptr, nullptr)) return 8;
      }
    }
    printf("descriptor native PASS first=122 second=161 VA_A=%llu VA_B=%llu captures=%d\n",
        (unsigned long long)addressA, (unsigned long long)addressB, api ? 2 : 0);
  }
  return 0;
}
