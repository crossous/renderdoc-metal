// SPDX-License-Identifier: MIT
// Native/injected Metal fixture for typed views of frame-created placement buffers.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "renderdoc/api/app/renderdoc_app.h"

int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device) return 2;
    NSString *source = @R"METAL(
#include <metal_stdlib>
using namespace metal;
vertex float4 vs_typed(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id], 0, 1);
}
fragment float4 fs_typed(texture_buffer<float, access::read> rg [[texture(0)]],
                         texture_buffer<float, access::read> r [[texture(1)]],
                         texture_buffer<float, access::read> snorm [[texture(2)]])
{
  return float4(rg.read(uint(0)).x, rg.read(uint(0)).y,
                r.read(uint(0)).x / 7.0, snorm.read(uint(0)).x);
}
)METAL";
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
    if(!library) { fprintf(stderr, "library: %s\n", error.localizedDescription.UTF8String); return 3; }
    id<MTLFunction> vertex = [library newFunctionWithName:@"vs_typed"];
    id<MTLFunction> fragment = [library newFunctionWithName:@"fs_typed"];
    MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertex;
    pipelineDescriptor.fragmentFunction = fragment;
    pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    id<MTLRenderPipelineState> pipeline =
        [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    [pipelineDescriptor release];
    if(!pipeline) { fprintf(stderr, "pipeline: %s\n", error.localizedDescription.UTF8String); return 4; }

    const MTLPixelFormat formats[3] = {MTLPixelFormatRG16Float, MTLPixelFormatR32Float,
                                       MTLPixelFormatRGBA16Snorm};
    id<MTLBuffer> backing[3] = {};
    id<MTLTexture> views[3] = {};
    MTLHeapDescriptor *heapDescriptor = [[MTLHeapDescriptor alloc] init];
    heapDescriptor.type = MTLHeapTypePlacement;
    heapDescriptor.storageMode = MTLStorageModePrivate;
    heapDescriptor.hazardTrackingMode = MTLHazardTrackingModeTracked;
    heapDescriptor.size = 4 * 4096;
    id<MTLHeap> heap = [device newHeapWithDescriptor:heapDescriptor];
    [heapDescriptor release];
    if(!heap) return 5;
    id<MTLBuffer> upload = [device newBufferWithLength:3 * 4096
                                               options:MTLResourceStorageModeShared];
    if(!upload) return 5;
    memset(upload.contents, 0, upload.length);
    const uint16_t rgValues[2] = {0x3800, 0x3c00}; // 0.5, 1.0 half floats
    const float rValue = 3.5f;
    const int16_t snormValue = 16384; // about 0.5
    memcpy((char *)upload.contents, rgValues, sizeof(rgValues));
    memcpy((char *)upload.contents + 4096, &rValue, sizeof(rValue));
    memcpy((char *)upload.contents + 8192, &snormValue, sizeof(snormValue));
    RENDERDOC_API_1_0_0 *api = nullptr;
    const char *capturePrefix = getenv("RENDERDOC_METAL_CAPTURE_PATH");
    if(capturePrefix)
    {
      auto getAPI = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!getAPI || getAPI(eRENDERDOC_API_Version_1_0_0, (void **)&api) != 1) return 7;
      api->SetCaptureFilePathTemplate(capturePrefix);
      api->StartFrameCapture(nullptr, nullptr);
    }
    for(int i = 0; i < 3; i++)
    {
      MTLTextureDescriptor *desc = [MTLTextureDescriptor
          textureBufferDescriptorWithPixelFormat:formats[i]
                                         width:i == 2 ? 32 : 64
                               resourceOptions:MTLResourceStorageModePrivate
                                         usage:MTLTextureUsageShaderRead];
      desc.allowGPUOptimizedContents = NO;
      backing[i] = [heap newBufferWithLength:4096 options:MTLResourceStorageModePrivate
                                    offset:i * 4096];
      views[i] = [backing[i] newTextureWithDescriptor:desc offset:0 bytesPerRow:256];
      if(!backing[i] || !views[i]) return 6;
    }

    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLCommandBuffer> command = [queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
    for(int i = 0; i < 3; i++)
      [blit copyFromBuffer:upload sourceOffset:i * 4096 toBuffer:backing[i]
       destinationOffset:0 size:256];
    [blit endEncoding];
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO;
    layer.drawableSize = CGSizeMake(4, 4);
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if(!drawable) return 8;
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
    id<MTLRenderCommandEncoder> render = [command renderCommandEncoderWithDescriptor:pass];
    [render setRenderPipelineState:pipeline];
    for(int i = 0; i < 3; i++) [render setFragmentTexture:views[i] atIndex:i];
    [render drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [render endEncoding];
    [command presentDrawable:drawable];
    [command commit];
    [command waitUntilCompleted];
    if(command.error) { fprintf(stderr, "GPU: %s\n", command.error.localizedDescription.UTF8String); return 8; }
    uint8_t pixel[4] = {};
    [drawable.texture getBytes:pixel bytesPerRow:4 fromRegion:MTLRegionMake2D(1, 1, 1, 1)
               mipmapLevel:0];
    printf("typed view BGRA: %u %u %u %u\n", pixel[0], pixel[1], pixel[2], pixel[3]);
    if(api && !api->EndFrameCapture(nullptr, nullptr)) return 9;
    if(pixel[0] < 126 || pixel[0] > 130 || pixel[1] != 255 ||
       pixel[2] < 126 || pixel[2] > 130 || pixel[3] < 126 || pixel[3] > 130) return 10;
    [heap release];
    return 0;
  }
}
