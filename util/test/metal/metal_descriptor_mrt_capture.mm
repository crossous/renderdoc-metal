// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main()
{
  @autoreleasepool
  {
    const bool renderIndirect=getenv("RENDERDOC_METAL_MRT_RENDER_INDIRECT")!=nullptr;
    const bool renderIndexed=getenv("RENDERDOC_METAL_MRT_RENDER_INDEXED")!=nullptr;
    const bool renderZero=getenv("RENDERDOC_METAL_MRT_RENDER_ZERO")!=nullptr;
    const bool zeroArgumentCompute=getenv("RENDERDOC_METAL_MRT_ZERO_ARGUMENT_COMPUTE")!=nullptr;
    const bool mixedFreshCPUSlot=getenv("RENDERDOC_METAL_MRT_MIXED_FRESH_CPU_SLOT")!=nullptr;
    const bool unusedProducerSlot=getenv("RENDERDOC_METAL_MRT_UNUSED_PRODUCER_SLOT")!=nullptr;
    const bool freshCPUSlot=getenv("RENDERDOC_METAL_MRT_FRESH_CPU_SLOT")!=nullptr;
    const bool indirectPass=getenv("RENDERDOC_METAL_MRT_COMPUTE_INDIRECT")!=nullptr;
    const bool zeroIndirect=getenv("RENDERDOC_METAL_MRT_ZERO_INDIRECT")!=nullptr;
    const bool privateArguments=getenv("RENDERDOC_METAL_MRT_PRIVATE_ARGUMENTS")!=nullptr;
    const bool deferredPass=getenv("RENDERDOC_METAL_MRT_DEFERRED_STORE")!=nullptr;
    const bool counterPass=getenv("RENDERDOC_METAL_MRT_COUNTERS")!=nullptr;
    const char *depthFormatName=getenv("RENDERDOC_METAL_MRT_DEPTH_FORMAT");
    const bool depthPass=depthFormatName!=nullptr;
    const bool depthOnlyPass=getenv("RENDERDOC_METAL_MRT_DEPTH_ONLY")!=nullptr;
    if(depthOnlyPass && !depthPass)return 51;
    const bool fragmentlessColor=getenv("RENDERDOC_METAL_MRT_FRAGMENTLESS_COLOR")!=nullptr;
    if(fragmentlessColor && !depthOnlyPass)return 91;
    const bool stencilPass=depthPass && !strcmp(depthFormatName,"d32s8");
    const bool frameDepth=getenv("RENDERDOC_METAL_MRT_FRAME_DEPTH")!=nullptr;
    if(frameDepth && !depthPass)return 41;
    const MTLPixelFormat depthFormat=stencilPass?MTLPixelFormatDepth32Float_Stencil8:
        (depthPass && !strcmp(depthFormatName,"d16")?MTLPixelFormatDepth16Unorm:MTLPixelFormatDepth32Float);
    const bool frameTextureView=getenv("RENDERDOC_METAL_FRAME_MRT_TEXTURE_VIEW")!=nullptr;
    const bool frameIntermediate=getenv("RENDERDOC_METAL_FRAME_MRT_TEXTURE")!=nullptr;
    const bool crossAlias=getenv("RENDERDOC_METAL_CROSS_KIND_ALIAS")!=nullptr;
    if(crossAlias && !frameIntermediate)return 35;
    if(frameTextureView && !frameIntermediate)return 38;
    const unsigned targetCount=getenv("RENDERDOC_METAL_MRT_TARGETS")?
        unsigned(strtoul(getenv("RENDERDOC_METAL_MRT_TARGETS"),nullptr,10)):
        getenv("RENDERDOC_METAL_FIVE_MRT")?5U:2U;
    if(targetCount!=2 && (targetCount<5 || targetCount>8))return 90;
    const bool fiveTargets=targetCount>2;
    const bool directGraphicsBuffers=getenv("RENDERDOC_METAL_MRT_DIRECT_GRAPHICS_BUFFERS")!=nullptr;
    const bool parallelPass=getenv("RENDERDOC_METAL_PARALLEL_MRT")!=nullptr;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice(); NSError *error=nil;
    const bool frameVisibility=getenv("RENDERDOC_METAL_MRT_FRAME_VISIBILITY")!=nullptr;
    id<MTLHeap> visibilityHeap=nil;
    if(frameVisibility && getenv("RENDERDOC_METAL_MRT_FRAME_VISIBILITY_HEAP"))
    {
      auto hd=[MTLHeapDescriptor new];hd.size=65536;hd.type=MTLHeapTypePlacement;
      hd.storageMode=MTLStorageModeShared;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
      visibilityHeap=[device newHeapWithDescriptor:hd];if(!visibilityHeap)return 75;
    }
    id<MTLBuffer> visibilityOutput=getenv("RENDERDOC_METAL_MRT_VISIBILITY")?[device newBufferWithLength:32 options:MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked]:nil;
    if(getenv("RENDERDOC_METAL_MRT_VISIBILITY") && !visibilityOutput)return 75;
    if(visibilityOutput)visibilityOutput.label=@"Resource visibility output";
    id<MTLEvent> interleavedSignal=getenv("RENDERDOC_METAL_MRT_INTERLEAVED_SIGNAL")?[device newEvent]:nil;
    if(getenv("RENDERDOC_METAL_MRT_INTERLEAVED_SIGNAL") && !interleavedSignal)return 74;
    id<MTLHeap> residencyA=nil,residencyB=nil;
    if(getenv("RENDERDOC_METAL_MRT_DUPLICATE_HEAPS")) {
      auto hd=[MTLHeapDescriptor new];hd.size=65536;hd.type=MTLHeapTypePlacement;
      hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
      residencyA=[device newHeapWithDescriptor:hd];residencyB=[device newHeapWithDescriptor:hd];
      if(!residencyA || !residencyB)return 72;
    }
    id<MTLCounterSampleBuffer> counter=nil;
    if(counterPass) {
      if(![device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary])return 46;
      id<MTLCounterSet> timestamps=nil;
      for(id<MTLCounterSet> set in device.counterSets)
        if([set.name isEqualToString:MTLCommonCounterSetTimestamp])timestamps=set;
      if(!timestamps)return 47;
      auto cd=[MTLCounterSampleBufferDescriptor new]; cd.counterSet=timestamps;
      cd.sampleCount=16; cd.storageMode=MTLStorageModeShared;
      counter=[device newCounterSampleBufferWithDescriptor:cd error:&error]; if(!counter)return 48;
    }
    NSString *shaderSource=@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct BufferEntry { device const uint *value [[id(0)]]; ulong z [[id(1)]]; ulong metadata [[id(2)]]; };
struct TextureEntry { ulong z [[id(0)]]; texture2d<float> image [[id(1)]]; ulong metadata [[id(2)]]; };
struct SamplerEntry { sampler point [[id(0)]]; ulong bias [[id(1)]]; ulong metadata [[id(2)]]; };
kernel void resources(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]])
{
  const device BufferEntry *buffer=reinterpret_cast<const device BufferEntry *>(root[0]);
  const device TextureEntry *texture=reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling=reinterpret_cast<const device SamplerEntry *>(root[4]);
  output[0]=buffer->value[0]+uint(texture->image.sample(sampling->point,float2(0.5)).r*255.0+0.5)+17;
  output[1]=(buffer->metadata==0x1111111111111111ul && texture->metadata==0x2222222222222222ul &&
    sampling->bias==0x123456789abcdef0ul && sampling->metadata==0x3333333333333333ul &&
    root[1]==0xabcdef0123456789ul && root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul) ? 0xdeadbeefU : 0xbadU;
}
kernel void resources_indirect(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]], uint3 position [[thread_position_in_grid]])
{
  if(any(position!=uint3(0)))return;
  const device BufferEntry *buffer=reinterpret_cast<const device BufferEntry *>(root[0]);
  const device TextureEntry *texture=reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling=reinterpret_cast<const device SamplerEntry *>(root[4]);
  output[0]=buffer->value[0]+uint(texture->image.sample(sampling->point,float2(0.5)).r*255.0+0.5)+17;
  output[1]=(buffer->metadata==0x1111111111111111ul && texture->metadata==0x2222222222222222ul &&
    sampling->bias==0x123456789abcdef0ul && sampling->metadata==0x3333333333333333ul &&
    root[1]==0xabcdef0123456789ul && root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul) ? 0xdeadbeefU : 0xbadU;
}
kernel void read_alias(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]],
                      const device uint *alias [[buffer(2)]], device uint *first [[buffer(3)]])
{
  const device TextureEntry *texture=reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling=reinterpret_cast<const device SamplerEntry *>(root[4]);
  output[0]=alias[1]+uint(texture->image.sample(sampling->point,float2(0.5)).r*255.0+0.5)+17;
  output[1]=root[1]==0xabcdef0123456789ul?0xdeadbeefU:0xbadU;
  first[0]=output[0];first[1]=output[1];
}
struct VSOut { float4 position [[position]]; float value; };
vertex VSOut resource_vertex(uint index [[vertex_id]], const device ulong *root [[buffer(0)]])
{
  const device BufferEntry *buffer=reinterpret_cast<const device BufferEntry *>(root[0]);
  float2 positions[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
  VSOut out;out.position=float4(positions[index%3],0,1);
  out.value=(buffer->metadata==0x1111111111111111ul && root[1]==0xabcdef0123456789ul) ? float(buffer->value[0]) : 0.0;
  return out;
}
vertex VSOut resource_vertex_direct(uint index [[vertex_id]], const device ulong *root [[buffer(0)]], const device uint *guard [[buffer(2)]])
{
  const device BufferEntry *buffer=reinterpret_cast<const device BufferEntry *>(root[0]);
  float2 positions[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
  VSOut out;out.position=float4(positions[index%3],0,1);
  out.value=(guard[0]==0xdecafbadU && buffer->metadata==0x1111111111111111ul && root[1]==0xabcdef0123456789ul) ? float(buffer->value[0]) : 0.0;
  return out;
}
struct MRTOut { float4 first [[color(0)]]; float4 second [[color(1)]]; };
fragment MRTOut resource_fragment(VSOut in [[stage_in]], const device ulong *root [[buffer(0)]])
{
  const device TextureEntry *texture=reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling=reinterpret_cast<const device SamplerEntry *>(root[4]);
  const bool ordinary=texture->metadata==0x2222222222222222ul && sampling->bias==0x123456789abcdef0ul &&
      sampling->metadata==0x3333333333333333ul && root[1]==0xabcdef0123456789ul &&
      root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul;
  float4 color=ordinary ? float4(texture->image.sample(sampling->point,float2(0.5)).r+(in.value+17.0)/255.0,64.0/255.0,128.0/255.0,1) : float4(1,0,1,1);
  MRTOut out;out.first=color;out.second=color;return out;
}
fragment MRTOut resource_fragment_direct(VSOut in [[stage_in]], const device ulong *root [[buffer(0)]], const device uint *guard [[buffer(2)]])
{
  const device TextureEntry *texture=reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling=reinterpret_cast<const device SamplerEntry *>(root[4]);
  const bool ordinary=guard[0]==0xdecafbadU && texture->metadata==0x2222222222222222ul && sampling->bias==0x123456789abcdef0ul &&
      sampling->metadata==0x3333333333333333ul && root[1]==0xabcdef0123456789ul &&
      root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul;
  float4 color=ordinary ? float4(texture->image.sample(sampling->point,float2(0.5)).r+(in.value+17.0)/255.0,64.0/255.0,128.0/255.0,1) : float4(1,0,1,1);
  MRTOut out;out.first=color;out.second=color;return out;
}
struct MRT5Out { float4 first [[color(0)]]; float4 second [[color(1)]]; float4 third [[color(2)]]; float4 fourth [[color(3)]]; float4 fifth [[color(4)]];
#if MRT_COUNT > 5
  float4 sixth [[color(5)]];
#endif
#if MRT_COUNT > 6
  float4 seventh [[color(6)]];
#endif
#if MRT_COUNT > 7
  float4 eighth [[color(7)]];
#endif
};
fragment MRT5Out resource_fragment_five(VSOut in [[stage_in]], const device ulong *root [[buffer(0)]])
{
  const device TextureEntry *texture=reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling=reinterpret_cast<const device SamplerEntry *>(root[4]);
  const bool ordinary=texture->metadata==0x2222222222222222ul && sampling->bias==0x123456789abcdef0ul &&
      sampling->metadata==0x3333333333333333ul && root[1]==0xabcdef0123456789ul &&
      root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul;
  float4 color=ordinary ? float4(texture->image.sample(sampling->point,float2(0.5)).r+(in.value+17.0)/255.0,64.0/255.0,128.0/255.0,1) : float4(1,0,1,1);
  MRT5Out out;out.first=color;out.second=color;out.third=color;out.fourth=color;out.fifth=color;
#if MRT_COUNT > 5
  out.sixth=color;
#endif
#if MRT_COUNT > 6
  out.seventh=color;
#endif
#if MRT_COUNT > 7
  out.eighth=color;
#endif
  return out;
}
fragment float4 mrt_resolve(VSOut in [[stage_in]], const device ulong *root [[buffer(0)]])
{
  const device TextureEntry *texture=reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling=reinterpret_cast<const device SamplerEntry *>(root[4]);
  const bool ordinary=texture->metadata==0x2222222222222222ul && sampling->bias==0x123456789abcdef0ul &&
      sampling->metadata==0x3333333333333333ul && root[1]==0xabcdef0123456789ul &&
      root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul;
  return ordinary ? texture->image.sample(sampling->point,float2(0.5)) : float4(1,0,1,1);
}
struct CopyRoot { device const ulong *source [[id(0)]]; device ulong *destination [[id(1)]]; };
kernel void copy_resource(const device CopyRoot &root [[buffer(0)]], device uint *output [[buffer(1)]])
{ for(uint i=0;i<3;i++)root.destination[i]=root.source[i]; output[1]=0xdeadbeefU; }
kernel void copy_resource_indirect(const device CopyRoot &root [[buffer(0)]], device uint *output [[buffer(1)]], device uint *args [[buffer(2)]])
{ for(uint i=0;i<3;i++)root.destination[i]=root.source[i]; output[1]=0xdeadbeefU; args[0]=2;args[1]=1;args[2]=1; }
kernel void zero_arguments() {}
kernel void zero_indirect(const device ulong *root [[buffer(0)]], device uint *args [[buffer(2)]])
{ if(root[1]==0xabcdef0123456789ul){args[0]=0;args[1]=1;args[2]=1;} }

)MSL";
    id<MTLLibrary> library=[device newLibraryWithSource:
        [NSString stringWithFormat:@"#define MRT_COUNT %u\n%@",targetCount,shaderSource]
        options:nil error:&error];
    if(!library) { fprintf(stderr,"%s\n",error.localizedDescription.UTF8String); return 2; }
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:indirectPass?@"resources_indirect":@"resources"] error:&error];
    id<MTLComputePipelineState> copyPipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:indirectPass?@"copy_resource_indirect":@"copy_resource"] error:&error];
    id<MTLComputePipelineState> aliasPipeline=crossAlias?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"read_alias"] error:&error]:nil;
    if(crossAlias && !aliasPipeline)return 36;
    id<MTLComputePipelineState> zeroPipeline=zeroIndirect?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"zero_indirect"] error:&error]:nil;
    if(zeroIndirect && !zeroPipeline)return 58;
    id<MTLComputePipelineState> zeroArgumentPipeline=(zeroArgumentCompute || getenv("RENDERDOC_METAL_MRT_DEAD_SLOT_TABLE_BORROW"))?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"zero_arguments"] error:&error]:nil;
    if((zeroArgumentCompute || getenv("RENDERDOC_METAL_MRT_DEAD_SLOT_TABLE_BORROW")) && !zeroArgumentPipeline)return 70;
    MTLRenderPipelineDescriptor *renderDescriptor=[MTLRenderPipelineDescriptor new];
    renderDescriptor.vertexFunction=[library newFunctionWithName:directGraphicsBuffers?@"resource_vertex_direct":@"resource_vertex"];
    renderDescriptor.fragmentFunction=[library newFunctionWithName:directGraphicsBuffers?@"resource_fragment_direct":fiveTargets?@"resource_fragment_five":@"resource_fragment"];
    if(depthPass) { renderDescriptor.depthAttachmentPixelFormat=depthFormat;
      renderDescriptor.stencilAttachmentPixelFormat=stencilPass?depthFormat:MTLPixelFormatInvalid; }
    renderDescriptor.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;
    renderDescriptor.colorAttachments[1].pixelFormat=MTLPixelFormatRGBA8Unorm;
    if(fiveTargets)for(unsigned slot=2;slot<targetCount;slot++)renderDescriptor.colorAttachments[slot].pixelFormat=MTLPixelFormatRGBA8Unorm;
    id<MTLRenderPipelineState> renderPipeline=[device newRenderPipelineStateWithDescriptor:renderDescriptor error:&error];
    if(!renderPipeline){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 20;}
    id<MTLRenderPipelineState> depthOnlyPipeline=nil;
    if(depthOnlyPass) {
      MTLRenderPipelineDescriptor *depthOnlyDescriptor=[renderDescriptor copy]; depthOnlyDescriptor.fragmentFunction=nil;
      for(unsigned slot=0;slot<8;slot++)depthOnlyDescriptor.colorAttachments[slot].pixelFormat=MTLPixelFormatInvalid;
      if(fragmentlessColor)depthOnlyDescriptor.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;
      depthOnlyPipeline=[device newRenderPipelineStateWithDescriptor:depthOnlyDescriptor error:&error];
      if(!depthOnlyPipeline){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 52;}
    }
    renderDescriptor.fragmentFunction=[library newFunctionWithName:@"mrt_resolve"];
    if(directGraphicsBuffers) renderDescriptor.vertexFunction=[library newFunctionWithName:@"resource_vertex"];
    for(unsigned slot=1;slot<targetCount;slot++)renderDescriptor.colorAttachments[slot].pixelFormat=MTLPixelFormatInvalid;
    id<MTLRenderPipelineState> resolvePipeline=[device newRenderPipelineStateWithDescriptor:renderDescriptor error:&error];
    MTLTextureDescriptor *intermediateDescriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:2 height:2 mipmapped:NO];
    intermediateDescriptor.storageMode=MTLStorageModeShared;
    intermediateDescriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
    id<MTLTexture> intermediate=[device newTextureWithDescriptor:intermediateDescriptor];
    id<MTLTexture> fragmentlessTarget=fragmentlessColor?[device newTextureWithDescriptor:intermediateDescriptor]:nil;
    if(fragmentlessColor && !fragmentlessTarget)return 92;
    id<MTLTexture> baseIntermediate=intermediate;
    id<MTLTexture> sampledIntermediate=intermediate;
    MTLTextureDescriptor *frameDescriptor=[intermediateDescriptor copy];
    frameDescriptor.storageMode=MTLStorageModePrivate;frameDescriptor.hazardTrackingMode=MTLHazardTrackingModeTracked;
    id<MTLHeap> frameHeap=nil;NSUInteger frameOffset=0;
    if(frameIntermediate)
    {
      const MTLSizeAndAlign layout=[device heapTextureSizeAndAlignWithDescriptor:frameDescriptor];
      if(!layout.size||!layout.align||layout.size+layout.align>1024*1024)return 30;
      auto heapDescriptor=[MTLHeapDescriptor new];heapDescriptor.type=MTLHeapTypePlacement;
      heapDescriptor.storageMode=MTLStorageModePrivate;heapDescriptor.hazardTrackingMode=MTLHazardTrackingModeTracked;
      heapDescriptor.size=MAX(NSUInteger(4096),layout.size+layout.align);frameOffset=crossAlias?0:layout.align;
      frameHeap=[device newHeapWithDescriptor:heapDescriptor];if(!frameHeap)return 31;
    }
    NSMutableArray<id<MTLTexture>> *extraTargets=[NSMutableArray new];
    if(fiveTargets)for(unsigned slot=2;slot<targetCount;slot++)[extraTargets addObject:[device newTextureWithDescriptor:intermediateDescriptor]];
    const uint64_t resolveBytes[]={0,intermediate.gpuResourceID._impl,0x2222222222222222ULL};
    id<MTLBuffer> resolveTable=nil;
    if(!resolvePipeline||!intermediate)return 24;
    const uint32_t inputValues[]={0xbadU,41U,80U};
    MTLTextureDescriptor *depthDescriptor=nil; id<MTLTexture> depthTexture=nil;
    id<MTLHeap> depthHeap=nil; id<MTLDepthStencilState> depthState=nil, resolveDepthState=nil;
    if(depthPass)
    {
      depthDescriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:depthFormat width:2 height:2 mipmapped:NO];
      depthDescriptor.storageMode=MTLStorageModePrivate; depthDescriptor.hazardTrackingMode=MTLHazardTrackingModeTracked;
      depthDescriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
      if(frameDepth) {
        auto hd=[MTLHeapDescriptor new]; hd.type=MTLHeapTypePlacement; hd.storageMode=MTLStorageModePrivate;
        hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
        auto size=[device heapTextureSizeAndAlignWithDescriptor:depthDescriptor];
        hd.size=MAX(NSUInteger(65536),size.size+size.align); depthHeap=[device newHeapWithDescriptor:hd];
        if(!depthHeap || hd.size>1024*1024)return 42;
      } else depthTexture=[device newTextureWithDescriptor:depthDescriptor];
      auto ds=[MTLDepthStencilDescriptor new]; ds.depthCompareFunction=MTLCompareFunctionLessEqual; ds.depthWriteEnabled=YES;
      if(stencilPass) {
        auto stencil=[MTLStencilDescriptor new]; stencil.stencilCompareFunction=MTLCompareFunctionEqual;
        stencil.depthStencilPassOperation=MTLStencilOperationIncrementClamp;
        ds.frontFaceStencil=stencil; ds.backFaceStencil=stencil;
      }
      depthState=[device newDepthStencilStateWithDescriptor:ds];
      ds.depthCompareFunction=MTLCompareFunctionEqual; ds.depthWriteEnabled=NO;
      if(stencilPass) { ds.frontFaceStencil.depthStencilPassOperation=MTLStencilOperationKeep;
        ds.backFaceStencil.depthStencilPassOperation=MTLStencilOperationKeep; }
      resolveDepthState=[device newDepthStencilStateWithDescriptor:ds];
      if(!depthState || !resolveDepthState || (!frameDepth && !depthTexture))return 43;
    }
    id<MTLBuffer> input=[device newBufferWithBytes:inputValues length:12 options:MTLResourceStorageModeShared];
    MTLTextureDescriptor *descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    descriptor.storageMode=MTLStorageModeShared; descriptor.usage=MTLTextureUsageShaderRead;
    id<MTLTexture> image=[device newTextureWithDescriptor:descriptor];
    const uint8_t pixel[]={64,128,192,255};
    [image replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:pixel bytesPerRow:4];
    id<MTLTexture> other=[device newTextureWithDescriptor:descriptor];
    const uint8_t otherPixel[]={128,128,192,255};
    [other replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:otherPixel bytesPerRow:4];
    const uint64_t otherID=other.gpuResourceID._impl;
    MTLSamplerDescriptor *sampling=[MTLSamplerDescriptor new]; sampling.supportArgumentBuffers=YES;
    id<MTLSamplerState> point=[device newSamplerStateWithDescriptor:sampling];
    const uint64_t bufferBytes[]={input.gpuAddress+4,0,0x1111111111111111ULL};
    const uint64_t textureBytes[]={0,image.gpuResourceID._impl,0x2222222222222222ULL};
    const uint64_t samplerBytes[]={point.gpuResourceID._impl,0x123456789abcdef0ULL,0x3333333333333333ULL};
    id<MTLBuffer> bufferTable=[device newBufferWithBytes:bufferBytes length:24 options:MTLResourceStorageModeShared];
    id<MTLBuffer> textureTable=[device newBufferWithLength:unusedProducerSlot?48:24 options:MTLResourceStorageModeShared];
    memcpy(textureTable.contents,textureBytes,24);
    if(unusedProducerSlot)memset((char *)textureTable.contents+24,0,24);
    id<MTLBuffer> samplerTable=[device newBufferWithBytes:samplerBytes length:24 options:MTLResourceStorageModeShared];
    uint64_t payloadBytes[]={0,otherID,0x2222222222222222ULL};
    id<MTLBuffer> payload=[device newBufferWithLength:freshCPUSlot?48:24 options:MTLResourceStorageModeShared];
    memcpy(payload.contents,payloadBytes,24);
    if(freshCPUSlot)memset((char *)payload.contents+24,0,24);
    resolveTable=[device newBufferWithBytes:resolveBytes length:24 options:MTLResourceStorageModeShared];
    resolveTable.label=@"Resource MRT resolve";
    uint64_t frameTextureIDs[2]={};
    if(!resolveTable)return 24;
    textureTable.label=@"Resource texture destination";payload.label=@"Resource texture payload";
    id<MTLBuffer> graphicsGuard=directGraphicsBuffers?[device newBufferWithLength:32 options:MTLResourceStorageModeShared]:nil;
    if(directGraphicsBuffers) {if(!graphicsGuard)return 73;uint32_t words[8]={};words[4]=0xdecafbadU;memcpy(graphicsGuard.contents,words,sizeof(words));}
    id<MTLBuffer> output=[device newBufferWithLength:8 options:MTLResourceStorageModeShared];
    id<MTLBuffer> firstAliasOutput=crossAlias?[device newBufferWithLength:16 options:MTLResourceStorageModeShared]:nil;
    if(crossAlias && !firstAliasOutput)return 39;
    if(crossAlias)memset(firstAliasOutput.contents,0,16);
    const uint64_t tableVAs[]={bufferTable.gpuAddress,textureTable.gpuAddress,samplerTable.gpuAddress};
    if(!pipeline || !input || !image || !point || !output) return 3;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path);
      RENDERDOC_AnnotationValue coverage={}; coverage.uint32=(fragmentlessColor || targetCount>5 || getenv("RENDERDOC_METAL_MRT_BLIT_MARKERS") || residencyA || directGraphicsBuffers || visibilityOutput || getenv("RENDERDOC_METAL_MRT_LOGICAL_GPU_RETIREMENT"))?65:mixedFreshCPUSlot?64:unusedProducerSlot?63:freshCPUSlot?62:getenv("RENDERDOC_METAL_MRT_INITIAL_CPU_SLOT")?61:getenv("RENDERDOC_METAL_MRT_NATIVE_BUDGET")?60:renderIndirect?58:indirectPass?57:depthOnlyPass?56:deferredPass?55:counterPass?54:depthPass?53:frameTextureView?26:(crossAlias?24:(frameIntermediate?21:((fiveTargets||parallelPass)?20:9)));
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage)) return 5;
    }
    auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
      if(!api) return uint32_t(0);
      RENDERDOC_AnnotationValue v={}; v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
    };
    id<MTLBuffer> tables[]={bufferTable,textureTable,samplerTable};
    id sources[]={input,image,point};
    const uint64_t types[]={0,4,7}, kinds[]={0,1,2};
    for(unsigned i=0;i<3;i++)
      if(annotation(tables[i],"metal.descriptorTable",i==2?2:1,0,(unusedProducerSlot && i==1)?2:1,24) ||
         annotation(tables[i],"metal.descriptorSlotEvent",0,1,0,types[i]) ||
         annotation(tables[i],"metal.descriptorSlotEvent",0,1,2,types[i]) ||
         annotation(tables[i],"metal.descriptorSlotBinding",0,kinds[i],(uint64_t)(__bridge void *)sources[i],i==0?4:0)) return 6;
    if((!getenv("RENDERDOC_METAL_MRT_FRESH_UNDECLARED") && annotation(payload,"metal.descriptorTable",1,0,freshCPUSlot?2:1,24)) ||
       annotation(payload,"metal.descriptorSlotEvent",0,1,0,4)) return 11;
    if(api)
    {
      RENDERDOC_AnnotationValue gpu={};gpu.uint32=1;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)textureTable,"metal.descriptorGPUWrites",eRENDERDOC_UInt32,0,&gpu))return 12;
    }
    if(annotation(resolveTable,"metal.descriptorTable",1,0,1,24) ||
       annotation(resolveTable,"metal.descriptorSlotEvent",0,1,0,4) ||
       annotation(resolveTable,"metal.descriptorSlotEvent",0,1,2,4) ||
       annotation(resolveTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)intermediate,0))return 25;
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    const uint32_t initialArguments[16]={0,0,0,0,1,1,1};
    id<MTLBuffer> argumentUpload=nil,arguments=nil;
    if(indirectPass) {
      argumentUpload=[device newBufferWithBytes:initialArguments length:sizeof(initialArguments) options:MTLResourceStorageModeShared];
      arguments=privateArguments?[device newBufferWithLength:sizeof(initialArguments) options:MTLResourceStorageModePrivate]:argumentUpload;
      if(!arguments)return 56;
    }
    uint32_t renderArgumentWords[32]={};
    renderArgumentWords[4]=3;renderArgumentWords[5]=1;renderArgumentWords[16]=6;renderArgumentWords[17]=1;
    renderArgumentWords[renderIndexed?8:7]=2;renderArgumentWords[renderIndexed?20:19]=2;
    renderArgumentWords[25]=1;renderArgumentWords[renderIndexed?28:27]=2;
    const uint16_t renderIndexWords[6]={0,1,2,0,1,2};
    const uint32_t renderZeroWords[32]={};
    id<MTLBuffer> renderZeroUpload=renderIndirect?[device newBufferWithBytes:renderZeroWords length:sizeof(renderZeroWords) options:MTLResourceStorageModeShared]:nil;
    id<MTLBuffer> renderUpload=renderIndirect?[device newBufferWithBytes:renderArgumentWords length:sizeof(renderArgumentWords) options:MTLResourceStorageModeShared]:nil;
    id<MTLHeap> renderArgumentHeap=nil;
    if(renderIndirect && getenv("RENDERDOC_METAL_MRT_RENDER_ARGUMENT_HEAP"))
    {
      if(!privateArguments)return 62;
      auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;
      hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=getenv("RENDERDOC_METAL_MRT_ARGUMENT_HEAP_MIB")?strtoull(getenv("RENDERDOC_METAL_MRT_ARGUMENT_HEAP_MIB"),nullptr,10)*1024*1024:65536;
      renderArgumentHeap=[device newHeapWithDescriptor:hd];
    }
    id<MTLBuffer> renderArguments=renderIndirect?(renderArgumentHeap?[renderArgumentHeap newBufferWithLength:sizeof(renderArgumentWords) options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:0]:privateArguments?[device newBufferWithLength:sizeof(renderArgumentWords) options:MTLResourceStorageModePrivate]:renderUpload):nil;
    id<MTLBuffer> renderIndices=renderIndirect&&renderIndexed?[device newBufferWithBytes:renderIndexWords length:sizeof(renderIndexWords) options:MTLResourceStorageModeShared]:nil;
    if(renderIndirect && (!renderUpload || !renderArguments || !renderZeroUpload || (renderIndexed&&!renderIndices)))return 58;
    id<MTLCommandQueue> queue=[device newCommandQueue];
    if(depthPass && !frameDepth) {
      auto initial=[queue commandBuffer]; auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.depthAttachment.texture=depthTexture; pass.depthAttachment.loadAction=MTLLoadActionClear;
      pass.depthAttachment.storeAction=MTLStoreActionStore; pass.depthAttachment.clearDepth=1;
      if(stencilPass) { pass.stencilAttachment.texture=depthTexture; pass.stencilAttachment.loadAction=MTLLoadActionClear;
        pass.stencilAttachment.storeAction=MTLStoreActionStore; pass.stencilAttachment.clearStencil=3; }
      [[initial renderCommandEncoderWithDescriptor:pass] endEncoding]; [initial commit]; [initial waitUntilCompleted];
      if(initial.error)return 44;
    }
    id<MTLBuffer> aliasBuffer=crossAlias?[frameHeap newBufferWithLength:4096 options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:0]:nil;
    if(crossAlias && !aliasBuffer)return 37;
    for(int capture=0;capture<(api?2:1);capture++)
    {
      const bool reuseGPUGeneration=getenv("RENDERDOC_METAL_MRT_SUBMITTED_GPU_REUSE")!=nullptr;
      const uint64_t textureGeneration=reuseGPUGeneration?uint64_t(capture)*100+1:capture+1;
      if(crossAlias)
      {
        id<MTLCommandBuffer> initialize=[queue commandBuffer];id<MTLBlitCommandEncoder> copy=[initialize blitCommandEncoder];
        [copy copyFromBuffer:input sourceOffset:0 toBuffer:aliasBuffer destinationOffset:0 size:12];[copy endEncoding];
        [initialize commit];[initialize waitUntilCompleted];if(initialize.error)return 38;
      }
      id<MTLTexture> nextImage=capture==0?other:image;
      payloadBytes[1]=capture==0?otherID:textureBytes[1];
      memcpy(payload.contents,payloadBytes,24);
      if(annotation(payload,"metal.descriptorSlotEvent",0,1,2,4) ||
         annotation(payload,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)nextImage,0))return 13;
      if(indirectPass) {
        memcpy(argumentUpload.contents,initialArguments,sizeof(initialArguments));
        if(privateArguments) { id<MTLCommandBuffer> initial=[queue commandBuffer];
          id<MTLBlitCommandEncoder> blit=[initial blitCommandEncoder]; [blit copyFromBuffer:argumentUpload sourceOffset:0 toBuffer:arguments destinationOffset:0 size:64];
          [blit endEncoding];[initial commit];[initial waitUntilCompleted];if(initial.error)return 57; }
      }
      if(renderIndirect)
      {
        memcpy(renderUpload.contents,renderArgumentWords,sizeof(renderArgumentWords));
        if(privateArguments)
        {
          id<MTLCommandBuffer> initial=[queue commandBuffer];id<MTLBlitCommandEncoder> copy=[initial blitCommandEncoder];
          [copy copyFromBuffer:renderUpload sourceOffset:0 toBuffer:renderArguments destinationOffset:0 size:sizeof(renderArgumentWords)];
          [copy endEncoding];[initial commit];[initial waitUntilCompleted];if(initial.error)return 59;
        }
      }
      if(visibilityOutput) {uint64_t words[]={0x1234,0,0x5678,0xdead};memcpy(visibilityOutput.contents,words,sizeof(words));}
      if(capture && getenv("RENDERDOC_METAL_MRT_LOGICAL_GPU_RETIREMENT")) {
        if(annotation(textureTable,"metal.descriptorSlotEvent",0,textureGeneration,0,4) ||
           annotation(textureTable,"metal.descriptorSlotEvent",0,textureGeneration,2,4) ||
           annotation(textureTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)other,0))return 77;
      }
      if(api) api->StartFrameCapture(nullptr,nullptr);
      if(frameVisibility)
      {
        visibilityOutput=visibilityHeap?[visibilityHeap newBufferWithLength:32 options:MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked offset:1024+capture*1024]:[device newBufferWithLength:32 options:MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked];
        if(!visibilityOutput)return 75;
        visibilityOutput.label=@"Resource visibility output";
        uint64_t words[]={0x1234,0,0x5678,0xdead};memcpy(visibilityOutput.contents,words,sizeof(words));
      }
      if(getenv("RENDERDOC_METAL_MRT_INITIAL_CPU_SLOT"))
      {
        memcpy(textureTable.contents,payload.contents,24);
        id<MTLTexture> source=capture?image:other;
        if(annotation(textureTable,"metal.descriptorSlotEvent",0,1,2,4) ||
           annotation(textureTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)source,0))return 63;
      }
      if(frameDepth) { depthTexture=[depthHeap newTextureWithDescriptor:depthDescriptor offset:0]; if(!depthTexture)return 45; }
      if(frameIntermediate)
      {
        intermediate=[frameHeap newTextureWithDescriptor:frameDescriptor offset:frameOffset];if(!intermediate)return 32;
        sampledIntermediate=frameTextureView?[intermediate newTextureViewWithPixelFormat:MTLPixelFormatRGBA8Unorm textureType:MTLTextureType2D levels:NSMakeRange(0,1) slices:NSMakeRange(0,1)]:intermediate;
        if(!sampledIntermediate)return 39;
        if(frameTextureView)fprintf(stderr,"FRAME_VIEW capture=%d parentID=%llu viewID=%llu parentObject=%p viewObject=%p\n",capture,(unsigned long long)intermediate.gpuResourceID._impl,(unsigned long long)sampledIntermediate.gpuResourceID._impl,(__bridge void *)intermediate,(__bridge void *)sampledIntermediate);
        const uint64_t frameBytes[]={0,sampledIntermediate.gpuResourceID._impl,0x2222222222222222ULL};
        frameTextureIDs[capture]=frameBytes[1];
        memcpy(resolveTable.contents,frameBytes,sizeof(frameBytes));
        if(annotation(resolveTable,"metal.descriptorSlotEvent",0,1,2,4) ||
           annotation(resolveTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)sampledIntermediate,0))return 33;
      }
      uint64_t root[6]={};for(unsigned i=0;i<3;i++){root[i*2]=tableVAs[i];root[i*2+1]=0xabcdef0123456789ULL;}
      id<MTLCommandBuffer> signalCommand=interleavedSignal?[queue commandBuffer]:nil;
      if(signalCommand)[signalCommand enqueue];
      id<MTLCommandBuffer> earlier=frameVisibility && getenv("RENDERDOC_METAL_MRT_VISIBILITY_EARLIER_SNAPSHOT")?[queue commandBuffer]:nil;
      if(earlier)[earlier enqueue];
      id<MTLCommandBuffer> command=[queue commandBuffer];[command enqueue];
      auto indirectDraw=[&](id<MTLRenderCommandEncoder> encoder,NSUInteger offset) {
        [encoder useResource:renderArguments usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
        if(api)
        {
          RENDERDOC_AnnotationValue declared={};declared.uint32=1;
          if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)encoder,
              "metal.renderWritesDeclared",eRENDERDOC_UInt32,0,&declared))return false;
        }
        if(renderIndexed)[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexType:MTLIndexTypeUInt16 indexBuffer:renderIndices indexBufferOffset:0 indirectBuffer:renderArguments indirectBufferOffset:offset];
        else [encoder drawPrimitives:MTLPrimitiveTypeTriangle indirectBuffer:renderArguments indirectBufferOffset:offset];
        return true;
      };
      bool firstConsume=true;unsigned consumeOrdinal=0;
      auto consume=[&]() {
        const bool firstIndirect=firstConsume;
        const bool readOldAlias=crossAlias && firstConsume;firstConsume=false;
        id<MTLComputeCommandEncoder> compute=nil;
        if(counterPass) {
          auto cp=[MTLComputePassDescriptor new]; cp.sampleBufferAttachments[0].sampleBuffer=counter;
          cp.sampleBufferAttachments[0].startOfEncoderSampleIndex=4+2*consumeOrdinal;
          cp.sampleBufferAttachments[0].endOfEncoderSampleIndex=5+2*consumeOrdinal;
          compute=[command computeCommandEncoderWithDescriptor:cp]; consumeOrdinal++;
        } else compute=[command computeCommandEncoder];
        [compute setComputePipelineState:readOldAlias?aliasPipeline:pipeline];
        if(residencyA) {
          id<MTLHeap> __unsafe_unretained heaps[]={residencyA,residencyB,residencyA,residencyB};
          [compute useHeaps:heaps count:4];
        }
        if(readOldAlias)
        {
          [compute setBuffer:aliasBuffer offset:0 atIndex:2];
          [compute setBuffer:firstAliasOutput offset:0 atIndex:3];
        }
        if(annotation(compute,"metal.descriptorInlineLayout",0,0,3,16))return false;
        for(unsigned i=0;i<3;i++)
        {
          if(annotation(compute,"metal.descriptorInlineBinding",0,i,(uint64_t)(__bridge void *)tables[i],0))return false;
          [compute useResource:tables[i] usage:MTLResourceUsageRead];
        }
        if(freshCPUSlot)[compute useResource:payload usage:MTLResourceUsageRead];
        [compute setBytes:root length:sizeof(root) atIndex:0];[compute setBuffer:output offset:0 atIndex:1];
        [compute useResource:input usage:MTLResourceUsageRead];[compute useResource:image usage:MTLResourceUsageRead];
        [compute useResource:other usage:MTLResourceUsageRead];
        if(indirectPass)[compute dispatchThreadgroupsWithIndirectBuffer:arguments indirectBufferOffset:16 threadsPerThreadgroup:MTLSizeMake(1,1,1)];
        else [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
        if(indirectPass && zeroIndirect && !firstIndirect) {
          [compute memoryBarrierWithScope:MTLBarrierScopeBuffers];
          [compute setComputePipelineState:zeroPipeline];[compute setBuffer:arguments offset:16 atIndex:2];
          [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
          [compute memoryBarrierWithScope:MTLBarrierScopeBuffers];[compute setComputePipelineState:pipeline];
          [compute dispatchThreadgroupsWithIndirectBuffer:arguments indirectBufferOffset:16 threadsPerThreadgroup:MTLSizeMake(1,1,1)];
        }
        [compute endEncoding];return true;
      };
      if(!consume())return 14;
      const uint64_t payloadOffset=freshCPUSlot?24:0;
      if(freshCPUSlot)
      {
        memcpy((char *)payload.contents+payloadOffset,payloadBytes,24);
        if(annotation(payload,"metal.descriptorSlotEvent",payloadOffset,capture+2,0,4) ||
           annotation(payload,"metal.descriptorSlotEvent",payloadOffset,capture+2,2,4) ||
           annotation(payload,"metal.descriptorSlotBinding",payloadOffset,1,(uint64_t)(__bridge void *)nextImage,0))return 64;
      }
      if(unusedProducerSlot && annotation(textureTable,"metal.descriptorSlotEvent",24,capture+2,0,4))return 66;
      id<MTLComputeCommandEncoder> producer=[command computeCommandEncoder];[producer setComputePipelineState:copyPipeline];
      const uint64_t copyRoot[]={payload.gpuAddress+payloadOffset,textureTable.gpuAddress};
      if(annotation(producer,"metal.descriptorInlineLayout",0,0,2,8) ||
         annotation(producer,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void *)payload,payloadOffset) ||
         annotation(producer,"metal.descriptorInlineBinding",0,1,(uint64_t)(__bridge void *)textureTable,0))return 15;
      [producer setBytes:copyRoot length:16 atIndex:0];[producer setBuffer:output offset:0 atIndex:1];[producer useResource:payload usage:MTLResourceUsageRead];
      [producer useResource:textureTable usage:MTLResourceUsageWrite];
      if(indirectPass)[producer setBuffer:arguments offset:16 atIndex:2];
      [producer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      if(annotation(textureTable,"metal.descriptorSlotProducer",0,(uint64_t)(__bridge void *)producer,(uint64_t)(__bridge void *)payload,payloadOffset))return 16;
      if(api)
      {
        RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=0;memcpy(&value.vector.uint64[1],payloadBytes,24);
        if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)textureTable,"metal.descriptorSlotGPUValue",eRENDERDOC_UInt64,4,&value))return 17;
      }
      if(annotation(textureTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)nextImage,0))return 18;
      [producer endEncoding];
      if(unusedProducerSlot)
      {
        if(mixedFreshCPUSlot)
        {
          memcpy((char *)textureTable.contents+24,payloadBytes,24);
          if(annotation(textureTable,"metal.descriptorSlotEvent",24,capture+2,2,4) ||
             annotation(textureTable,"metal.descriptorSlotBinding",24,1,(uint64_t)(__bridge void *)nextImage,0))return 68;
        }
        else if(annotation(textureTable,"metal.descriptorSlotEvent",24,capture+2,1,4))return 67;
      }
      if(zeroArgumentCompute)
      {
        auto empty=[command computeCommandEncoder];[empty setComputePipelineState:zeroArgumentPipeline];
        if(annotation(empty,"metal.descriptorInlineLayout",0,0,3,16))return 71;
        for(unsigned i=0;i<3;i++)
          if(annotation(empty,"metal.descriptorInlineBinding",0,i,(uint64_t)(__bridge void *)tables[i],0))return 72;
        [empty setBytes:root length:sizeof(root) atIndex:0];
        [empty setBuffer:output offset:0 atIndex:1];
        [empty dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[empty endEncoding];
      }
      if(!consume())return 19;
      if(depthOnlyPass) {
        auto dp=[MTLRenderPassDescriptor renderPassDescriptor];
        if(fragmentlessColor) {
          dp.colorAttachments[0].texture=fragmentlessTarget;
          dp.colorAttachments[0].loadAction=MTLLoadActionClear;
          dp.colorAttachments[0].storeAction=deferredPass?MTLStoreActionUnknown:MTLStoreActionStore;
          dp.colorAttachments[0].clearColor=MTLClearColorMake(80.0/255,64.0/255,128.0/255,1);
        }
        dp.depthAttachment.texture=depthTexture; dp.depthAttachment.loadAction=MTLLoadActionClear;
        dp.depthAttachment.storeAction=deferredPass?MTLStoreActionUnknown:MTLStoreActionStore; dp.depthAttachment.clearDepth=1;
        if(stencilPass) { dp.stencilAttachment.texture=depthTexture; dp.stencilAttachment.loadAction=MTLLoadActionClear;
          dp.stencilAttachment.storeAction=deferredPass?MTLStoreActionUnknown:MTLStoreActionStore; dp.stencilAttachment.clearStencil=7; }
        id<MTLParallelRenderCommandEncoder> parent=parallelPass?[command parallelRenderCommandEncoderWithDescriptor:dp]:nil;
        id<MTLRenderCommandEncoder> child=parallelPass?[parent renderCommandEncoder]:[command renderCommandEncoderWithDescriptor:dp];
        [child setRenderPipelineState:depthOnlyPipeline]; [child setDepthStencilState:depthState];
        if(stencilPass)[child setStencilReferenceValue:7];
        if(annotation(child,"metal.descriptorInlineLayout",1,0,3,16))return 53;
        for(unsigned i=0;i<3;i++)if(annotation(child,"metal.descriptorInlineBinding",uint64_t(1)<<32,i,(uint64_t)(__bridge void *)tables[i],0))return 54;
        [child setVertexBytes:root length:sizeof(root) atIndex:0];
        for(id<MTLBuffer> table:tables)[child useResource:table usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
        [child useResource:input usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
        if(fragmentlessColor) {
          if(annotation(child,"metal.descriptorInlineLayout",2,0,3,16))return 93;
          for(unsigned i=0;i<3;i++)if(annotation(child,"metal.descriptorInlineBinding",uint64_t(2)<<32,i,(uint64_t)(__bridge void *)tables[i],0))return 94;
          [child setFragmentBytes:root length:sizeof(root) atIndex:0];
          [child setFragmentBuffer:input offset:0 atIndex:2];
        }
        [child drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        if(deferredPass && !parallelPass) { if(fragmentlessColor)[child setColorStoreAction:MTLStoreActionStore atIndex:0]; [child setDepthStoreAction:MTLStoreActionStore];
          if(stencilPass)[child setStencilStoreAction:MTLStoreActionStore]; }
        [child endEncoding];
        if(parallelPass) { if(deferredPass) { if(fragmentlessColor)[parent setColorStoreAction:MTLStoreActionStore atIndex:0]; [parent setDepthStoreAction:MTLStoreActionStore];
            if(stencilPass)[parent setStencilStoreAction:MTLStoreActionStore]; } [parent endEncoding]; }
      }
      id<CAMetalDrawable> drawable=[layer nextDrawable];MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=drawable.texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
      pass.colorAttachments[0].storeAction=(parallelPass||deferredPass)?MTLStoreActionUnknown:MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(0.125,0.25,0.5,1);
      pass.colorAttachments[1].texture=intermediate;pass.colorAttachments[1].loadAction=MTLLoadActionClear;
      pass.colorAttachments[1].storeAction=(parallelPass||deferredPass)?MTLStoreActionUnknown:MTLStoreActionStore;
      if(fiveTargets)for(unsigned slot=2;slot<targetCount;slot++)
      {
        pass.colorAttachments[slot].texture=extraTargets[slot-2];
        pass.colorAttachments[slot].loadAction=MTLLoadActionClear;
        pass.colorAttachments[slot].storeAction=(parallelPass||deferredPass)?MTLStoreActionUnknown:MTLStoreActionStore;
      }
      if(depthPass) {
        pass.depthAttachment.texture=depthTexture; pass.depthAttachment.loadAction=MTLLoadActionClear;
        pass.depthAttachment.storeAction=deferredPass?MTLStoreActionUnknown:MTLStoreActionStore; pass.depthAttachment.clearDepth=1;
        if(stencilPass) { pass.stencilAttachment.texture=depthTexture; pass.stencilAttachment.loadAction=MTLLoadActionClear;
          pass.stencilAttachment.storeAction=deferredPass?MTLStoreActionUnknown:MTLStoreActionStore; pass.stencilAttachment.clearStencil=7; }
      }
      if(counterPass) {
        pass.sampleBufferAttachments[0].sampleBuffer=counter;
        pass.sampleBufferAttachments[0].startOfVertexSampleIndex=0;
        pass.sampleBufferAttachments[0].endOfFragmentSampleIndex=1;
      }
      if(visibilityOutput)pass.visibilityResultBuffer=visibilityOutput;
      id<MTLParallelRenderCommandEncoder> parallel=parallelPass?[command parallelRenderCommandEncoderWithDescriptor:pass]:nil;
      if(parallelPass)
      {
        // One empty child followed by a descriptor-consuming child. Parent ownership
        // remains live between children and no clear/store is reissued for a child.
        id<MTLRenderCommandEncoder> empty=[parallel renderCommandEncoder];[empty endEncoding];
      }
      id<MTLRenderCommandEncoder> render=parallelPass?[parallel renderCommandEncoder]:[command renderCommandEncoderWithDescriptor:pass];
      [render setRenderPipelineState:renderPipeline];
      if(visibilityOutput)[render setVisibilityResultMode:MTLVisibilityResultModeCounting offset:8];
      if(residencyA) {
        id<MTLHeap> __unsafe_unretained heaps[]={residencyA,residencyB,residencyA,residencyB};
        [render useHeaps:heaps count:4 stages:MTLRenderStageVertex|MTLRenderStageFragment];
      }
      if(depthPass) { [render setDepthStencilState:depthState]; if(stencilPass)[render setStencilReferenceValue:7]; }
      for(unsigned stage=1;stage<=2;stage++)
      {
        if(annotation(render,"metal.descriptorInlineLayout",stage,0,3,16)){[render endEncoding];return 21;}
        for(unsigned i=0;i<3;i++)
        {
          if(annotation(render,"metal.descriptorInlineBinding",uint64_t(stage)<<32,i,(uint64_t)(__bridge void *)tables[i],0)){[render endEncoding];return 22;}
          if(signalCommand && stage==1 && i==0) {
            [signalCommand encodeSignalEvent:interleavedSignal value:1+capture];
            [signalCommand commit];
          }
        }
      }
      if(directGraphicsBuffers) {
        [render setVertexBuffer:graphicsGuard offset:16 atIndex:0];
        [render setFragmentBuffer:graphicsGuard offset:16 atIndex:0];
        [render setVertexBuffer:graphicsGuard offset:16 atIndex:2];
        [render setFragmentBuffer:graphicsGuard offset:16 atIndex:2];
        [render setVertexBuffer:graphicsGuard offset:16 atIndex:3];
        [render setFragmentBuffer:graphicsGuard offset:16 atIndex:3];
        [render setVertexBuffer:nil offset:0 atIndex:3];
        [render setFragmentBuffer:nil offset:0 atIndex:3];
      }
      [render setVertexBytes:root length:sizeof(root) atIndex:0];
      [render setFragmentBytes:root length:sizeof(root) atIndex:0];
      for(id<MTLBuffer> table:tables)[render useResource:table usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
      [render useResource:input usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
      [render useResource:image usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
      [render useResource:other usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
      if(renderIndirect) { if(!indirectDraw(render,16)) { [render endEncoding];if(parallel)[parallel endEncoding];return 60; } }
      else [render drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
      if(deferredPass && !parallelPass) {
        for(unsigned slot=0;slot<targetCount;slot++)[render setColorStoreAction:MTLStoreActionStore atIndex:slot];
        if(depthPass)[render setDepthStoreAction:MTLStoreActionStore];
        if(stencilPass)[render setStencilStoreAction:MTLStoreActionStore];
      }
      if(visibilityOutput)[render setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0];
      [render endEncoding];
      if(parallelPass)
      {
        for(unsigned slot=0;slot<targetCount;slot++)[parallel setColorStoreAction:MTLStoreActionStore atIndex:slot];
        if(deferredPass && depthPass)[parallel setDepthStoreAction:MTLStoreActionStore];
        if(deferredPass && stencilPass)[parallel setStencilStoreAction:MTLStoreActionStore];
        [parallel endEncoding];
      }
      if(getenv("RENDERDOC_METAL_MRT_LOGICAL_GPU_RETIREMENT") &&
         annotation(textureTable,"metal.descriptorSlotEvent",0,textureGeneration,1,4))return 78;
      MTLRenderPassDescriptor *resolvePass=[MTLRenderPassDescriptor renderPassDescriptor];
      resolvePass.colorAttachments[0].texture=drawable.texture;resolvePass.colorAttachments[0].loadAction=MTLLoadActionClear;
      resolvePass.colorAttachments[0].storeAction=deferredPass?MTLStoreActionUnknown:MTLStoreActionStore;
      if(depthPass) {
        resolvePass.depthAttachment.texture=depthTexture; resolvePass.depthAttachment.loadAction=MTLLoadActionLoad;
        resolvePass.depthAttachment.storeAction=deferredPass?MTLStoreActionUnknown:MTLStoreActionStore;
        if(stencilPass) { resolvePass.stencilAttachment.texture=depthTexture; resolvePass.stencilAttachment.loadAction=MTLLoadActionLoad;
          resolvePass.stencilAttachment.storeAction=deferredPass?MTLStoreActionUnknown:MTLStoreActionStore; }
      }
      if(counterPass) {
        resolvePass.sampleBufferAttachments[0].sampleBuffer=counter;
        resolvePass.sampleBufferAttachments[0].startOfVertexSampleIndex=2;
        resolvePass.sampleBufferAttachments[0].endOfFragmentSampleIndex=3;
      }
      id<MTLRenderCommandEncoder> resolve=[command renderCommandEncoderWithDescriptor:resolvePass];
      [resolve setRenderPipelineState:resolvePipeline];
      if(depthPass) { [resolve setDepthStencilState:resolveDepthState]; if(stencilPass)[resolve setStencilReferenceValue:8]; }
      id<MTLBuffer> resolveTables[]={bufferTable,resolveTable,samplerTable};
      uint64_t resolveRoot[6]={};for(unsigned i=0;i<3;i++){resolveRoot[i*2]=resolveTables[i].gpuAddress;resolveRoot[i*2+1]=0xabcdef0123456789ULL;}
      for(unsigned stage=1;stage<=2;stage++)
      {
        if(annotation(resolve,"metal.descriptorInlineLayout",stage,0,3,16)){[resolve endEncoding];return 26;}
        for(unsigned i=0;i<3;i++)
          if(annotation(resolve,"metal.descriptorInlineBinding",uint64_t(stage)<<32,i,(uint64_t)(__bridge void *)resolveTables[i],0)){[resolve endEncoding];return 27;}
      }
      [resolve setVertexBytes:resolveRoot length:sizeof(resolveRoot) atIndex:0];[resolve setFragmentBytes:resolveRoot length:sizeof(resolveRoot) atIndex:0];
      for(id<MTLBuffer> table:resolveTables)[resolve useResource:table usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
      [resolve useResource:input usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
      [resolve useResource:sampledIntermediate usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
      if(renderIndirect) {
        if(!indirectDraw(resolve,64) || (renderZero&&!indirectDraw(resolve,96))) { [resolve endEncoding];return 61; }
      }
      else [resolve drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
      if(deferredPass) {
        [resolve setColorStoreAction:MTLStoreActionStore atIndex:0];
        if(depthPass)[resolve setDepthStoreAction:MTLStoreActionStore];
        if(stencilPass)[resolve setStencilStoreAction:MTLStoreActionStore];
      }
      [resolve endEncoding];
      if(renderIndirect) { id<MTLBlitCommandEncoder> zero=[command blitCommandEncoder];
        if(getenv("RENDERDOC_METAL_MRT_BLIT_MARKERS")) {
          [zero pushDebugGroup:@"Sourced tail copy"];[zero pushDebugGroup:@"Nested tail copy"];
          [zero insertDebugSignpost:@"Tail source reset"]; }
        [zero copyFromBuffer:renderZeroUpload sourceOffset:0 toBuffer:renderArguments destinationOffset:0 size:sizeof(renderArgumentWords)];
        if(getenv("RENDERDOC_METAL_MRT_BLIT_MARKERS") && !getenv("RENDERDOC_METAL_MRT_BLIT_MARKERS_IMPLICIT")) { [zero popDebugGroup];[zero popDebugGroup]; }
        [zero endEncoding]; }
      [command presentDrawable:drawable];
      if(frameVisibility && getenv("RENDERDOC_METAL_MRT_VISIBILITY_EARLIER_SNAPSHOT"))
      {
        // The query pass is already encoded, but its CPU initialization is
        // captured with a separate submission which executes before it.
        [earlier commit];
      }
      [command commit];
      if(reuseGPUGeneration) {
        // Original Native work is complete, without a captured explicit wait.
        for(unsigned attempt=0;command.status<MTLCommandBufferStatusCompleted && attempt<5000;attempt++)usleep(1000);
        if(command.status!=MTLCommandBufferStatusCompleted)return 82;
      } else [command waitUntilCompleted];
      if(crossAlias)
      {
        const uint32_t *first=(const uint32_t *)firstAliasOutput.contents;
        if(first[0]!=(capture==0?122U:186U)||first[1]!=0xdeadbeefU)return 40;
        printf("CROSS first GPU buffer read PASS capture=%d value=%u\n",capture,first[0]);
      }
      if(visibilityOutput) {const uint64_t *v=(const uint64_t *)visibilityOutput.contents;fprintf(stderr,"Native visibility=%llu/%llu/%llu/%llu\n",(unsigned long long)v[0],(unsigned long long)v[1],(unsigned long long)v[2],(unsigned long long)v[3]);if(v[0]!=0||v[1]!=4||v[2]!=0x5678||v[3]!=0xdead)return 76;}
      const uint32_t *words=(const uint32_t *)output.contents;
      if(command.error || words[0]!=(capture==0?186U:122U) || words[1]!=0xdeadbeefU)return 9;
      uint8_t pixels[16]={};[drawable.texture getBytes:pixels bytesPerRow:8 fromRegion:MTLRegionMake2D(0,0,2,2) mipmapLevel:0];
      for(unsigned i=0;i<16;i+=4)if(pixels[i]!=128 || pixels[i+1]!=64 || pixels[i+2]!=(capture==0?186:122) || pixels[i+3]!=255)return 23;
      uint8_t mrtPixels[16]={};
      if(!frameIntermediate)
      {
        [intermediate getBytes:mrtPixels bytesPerRow:8 fromRegion:MTLRegionMake2D(0,0,2,2) mipmapLevel:0];
        for(unsigned i=0;i<16;i+=4)if(mrtPixels[i]!=(capture==0?186:122)||mrtPixels[i+1]!=64||mrtPixels[i+2]!=128||mrtPixels[i+3]!=255)return 28;
      }
      for(id<MTLTexture> extra:extraTargets)
      {
        [extra getBytes:mrtPixels bytesPerRow:8 fromRegion:MTLRegionMake2D(0,0,2,2) mipmapLevel:0];
        for(unsigned i=0;i<16;i+=4)if(mrtPixels[i]!=(capture==0?186:122)||mrtPixels[i+1]!=64||mrtPixels[i+2]!=128||mrtPixels[i+3]!=255)return 29;
      }
      if(fragmentlessColor) {
        [fragmentlessTarget getBytes:mrtPixels bytesPerRow:8 fromRegion:MTLRegionMake2D(0,0,2,2) mipmapLevel:0];
        for(unsigned i=0;i<16;i+=4)if(mrtPixels[i]!=80 || mrtPixels[i+1]!=64 || mrtPixels[i+2]!=128 || mrtPixels[i+3]!=255)return 95;
        printf("Native vertex-only colour retained PASS capture=%d\n",capture);
      }
      if(counterPass) {
        NSData *data=[counter resolveCounterRange:NSMakeRange(0,8)];
        if(data.length!=8*sizeof(uint64_t))return 49;
        const uint64_t *times=(const uint64_t *)data.bytes;
        for(unsigned i=0;i<8;i+=2)if(!times[i] || times[i+1]<times[i])return 50;
        printf("COUNTER native timestamp pairs PASS capture=%d render/compute=4\n",capture);
      }
      if(reuseGPUGeneration) {
        id<MTLCommandBuffer> borrowedTableCommand=nil;
        if(getenv("RENDERDOC_METAL_MRT_DEAD_SLOT_TABLE_BORROW")) {
          borrowedTableCommand=[queue commandBuffer];[borrowedTableCommand enqueue];
          auto empty=[borrowedTableCommand computeCommandEncoder];[empty setComputePipelineState:zeroArgumentPipeline];
          if(annotation(empty,"metal.descriptorInlineLayout",0,0,3,16))return 87;
          for(unsigned slot=0;slot<3;slot++)
            if(annotation(empty,"metal.descriptorInlineBinding",0,slot,(uint64_t)(__bridge void *)tables[slot],0))return 88;
          [empty setBytes:root length:sizeof(root) atIndex:0];[empty useResource:textureTable usage:MTLResourceUsageRead];
          [empty dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[empty endEncoding];
        }
        memcpy(textureTable.contents,payloadBytes,24);
        if(annotation(textureTable,"metal.descriptorSlotEvent",0,textureGeneration+1,0,4) ||
           annotation(textureTable,"metal.descriptorSlotEvent",0,textureGeneration+1,2,4) ||
           annotation(textureTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)nextImage,0))return 83;
        if(borrowedTableCommand)[borrowedTableCommand commit];
        command=[queue commandBuffer];[command enqueue];if(!consume())return 84;
        [command commit];[command waitUntilCompleted];if(command.error)return 85;
        if(annotation(textureTable,"metal.descriptorSlotEvent",0,textureGeneration+1,1,4))return 86;
      }
      if(freshCPUSlot && annotation(payload,"metal.descriptorSlotEvent",24,capture+2,1,4))return 65;
      if(api && !api->EndFrameCapture(nullptr,nullptr))return 10;
      if(mixedFreshCPUSlot && annotation(textureTable,"metal.descriptorSlotEvent",24,capture+2,1,4))return 69;
      if(frameDepth) { [depthTexture makeAliasable]; depthTexture=nil; }
      if(frameIntermediate)
      {
        [intermediate makeAliasable];intermediate=baseIntermediate;sampledIntermediate=baseIntermediate;
        memcpy(resolveTable.contents,resolveBytes,sizeof(resolveBytes));
        if(annotation(resolveTable,"metal.descriptorSlotEvent",0,1,2,4) ||
           annotation(resolveTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)baseIntermediate,0))return 34;
      }
    }
    if(frameIntermediate)printf("FRAME_TEX_0=%llu FRAME_TEX_1=%llu\n",(unsigned long long)frameTextureIDs[0],(unsigned long long)frameTextureIDs[1]);
    printf("MRT and cross-pass native PASS results=122/186/122 VA_A=%llu VA_B=%llu VA_TABLE=%llu TEX=%llu SAMP=%llu TEX_B=%llu captures=%d\n",(unsigned long long)bufferBytes[0],(unsigned long long)tableVAs[1],(unsigned long long)tableVAs[0],(unsigned long long)textureBytes[1],(unsigned long long)samplerBytes[0],(unsigned long long)otherID,api?2:0);
  }
  return 0;
}
