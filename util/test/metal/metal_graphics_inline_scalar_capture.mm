// SPDX-License-Identifier: MIT
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "renderdoc/api/app/renderdoc_app.h"
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
struct Payload { uint64_t ordinary,salt;uint32_t offset,count;uint64_t extra,tail; };
int main(int argc,char **argv) { if(argc!=6)return 1;@autoreleasepool {
 auto device=MTLCreateSystemDefaultDevice();auto queue=[device newCommandQueue];NSError *err=nil;
 const bool source=[[NSString stringWithUTF8String:argv[1]] hasSuffix:@".metal"];
 auto lib=source?[device newLibraryWithSource:[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&err] options:nil error:&err]:[device newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[1]]] error:&err];
 const bool wide=atoi(argv[4]), layout=atoi(argv[5]), pointer=getenv("RENDERDOC_METAL_TEST_GRAPHICS_POINTER")!=nullptr;
 auto pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[lib newFunctionWithName:wide?@"wide_v":@"scalar_v"];
 pd.fragmentFunction=[lib newFunctionWithName:wide?@"wide_f":@"scalar_f"];pd.colorAttachments[0].pixelFormat=wide?MTLPixelFormatRGBA16Float:MTLPixelFormatBGRA8Unorm;
 auto pipeline=[device newRenderPipelineStateWithDescriptor:pd error:&err];id<MTLRenderPipelineState> pointerPipeline=nil;
 if(pointer){pd.vertexFunction=[lib newFunctionWithName:@"pointer_v"];pointerPipeline=[device newRenderPipelineStateWithDescriptor:pd error:&err];}
 if(!pipeline || (pointer && !pointerPipeline)){fprintf(stderr,"%s\n",err.description.UTF8String);return 2;}
 const float points[]={-1,-1,3,-1,-1,3};auto geometry=[device newBufferWithBytes:points length:sizeof(points) options:MTLResourceStorageModeShared];
 uint64_t packet[]={geometry.gpuAddress,0,geometry.length};auto table=[device newBufferWithBytes:packet length:24 options:MTLResourceStorageModeShared];
 unsigned width=wide?37:19,height=wide?23:11,bpp=wide?8:4;
 auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pd.colorAttachments[0].pixelFormat width:width height:height mipmapped:NO];
 td.storageMode=MTLStorageModePrivate;td.usage=MTLTextureUsageRenderTarget;auto image=[device newTextureWithDescriptor:td];image.label=@"Graphics inline full output";
 unsigned pitch=(width*bpp+255)&~255u;auto staging=[device newBufferWithLength:pitch*height options:MTLResourceStorageModeShared];
 auto save=[&](unsigned step) {auto c=[queue commandBuffer];auto b=[c blitCommandEncoder];
 [b copyFromTexture:image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1) toBuffer:staging destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:pitch*height];
 [b endEncoding];[c commit];[c waitUntilCompleted];if(c.status!=MTLCommandBufferStatusCompleted)return false;
 char path[4096];snprintf(path,sizeof(path),"%s.%u.bin",argv[3],step);FILE*f=fopen(path,"wb");if(!f)return false;
 for(unsigned y=0;y<height;y++)if(fwrite((char*)staging.contents+y*pitch,1,width*bpp,f)!=width*bpp)return false;return fclose(f)==0;};
 auto pass=[&](bool clear) {auto p=[MTLRenderPassDescriptor renderPassDescriptor];p.colorAttachments[0].texture=image;p.colorAttachments[0].loadAction=clear?MTLLoadActionClear:MTLLoadActionLoad;p.colorAttachments[0].storeAction=MTLStoreActionStore;p.colorAttachments[0].clearColor=MTLClearColorMake(.125,.25,.5,1);return p;};
 auto warm=[queue commandBuffer];auto e=[warm renderCommandEncoderWithDescriptor:pass(true)];[e endEncoding];[warm commit];[warm waitUntilCompleted];if(!save(0))return 3;
 RENDERDOC_API_1_7_0 *api=nullptr;auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(get)get(eRENDERDOC_API_Version_1_7_0,(void**)&api);
 auto ann=[&](id object,const char*key,uint64_t a,uint64_t b,uint64_t c,uint64_t d){if(!api)return 0U;RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;return api->SetObjectAnnotation((__bridge void*)device,(__bridge void*)object,key,eRENDERDOC_UInt64,4,&v);};
 if(api){RENDERDOC_AnnotationValue v={};v.uint32=67;if(api->SetObjectAnnotation((__bridge void*)device,(__bridge void*)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v)||ann(table,"metal.descriptorTable",2,0,1,24)||ann(table,"metal.descriptorSlotEvent",0,1,0,4)||ann(table,"metal.descriptorSlotEvent",0,1,2,4)||ann(table,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void*)geometry,0))return 4;api->SetCaptureFilePathTemplate(argv[2]);api->StartFrameCapture(nullptr,nullptr);}
 for(unsigned step=0;step<2;step++) {auto c=[queue commandBuffer];auto e=[c renderCommandEncoderWithDescriptor:pass(false)];
 [e setRenderPipelineState:pointer && step?pointerPipeline:pipeline];[e setVertexBuffer:table offset:0 atIndex:30];[e useResource:geometry usage:MTLResourceUsageRead];
 Payload p={geometry.gpuAddress,0x1234567800000041ULL+step,0,50+step*100,0x2345678800000023ULL,13};
 if(pointer && step){
 // A prior ordinary binding in the same encoder must not exempt the rebound pointer.
 [e setRenderPipelineState:pipeline];if(ann(e,"metal.descriptorInlineLayout",1,2,0,wide?40:24))return 5;[e setVertexBytes:&p length:wide?40:24 atIndex:2];[e setRenderPipelineState:pointerPipeline];
 if((getenv("RENDERDOC_METAL_TEST_GRAPHICS_POINTER_SOURCE")||getenv("RENDERDOC_METAL_TEST_GRAPHICS_LAYOUT_ONLY"))&&ann(e,"metal.descriptorInlineLayout",1,2,1,8))return 5;
 if(getenv("RENDERDOC_METAL_TEST_GRAPHICS_POINTER_SOURCE")&&ann(e,"metal.descriptorInlineBinding",(1ULL<<32)|2,0,(uint64_t)(__bridge void*)geometry,0))return 5;
 uint64_t address=geometry.gpuAddress;[e setVertexBytes:&address length:8 atIndex:2];}
 else {if(layout && ann(e,"metal.descriptorInlineLayout",1,2,0,wide?40:24))return 5;[e setVertexBytes:&p length:wide?40:24 atIndex:2];}
 if(layout && ann(e,"metal.descriptorInlineLayout",2,7,0,wide?40:24))return 5;[e setFragmentBytes:&p length:wide?40:24 atIndex:7];
 [e setScissorRect:MTLScissorRect{step?width/2:0,0,step?width-width/2:width/2,height}];[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[e endEncoding];[c commit];[c waitUntilCompleted];if(c.status!=MTLCommandBufferStatusCompleted||c.error)return 6;if(!save(step+1))return 7;
 }
 if(api&&!api->EndFrameCapture(nullptr,nullptr))return 8;printf("Native full image %u B, two draws/independent command submissions; layout=%d pointer=%d\n",width*height*bpp,layout,pointer);
 }}
