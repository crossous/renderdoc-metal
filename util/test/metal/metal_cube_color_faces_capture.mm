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
 const NSUInteger f=MTLPixelFormatRG11B10Float;
 auto td=[MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:(MTLPixelFormat)f size:8 mipmapped:YES];
 td.resourceOptions=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;
 td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
 id<MTLTexture>t=[d newTextureWithDescriptor:td];t.label=@"Background color target";
 if(!t)return 2;
 auto clear=[MTLRenderPassDescriptor renderPassDescriptor];clear.colorAttachments[0].texture=t;
 clear.colorAttachments[0].loadAction=MTLLoadActionClear;clear.colorAttachments[0].storeAction=MTLStoreActionStore;
 clear.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,0);
 id<MTLCommandBuffer>initial=[q commandBuffer];for(NSUInteger mip=0;mip<td.mipmapLevelCount;mip++)for(NSUInteger face=0;face<6;face++){clear.colorAttachments[0].level=mip;clear.colorAttachments[0].slice=face;[[initial renderCommandEncoderWithDescriptor:clear] endEncoding];}[initial commit];[initial waitUntilCompleted];if(initial.error)return 3;
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
  clear.colorAttachments[0].level=i;clear.colorAttachments[0].slice=4+i;
  clear.colorAttachments[0].storeAction=MTLStoreActionUnknown;
  clear.colorAttachments[0].clearColor=f==MTLPixelFormatR32Uint?MTLClearColorMake(17+i*14,0,0,1):MTLClearColorMake(.25+.5*i,.5,.75,1);
  id<MTLRenderCommandEncoder>enc=[cb renderCommandEncoderWithDescriptor:clear];[enc setColorStoreAction:MTLStoreActionStore atIndex:0];[enc endEncoding];
  id<CAMetalDrawable>dr=[l nextDrawable];auto present=[MTLRenderPassDescriptor renderPassDescriptor];present.colorAttachments[0].texture=dr.texture;present.colorAttachments[0].loadAction=MTLLoadActionClear;present.colorAttachments[0].storeAction=MTLStoreActionStore;
  [[cb renderCommandEncoderWithDescriptor:present] endEncoding];[cb presentDrawable:dr];[cb commit];[cb waitUntilCompleted];if(cb.error)return 7;
  if(api&&!api->EndFrameCapture(nullptr,nullptr))return 8;
  const NSUInteger bpp=(f==MTLPixelFormatRGBA16Float||f==MTLPixelFormatRGBA16Unorm)?8:f==MTLPixelFormatR8Unorm?1:4;
  id<MTLBuffer>read=[d newBufferWithLength:256*8 options:MTLResourceStorageModeShared];id<MTLCommandBuffer>copy=[q commandBuffer];id<MTLBlitCommandEncoder>blit=[copy blitCommandEncoder];
  [blit copyFromTexture:t sourceSlice:4+i sourceLevel:i sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(8>>i,8>>i,1) toBuffer:read destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:2048];[blit endEncoding];[copy commit];[copy waitUntilCompleted];if(copy.error)return 9;
  printf("NATIVE_CUBE_FORMAT=%lu capture=%u FIRST_PIXEL=",f,i);for(NSUInteger j=0;j<bpp;j++)printf("%02x",((const unsigned char*)read.contents)[j]);puts("");
 }
 return 0;
}}
