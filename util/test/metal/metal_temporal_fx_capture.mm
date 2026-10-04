// SPDX-License-Identifier: MIT
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc, char **argv)
{
  @autoreleasepool
  {
    if(argc!=3) return 1;
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(get) get(eRENDERDOC_API_Version_1_7_0,(void**)&api);
    auto device=MTLCreateSystemDefaultDevice();
    if(![MTLFXTemporalScalerDescriptor supportsDevice:device]) return 77;
    auto desc=[MTLFXTemporalScalerDescriptor new];
    desc.inputWidth=64; desc.inputHeight=64; desc.outputWidth=128; desc.outputHeight=128;
    desc.colorTextureFormat=MTLPixelFormatRGBA16Float; desc.outputTextureFormat=MTLPixelFormatRGBA16Float;
    desc.depthTextureFormat=MTLPixelFormatDepth32Float; desc.motionTextureFormat=MTLPixelFormatRG16Float;
    desc.requiresSynchronousInitialization=YES;
    bool autoExposure=!strcmp(argv[2],"auto");
    desc.autoExposureEnabled=autoExposure;
    bool minimal=!strcmp(argv[2],"minimal");
    desc.reactiveMaskTextureEnabled=!minimal; desc.reactiveMaskTextureFormat=MTLPixelFormatR8Unorm;
    auto fx=[desc newTemporalScalerWithDevice:device]; if(!fx) return 3;
    auto make=[&](MTLPixelFormat format,NSUInteger w,NSUInteger h,MTLTextureUsage usage,MTLStorageMode storage,NSString *label) {
      auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:NO];
      td.usage=usage; td.storageMode=storage; auto t=[device newTextureWithDescriptor:td]; t.label=label; return t;
    };
    auto color=make(desc.colorTextureFormat,64,64,fx.colorTextureUsage|MTLTextureUsageRenderTarget,MTLStorageModePrivate,@"Temporal Color");
    auto depth=make(desc.depthTextureFormat,64,64,fx.depthTextureUsage|MTLTextureUsageRenderTarget,MTLStorageModePrivate,@"Temporal Depth");
    auto motion=make(desc.motionTextureFormat,64,64,fx.motionTextureUsage,MTLStorageModeShared,@"Temporal Motion Vectors");
    auto exposure=make(MTLPixelFormatR16Float,1,1,MTLTextureUsageShaderRead,MTLStorageModeShared,@"Temporal Exposure");
    auto reactive=make(desc.reactiveMaskTextureFormat,64,64,fx.reactiveTextureUsage,MTLStorageModeShared,@"Temporal Reactive Mask");
    uint16_t mv[64*64*2];
    for(unsigned i=0;i<64*64;i++) { mv[i*2]=0x3800; mv[i*2+1]=0xb400; }
    [motion replaceRegion:MTLRegionMake2D(0,0,64,64) mipmapLevel:0 withBytes:mv bytesPerRow:64*4];
    uint16_t exp=0x3c00; [exposure replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:&exp bytesPerRow:2];
    uint8_t mask[64*64]; memset(mask,128,sizeof(mask));
    [reactive replaceRegion:MTLRegionMake2D(0,0,64,64) mipmapLevel:0 withBytes:mask bytesPerRow:64];
    fx.colorTexture=color; fx.depthTexture=depth; fx.motionTexture=motion;
    fx.exposureTexture=minimal?nil:exposure; fx.reactiveMaskTexture=minimal?nil:reactive;
    fx.inputContentWidth=64; fx.inputContentHeight=64; fx.motionVectorScaleX=1; fx.motionVectorScaleY=-1;
    fx.preExposure=1; fx.depthReversed=YES;
    auto queue=[device newCommandQueue];
    bool recovery=!strcmp(argv[2],"recovery");
    bool missing=!strcmp(argv[2],"missing")||recovery;
    if(missing)
    {
      auto cb=[queue commandBuffer]; auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=color; pass.colorAttachments[0].loadAction=MTLLoadActionClear;
      pass.colorAttachments[0].storeAction=MTLStoreActionStore; pass.colorAttachments[0].clearColor=MTLClearColorMake(.1,.2,.3,1);
      pass.depthAttachment.texture=depth; pass.depthAttachment.loadAction=MTLLoadActionClear;
      pass.depthAttachment.storeAction=MTLStoreActionStore; pass.depthAttachment.clearDepth=.75;
      [[cb renderCommandEncoderWithDescriptor:pass] endEncoding];
      fx.outputTexture=make(desc.outputTextureFormat,128,128,fx.outputTextureUsage,MTLStorageModePrivate,@"Warmup Output");
      fx.reset=YES; [fx encodeToCommandBuffer:cb]; [cb commit]; [cb waitUntilCompleted];
    }
    if(api) { api->SetCaptureFilePathTemplate(argv[1]); api->StartFrameCapture(nullptr,nullptr); }
    auto layer=[CAMetalLayer layer]; layer.device=device; layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.drawableSize=CGSizeMake(2,2); layer.framebufferOnly=NO; auto drawable=[layer nextDrawable];
    if(!drawable) return 4;
    unsigned frames=recovery?3:2;
    auto readback=[device newBufferWithLength:frames*128*128*8 options:MTLResourceStorageModeShared];
    for(unsigned i=0;i<frames;i++)
    {
      auto cb=[queue commandBuffer]; cb.label=i?@"Temporal History Frame 2":@"Temporal Frame 1";
      auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=color; pass.colorAttachments[0].loadAction=MTLLoadActionClear;
      pass.colorAttachments[0].storeAction=MTLStoreActionStore;
      pass.colorAttachments[0].clearColor=MTLClearColorMake(.25+i*.125,.5,.75,1);
      pass.depthAttachment.texture=depth; pass.depthAttachment.loadAction=MTLLoadActionClear;
      pass.depthAttachment.storeAction=MTLStoreActionStore; pass.depthAttachment.clearDepth=.75;
      [[cb renderCommandEncoderWithDescriptor:pass] endEncoding];
      fx.outputTexture=make(desc.outputTextureFormat,128,128,fx.outputTextureUsage,MTLStorageModePrivate,
                           i?@"Temporal Output 2":@"Temporal Output 1");
      fx.reset=(i==0 && !missing)||(recovery&&i==2); fx.jitterOffsetX=i?.25f:-.25f; fx.jitterOffsetY=i?-.125f:.125f;
      [fx encodeToCommandBuffer:cb];
      auto blit=[cb blitCommandEncoder];
      [blit copyFromTexture:fx.outputTexture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                sourceSize:MTLSizeMake(128,128,1) toBuffer:readback destinationOffset:i*128*128*8
                destinationBytesPerRow:128*8 destinationBytesPerImage:128*128*8];
      [blit endEncoding];
      if(i==frames-1) {
        auto pp=[MTLRenderPassDescriptor renderPassDescriptor]; pp.colorAttachments[0].texture=drawable.texture;
        pp.colorAttachments[0].loadAction=MTLLoadActionClear; pp.colorAttachments[0].storeAction=MTLStoreActionStore;
        [[cb renderCommandEncoderWithDescriptor:pp] endEncoding]; [cb presentDrawable:drawable];
      }
      [cb commit]; [cb waitUntilCompleted];
      if(cb.error) { fprintf(stderr,"%s\n",cb.error.description.UTF8String); return 5; }
    }
    if(const char *path=getenv("METAL_TEMPORAL_ORACLE"))
    {
      FILE *f=fopen(path,"wb"); if(!f) return 6;
      fwrite(readback.contents,1,readback.length,f); fclose(f);
    }
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 7;
    puts("PASS native Temporal: motion, depth, exposure, reactive mask, reset and cross-command-buffer history");
  }
}
