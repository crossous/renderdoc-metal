// SPDX-License-Identifier: MIT
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include "renderdoc/api/app/renderdoc_app.h"
int main()
{
  @autoreleasepool {
    NSString *source = @R"MSL(
#include <metal_stdlib>
using namespace metal;
constant float shade [[function_constant(0)]];
vertex float4 vs(uint i [[vertex_id]]) {
  float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[i],0,1);
}
fragment float4 fs() { return float4(shade,0.5,0.75,1); }
kernel void cs(device uint *out [[buffer(0)]],uint i [[thread_position_in_grid]]) {
  out[i]=100+i;
}
)MSL";
    if(const char *path=getenv("RENDERDOC_METAL_EDIT_SOURCE"))
      return [source writeToFile:[NSString stringWithUTF8String:path] atomically:YES encoding:NSUTF8StringEncoding error:nil]?0:2;
    auto d=MTLCreateSystemDefaultDevice(); NSError *error=nil;
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void**)&api)) return 4;
    api->SetCaptureFilePathTemplate(getenv("RENDERDOC_METAL_CAPTURE_PATH"));
    const bool frameBorn=getenv("RENDERDOC_METAL_EDIT_FRAME_BORN")!=nullptr;
    if(frameBorn)api->StartFrameCapture(nullptr,nullptr);
    id<MTLLibrary> lib;
    if(const char *path=getenv("RENDERDOC_METAL_EDIT_BINARY"))
      lib=[d newLibraryWithFile:[NSString stringWithUTF8String:path] error:&error];
    else lib=[d newLibraryWithSource:source options:nil error:&error];
    if(!lib) { fprintf(stderr,"%s\n",error.description.UTF8String); return 2; }
    auto values=[MTLFunctionConstantValues new]; float value=0.25;
    [values setConstantValue:&value type:MTLDataTypeFloat atIndex:0];
    auto pd=[MTLRenderPipelineDescriptor new]; pd.vertexFunction=[lib newFunctionWithName:@"vs"];
    if(getenv("RENDERDOC_METAL_EDIT_ALIAS")) {
      auto function=[MTLFunctionDescriptor functionDescriptor];
      function.name=@"fs"; function.specializedName=@"fs_specialized_alias";
      function.constantValues=values;
      pd.fragmentFunction=[lib newFunctionWithDescriptor:function error:&error];
    } else pd.fragmentFunction=[lib newFunctionWithName:@"fs" constantValues:values error:&error];
    pd.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;
    auto p1=[d newRenderPipelineStateWithDescriptor:pd error:&error];
    pd.label=@"Second dependent graphics pipeline";
    auto p2=[d newRenderPipelineStateWithDescriptor:pd error:&error];
    auto cp=[d newComputePipelineStateWithFunction:[lib newFunctionWithName:@"cs"] error:&error];
    if(!p1||!p2||!cp) return 3;
    auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:8 height:8 mipmapped:NO];
    td.storageMode=MTLStorageModePrivate; td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
    auto texture=[d newTextureWithDescriptor:td]; texture.label=@"Shader edit colour";
    auto buffer=[d newBufferWithLength:16 options:MTLResourceStorageModeShared]; buffer.label=@"Shader edit words";
    auto q=[d newCommandQueue]; if(!frameBorn)api->StartFrameCapture(nullptr,nullptr);
    auto layer=[CAMetalLayer layer]; layer.device=d; layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.drawableSize=CGSizeMake(2,2); layer.framebufferOnly=NO; auto drawable=[layer nextDrawable];
    if(!drawable)return 7;
    auto c=[q commandBuffer]; auto e=[c computeCommandEncoder]; [e setComputePipelineState:cp];
    [e setBuffer:buffer offset:0 atIndex:0]; [e dispatchThreadgroups:MTLSizeMake(4,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)]; [e endEncoding];
    auto pass=[MTLRenderPassDescriptor renderPassDescriptor]; pass.colorAttachments[0].texture=texture;
    pass.colorAttachments[0].loadAction=MTLLoadActionClear; pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    auto r=[c renderCommandEncoderWithDescriptor:pass]; [r setRenderPipelineState:p1];
    [r drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [r setRenderPipelineState:p2]; [r drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [r endEncoding];
    auto present=[MTLRenderPassDescriptor renderPassDescriptor]; present.colorAttachments[0].texture=drawable.texture;
    present.colorAttachments[0].loadAction=MTLLoadActionClear; present.colorAttachments[0].storeAction=MTLStoreActionStore;
    auto pe=[c renderCommandEncoderWithDescriptor:present]; [pe endEncoding]; [c presentDrawable:drawable];
    [c commit]; [c waitUntilCompleted];
    if(c.status==MTLCommandBufferStatusError) return 5;
    if(!api->EndFrameCapture(nullptr,nullptr)) return 6;
    puts("PASS shader edit capture: VS/FS/CS, specialization and two dependent PSOs"); return 0;
  }
}
