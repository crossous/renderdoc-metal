// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vector>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc,char **argv) {
  if(argc!=6)return 1;
  @autoreleasepool {
    auto device=MTLCreateSystemDefaultDevice();NSError *error=nil;
    const bool source=!strcmp(argv[2],"source"),wide=atoi(argv[5])!=0;
    auto library=source?[device newLibraryWithSource:[NSString stringWithContentsOfFile:
      [NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&error] options:nil error:&error]:
      [device newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[1]]] error:&error];
    auto pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:wide?@"wide_scalar":@"scalar"] error:&error];
    if(!pipeline){fprintf(stderr,"%s\n",error.description.UTF8String);return 2;}
    id<MTLComputePipelineState> pointerPipeline=nil;
    if(getenv("RENDERDOC_METAL_TEST_POINTER_REBIND")) {
      pointerPipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"pointer_scalar"] error:&error];
      if(!pointerPipeline)return 8;
    }
    std::vector<uint32_t> in(64),seed(160,0x72727272);
    for(unsigned i=0;i<64;i++)in[i]=i*17+9;
    auto input=[device newBufferWithBytes:in.data() length:in.size()*4 options:MTLResourceStorageModeShared];
    auto output=[device newBufferWithBytes:seed.data() length:seed.size()*4 options:MTLResourceStorageModeShared];
    output.label=@"Inline scalar full output";
    uint64_t packet[]={input.gpuAddress,0,input.length};
    auto table=[device newBufferWithBytes:packet length:sizeof(packet) options:MTLResourceStorageModeShared];
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");RENDERDOC_API_1_7_0 *api=nullptr;
    if(get && get(eRENDERDOC_API_Version_1_7_0,(void **)&api)!=1)return 3;
    const bool pointerRebind=getenv("RENDERDOC_METAL_TEST_POINTER_REBIND")!=nullptr;
    const bool sourced=getenv("RENDERDOC_METAL_TEST_POINTER_SOURCE")!=nullptr;
    const bool undeclared=getenv("RENDERDOC_METAL_TEST_UNDECLARED")!=nullptr;
    auto ann=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
      if(!api)return uint32_t(0);RENDERDOC_AnnotationValue v={};
      v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
    };
    if(api) {
      RENDERDOC_AnnotationValue v={};v.uint32=undeclared?65:67;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&v) ||
         ann(table,"metal.descriptorTable",2,0,1,24) || ann(table,"metal.descriptorSlotEvent",0,1,0,4) ||
         ann(table,"metal.descriptorSlotEvent",0,1,2,4) || ann(table,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)input,0))return 3;
      api->SetCaptureFilePathTemplate(argv[3]);api->StartFrameCapture(nullptr,nullptr);
    }
    auto queue=[device newCommandQueue];auto command=[queue commandBuffer];auto encoder=[command computeCommandEncoder];
    [encoder setComputePipelineState:pipeline];[encoder setBuffer:table offset:0 atIndex:30];
    [encoder setBuffer:output offset:0 atIndex:9];[encoder useResource:input usage:MTLResourceUsageRead];
    // This integer deliberately equals a real GPU address but is used only as
    // arithmetic data. A zero-field layout describes use, not a value heuristic.
    for(unsigned pass=0;pass<2;pass++) {
      struct Payload {uint64_t ordinary, salt;uint32_t offset,count;uint64_t extra,tail;} p=
        {input.gpuAddress,0x1234567800000041ULL+pass,pass?88U:8U,64,0x8765432100000023ULL,13};
      const unsigned bytes=wide?40:24;
      if(pass && pointerRebind) {
        [encoder setComputePipelineState:pointerPipeline];
        // Legal Native ownership: the caller releases its PSO after encoding.
        pointerPipeline=nil;
        if(sourced && (ann(encoder,"metal.descriptorInlineLayout",0,2,1,8) ||
           ann(encoder,"metal.descriptorInlineBinding",2,0,(uint64_t)(__bridge void *)input,0)))return 4;
        uint64_t address=input.gpuAddress;[encoder setBytes:&address length:8 atIndex:2];
      } else {
        if(!undeclared && ann(encoder,"metal.descriptorInlineLayout",0,2,0,bytes))return 4;
        [encoder setBytes:&p length:bytes atIndex:2];
      }
      [encoder dispatchThreads:MTLSizeMake(64,1,1) threadsPerThreadgroup:MTLSizeMake(wide?16:32,1,1)];
    }
    [encoder endEncoding];[command commit];[command waitUntilCompleted];
    if(command.status!=MTLCommandBufferStatusCompleted || command.error)return 5;
    if(api && !api->EndFrameCapture(nullptr,nullptr))return 6;
    const uint32_t *values=(const uint32_t *)output.contents;
    for(unsigned i=0;i<160;i++) {
      const bool written=(i>=8 && i<72) || (i>=88 && i<152);
      if(written ? values[i]==0x72727272 : values[i]!=0x72727272)return 9;
    }
    FILE *f=fopen(argv[4],"wb");if(!f)return 7;
    bool saved=fwrite(output.contents,1,output.length,f)==output.length;fclose(f);
    if(!saved)return 7;
    printf("Native complete output %lu B; scalar payload %u B; two rebindings; no addresses in payload; source=%d\n",(unsigned long)output.length,wide?40:24,source);
  }
}
