// SPDX-License-Identifier: MIT
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc, char **argv) {
  @autoreleasepool {
    if(argc!=2) return 1;
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI"); RENDERDOC_API_1_7_0 *api=nullptr;
    if(get)get(eRENDERDOC_API_Version_1_7_0,(void**)&api);
    auto d=MTLCreateSystemDefaultDevice();
    auto desc=[MTLFXSpatialScalerDescriptor new]; desc.inputWidth=16; desc.inputHeight=16;
    desc.outputWidth=32; desc.outputHeight=32; desc.colorTextureFormat=MTLPixelFormatRGBA16Float;
    desc.outputTextureFormat=MTLPixelFormatRGBA16Float; desc.colorProcessingMode=MTLFXSpatialScalerColorProcessingModeLinear;
    auto fx=[desc newSpatialScalerWithDevice:d]; if(!fx)return 2;
    auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float width:16 height:16 mipmapped:NO];
    td.storageMode=MTLStorageModePrivate; td.usage=fx.colorTextureUsage|MTLTextureUsageRenderTarget;
    auto input=[d newTextureWithDescriptor:td]; input.label=@"MetalFX input 16x16";
    td.width=32; td.height=32; td.usage=fx.outputTextureUsage|MTLTextureUsageShaderRead;
    auto output=[d newTextureWithDescriptor:td]; output.label=@"MetalFX output 32x32";
    fx.colorTexture=input; fx.outputTexture=output; fx.inputContentWidth=16; fx.inputContentHeight=16;
    auto q=[d newCommandQueue]; if(api){api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);}
    auto layer=[CAMetalLayer layer]; layer.device=d; layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.drawableSize=CGSizeMake(2,2); layer.framebufferOnly=NO; auto drawable=[layer nextDrawable]; if(!drawable)return 3;
    auto cb=[q commandBuffer];
    auto pass=[MTLRenderPassDescriptor renderPassDescriptor]; pass.colorAttachments[0].texture=input;
    pass.colorAttachments[0].clearColor=MTLClearColorMake(.25,.5,.75,1); pass.colorAttachments[0].loadAction=MTLLoadActionClear;
    pass.colorAttachments[0].storeAction=MTLStoreActionStore; auto r=[cb renderCommandEncoderWithDescriptor:pass]; [r endEncoding];
    [cb pushDebugGroup:@"MetalFX Spatial: 16x16 → 32x32, Linear"]; [fx encodeToCommandBuffer:cb]; [cb popDebugGroup];
    pass.colorAttachments[0].texture=drawable.texture; auto p=[cb renderCommandEncoderWithDescriptor:pass]; [p endEncoding];
    [cb presentDrawable:drawable]; [cb commit]; [cb waitUntilCompleted];
    if(cb.status==MTLCommandBufferStatusError){fprintf(stderr,"%s\n",cb.error.description.UTF8String);return 4;}
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 5;
    if(const char *oracle=getenv("METAL_FX_ORACLE_PATH")) {
      auto readback=[d newBufferWithLength:32*32*8 options:MTLResourceStorageModeShared];
      auto read=[q commandBuffer]; auto blit=[read blitCommandEncoder];
      [blit copyFromTexture:output sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
        sourceSize:MTLSizeMake(32,32,1) toBuffer:readback destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:8192];
      [blit endEncoding]; [read commit]; [read waitUntilCompleted];
      FILE *f=fopen(oracle,"wb"); if(!f)return 6;
      bool saved=fwrite(readback.contents,1,8192,f)==8192; fclose(f); if(!saved)return 7;
    }
    puts("PASS MetalFX Spatial capture"); return 0;
  }
}
