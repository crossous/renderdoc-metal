// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "renderdoc/api/app/renderdoc_app.h"
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "metal_frame_attachment_load_cases.h"
int main(int argc,char **argv){if(argc!=4)return 1;@autoreleasepool {
 __attribute__((objc_precise_lifetime)) id<MTLDevice>d=MTLCreateSystemDefaultDevice();auto q=[d newCommandQueue];NSError *error=nil;
 auto lib=[d newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[2]]] error:&error];if(!lib)return 2;
 const FrameColorCase *initialColor=FindFrameColorCase(argv[3]);
 const MTLPixelFormat formats[]={initialColor?MTLPixelFormat(initialColor->format):MTLPixelFormatRGBA8Unorm,MTLPixelFormatRGBA8Unorm,MTLPixelFormatRG11B10Float,MTLPixelFormatDepth32Float_Stencil8};
 id<MTLRenderPipelineState> pipelines[2];
 for(unsigned i=0;i<2;i++){
  auto pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[lib newFunctionWithName:i?@"load_second":@"load_first"];pd.fragmentFunction=[lib newFunctionWithName:i?@"color_second":@"color_first"];
  for(unsigned j=0;j<3;j++)pd.colorAttachments[j].pixelFormat=formats[j];pd.depthAttachmentPixelFormat=formats[3];pd.stencilAttachmentPixelFormat=formats[3];
  pipelines[i]=[d newRenderPipelineStateWithDescriptor:pd error:&error];if(!pipelines[i]){fprintf(stderr,"%s\n",error.description.UTF8String);return 3;}
 }
 auto ds=[MTLDepthStencilDescriptor new];ds.depthCompareFunction=MTLCompareFunctionAlways;ds.depthWriteEnabled=YES;
 auto sd=[MTLStencilDescriptor new];sd.stencilCompareFunction=MTLCompareFunctionAlways;sd.depthStencilPassOperation=MTLStencilOperationReplace;ds.frontFaceStencil=sd;ds.backFaceStencil=sd;auto state=[d newDepthStencilStateWithDescriptor:ds];
 MTLTextureDescriptor *td[4];NSUInteger offsets[4];NSUInteger total=0;
 for(unsigned i=0;i<4;i++){
  td[i]=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:formats[i] width:16 height:16 mipmapped:NO];td[i].resourceOptions=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;
  td[i].usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead|(i<3?MTLTextureUsageShaderWrite:0);
  const auto layout=[d heapTextureSizeAndAlignWithDescriptor:td[i]];offsets[i]=(total+layout.align-1)/layout.align*layout.align;total=offsets[i]+layout.size;
 }
 auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=MAX(total,NSUInteger(65536));auto heap=[d newHeapWithDescriptor:hd];if(!heap)return 4;
 RENDERDOC_API_1_7_0 *api=nullptr;auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(get)get(eRENDERDOC_API_Version_1_7_0,(void **)&api);
 id<MTLTexture>images[4]={nil,nil,nil,nil};
 if(initialColor){images[0]=[heap newTextureWithDescriptor:td[0] offset:offsets[0]];images[0].label=@"Frame Load target 0";auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=images[0];pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,0);pass.colorAttachments[0].storeAction=MTLStoreActionStore;auto cb=[q commandBuffer];auto e=[cb renderCommandEncoderWithDescriptor:pass];[e endEncoding];[cb commit];[cb waitUntilCompleted];if(cb.status!=MTLCommandBufferStatusCompleted)return 12;}
 if(api){RENDERDOC_AnnotationValue v={};v.uint32=65;if(api->SetObjectAnnotation((__bridge void *)d,(__bridge void *)d,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v))return 5;api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);}
 for(unsigned i=initialColor?1:0;i<4;i++){images[i]=[heap newTextureWithDescriptor:td[i] offset:offsets[i]];if(!images[i])return 6;images[i].label=[NSString stringWithFormat:@"Frame Load target %u",i];}
 const bool discard=strcmp(argv[3],"discard")==0;if(!discard && !initialColor && strcmp(argv[3],"load"))return 7;
 for(unsigned i=0;i<2;i++){
  auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
  for(unsigned j=0;j<3;j++){pass.colorAttachments[j].texture=images[j];pass.colorAttachments[j].loadAction=(!i && discard)?MTLLoadActionDontCare:MTLLoadActionLoad;pass.colorAttachments[j].storeAction=MTLStoreActionUnknown;}
  pass.depthAttachment.texture=images[3];pass.depthAttachment.loadAction=(!i && discard)?MTLLoadActionDontCare:MTLLoadActionLoad;pass.depthAttachment.storeAction=MTLStoreActionUnknown;
  pass.stencilAttachment.texture=images[3];pass.stencilAttachment.loadAction=pass.depthAttachment.loadAction;pass.stencilAttachment.storeAction=MTLStoreActionUnknown;
  auto cb=[q commandBuffer];auto e=[cb renderCommandEncoderWithDescriptor:pass];[e setRenderPipelineState:pipelines[i]];[e setDepthStencilState:state];[e setStencilReferenceValue:i?42:7];[e setViewport:MTLViewport{0,0,16,16,0,1}];[e setCullMode:MTLCullModeNone];
  [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];for(unsigned j=0;j<3;j++)[e setColorStoreAction:MTLStoreActionStore atIndex:j];[e setDepthStoreAction:MTLStoreActionStore];[e setStencilStoreAction:MTLStoreActionStore];[e endEncoding];[cb commit];[cb waitUntilCompleted];if(cb.status!=MTLCommandBufferStatusCompleted)return 8;
 }
 if(api && !api->EndFrameCapture(nullptr,nullptr))return 9;
 // Verify only fully overwritten pixels; the pre-draw first Load is undefined.
 const uint8_t colors[2][4]={{191,128,64,255},{128,191,64,255}};const uint32_t packed=(15U<<6)|((14U<<6)<<11)|((13U<<5)<<22);
 for(unsigned i=0;i<5;i++){
  auto read=[d newBufferWithLength:4096 options:MTLResourceStorageModeShared];auto cb=[q commandBuffer];auto e=[cb blitCommandEncoder];const unsigned target=i<3?i:3;
  if(i<3)[e copyFromTexture:images[target] sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(16,16,1) toBuffer:read destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:4096];
  else [e copyFromTexture:images[target] sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(16,16,1) toBuffer:read destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:4096 options:i==3?MTLBlitOptionDepthFromDepthStencil:MTLBlitOptionStencilFromDepthStencil];
  [e endEncoding];[cb commit];[cb waitUntilCompleted];if(cb.status!=MTLCommandBufferStatusCompleted)return 10;
  for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++){
   const auto pixel=(const uint8_t *)read.contents+y*256+x*(i==4?1:initialColor&&i==0?initialColor->stride:4);bool good=initialColor&&i==0?!memcmp(pixel,initialColor->second,initialColor->stride):i<2?!memcmp(pixel,colors[i],4):i==2?*(const uint32_t *)pixel==packed:i==3?*(const float *)pixel==.75f:*pixel==42;
   if(!good){fprintf(stderr,"Native pixel mismatch target=%u y=%u x=%u value=%08x\n",i,y,x,*(const uint32_t *)pixel);return 11;}
  }
 }
 printf("PASS Native frame %s/Load MRT/depth/stencil, deferred stores, two submissions, 256 pixels per plane\n",argv[3]);return 0;
}}
