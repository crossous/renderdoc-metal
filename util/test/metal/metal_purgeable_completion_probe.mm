// SPDX-License-Identifier: MIT
// Single native Metal Validation probe for UE-style completion before purge.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <dispatch/dispatch.h>

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
    id<MTLCommandQueue> queue = [device newCommandQueue];
    constexpr NSUInteger length = 4096;
    id<MTLBuffer> source = [device newBufferWithLength:length
                                               options:MTLResourceStorageModeShared];
    id<MTLBuffer> destination = [device newBufferWithLength:length
                                                    options:MTLResourceStorageModeShared];
    if(!device || !queue || !source || !destination)
      return 2;

    RENDERDOC_API_1_0_0 *api = nullptr;
    const char *capturePrefix = getenv("RENDERDOC_METAL_CAPTURE_PATH");
    if(capturePrefix)
    {
      auto getAPI = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!getAPI || getAPI(eRENDERDOC_API_Version_1_0_0, (void **)&api) != 1)
        return 12;
      api->SetCaptureFilePathTemplate(capturePrefix);
      api->StartFrameCapture(nullptr, nullptr);
    }

    memset(source.contents, 0x5a, length);
    id<MTLCommandBuffer> work = [queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [work blitCommandEncoder];
    if(!work || !blit)
      return 3;
    [blit copyFromBuffer:source sourceOffset:0 toBuffer:destination destinationOffset:0 size:length];
    [blit endEncoding];
    [work commit];

    // UE submits a later completion fence on the same queue and frees resources only after
    // its callback. The fence must complete all earlier work before Empty is legal.
    id<MTLCommandBuffer> fence = [queue commandBuffer];
    id<MTLSharedEvent> event = [device newSharedEvent];
    if(!fence || !event)
      return 4;
    dispatch_semaphore_t completed = dispatch_semaphore_create(0);
    [fence addCompletedHandler:^(id<MTLCommandBuffer>) {
      dispatch_semaphore_signal(completed);
    }];
    [fence encodeSignalEvent:event value:1];
    [fence commit];
    if(dispatch_semaphore_wait(completed,
                               dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)) != 0)
    {
      fprintf(stderr, "completion callback timed out\n");
      return 5;
    }
    if(work.status != MTLCommandBufferStatusCompleted ||
       fence.status != MTLCommandBufferStatusCompleted ||
       event.signaledValue != 1 || work.error || fence.error)
    {
      fprintf(stderr, "command buffer or event did not complete successfully\n");
      return 6;
    }
    for(NSUInteger i = 0; i < length; ++i)
      if(((const unsigned char *)destination.contents)[i] != 0x5a)
        return 7;

    MTLPurgeableState old = [source setPurgeableState:MTLPurgeableStateEmpty];
    if(old != MTLPurgeableStateNonVolatile)
    {
      fprintf(stderr, "unexpected previous purgeable state: %lu\n", (unsigned long)old);
      return 8;
    }
    if(!api)
    {
      [source setPurgeableState:MTLPurgeableStateNonVolatile];
      memset(source.contents, 0xa5, length);
      id<MTLCommandBuffer> refill = [queue commandBuffer];
      id<MTLBlitCommandEncoder> refillBlit = [refill blitCommandEncoder];
      if(!refill || !refillBlit)
        return 9;
      [refillBlit copyFromBuffer:source sourceOffset:0 toBuffer:destination destinationOffset:0
                         size:length];
      [refillBlit endEncoding];
      [refill commit];
      [refill waitUntilCompleted];
      if(refill.status != MTLCommandBufferStatusCompleted || refill.error)
        return 10;
      for(NSUInteger i = 0; i < length; ++i)
        if(((const unsigned char *)destination.contents)[i] != 0xa5)
          return 11;
    }

    if(api)
    {
      CAMetalLayer *layer = [CAMetalLayer layer];
      layer.device = device;
      layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
      layer.framebufferOnly = NO;
      layer.drawableSize = CGSizeMake(2, 2);
      id<CAMetalDrawable> drawable = [layer nextDrawable];
      if(!drawable) return 13;
      id<MTLCommandBuffer> present = [queue commandBuffer];
      MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture = drawable.texture;
      pass.colorAttachments[0].loadAction = MTLLoadActionClear;
      pass.colorAttachments[0].clearColor = MTLClearColorMake(0.25, 0.5, 0.75, 1);
      pass.colorAttachments[0].storeAction = MTLStoreActionStore;
      id<MTLRenderCommandEncoder> render = [present renderCommandEncoderWithDescriptor:pass];
      if(!render) return 14;
      [render endEncoding];
      [present presentDrawable:drawable];
      [present commit];
      [present waitUntilCompleted];
      if(present.error || !api->EndFrameCapture(nullptr, nullptr)) return 15;
    }

    printf("completion -> Empty: PASS capture=%u\n", api ? 1U : 0U);
    return 0;
  }
}
