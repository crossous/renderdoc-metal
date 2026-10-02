// SPDX-License-Identifier: MIT
// Tiny native positive and capture-metadata probe. Never replay an unpatched raw GPU pointer.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device) return 2;
    NSError *error = nil;
    const bool indirectOnly = getenv("METAL_IDENTITY_INDIRECT_ONLY") != nullptr;
    id<MTLLibrary> library = [device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct IdentityArguments
{
  device const uint *input [[id(0)]];
  texture2d<float> image [[id(1)]];
  sampler point [[id(2)]];
};
kernel void indirect_identity_probe(device IdentityArguments &args [[buffer(0)]],
                                    device uint *output [[buffer(1)]])
{
  output[0] = args.input[0] + uint(args.image.sample(args.point, float2(0.5)).r * 255.0 + 0.5);
}
kernel void identity_probe(device const ulong *table [[buffer(0)]],
                           device uint *output [[buffer(1)]],
                           texture2d<float> image [[texture(0)]], sampler point [[sampler(0)]])
{
  device const uint *input = reinterpret_cast<device const uint *>(table[0]);
  output[0] = input[0] + uint(image.sample(point, float2(0.5)).r * 255.0 + 0.5);
}
)MSL" options:nil error:&error];
    if(!library) return 3;
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:
        [library newFunctionWithName:indirectOnly ? @"indirect_identity_probe" : @"identity_probe"]
        error:&error];
    const uint32_t value = 41;
    MTLHeapDescriptor *heapDesc = [MTLHeapDescriptor new];
    heapDesc.storageMode = MTLStorageModeShared;
    heapDesc.size = 65536;
    id<MTLHeap> heap = indirectOnly ? [device newHeapWithDescriptor:heapDesc] : nil;
    id<MTLBuffer> input = indirectOnly ?
        [heap newBufferWithLength:4 options:MTLResourceStorageModeShared] :
        [device newBufferWithBytes:&value length:4 options:MTLResourceStorageModeShared];
    if(indirectOnly && input) *(uint32_t *)input.contents = value;
    id<MTLBuffer> output = [device newBufferWithLength:4 options:MTLResourceStorageModeShared];
    const uint64_t address = input.gpuAddress;
    MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
        MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    desc.storageMode = MTLStorageModeShared;
    desc.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> image = indirectOnly ? [heap newTextureWithDescriptor:desc] :
        [device newTextureWithDescriptor:desc];
    const unsigned char pixel[] = {64, 128, 192, 255};
    [image replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:pixel bytesPerRow:4];
    MTLSamplerDescriptor *sampling = [MTLSamplerDescriptor new];
    sampling.supportArgumentBuffers = YES;
    id<MTLSamplerState> point = [device newSamplerStateWithDescriptor:sampling];
    const bool queryInFrame = getenv("METAL_IDENTITY_QUERY_IN_FRAME") != nullptr;
    uint64_t textureID = queryInFrame ? 0 : image.gpuResourceID._impl;
    uint64_t samplerID = queryInFrame ? 0 : point.gpuResourceID._impl;
    const uint64_t identities[] = {address, textureID, samplerID};
    id<MTLBuffer> table = [device newBufferWithBytes:identities length:indirectOnly ? 24 : 8
        options:MTLResourceStorageModeShared];
    if(indirectOnly && queryInFrame) return 4;
    if(!pipeline || !input || !output || !table || !image || !point || !address ||
       (!queryInFrame && (!textureID || !samplerID))) return 4;

    RENDERDOC_API_1_0_0 *api = nullptr;
    const char *prefix = getenv("RENDERDOC_METAL_CAPTURE_PATH");
    if(prefix)
    {
      auto getAPI = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!getAPI || getAPI(eRENDERDOC_API_Version_1_0_0, (void **)&api) != 1) return 5;
      api->SetCaptureFilePathTemplate(prefix);
    }
    id<MTLCommandQueue> queue = [device newCommandQueue];
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO;
    layer.drawableSize = CGSizeMake(2, 2);
    uint64_t discardedAddress = 0;
    if(indirectOnly)
    {
      // A queried object destroyed before capture must not survive in the resource closure.
      @autoreleasepool
      {
        id<MTLBuffer> discarded = [device newBufferWithLength:64 options:MTLResourceStorageModeShared];
        discardedAddress = discarded.gpuAddress;
        discarded = nil;
      }
    }
    for(int iteration = 0; iteration < (api ? 2 : 1); ++iteration)
    {
      if(api) api->StartFrameCapture(nullptr, nullptr);
      if(queryInFrame && iteration == 0)
      {
        textureID = image.gpuResourceID._impl;
        samplerID = point.gpuResourceID._impl;
        if(!textureID || !samplerID) return 6;
      }
      // Cached first-query metadata must survive both captures.
      if(input.gpuAddress != address || image.gpuResourceID._impl != textureID ||
         point.gpuResourceID._impl != samplerID) return 6;
      id<MTLCommandBuffer> command = [queue commandBuffer];
      id<MTLComputeCommandEncoder> compute = [command computeCommandEncoder];
      [compute setComputePipelineState:pipeline];
      [compute setBuffer:table offset:0 atIndex:0];
      [compute setBuffer:output offset:0 atIndex:1];
      if(indirectOnly)
        [compute useHeap:heap];
      else
      {
        [compute useResource:input usage:MTLResourceUsageRead];
        [compute setTexture:image atIndex:0];
        [compute setSamplerState:point atIndex:0];
      }
      [compute dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
      [compute endEncoding];
      [command commit];
      [command waitUntilCompleted];
      if(command.error || command.status != MTLCommandBufferStatusCompleted ||
         *(const uint32_t *)output.contents != 105) return 7;
      if(api)
      {
        id<CAMetalDrawable> drawable = [layer nextDrawable];
        if(!drawable) return 8;
        id<MTLCommandBuffer> present = [queue commandBuffer];
        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> render = [present renderCommandEncoderWithDescriptor:pass];
        [render endEncoding];
        [present presentDrawable:drawable];
        [present commit];
        [present waitUntilCompleted];
        if(present.error || !api->EndFrameCapture(nullptr, nullptr)) return 9;
      }
    }
    printf("native GPU identities PASS result=105 bufferVA=%llu textureID=%llu samplerID=%llu captures=%u discardedVA=%llu\n",
           (unsigned long long)address, (unsigned long long)textureID,
           (unsigned long long)samplerID, api ? 2U : 0U, (unsigned long long)discardedAddress);
  }
  return 0;
}
