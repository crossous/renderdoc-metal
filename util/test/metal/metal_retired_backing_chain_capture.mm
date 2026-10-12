// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "renderdoc/api/app/renderdoc_app.h"

int main(int argc, char **argv)
{
  if(argc != 3) return 1;
  const NSUInteger unit=strtoull(argv[2],nullptr,10);
  if(unit<256 || unit>4096 || unit%256) return 1;
  @autoreleasepool {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue=[device newCommandQueue];
    const auto options=MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked;
    auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.size=12*unit;
    hd.storageMode=MTLStorageModeShared;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
    id<MTLHeap> heap=[device newHeapWithDescriptor:hd];
    id<MTLBuffer> source=[device newBufferWithLength:64 options:options];
    id<MTLBuffer> reads[3];
    for(unsigned i=0;i<3;i++) { reads[i]=[device newBufferWithLength:4*unit options:options]; if(!reads[i]) return 2; memset(reads[i].contents,0,4*unit); }
    if(!heap || !source || !queue) return 2;
    memset(source.contents,0,64);
    RENDERDOC_API_1_7_0 *api=nullptr;
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    if(get && get(eRENDERDOC_API_Version_1_7_0,(void**)&api)!=1) return 3;
    auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
      if(!api) return uint32_t(0);
      RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void*)device,(__bridge void*)object,key,eRENDERDOC_UInt64,4,&v);
    };
    if(api) {
      RENDERDOC_AnnotationValue v={};v.uint32=65;
      if(api->SetObjectAnnotation((__bridge void*)device,(__bridge void*)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v)) return 3;
      api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);
    }
    id<MTLBuffer> a=[heap newBufferWithLength:8*unit options:options offset:0];
    if(!a) return 4;
    memset(a.contents,0x31,8*unit);uint64_t packet[3]={source.gpuAddress,0,0};memcpy(a.contents,packet,24);
    if(annotation(a,"metal.descriptorTable",1,0,1,24) ||
       annotation(a,"metal.descriptorSlotEvent",0,7,0,0) ||
       annotation(a,"metal.descriptorSlotEvent",0,7,2,0) ||
       annotation(a,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void*)source,0)) return 5;
    auto first=[queue commandBuffer];auto e1=[first blitCommandEncoder];
    [e1 copyFromBuffer:a sourceOffset:4*unit toBuffer:reads[0] destinationOffset:0 size:4*unit];
    [e1 endEncoding];[first commit];[first waitUntilCompleted];
    if(first.error || first.status!=MTLCommandBufferStatusCompleted) return 6;
    if(annotation(a,"metal.descriptorSlotEvent",0,7,1,0)) return 5;
    // B and C are disjoint, but each overlaps a different part of retained A.
    // A's logical packet is retired; the Native object remains alive throughout.
    id<MTLBuffer> b=[heap newBufferWithLength:4*unit options:options offset:0];
    id<MTLBuffer> c=[heap newBufferWithLength:4*unit options:options offset:6*unit];
    if(!b || !c) return 7;
    memset(b.contents,0x52,4*unit);memset(c.contents,0x73,4*unit);
    auto second=[queue commandBuffer];auto e2=[second blitCommandEncoder];
    [e2 copyFromBuffer:b sourceOffset:0 toBuffer:reads[1] destinationOffset:0 size:4*unit];
    [e2 copyFromBuffer:c sourceOffset:0 toBuffer:reads[2] destinationOffset:0 size:4*unit];
    [e2 endEncoding];[second commit];[second waitUntilCompleted];
    if(second.error || second.status!=MTLCommandBufferStatusCompleted) return 8;
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 9;
    for(unsigned k=0;k<3;k++) for(NSUInteger i=0;i<4*unit;i++)
      if(((unsigned char*)reads[k].contents)[i]!=(k==0?0x31:k==1?0x52:0x73)) return 10;
    printf("PASS Native retired backing chain unit=%lu full three outputs=%lu retained A and disjoint B/C\n",(unsigned long)unit,(unsigned long)(12*unit));
    return 0;
  }
}
