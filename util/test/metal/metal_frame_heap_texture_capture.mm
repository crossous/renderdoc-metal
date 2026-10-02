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
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:2 height:2 mipmapped:NO];
    descriptor.storageMode=MTLStorageModePrivate;
    descriptor.hazardTrackingMode=MTLHazardTrackingModeTracked;
    descriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
    const MTLSizeAndAlign layout=[device heapTextureSizeAndAlignWithDescriptor:descriptor];
    if(!layout.size||!layout.align||layout.size+layout.align>1024*1024)return 2;
    auto heapDescriptor=[MTLHeapDescriptor new];heapDescriptor.type=MTLHeapTypePlacement;
    heapDescriptor.storageMode=MTLStorageModePrivate;heapDescriptor.hazardTrackingMode=MTLHazardTrackingModeTracked;
    heapDescriptor.size=MAX(NSUInteger(4096),layout.align+layout.size);
    id<MTLHeap> heap=[device newHeapWithDescriptor:heapDescriptor];
    id<MTLCommandQueue> queue=[device newCommandQueue];
    id<MTLBuffer> output=[device newBufferWithLength:512 options:MTLResourceStorageModeShared];
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 3;
      api->SetCaptureFilePathTemplate(path);
    }
    if(!heap||!queue||!output)return 4;
    for(unsigned frame=0;frame<2;frame++)
    {
      memset(output.contents,0,512);
      if(api)api->StartFrameCapture(nullptr,nullptr);
      id<MTLTexture> texture=[heap newTextureWithDescriptor:descriptor offset:layout.align];
      if(!texture)return 5;
      id<MTLTexture> target=texture;
      if(getenv("RENDERDOC_METAL_FRAME_TEXTURE_VIEW"))
        target=[texture newTextureViewWithPixelFormat:MTLPixelFormatRGBA8Unorm textureType:MTLTextureType2D
          levels:NSMakeRange(0,1) slices:NSMakeRange(0,1)];
      if(!target)return 9;
      id<MTLCommandBuffer> command=[queue commandBuffer];
      auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
      pass.colorAttachments[0].storeAction=MTLStoreActionStore;
      pass.colorAttachments[0].clearColor=MTLClearColorMake((frame?173.0:41.0)/255.0,64.0/255.0,128.0/255.0,1);
      [[command renderCommandEncoderWithDescriptor:pass] endEncoding];
      id<MTLBlitCommandEncoder> blit=[command blitCommandEncoder];
      [blit copyFromTexture:target sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
        sourceSize:MTLSizeMake(2,2,1) toBuffer:output destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:512];
      [blit endEncoding];
      id<CAMetalDrawable> drawable=[layer nextDrawable];
      auto present=[MTLRenderPassDescriptor renderPassDescriptor];present.colorAttachments[0].texture=drawable.texture;
      present.colorAttachments[0].loadAction=MTLLoadActionClear;present.colorAttachments[0].storeAction=MTLStoreActionStore;
      [[command renderCommandEncoderWithDescriptor:present] endEncoding];
      [command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
      const uint8_t *bytes=(const uint8_t *)output.contents;
      if(command.error)return 6;
      for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++)
      {const auto p=bytes+y*256+x*4;if(p[0]!=(frame?173:41)||p[1]!=64||p[2]!=128||p[3]!=255)return 7;}
      if(api&&!api->EndFrameCapture(nullptr,nullptr))return 8;
      target=nil;texture=nil;
    }
    puts("PASS native frame Private placement textures, two captures, pixels 41/173");
  }
  return 0;
}
