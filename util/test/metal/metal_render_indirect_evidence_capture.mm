// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <initializer_list>
#include "renderdoc/api/app/renderdoc_app.h"

int main()
{
  @autoreleasepool {
    const bool indexed=getenv("RENDERDOC_METAL_RENDER_INDIRECT_INDEXED")!=nullptr;
    const bool parallel=getenv("RENDERDOC_METAL_RENDER_INDIRECT_PARALLEL")!=nullptr;
    const bool unretained=getenv("RENDERDOC_METAL_RENDER_INDIRECT_UNRETAINED")!=nullptr;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();NSError *error=nil;
    id<MTLLibrary> library=[device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 triangle(uint index [[vertex_id]]) {
  float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return float4(p[index%3],0,1);
}
fragment float4 color(){return float4(1,0.25,0.5,1);}
)MSL" options:nil error:&error];
    auto pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[library newFunctionWithName:@"triangle"];
    pd.fragmentFunction=[library newFunctionWithName:@"color"];pd.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;
    id<MTLRenderPipelineState> pipeline=[device newRenderPipelineStateWithDescriptor:pd error:&error];
    uint32_t initial[24]={};initial[4]=3;initial[5]=1;initial[16]=6;initial[17]=1;
    if(indexed){initial[8]=2;initial[20]=2;}else{initial[7]=2;initial[19]=2;}
    auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;
    hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=65536;
    id<MTLHeap> heap=[device newHeapWithDescriptor:hd];
    id<MTLBuffer> arguments=[heap newBufferWithLength:sizeof(initial) options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:0];
    id<MTLBuffer> upload=[device newBufferWithBytes:initial length:sizeof(initial) options:MTLResourceStorageModeShared];
    const uint16_t indices[6]={0,1,2,0,1,2};
    id<MTLBuffer> index=[device newBufferWithBytes:indices length:sizeof(indices) options:MTLResourceStorageModeShared];
    if(!pipeline||!arguments||!upload||!index)return 2;
    id<MTLCommandQueue> queue=[device newCommandQueue];
    id<MTLCommandBuffer> init=[queue commandBuffer];id<MTLBlitCommandEncoder> blit=[init blitCommandEncoder];
    [blit copyFromBuffer:upload sourceOffset:0 toBuffer:arguments destinationOffset:0 size:sizeof(initial)];
    [blit endEncoding];[init commit];[init waitUntilCompleted];if(init.error)return 3;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH")) {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 4;
      api->SetCaptureFilePathTemplate(path);
    }
    auto layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=NO;
    layer.drawableSize=CGSizeMake(2,2);
    if(api)api->StartFrameCapture(nullptr,nullptr);
    id<MTLCommandBuffer> command=unretained?[queue commandBufferWithUnretainedReferences]:[queue commandBuffer];
    id<CAMetalDrawable> drawable=[layer nextDrawable];if(!drawable)return 5;
    auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=drawable.texture;
    pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    id<MTLParallelRenderCommandEncoder> parent=parallel?[command parallelRenderCommandEncoderWithDescriptor:pass]:nil;
    id<MTLRenderCommandEncoder> render=parallel?[parent renderCommandEncoder]:[command renderCommandEncoderWithDescriptor:pass];
    [render setRenderPipelineState:pipeline];
    if(getenv("RENDERDOC_METAL_RENDER_INDIRECT_WRITE_ALIAS"))
      [render useResource:arguments usage:MTLResourceUsageRead|MTLResourceUsageWrite];
    if(api&&!getenv("RENDERDOC_METAL_RENDER_INDIRECT_UNDECLARED")) {
      RENDERDOC_AnnotationValue declaration={};declaration.uint32=1;
      const uint32_t result=api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)render,"metal.renderWritesDeclared",eRENDERDOC_UInt32,0,&declaration);
      if(result){fprintf(stderr,"render write declaration rejected: %u\n",result);[render endEncoding];if(parent)[parent endEncoding];return 6;}
    }
    for(NSUInteger offset:{NSUInteger(16),NSUInteger(64)})
      if(indexed)[render drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexType:MTLIndexTypeUInt16 indexBuffer:index indexBufferOffset:0 indirectBuffer:arguments indirectBufferOffset:offset];
      else[render drawPrimitives:MTLPrimitiveTypeTriangle indirectBuffer:arguments indirectBufferOffset:offset];
    [render endEncoding];if(parent)[parent endEncoding];
    blit=[command blitCommandEncoder];[blit fillBuffer:arguments range:NSMakeRange(0,sizeof(initial)) value:0];[blit endEncoding];
    [command presentDrawable:drawable];[command commit];[command waitUntilCompleted];if(command.error)return 7;
    uint8_t pixels[16]={};[drawable.texture getBytes:pixels bytesPerRow:8 fromRegion:MTLRegionMake2D(0,0,2,2) mipmapLevel:0];
    for(unsigned i=0;i<16;i+=4)if(pixels[i]!=128||pixels[i+1]!=64||pixels[i+2]!=255||pixels[i+3]!=255)return 8;
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 9;
    printf("PASS Native render indirect 3/6, tail-zero, every pixel, indexed=%u parallel=%u unretained=%u\n",indexed,parallel,unretained);
  }
  return 0;
}
