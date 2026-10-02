// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main()
{
  @autoreleasepool
  {
    const bool tableAlias=getenv("RENDERDOC_METAL_DESCRIPTOR_BACKING_ALIAS")!=nullptr;
    const bool largeAlias=getenv("RENDERDOC_METAL_LARGE_BUFFER_ALIAS")!=nullptr;
    const NSUInteger originalLength=largeAlias && getenv("RENDERDOC_METAL_LARGE_ALIAS_BOTH")?131072:12;
    const bool asyncAlias=getenv("RENDERDOC_METAL_ASYNC_BUFFER_ALIAS")!=nullptr;
    const bool privateAlias=getenv("RENDERDOC_METAL_PRIVATE_BUFFER_ALIAS")!=nullptr;
    const bool delayedAlias=getenv("RENDERDOC_METAL_ALIAS_BEFORE_COMMIT")!=nullptr;
    const bool cpuAlias=getenv("RENDERDOC_METAL_ALIAS_CPU_BEFORE_COMMIT")!=nullptr;
    if(tableAlias && (!asyncAlias || privateAlias || delayedAlias || cpuAlias))return 45;
    if(cpuAlias && (!delayedAlias || privateAlias))return 43;
    if(delayedAlias && !asyncAlias)return 42;
    if(privateAlias && !asyncAlias)return 37;
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
  output[0]=buffer->value[0]+uint(texture->image.sample(sampling->point,float2(0.5)).r*255.0+0.5)+17;
  output[1]=(buffer->metadata==0x1111111111111111ul && texture->metadata==0x2222222222222222ul &&
    sampling->bias==0x123456789abcdef0ul && sampling->metadata==0x3333333333333333ul &&
    root[1]==0xabcdef0123456789ul && root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul) ? 0xdeadbeefU : 0xbadU;
}
kernel void resources_saved(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]])
{
  const device BufferEntry *buffer=reinterpret_cast<const device BufferEntry *>(root[0]);
  const device TextureEntry *texture=reinterpret_cast<const device TextureEntry *>(root[2]);
  const device SamplerEntry *sampling=reinterpret_cast<const device SamplerEntry *>(root[4]);
  output[0]=buffer->value[0]+uint(texture->image.sample(sampling->point,float2(0.5)).r*255.0+0.5)+17;
  output[1]=(buffer->metadata==0x1111111111111111ul && texture->metadata==0x2222222222222222ul &&
    sampling->bias==0x123456789abcdef0ul && sampling->metadata==0x3333333333333333ul &&
    root[1]==0xabcdef0123456789ul && root[3]==0xabcdef0123456789ul && root[5]==0xabcdef0123456789ul) ? 0xdeadbeefU : 0xbadU;
  if(!output[3]){output[2]=output[0];output[3]=1;}
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
kernel void initialize_private(const device ulong *root [[buffer(0)]], device uint *output [[buffer(1)]],
                               device uint *target [[buffer(2)]], const device uint *seed [[buffer(3)]])
{ for(uint i=0;i<3;i++)target[i]=seed[i]; output[1]=root[1]==0xabcdef0123456789ul?0xdeadbeefU:0xbadU; }
kernel void copy_resource(const device CopyRoot &root [[buffer(0)]], device uint *output [[buffer(1)]])
{ for(uint i=0;i<3;i++)root.destination[i]=root.source[i]; output[1]=0xdeadbeefU; }
kernel void copy_and_alias_write(const device CopyRoot &root [[buffer(0)]], device uint *output [[buffer(1)]],
                                device uint *alias [[buffer(2)]])
{ for(uint i=0;i<3;i++)root.destination[i]=root.source[i]; alias[1]=80; output[1]=0xdeadbeefU; }
kernel void copy_and_table_alias_write(const device CopyRoot &root [[buffer(0)]], device uint *output [[buffer(1)]],
                                device uint *alias [[buffer(2)]])
{ for(uint i=0;i<3;i++)root.destination[i]=root.source[i]; alias[1]=80; alias[2]=80; output[1]=0xdeadbeefU; }
)MSL" options:nil error:&error];
    if(!library) { fprintf(stderr,"%s\n",error.localizedDescription.UTF8String); return 2; }
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:cpuAlias?@"resources_saved":@"resources"] error:&error];
    id<MTLComputePipelineState> copyPipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:tableAlias?@"copy_and_table_alias_write":(asyncAlias?@"copy_and_alias_write":@"copy_resource")] error:&error];
    id<MTLComputePipelineState> initialPipeline=privateAlias?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"initialize_private"] error:&error]:nil;
    if(privateAlias && !initialPipeline)return 38;
    MTLRenderPipelineDescriptor *renderDescriptor=[MTLRenderPipelineDescriptor new];
    renderDescriptor.vertexFunction=[library newFunctionWithName:@"resource_vertex"];
    renderDescriptor.fragmentFunction=[library newFunctionWithName:@"resource_fragment"];
    renderDescriptor.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;
    id<MTLRenderPipelineState> renderPipeline=[device newRenderPipelineStateWithDescriptor:renderDescriptor error:&error];
    if(!renderPipeline){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 20;}
    const uint32_t inputValues[]={0xbadU,41U,tableAlias?41U:80U};
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
    MTLHeapDescriptor *tableHeapDescriptor=[MTLHeapDescriptor new];tableHeapDescriptor.type=MTLHeapTypePlacement;
    tableHeapDescriptor.storageMode=MTLStorageModeShared;tableHeapDescriptor.hazardTrackingMode=MTLHazardTrackingModeTracked;tableHeapDescriptor.size=65536;
    id<MTLHeap> tableHeap=tableAlias?[device newHeapWithDescriptor:tableHeapDescriptor]:nil;
    id<MTLBuffer> bufferTable=tableAlias?[tableHeap newBufferWithLength:24 options:MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked offset:0]:[device newBufferWithBytes:bufferBytes length:24 options:MTLResourceStorageModeShared];
    if(tableAlias)memcpy(bufferTable.contents,bufferBytes,24);
    id<MTLBuffer> baseTable=bufferTable;uint64_t tableGeneration=1;
    id<MTLBuffer> textureTable=[device newBufferWithBytes:textureBytes length:24 options:MTLResourceStorageModeShared];
    id<MTLBuffer> samplerTable=[device newBufferWithBytes:samplerBytes length:24 options:MTLResourceStorageModeShared];
    uint64_t payloadBytes[]={0,otherID,0x2222222222222222ULL};
    id<MTLBuffer> payload=[device newBufferWithBytes:payloadBytes length:24 options:MTLResourceStorageModeShared];
    textureTable.label=@"Resource texture destination";payload.label=@"Resource texture payload";
    id<MTLBuffer> output=[device newBufferWithLength:cpuAlias?16:8 options:MTLResourceStorageModeShared];
    uint64_t tableVAs[]={bufferTable.gpuAddress,textureTable.gpuAddress,samplerTable.gpuAddress};
    if(!pipeline || !input || !image || !point || !output) return 3;
    const bool implicitAlias=getenv("RENDERDOC_METAL_IMPLICIT_BUFFER_ALIAS")!=nullptr;
    const bool backgroundAlias=getenv("RENDERDOC_METAL_BACKGROUND_BUFFER_ALIAS")!=nullptr;
    const NSUInteger replacementLength=getenv("RENDERDOC_METAL_IMPLICIT_ALIAS_LENGTH")?strtoul(getenv("RENDERDOC_METAL_IMPLICIT_ALIAS_LENGTH"),nullptr,10):12;
    if(replacementLength<12 || replacementLength>(largeAlias?131072U:65536U) || (backgroundAlias&&!implicitAlias) || (largeAlias && (!implicitAlias || !asyncAlias || tableAlias)))return 34;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path);
      RENDERDOC_AnnotationValue coverage={}; coverage.uint32=largeAlias?42:tableAlias?25:(delayedAlias?24:(privateAlias?23:(asyncAlias?22:(backgroundAlias?18:(implicitAlias?17:13)))));
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
    heapDescriptor.storageMode=privateAlias?MTLStorageModePrivate:MTLStorageModeShared;heapDescriptor.hazardTrackingMode=MTLHazardTrackingModeTracked;heapDescriptor.size=largeAlias?262144:65536;
    id<MTLHeap> inputHeap=[device newHeapWithDescriptor:heapDescriptor];
    const MTLResourceOptions inputOptions=(privateAlias?MTLResourceStorageModePrivate:MTLResourceStorageModeShared)|MTLResourceHazardTrackingModeTracked;
    if(!inputHeap)return 26;
    uint64_t generation=1;
    id<MTLCommandQueue> queue=[device newCommandQueue];
    const bool unretained=getenv("RENDERDOC_METAL_UNRETAINED_SUBMISSIONS")!=nullptr;
    MTLCommandBufferDescriptor *submissionDescriptor=[MTLCommandBufferDescriptor new];
    submissionDescriptor.retainedReferences=NO;submissionDescriptor.errorOptions=MTLCommandBufferErrorOptionEncoderExecutionStatus;

    for(int capture=0;capture<(api?2:1);capture++)
    {
      if(cpuAlias)memset(output.contents,0,16);
      id<MTLTexture> nextImage=capture==0?image:other;
      payloadBytes[1]=capture==0?textureBytes[1]:otherID;
      memcpy(payload.contents,payloadBytes,24);
      if(annotation(payload,"metal.descriptorSlotEvent",0,1,2,4) ||
         annotation(payload,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)nextImage,0))return 13;
      id<MTLBuffer> activeInput=nil;
      if(backgroundAlias)
      {
        activeInput=[inputHeap newBufferWithLength:originalLength options:inputOptions offset:0];
        if(!activeInput)return 35;
        if(privateAlias)
        {
          // Background Private bytes use the existing captured initial-content upload path.
          id<MTLCommandBuffer> initialize=[queue commandBuffer];id<MTLBlitCommandEncoder> copy=[initialize blitCommandEncoder];
          [copy copyFromBuffer:input sourceOffset:0 toBuffer:activeInput destinationOffset:0 size:12];[copy endEncoding];
          [initialize commit];[initialize waitUntilCompleted];if(initialize.error)return 39;
        }
        else memcpy(activeInput.contents,inputValues,12);
        const uint64_t packet[]={activeInput.gpuAddress+4,0,0x1111111111111111ULL};
        memcpy(bufferTable.contents,packet,24);
        if(annotation(bufferTable,"metal.descriptorSlotEvent",0,generation,2,0) ||
           annotation(bufferTable,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)activeInput,4))return 36;
      }
      if(api) api->StartFrameCapture(nullptr,nullptr);
      if(!backgroundAlias)activeInput=[inputHeap newBufferWithLength:originalLength options:inputOptions offset:0];
      if(!activeInput)return 27;fprintf(stderr,"A aliasable at creation=%d\n",int(activeInput.isAliasable));
      if(!privateAlias)memcpy(activeInput.contents,inputValues,12);
      uint64_t inputPacket[]={activeInput.gpuAddress+4,0,0x1111111111111111ULL};
      memcpy(bufferTable.contents,inputPacket,24);
      if(annotation(bufferTable,"metal.descriptorSlotEvent",0,generation,2,0) ||
         annotation(bufferTable,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)activeInput,4))return 28;

      uint64_t root[6]={};for(unsigned i=0;i<3;i++){root[i*2]=tableVAs[i];root[i*2+1]=0xabcdef0123456789ULL;}
      id<MTLCommandBuffer> command=unretained?[queue commandBufferWithDescriptor:submissionDescriptor]:[queue commandBuffer];[command enqueue];
      if(privateAlias && !backgroundAlias)
      {
        // Frame Private allocation is initialized on the GPU before its first descriptor read.
        id<MTLComputeCommandEncoder> initialize=[command computeCommandEncoder];[initialize setComputePipelineState:initialPipeline];
        if(annotation(initialize,"metal.descriptorInlineLayout",0,0,3,16))return 40;
        for(unsigned i=0;i<3;i++)
        {
          if(annotation(initialize,"metal.descriptorInlineBinding",0,i,(uint64_t)(__bridge void *)tables[i],0))return 41;
          [initialize useResource:tables[i] usage:MTLResourceUsageRead];
        }
        [initialize setBytes:root length:sizeof(root) atIndex:0];[initialize setBuffer:output offset:0 atIndex:1];
        [initialize setBuffer:activeInput offset:0 atIndex:2];[initialize setBuffer:input offset:0 atIndex:3];
        [initialize dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[initialize endEncoding];
      }
      auto consume=[&]() {
        id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];[compute setComputePipelineState:pipeline];
        if(annotation(compute,"metal.descriptorInlineLayout",0,0,3,16))return false;
        for(unsigned i=0;i<3;i++)
        {
          if(annotation(compute,"metal.descriptorInlineBinding",0,i,(uint64_t)(__bridge void *)tables[i],0))return false;
          [compute useResource:tables[i] usage:MTLResourceUsageRead];
        }
        [compute setBytes:root length:sizeof(root) atIndex:0];[compute setBuffer:output offset:0 atIndex:1];
        if(asyncAlias)
        {
          id<MTLHeap> __unsafe_unretained heaps[]={inputHeap,tableHeap};[compute useHeaps:heaps count:tableAlias?2:1];
        }
        else [compute useResource:activeInput usage:MTLResourceUsageRead];
        [compute useResource:image usage:MTLResourceUsageRead];
        [compute useResource:other usage:MTLResourceUsageRead];
        [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];return true;
      };
      if(!consume())return 14;
      id<MTLCommandBuffer> firstCommand=command;
      if(!delayedAlias){[command commit];if(!asyncAlias)[command waitUntilCompleted];}
      if(command.error)return 24;
      const uint64_t aliasVA=activeInput.gpuAddress;
      id<MTLBuffer> originalInput=activeInput;
      if(!implicitAlias)
      {
        if(annotation(bufferTable,"metal.descriptorSlotEvent",0,generation,1,0))return 29;
        [activeInput makeAliasable];fprintf(stderr,"A aliasable after retire=%d\n",int(activeInput.isAliasable));
      }
      id<MTLBuffer> replacementInput=[inputHeap newBufferWithLength:replacementLength options:inputOptions offset:0];
      activeInput=replacementInput;
      const uint32_t replacement[]={0xbadU,80U,41U};
      if(!activeInput || activeInput.gpuAddress!=aliasVA)return 30;
      if(!asyncAlias)
      {
        memset(activeInput.contents,0x5c,replacementLength);
        memcpy(activeInput.contents,replacement,12);
      }
      if(cpuAlias)((uint32_t *)activeInput.contents)[1]=103;
      inputPacket[0]=activeInput.gpuAddress+4;
      memcpy(bufferTable.contents,inputPacket,24);generation++;
      if(implicitAlias)
      {
        // Write through B, consume through still-live A and its unchanged descriptor.
        activeInput=originalInput;generation--;
        if((!asyncAlias && ((uint32_t *)activeInput.contents)[1]!=80U) || activeInput.gpuAddress!=replacementInput.gpuAddress)return 33;
      }
      else if(annotation(bufferTable,"metal.descriptorSlotEvent",0,generation,0,0) ||
         annotation(bufferTable,"metal.descriptorSlotEvent",0,generation,2,0) ||
         annotation(bufferTable,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)activeInput,4))return 31;

      if(tableAlias)
      {
        if(annotation(baseTable,"metal.descriptorSlotEvent",0,tableGeneration,1,0))return 46;
        bufferTable=[tableHeap newBufferWithLength:24 options:MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked offset:0];
        if(!bufferTable || bufferTable.gpuAddress!=baseTable.gpuAddress)return 47;
        const uint64_t replacementPacket[]={activeInput.gpuAddress+8,0,0x1111111111111111ULL};
        memcpy(bufferTable.contents,replacementPacket,24);
        if(annotation(bufferTable,"metal.descriptorTable",1,0,1,24) ||
           annotation(bufferTable,"metal.descriptorSlotEvent",0,1,0,0) ||
           annotation(bufferTable,"metal.descriptorSlotEvent",0,1,2,0) ||
           annotation(bufferTable,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)activeInput,8))return 48;
        tables[0]=bufferTable;tableVAs[0]=bufferTable.gpuAddress;root[0]=tableVAs[0];
      }
      if(delayedAlias)[firstCommand commit];
      nextImage=capture==0?other:image;
      payloadBytes[1]=capture==0?otherID:textureBytes[1];memcpy(payload.contents,payloadBytes,24);
      if(annotation(payload,"metal.descriptorSlotEvent",0,1,2,4) ||
         annotation(payload,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)nextImage,0))return 25;
      command=unretained?[queue commandBufferWithUnretainedReferences]:[queue commandBuffer];[command enqueue];

      id<MTLComputeCommandEncoder> producer=[command computeCommandEncoder];[producer setComputePipelineState:copyPipeline];
      const uint64_t copyRoot[]={payload.gpuAddress,textureTable.gpuAddress};
      if(annotation(producer,"metal.descriptorInlineLayout",0,0,2,8) ||
         annotation(producer,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void *)payload,0) ||
         annotation(producer,"metal.descriptorInlineBinding",0,1,(uint64_t)(__bridge void *)textureTable,0))return 15;
      [producer setBytes:copyRoot length:16 atIndex:0];[producer setBuffer:output offset:0 atIndex:1];[producer useResource:payload usage:MTLResourceUsageRead];
      if(asyncAlias)[producer setBuffer:replacementInput offset:0 atIndex:2];
      [producer useResource:textureTable usage:MTLResourceUsageWrite];
      [producer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      if(annotation(textureTable,"metal.descriptorSlotProducer",0,(uint64_t)(__bridge void *)producer,(uint64_t)(__bridge void *)payload,0))return 16;
      if(api)
      {
        RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=0;memcpy(&value.vector.uint64[1],payloadBytes,24);
        if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)textureTable,"metal.descriptorSlotGPUValue",eRENDERDOC_UInt64,4,&value))return 17;
      }
      if(annotation(textureTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)nextImage,0))return 18;
      [producer useResource:replacementInput usage:asyncAlias?MTLResourceUsageWrite:MTLResourceUsageRead];
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
      if(asyncAlias)
      {
        id<MTLHeap> __unsafe_unretained heaps[]={inputHeap,tableHeap};[render useHeaps:heaps count:tableAlias?2:1 stages:MTLRenderStageVertex];
      }
      else [render useResource:activeInput usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
      [render useResource:image usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
      [render useResource:other usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
      [render drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[render endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
      const uint32_t *words=(const uint32_t *)output.contents;
      if(cpuAlias && words[2]!=(capture==0?184U:248U))return 44;
      if(command.error || words[0]!=(capture==0?225U:161U) || words[1]!=0xdeadbeefU)return 9;
      uint8_t pixels[16]={};[drawable.texture getBytes:pixels bytesPerRow:8 fromRegion:MTLRegionMake2D(0,0,2,2) mipmapLevel:0];
      for(unsigned i=0;i<16;i+=4)if(pixels[i]!=128 || pixels[i+1]!=64 || pixels[i+2]!=(capture==0?225:161) || pixels[i+3]!=255)return 23;
      if(api && !api->EndFrameCapture(nullptr,nullptr))return 10;
      if(tableAlias)
      {
        if(annotation(bufferTable,"metal.descriptorSlotEvent",0,1,1,0))return 49;
        [bufferTable makeAliasable];bufferTable=baseTable;tables[0]=baseTable;tableVAs[0]=baseTable.gpuAddress;
        tableGeneration++;
        if(annotation(baseTable,"metal.descriptorSlotEvent",0,tableGeneration,0,0))return 50;
        generation=tableGeneration;
      }
      memcpy(bufferTable.contents,bufferBytes,24);
      if(annotation(bufferTable,"metal.descriptorSlotEvent",0,generation,2,0) ||
         annotation(bufferTable,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)input,4))return 32;
      [activeInput makeAliasable];
      if(implicitAlias)[replacementInput makeAliasable];

    }
    printf("frame heap alias native PASS results=122/225/161 VA_A=%llu VA_B=%llu VA_TABLE=%llu TEX=%llu SAMP=%llu TEX_B=%llu captures=%d\n",(unsigned long long)bufferBytes[0],(unsigned long long)tableVAs[1],(unsigned long long)tableVAs[0],(unsigned long long)textureBytes[1],(unsigned long long)samplerBytes[0],(unsigned long long)otherID,api?2:0);
  }
  return 0;
}
