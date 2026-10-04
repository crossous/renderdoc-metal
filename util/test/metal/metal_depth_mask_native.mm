// SPDX-License-Identifier: MIT
// Native component proof for Vulkan's original-FS depth-test stencil mask.
// This does not implement or advertise a RenderDoc overlay.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>

int main()
{
  @autoreleasepool
  {
    constexpr unsigned width = 64, height = 32;
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue = [device newCommandQueue];
    NSError *error = nil;
    NSString *source = @"#include <metal_stdlib>\nusing namespace metal;\n"
        "vertex float4 full(uint i [[vertex_id]]) {"
        "const float2 p[6]={float2(-1,-1),float2(1,-1),float2(-1,1),"
        "float2(-1,1),float2(1,-1),float2(1,1)};return float4(p[i],.8,1);}"
        "struct DepthValue {float depth [[depth(any)]];};"
        "fragment DepthValue copyDepth(float4 p [[position]], "
        "depth2d<float,access::read> src [[texture(0)]]) {"
        "return {src.read(uint2(p.xy))};}"
        "struct Result {float4 colour [[color(0)]];float depth [[depth(any)]];};"
        "fragment Result original(float4 p [[position]]) {"
        "if(p.x>=48) discard_fragment();"
        "return {float4(1,1,0,1),p.x<32?.1f:.9f};}"
        "fragment float4 resolve(float4 p [[position]], "
        "texture2d<uint,access::read> mask [[texture(0)]]) {"
        "return mask.read(uint2(p.xy)).x ? float4(0,1,0,1) : float4(1,0,0,1);}";
    id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
    if(!library) {fprintf(stderr,"MSL: %s\n",error.description.UTF8String);return 2;}
    auto texture = [&](MTLPixelFormat format, MTLStorageMode storage, MTLTextureUsage usage) {
      auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                  width:width height:height mipmapped:NO];
      desc.storageMode = storage; desc.usage = usage;
      return [device newTextureWithDescriptor:desc];
    };
    id<MTLTexture> originalColour = texture(MTLPixelFormatRGBA8Unorm, MTLStorageModeShared,
        MTLTextureUsageRenderTarget);
    id<MTLTexture> overlay = texture(MTLPixelFormatRGBA8Unorm, MTLStorageModeShared,
        MTLTextureUsageRenderTarget);
    id<MTLTexture> originalDepth = texture(MTLPixelFormatDepth32Float, MTLStorageModePrivate,
        MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead);
    id<MTLTexture> scratch = texture(MTLPixelFormatDepth32Float_Stencil8, MTLStorageModePrivate,
        MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView);
    id<MTLTexture> stencil = [scratch newTextureViewWithPixelFormat:MTLPixelFormatX32_Stencil8];
    if(!stencil) return 3;
    auto pipeline = [&](NSString *fragment, bool colour, bool depth, bool mask) {
      auto desc = [MTLRenderPipelineDescriptor new];
      desc.vertexFunction = [library newFunctionWithName:@"full"];
      desc.fragmentFunction = [library newFunctionWithName:fragment];
      if(colour) desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
      if(mask) desc.colorAttachments[0].writeMask = MTLColorWriteMaskNone;
      if(depth)
      {
        desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
        desc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
      }
      auto p = [device newRenderPipelineStateWithDescriptor:desc error:&error];
      if(!p) fprintf(stderr,"PSO: %s\n",error.description.UTF8String);
      return p;
    };
    auto copyPipeline = pipeline(@"copyDepth", false, true, false);
    auto maskPipeline = pipeline(@"original", true, true, true);
    auto resolvePipeline = pipeline(@"resolve", true, false, false);
    if(!copyPipeline || !maskPipeline || !resolvePipeline) return 4;
    auto ds = [MTLDepthStencilDescriptor new];
    ds.depthCompareFunction = MTLCompareFunctionAlways; ds.depthWriteEnabled = YES;
    auto copyDS = [device newDepthStencilStateWithDescriptor:ds];
    ds.depthCompareFunction = MTLCompareFunctionLess; ds.depthWriteEnabled = NO;
    auto stamp = [MTLStencilDescriptor new];
    stamp.stencilCompareFunction = MTLCompareFunctionAlways;
    stamp.depthStencilPassOperation = MTLStencilOperationReplace;
    ds.frontFaceStencil = stamp; ds.backFaceStencil = stamp;
    auto maskDS = [device newDepthStencilStateWithDescriptor:ds];
    auto command = [queue commandBuffer];
    auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = originalColour;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0,0,1,1);
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.depthAttachment.texture = originalDepth;
    pass.depthAttachment.loadAction = MTLLoadActionClear;
    pass.depthAttachment.clearDepth = .5;
    pass.depthAttachment.storeAction = MTLStoreActionStore;
    [[command renderCommandEncoderWithDescriptor:pass] endEncoding];
    pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.depthAttachment.texture = scratch;
    pass.depthAttachment.loadAction = MTLLoadActionClear;
    pass.depthAttachment.clearDepth = 1;
    pass.depthAttachment.storeAction = MTLStoreActionStore;
    pass.stencilAttachment.texture = scratch;
    pass.stencilAttachment.loadAction = MTLLoadActionClear;
    pass.stencilAttachment.clearStencil = 0;
    pass.stencilAttachment.storeAction = MTLStoreActionStore;
    auto encoder = [command renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:copyPipeline]; [encoder setDepthStencilState:copyDS];
    [encoder setFragmentTexture:originalDepth atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    [encoder endEncoding];
    pass.depthAttachment.loadAction = MTLLoadActionLoad;
    pass.stencilAttachment.loadAction = MTLLoadActionLoad;
    pass.colorAttachments[0].texture = originalColour;
    pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    encoder = [command renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:maskPipeline]; [encoder setDepthStencilState:maskDS];
    [encoder setStencilReferenceValue:1];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    [encoder endEncoding];
    pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = overlay;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    encoder = [command renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:resolvePipeline];
    [encoder setFragmentTexture:stencil atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    [encoder endEncoding];
    auto depthBytes = [device newBufferWithLength:width*height*sizeof(float)
                          options:MTLResourceStorageModeShared];
    auto blit = [command blitCommandEncoder];
    [blit copyFromTexture:originalDepth sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
        sourceSize:MTLSizeMake(width,height,1) toBuffer:depthBytes destinationOffset:0
        destinationBytesPerRow:width*sizeof(float) destinationBytesPerImage:width*height*sizeof(float)];
    [blit endEncoding]; [command commit]; [command waitUntilCompleted];
    if(command.error) {fprintf(stderr,"GPU: %s\n",command.error.description.UTF8String);return 5;}
    unsigned char pixels[width*height*4], originals[sizeof(pixels)];
    [overlay getBytes:pixels bytesPerRow:width*4 fromRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0];
    [originalColour getBytes:originals bytesPerRow:width*4 fromRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0];
    for(unsigned y=0;y<height;y++) for(unsigned x=0;x<width;x++)
    {
      unsigned i=y*width+x, p=i*4;
      if(pixels[p]!=(x<32?0:255) || pixels[p+1]!=(x<32?255:0) || pixels[p+2] || pixels[p+3]!=255)
      {fprintf(stderr,"Mask mismatch %u,%u\n",x,y);return 6;}
      if(originals[p] || originals[p+1] || originals[p+2]!=255 || originals[p+3]!=255 ||
         ((const float *)depthBytes.contents)[i]!=.5f) return 7;
    }
    puts("PASS Native original-FS depth export/discard -> stencil mask -> colour resolve; original colour/depth unchanged");
  }
  return 0;
}
