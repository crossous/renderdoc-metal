// SPDX-License-Identifier: MIT
// Executes the actual captured UE shader with a tiny exact opaque-copy input.
// The copied packet has no descriptor consumer in this fixture.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc,char **argv)
{
  if(argc!=2)return 2;
  @autoreleasepool
  {
    NSString *folder=[NSString stringWithUTF8String:argv[1]];
    auto data=[&](NSString *name){return [NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:name]];};
    NSDictionary *manifest=[NSJSONSerialization JSONObjectWithData:data(@"manifest.json") options:0 error:nil];
    NSData *code=data(@"shader.metallib"),*payload=data(@"payload.bin"),*indices=data(@"indices.bin"),*uniform=data(@"uniform.bin"),*samplers=data(@"samplers.bin");
    const NSUInteger count=[manifest[@"count"] unsignedIntegerValue],size=[manifest[@"destination_bytes"] unsignedIntegerValue];
    if(!code||!payload||!indices||!uniform||!samplers||!count||count>8||size>65536||size<24||payload.length!=count*24||indices.length!=count*4||uniform.length!=16)return 3;
    const uint32_t *header=(const uint32_t *)uniform.bytes,*destinations=(const uint32_t *)indices.bytes;
    if(header[0]!=count||header[1]!=1||header[2]||header[3]!=2)return 4;
    for(NSUInteger i=0;i<count;i++)if(destinations[i]>=size/24)return 5;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();NSError *error=nil;
    dispatch_data_t binary=dispatch_data_create(code.bytes,code.length,dispatch_get_main_queue(),DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    id<MTLLibrary> library=[device newLibraryWithData:binary error:&error];
    id<MTLFunction> function=[library newFunctionWithName:manifest[@"function"]];
    MTLComputePipelineDescriptor *descriptor=[MTLComputePipelineDescriptor new];descriptor.computeFunction=function;
    descriptor.label=manifest[@"function"];descriptor.maxTotalThreadsPerThreadgroup=1;
    descriptor.buffers[0].mutability=MTLMutabilityImmutable;
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithDescriptor:descriptor options:MTLPipelineOptionBindingInfo|MTLPipelineOptionBufferTypeInfo reflection:nil error:&error];
    if(!pipeline){fprintf(stderr,"UE pipeline failed: %s\n",error.localizedDescription.UTF8String);return 6;}
    id<MTLBuffer> entries=[device newBufferWithBytes:payload.bytes length:payload.length options:MTLResourceStorageModeShared];
    id<MTLBuffer> indexBuffer=[device newBufferWithBytes:indices.bytes length:indices.length options:MTLResourceStorageModeShared];
    id<MTLBuffer> constants=[device newBufferWithBytes:uniform.bytes length:16 options:MTLResourceStorageModeShared];
    id<MTLBuffer> sampling=[device newBufferWithBytes:samplers.bytes length:samplers.length options:MTLResourceStorageModeShared];
    id<MTLBuffer> output=[device newBufferWithLength:size options:MTLResourceStorageModeShared];
    memset(output.contents,0xcd,size);
    const uint64_t packets[]={entries.gpuAddress,0,payload.length,indexBuffer.gpuAddress,0,indices.length,output.gpuAddress,0,size};
    id<MTLBuffer> table=[device newBufferWithBytes:packets length:72 options:MTLResourceStorageModeShared];
    const uint64_t root[]={constants.gpuAddress,sampling.gpuAddress};
    if(!entries||!indexBuffer||!constants||!sampling||!output||!table)return 7;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH"))
    {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 8;
      api->SetCaptureFilePathTemplate(path);RENDERDOC_AnnotationValue coverage={};coverage.uint32=8;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage))return 9;
    }
    auto annotate=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d){
      if(!api)return uint32_t(0);RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);};
    if(annotate(table,"metal.descriptorTable",1,0,3,24))return 10;
    id<MTLBuffer> sources[]={entries,indexBuffer,output};
    for(unsigned i=0;i<3;i++)
      if(annotate(table,"metal.descriptorSlotEvent",i*24,1,0,i==2?1:0)||
         annotate(table,"metal.descriptorSlotEvent",i*24,1,2,i==2?1:0)||
         annotate(table,"metal.descriptorSlotBinding",i*24,0,(uint64_t)(__bridge void *)sources[i],0))return 11;
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    id<MTLCommandQueue> queue=[device newCommandQueue];
    for(int capture=0;capture<(api?2:1);capture++)
    {
      memset(output.contents,0xcd,size);
      if(api)api->StartFrameCapture(nullptr,nullptr);
      id<MTLCommandBuffer> command=[queue commandBuffer];[command enqueue];
      id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];[compute setComputePipelineState:pipeline];
      [compute setBuffer:table offset:0 atIndex:0];
      if(annotate(compute,"metal.descriptorInlineLayout",0,2,2,8)||
         annotate(compute,"metal.descriptorInlineBinding",2,0,(uint64_t)(__bridge void *)constants,0)||
         annotate(compute,"metal.descriptorInlineBinding",2,1,(uint64_t)(__bridge void *)sampling,0))return 12;
      [compute setBytes:root length:16 atIndex:2];
      for(id<MTLBuffer> source in @[entries,indexBuffer,constants,sampling])[compute useResource:source usage:MTLResourceUsageRead];
      [compute useResource:output usage:MTLResourceUsageRead|MTLResourceUsageWrite];
      [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[compute endEncoding];
      id<CAMetalDrawable> drawable=[layer nextDrawable];MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture=drawable.texture;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
      [[command renderCommandEncoderWithDescriptor:pass] endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
      if(command.error){fprintf(stderr,"GPU failed: %s\n",command.error.localizedDescription.UTF8String);return 13;}
      NSMutableData *expected=[NSMutableData dataWithLength:size];memset(expected.mutableBytes,0xcd,size);
      for(NSUInteger i=0;i<count;i++)memcpy((uint8_t *)expected.mutableBytes+destinations[i]*24,(const uint8_t *)payload.bytes+i*24,24);
      if(memcmp(output.contents,expected.bytes,size))return 14;
      if(api&&!api->EndFrameCapture(nullptr,nullptr))return 15;
    }
    printf("PASS actual UE shader %s: %lu updates, %lu bytes destination, exact opaque payload and untouched sentinel; captures=%d\n",[manifest[@"function"] UTF8String],(unsigned long)count,(unsigned long)size,api?2:0);
  }
  return 0;
}
