// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vector>
#include <algorithm>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc,char **argv)
{
  if(argc!=7)return 1;
  const bool source=!strcmp(argv[2],"source"),volume=atoi(argv[5])!=0;
  const unsigned offset=atoi(argv[6]);if(!offset || offset>128)return 1;
  const bool groups=getenv("RENDERDOC_METAL_TEST_GROUPS")!=nullptr;
  const unsigned width=volume?(groups?8:5):(groups?64:67),height=volume?(groups?6:7):1,
      depth=volume?(groups?4:3):1,count=width*height*depth;
  const unsigned capacity=512;
  const bool dynamic=getenv("RENDERDOC_METAL_TEST_DYNAMIC_MEMORY")!=nullptr;
  @autoreleasepool
  {
    auto device=MTLCreateSystemDefaultDevice();NSError *error=nil;
    if(![device supportsFamily:MTLGPUFamilyApple4] && ![device supportsFamily:MTLGPUFamilyMac2])return 2;
    auto library=source?[device newLibraryWithSource:[NSString stringWithContentsOfFile:
        [NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&error] options:nil error:&error]:
        [device newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[1]]] error:&error];
    auto pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:
        dynamic?@"dynamic_grid":@"exact_grid"] error:&error];
    if(!pipeline){fprintf(stderr,"%s\n",error.description.UTF8String);return 3;}
    std::vector<uint32_t> input(capacity),expected(capacity+offset+16,0x6a6a6a6aU);
    for(unsigned i=0;i<capacity;i++)input[i]=i*23+7;
    const uint32_t parameters[]={width,height,depth,offset};
    auto inputs=[device newBufferWithBytes:input.data() length:input.size()*4 options:MTLResourceStorageModeShared];
    auto outputs=[device newBufferWithBytes:expected.data() length:expected.size()*4 options:MTLResourceStorageModeShared];
    auto params=[device newBufferWithBytes:parameters length:sizeof(parameters) options:MTLResourceStorageModeShared];
    outputs.label=@"Exact grid output";
    const uint64_t packet[]={inputs.gpuAddress,0,inputs.length};
    auto table=[device newBufferWithBytes:packet length:sizeof(packet) options:MTLResourceStorageModeShared];
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");RENDERDOC_API_1_7_0 *api=nullptr;
    if(get && get(eRENDERDOC_API_Version_1_7_0,(void **)&api)!=1)return 4;
    if(api)
    {
      RENDERDOC_AnnotationValue coverage={};coverage.uint32=65;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage))return 4;
      auto annotate=[&](const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
        RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=a;value.vector.uint64[1]=b;
        value.vector.uint64[2]=c;value.vector.uint64[3]=d;
        return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)table,key,eRENDERDOC_UInt64,4,&value);
      };
      if(annotate("metal.descriptorTable",2,0,1,24) || annotate("metal.descriptorSlotEvent",0,1,0,4) ||
         annotate("metal.descriptorSlotEvent",0,1,2,4) ||
         annotate("metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)inputs,0))return 4;
      api->SetCaptureFilePathTemplate(argv[3]);api->StartFrameCapture(nullptr,nullptr);
    }
    auto queue=[device newCommandQueue];auto command=[queue commandBuffer];
    const unsigned encoders=getenv("RENDERDOC_METAL_TEST_TWO_ENCODERS")?2:1;
    for(unsigned pass=0;pass<encoders;pass++)
    {
    auto encoder=[command computeCommandEncoder];
    [encoder setComputePipelineState:pipeline];[encoder setBuffer:inputs offset:0 atIndex:4];
    [encoder setBuffer:outputs offset:0 atIndex:9];[encoder setBuffer:params offset:0 atIndex:2];
    [encoder setBuffer:table offset:0 atIndex:30];
    if(dynamic)
    {
      [encoder setThreadgroupMemoryLength:256 atIndex:3];
      [encoder setThreadgroupMemoryLength:128 atIndex:3];
      [encoder setThreadgroupMemoryLength:128 atIndex:7];
    }
    const MTLSize localSize=volume?MTLSizeMake(4,3,2):MTLSizeMake(32,1,1);
    if(groups)[encoder dispatchThreadgroups:MTLSizeMake(width/localSize.width,
        height/localSize.height,depth/localSize.depth) threadsPerThreadgroup:localSize];
    else [encoder dispatchThreads:MTLSizeMake(width,height,depth) threadsPerThreadgroup:localSize];
    [encoder endEncoding];
    }
    [command commit];[command waitUntilCompleted];
    if(command.status!=MTLCommandBufferStatusCompleted || command.error)return 5;
    if(api && !api->EndFrameCapture(nullptr,nullptr))return 6;
    for(unsigned z=0;z<depth;z++)for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)
    {
      const unsigned i=x+width*(y+height*z);
      if(!dynamic){expected[offset+i]=input[i]*7+i*11+101;continue;}
      const unsigned gx=volume?4:32,gy=volume?3:1,gz=volume?2:1;
      const unsigned bx=x/gx*gx,by=y/gy*gy,bz=z/gz*gz;
      const unsigned ax=std::min(gx,width-bx),ay=std::min(gy,height-by),az=std::min(gz,depth-bz);
      const unsigned lane=(x-bx)+ax*((y-by)+ay*(z-bz));
      const unsigned next=(lane+1)%(ax*ay*az);
      const unsigned neighbour=(bx+next%ax)+width*((by+next/ax%ay)+height*(bz+next/(ax*ay)));
      const unsigned base=bx+width*(by+height*bz);
      expected[offset+i]=input[neighbour]*7+i*11+input[i]+101+input[base]%13;
    }
    if(memcmp(outputs.contents,expected.data(),expected.size()*4))return 7;
    FILE *raw=fopen(argv[4],"wb");if(!raw)return 8;
    bool saved=fwrite(outputs.contents,4,expected.size(),raw)==expected.size();fclose(raw);if(!saved)return 8;
    printf("Native full output/padding MATCH: grid=%u,%u,%u; nonuniform edge; source=%d; dynamic_memory=%d static_memory=%lu max_memory=%lu; device=%s\n",width,height,depth,source,dynamic,(unsigned long)pipeline.staticThreadgroupMemoryLength,(unsigned long)device.maxThreadgroupMemoryLength,device.name.UTF8String);
  }
}
