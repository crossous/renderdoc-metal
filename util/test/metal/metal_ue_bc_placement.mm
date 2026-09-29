// SPDX-License-Identifier: MIT
// Native and injected probe for UE's BC1/BC5 placement textures.
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
    const MTLPixelFormat formats[] = {MTLPixelFormatBC1_RGBA,
                                      MTLPixelFormatBC1_RGBA_sRGB,
                                      MTLPixelFormatBC5_RGUnorm};
    const uint8_t red[8] = {0, 0xf8, 0, 0, 0, 0, 0, 0};
    const uint8_t blue[8] = {0x1f, 0, 0, 0, 0, 0, 0, 0};
    const uint8_t rg[16] = {255, 0, 0, 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0};
    const uint8_t *blocks[] = {red, blue, rg};
    const NSUInteger sizes[] = {8, 8, 16};
    MTLTextureDescriptor *descs[3] = {};
    NSUInteger offsets[3] = {};
    NSUInteger end = 0;
    for(int i = 0; i < 3; i++)
    {
      descs[i] = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:formats[i]
                                                                      width:8 height:8 mipmapped:YES];
      descs[i].storageMode = MTLStorageModePrivate;
      descs[i].hazardTrackingMode = MTLHazardTrackingModeTracked;
      descs[i].usage = MTLTextureUsageShaderRead;
      MTLSizeAndAlign layout = [device heapTextureSizeAndAlignWithDescriptor:descs[i]];
      if(!layout.size || !layout.align) return 3;
      offsets[i] = (end + layout.align - 1) / layout.align * layout.align;
      end = offsets[i] + layout.size;
      printf("format %u layout %zu/%zu offset %zu\n", (unsigned)formats[i],
             layout.size, layout.align, offsets[i]);
    }
    MTLHeapDescriptor *hd = [[MTLHeapDescriptor alloc] init];
    hd.type = MTLHeapTypePlacement;
    hd.storageMode = MTLStorageModePrivate;
    hd.hazardTrackingMode = MTLHazardTrackingModeTracked;
    hd.size = end < 4096 ? 4096 : end;
    id<MTLHeap> heap = [device newHeapWithDescriptor:hd];
    [hd release];
    if(!heap) return 4;
    id<MTLTexture> tex[3] = {};
    for(int i = 0; i < 3; i++)
    {
      tex[i] = [heap newTextureWithDescriptor:descs[i] offset:offsets[i]];
      if(!tex[i] || tex[i].heapOffset != offsets[i]) return 5;
    }
    id<MTLBuffer> upload = [device newBufferWithLength:3 * 4096
                                             options:MTLResourceStorageModeShared];
    id<MTLBuffer> readback = [device newBufferWithLength:3 * 4096
                                               options:MTLResourceStorageModeShared];
    if(!upload || !readback) return 6;
    for(int i = 0; i < 3; i++)
      for(int block = 0; block < 4; block++)
        memcpy((uint8_t *)upload.contents + i * 4096 + block * sizes[i], blocks[i], sizes[i]);

    NSString *source = @R"METAL(
