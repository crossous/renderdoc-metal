// SPDX-License-Identifier: MIT
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc,char **argv)
{
  @autoreleasepool
  {
    if(argc<2||argc>3)return 1;
    bool late=argc==3;
    bool withBytes=late&&!strcmp(argv[2],"late-bytes");
    if(late&&!withBytes&&strcmp(argv[2],"late"))return 1;
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(get)get(eRENDERDOC_API_Version_1_7_0,(void**)&api);
    auto d=MTLCreateSystemDefaultDevice(); if(![d supportsRasterizationRateMapWithLayerCount:1])return 77;
    float horizontal[]={.5,.5}, vertical[]={.75,.75};
    auto ld=[[MTLRasterizationRateLayerDescriptor alloc] initWithSampleCount:MTLSizeMake(2,2,0)
                                                    horizontal:horizontal vertical:vertical];
    auto md=[MTLRasterizationRateMapDescriptor rasterizationRateMapDescriptorWithScreenSize:MTLSizeMake(400,300,0) layer:ld];
    auto map=[d newRasterizationRateMapWithDescriptor:md];
    auto physical=[map physicalSizeForLayer:0];
    auto align=map.parameterBufferSizeAndAlign;
    auto params=[d newBufferWithLength:align.size options:MTLResourceStorageModeShared]; params.label=@"VRR coordinate decoder parameters";
    [map copyParameterDataToBuffer:params offset:0];
    NSError *error=nil;
    NSString *source=@"#include <metal_stdlib>\nusing namespace metal; struct V {float4 p [[position]];}; vertex V vs(uint i [[vertex_id]]) {float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return {float4(p[i],0,1)};} fragment float4 fs(){return float4(.25,.5,.75,1);}";
    auto lib=[d newLibraryWithSource:source options:nil error:&error];
    auto pd=[MTLRenderPipelineDescriptor new]; pd.vertexFunction=[lib newFunctionWithName:@"vs"];
    pd.fragmentFunction=[lib newFunctionWithName:@"fs"];pd.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;
    auto pso=[d newRenderPipelineStateWithDescriptor:pd error:&error]; if(!pso){fprintf(stderr,"%s\n",error.description.UTF8String);return 3;}
    auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:physical.width height:physical.height mipmapped:NO];
    td.storageMode=MTLStorageModePrivate; td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
    auto target=[d newTextureWithDescriptor:td];target.label=@"VRR physical render target";
    id<MTLBuffer> copied=late?nil:[d newBufferWithLength:align.size options:MTLResourceStorageModeShared];
    copied.label=@"Copied VRR parameters";
    auto q=[d newCommandQueue];
    if(api){api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);}
    auto layer=[CAMetalLayer layer];layer.device=d;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.drawableSize=CGSizeMake(2,2);layer.framebufferOnly=NO;auto drawable=[layer nextDrawable];if(!drawable)return 4;
    auto cb=[q commandBuffer];auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
    pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.rasterizationRateMap=map;
    auto r=[cb renderCommandEncoderWithDescriptor:pass];r.label=@"VRR enabled draw";
    [r setRenderPipelineState:pso];[r setViewport:MTLViewport{0,0,400,300,0,1}];
    [r drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[r endEncoding];
    if(late)
    {
      std::vector<unsigned char> initial(align.size,0xa5);
      copied=withBytes?[d newBufferWithBytes:initial.data() length:align.size options:MTLResourceStorageModeShared]:
                       [d newBufferWithLength:align.size options:MTLResourceStorageModeShared];
      copied.label=@"Copied VRR parameters";
      if(!withBytes)memset(copied.contents,0xa5,align.size);
    }
    // Keep the public coordinate-decoder buffer in the capture and verify its copy path.
    auto blit=[cb blitCommandEncoder];
    [blit copyFromBuffer:params sourceOffset:0 toBuffer:copied destinationOffset:0 size:align.size];
    [blit endEncoding];
    pass.rasterizationRateMap=nil;pass.colorAttachments[0].texture=drawable.texture;
    auto plain=[cb renderCommandEncoderWithDescriptor:pass];plain.label=@"VRR disabled pass";[plain endEncoding];
    [cb presentDrawable:drawable];[cb commit];[cb waitUntilCompleted];
    if(cb.error){fprintf(stderr,"%s\n",cb.error.description.UTF8String);return 5;}
    if(memcmp(params.contents,copied.contents,align.size))return 7;
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 6;
    printf("PASS VRR native: logical 400x300 physical %lux%lu\n",physical.width,physical.height);
  }
}
