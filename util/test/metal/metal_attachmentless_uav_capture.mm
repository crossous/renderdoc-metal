// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <initializer_list>
#include "renderdoc/api/app/renderdoc_app.h"
int main()
{
  @autoreleasepool {
    const bool sourced=getenv("RENDERDOC_METAL_ATTACHMENTLESS_SOURCED")!=nullptr;
    const bool parallel=getenv("RENDERDOC_METAL_ATTACHMENTLESS_PARALLEL")!=nullptr;
    const bool unretained=getenv("RENDERDOC_METAL_ATTACHMENTLESS_UNRETAINED")!=nullptr;
    const NSUInteger width=getenv("RENDERDOC_METAL_ATTACHMENTLESS_WIDTH")?strtoul(getenv("RENDERDOC_METAL_ATTACHMENTLESS_WIDTH"),nullptr,10):2;
    const NSUInteger height=getenv("RENDERDOC_METAL_ATTACHMENTLESS_HEIGHT")?strtoul(getenv("RENDERDOC_METAL_ATTACHMENTLESS_HEIGHT"),nullptr,10):2;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();NSError *error=nil;
    id<MTLLibrary> library=[device newLibraryWithSource:@R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 writer(uint index [[vertex_id]],device atomic_uint *output [[buffer(1)]]) {
  atomic_fetch_add_explicit(output,1u,memory_order_relaxed);
  atomic_fetch_add_explicit(output+1,index+1,memory_order_relaxed);
  float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return float4(p[index%3],0,1);
}
struct Counters { atomic_uint count; atomic_uint sum; uint sentinelA; uint sentinelB; };
struct Entry { device Counters *value [[id(0)]]; ulong pad [[id(1)]]; ulong metadata [[id(2)]]; };
vertex float4 writer_sourced(uint index [[vertex_id]],const device ulong *root [[buffer(0)]]) {
  const device Entry *entry=reinterpret_cast<const device Entry *>(root[0]);
  if(entry->metadata==0xabcdef0123456789ul && root[1]==0x1357246824681357ul) {
    atomic_fetch_add_explicit(&entry->value->count,1u,memory_order_relaxed);
    atomic_fetch_add_explicit(&entry->value->sum,index+1,memory_order_relaxed);
  }
  float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return float4(p[index%3],0,1);
}
fragment void empty(){}
)MSL" options:nil error:&error];
    auto pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[library newFunctionWithName:sourced?@"writer_sourced":@"writer"];
    pd.fragmentFunction=[library newFunctionWithName:@"empty"];pd.rasterizationEnabled=YES;
    id<MTLRenderPipelineState> pipeline=[device newRenderPipelineStateWithDescriptor:pd error:&error];
    if(!pipeline){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 2;}
    uint32_t words[24]={};words[4]=3;words[5]=1;words[7]=2;words[16]=6;words[17]=1;words[19]=2;
    const uint32_t initial[4]={0,0,0x13572468,0x24681357};
    id<MTLBuffer> upload=[device newBufferWithBytes:words length:sizeof(words) options:MTLResourceStorageModeShared];
    id<MTLBuffer> arguments=[device newBufferWithLength:sizeof(words) options:MTLResourceStorageModePrivate];
    id<MTLBuffer> output=[device newBufferWithBytes:initial length:sizeof(initial) options:MTLResourceStorageModeShared];
    output.label=@"Attachmentless vertex UAV";arguments.label=@"Attachmentless indirect";
    id<MTLCommandQueue> queue=[device newCommandQueue];auto init=[queue commandBuffer];auto copy=[init blitCommandEncoder];
    [copy copyFromBuffer:upload sourceOffset:0 toBuffer:arguments destinationOffset:0 size:sizeof(words)];
    [copy endEncoding];[init commit];[init waitUntilCompleted];if(init.error)return 3;
    RENDERDOC_API_1_7_0 *api=nullptr;
    if(const char *path=getenv("RENDERDOC_METAL_CAPTURE_PATH")) {
      auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
      if(!get||!get(eRENDERDOC_API_Version_1_7_0,(void **)&api))return 4;api->SetCaptureFilePathTemplate(path);
    }
    const uint64_t entry[3]={output.gpuAddress,0,0xabcdef0123456789ULL};
    id<MTLBuffer> table=sourced?[device newBufferWithBytes:entry length:sizeof(entry) options:MTLResourceStorageModeShared]:nil;
    uint64_t root[2]={sourced?table.gpuAddress:0,0x1357246824681357ULL};
    auto annotation=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
      if(!api)return uint32_t(0);RENDERDOC_AnnotationValue v={};v.vector.uint64[0]=a;v.vector.uint64[1]=b;v.vector.uint64[2]=c;v.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&v);
    };
    if(api&&sourced)
    {
      RENDERDOC_AnnotationValue coverage={};coverage.uint32=59;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage))return 9;
      if(annotation(table,"metal.descriptorTable",1,0,1,24) ||
         annotation(table,"metal.descriptorSlotEvent",0,1,0,0) ||
         annotation(table,"metal.descriptorSlotEvent",0,1,2,0) ||
         annotation(table,"metal.descriptorSlotBinding",0,0,(uint64_t)(__bridge void *)output,0))return 10;
    }
    const uint32_t zeroWords[24]={};
    id<MTLBuffer> zeros=[device newBufferWithBytes:zeroWords length:sizeof(zeroWords) options:MTLResourceStorageModeShared];
    auto layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.drawableSize=CGSizeMake(2,2);
    if(api)api->StartFrameCapture(nullptr,nullptr);
    auto command=unretained?[queue commandBufferWithUnretainedReferences]:[queue commandBuffer];auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.renderTargetWidth=width;pass.renderTargetHeight=height;pass.defaultRasterSampleCount=1;pass.renderTargetArrayLength=1;
    id<MTLParallelRenderCommandEncoder> parent=parallel?[command parallelRenderCommandEncoderWithDescriptor:pass]:nil;
    auto render=parallel?[parent renderCommandEncoder]:[command renderCommandEncoderWithDescriptor:pass];[render setRenderPipelineState:pipeline];
    if(sourced)
    {
      if(annotation(render,"metal.descriptorInlineLayout",1,0,1,16) ||
         annotation(render,"metal.descriptorInlineBinding",uint64_t(1)<<32,0,(uint64_t)(__bridge void *)table,0)) { [render endEncoding];return 11; }
      [render setVertexBytes:root length:sizeof(root) atIndex:0];[render useResource:table usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
    }
    else [render setVertexBuffer:output offset:0 atIndex:1];
    [render useResource:output usage:MTLResourceUsageRead|MTLResourceUsageWrite stages:MTLRenderStageVertex];
    if(api){RENDERDOC_AnnotationValue declared={};declared.uint32=1;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)render,"metal.renderWritesDeclared",eRENDERDOC_UInt32,0,&declared)){[render endEncoding];return 5;}}
    for(NSUInteger offset:{NSUInteger(16),NSUInteger(64)})[render drawPrimitives:MTLPrimitiveTypeTriangle indirectBuffer:arguments indirectBufferOffset:offset];
    [render endEncoding];if(parent)[parent endEncoding];copy=[command blitCommandEncoder];[copy copyFromBuffer:zeros sourceOffset:0 toBuffer:arguments destinationOffset:0 size:sizeof(words)];[copy endEncoding];
    id<CAMetalDrawable> drawable=[layer nextDrawable];if(!drawable)return 6;
    auto present=[MTLRenderPassDescriptor renderPassDescriptor];present.colorAttachments[0].texture=drawable.texture;
    present.colorAttachments[0].loadAction=MTLLoadActionClear;present.colorAttachments[0].storeAction=MTLStoreActionStore;
    [[command renderCommandEncoderWithDescriptor:present] endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
    const uint32_t *result=(const uint32_t *)output.contents;
    if(command.error || result[0]!=9 || result[1]!=27 || result[2]!=initial[2] || result[3]!=initial[3]){
      fprintf(stderr,"UAV counts=%u sum=%u error=%s\n",result[0],result[1],command.error.localizedDescription.UTF8String);return 7;}
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 8;
    printf("PASS Native attachmentless render UAV: rasterization enabled, fragment present, %lux%lu explicit pass, parallel=%d unretained=%d, actual 9 vertex writes / sum27, sentinels, indirect 3/6 then source-zero\n",(unsigned long)width,(unsigned long)height,parallel,unretained);
  }
}
