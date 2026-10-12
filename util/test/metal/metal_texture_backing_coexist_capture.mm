// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc,char **argv) {
 if(argc!=4)return 1;const NSUInteger side=strtoull(argv[3],nullptr,10);if(side!=64&&side!=256)return 1;
 @autoreleasepool {
 auto device=MTLCreateSystemDefaultDevice();auto queue=[device newCommandQueue];
 auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Uint width:side height:side mipmapped:NO];
 td.storageMode=MTLStorageModePrivate;td.hazardTrackingMode=MTLHazardTrackingModeTracked;td.usage=MTLTextureUsageShaderRead;
 auto layout=[device heapTextureSizeAndAlignWithDescriptor:td];auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.size=layout.size+layout.align;hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
 auto heap=[device newHeapWithDescriptor:hd];auto texture=[heap newTextureWithDescriptor:td offset:0];
 const NSUInteger bytes=side*side*4,pitch=side*4;
 auto upload=[device newBufferWithLength:bytes options:MTLResourceStorageModeShared];auto image=[device newBufferWithLength:bytes options:MTLResourceStorageModeShared];auto result=[device newBufferWithLength:4096 options:MTLResourceStorageModeShared];
 auto table=[device newBufferWithLength:256 options:MTLResourceStorageModeShared];
 if(!heap||!texture||!upload||!image||!result||!table||!queue)return 2;
 memset(upload.contents,0x35,bytes);memset(image.contents,0x6a,bytes);memset(result.contents,0x6a,4096);memset(table.contents,0,256);
 uint64_t packet[3]={0,texture.gpuResourceID._impl,0};memcpy(table.contents,packet,24);
 RENDERDOC_API_1_7_0 *api=nullptr;auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(get&&!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 3;
 auto annotation=[&](const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d){if(!api)return 0u;RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;return api->SetObjectAnnotation((__bridge void*)device,(__bridge void*)table,key,eRENDERDOC_UInt64,4,&v);};
 if(api){RENDERDOC_AnnotationValue coverage={};coverage.uint32=65;if(api->SetObjectAnnotation((__bridge void*)device,(__bridge void*)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage))return 3;
 if(annotation("metal.descriptorTable",1,0,1,24)||annotation("metal.descriptorSlotEvent",0,7,0,4)||annotation("metal.descriptorSlotEvent",0,7,2,4)||annotation("metal.descriptorSlotBinding",0,1,(uint64_t)(__bridge void*)texture,0))return 3;}
 auto init=[queue commandBuffer];auto e=[init blitCommandEncoder];[e copyFromBuffer:upload sourceOffset:0 sourceBytesPerRow:pitch sourceBytesPerImage:bytes sourceSize:MTLSizeMake(side,side,1) toTexture:texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[e endEncoding];[init commit];[init waitUntilCompleted];if(init.error)return 4;
 image.label=@"Native texture before alias";result.label=@"Native buffer after alias";
 if(api){api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);}
 auto first=[queue commandBuffer];auto a=[first blitCommandEncoder];[a copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(side,side,1) toBuffer:image destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:bytes];[a endEncoding];[first commit];[first waitUntilCompleted];if(first.error)return 5;
 // Creation does not read or overwrite either physical interpretation. Retire
 // the actual logical texture slot before the overlapping buffer is written.
 auto buffer=[heap newBufferWithLength:4096 options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:0];if(!buffer)return 6;
 auto retained=[queue commandBuffer];auto read=[retained blitCommandEncoder];[read copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(side,side,1) toBuffer:image destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:bytes];[read endEncoding];[retained commit];[retained waitUntilCompleted];if(retained.error)return 7;
 if(annotation("metal.descriptorSlotEvent",0,7,1,4))return 3;
 auto second=[queue commandBuffer];auto b=[second blitCommandEncoder];[b fillBuffer:buffer range:NSMakeRange(0,4096) value:0x52];[b copyFromBuffer:buffer sourceOffset:0 toBuffer:result destinationOffset:0 size:4096];[b endEncoding];[second commit];[second waitUntilCompleted];if(second.error)return 7;
 if(api&&!api->EndFrameCapture(nullptr,nullptr))return 8;
 for(NSUInteger i=0;i<bytes;i++)if(((unsigned char*)image.contents)[i]!=0x35)return 9;
 for(NSUInteger i=0;i<4096;i++)if(((unsigned char*)result.contents)[i]!=0x52)return 9;
 FILE *f=fopen(argv[2],"wbx");if(!f)return 10;bool good=fwrite(image.contents,1,bytes,f)==bytes&&fwrite(result.contents,1,4096,f)==4096;fclose(f);
 printf("Native MATCH side=%lu texture-footprint=%lu complete bytes=%lu; live texture slot at buffer creation, retirement before write\n",side,layout.size,bytes+4096);return good?0:10;
 }
}
