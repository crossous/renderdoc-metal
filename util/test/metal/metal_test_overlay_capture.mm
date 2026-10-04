// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
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
    const bool depthExport = argc > 1 && !strncmp(argv[1], "depth-export",12);
    NSString *source = @"#include <metal_stdlib>\nusing namespace metal;\n"
      "vertex float4 vs(uint i [[vertex_id]], constant float4 &cfg [[buffer(0)]]) {"
      "const float2 p[6]={float2(-1,-1),float2(1,-1),float2(-1,1),float2(-1,1),float2(1,-1),float2(1,1)};"
      "uint v=(cfg.w>0 && i>=3) ? (i==4 ? 5 : i==5 ? 4 : i) : i;"
      "return float4(p[v].x*cfg.x+cfg.y,p[v].y,cfg.z,1); }";
    source = [source stringByAppendingString:depthExport ?
      @"fragment float4 seedfs() {return float4(1);}\n"
       "struct Output {float4 col [[color(0)]]; float z [[ depth(any) ]];};"
       "fragment Output fs(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {"
       "return {t.sample(s,float2(.5)),.1f}; }" :
      @"fragment float4 fs(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {"
       "return t.sample(s,float2(.5)); }"];
    if(argc>1 && !strcmp(argv[1],"depth-export-less"))
      source=[source stringByReplacingOccurrencesOfString:@"depth(any)" withString:@"depth(less)"];
    if(argc>1 && !strcmp(argv[1],"depth-export-greater"))
    {
      source=[source stringByReplacingOccurrencesOfString:@"depth(any)" withString:@"depth(greater)"];
      source=[source stringByReplacingOccurrencesOfString:@")),.1f}" withString:@")),.9f}"];
    }
    if(const char *path = getenv("RENDERDOC_METAL_OVERLAY_WRITE_SOURCE"))
      return [source writeToFile:[NSString stringWithUTF8String:path] atomically:YES encoding:NSUTF8StringEncoding error:nil] ? 0 : 9;
    id<MTLDevice> d = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> q = [d newCommandQueue];
    NSError *error = nil;
    id<MTLLibrary> lib;
    if(const char *path = getenv("RENDERDOC_METAL_OVERLAY_METALLIB"))
      lib = [d newLibraryWithFile:[NSString stringWithUTF8String:path] error:&error];
    else
      lib = [d newLibraryWithSource:source options:nil error:&error];
    MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [lib newFunctionWithName:@"vs"];
    pd.fragmentFunction = [lib newFunctionWithName:@"fs"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    pd.depthAttachmentPixelFormat = pd.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    id<MTLRenderPipelineState> pipeline = [d newRenderPipelineStateWithDescriptor:pd error:&error];
    if(!pipeline) { fprintf(stderr,"pipeline: %s\n",error.description.UTF8String); return 2; }
    id<MTLRenderPipelineState> seedPipeline = pipeline;
    if(depthExport)
    {
      pd.fragmentFunction = [lib newFunctionWithName:@"seedfs"];
      seedPipeline = [d newRenderPipelineStateWithDescriptor:pd error:&error];
      if(!seedPipeline) return 2;
    }
    auto td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:32 height:16 mipmapped:NO];
    td.storageMode = MTLStorageModePrivate;
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    id<MTLTexture> color = [d newTextureWithDescriptor:td];
    color.label = @"Overlay original colour";
    td.pixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    id<MTLTexture> depth = [d newTextureWithDescriptor:td];
    depth.label = @"Overlay original depth stencil";
    MTLDepthStencilDescriptor *dsd = [MTLDepthStencilDescriptor new];
    dsd.depthCompareFunction = MTLCompareFunctionAlways;
    dsd.depthWriteEnabled = YES;
    auto stencil = [MTLStencilDescriptor new];
    stencil.stencilCompareFunction = MTLCompareFunctionAlways;
    stencil.depthStencilPassOperation = MTLStencilOperationReplace;
    dsd.frontFaceStencil = dsd.backFaceStencil = stencil;
    id<MTLDepthStencilState> seedDS = [d newDepthStencilStateWithDescriptor:dsd];
    dsd.depthCompareFunction = MTLCompareFunctionLess;
    stencil.stencilCompareFunction = MTLCompareFunctionEqual;
    stencil.depthStencilPassOperation = MTLStencilOperationKeep;
    dsd.frontFaceStencil = dsd.backFaceStencil = stencil;
    id<MTLDepthStencilState> testDS = [d newDepthStencilStateWithDescriptor:dsd];
    id<MTLDepthStencilState> disabledDS = [d newDepthStencilStateWithDescriptor:[MTLDepthStencilDescriptor new]];
    auto sampledDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    sampledDesc.storageMode = MTLStorageModeShared; sampledDesc.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> sampled = [d newTextureWithDescriptor:sampledDesc];
    uint8_t whiteTexel[4] = {255,255,255,255};
    [sampled replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:whiteTexel bytesPerRow:4];
    id<MTLSamplerState> sampler = [d newSamplerStateWithDescriptor:[MTLSamplerDescriptor new]];
    const bool interleaved = argc>1 && !strcmp(argv[1],"depth-export-interleaved");
    uint32_t arguments[4] = {6,1,0,0};
    id<MTLBuffer> indirect = [d newBufferWithBytes:arguments length:sizeof(arguments) options:MTLResourceStorageModeShared];
    id<MTLBuffer> producerArgs = interleaved ? [d newBufferWithBytes:arguments length:sizeof(arguments) options:MTLResourceStorageModeShared] : nil;
    RENDERDOC_API_1_7_0 *api = nullptr;
    if(const char *path = getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 3;
      api->SetCaptureFilePathTemplate(path);
    }
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = d; layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO; layer.drawableSize = CGSizeMake(2,2);
    if(api) api->StartFrameCapture(nullptr,nullptr);
    auto command = [q commandBuffer];
    const bool parallel = argc > 1 && (!strcmp(argv[1], "parallel") || !strcmp(argv[1], "depth-export-parallel"));
    const bool ephemeral = argc > 1 && !strcmp(argv[1], "ephemeral");
    const bool cull = ephemeral || (argc > 1 && !strcmp(argv[1], "cull"));
    auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = color;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0,0,1,1);
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.depthAttachment.texture = depth; pass.stencilAttachment.texture = depth;
    pass.depthAttachment.loadAction = pass.stencilAttachment.loadAction = MTLLoadActionClear;
    // DontCare deliberately tests preserving the in-pass depth/stencil prefix.
    pass.depthAttachment.storeAction = pass.stencilAttachment.storeAction = MTLStoreActionDontCare;
    pass.depthAttachment.clearDepth = .75; pass.stencilAttachment.clearStencil = 2;
    id<MTLParallelRenderCommandEncoder> parent = parallel ? [command parallelRenderCommandEncoderWithDescriptor:pass] : nil;
    id<MTLRenderCommandEncoder> e = parent ? [parent renderCommandEncoder] : [command renderCommandEncoderWithDescriptor:pass];
    [e setRenderPipelineState:seedPipeline];
    [e setFragmentTexture:sampled atIndex:0];
    [e setFragmentSamplerState:ephemeral ? [d newSamplerStateWithDescriptor:[MTLSamplerDescriptor new]] : sampler atIndex:0];
    [e setDepthStencilState:seedDS]; [e setStencilReferenceValue:1];
    float seed[4] = {.5f,-.5f,.25f,0};
    [e setVertexBytes:seed length:sizeof(seed) atIndex:0];
    if(!cull) [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    if(parent)
    {
      [e endEncoding];
      e = [parent renderCommandEncoder];
      [e setRenderPipelineState:pipeline];
      [e setFragmentTexture:sampled atIndex:0];
      [e setFragmentSamplerState:sampler atIndex:0];
    }
    if(depthExport) [e setRenderPipelineState:pipeline];
    [e setDepthStencilState:testDS];
    [e setStencilReferenceValue:1];
    if(cull)
    {
      [e setCullMode:MTLCullModeBack];
      [e setDepthStencilState:ephemeral ? [d newDepthStencilStateWithDescriptor:[MTLDepthStencilDescriptor new]] : disabledDS];
    }
    float cfg[4] = {1,0,.5f,cull ? 1.0f : 0.0f};
    [e setVertexBytes:cfg length:sizeof(cfg) atIndex:0];
    if(argc > 1 && !strncmp(argv[1], "viewport", 8))
    {
      [e setViewport:MTLViewport{4,2,24,12,0,1}];
      [e setScissorRect:MTLScissorRect{12,4,8,8}];
      if(strcmp(argv[1], "viewport"))
      {
        MTLViewport views[2] = {{4,2,24,12,0,1},{0,0,32,16,0,1}};
        MTLScissorRect scissors[2] = {{12,4,8,8},{0,0,32,16}};
        [e setViewports:views count:2]; [e setScissorRects:scissors count:2];
        if(!strcmp(argv[1], "viewport-reset"))
        {
          [e setViewport:views[0]]; [e setScissorRect:scissors[0]];
        }
      }
    }
    [e pushDebugGroup:@"Overlay selected indirect draw"];
    [e drawPrimitives:MTLPrimitiveTypeTriangle indirectBuffer:indirect indirectBufferOffset:0];
    [e popDebugGroup]; [e endEncoding];
    if(parent) [parent endEncoding];
    // Producer is encoded after the consumer draw, but submitted first.
    // Replay must finish this prefix and then select the consumer's context.
    if(interleaved)
    {
      auto producer=[q commandBuffer];auto blit=[producer blitCommandEncoder];
      [blit copyFromBuffer:producerArgs sourceOffset:0 toBuffer:indirect destinationOffset:0 size:sizeof(arguments)];
      [blit endEncoding];[producer commit];
    }
    auto drawable = [layer nextDrawable];
    auto pp = [MTLRenderPassDescriptor renderPassDescriptor];
    pp.colorAttachments[0].texture = drawable.texture;
    pp.colorAttachments[0].loadAction = MTLLoadActionClear;
    pp.colorAttachments[0].storeAction = MTLStoreActionStore;
    [[command renderCommandEncoderWithDescriptor:pp] endEncoding];
    [command presentDrawable:drawable]; [command commit]; [command waitUntilCompleted];
    if(command.error) { fprintf(stderr,"GPU: %s\n",command.error.description.UTF8String); return 4; }
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 5;
    auto data = [d newBufferWithLength:4096 options:MTLResourceStorageModeShared];
    auto copy = [q commandBuffer]; auto blit = [copy blitCommandEncoder];
    [blit copyFromTexture:color sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
      sourceSize:MTLSizeMake(32,16,1) toBuffer:data destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:4096];
    [blit endEncoding]; [copy commit]; [copy waitUntilCompleted];
    if(copy.error) return 6;
    unsigned white = 0, blue = 0;
    for(unsigned y=0;y<16;y++) for(unsigned x=0;x<32;x++)
    {
      auto p = (const uint8_t *)data.contents + y*256+x*4;
      white += p[0]==255 && p[1]==255 && p[2]==255 && p[3]==255;
      blue += p[0]==0 && p[1]==0 && p[2]==255 && p[3]==255;
      if(cull) continue;
      if(p[0] != (x<16 ? 255 : 0) || p[1] != (x<16 ? 255 : 0) || p[2] != 255 || p[3] != 255)
      { fprintf(stderr,"Native mismatch xy=%u,%u got=%u,%u,%u,%u\n",x,y,p[0],p[1],p[2],p[3]); return 7; }
    }
    if(cull && (white!=256 || blue!=256)) return 8;
    if(cull) puts("PASS Native Cull reference: 256 white front pixels, 256 blue culled pixels");
    puts("PASS Native original: left white, right blue; selected draw rejected by depth/stencil");
    return 0;
  }
}
