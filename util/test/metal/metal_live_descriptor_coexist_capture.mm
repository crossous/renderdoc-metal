// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
// No shader/pass identity is involved in backend qualification. A live typed
// packet and a newly created ordinary physical view coexist; both remain owned.
int main(int argc, char **argv)
{
  if(argc!=5)return 1;
  unsigned mode=atoi(argv[2]), sourceOffset=atoi(argv[3]), tableRows=atoi(argv[4]);
  if(mode>6 || sourceOffset>=32 || !tableRows || tableRows>16)return 1;
  @autoreleasepool {
    auto device=MTLCreateSystemDefaultDevice();NSError *error=nil;
    auto library=[device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Entry { device const uint *value [[id(0)]]; ulong unused [[id(1)]]; ulong scalar [[id(2)]]; };
kernel void consume(const device ulong *root [[buffer(0)]], device uint *out [[buffer(1)]], constant uint &at [[buffer(2)]])
{ const device Entry *table=reinterpret_cast<const device Entry *>(root[0]);out[at]=table->value[0]+uint(table->scalar);out[at+1]=0x5a17U; }
)MSL" options:nil error:&error];
    MTLComputePipelineDescriptor *pd=[MTLComputePipelineDescriptor new];pd.computeFunction=[library newFunctionWithName:@"consume"];
    auto pipeline=[device newComputePipelineStateWithDescriptor:pd options:MTLPipelineOptionArgumentInfo|MTLPipelineOptionBufferTypeInfo reflection:nil error:&error];
    if(!pipeline){fprintf(stderr,"%s\n",error.description.UTF8String);return 2;}
    uint32_t values[32];for(unsigned i=0;i<32;i++)values[i]=i*37+11;
    uint32_t zeros[8]={};auto source=[device newBufferWithBytes:values length:sizeof(values) options:MTLResourceStorageModeShared];
    id<MTLHeap> sourceHeap=nil;
    if(mode>=5){
      const auto sourceOptions=(mode==6?MTLResourceStorageModePrivate:MTLResourceStorageModeShared)|MTLResourceHazardTrackingModeTracked;
      auto layout=[device heapBufferSizeAndAlignWithLength:sizeof(values) options:sourceOptions];
      auto desc=[MTLHeapDescriptor new];desc.type=MTLHeapTypePlacement;desc.storageMode=mode==6?MTLStorageModePrivate:MTLStorageModeShared;desc.hazardTrackingMode=MTLHazardTrackingModeTracked;desc.size=layout.align*3+layout.size;
      sourceHeap=[device newHeapWithDescriptor:desc];source=[sourceHeap newBufferWithLength:sizeof(values) options:sourceOptions offset:layout.align*3];if(!source)return 3;
      if(mode==6){auto staging=[device newBufferWithBytes:values length:sizeof(values) options:MTLResourceStorageModeShared];auto initQueue=[device newCommandQueue];auto init=[initQueue commandBuffer];auto blit=[init blitCommandEncoder];[blit copyFromBuffer:staging sourceOffset:0 toBuffer:source destinationOffset:0 size:sizeof(values)];[blit endEncoding];[init commit];[init waitUntilCompleted];if(init.error)return 3;}
      else memcpy(source.contents,values,sizeof(values));
    }
    source.label=@"Live coexist source";
    auto output=[device newBufferWithBytes:zeros length:sizeof(zeros) options:MTLResourceStorageModeShared];output.label=@"Live coexist output";
    auto scratch=[device newBufferWithLength:64 options:MTLResourceStorageModeShared];memset(scratch.contents,0xa7,64);
    auto options=MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked;
    auto size=[device heapBufferSizeAndAlignWithLength:tableRows*24 options:options];
    auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModeShared;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=size.align*3+size.size;
    auto heap=[device newHeapWithDescriptor:hd];auto table=[heap newBufferWithLength:tableRows*24 options:options offset:size.align*3];
    if(!table)return 3;memset(table.contents,0,table.length);
    uint64_t packet[]={source.gpuAddress+sourceOffset*4,0,53};memcpy(table.contents,packet,24);
    char initialPath[4096];snprintf(initialPath,sizeof(initialPath),"%s.input.bin",argv[1]);FILE *initial=fopen(initialPath,"wb");if(!initial)return 11;fwrite(values,1,sizeof(values),initial);fclose(initial);
    RENDERDOC_API_1_7_0 *api=nullptr;auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(get && get(eRENDERDOC_API_Version_1_7_0,(void**)&api)!=1)return 4;
    auto annotate=[&](id object,const char *name,uint64_t a,uint64_t b,uint64_t c,uint64_t d){
      if(!api)return 0U;RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void*)device,(__bridge void*)object,name,eRENDERDOC_UInt64,4,&v);
    };
    if(api){RENDERDOC_AnnotationValue v={};v.uint32=65;
      if(api->SetObjectAnnotation((__bridge void*)device,(__bridge void*)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v))return 5;
      api->SetCaptureFilePathTemplate(argv[1]);
    }
    if(annotate(table,"metal.descriptorTable",2,0,tableRows,24) ||
       annotate(table,"metal.descriptorSlotEvent",0,1,0,0) || annotate(table,"metal.descriptorSlotEvent",0,1,2,0) ||
       annotate(table,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void*)source,sourceOffset*4))return 5;
    const uint32_t positions[]={0,2};auto params=[device newBufferWithBytes:positions length:8 options:MTLResourceStorageModeShared];
    auto queue=[device newCommandQueue];if(api)api->StartFrameCapture(nullptr,nullptr);
    auto first=[queue commandBuffer];[first enqueue];
    auto dispatch=[&](id<MTLCommandBuffer> command,unsigned at){
      auto encoder=[command computeCommandEncoder];[encoder setComputePipelineState:pipeline];
      const uint64_t root=table.gpuAddress;
      if(annotate(encoder,"metal.descriptorInlineLayout",0,0,1,8) || annotate(encoder,"metal.descriptorInlineBinding",0,0,(uint64_t)(__bridge void*)table,0))abort();
      [encoder setBytes:&root length:8 atIndex:0];[encoder setBuffer:output offset:0 atIndex:1];[encoder setBuffer:params offset:at?4:0 atIndex:2];[encoder useResource:table usage:MTLResourceUsageRead];
      [encoder useResource:source usage:MTLResourceUsageRead];[encoder dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[encoder endEncoding];
    };
    dispatch(first,0);
    if(mode==2){[first commit];[first waitUntilCompleted];if(first.error)return 6;}
    id<MTLBuffer> sourceWriter=source;
    if(mode>=5){sourceWriter=[sourceHeap newBufferWithLength:source.length options:(mode==6?MTLResourceStorageModePrivate:MTLResourceStorageModeShared)|MTLResourceHazardTrackingModeTracked offset:source.heapOffset];if(!sourceWriter || sourceWriter.gpuAddress!=source.gpuAddress)return 7;}
    if(mode>=3){auto otherQueue=[device newCommandQueue];auto other=[otherQueue commandBuffer];auto b=[other blitCommandEncoder];[b fillBuffer:mode>=4?sourceWriter:scratch range:mode>=4?NSMakeRange(sourceOffset*4,4):NSMakeRange(7,19) value:0x35];[b endEncoding];[other commit];[other waitUntilCompleted];if(other.error)return 6;}
    id<MTLBuffer> alias=nil;
    if(mode){alias=[heap newBufferWithLength:table.length options:options offset:table.heapOffset];if(!alias || alias.gpuAddress!=table.gpuAddress)return 7;}
    if(mode!=2){[first commit];[first waitUntilCompleted];if(first.error)return 6;}
    // A legal ordinary scalar publication through the new view. Identity lanes
    // stay unchanged; the old typed object is consumed again after publication.
    const uint64_t scalar=79;memcpy((char*)(alias?alias:table).contents+16,&scalar,8);
    auto second=[queue commandBuffer];[second enqueue];dispatch(second,2);[second commit];[second waitUntilCompleted];
    if(second.error)return 8;
    auto words=(uint32_t*)output.contents;
    const uint32_t consumed=mode>=4?0x35353535U:values[sourceOffset];
    if(words[0]!=consumed+53 || words[1]!=0x5a17 || words[2]!=consumed+79 || words[3]!=0x5a17)return 9;
    if(api && !api->EndFrameCapture(nullptr,nullptr))return 10;
    char path[4096];snprintf(path,sizeof(path),"%s.output.bin",argv[1]);FILE *f=fopen(path,"wb");if(!f)return 11;fwrite(words,1,32,f);fclose(f);
    printf("Native GPU COMPLETED output MATCH mode=%u offset=%u rows=%u values=%u/%u retainedVA=%llu\n",mode,sourceOffset,tableRows,words[0],words[2],(unsigned long long)packet[0]);
  }
  return 0;
}
