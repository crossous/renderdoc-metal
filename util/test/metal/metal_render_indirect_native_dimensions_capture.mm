// SPDX-License-Identifier: MIT
// Original Native shaders choose one or two identical opaque instances from
// an ordinary value derived from the actual Native pointer. Every output pixel
// is invariant; a captured dispatch count is not a resource restoration proof.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "renderdoc/api/app/renderdoc_app.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

int main(int argc, char **argv)
{
  if(argc != 4 && argc != 5) return 1;
  const unsigned shift = unsigned(atoi(argv[3]));
  if(shift < 8 || shift > 40) return 1;
  const unsigned spacerBytes = argc == 5 ? unsigned(atoi(argv[4])) : 0;
  if(spacerBytes > 256*1024) return 1;
  const bool pointerRoots = getenv("RENDERDOC_METAL_TEST_INDIRECT_POINTER_ROOTS") != nullptr;
  @autoreleasepool
  {
    auto device = MTLCreateSystemDefaultDevice();
    auto queue = [device newCommandQueue]; NSError *error = nil;
    // This legal, unreferenced pre-frame allocation changes the Native address
    // placement without introducing a shader input or a missing captured write.
    auto spacer = spacerBytes ? [device newBufferWithLength:spacerBytes options:MTLResourceStorageModeShared] : nil;
    if(spacerBytes && !spacer) return 3;
    const uint64_t spacerAddress = spacer ? spacer.gpuAddress : 0;
    if(spacer && !spacerAddress) return 3;
    auto library = [device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void arguments(device const uint *input [[buffer(0)]], device uint *output [[buffer(1)]],
                      constant uint &shift [[buffer(2)]]) {
  output[0]=3; output[1]=1+uint((reinterpret_cast<ulong>(input)>>shift)&1ul);
  output[2]=0; output[3]=0;
}
vertex float4 triangle(uint i [[vertex_id]]) {
  float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[i%3],0,1);
}
struct Roots { device const float2 *points[2]; };
vertex float4 pointer_triangle(uint i [[vertex_id]], uint instance [[instance_id]],
                              constant Roots &roots [[buffer(2)]]) {
  return float4(roots.points[instance%2][i%3],0,1);
}
fragment float4 color() { return float4(1,.25,.5,1); }
)MSL" options:nil error:&error];
    auto compute = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"arguments"] error:&error];
    auto pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [library newFunctionWithName:pointerRoots?@"pointer_triangle":@"triangle"];
    pd.fragmentFunction = [library newFunctionWithName:@"color"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    auto render = [device newRenderPipelineStateWithDescriptor:pd error:&error];
    if(!compute || !render) { fprintf(stderr,"%s\n",error.description.UTF8String); return 2; }
    const uint32_t data[16] = {};
    auto input = [device newBufferWithBytes:data length:sizeof(data) options:MTLResourceStorageModeShared];
    const float positions[6] = {-1,-1,3,-1,-1,3};
    auto firstPoints = pointerRoots ? [device newBufferWithBytes:positions length:sizeof(positions) options:MTLResourceStorageModeShared] : nil;
    auto secondPoints = pointerRoots ? [device newBufferWithBytes:positions length:sizeof(positions) options:MTLResourceStorageModeShared] : nil;
    if(pointerRoots && (!firstPoints || !secondPoints)) return 3;
    auto args = [device newBufferWithLength:16 options:MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeTracked];
    const uint64_t packet[3] = {input.gpuAddress,0,input.length};
    auto table = [device newBufferWithBytes:packet length:sizeof(packet) options:MTLResourceStorageModeShared];
    auto td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:31 height:17 mipmapped:NO];
    td.storageMode = MTLStorageModePrivate; td.usage = MTLTextureUsageRenderTarget;
    auto image = [device newTextureWithDescriptor:td];
    auto staging = [device newBufferWithLength:256*17 options:MTLResourceStorageModeShared];
    auto argsCopy = [device newBufferWithLength:16 options:MTLResourceStorageModeShared];
    if(!input || !args || !table || !image || !staging || !argsCopy) return 3;
    image.label = @"Native indirect invariant image";
    args.label = @"Native indirect current arguments";
    staging.label = @"Native indirect padded copy";
    memset(staging.contents,0xa7,staging.length);
    auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=image;
    pass.colorAttachments[0].loadAction=MTLLoadActionClear;
    pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,1);
    auto init = [queue commandBuffer];auto blit=[init blitCommandEncoder];
    [blit fillBuffer:args range:NSMakeRange(0,16) value:0];[blit endEncoding];
    auto warm=[init renderCommandEncoderWithDescriptor:pass];[warm endEncoding];
    [init commit];[init waitUntilCompleted];if(init.error)return 4;
    RENDERDOC_API_1_7_0 *api=nullptr;
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    if(get && !get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 5;
    auto ann=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d){
      if(!api)return 0u;
      RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;
      v.vector.uint64[2]=c;v.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
    };
    if(api)
    {
      RENDERDOC_AnnotationValue v={};v.uint32=67;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v) ||
         ann(table,"metal.descriptorTable",1,0,1,24) ||
         ann(table,"metal.descriptorSlotEvent",0,1,0,2) ||
         ann(table,"metal.descriptorSlotEvent",0,1,2,2) ||
         ann(table,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)input,0))return 5;
      api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);
    }
    auto command=[queue commandBuffer];auto c=[command computeCommandEncoder];
    [c setComputePipelineState:compute];[c setBuffer:input offset:0 atIndex:0];[c setBuffer:args offset:0 atIndex:1];
    if(ann(c,"metal.descriptorInlineLayout",0,2,0,4)){[c endEncoding];return 6;}
    [c setBytes:&shift length:4 atIndex:2];
    [c dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[c endEncoding];
    auto e=[command renderCommandEncoderWithDescriptor:pass];[e setRenderPipelineState:render];
    if(pointerRoots)
    {
      const uint64_t roots[2] = {firstPoints.gpuAddress,secondPoints.gpuAddress};
      if(ann(e,"metal.descriptorInlineLayout",1,2,2,8) ||
         ann(e,"metal.descriptorInlineBinding",(1ULL<<32)|2,0,(uint64_t)(__bridge void *)firstPoints,0) ||
         ann(e,"metal.descriptorInlineBinding",(1ULL<<32)|2,1,(uint64_t)(__bridge void *)secondPoints,0))
      { [e endEncoding];return 7; }
      [e setVertexBytes:roots length:sizeof(roots) atIndex:2];
      [e useResource:firstPoints usage:MTLResourceUsageRead];
      [e useResource:secondPoints usage:MTLResourceUsageRead];
    }
    if(api){RENDERDOC_AnnotationValue v={};v.uint32=1;if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)e,"metal.renderWritesDeclared",eRENDERDOC_UInt32,0,&v)){[e endEncoding];return 7;}}
    [e drawPrimitives:MTLPrimitiveTypeTriangle indirectBuffer:args indirectBufferOffset:0];[e endEncoding];
    blit=[command blitCommandEncoder];
    [blit copyFromTexture:image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(31,17,1)
      toBuffer:staging destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:256*17];
    [blit copyFromBuffer:args sourceOffset:0 toBuffer:argsCopy destinationOffset:0 size:16];
    [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 8;
    if(api && !api->EndFrameCapture(nullptr,nullptr))return 9;
    unsigned errors=0;
    for(unsigned y=0;y<17;y++)for(unsigned x=0;x<256;x++)
    {
      const unsigned char expected=x<124?((const unsigned char[]){255,64,128,255})[x%4]:0xa7;
      errors+=((unsigned char *)staging.contents)[y*256+x]!=expected;
    }
    const uint32_t *actual=(const uint32_t *)argsCopy.contents;
    if(spacer && spacer.length != spacerBytes) return 10;
    if(actual[0]!=3 || actual[1]<1 || actual[1]>2 || actual[2] || actual[3])return 10;
    FILE *out=fopen(argv[2],"wbx");if(!out)return 11;
    const bool saved=fwrite(staging.contents,1,staging.length,out)==staging.length;fclose(out);
    printf("Native GPU COMPLETED; full=%lu image/padding errors=%u instances=%u shift=%u actual-input-address=%llu pre-frame-address=%llu\n",
           staging.length,errors,actual[1],shift,(unsigned long long)input.gpuAddress,(unsigned long long)spacerAddress);
    return saved && !errors?0:12;
  }
}
