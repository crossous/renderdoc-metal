// SPDX-License-Identifier: MIT
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include "renderdoc/api/app/renderdoc_app.h"

int main(int argc, char **argv)
{
  if(argc != 2) return 1;
  @autoreleasepool
  {
    RENDERDOC_API_1_7_0 *api = nullptr;
    auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    if(get) get(eRENDERDOC_API_Version_1_7_0, (void **)&api);
    auto d = MTLCreateSystemDefaultDevice();
    auto q = [d newCommandQueue];
    float initialColor[4] = {0, 0, 0, 1};
    auto color = [d newBufferWithBytes:initialColor length:sizeof(initialColor)
                              options:MTLResourceStorageModeShared];
    color.label = @"Existing draw color updated after encoding";
    NSError *error = nil;
    auto lib = [d newLibraryWithSource:@"#include <metal_stdlib>\nusing namespace metal; struct V {float4 p [[position]];}; vertex V vs(uint i [[vertex_id]]) {float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return {float4(p[i],0,1)};} fragment float4 fs(constant float4 &color [[buffer(0)]]) {return color;}"
                              options:nil error:&error];
    auto pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [lib newFunctionWithName:@"vs"];
    pd.fragmentFunction = [lib newFunctionWithName:@"fs"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    auto pso = [d newRenderPipelineStateWithDescriptor:pd error:&error];
    if(!pso) {fprintf(stderr, "%s\n", error.description.UTF8String); return 2;}
    auto td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                  width:32 height:32 mipmapped:NO];
    td.storageMode = MTLStorageModePrivate;
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    auto target = [d newTextureWithDescriptor:td];
    target.label = @"Future Shared regression target";
    if(api) {api->SetCaptureFilePathTemplate(argv[1]); api->StartFrameCapture(nullptr, nullptr);}
    auto layer = [CAMetalLayer layer];
    layer.device = d; layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.drawableSize = CGSizeMake(2, 2); layer.framebufferOnly = NO;
    auto drawable = [layer nextDrawable];
    if(!drawable) return 3;
    auto cb = [q commandBuffer];
    auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLBuffer> source = nil, destination = nil;
    for(unsigned draw = 0; draw < 2; draw++)
    {
      auto r = [cb renderCommandEncoderWithDescriptor:pass];
      r.label = draw ? @"Draw after future buffer copy" : @"Draw before future buffer birth";
      [r setRenderPipelineState:pso]; [r setFragmentBuffer:color offset:0 atIndex:0];
      [r drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [r endEncoding];
      if(draw == 0)
      {
        // These writes precede submission, so the first draw must see them even on a seek.
        float finalColor[4] = {.25f, .5f, .75f, 1};
        memcpy(color.contents, finalColor, sizeof(finalColor));
        unsigned char initial[256]; memset(initial, 0x11, sizeof(initial));
        source = [d newBufferWithBytes:initial length:sizeof(initial)
                                   options:MTLResourceStorageModeShared];
        source.label = @"Future Shared source with creation bytes";
        memset((unsigned char *)source.contents + 64, 0x3c, 64);
        destination = [d newBufferWithLength:sizeof(initial) options:MTLResourceStorageModeShared];
        destination.label = @"Future Shared blit destination";
        memset(destination.contents, 0xa5, sizeof(initial));
        auto b = [cb blitCommandEncoder];
        [b copyFromBuffer:source sourceOffset:32 toBuffer:destination destinationOffset:32 size:160];
        [b endEncoding];
      }
    }
    [cb presentDrawable:drawable]; [cb commit]; [cb waitUntilCompleted];
    if(cb.error) {fprintf(stderr, "%s\n", cb.error.description.UTF8String); return 4;}
    auto src = (unsigned char *)source.contents, dst = (unsigned char *)destination.contents;
    for(unsigned i = 0; i < 256; i++)
      if(src[i] != (i >= 64 && i < 128 ? 0x3c : 0x11) ||
         dst[i] != (i >= 32 && i < 192 ? src[i] : 0xa5)) return 5;
    if(api && !api->EndFrameCapture(nullptr, nullptr)) return 6;
    puts("PASS native future Shared creation bytes, CPU snapshot and partial GPU copy");
  }
}
