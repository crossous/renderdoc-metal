// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "renderdoc/api/app/renderdoc_app.h"
int main()
{
  @autoreleasepool {
    auto d=MTLCreateSystemDefaultDevice(); auto q=[d newCommandQueue];
    NSError *error=nil;
    auto lib=[d newLibraryWithSource:@"#include <metal_stdlib>\nusing namespace metal; vertex float4 vs(uint i [[vertex_id]]) { float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)}; return float4(p[i],0,1); } fragment float4 fs(){return float4(.25,.5,.75,1);}" options:nil error:&error];
    auto pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[lib newFunctionWithName:@"vs"];
    pd.fragmentFunction=[lib newFunctionWithName:@"fs"];pd.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;
    auto pipeline=[d newRenderPipelineStateWithDescriptor:pd error:&error];if(!pipeline)return 6;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(auto path=getenv("RENDERDOC_METAL_CAPTURE_PATH")) {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 2;
      api->SetCaptureFilePathTemplate(path);
    }
    auto layer=[CAMetalLayer layer];layer.device=d;layer.framebufferOnly=NO;
    layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.drawableSize=CGSizeMake(2,2);
    NSMutableArray *textures=[NSMutableArray new],*heaps=[NSMutableArray new];
    const MTLPixelFormat fmts[]={MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGB10A2Unorm,
      MTLPixelFormatR32Float,MTLPixelFormatR32Uint,MTLPixelFormatDepth32Float,
      MTLPixelFormatDepth32Float_Stencil8,MTLPixelFormatDepth32Float_Stencil8,
      MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm,
      MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm,
      MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm};
    for(unsigned i=0;i<18;i++) {
      auto desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmts[i] width:130 height:i==13?130:34 mipmapped:i==8||i==13];
      desc.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
      desc.storageMode=MTLStorageModePrivate;
      if(i==10)desc.hazardTrackingMode=MTLHazardTrackingModeUntracked;
      if(i==8){desc.textureType=MTLTextureType2DArray;desc.arrayLength=2;}
      if(i==11||i==12){desc.textureType=MTLTextureType3D;desc.depth=4;}
      if(i==13)desc.textureType=MTLTextureTypeCube;
      id<MTLTexture> t;
      if(i==15||i==16){auto hd=[MTLHeapDescriptor new];hd.storageMode=MTLStorageModePrivate;
        hd.hazardTrackingMode=i==15?MTLHazardTrackingModeUntracked:MTLHazardTrackingModeDefault;
        hd.type=i==15?MTLHeapTypeAutomatic:MTLHeapTypePlacement;
        auto sa=[d heapTextureSizeAndAlignWithDescriptor:desc];hd.size=((sa.size+sa.align-1)/sa.align)*sa.align;
        hd.size=MAX(4096,hd.size);auto heap=[d newHeapWithDescriptor:hd];if(!heap)return 7;
        [heaps addObject:heap];t=i==15?[heap newTextureWithDescriptor:desc]:[heap newTextureWithDescriptor:desc offset:0];
      }else t=[d newTextureWithDescriptor:desc];if(!t)return 3;
      t.label=[NSString stringWithFormat:@"Discard %u",i];[textures addObject:t];
    }
    // Defined neighbours prove that a 3D plane or cube face is not confused
    // with a texture array slice when diagnostics select their subresource.
    auto init=[q commandBuffer];
    for(unsigned i:{11U,12U,13U})for(unsigned plane=0;plane<(i==13?6U:4U);plane++){
      auto p=[MTLRenderPassDescriptor renderPassDescriptor];p.colorAttachments[0].texture=textures[i];
      p.colorAttachments[0].loadAction=MTLLoadActionClear;p.colorAttachments[0].clearColor=MTLClearColorMake(.25,.5,.75,1);
      p.colorAttachments[0].storeAction=MTLStoreActionStore;
      if(i==13){p.colorAttachments[0].slice=plane;p.colorAttachments[0].level=1;}
      else p.colorAttachments[0].depthPlane=plane;
      [[init renderCommandEncoderWithDescriptor:p] endEncoding];
    }
    [init commit];[init waitUntilCompleted];if(init.error)return 8;
    const float rates[]={.5f,1.f,.5f};
    auto rl=[[MTLRasterizationRateLayerDescriptor alloc] initWithSampleCount:MTLSizeMake(3,3,0) horizontal:rates vertical:rates];
    auto rm=[d newRasterizationRateMapWithDescriptor:[MTLRasterizationRateMapDescriptor rasterizationRateMapDescriptorWithScreenSize:MTLSizeMake(130,34,1) layer:rl]];
    if(!rm)return 9;
    if(api)api->StartFrameCapture(nullptr,nullptr);
    auto cb=[q commandBufferWithUnretainedReferences];
    for(unsigned i=0;i<18;i++) {
      auto p=[MTLRenderPassDescriptor renderPassDescriptor];id<MTLTexture> t=textures[i];
      if(i>=4 && i<=6) {
        p.depthAttachment.texture=t;p.depthAttachment.loadAction=i==6?MTLLoadActionClear:MTLLoadActionDontCare;
        p.depthAttachment.clearDepth=.25;p.depthAttachment.storeAction=MTLStoreActionStore;
        if(i>=5){p.stencilAttachment.texture=t;p.stencilAttachment.loadAction=i==5?MTLLoadActionClear:MTLLoadActionDontCare;
          p.stencilAttachment.clearStencil=37;p.stencilAttachment.storeAction=MTLStoreActionStore;}
      } else {
        p.colorAttachments[0].texture=t;p.colorAttachments[0].loadAction=i==9?MTLLoadActionClear:MTLLoadActionDontCare;
        p.colorAttachments[0].clearColor=MTLClearColorMake(.25,.5,.75,1);
        p.colorAttachments[0].storeAction=i==7?MTLStoreActionUnknown:i==12||i==17?MTLStoreActionDontCare:MTLStoreActionStore;
        if(i==8){p.colorAttachments[0].level=1;p.colorAttachments[0].slice=1;}
        if(i==11||i==12)p.colorAttachments[0].depthPlane=2;
        if(i==13){p.colorAttachments[0].level=1;p.colorAttachments[0].slice=4;}
        if(i==14)p.rasterizationRateMap=rm;
      }
      if(i==7) {
        auto parent=[cb parallelRenderCommandEncoderWithDescriptor:p];auto child=[parent renderCommandEncoder];
        [child endEncoding];[parent setColorStoreAction:MTLStoreActionDontCare atIndex:0];[parent endEncoding];
      } else {
        auto e=[cb renderCommandEncoderWithDescriptor:p];
        if(i==0 || i==10 || i==17){[e setRenderPipelineState:pipeline];[e setScissorRect:MTLScissorRect{0,0,4,4}];
          [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];}
        [e endEncoding];
      }
    }
    auto draw=[layer nextDrawable];auto p=[MTLRenderPassDescriptor renderPassDescriptor];
    p.colorAttachments[0].texture=draw.texture;p.colorAttachments[0].loadAction=MTLLoadActionClear;
    p.colorAttachments[0].storeAction=MTLStoreActionStore;
    [[cb renderCommandEncoderWithDescriptor:p] endEncoding];[cb presentDrawable:draw];
    [cb commit];[cb waitUntilCompleted];if(cb.error){fprintf(stderr,"%s\n",cb.error.description.UTF8String);return 4;}
    if(api && !api->EndFrameCapture(nullptr,nullptr))return 5;
    puts("PASS Native serial/parallel, unretained, typed colour, independent depth/stencil and mip/array DontCare passes");
  }
}
