// SPDX-License-Identifier: MIT
// Exercise Native recursive drawable queries for all presentation entry points.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc, char **argv)
{
  if(argc != 5) return 1;
  const unsigned mode=atoi(argv[1]), width=atoi(argv[2]), height=atoi(argv[3]);
  if(mode>4 || !width || !height || width>512 || height>512) return 1;
  @autoreleasepool
  {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    auto queue=[device newCommandQueue];
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;
    layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=NO;
    layer.drawableSize=CGSizeMake(width,height);
    RENDERDOC_API_1_7_0 *api=nullptr;
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    if(get && get(eRENDERDOC_API_Version_1_7_0,(void **)&api)!=1) return 2;
    if(api) {api->SetCaptureFilePathTemplate(argv[4]);api->StartFrameCapture(nullptr,nullptr);}
    auto drawable=[layer nextDrawable];if(!drawable) return 3;
    id<MTLTexture> texture=drawable.texture;
    if(texture.device!=device || layer.device!=device || drawable.texture!=texture) return 4;
    const NSUInteger row=((width*4+255)/256)*256;
    auto read=[device newBufferWithLength:row*height options:MTLResourceStorageModeShared];
    memset(read.contents,0x5a,row*height);
    auto command=[queue commandBuffer];
    auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=texture;
    pass.colorAttachments[0].loadAction=MTLLoadActionClear;
    pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    pass.colorAttachments[0].clearColor=MTLClearColorMake(1,0,1,1);
    [[command renderCommandEncoderWithDescriptor:pass] endEncoding];
    auto blit=[command blitCommandEncoder];
    [blit copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
      sourceSize:MTLSizeMake(width,height,1) toBuffer:read destinationOffset:0
      destinationBytesPerRow:row destinationBytesPerImage:row*height];
    [blit endEncoding];
    if(mode==3) [command presentDrawable:drawable];
    else if(mode==4) [command presentDrawable:drawable afterMinimumDuration:0];
    else [command addScheduledHandler:^(id<MTLCommandBuffer> buffer) {
      if(mode==0) [drawable present];
      else if(mode==1) [drawable presentAtTime:CACurrentMediaTime()];
      else [drawable presentAfterMinimumDuration:0];
    }];
    [command commit];[command waitUntilCompleted];
    if(command.error || command.status!=MTLCommandBufferStatusCompleted) return 5;
    // Presentation retires the drawable registration. Check held object identity
    // and a new live acquisition, rather than require a retired getter's proxy.
    auto next=[layer nextDrawable];
    if(texture.device!=device || layer.device!=device || !next || next.texture.device!=device) return 6;
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 7;
    const unsigned char pixel[4]={255,0,255,255};
    for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)
      if(memcmp((unsigned char *)read.contents+y*row+x*4,pixel,4)) return 8;
    printf("Native complete-image MATCH mode=%u size=%ux%u; application identities restored\n",mode,width,height);
  }
  return 0;
}
