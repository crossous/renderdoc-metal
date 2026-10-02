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
 const bool terminal=getenv("RENDERDOC_METAL_LINEAR_TERMINAL_UPLOAD")!=nullptr;
 id<MTLDevice>d=MTLCreateSystemDefaultDevice();id<MTLCommandQueue>q=[d newCommandQueue];
 const NSUInteger f=strtoull(getenv("RENDERDOC_METAL_LINEAR_UPLOAD_FORMAT"),nullptr,10);
 auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(MTLPixelFormat)f width:8 height:4 mipmapped:NO];
 td.resourceOptions=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;
 td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
 id<MTLTexture>t=[d newTextureWithDescriptor:td];t.label=@"Background color target";
 if(!t)return 2;
 auto clear=[MTLRenderPassDescriptor renderPassDescriptor];clear.colorAttachments[0].texture=t;
 clear.colorAttachments[0].loadAction=MTLLoadActionClear;clear.colorAttachments[0].storeAction=MTLStoreActionStore;
 clear.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,0);
 id<MTLCommandBuffer>initial=[q commandBuffer];[[initial renderCommandEncoderWithDescriptor:clear] endEncoding];[initial commit];[initial waitUntilCompleted];if(initial.error)return 3;
 const uint32_t inputValue=13;id<MTLBuffer>input=[d newBufferWithBytes:&inputValue length:4 options:MTLResourceStorageModeShared];
 const uint64_t packet[]={input.gpuAddress,0,0x5151515151515151ULL};id<MTLBuffer>table=[d newBufferWithBytes:packet length:24 options:MTLResourceStorageModeShared];
 RENDERDOC_API_1_7_0 *api=nullptr;if(const char*path=getenv("RENDERDOC_METAL_CAPTURE_PATH")){
  auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void**)&api))return 4;api->SetCaptureFilePathTemplate(path);
  RENDERDOC_AnnotationValue v={};v.uint32=65;if(api->SetObjectAnnotation((__bridge void*)d,(__bridge void*)d,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v))return 5;
 }
 auto ann=[&](const char*key,uint64_t a,uint64_t b,uint64_t c,uint64_t e){if(!api)return 0U;RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=e;return api->SetObjectAnnotation((__bridge void*)d,(__bridge void*)table,key,eRENDERDOC_UInt64,4,&v);};
 if(ann("metal.descriptorTable",1,0,1,24)||ann("metal.descriptorSlotEvent",0,1,0,0)||ann("metal.descriptorSlotEvent",0,1,2,0)||ann("metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void*)input,0))return 6;
 CAMetalLayer*l=[CAMetalLayer layer];l.device=d;l.pixelFormat=MTLPixelFormatBGRA8Unorm;l.framebufferOnly=NO;l.drawableSize=CGSizeMake(2,2);
 for(unsigned i=0;i<(api?2U:1U);i++){
  if(api)api->StartFrameCapture(nullptr,nullptr);
  id<MTLCommandBuffer>cb=[q commandBuffer];[cb enqueue];
  const NSUInteger bpp=f==MTLPixelFormatR8Unorm?1:4;
  auto source=[d newBufferWithLength:terminal?4194304:1056 options:MTLResourceStorageModeShared];memset(source.contents,0,source.length);
  auto patch=[d newBufferWithLength:terminal?4194304:544 options:MTLResourceStorageModeShared];memset(patch.contents,0,patch.length);
  for(unsigned y=0;y<4;y++)memset((uint8_t*)source.contents+32+y*256,0x40+i*16,8*bpp);
  for(unsigned y=0;y<2;y++)memset((uint8_t*)patch.contents+32+y*256,0x90+i*16,2*bpp);
  auto enc=[cb blitCommandEncoder];
  [enc copyFromBuffer:source sourceOffset:32 sourceBytesPerRow:256 sourceBytesPerImage:1024 sourceSize:MTLSizeMake(8,4,1) toTexture:t destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0) options:MTLBlitOptionNone];
  [enc copyFromBuffer:patch sourceOffset:32 sourceBytesPerRow:256 sourceBytesPerImage:512 sourceSize:MTLSizeMake(2,2,1) toTexture:t destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(1,1,0) options:MTLBlitOptionNone];
  [enc endEncoding];
  id<CAMetalDrawable>dr=[l nextDrawable];auto present=[MTLRenderPassDescriptor renderPassDescriptor];present.colorAttachments[0].texture=dr.texture;present.colorAttachments[0].loadAction=MTLLoadActionClear;present.colorAttachments[0].storeAction=MTLStoreActionStore;
  [[cb renderCommandEncoderWithDescriptor:present] endEncoding];[cb presentDrawable:dr];[cb commit];[cb waitUntilCompleted];if(cb.error)return 7;
  if(terminal && ([source setPurgeableState:MTLPurgeableStateEmpty]!=MTLPurgeableStateNonVolatile ||
                  [patch setPurgeableState:MTLPurgeableStateEmpty]!=MTLPurgeableStateNonVolatile))return 11;
  if(api&&!api->EndFrameCapture(nullptr,nullptr))return 8;

  id<MTLBuffer>read=[d newBufferWithLength:256*4 options:MTLResourceStorageModeShared];id<MTLCommandBuffer>copy=[q commandBuffer];id<MTLBlitCommandEncoder>blit=[copy blitCommandEncoder];
  [blit copyFromTexture:t sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(8,4,1) toBuffer:read destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:1024];[blit endEncoding];[copy commit];[copy waitUntilCompleted];if(copy.error)return 9;
  for(unsigned y=0;y<4;y++)for(unsigned x=0;x<8;x++)for(unsigned component=0;component<bpp;component++) {
   const uint8_t value=((const uint8_t*)read.contents)[y*256+x*bpp+component];
   if(value!=((x>=1&&x<3&&y>=1&&y<3)?0x90+i*16:0x40+i*16))return 10;
  }
  printf("NATIVE_FORMAT=%lu capture=%u FIRST_PIXEL=",f,i);for(NSUInteger j=0;j<bpp;j++)printf("%02x",((const unsigned char*)read.contents)[j]);puts("");
 }
 return 0;
}}
