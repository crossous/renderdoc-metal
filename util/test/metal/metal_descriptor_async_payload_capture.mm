// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vector>
#include "renderdoc/api/app/renderdoc_app.h"
int main()
{
  @autoreleasepool
  {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice(); NSError *error=nil;
    id<MTLLibrary> library=[device newLibraryWithSource:@R"MSL(
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
  output[0]=output[0]+buffer->value[0]+uint(texture->image.sample(sampling->point,float2(0.5)).r*255.0+0.5)+17;
  output[1]=(buffer->metadata==0x1111111111111111ul && texture->metadata==0x2222222222222222ul &&
    sampling->bias==0x123456789abcdef0ul && sampling->metadata==0x3333333333333333ul &&
    root[1]==0xabcdef0123456789ul && root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul) ? 0xdeadbeefU : 0xbadU;
}
struct VSOut { float4 position [[position]]; float value; };
vertex VSOut resource_vertex(uint index [[vertex_id]], const device ulong *root [[buffer(0)]])
{
  const device BufferEntry *buffer=reinterpret_cast<const device BufferEntry *>(root[0]);
  float2 positions[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
  VSOut out;out.position=float4(positions[index],0,1);
  out.value=(buffer->metadata==0x1111111111111111ul && root[1]==0xabcdef0123456789ul) ? float(buffer->value[0]) : 0.0;
  return out;
}
fragment float4 resource_fragment(VSOut in [[stage_in]], const device ulong *root [[buffer(0)]])
{
  const device TextureEntry *texture=reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling=reinterpret_cast<const device SamplerEntry *>(root[4]);
  const bool ordinary=texture->metadata==0x2222222222222222ul && sampling->bias==0x123456789abcdef0ul &&
      sampling->metadata==0x3333333333333333ul && root[1]==0xabcdef0123456789ul &&
      root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul;
  return ordinary ? float4(texture->image.sample(sampling->point,float2(0.5)).r+(in.value+17.0)/255.0,64.0/255.0,128.0/255.0,1) : float4(1,0,1,1);
}
struct CopyRoot { device const ulong *source [[id(0)]]; device ulong *destination [[id(1)]]; };
kernel void copy_resource(const device CopyRoot &root [[buffer(0)]], device uint *output [[buffer(1)]])
{ for(uint i=0;i<3;i++)root.destination[i]=root.source[i]; output[1]=0xdeadbeefU; }
)MSL" options:nil error:&error];
    if(!library) { fprintf(stderr,"%s\n",error.localizedDescription.UTF8String); return 2; }
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"resources"] error:&error];
    id<MTLComputePipelineState> copyPipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"copy_resource"] error:&error];
    MTLRenderPipelineDescriptor *renderDescriptor=[MTLRenderPipelineDescriptor new];
    renderDescriptor.vertexFunction=[library newFunctionWithName:@"resource_vertex"];
    renderDescriptor.fragmentFunction=[library newFunctionWithName:@"resource_fragment"];
    renderDescriptor.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;
    id<MTLRenderPipelineState> renderPipeline=[device newRenderPipelineStateWithDescriptor:renderDescriptor error:&error];
    if(!renderPipeline){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 20;}
    const uint32_t inputValues[]={0xbadU,41U,80U};
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
    id<MTLBuffer> textureTable=[device newBufferWithBytes:textureBytes length:24 options:MTLResourceStorageModeShared];
    id<MTLBuffer> samplerTable=[device newBufferWithBytes:samplerBytes length:24 options:MTLResourceStorageModeShared];
    uint64_t payloadBytes[]={0,otherID,0x2222222222222222ULL};
    id<MTLBuffer> payload=[device newBufferWithBytes:payloadBytes length:24 options:MTLResourceStorageModeShared];
    textureTable.label=@"Resource texture destination";payload.label=@"Resource texture payload";
    id<MTLBuffer> output=[device newBufferWithLength:8 options:MTLResourceStorageModeShared];
    const uint64_t tableVAs[]={bufferTable.gpuAddress,textureTable.gpuAddress,samplerTable.gpuAddress};
    if(!pipeline || !input || !image || !point || !output) return 3;
    const bool signals=getenv("RENDERDOC_METAL_SIGNALS_ONLY")!=nullptr;
    const bool sharedSignals=getenv("RENDERDOC_METAL_SHARED_SIGNALS")!=nullptr;
    const bool interleaved=getenv("RENDERDOC_METAL_INTERLEAVED_SUBMISSIONS")!=nullptr;
    const bool retirement=getenv("RENDERDOC_METAL_PRELUDE_RETIREMENT")!=nullptr;
    const bool retirementGPU=getenv("RENDERDOC_METAL_PRELUDE_GPU_WRITES")!=nullptr;
    const bool mixedRetirement=getenv("RENDERDOC_METAL_MIXED_PRELUDE_RETIREMENT")!=nullptr;
    const bool blitRetirement=getenv("RENDERDOC_METAL_BLIT_PRELUDE_RETIREMENT")!=nullptr;
    const bool textureRetirement=getenv("RENDERDOC_METAL_TEXTURE_PRELUDE_RETIREMENT")!=nullptr;
    const unsigned retirementSlots=getenv("RENDERDOC_METAL_MANY_PRELUDE_RETIREMENTS")?80:2;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path);
      RENDERDOC_AnnotationValue coverage={}; coverage.uint32=textureRetirement?31:blitRetirement?30:mixedRetirement?29:signals?19:(retirement?16:(interleaved?15:12));
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
      if(annotation(tables[i],"metal.descriptorTable",i==2?2:1,0,1,24) ||
         annotation(tables[i],"metal.descriptorSlotEvent",0,1,0,types[i]) ||
         annotation(tables[i],"metal.descriptorSlotEvent",0,1,2,types[i]) ||
         annotation(tables[i],"metal.descriptorSlotBinding",0,kinds[i],(uint64_t)(__bridge void *)sources[i],i==0?4:0)) return 6;
    if(annotation(payload,"metal.descriptorTable",1,0,1,24) ||
       annotation(payload,"metal.descriptorSlotEvent",0,1,0,4)) return 11;
    if(api)
    {
      RENDERDOC_AnnotationValue gpu={};gpu.uint32=1;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)textureTable,"metal.descriptorGPUWrites",eRENDERDOC_UInt32,0,&gpu))return 12;
    }
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    MTLHeapDescriptor *heapDescriptor=[MTLHeapDescriptor new];heapDescriptor.type=MTLHeapTypePlacement;
    heapDescriptor.storageMode=MTLStorageModeShared;heapDescriptor.hazardTrackingMode=MTLHazardTrackingModeTracked;heapDescriptor.size=65536;
    id<MTLHeap> payloadHeap=[device newHeapWithDescriptor:heapDescriptor];
    const MTLResourceOptions payloadOptions=MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked;
    const MTLSizeAndAlign payloadLayout=[device heapBufferSizeAndAlignWithLength:24 options:payloadOptions];
    if(!payloadHeap || !payloadLayout.align || !payloadLayout.size)return 26;
    MTLHeapDescriptor *textureHeapDescriptor=[MTLHeapDescriptor new];textureHeapDescriptor.type=MTLHeapTypePlacement;
    textureHeapDescriptor.storageMode=MTLStorageModePrivate;textureHeapDescriptor.hazardTrackingMode=MTLHazardTrackingModeTracked;textureHeapDescriptor.size=65536;
    id<MTLHeap> texturePreludeHeap=textureRetirement?[device newHeapWithDescriptor:textureHeapDescriptor]:nil;
    if(textureRetirement&&!texturePreludeHeap)return 36;
    id<MTLCommandQueue> queue=[device newCommandQueue];
    const bool unretained=getenv("RENDERDOC_METAL_UNRETAINED_SUBMISSIONS")!=nullptr;
    MTLCommandBufferDescriptor *submissionDescriptor=[MTLCommandBufferDescriptor new];
    submissionDescriptor.retainedReferences=NO;submissionDescriptor.errorOptions=MTLCommandBufferErrorOptionEncoderExecutionStatus;

    for(int capture=0;capture<(api?2:1);capture++)
    {
      id<MTLTexture> nextImage=capture==0?image:other;
      payloadBytes[1]=capture==0?textureBytes[1]:otherID;
      memcpy(payload.contents,payloadBytes,24);
      if(annotation(payload,"metal.descriptorSlotEvent",0,1,2,4) ||
         annotation(payload,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)nextImage,0))return 13;
      memset(output.contents,0,8);
      id<MTLBuffer> retiredTable=nil;
      if(retirement)
      {
        id<MTLBuffer> expiredBuffer=[device newBufferWithBytes:inputValues length:12 options:MTLResourceStorageModeShared];
        id<MTLTexture> expiredTexture=[device newTextureWithDescriptor:descriptor];
        std::vector<uint64_t> retiredBytes(retirementSlots*3);
        for(unsigned i=0;i<retirementSlots;i++)
        {retiredBytes[i*3]=i==1?0:expiredBuffer.gpuAddress+4;retiredBytes[i*3+1]=i==1?expiredTexture.gpuResourceID._impl:0;retiredBytes[i*3+2]=i==1?0x7070707070707070ULL:0x6060606060606060ULL;}
        retiredTable=[device newBufferWithBytes:retiredBytes.data() length:retirementSlots*24 options:MTLResourceStorageModeShared];
        retiredTable.label=@"Prelude retired table";
        if(annotation(retiredTable,"metal.descriptorTable",1,0,retirementSlots,24) ||
           annotation(retiredTable,"metal.descriptorSlotEvent",0,1,0,6) ||
           annotation(retiredTable,"metal.descriptorSlotEvent",0,1,2,6) ||
           annotation(retiredTable,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)expiredBuffer,4) ||
           annotation(retiredTable,"metal.descriptorSlotEvent",24,1,0,4) ||
           annotation(retiredTable,"metal.descriptorSlotEvent",24,1,2,4) ||
           annotation(retiredTable,"metal.descriptorSlotBinding",24,1,(uint64_t)(__bridge void *)expiredTexture,0))return 28;
        for(unsigned i=2;i<retirementSlots;i++)
          if(annotation(retiredTable,"metal.descriptorSlotEvent",i*24,1,0,6)||
             annotation(retiredTable,"metal.descriptorSlotEvent",i*24,1,2,6)||
             annotation(retiredTable,"metal.descriptorSlotBinding",i*24,0,(uint64_t)(__bridge void *)expiredBuffer,4))return 28;
        if(api && retirementGPU)
        {
          RENDERDOC_AnnotationValue gpu={};gpu.uint32=1;
          if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)retiredTable,"metal.descriptorGPUWrites",eRENDERDOC_UInt32,0,&gpu))return 29;
        }
        expiredBuffer=nil; expiredTexture=nil;
      }
      id<MTLEvent> signalEvent=signals?(sharedSignals?[device newSharedEvent]:[device newEvent]):nil;
      if(signals && !signalEvent)return 31;
      const uint64_t signalBase=sharedSignals?100:0;
      if(sharedSignals)[(id<MTLSharedEvent>)signalEvent setSignaledValue:signalBase];
      if(api) api->StartFrameCapture(nullptr,nullptr);
      id<MTLBuffer> mixedBirth=nil;
      id<MTLBuffer> preludeCopyDestination=nil;
      id<MTLCommandBuffer> preludeCopy=nil;
      id<MTLTexture> preludeView=nil;id<MTLBuffer> preludeViewTable=nil;
      if(retirement)
      {
        if(annotation(retiredTable,"metal.descriptorSlotEvent",0,1,1,6))return 30;
        if(mixedRetirement)
        {
          mixedBirth=[device newBufferWithLength:16 options:MTLResourceStorageModeShared];
          if(!mixedBirth)return 33;
          memset(mixedBirth.contents,0,16);
          (void)mixedBirth.gpuAddress;
        }
        if(blitRetirement)
        {
          const uint8_t values[]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
          id<MTLBuffer> source=[device newBufferWithBytes:values length:16 options:MTLResourceStorageModeShared];
          const NSUInteger row=textureRetirement?[device minimumLinearTextureAlignmentForPixelFormat:MTLPixelFormatRGBA8Unorm]:16;
          preludeCopyDestination=[device newBufferWithLength:MAX(row,16) options:MTLResourceStorageModeShared];
          memset(preludeCopyDestination.contents,0,MAX(row,16));
          if(textureRetirement)
          {
            MTLTextureDescriptor *viewDesc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
            viewDesc.storageMode=MTLStorageModeShared;viewDesc.usage=MTLTextureUsageShaderRead;
            preludeView=[preludeCopyDestination newTextureWithDescriptor:viewDesc offset:0 bytesPerRow:row];
            if(!preludeView)return 37;
            const uint64_t words[]={0,preludeView.gpuResourceID._impl,0x4141414141414141ULL};
            preludeViewTable=[device newBufferWithBytes:words length:24 options:MTLResourceStorageModeShared];
            if(annotation(preludeViewTable,"metal.descriptorTable",1,0,1,24)||
               annotation(preludeViewTable,"metal.descriptorSlotEvent",0,1,0,4)||
               annotation(preludeViewTable,"metal.descriptorSlotEvent",0,1,2,4)||
               annotation(preludeViewTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)preludeView,0))return 38;
            MTLTextureDescriptor *heapDesc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:2 height:2 mipmapped:NO];
            heapDesc.resourceOptions=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;
            heapDesc.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
            id<MTLTexture> heapTexture=[texturePreludeHeap newTextureWithDescriptor:heapDesc offset:capture*32768];
            if(!heapTexture)return 39;(void)heapTexture.gpuResourceID;
          }
          preludeCopy=[queue commandBuffer];id<MTLBlitCommandEncoder> blit=[preludeCopy blitCommandEncoder];
          [blit copyFromBuffer:source sourceOffset:0 toBuffer:preludeCopyDestination destinationOffset:0 size:16];
          [blit endEncoding];
        }
        if(annotation(retiredTable,"metal.descriptorSlotEvent",24,1,1,4))return 30;
        for(unsigned i=2;i<retirementSlots;i++)if(annotation(retiredTable,"metal.descriptorSlotEvent",i*24,1,1,6))return 30;
        if(preludeCopy)
        {
          [preludeCopy commit];[preludeCopy waitUntilCompleted];
          if(preludeCopy.status!=MTLCommandBufferStatusCompleted)return 34;
          for(unsigned i=0;i<16;i++)if(((const uint8_t *)preludeCopyDestination.contents)[i]!=i+1)return 35;
          if(preludeView){uint8_t bytes[4]={};[preludeView getBytes:bytes bytesPerRow:4 fromRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0];for(unsigned i=0;i<4;i++)if(bytes[i]!=i+1)return 40;}
        }
      }
      uint64_t root[6]={};for(unsigned i=0;i<3;i++){root[i*2]=tableVAs[i];root[i*2+1]=0xabcdef0123456789ULL;}
      id<MTLBuffer> activePayload=payload;
      // Create the later work submission first, without reserving native queue order.
      // An empty submission is also born before the first GPU work is committed.
      id<MTLCommandBuffer> later=interleaved?(unretained?[queue commandBufferWithUnretainedReferences]:[queue commandBuffer]):nil;
      NSMutableArray<id<MTLCommandBuffer>> *emptyCommands=[NSMutableArray new];
      if(interleaved)
        for(unsigned i=0;i<9;i++) [emptyCommands addObject:[queue commandBuffer]];
      id<MTLCommandBuffer> command=unretained?[queue commandBufferWithDescriptor:submissionDescriptor]:[queue commandBuffer];[command enqueue];
      auto consume=[&]() {
        id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];[compute setComputePipelineState:pipeline];
        if(annotation(compute,"metal.descriptorInlineLayout",0,0,3,16))return false;
        for(unsigned i=0;i<3;i++)
        {
          if(annotation(compute,"metal.descriptorInlineBinding",0,i,(uint64_t)(__bridge void *)tables[i],0))return false;
          [compute useResource:tables[i] usage:MTLResourceUsageRead];
        }
        [compute setBytes:root length:sizeof(root) atIndex:0];[compute setBuffer:output offset:0 atIndex:1];
        [compute useResource:input usage:MTLResourceUsageRead];[compute useResource:image usage:MTLResourceUsageRead];
        [compute useResource:other usage:MTLResourceUsageRead];
        if(preludeViewTable){[compute useResource:preludeViewTable usage:MTLResourceUsageRead];[compute useResource:preludeView usage:MTLResourceUsageRead];}
        [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];return true;
      };
      if(!consume())return 14;
      if(signals)[command encodeSignalEvent:signalEvent value:signalBase+1];
      [command commit];
      for(id<MTLCommandBuffer> empty in emptyCommands) [empty commit];
      // Match the real UE frame: 75 births overall, peak 11 outstanding creations.
      // Holding all 75 before committing exceeds Metal's default queue reservation pool.
      if(interleaved)
        for(unsigned i=0;i<64;i++) [[queue commandBuffer] commit];
      if(command.error)return 24;
      nextImage=capture==0?other:image;
      payloadBytes[1]=capture==0?otherID:textureBytes[1];
      const NSUInteger payloadOffset=capture*((payloadLayout.size+payloadLayout.align-1)/payloadLayout.align*payloadLayout.align);
      activePayload=[payloadHeap newBufferWithLength:24 options:payloadOptions offset:payloadOffset];
      if(!activePayload)return 27;memcpy(activePayload.contents,payloadBytes,24);
      if(annotation(activePayload,"metal.descriptorTable",1,0,1,24) ||
         annotation(activePayload,"metal.descriptorSlotEvent",0,1,0,4) ||
         annotation(activePayload,"metal.descriptorSlotEvent",0,1,2,4) ||
         annotation(activePayload,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)nextImage,0))return 25;
      command=interleaved?later:(unretained?[queue commandBufferWithUnretainedReferences]:[queue commandBuffer]);[command enqueue];

      id<MTLComputeCommandEncoder> producer=[command computeCommandEncoder];[producer setComputePipelineState:copyPipeline];
      const uint64_t copyRoot[]={activePayload.gpuAddress,textureTable.gpuAddress};
      if(annotation(producer,"metal.descriptorInlineLayout",0,0,2,8) ||
         annotation(producer,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void *)activePayload,0) ||
         annotation(producer,"metal.descriptorInlineBinding",0,1,(uint64_t)(__bridge void *)textureTable,0))return 15;
      [producer setBytes:copyRoot length:16 atIndex:0];[producer setBuffer:output offset:0 atIndex:1];[producer useResource:activePayload usage:MTLResourceUsageRead];
      [producer useResource:textureTable usage:MTLResourceUsageWrite];
      [producer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      if(annotation(textureTable,"metal.descriptorSlotProducer",0,(uint64_t)(__bridge void *)producer,(uint64_t)(__bridge void *)activePayload,0))return 16;
      if(api)
      {
        RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=0;memcpy(&value.vector.uint64[1],payloadBytes,24);
        if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)textureTable,"metal.descriptorSlotGPUValue",eRENDERDOC_UInt64,4,&value))return 17;
      }
      if(annotation(textureTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)nextImage,0))return 18;
      [producer endEncoding];if(!consume())return 19;
      id<CAMetalDrawable> drawable=[layer nextDrawable];MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=drawable.texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
      pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(0.125,0.25,0.5,1);
      id<MTLRenderCommandEncoder> render=[command renderCommandEncoderWithDescriptor:pass];
      [render setRenderPipelineState:renderPipeline];
      for(unsigned stage=1;stage<=2;stage++)
      {
        if(annotation(render,"metal.descriptorInlineLayout",stage,0,3,16)){[render endEncoding];return 21;}
        for(unsigned i=0;i<3;i++)
          if(annotation(render,"metal.descriptorInlineBinding",uint64_t(stage)<<32,i,(uint64_t)(__bridge void *)tables[i],0)){[render endEncoding];return 22;}
      }
      [render setVertexBytes:root length:sizeof(root) atIndex:0];
      [render setFragmentBytes:root length:sizeof(root) atIndex:0];
      for(id<MTLBuffer> table:tables)[render useResource:table usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
      [render useResource:input usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
      [render useResource:image usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
      [render useResource:other usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
      [render drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[render endEncoding];
      if(signals)[command encodeSignalEvent:signalEvent value:signalBase+2];
      [command presentDrawable:drawable];[command commit];
      if(api && !api->EndFrameCapture(nullptr,nullptr))return 10;
      [command waitUntilCompleted];
      if(sharedSignals && ((id<MTLSharedEvent>)signalEvent).signaledValue!=signalBase+2)return 32;
      const uint32_t *words=(const uint32_t *)output.contents;
      if(command.error || words[0]!=308U || words[1]!=0xdeadbeefU)return 9;
      uint8_t pixels[16]={};[drawable.texture getBytes:pixels bytesPerRow:8 fromRegion:MTLRegionMake2D(0,0,2,2) mipmapLevel:0];
      for(unsigned i=0;i<16;i+=4)if(pixels[i]!=128 || pixels[i+1]!=64 || pixels[i+2]!=(capture==0?186:122) || pixels[i+3]!=255)return 23;

    }
    printf("%s same-queue heap payload native PASS results=122+186/186+122=308 VA_A=%llu VA_B=%llu VA_TABLE=%llu TEX=%llu SAMP=%llu TEX_B=%llu captures=%d\n",interleaved?"interleaved":"async",(unsigned long long)bufferBytes[0],(unsigned long long)tableVAs[1],(unsigned long long)tableVAs[0],(unsigned long long)textureBytes[1],(unsigned long long)samplerBytes[0],(unsigned long long)otherID,api?2:0);
  }
  return 0;
}
