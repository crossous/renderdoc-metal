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
 @autoreleasepool {
  id<MTLDevice> device=MTLCreateSystemDefaultDevice(); NSError *error=nil;
  const bool r16=getenv("RENDERDOC_METAL_FRAME_COLOR_R16")!=nullptr;
  const bool view=getenv("RENDERDOC_METAL_FRAME_COLOR_VIEW")!=nullptr;
  const bool retiredUniform=getenv("RENDERDOC_METAL_RETIRED_UNIFORM_ARENA")!=nullptr;
  NSString *source=@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct BufferEntry { const device uint *value [[id(0)]]; ulong zero [[id(1)]]; ulong metadata [[id(2)]]; };
struct Entry { ulong zero [[id(0)]]; texture2d<float> image [[id(1)]]; ulong metadata [[id(2)]]; };
kernel void read_color(const device ulong *root [[buffer(0)]], device uint *result [[buffer(1)]]) {
 const device Entry *table=reinterpret_cast<const device Entry *>(root[0]);
 float4 c=table[0].image.read(uint2(191,103));
 result[0]=uint(c.r*256); result[1]=uint(c.g*256); result[2]=uint(c.b*256);
 const device BufferEntry *input=reinterpret_cast<const device BufferEntry *>(root[1]);
 result[3]=table[0].metadata==0x4141414141414141ul&&input[0].value[0]==13&&input[0].metadata==0x5151515151515151ul?0xdeadbeef:0xbad;
}
)MSL";
  if(retiredUniform)
    source=[source stringByReplacingOccurrencesOfString:@"input[0].metadata==0x5151515151515151ul?"
      withString:@"input[0].metadata==0x5151515151515151ul && reinterpret_cast<const device uint *>(root[2])[0]==13 && reinterpret_cast<const device uint *>(root[2])[1]==17 && reinterpret_cast<const device uint *>(root[2])[2]==19 && reinterpret_cast<const device uint *>(root[2])[3]==23 && reinterpret_cast<const device uint *>(root[2])[4]==29 && reinterpret_cast<const device uint *>(root[2])[5]==31?"];
  id<MTLLibrary> library=[device newLibraryWithSource:source options:nil error:&error];
  id<MTLComputePipelineState> pipeline=library?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"read_color"] error:&error]:nil;
  if(!pipeline){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 2;}
  auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:r16?MTLPixelFormatR16Float:MTLPixelFormatRG11B10Float width:192 height:104 mipmapped:NO];
  d.resourceOptions=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;
  d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite|MTLTextureUsageRenderTarget;
  MTLTextureDescriptor *rw=[d copy];rw.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
  const auto layout=[device heapTextureSizeAndAlignWithDescriptor:d];
  const auto rwLayout=[device heapTextureSizeAndAlignWithDescriptor:rw];
  const NSUInteger rwOffset=rwLayout.align?(layout.size+rwLayout.align-1)/rwLayout.align*rwLayout.align:0;
  const NSUInteger stride=(rwOffset+rwLayout.size+layout.align-1)/layout.align*layout.align;
  if(!layout.size||!layout.align||!rwLayout.size||!rwLayout.align||stride*2>1024*1024)return 3;
  auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;
  hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=stride*2;
  id<MTLHeap> heap=[device newHeapWithDescriptor:hd];id<MTLCommandQueue> queue=[device newCommandQueue];
  const uint64_t zero[3]={};id<MTLBuffer> table=[device newBufferWithBytes:zero length:24 options:MTLResourceStorageModeShared];
  id<MTLBuffer> output=[device newBufferWithLength:16 options:MTLResourceStorageModeShared];
  table.label=@"Frame color table";output.label=@"Frame color output";
  const uint32_t value=13;id<MTLBuffer> input=[device newBufferWithBytes:&value length:4 options:MTLResourceStorageModeShared];
  const uint64_t inputPacket[]={input.gpuAddress,0,0x5151515151515151ULL};
  id<MTLBuffer> inputTable=[device newBufferWithBytes:inputPacket length:24 options:MTLResourceStorageModeShared];
  id<MTLBuffer> arena=retiredUniform?[device newBufferWithLength:24 options:MTLResourceStorageModeShared]:nil;
  if(retiredUniform&&!arena)return 15;
  arena.label=@"Retired descriptor uniform arena";
  CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
  layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
  RENDERDOC_API_1_7_0 *api=nullptr;
  if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH")) {
   auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
   if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 4;
   api->SetCaptureFilePathTemplate(path);RENDERDOC_AnnotationValue v={};v.uint32=retiredUniform?65:38;
   if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v))return 5;
  }
  auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t e) {
   if(!api)return uint32_t(0);RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=e;
   return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
  };
  if(annotation(table,"metal.descriptorTable",1,0,1,24)||annotation(inputTable,"metal.descriptorTable",1,0,1,24)||
     annotation(inputTable,"metal.descriptorSlotEvent",0,1,0,0)||annotation(inputTable,"metal.descriptorSlotEvent",0,1,2,0)||
     annotation(inputTable,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)input,0))return 6;
  const uint64_t retiredPacket[]={input.gpuAddress,0,0};
  if(retiredUniform) {
   memcpy(arena.contents,retiredPacket,24);
   if(annotation(arena,"metal.descriptorTable",2,0,1,24)||
      annotation(arena,"metal.descriptorSlotEvent",0,1,0,6)||
      annotation(arena,"metal.descriptorSlotEvent",0,1,2,6)||
      annotation(arena,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)input,0)||
      annotation(arena,"metal.descriptorSlotEvent",0,1,1,6))return 16;
  }
  uint64_t capturedIDs[2]={};
  for(unsigned capture=0;capture<(api?2u:1u);capture++) {
   memset(output.contents,0,16);memset(table.contents,0,24);
   if(retiredUniform)memcpy(arena.contents,retiredPacket,24);
   if(api)api->StartFrameCapture(nullptr,nullptr);
   if(retiredUniform) {
    const uint32_t constants[]={13,17,19,23,29,31};
    memcpy(arena.contents,constants,sizeof(constants));
   }
   id<MTLTexture> image=[heap newTextureWithDescriptor:d offset:capture*stride];if(!image)return 7;
   // Exact UE usage (read/write, no render target) also has a legal CPU birth.
   id<MTLTexture> rwImage=[heap newTextureWithDescriptor:rw offset:capture*stride+rwOffset];if(!rwImage)return 8;
   id<MTLTexture> sampled=view?[image newTextureViewWithPixelFormat:d.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(0,1) slices:NSMakeRange(0,1)]:image;
   if(!sampled)return 9;capturedIDs[capture]=sampled.gpuResourceID._impl;
   uint64_t packet[3]={0,capturedIDs[capture],0x4141414141414141ULL};memcpy(table.contents,packet,24);
   if(annotation(table,"metal.descriptorSlotEvent",0,capture+1,0,4)||annotation(table,"metal.descriptorSlotEvent",0,capture+1,2,4)||
      annotation(table,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void *)sampled,0))return 10;
   id<MTLCommandBuffer> command=[queue commandBuffer];
   auto clear=[MTLRenderPassDescriptor renderPassDescriptor];clear.colorAttachments[0].texture=image;
   clear.colorAttachments[0].loadAction=MTLLoadActionClear;clear.colorAttachments[0].storeAction=MTLStoreActionStore;
   clear.colorAttachments[0].clearColor=MTLClearColorMake(0.25,0.5,0.75,1);
   [[command renderCommandEncoderWithDescriptor:clear] endEncoding];
   id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];[compute setComputePipelineState:pipeline];
   if(annotation(compute,"metal.descriptorInlineLayout",0,0,retiredUniform?3:2,8)||annotation(compute,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void *)table,0)||
      annotation(compute,"metal.descriptorInlineBinding",0,1,(uint64_t)(__bridge void *)inputTable,0))return 14;
   if(retiredUniform&&annotation(compute,"metal.descriptorInlineBinding",0,2,(uint64_t)(__bridge void *)arena,0))return 17;
   const uint64_t root[]={table.gpuAddress,inputTable.gpuAddress,arena.gpuAddress};[compute setBytes:root length:retiredUniform?24:16 atIndex:0];[compute setBuffer:output offset:0 atIndex:1];
   if(retiredUniform)[compute useResource:arena usage:MTLResourceUsageRead];
   [compute useResource:input usage:MTLResourceUsageRead];[compute useResource:inputTable usage:MTLResourceUsageRead];
   [compute useResource:table usage:MTLResourceUsageRead];[compute useResource:sampled usage:MTLResourceUsageRead];
   [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];
   clear.colorAttachments[0].clearColor=MTLClearColorMake(0.75,0.25,0.5,1);
   [[command renderCommandEncoderWithDescriptor:clear] endEncoding];
   id<CAMetalDrawable> drawable=[layer nextDrawable];auto present=[MTLRenderPassDescriptor renderPassDescriptor];
   present.colorAttachments[0].texture=drawable.texture;present.colorAttachments[0].loadAction=MTLLoadActionClear;present.colorAttachments[0].storeAction=MTLStoreActionStore;
   [[command renderCommandEncoderWithDescriptor:present] endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
   const uint32_t *result=(const uint32_t *)output.contents;
   if(command.status!=MTLCommandBufferStatusCompleted||result[0]!=64||result[1]!=(r16?0:128)||result[2]!=(r16?0:192)||result[3]!=0xdeadbeef)return 11;
   if(api&&!api->EndFrameCapture(nullptr,nullptr))return 12;
   printf("Native frame color PASS capture=%u gpu=%u/%u/%u id=%llu\n",capture,result[0],result[1],result[2],(unsigned long long)capturedIDs[capture]);
   if(annotation(table,"metal.descriptorSlotEvent",0,capture+1,1,4))return 13;
   memset(table.contents,0,24);sampled=nil;image=nil;rwImage=nil;
  }
  printf("FRAME_IDS=%llu,%llu captures=%u\n",(unsigned long long)capturedIDs[0],(unsigned long long)capturedIDs[1],api?2:1);
 }
 return 0;
}
