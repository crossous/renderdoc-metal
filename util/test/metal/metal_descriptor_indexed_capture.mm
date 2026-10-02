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
    const bool privateIndices=getenv("RENDERDOC_METAL_PRIVATE_INDICES")!=nullptr;
    const bool frameIndices=getenv("RENDERDOC_METAL_FRAME_INDICES")!=nullptr;
    const bool splitUpload=frameIndices && getenv("RENDERDOC_METAL_SPLIT_INDEX_UPLOAD")!=nullptr;
    const bool lateUpload=frameIndices && getenv("RENDERDOC_METAL_LATE_INDEX_UPLOAD")!=nullptr;
    const bool lateSignal=lateUpload && getenv("RENDERDOC_METAL_LATE_SIGNAL_PREFIX")!=nullptr;
    const bool poisonTail=frameIndices && getenv("RENDERDOC_METAL_POISON_INDEX_TAIL")!=nullptr;
    const bool lateStagingBirth=lateUpload && getenv("RENDERDOC_METAL_LATE_INDEX_STAGING_BIRTH")!=nullptr;
    const unsigned uploadCopies=getenv("RENDERDOC_METAL_INDEX_UPLOAD_COPY_COUNT")?unsigned(atoi(getenv("RENDERDOC_METAL_INDEX_UPLOAD_COPY_COUNT"))):1;
    if(!uploadCopies || uploadCopies>16)return 35;
    const bool drawWork=getenv("RENDERDOC_METAL_DRAW_WORK")!=nullptr;
    const bool strip=drawWork && getenv("RENDERDOC_METAL_TRIANGLE_STRIP")!=nullptr;
    const char *batchOption=getenv("RENDERDOC_METAL_DRAW_BATCH_COUNT");
    const unsigned drawBatch=drawWork ? (batchOption?unsigned(atoi(batchOption)):256) : 1;
    if(!drawBatch || drawBatch>512)return 30;
    const unsigned drawCount=drawWork?(strip?4:6):3, instances=drawWork?2:1;
    const int baseVertex=drawWork&&!strip?-3:0;
    const unsigned baseInstance=drawWork?7:0;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice(); NSError *error=nil;
    id<MTLSharedEvent> prefixEvent=lateSignal?[device newSharedEvent]:nil;
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
struct VSOut { float4 position [[position]]; float value; };
vertex VSOut resource_vertex(uint index [[vertex_id]], const device ulong *root [[buffer(0)]],
    constant uint *drawParams [[buffer(4)]], constant ushort *indexKind [[buffer(5)]])
{
  const device BufferEntry *buffer=reinterpret_cast<const device BufferEntry *>(root[0]);
  float2 positions[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
  float2 quad[4]={float2(-1,-1),float2(1,-1),float2(-1,1),float2(1,1)};
  VSOut out;out.position=float4(drawParams[0]==4?quad[index%4]:positions[index%3],0,1);
  out.value=(buffer->metadata==0x1111111111111111ul && root[1]==0xabcdef0123456789ul &&
      (drawParams[0]==3 || drawParams[0]==4 || drawParams[0]==6) &&
      drawParams[1]==(drawParams[0]==3?1u:2u) && drawParams[2]==(indexKind[0]==2 ? 4u : 2u) &&
      drawParams[3]==(drawParams[0]==6?0xfffffffdu:0u) &&
      drawParams[4]==(drawParams[0]==3?0u:7u) && (indexKind[0]==1 || indexKind[0]==2)) ? float(buffer->value[0]) : 0.0;
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
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get || !get(eRENDERDOC_API_Version_1_7_0,(void **)&api)) return 4;
      api->SetCaptureFilePathTemplate(path);
      RENDERDOC_AnnotationValue coverage={}; coverage.uint32=lateSignal?65:(frameIndices?52:(drawWork?49:(privateIndices?48:14)));
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
    const char *wideOption=getenv("RENDERDOC_METAL_WIDE_INDICES");
    const bool wideIndices=wideOption && atoi(wideOption)!=0;
    uint16_t narrow[]={65535,0,1,2,65535,65535,65535,65535};
    uint32_t wide[]={0xffffffffU,0,1,2,0xffffffffU,0xffffffffU,0xffffffffU,0xffffffffU};
    if(drawWork)for(unsigned i=0;i<drawCount;i++) {
      const unsigned value=strip?i:(i+2)%3+3;
      narrow[i+1]=uint16_t(value);wide[i+1]=value;
    }
    id<MTLBuffer> indices=wideIndices ? [device newBufferWithBytes:wide length:sizeof(wide) options:MTLResourceStorageModeShared] :
        [device newBufferWithBytes:narrow length:sizeof(narrow) options:MTLResourceStorageModeShared];
    if(!indices)return 26;
    const uint32_t zeroIndices[8]={};
    id<MTLBuffer> poison=poisonTail?[device newBufferWithBytes:zeroIndices length:sizeof(zeroIndices) options:MTLResourceStorageModeShared]:nil;
    if(poisonTail && !poison)return 33;
    id<MTLCommandQueue> queue=[device newCommandQueue];
    MTLHeapDescriptor *heapDesc=[MTLHeapDescriptor new];heapDesc.type=MTLHeapTypePlacement;
    heapDesc.size=128*1024;heapDesc.storageMode=MTLStorageModePrivate;heapDesc.hazardTrackingMode=MTLHazardTrackingModeTracked;
    id<MTLHeap> indexHeap=frameIndices?[device newHeapWithDescriptor:heapDesc]:nil;
    heapDesc.storageMode=MTLStorageModeShared;
    id<MTLHeap> uploadHeap=frameIndices?[device newHeapWithDescriptor:heapDesc]:nil;
    if(frameIndices && (!indexHeap || !uploadHeap))return 31;
    if(privateIndices && !frameIndices) {
      id<MTLBuffer> staging=indices;
      indices=[device newBufferWithLength:staging.length options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked];
      if(!indices)return 28;
      id<MTLCommandBuffer> seed=[queue commandBuffer];id<MTLBlitCommandEncoder> blit=[seed blitCommandEncoder];
      [blit copyFromBuffer:staging sourceOffset:0 toBuffer:indices destinationOffset:0 size:staging.length];[blit endEncoding];
      [seed commit];[seed waitUntilCompleted];if(seed.status!=MTLCommandBufferStatusCompleted)return 29;
    }
    for(int capture=0;capture<(api?2:1);capture++)
    {
      id<MTLTexture> nextImage=capture==0?other:image;
      payloadBytes[1]=capture==0?otherID:textureBytes[1];
      memcpy(payload.contents,payloadBytes,24);
      if(annotation(payload,"metal.descriptorSlotEvent",0,1,2,4) ||
         annotation(payload,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)nextImage,0))return 13;
      if(api) api->StartFrameCapture(nullptr,nullptr);
      id<MTLBuffer> indexUpload=nil;
      if(frameIndices) {
        const NSUInteger bytes=wideIndices?sizeof(wide):sizeof(narrow);
        indices=[indexHeap newBufferWithLength:256 options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:0];
        if(!lateStagingBirth) {
          indexUpload=[uploadHeap newBufferWithLength:bytes options:MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked offset:0];
          if(!indexUpload)return 32;
          memset(indexUpload.contents,0xff,bytes);
          // Writes after birth are captured in this submission's automatic CPU snapshot.
          memcpy(indexUpload.contents,wideIndices?(const void *)wide:(const void *)narrow,bytes);
        }
        if(!indices)return 32;
      }
      uint64_t root[6]={};for(unsigned i=0;i<3;i++){root[i*2]=tableVAs[i];root[i*2+1]=0xabcdef0123456789ULL;}
      id<MTLCommandBuffer> command=[queue commandBuffer];if(!splitUpload && !lateUpload)[command enqueue];
      if(frameIndices && !lateUpload) {
        id<MTLCommandBuffer> uploadCommand=command;
        if(splitUpload) command=[queue commandBuffer];
        id<MTLBlitCommandEncoder> b=[uploadCommand blitCommandEncoder];
        [b copyFromBuffer:indexUpload sourceOffset:0 toBuffer:indices destinationOffset:0 size:indexUpload.length];[b endEncoding];
        if(splitUpload) { [uploadCommand commit]; }
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
        [compute useResource:input usage:MTLResourceUsageRead];[compute useResource:image usage:MTLResourceUsageRead];
        [compute useResource:other usage:MTLResourceUsageRead];
        [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];return true;
      };
      if(!consume())return 14;
      id<MTLComputeCommandEncoder> producer=[command computeCommandEncoder];[producer setComputePipelineState:copyPipeline];
      const uint64_t copyRoot[]={payload.gpuAddress,textureTable.gpuAddress};
      if(annotation(producer,"metal.descriptorInlineLayout",0,0,2,8) ||
         annotation(producer,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void *)payload,0) ||
         annotation(producer,"metal.descriptorInlineBinding",0,1,(uint64_t)(__bridge void *)textureTable,0))return 15;
      [producer setBytes:copyRoot length:16 atIndex:0];[producer setBuffer:output offset:0 atIndex:1];[producer useResource:payload usage:MTLResourceUsageRead];
      [producer useResource:textureTable usage:MTLResourceUsageWrite];
      [producer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
      if(annotation(textureTable,"metal.descriptorSlotProducer",0,(uint64_t)(__bridge void *)producer,(uint64_t)(__bridge void *)payload,0))return 16;
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
      const uint32_t drawParams[]={drawCount,instances,wideIndices?4u:2u,uint32_t(baseVertex),baseInstance};const uint16_t indexKind=wideIndices?2:1;
      if(annotation(render,"metal.inlineDrawConstants",1,4,sizeof(drawParams),0) ||
         annotation(render,"metal.inlineDrawConstants",1,5,sizeof(indexKind),0)){[render endEncoding];return 27;}
      [render setVertexBytes:drawParams length:sizeof(drawParams) atIndex:4];
      [render setVertexBytes:&indexKind length:sizeof(indexKind) atIndex:5];
      [render setVertexBytes:root length:sizeof(root) atIndex:0];
      [render setFragmentBytes:root length:sizeof(root) atIndex:0];
      for(id<MTLBuffer> table:tables)[render useResource:table usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
      [render useResource:input usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
      [render useResource:image usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
      [render useResource:other usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
      for(unsigned draw=0;draw<drawBatch;draw++)
        [render drawIndexedPrimitives:strip?MTLPrimitiveTypeTriangleStrip:MTLPrimitiveTypeTriangle indexCount:drawCount indexType:wideIndices?MTLIndexTypeUInt32:MTLIndexTypeUInt16 indexBuffer:indices indexBufferOffset:wideIndices?4:2 instanceCount:instances baseVertex:baseVertex baseInstance:baseInstance];
      [render endEncoding];
      if(poisonTail) {
        // Make the loading pass leave different Private bytes at the frame end.
        // A later draw seek must reconstruct its upload instead of relying on heap leftovers.
        id<MTLBlitCommandEncoder> b=[command blitCommandEncoder];
        [b copyFromBuffer:poison sourceOffset:0 toBuffer:indices destinationOffset:0 size:wideIndices?sizeof(wide):sizeof(narrow)];[b endEncoding];
      }
      NSMutableArray<id<MTLBuffer>> *extraUploads=[NSMutableArray new];
      if(lateUpload) {
        if(lateSignal) {
          // UE may encode this submission after the selected draw but submit it first.
          // A partial replay must preserve the signal rather than invent or skip its commit.
          id<MTLCommandBuffer> signal=[queue commandBuffer];
          [signal encodeSignalEvent:prefixEvent value:capture+1];[signal commit];
        }
        if(lateStagingBirth) {
          const NSUInteger bytes=wideIndices?sizeof(wide):sizeof(narrow);
          indexUpload=[uploadHeap newBufferWithLength:bytes options:MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked offset:0];
          if(!indexUpload)return 34;
          memcpy(indexUpload.contents,wideIndices?(const void *)wide:(const void *)narrow,bytes);
        }
        // Encode the producer after its consumer, but submit it first, as UE does.
        id<MTLCommandBuffer> uploadCommand=[queue commandBuffer];id<MTLBlitCommandEncoder> b=[uploadCommand blitCommandEncoder];
        [b copyFromBuffer:indexUpload sourceOffset:0 toBuffer:indices destinationOffset:0 size:indexUpload.length];
        const auto extraLayout=[device heapBufferSizeAndAlignWithLength:256 options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked];
        const NSUInteger extraStride=(extraLayout.size+extraLayout.align-1)/extraLayout.align*extraLayout.align;
        for(unsigned copy=1;copy<uploadCopies;copy++) {
          id<MTLBuffer> destination=[indexHeap newBufferWithLength:256 options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:extraStride*copy];
          if(!destination)return 36;[extraUploads addObject:destination];
          [b copyFromBuffer:indexUpload sourceOffset:0 toBuffer:destination destinationOffset:0 size:indexUpload.length];
        }
        [b endEncoding];[uploadCommand commit];
      }
      [command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
      if(lateSignal && prefixEvent.signaledValue!=capture+1)return 37;
      const uint32_t *words=(const uint32_t *)output.contents;
      if(command.error || words[0]!=(capture==0?186U:122U) || words[1]!=0xdeadbeefU)return 9;
      uint8_t pixels[16]={};[drawable.texture getBytes:pixels bytesPerRow:8 fromRegion:MTLRegionMake2D(0,0,2,2) mipmapLevel:0];
      for(unsigned i=0;i<16;i+=4)if(pixels[i]!=128 || pixels[i+1]!=64 || pixels[i+2]!=(capture==0?186:122) || pixels[i+3]!=255)return 23;
      if(api && !api->EndFrameCapture(nullptr,nullptr))return 10;
      if(frameIndices){indices=nil;indexUpload=nil;}
    }
    printf("resources GPU producer native PASS results=122/186/122 VA_A=%llu VA_B=%llu VA_TABLE=%llu TEX=%llu SAMP=%llu TEX_B=%llu captures=%d\n",(unsigned long long)bufferBytes[0],(unsigned long long)tableVAs[1],(unsigned long long)tableVAs[0],(unsigned long long)textureBytes[1],(unsigned long long)samplerBytes[0],(unsigned long long)otherID,api?2:0);
  }
  return 0;
}
