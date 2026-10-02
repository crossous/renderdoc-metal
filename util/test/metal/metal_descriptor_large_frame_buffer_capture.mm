// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main() {
 @autoreleasepool {
  const bool heapSource=getenv("RENDERDOC_METAL_LARGE_FRAME_HEAP")!=nullptr;
  const bool privateSource=getenv("RENDERDOC_METAL_LARGE_FRAME_PRIVATE")!=nullptr;
  const bool largeCopy=getenv("RENDERDOC_METAL_LARGE_COPY")!=nullptr;
  const bool submissionUpload=getenv("RENDERDOC_METAL_SUBMISSION_ONLY_UPLOAD")!=nullptr;
  const unsigned copyCount=getenv("RENDERDOC_METAL_LARGE_COPY_COUNT")?unsigned(strtoul(getenv("RENDERDOC_METAL_LARGE_COPY_COUNT"),nullptr,10)):128;
  if(!copyCount||copyCount>255)return 15;
  if(privateSource&&!heapSource)return 2;
  const bool extendedHeap=getenv("RENDERDOC_METAL_EXTENDED_FRAME_HEAP_BYTES")!=nullptr;
  const NSUInteger length=extendedHeap?strtoul(getenv("RENDERDOC_METAL_EXTENDED_FRAME_HEAP_BYTES"),nullptr,10):heapSource?131072:4*1024*1024,offset=length-12;
  if(extendedHeap&&(!heapSource||privateSource||length<131072||length>1024*1024))return 16;
  const NSUInteger copyBytes=largeCopy?(heapSource?length:1024*1024):12;
  id<MTLDevice> device=MTLCreateSystemDefaultDevice();NSError *error=nil;
  NSString *code=@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Entry { const device uint *value [[id(0)]]; ulong zero [[id(1)]]; ulong metadata [[id(2)]]; };
kernel void frame_buffer(const device ulong *root [[buffer(0)]],device uint *out [[buffer(1)]]) {
 const device Entry *source=reinterpret_cast<const device Entry *>(root[0]);
 const device Entry *initial=reinterpret_cast<const device Entry *>(root[2]);
 out[0]=source->value[0]+initial->value[0];out[1]=source->value[1];out[2]=0x12345678;
 out[3]=source->metadata==0x4141414141414141ul&&initial->metadata==0x5151515151515151ul&&
  root[1]==0xabcdef0123456789ul&&root[3]==0xabcdef0123456789ul?0xdeadbeef:0xbad;
}
)MSL";
  id<MTLLibrary> library=[device newLibraryWithSource:code options:nil error:&error];
  id<MTLComputePipelineState> pipeline=library?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"frame_buffer"] error:&error]:nil;
  if(!pipeline){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 3;}
  const MTLResourceOptions options=(privateSource?MTLResourceStorageModePrivate:MTLResourceStorageModeShared)|MTLResourceHazardTrackingModeTracked;
  const auto layout=[device heapBufferSizeAndAlignWithLength:length options:options];
  if(!layout.align||!layout.size)return 4;
  const NSUInteger stride=(layout.size+layout.align-1)/layout.align*layout.align;
  auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=privateSource?MTLStorageModePrivate:MTLStorageModeShared;
  hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=MAX(NSUInteger(4096),stride*2);
  id<MTLHeap> heap=heapSource?[device newHeapWithDescriptor:hd]:nil;
  if(heapSource&&(!heap||hd.size>(extendedHeap?2:1)*1024*1024))return 5;
  const uint32_t inputValue=13,values[]={0xbad,41,80},overwritten[]={0xbad,90,120};
  id<MTLBuffer> input=[device newBufferWithBytes:&inputValue length:4 options:MTLResourceStorageModeShared];
  const uint64_t initialPacket[]={input.gpuAddress,0,0x5151515151515151ULL},empty[]={0,0,0};
  id<MTLBuffer> initialTable=[device newBufferWithBytes:initialPacket length:24 options:MTLResourceStorageModeShared];
  id<MTLBuffer> table=[device newBufferWithBytes:empty length:24 options:MTLResourceStorageModeShared];
  id<MTLBuffer> output=[device newBufferWithLength:16 options:MTLResourceStorageModeShared];
  id<MTLBuffer> upload=[device newBufferWithLength:copyBytes options:MTLResourceStorageModeShared];
  memset(upload.contents,0xee,copyBytes);memcpy((uint8_t *)upload.contents+copyBytes-12,values,12);
  id<MTLBuffer> overwrite=[device newBufferWithBytes:overwritten length:12 options:MTLResourceStorageModeShared];
  id<MTLCommandQueue> queue=[device newCommandQueue];
  CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
  RENDERDOC_API_1_7_0 *api=nullptr;
  if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH")) {
   auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 6;
   api->SetCaptureFilePathTemplate(path);RENDERDOC_AnnotationValue v={};v.uint32=extendedHeap?65:largeCopy?47:40;
   if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v))return 7;
  }
  auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
   if(!api)return uint32_t(0);RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;
   return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
  };
  if(annotation(table,"metal.descriptorTable",1,0,1,24)||annotation(initialTable,"metal.descriptorTable",1,0,1,24)||
     annotation(initialTable,"metal.descriptorSlotEvent",0,1,0,0)||annotation(initialTable,"metal.descriptorSlotEvent",0,1,2,0)||
     annotation(initialTable,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)input,0))return 8;
  uint64_t captured[2]={};
  for(unsigned capture=0;capture<(api?2u:1u);capture++) {
   memset(table.contents,0,24);memset(output.contents,0,16);
   if(submissionUpload)memset(upload.contents,0xaa,copyBytes);
   if(api)api->StartFrameCapture(nullptr,nullptr);
   if(submissionUpload){memset(upload.contents,0xee,copyBytes);memcpy((uint8_t *)upload.contents+copyBytes-12,values,12);}
   id<MTLBuffer> source=heapSource?[heap newBufferWithLength:length options:options offset:capture*stride]:[device newBufferWithLength:length options:MTLResourceStorageModeShared];
   if(!source)return 9;
   if(!privateSource){memset(source.contents,0xee,length);if(largeCopy)memset((uint8_t *)source.contents+length-copyBytes,0xaa,copyBytes);else memcpy((uint8_t *)source.contents+offset,values,12);}
   captured[capture]=source.gpuAddress+offset+4;
   const uint64_t packet[]={captured[capture],0,0x4141414141414141ULL};memcpy(table.contents,packet,24);
   if(annotation(table,"metal.descriptorSlotEvent",0,capture+1,0,0)||annotation(table,"metal.descriptorSlotEvent",0,capture+1,2,0)||
      annotation(table,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)source,offset+4))return 10;
   id<MTLCommandBuffer> command=[queue commandBuffer];
   if(privateSource||largeCopy){id<MTLBlitCommandEncoder> b=[command blitCommandEncoder];
    [b copyFromBuffer:upload sourceOffset:0 toBuffer:source destinationOffset:length-copyBytes size:copyBytes];
    if(largeCopy)for(unsigned copy=1;copy<copyCount;copy++)[b copyFromBuffer:upload sourceOffset:copyBytes-12 toBuffer:source destinationOffset:offset size:12];
    [b endEncoding];}
   id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];[compute setComputePipelineState:pipeline];
   if(annotation(compute,"metal.descriptorInlineLayout",0,0,2,16)||annotation(compute,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void *)table,0)||
      annotation(compute,"metal.descriptorInlineBinding",0,1,(uint64_t)(__bridge void *)initialTable,0))return 11;
   const uint64_t root[]={table.gpuAddress,0xabcdef0123456789ULL,initialTable.gpuAddress,0xabcdef0123456789ULL};
   [compute setBytes:root length:32 atIndex:0];[compute setBuffer:output offset:0 atIndex:1];
   for(id<MTLBuffer> b in @[table,initialTable,input,source])[compute useResource:b usage:MTLResourceUsageRead];
   [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];
   id<MTLBlitCommandEncoder> b=[command blitCommandEncoder];[b copyFromBuffer:overwrite sourceOffset:0 toBuffer:source destinationOffset:offset size:12];[b endEncoding];
   id<CAMetalDrawable> drawable=[layer nextDrawable];auto present=[MTLRenderPassDescriptor renderPassDescriptor];present.colorAttachments[0].texture=drawable.texture;
   present.colorAttachments[0].loadAction=MTLLoadActionClear;present.colorAttachments[0].storeAction=MTLStoreActionStore;
   [[command renderCommandEncoderWithDescriptor:present] endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
   const uint32_t *result=(const uint32_t *)output.contents;
   if(command.status!=MTLCommandBufferStatusCompleted||result[0]!=54||result[1]!=80||result[2]!=0x12345678||result[3]!=0xdeadbeef)return 12;
   if(api&&!api->EndFrameCapture(nullptr,nullptr))return 13;
   printf("Native large frame buffer PASS capture=%u length=%lu heap=%d private=%d GPU54/80\n",capture,(unsigned long)length,heapSource,privateSource);
   if(annotation(table,"metal.descriptorSlotEvent",0,capture+1,1,0))return 14;memset(table.contents,0,24);source=nil;
  }
  printf("FRAME_VAS=%llu,%llu captures=%u\n",(unsigned long long)captured[0],(unsigned long long)captured[1],api?2:1);
 }
 return 0;
}
