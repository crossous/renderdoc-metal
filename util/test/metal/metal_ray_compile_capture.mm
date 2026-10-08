// SPDX-License-Identifier: MIT
// Six native compute creation APIs, each executing a real triangle ray query.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <cstdio>
#include "renderdoc/api/app/renderdoc_app.h"

int main(int argc, char **argv)
{
  if(argc != 2) return 1;
  @autoreleasepool
  {
    id<MTLDevice> d=MTLCreateSystemDefaultDevice();
    if(!d) return 2;
    RENDERDOC_API_1_6_0 *api=nullptr;
    auto getAPI=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    if(getAPI) getAPI(eRENDERDOC_API_Version_1_6_0,(void **)&api);
    NSMutableString *source=[NSMutableString stringWithString:
      @"#include <metal_stdlib>\n#include <metal_raytracing>\nusing namespace metal;using namespace raytracing;\n"];
    for(unsigned i=0;i<6;i++)
      [source appendFormat:@"kernel void trace%u(primitive_acceleration_structure scene [[buffer(0)]],"
        "device uint *out [[buffer(1)]],uint x [[thread_position_in_grid]]){"
        "ray r;r.origin=float3(x?100:0,0,-2);r.direction=float3(0,0,1);"
        "r.min_distance=0;r.max_distance=10;intersector<triangle_data> q;"
        "q.assume_geometry_type(geometry_type::triangle);q.force_opacity(forced_opacity::opaque);"
        "auto hit=q.intersect(r,scene);out[x]=hit.type==intersection_type::triangle?1:0;}\n",i];
    NSError *error=nil;
    auto library=[d newLibraryWithSource:source options:nil error:&error];
    if(!library) {fprintf(stderr,"library: %s\n",error.description.UTF8String);return 3;}
    const float triangle[]={-1,-1,0,1,-1,0,0,1,0};
    auto vertices=[d newBufferWithBytes:triangle length:sizeof(triangle) options:MTLResourceStorageModeShared];
    auto geometry=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    geometry.vertexBuffer=vertices;geometry.vertexStride=12;geometry.triangleCount=1;geometry.opaque=YES;
    auto description=[MTLPrimitiveAccelerationStructureDescriptor descriptor];
    description.geometryDescriptors=@[geometry];
    auto sizes=[d accelerationStructureSizesWithDescriptor:description];
    auto structure=[d newAccelerationStructureWithSize:sizes.accelerationStructureSize];
    structure.label=@"Compile API triangle AS";
    auto scratch=[d newBufferWithLength:sizes.buildScratchBufferSize options:MTLResourceStorageModePrivate];
    auto queue=[d newCommandQueue];
    auto build=[queue commandBuffer];auto acceleration=[build accelerationStructureCommandEncoder];
    [acceleration buildAccelerationStructure:structure descriptor:description scratchBuffer:scratch scratchBufferOffset:0];
    [acceleration endEncoding];[build commit];[build waitUntilCompleted];
    if(build.error) {fprintf(stderr,"build: %s\n",build.error.description.UTF8String);return 4;}
    unsigned initial[12];for(auto &v:initial)v=7;
    auto output=[d newBufferWithBytes:initial length:sizeof(initial) options:MTLResourceStorageModeShared];
    output.label=@"Six compile API ray results";
    auto layer=[CAMetalLayer layer];layer.device=d;layer.pixelFormat=MTLPixelFormatRGBA16Float;
    layer.drawableSize=CGSizeMake(16,16);layer.framebufferOnly=NO;
    auto drawable=[layer nextDrawable];if(!drawable)return 5;
    if(api){api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);}
    const char *names[]={"function-sync","function-options-sync","descriptor-sync",
                         "function-async","function-options-async","descriptor-async"};
    id<MTLComputePipelineState> pipelines[6]={};
    for(unsigned i=0;i<6;i++)
    {
      auto function=[library newFunctionWithName:[NSString stringWithFormat:@"trace%u",i]];
      auto pd=[MTLComputePipelineDescriptor new];pd.computeFunction=function;
      pd.label=[NSString stringWithUTF8String:names[i]];
      if(i==0)pipelines[i]=[d newComputePipelineStateWithFunction:function error:&error];
      if(i==1)pipelines[i]=[d newComputePipelineStateWithFunction:function options:MTLPipelineOptionArgumentInfo reflection:nil error:&error];
      if(i==2)pipelines[i]=[d newComputePipelineStateWithDescriptor:pd options:MTLPipelineOptionArgumentInfo reflection:nil error:&error];
      if(i>=3)
      {
        dispatch_semaphore_t done=dispatch_semaphore_create(0);
        __block id<MTLComputePipelineState> created=nil;
        __block NSError *failure=nil;
        void (^receive)(id<MTLComputePipelineState>,NSError *)=^(id<MTLComputePipelineState> p,NSError *e){
          created=[p retain];failure=[e retain];dispatch_semaphore_signal(done);
        };
        if(i==3)[d newComputePipelineStateWithFunction:function completionHandler:receive];
        else if(i==4)[d newComputePipelineStateWithFunction:function options:MTLPipelineOptionArgumentInfo
          completionHandler:^(id<MTLComputePipelineState> p,MTLComputePipelineReflection *r,NSError *e){receive(p,e);}];
        else [d newComputePipelineStateWithDescriptor:pd options:MTLPipelineOptionArgumentInfo
          completionHandler:^(id<MTLComputePipelineState> p,MTLComputePipelineReflection *r,NSError *e){receive(p,e);}];
        if(dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,10*NSEC_PER_SEC)))
        {fprintf(stderr,"timeout %s\n",names[i]);return 6;}
        pipelines[i]=created;error=failure;
        dispatch_release(done);
      }
      if(!pipelines[i]){fprintf(stderr,"%s: %s\n",names[i],error.description.UTF8String);return 7;}
      printf("PASS native creation %s\n",names[i]);
    }
    auto command=[queue commandBuffer];auto compute=[command computeCommandEncoder];
    for(unsigned i=0;i<6;i++)
    {
      [compute setComputePipelineState:pipelines[i]];
      [compute setAccelerationStructure:structure atBufferIndex:0];
      [compute setBuffer:output offset:i*8 atIndex:1];
      [compute dispatchThreads:MTLSizeMake(2,1,1) threadsPerThreadgroup:MTLSizeMake(2,1,1)];
    }
    [compute endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
    if(command.error){fprintf(stderr,"dispatch: %s\n",command.error.description.UTF8String);return 8;}
    auto values=(const unsigned *)output.contents;
    for(unsigned i=0;i<6;i++)
    {
      printf("%s ray query %s hit/miss=%u/%u\n",values[i*2]==1&&values[i*2+1]==0?"PASS":"FAIL",names[i],values[i*2],values[i*2+1]);
      if(values[i*2]!=1||values[i*2+1]!=0)return 9;
    }
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 10;
  }
  return 0;
}
