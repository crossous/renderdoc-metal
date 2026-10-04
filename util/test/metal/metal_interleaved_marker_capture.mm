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
    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLBuffer> a = [device newBufferWithLength:64 options:MTLResourceStorageModeShared];
    id<MTLBuffer> b = [device newBufferWithLength:64 options:MTLResourceStorageModeShared];
    a.label = @"Marker A output"; b.label = @"Marker B output";
    memset(a.contents, 0, 64); memset(b.contents, 0, 64);
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0, (void **)&api)) return 2;
      api->SetCaptureFilePathTemplate(path);
      api->StartFrameCapture(nullptr, nullptr);
    }
    id<MTLCommandBuffer> ca = [queue commandBuffer], cb = [queue commandBuffer];
    id<MTLBlitCommandEncoder> ea = [ca blitCommandEncoder];
    [ea pushDebugGroup:@"Owner A"];
    id<MTLBlitCommandEncoder> eb = [cb blitCommandEncoder];
    [eb pushDebugGroup:@"Owner B"];
    // Return to A while B remains open: recording a marker must select A first.
    [ea insertDebugSignpost:@"Signpost A"];
    [ea pushDebugGroup:@"Nested A"];
    [ea fillBuffer:a range:NSMakeRange(0,64) value:17];
    [eb insertDebugSignpost:@"Signpost B"];
    [eb fillBuffer:b range:NSMakeRange(0,64) value:34];
    [eb popDebugGroup]; [eb endEncoding];
    [ea popDebugGroup]; [ea popDebugGroup]; [ea endEncoding];
    [cb commit]; [ca commit]; [ca waitUntilCompleted]; [cb waitUntilCompleted];
    if(ca.error || cb.error) return 4;
    for(unsigned i=0;i<64;i++)
      if(((const unsigned char *)a.contents)[i]!=17 || ((const unsigned char *)b.contents)[i]!=34) return 5;
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;
    layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=NO;
    layer.drawableSize=CGSizeMake(2,2);
    id<CAMetalDrawable> drawable=[layer nextDrawable];
    MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=drawable.texture;
    pass.colorAttachments[0].loadAction=MTLLoadActionClear;
    pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    id<MTLCommandBuffer> present=[queue commandBuffer];
    [[present renderCommandEncoderWithDescriptor:pass] endEncoding];
    [present presentDrawable:drawable];[present commit];[present waitUntilCompleted];
    if(present.error)return 7;
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 6;
    puts("PASS Native interleaved marker owners and 64-byte outputs 17/34");
  }
  return 0;
}
