// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

int main()
{
  @autoreleasepool
  {
    id<MTLDevice> d = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> q = [d newCommandQueue];
    NSError *error = nil;
    id<MTLLibrary> lib = [d newLibraryWithSource:@"#include <metal_stdlib>\nusing namespace metal;\n"
      "vertex float4 vs(uint i [[vertex_id]]) {const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return float4(p[i],0,1);}"
      "fragment float4 fs(constant float4 &c [[buffer(0)]]) {return c;}" options:nil error:&error];
    auto pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [lib newFunctionWithName:@"vs"];
    pd.fragmentFunction = [lib newFunctionWithName:@"fs"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    id<MTLRenderPipelineState> pipeline = [d newRenderPipelineStateWithDescriptor:pd error:&error];
    if(!pipeline) return 2;
    auto td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:16 height:16 mipmapped:NO];
    td.storageMode = MTLStorageModeShared;
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    id<MTLTexture> texture = [d newTextureWithDescriptor:td];
    texture.label = @"Interleaved pass shared target";
    auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0, (void **)&api)) return 3;
      api->SetCaptureFilePathTemplate(path); api->StartFrameCapture(nullptr, nullptr);
    }
    auto ca = [q commandBuffer], cb = [q commandBuffer];
    auto a = [ca renderCommandEncoderWithDescriptor:pass];
    auto b = [cb renderCommandEncoderWithDescriptor:pass];
    float red[4] = {1,0,0,1}, green[4] = {0,1,0,1}, blue[4] = {0,0,1,1};
    [a pushDebugGroup:@"Pass owner A"];
    [a setRenderPipelineState:pipeline]; [a setFragmentBytes:red length:sizeof(red) atIndex:0];
    [a setScissorRect:MTLScissorRect{0,0,8,16}];
    [a drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [b pushDebugGroup:@"Pass owner B"];
    [b setRenderPipelineState:pipeline]; [b setFragmentBytes:blue length:sizeof(blue) atIndex:0];
    [b drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [b popDebugGroup]; [b endEncoding];
    [a setFragmentBytes:green length:sizeof(green) atIndex:0];
    [a setScissorRect:MTLScissorRect{8,0,8,16}];
    [a drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [a popDebugGroup]; [a endEncoding];
    // Same command buffer and target, but a new encoder is a new Metal pass.
    pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
    auto next = [ca renderCommandEncoderWithDescriptor:pass];
    [next setRenderPipelineState:pipeline]; [next setFragmentBytes:green length:sizeof(green) atIndex:0];
    [next setScissorRect:MTLScissorRect{8,0,8,16}];
    [next drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [next endEncoding];
    [cb commit]; [ca commit]; [ca waitUntilCompleted]; [cb waitUntilCompleted];
    if(ca.error || cb.error) return 4;
    auto layer = [CAMetalLayer layer]; layer.device = d; layer.framebufferOnly = NO;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm; layer.drawableSize = CGSizeMake(2,2);
    auto drawable = [layer nextDrawable]; auto present = [q commandBuffer];
    auto pp = [MTLRenderPassDescriptor renderPassDescriptor];
    pp.colorAttachments[0].texture = drawable.texture;
    pp.colorAttachments[0].loadAction = MTLLoadActionClear;
    pp.colorAttachments[0].storeAction = MTLStoreActionStore;
    [[present renderCommandEncoderWithDescriptor:pp] endEncoding];
    [present presentDrawable:drawable]; [present commit]; [present waitUntilCompleted];
    if(present.error || (api && !api->EndFrameCapture(nullptr,nullptr))) return 5;
    uint8_t pixels[16*16*4];
    [texture getBytes:pixels bytesPerRow:64 fromRegion:MTLRegionMake2D(0,0,16,16) mipmapLevel:0];
    for(unsigned y=0;y<16;y++) for(unsigned x=0;x<16;x++)
    {
      auto p = pixels+(y*16+x)*4;
      if(p[0]!=(x<8?255:0) || p[1]!=(x<8?0:255) || p[2]!=0 || p[3]!=255) return 6;
    }
    puts("PASS Native interleaved shared target: left red, right green; B committed before A");
  }
  return 0;
}
