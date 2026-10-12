// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vector>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc,char **argv)
{
  if(argc != 9)return 1;
  const bool source = !strcmp(argv[2],"source"), active = !strcmp(argv[3],"active");
  const unsigned count=atoi(argv[5]),offset=atoi(argv[6]),slot=atoi(argv[7]),bytes=atoi(argv[8]);
  if((count!=64 && count!=1024) || !offset || slot<3 || slot>=31 ||
     !bytes || bytes>4096 || bytes%8)return 1;
  @autoreleasepool
  {
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();NSError *error=nil;
    id<MTLLibrary> library=source?
      [device newLibraryWithSource:[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]
        encoding:NSUTF8StringEncoding error:&error] options:nil error:&error]:
      [device newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[1]]] error:&error];
    auto pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:
        active?@"active_address":@"ordinary_direct"] error:&error];
    if(!pipeline){fprintf(stderr,"%s\n",error.description.UTF8String);return 2;}
    std::vector<uint32_t> inputs(count),initial(count+offset+16,0x6a6a6a6aU);
    for(unsigned i=0;i<count;i++)inputs[i]=i*23+7;
    const uint32_t parameters[3]={count,offset,101};
    auto input=[device newBufferWithBytes:inputs.data() length:inputs.size()*4 options:MTLResourceStorageModeShared];
    auto output=[device newBufferWithBytes:initial.data() length:initial.size()*4 options:MTLResourceStorageModeShared];
    auto params=[device newBufferWithBytes:parameters length:sizeof(parameters) options:MTLResourceStorageModeShared];
    output.label=@"Unused inline output";
    std::vector<uint64_t> unused(bytes/8,0xfedcba9876543210ULL);
    unused[0]=input.gpuAddress;
    // Keep the existing sourced-capture protocol separate from the shader
    // consumer. This valid table is bound but unused by the Native pipeline.
    const uint64_t packet[3]={input.gpuAddress,0,input.length};
    auto table=[device newBufferWithBytes:packet length:sizeof(packet) options:MTLResourceStorageModeShared];
    auto queue=[device newCommandQueue];
    RENDERDOC_API_1_7_0 *api=nullptr;auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    if(get && get(eRENDERDOC_API_Version_1_7_0,(void **)&api)!=1)return 3;
    if(api)
    {
      RENDERDOC_AnnotationValue coverage={};coverage.uint32=65;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,
          "metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage))return 3;
      auto annotate=[&](const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
        RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=a;value.vector.uint64[1]=b;
        value.vector.uint64[2]=c;value.vector.uint64[3]=d;
        return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)table,key,eRENDERDOC_UInt64,4,&value);
      };
      if(annotate("metal.descriptorTable",2,0,1,24) ||
         annotate("metal.descriptorSlotEvent",0,1,0,4) ||
         annotate("metal.descriptorSlotEvent",0,1,2,4) ||
         annotate("metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)input,0))return 3;
      api->SetCaptureFilePathTemplate(argv[4]);api->StartFrameCapture(nullptr,nullptr);
    }
    auto command=[queue commandBuffer];auto encoder=[command computeCommandEncoder];
    [encoder setComputePipelineState:pipeline];
    if(active)
    {
      const uint64_t address=input.gpuAddress;[encoder setBytes:&address length:8 atIndex:0];
      [encoder useResource:input usage:MTLResourceUsageRead];
    }
    else [encoder setBuffer:input offset:0 atIndex:0];
    [encoder setBuffer:output offset:0 atIndex:1];[encoder setBuffer:params offset:0 atIndex:2];
    [encoder setBuffer:table offset:0 atIndex:30];
    [encoder setBytes:unused.data() length:bytes atIndex:slot];
    if(getenv("RENDERDOC_METAL_TEST_THREADS"))
      [encoder dispatchThreads:MTLSizeMake(count,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
    else
      [encoder dispatchThreadgroups:MTLSizeMake(count/32,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
    [encoder endEncoding];[command commit];[command waitUntilCompleted];
    if(command.status!=MTLCommandBufferStatusCompleted || command.error)return 4;
    if(api && !api->EndFrameCapture(nullptr,nullptr))return 5;
    for(unsigned t=0;t<count;t++)initial[offset+(t*17)%count]=inputs[(t*13)%count]+101;
    if(memcmp(output.contents,initial.data(),initial.size()*4))return 6;
    printf("Native output MATCH: source=%d active_address=%d count=%u offset=%u unused_slot=%u bytes=%u; full output/padding\n",
        source,active,count,offset,slot,bytes);
  }
}
