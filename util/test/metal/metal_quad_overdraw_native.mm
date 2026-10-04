// SPDX-License-Identifier: MIT
// Component proof of Vulkan quadwrite/quadresolve semantics on Apple Metal.
// This is not an advertised RenderDoc overlay.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#import <QuartzCore/CAMetalLayer.h>
#include "renderdoc/api/app/renderdoc_app.h"

int main(int argc, char **argv)
{
  @autoreleasepool
  {
    const unsigned requested=argc>1?unsigned(strtoul(argv[1],nullptr,10)):0;
    if(requested>10) return 8;
    const unsigned width=requested==9?5:2, height=requested==9?3:2;
    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue = [dev newCommandQueue];
    NSError *error = nil;
    NSString *msl = @"#include <metal_stdlib>\nusing namespace metal;\n"
      "vertex float4 full(uint i [[vertex_id]]) {"
      "const float2 p[3]={float2(-1,1),float2(3,1),float2(-1,-3)};"
      "return float4(p[i],.5,1);}"
      "vertex float4 three(uint i [[vertex_id]]) {"
      "const float2 p[3]={float2(-2,2),float2(2.5,2),float2(-2,-2.5)};"
      "return float4(p[i],.5,1);}"
      "struct Depth {float z [[depth(any)]];};"
      "fragment Depth seed(float4 p [[position]]) {return {p.x<1?.25f:.75f};}"
      "[[early_fragment_tests]] fragment void count(float4 p [[position]],"
      "uint coverage [[sample_mask,post_depth_coverage]],device atomic_uint *b [[buffer(0)]]) {"
      "uint c=coverage&1u;"
      "uint n=c+quad_shuffle_xor(c,1)+quad_shuffle_xor(c,2)+quad_shuffle_xor(c,3);"
      "if(c && n) atomic_fetch_add_explicit(b+n-1,1u,memory_order_relaxed);}";
    id<MTLLibrary> lib = [dev newLibraryWithSource:msl options:nil error:&error];
    if(!lib) {fprintf(stderr,"MSL: %s\n",error.description.UTF8String);return 2;}
    auto pipeline = [&](NSString *vs, NSString *fs) {
      auto desc = [MTLRenderPipelineDescriptor new];
      desc.vertexFunction = [lib newFunctionWithName:vs];
      desc.fragmentFunction = [lib newFunctionWithName:fs];
      desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
      desc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
      auto p = [dev newRenderPipelineStateWithDescriptor:desc error:&error];
      if(!p) fprintf(stderr,"PSO: %s\n",error.description.UTF8String);
      return p;
    };
    auto seedPSO=pipeline(@"full",@"seed"), fullPSO=pipeline(@"full",@"count"),
         threePSO=pipeline(@"three",@"count");
    if(!seedPSO || !fullPSO || !threePSO) return 3;
    auto desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float_Stencil8
                 width:width height:height mipmapped:NO];
    desc.storageMode=MTLStorageModePrivate;desc.usage=MTLTextureUsageRenderTarget;
    id<MTLTexture> depth=[dev newTextureWithDescriptor:desc];
    depth.label=@"Quad original depth stencil";
    auto counters=[dev newBufferWithLength:4*sizeof(unsigned) options:MTLResourceStorageModeShared];
    auto states=[MTLDepthStencilDescriptor new];
    states.depthCompareFunction=MTLCompareFunctionAlways;states.depthWriteEnabled=YES;
    auto seedDS=[dev newDepthStencilStateWithDescriptor:states];
    states.depthWriteEnabled=NO;
    auto alwaysDS=[dev newDepthStencilStateWithDescriptor:states];
    states.depthCompareFunction=MTLCompareFunctionLess;
    auto lessDS=[dev newDepthStencilStateWithDescriptor:states];
    states.depthCompareFunction=MTLCompareFunctionAlways;
    auto stencil=[MTLStencilDescriptor new];
    stencil.stencilCompareFunction=MTLCompareFunctionEqual;
    stencil.readMask=0xff;stencil.writeMask=0;
    states.frontFaceStencil=stencil;states.backFaceStencil=stencil;
    auto stencilDS=[dev newDepthStencilStateWithDescriptor:states];
    RENDERDOC_API_1_7_0 *api=nullptr;
    auto layer=[CAMetalLayer layer];layer.device=dev;layer.drawableSize=CGSizeMake(2,2);
    layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=NO;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 7;
      api->SetCaptureFilePathTemplate(path);
    }
    const unsigned first=requested;
    const unsigned end=argc>1?first+1:9;
    for(unsigned mode=first;mode<end;mode++)
    {
      std::memset(counters.contents,0,4*sizeof(unsigned));
      if(api) api->StartFrameCapture(nullptr,nullptr);
      auto command=[queue commandBuffer];
      auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.depthAttachment.texture=depth;pass.depthAttachment.loadAction=MTLLoadActionClear;
      pass.depthAttachment.clearDepth=1;pass.depthAttachment.storeAction=MTLStoreActionStore;
      pass.stencilAttachment.texture=depth;pass.stencilAttachment.loadAction=MTLLoadActionClear;
      pass.stencilAttachment.clearStencil=1;pass.stencilAttachment.storeAction=MTLStoreActionStore;
      auto encoder=[command renderCommandEncoderWithDescriptor:pass];
      [encoder setRenderPipelineState:seedPSO];[encoder setDepthStencilState:seedDS];
      [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[encoder endEncoding];
      pass.depthAttachment.loadAction=MTLLoadActionLoad;pass.stencilAttachment.loadAction=MTLLoadActionLoad;
      id<MTLParallelRenderCommandEncoder> parent=mode==10?[command parallelRenderCommandEncoderWithDescriptor:pass]:nil;
      encoder=parent?[parent renderCommandEncoder]:[command renderCommandEncoderWithDescriptor:pass];
      [encoder setRenderPipelineState:mode==2?threePSO:fullPSO];
      [encoder setDepthStencilState:mode==6?lessDS:(mode>=7?stencilDS:alwaysDS)];
      [encoder setStencilReferenceValue:mode==8?0:1];
      [encoder setFragmentBuffer:counters offset:0 atIndex:0];
      if(mode==0 || mode==4) [encoder setScissorRect:MTLScissorRect{0,0,1,1}];
      else if(mode==1) [encoder setScissorRect:MTLScissorRect{0,0,2,1}];
      unsigned draws=(mode==4 || mode==5)?2:1;
      for(unsigned d=0;d<draws;d++) [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
      [encoder endEncoding];
      if(parent) [parent endEncoding];
      if(api)
      {
        auto drawable=[layer nextDrawable];
        auto present=[MTLRenderPassDescriptor renderPassDescriptor];
        present.colorAttachments[0].texture=drawable.texture;
        present.colorAttachments[0].loadAction=MTLLoadActionClear;
        present.colorAttachments[0].storeAction=MTLStoreActionStore;
        [[command renderCommandEncoderWithDescriptor:present] endEncoding];
        [command presentDrawable:drawable];
      }
      [command commit];[command waitUntilCompleted];
      if(api && !api->EndFrameCapture(nullptr,nullptr)) return 9;
      if(command.error) {fprintf(stderr,"GPU: %s\n",command.error.description.UTF8String);return 4;}
      unsigned expected[4]={};
      unsigned live=mode==0 || mode==4?1:(mode==1 || mode==6?2:(mode==2?3:(mode==8?0:4)));
      if(live) expected[live-1]=live*draws;
      if(mode==9) {expected[0]=1;expected[1]=6;expected[2]=0;expected[3]=8;}
      auto result=(const unsigned *)counters.contents;
      unsigned resolved=0;
      for(unsigned i=0;i<4;i++)
      {
        if(result[i]!=expected[i])
        {fprintf(stderr,"Quad case %u bucket %u expected %u got %u (all %u,%u,%u,%u)\n",mode,i,expected[i],result[i],result[0],result[1],result[2],result[3]);return 5;}
        resolved+=result[i]/(i+1);
      }
      if(resolved!=(mode==9?6:(live?draws:0))) return 6;
      printf("PASS Native quad case %u live=%u draws=%u buckets=%u,%u,%u,%u resolved=%u\n",
        mode,live,draws,result[0],result[1],result[2],result[3],resolved);
    }
  }
  return 0;
}
