// SPDX-License-Identifier: MIT
// A Shared placement buffer created and CPU-written inside the captured frame.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include "renderdoc/api/app/renderdoc_app.h"

int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device) return 2;
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:
        @"#include <metal_stdlib>\nusing namespace metal;\n"
         "kernel void add_one(device uint *v [[buffer(0)]], uint i [[thread_position_in_grid]])"
         " { v[i] += 1; }" options:nil error:&error];
    id<MTLFunction> function = [library newFunctionWithName:@"add_one"];
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function
                                                                                error:&error];
    if(!pipeline) { fprintf(stderr, "pipeline: %s\n", error.localizedDescription.UTF8String); return 3; }
    MTLHeapDescriptor *descriptor = [[MTLHeapDescriptor alloc] init];
    descriptor.type = MTLHeapTypePlacement;
    descriptor.storageMode = MTLStorageModeShared;
    descriptor.hazardTrackingMode = MTLHazardTrackingModeTracked;
    descriptor.size = 4096;
    id<MTLHeap> heap = [device newHeapWithDescriptor:descriptor];
    [descriptor release];
    if(!heap) return 4;
    MTLSizeAndAlign layout = [device heapBufferSizeAndAlignWithLength:256
                                                              options:MTLResourceStorageModeShared];
    if(!layout.align || 1024 % layout.align) return 5;
    id<MTLCommandQueue> queue = [device newCommandQueue];
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = device;
    layer.framebufferOnly = NO;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.drawableSize = CGSizeMake(4, 4);
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if(!drawable) return 6;
    RENDERDOC_API_1_0_0 *api = nullptr;
    const char *prefix = getenv("RENDERDOC_METAL_CAPTURE_PATH");
    if(prefix)
    {
      auto getAPI = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!getAPI || getAPI(eRENDERDOC_API_Version_1_0_0, (void **)&api) != 1) return 7;
      api->SetCaptureFilePathTemplate(prefix);
      api->StartFrameCapture(nullptr, nullptr);
    }
    id<MTLBuffer> buffer = [heap newBufferWithLength:256
                                            options:MTLResourceStorageModeShared offset:1024];
    if(!buffer || buffer.heapOffset != 1024) return 8;
    uint32_t *values = (uint32_t *)buffer.contents;
    values[0] = 7;
    values[1] = 11;
    id<MTLCommandBuffer> command = [queue commandBuffer];
    id<MTLComputeCommandEncoder> compute = [command computeCommandEncoder];
    [compute setComputePipelineState:pipeline];
    [compute setBuffer:buffer offset:0 atIndex:0];
    [compute dispatchThreads:MTLSizeMake(2, 1, 1) threadsPerThreadgroup:MTLSizeMake(2, 1, 1)];
    [compute dispatchThreads:MTLSizeMake(2, 1, 1) threadsPerThreadgroup:MTLSizeMake(2, 1, 1)];
    [compute endEncoding];
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.25, 0.5, 0.75, 1);
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> render = [command renderCommandEncoderWithDescriptor:pass];
    [render endEncoding];
    [command presentDrawable:drawable];
    [command commit];
    [command waitUntilCompleted];
    if(command.error || values[0] != 9 || values[1] != 13) return 9;
    if(api && !api->EndFrameCapture(nullptr, nullptr)) return 10;
    printf("frame Shared placement: %u %u\n", values[0], values[1]);
    [buffer release];
    [queue release];
    [heap release];
    [pipeline release];
    [function release];
    [library release];
    return 0;
  }
}