#include <metal_stdlib>
using namespace metal;
vertex float4 vs_bc(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_bc(texture2d<float> a [[texture(0)]],
                       texture2d<float> b [[texture(1)]],
                       texture2d<float> c [[texture(2)]]) {
  constexpr sampler s(coord::pixel, filter::nearest);
  return float4(a.sample(s,float2(1,1)).r * 0.5,
                c.sample(s,float2(1,1)).g,
                b.sample(s,float2(1,1)).b, 1);
}
)METAL";
    NSError *error = nil;
    id<MTLLibrary> lib = [device newLibraryWithSource:source options:nil error:&error];
    if(!lib) { fprintf(stderr,"library: %s\n",error.localizedDescription.UTF8String); return 7; }
    MTLRenderPipelineDescriptor *pd = [[MTLRenderPipelineDescriptor alloc] init];
    pd.vertexFunction = [lib newFunctionWithName:@"vs_bc"];
    pd.fragmentFunction = [lib newFunctionWithName:@"fs_bc"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    id<MTLRenderPipelineState> pipeline =
        [device newRenderPipelineStateWithDescriptor:pd error:&error];
    [pd release];
    if(!pipeline) { fprintf(stderr,"pipeline: %s\n",error.localizedDescription.UTF8String); return 8; }
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = device;
    layer.framebufferOnly = NO;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.drawableSize = CGSizeMake(4,4);
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if(!drawable) return 9;
    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLCommandBuffer> initial = [queue commandBuffer];
    id<MTLBlitCommandEncoder> initialBlit = [initial blitCommandEncoder];
    for(int i = 0; i < 3; i++)
    {
      for(int mip = 0; mip < 4; mip++)
      {
        NSUInteger width = 8 >> mip;
        if(!width) width = 1;
        NSUInteger blocks = (width + 3) / 4;
        NSUInteger pitch = sizes[i] * blocks;
        [initialBlit copyFromBuffer:upload sourceOffset:i * 4096 sourceBytesPerRow:pitch
        sourceBytesPerImage:pitch * blocks sourceSize:MTLSizeMake(width,width,1)
        toTexture:tex[i] destinationSlice:0 destinationLevel:mip
        destinationOrigin:MTLOriginMake(0,0,0)];
      }
    }
    [initialBlit endEncoding];
    [initial commit];
    [initial waitUntilCompleted];
    if(initial.error) return 9;
    RENDERDOC_API_1_0_0 *api = nullptr;
    const char *prefix = getenv("RENDERDOC_METAL_CAPTURE_PATH");
    if(prefix)
    {
      auto getAPI = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!getAPI || getAPI(eRENDERDOC_API_Version_1_0_0,(void **)&api) != 1) return 10;
      api->SetCaptureFilePathTemplate(prefix);
      api->StartFrameCapture(nullptr,nullptr);
    }
    id<MTLCommandBuffer> command = [queue commandBuffer];
    if(!getenv("RENDERDOC_METAL_BC_INITIAL_ONLY"))
    {
      id<MTLBlitCommandEncoder> blit = [command blitCommandEncoder];
      for(int i = 0; i < 3; i++)
      {
        NSUInteger pitch = sizes[i] * 2;
        [blit copyFromBuffer:upload sourceOffset:i * 4096 sourceBytesPerRow:pitch
        sourceBytesPerImage:pitch * 2 sourceSize:MTLSizeMake(8,8,1)
        toTexture:tex[i] destinationSlice:0 destinationLevel:0
        destinationOrigin:MTLOriginMake(0,0,0)];
        [blit copyFromTexture:tex[i] sourceSlice:0 sourceLevel:0
        sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(8,8,1)
        toBuffer:readback destinationOffset:i * 4096 destinationBytesPerRow:pitch
        destinationBytesPerImage:pitch * 2];
      }
      [blit endEncoding];
    }
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> render = [command renderCommandEncoderWithDescriptor:pass];
    [render setRenderPipelineState:pipeline];
    for(int i = 0; i < 3; i++) [render setFragmentTexture:tex[i] atIndex:i];
    [render drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [render endEncoding];
    [command presentDrawable:drawable];
    [command commit];
    [command waitUntilCompleted];
    if(command.error) { fprintf(stderr,"GPU: %s\n",command.error.localizedDescription.UTF8String); return 11; }
    uint8_t pixel[4] = {};
    [drawable.texture getBytes:pixel bytesPerRow:4
                 fromRegion:MTLRegionMake2D(1,1,1,1) mipmapLevel:0];
    printf("BC placement BGRA %u %u %u %u\n",pixel[0],pixel[1],pixel[2],pixel[3]);
    if(!getenv("RENDERDOC_METAL_BC_INITIAL_ONLY"))
      for(int i = 0; i < 3; i++)
        if(memcmp((uint8_t *)readback.contents+i*4096,blocks[i],sizes[i])) return 12;
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 13;
    if(pixel[0] != 255 || pixel[1] < 126 || pixel[1] > 130 ||
       pixel[2] < 126 || pixel[2] > 130 || pixel[3] != 255) return 14;
    return 0;
  }
}
