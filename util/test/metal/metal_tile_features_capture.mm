// SPDX-License-Identifier: MIT
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc, char **argv)
{
  if(argc!=4) return 1;
  @autoreleasepool {
    bool tile=!strcmp(argv[1],"tile"), memoryless=!strcmp(argv[1],"memoryless"), combined=!strcmp(argv[1],"combined");
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(get) get(eRENDERDOC_API_Version_1_7_0,(void**)&api);
    auto d=MTLCreateSystemDefaultDevice(); NSError *error=nil;
    id<MTLLibrary> lib;
    if(getenv("METAL_FEATURES_SOURCE")) {
      auto source=[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[2]] encoding:NSUTF8StringEncoding error:&error];
      lib=[d newLibraryWithSource:source options:nil error:&error];
    } else lib=[d newLibraryWithFile:[NSString stringWithUTF8String:argv[2]] error:&error];
    if(!lib) { fprintf(stderr,"%s\n",error.description.UTF8String); return 2; }
    auto pd=[MTLRenderPipelineDescriptor new]; pd.vertexFunction=[lib newFunctionWithName:@"vs"];
    pd.fragmentFunction=[lib newFunctionWithName:@"fetch"];
    pd.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;
    pd.colorAttachments[1].pixelFormat=MTLPixelFormatRGBA8Unorm;
    pd.label=@"Programmable blending: fetch colour 0, ROG 2";
    if(combined) {
      pd.colorAttachments[0].blendingEnabled=YES;
      pd.colorAttachments[0].sourceRGBBlendFactor=MTLBlendFactorOne;
      pd.colorAttachments[0].destinationRGBBlendFactor=MTLBlendFactorOne;
      pd.colorAttachments[0].sourceAlphaBlendFactor=MTLBlendFactorOne;
      pd.colorAttachments[0].destinationAlphaBlendFactor=MTLBlendFactorZero;
      pd.label=@"Framebuffer fetch + fixed-function additive blending";
    }
    auto fetch=[d newRenderPipelineStateWithDescriptor:pd error:&error];
    pd.colorAttachments[0].blendingEnabled=NO;
    pd.fragmentFunction=[lib newFunctionWithName:@"plain"]; pd.label=@"Plain fragment: no fetch or ROG";
    auto plain=[d newRenderPipelineStateWithDescriptor:pd error:&error];
    pd.fragmentFunction=[lib newFunctionWithName:@"seed"]; pd.colorAttachments[1].pixelFormat=MTLPixelFormatInvalid;
    pd.label=@"Seed tile imageblock"; auto seed=[d newRenderPipelineStateWithDescriptor:pd error:&error];
    auto tpd=[MTLTileRenderPipelineDescriptor new]; tpd.tileFunction=[lib newFunctionWithName:@"tile_image"];
    tpd.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm; tpd.threadgroupSizeMatchesTileSize=YES;
    tpd.maxTotalThreadsPerThreadgroup=256; tpd.label=@"Imageblock Tile Pipeline";
    auto tp=[d newRenderPipelineStateWithTileDescriptor:tpd options:MTLPipelineOptionArgumentInfo reflection:nil error:&error];
    if(!fetch||!seed||!plain||!tp) { fprintf(stderr,"%s\n",error.description.UTF8String); return 3; }
    auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:32 height:32 mipmapped:NO];
    td.storageMode=memoryless?MTLStorageModeMemoryless:MTLStorageModePrivate; td.usage=MTLTextureUsageRenderTarget;
    auto colour=[d newTextureWithDescriptor:td]; colour.label=memoryless?@"Memoryless fetch attachment":@"Fetch and Tile attachment";
    td.storageMode=MTLStorageModePrivate; td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
    auto output=[d newTextureWithDescriptor:td]; output.label=@"Stored programmable blend result";
    auto counter=[d newBufferWithLength:16 options:MTLResourceStorageModeShared]; counter.label=@"Tile counter";
    memset(counter.contents,0,16);
    auto q=[d newCommandQueue];
    if(api) { api->SetCaptureFilePathTemplate(argv[3]); api->StartFrameCapture(nullptr,nullptr); }
    auto layer=[CAMetalLayer layer]; layer.device=d; layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.drawableSize=CGSizeMake(2,2); layer.framebufferOnly=NO; auto drawable=[layer nextDrawable];
    if(!drawable) return 4;
    auto cb=[q commandBuffer]; cb.label=@"Metal Tile / fetch / memoryless / ROG fixture";
    auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=colour; pass.colorAttachments[0].loadAction=MTLLoadActionClear;
    pass.colorAttachments[0].clearColor=MTLClearColorMake(.25,.25,.25,1);
    pass.colorAttachments[0].storeAction=memoryless?MTLStoreActionDontCare:MTLStoreActionStore;
    id<MTLRenderCommandEncoder> r;
    if(tile) {
      pass.tileWidth=16; pass.tileHeight=16; pass.threadgroupMemoryLength=32;
      pass.imageblockSampleLength=tp.imageblockSampleLength;
      r=[cb renderCommandEncoderWithDescriptor:pass]; r.label=@"Draw → Tile imageblock → Draw";
      [r pushDebugGroup:@"Seed imageblock"]; [r setRenderPipelineState:seed];
      [r drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [r popDebugGroup];
      [r pushDebugGroup:@"Tile imageblock + threadgroup memory"]; [r setRenderPipelineState:tp];
      [r setTileBuffer:counter offset:4 atIndex:0]; __fp16 delta=.125;
      [r setTileBytes:&delta length:sizeof(delta) atIndex:1];
      [r setThreadgroupMemoryLength:16 offset:16 atIndex:0];
      [r dispatchThreadsPerTile:MTLSizeMake(16,16,1)]; [r popDebugGroup];
      [r pushDebugGroup:@"Return to graphics pipeline"]; [r setRenderPipelineState:seed];
      [r setScissorRect:MTLScissorRect{0,0,1,1}];
      [r drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [r popDebugGroup];
    } else {
      pass.colorAttachments[1].texture=output; pass.colorAttachments[1].loadAction=MTLLoadActionClear;
      pass.colorAttachments[1].storeAction=MTLStoreActionStore;
      r=[cb renderCommandEncoderWithDescriptor:pass]; r.label=memoryless?@"Memoryless programmable blend":@"Private programmable blend";
      [r setRenderPipelineState:fetch]; __fp16 delta[4]={.125,.125,.125,0};
      [r setFragmentBytes:delta length:sizeof(delta) atIndex:0];
      for(unsigned i=0;i<2;i++) { [r pushDebugGroup:i?@"Fetch draw 2":@"Fetch draw 1"];
        [r drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [r popDebugGroup]; }
    }
    [r endEncoding];
    // A later unrelated entry from the same library must not inherit fetch/ROG metadata.
    if(!tile) {
      pass.colorAttachments[0].texture=output; pass.colorAttachments[0].storeAction=MTLStoreActionStore;
      pass.colorAttachments[1].texture=nil; pd.colorAttachments[1].pixelFormat=MTLPixelFormatInvalid;
      pd.fragmentFunction=[lib newFunctionWithName:@"plain"]; plain=[d newRenderPipelineStateWithDescriptor:pd error:&error];
      auto check=[cb renderCommandEncoderWithDescriptor:pass]; [check setRenderPipelineState:plain];
      [check setScissorRect:MTLScissorRect{0,0,1,1}]; [check drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [check endEncoding];
    }
    auto pp=[MTLRenderPassDescriptor renderPassDescriptor]; pp.colorAttachments[0].texture=drawable.texture;
    pp.colorAttachments[0].loadAction=MTLLoadActionClear; pp.colorAttachments[0].storeAction=MTLStoreActionStore;
    auto pe=[cb renderCommandEncoderWithDescriptor:pp]; [pe endEncoding]; [cb presentDrawable:drawable];
    [cb commit]; [cb waitUntilCompleted];
    if(cb.status==MTLCommandBufferStatusError) { fprintf(stderr,"%s\n",cb.error.description.UTF8String); return 5; }
    if(tile && ((unsigned*)counter.contents)[1]!=4) return 6;
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 7;
    printf("PASS native %s; tile sample length=%lu\n",argv[1],(unsigned long)tp.imageblockSampleLength);
  }
  return 0;
}
