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
 id<MTLDevice>d=MTLCreateSystemDefaultDevice();id<MTLCommandQueue>q=[d newCommandQueue];
 const NSUInteger f=MTLPixelFormatBGRA8Unorm;
 NSError *error=nil;auto lib=[d newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Entry {ulong zero [[id(0)]];texture2d<float,access::read> image [[id(1)]];ulong metadata [[id(2)]];};
kernel void read_drawable(const device ulong *root [[buffer(0)]],device uint *output [[buffer(1)]]) {
 const device Entry *entry=reinterpret_cast<const device Entry *>(root[0]);
 uint4 color=uint4(entry->image.read(uint2(0))*255.0+.5);
 output[0]=color.x+color.y+color.z+color.w;output[1]=entry->metadata==0x4141414141414141ul?0xdeadbeefU:0xbadU;
}
)MSL" options:nil error:&error];auto pso=lib?[d newComputePipelineStateWithFunction:[lib newFunctionWithName:@"read_drawable"] error:&error]:nil;if(!pso){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 2;}
 auto output=[d newBufferWithLength:8 options:MTLResourceStorageModeShared];
 uint64_t zero[3]={};auto imageTable=[d newBufferWithBytes:zero length:24 options:MTLResourceStorageModeShared];
 const uint32_t inputValue=13;id<MTLBuffer>input=[d newBufferWithBytes:&inputValue length:4 options:MTLResourceStorageModeShared];
 const uint64_t packet[]={input.gpuAddress,0,0x5151515151515151ULL};id<MTLBuffer>table=[d newBufferWithBytes:packet length:24 options:MTLResourceStorageModeShared];
 RENDERDOC_API_1_7_0 *api=nullptr;if(const char*path=getenv("RENDERDOC_METAL_CAPTURE_PATH")){
  auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void**)&api))return 4;api->SetCaptureFilePathTemplate(path);
  RENDERDOC_AnnotationValue v={};v.uint32=65;if(api->SetObjectAnnotation((__bridge void*)d,(__bridge void*)d,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v))return 5;
 }
 auto ann=[&](const char*key,uint64_t a,uint64_t b,uint64_t c,uint64_t e){if(!api)return 0U;RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=e;return api->SetObjectAnnotation((__bridge void*)d,(__bridge void*)table,key,eRENDERDOC_UInt64,4,&v);};
 if(ann("metal.descriptorTable",1,0,1,24)||ann("metal.descriptorSlotEvent",0,1,0,0)||ann("metal.descriptorSlotEvent",0,1,2,0)||ann("metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void*)input,0))return 6;
 auto imageAnn=[&](id object,const char*key,uint64_t a,uint64_t b,uint64_t c,uint64_t e){if(!api)return 0U;RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=e;return api->SetObjectAnnotation((__bridge void*)d,(__bridge void*)object,key,eRENDERDOC_UInt64,4,&v);};
 if(imageAnn(imageTable,"metal.descriptorTable",1,0,1,24))return 11;
 CAMetalLayer*l=[CAMetalLayer layer];l.device=d;l.pixelFormat=MTLPixelFormatBGRA8Unorm;l.framebufferOnly=NO;l.drawableSize=CGSizeMake(2,2);
 for(unsigned i=0;i<(api?2U:1U);i++){
  if(api)api->StartFrameCapture(nullptr,nullptr);
  id<CAMetalDrawable>dr=[l nextDrawable];id<MTLTexture>t=dr.texture;
  uint64_t imagePacket[]={0,t.gpuResourceID._impl,0x4141414141414141ULL};memcpy(imageTable.contents,imagePacket,24);
  if(imageAnn(imageTable,"metal.descriptorSlotEvent",0,i+1,0,4)||imageAnn(imageTable,"metal.descriptorSlotEvent",0,i+1,2,4)||imageAnn(imageTable,"metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void*)t,0))return 12;
  id<MTLCommandBuffer>cb=[q commandBuffer];[cb enqueue];
  auto clear=[MTLRenderPassDescriptor renderPassDescriptor];clear.colorAttachments[0].texture=t;clear.colorAttachments[0].loadAction=MTLLoadActionClear;clear.colorAttachments[0].storeAction=MTLStoreActionStore;
  clear.colorAttachments[0].clearColor=MTLClearColorMake(.25,.5,.75,1);
  [[cb renderCommandEncoderWithDescriptor:clear] endEncoding];
  auto cs=[cb computeCommandEncoder];[cs setComputePipelineState:pso];
  if(imageAnn(cs,"metal.descriptorInlineLayout",0,0,1,8)||imageAnn(cs,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void*)imageTable,0))return 13;
  uint64_t root=imageTable.gpuAddress;[cs setBytes:&root length:8 atIndex:0];[cs setBuffer:output offset:0 atIndex:1];[cs useResource:imageTable usage:MTLResourceUsageRead];[cs useResource:t usage:MTLResourceUsageRead];
  [cs dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[cs endEncoding];
  [cb presentDrawable:dr];[cb commit];[cb waitUntilCompleted];if(cb.error)return 7;
  const uint32_t *result=(const uint32_t*)output.contents;if(result[0]!=638||result[1]!=0xdeadbeefU)return 14;
  if(imageAnn(imageTable,"metal.descriptorSlotEvent",0,i+1,1,4))return 15;
  if(api&&!api->EndFrameCapture(nullptr,nullptr))return 8;
  const NSUInteger bpp=(f==MTLPixelFormatRGBA16Float||f==MTLPixelFormatRGBA16Unorm)?8:f==MTLPixelFormatR8Unorm?1:4;
  id<MTLBuffer>read=[d newBufferWithLength:256*4 options:MTLResourceStorageModeShared];id<MTLCommandBuffer>copy=[q commandBuffer];id<MTLBlitCommandEncoder>blit=[copy blitCommandEncoder];
  [blit copyFromTexture:t sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(2,2,1) toBuffer:read destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:512];[blit endEncoding];[copy commit];[copy waitUntilCompleted];if(copy.error)return 9;
  printf("PASS drawable binding-before-Clear / GPU read-after-Clear output638 capture=%u FIRST_PIXEL=",i);for(NSUInteger j=0;j<bpp;j++)printf("%02x",((const unsigned char*)read.contents)[j]);puts("");
 }
 return 0;
}}
