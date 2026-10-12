// SPDX-License-Identifier: MIT
// Native pointer-derived scalar data selects either six indices from zero, or
// three indices from three with baseVertex=-3. Both draw identical triangles.
// Every index byte and both vertex pointer sources have actual Native origins.
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
  const bool unspecifiedIndices = getenv("RENDERDOC_METAL_TEST_INDIRECT_UNSPECIFIED_INDICES") != nullptr;
  const bool pointerRoots = !unspecifiedIndices;
  const bool wideIndices = getenv("RENDERDOC_METAL_TEST_INDIRECT_UINT32") != nullptr;
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
    NSString *source = @R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void arguments(device const uint *input [[buffer(0)]], device uint *output [[buffer(1)]],
                      constant uint &shift [[buffer(2)]]) {
  uint choice=uint((reinterpret_cast<ulong>(input)>>shift)&1ul);
  output[0]=choice?6:3; output[1]=1+choice;
  output[2]=choice?0:3; output[3]=choice?0:uint(-3); output[4]=0;
}
vertex float4 triangle(uint i [[vertex_id]]) {
  float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[i%3],0,1);
}
vertex float4 point(uint i [[vertex_id]]) { return float4(0,0,0,1); }
struct Roots { device const float2 *points[2]; };
vertex float4 pointer_triangle(uint i [[vertex_id]], uint instance [[instance_id]],
                              constant Roots &roots [[buffer(2)]]) {
  return float4(roots.points[instance%2][i%3],0,1);
}
fragment float4 color() { return float4(1,.25,.5,1); }
)MSL";
    id<MTLLibrary> library = nil;
    if(const char *path=getenv("RENDERDOC_METAL_TEST_INDEXED_LIBRARY"))
    {
      NSData *bytes=[NSData dataWithContentsOfFile:[NSString stringWithUTF8String:path]];
      if(!bytes.length)return 2;
      dispatch_data_t data=dispatch_data_create(bytes.bytes,bytes.length,
          dispatch_get_global_queue(QOS_CLASS_DEFAULT,0),DISPATCH_DATA_DESTRUCTOR_DEFAULT);
      library=[device newLibraryWithData:data error:&error];
    }
    else library=[device newLibraryWithSource:source options:nil error:&error];
    auto compute = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"arguments"] error:&error];
    auto pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [library newFunctionWithName:unspecifiedIndices?@"point":pointerRoots?@"pointer_triangle":@"triangle"];
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
    const uint16_t narrow[6]={0,1,2,3,4,5};
    const uint32_t wide[6]={0,1,2,3,4,5};
    const NSUInteger indexLength=wideIndices?sizeof(wide):sizeof(narrow);
    auto indices=unspecifiedIndices?nil:[device newBufferWithBytes:wideIndices?(const void *)wide:(const void *)narrow length:indexLength options:MTLResourceStorageModeShared];
    auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;
    hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
    const auto indexOptions=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;
    hd.size=[device heapBufferSizeAndAlignWithLength:indexLength options:indexOptions].size;
    auto indexHeap=unspecifiedIndices?[device newHeapWithDescriptor:hd]:nil;
    auto args = [device newBufferWithLength:20 options:MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeTracked];
    const uint64_t packet[3] = {input.gpuAddress,0,input.length};
    auto table = [device newBufferWithBytes:packet length:sizeof(packet) options:MTLResourceStorageModeShared];
    auto td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:31 height:17 mipmapped:NO];
    td.storageMode = MTLStorageModePrivate; td.usage = MTLTextureUsageRenderTarget;
    auto image = [device newTextureWithDescriptor:td];
    auto staging = [device newBufferWithLength:256*17 options:MTLResourceStorageModeShared];
    auto argsCopy = [device newBufferWithLength:20 options:MTLResourceStorageModeShared];
    if(!input || (!unspecifiedIndices && !indices) || (unspecifiedIndices && !indexHeap) || !args || !table || !image || !staging || !argsCopy) return 3;
    if(indices)indices.label = @"Native indirect index input";
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
    [blit fillBuffer:args range:NSMakeRange(0,20) value:0];[blit endEncoding];
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
    // No bytes are initialized or compared here. Every index selects the same
    // degenerate Native vertex, so the full clear image is an independent oracle.
    if(unspecifiedIndices)
    {
      indices=[indexHeap newBufferWithLength:indexLength options:indexOptions offset:0];
      if(!indices)return 6;
      indices.label=@"Native indirect index input";
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
    [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexType:wideIndices?MTLIndexTypeUInt32:MTLIndexTypeUInt16 indexBuffer:indices indexBufferOffset:0 indirectBuffer:args indirectBufferOffset:0];[e endEncoding];
    blit=[command blitCommandEncoder];
    [blit copyFromTexture:image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(31,17,1)
      toBuffer:staging destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:256*17];
    [blit copyFromBuffer:args sourceOffset:0 toBuffer:argsCopy destinationOffset:0 size:20];
    [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 8;
    if(api && !api->EndFrameCapture(nullptr,nullptr))return 9;
    unsigned errors=0;
    for(unsigned y=0;y<17;y++)for(unsigned x=0;x<256;x++)
    {
      const unsigned char expected=x<124?(unspecifiedIndices?((const unsigned char[]){0,0,0,255})[x%4]:((const unsigned char[]){255,64,128,255})[x%4]):0xa7;
      errors+=((unsigned char *)staging.contents)[y*256+x]!=expected;
    }
    const uint32_t *actual=(const uint32_t *)argsCopy.contents;
    if(spacer && spacer.length != spacerBytes) return 10;
    if(!((actual[0]==3 && actual[1]==1 && actual[2]==3 && int32_t(actual[3])==-3) ||
         (actual[0]==6 && actual[1]==2 && actual[2]==0 && actual[3]==0)) || actual[4])return 10;
    FILE *out=fopen(argv[2],"wbx");if(!out)return 11;
    const bool saved=fwrite(staging.contents,1,staging.length,out)==staging.length;fclose(out);
    printf("Native GPU COMPLETED; full=%lu image/padding errors=%u indices=%u instances=%u firstIndex=%u baseVertex=%d wide=%u shift=%u actual-input-address=%llu pre-frame-address=%llu\n",
           staging.length,errors,actual[0],actual[1],actual[2],int32_t(actual[3]),wideIndices,shift,(unsigned long long)input.gpuAddress,(unsigned long long)spacerAddress);
    return saved && !errors?0:12;
  }
}
