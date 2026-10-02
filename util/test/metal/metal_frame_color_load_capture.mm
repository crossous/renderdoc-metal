// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main(){@autoreleasepool {
 id<MTLDevice>d=MTLCreateSystemDefaultDevice();id<MTLCommandQueue>q=[d newCommandQueue];NSError*error=nil;
 auto lib=[d newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Entry {ulong zero [[id(0)]];texture2d<float,access::write> image [[id(1)]];ulong metadata [[id(2)]];};
kernel void write_color(const device ulong *root [[buffer(0)]],uint2 position [[thread_position_in_grid]]) {
 const device Entry *entry=reinterpret_cast<const device Entry *>(root[0]);
 if(entry->metadata==0x4141414141414141ul)entry->image.write(float4(.25,.5,.75,1),position);
}
)MSL" options:nil error:&error];
 auto pso=lib?[d newComputePipelineStateWithFunction:[lib newFunctionWithName:@"write_color"] error:&error]:nil;if(!pso){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 2;}
 auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:8 height:4 mipmapped:NO];
 td.resourceOptions=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
 auto layout=[d heapTextureSizeAndAlignWithDescriptor:td];auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=MAX(NSUInteger(65536),layout.size*2+layout.align*2);
 auto heap=[d newHeapWithDescriptor:hd];if(!heap)return 3;
 uint32_t value=13;auto input=[d newBufferWithBytes:&value length:4 options:MTLResourceStorageModeShared];
 uint64_t inputPacket[]={input.gpuAddress,0,0x5151515151515151ULL};auto inputTable=[d newBufferWithBytes:inputPacket length:24 options:MTLResourceStorageModeShared];
 uint64_t zero[3]={};id<MTLBuffer>table=[d newBufferWithBytes:zero length:24 options:MTLResourceStorageModeShared];
 RENDERDOC_API_1_7_0*api=nullptr;if(const char*path=getenv("RENDERDOC_METAL_CAPTURE_PATH")){auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void**)&api))return 4;api->SetCaptureFilePathTemplate(path);RENDERDOC_AnnotationValue v={};v.uint32=65;if(api->SetObjectAnnotation((__bridge void*)d,(__bridge void*)d,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v))return 5;}
 auto ann=[&](id object,const char*key,uint64_t a,uint64_t b,uint64_t c,uint64_t e){if(!api)return 0U;RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=e;return api->SetObjectAnnotation((__bridge void*)d,(__bridge void*)object,key,eRENDERDOC_UInt64,4,&v);};
 if(ann(table,"metal.descriptorTable",1,0,1,24)||ann(inputTable,"metal.descriptorTable",1,0,1,24)||ann(inputTable,"metal.descriptorSlotEvent",0,1,0,0)||ann(inputTable,"metal.descriptorSlotEvent",0,1,2,0)||ann(inputTable,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void*)input,0))return 6;
 CAMetalLayer*l=[CAMetalLayer layer];l.device=d;l.pixelFormat=MTLPixelFormatBGRA8Unorm;l.framebufferOnly=NO;l.drawableSize=CGSizeMake(2,2);
 for(unsigned i=0;i<(api?2U:1U);i++){
  memset(table.contents,0,24);if(api)api->StartFrameCapture(nullptr,nullptr);
  auto t=[heap newTextureWithDescriptor:td offset:i*((layout.size+layout.align-1)/layout.align*layout.align)];if(!t)return 7;t.label=@"Frame GPU-written color";
  uint64_t packet[]={0,t.gpuResourceID._impl,0x4141414141414141ULL};memcpy(table.contents,packet,24);
  if(ann(table,"metal.descriptorSlotEvent",0,i+1,0,5)||ann(table,"metal.descriptorSlotEvent",0,i+1,2,5)||ann(table,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void*)t,0))return 8;
  id<MTLCommandBuffer>cb=[q commandBuffer];[cb enqueue];auto cs=[cb computeCommandEncoder];[cs setComputePipelineState:pso];
  if(ann(cs,"metal.descriptorInlineLayout",0,0,1,8)||ann(cs,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void*)table,0))return 9;
  uint64_t root=table.gpuAddress;[cs setBytes:&root length:8 atIndex:0];[cs useResource:table usage:MTLResourceUsageRead];[cs useHeap:heap];
  [cs dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(8,4,1)];[cs endEncoding];
  auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=t;pass.colorAttachments[0].loadAction=MTLLoadActionLoad;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
  [[cb renderCommandEncoderWithDescriptor:pass] endEncoding];
  id<CAMetalDrawable>dr=[l nextDrawable];auto present=[MTLRenderPassDescriptor renderPassDescriptor];present.colorAttachments[0].texture=dr.texture;present.colorAttachments[0].loadAction=MTLLoadActionClear;present.colorAttachments[0].storeAction=MTLStoreActionStore;
  [[cb renderCommandEncoderWithDescriptor:present] endEncoding];[cb presentDrawable:dr];[cb commit];[cb waitUntilCompleted];if(cb.error)return 10;
  if(api&&!api->EndFrameCapture(nullptr,nullptr))return 11;
  auto read=[d newBufferWithLength:1024 options:MTLResourceStorageModeShared];auto copy=[q commandBuffer];auto blit=[copy blitCommandEncoder];[blit copyFromTexture:t sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(8,4,1) toBuffer:read destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:1024];[blit endEncoding];[copy commit];[copy waitUntilCompleted];if(copy.error)return 12;
  for(unsigned y=0;y<4;y++)for(unsigned x=0;x<8;x++)if(((const uint8_t*)read.contents)[y*256+x]!=64)return 13;
  if(ann(table,"metal.descriptorSlotEvent",0,i+1,1,5))return 14;
  printf("PASS Native frame color load capture=%u full32pixels=64 id=%llu\n",i,(unsigned long long)t.gpuResourceID._impl);
 }
 return 0;
}}
