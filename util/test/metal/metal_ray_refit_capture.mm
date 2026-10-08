// SPDX-License-Identifier: MIT
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include "renderdoc/api/app/renderdoc_app.h"

// Initial snapshots must restore AS geometry independently of the current input
// bytes, then permit a genuine update in the captured frame and on replay resets.
int main(int argc, char **argv)
{
  if(argc != 3) return 1;
  const bool indexed = strstr(argv[2], "indexed");
  const bool u32 = strstr(argv[2], "u32");
  const bool formatted = strstr(argv[2], "formatted");
  const bool noDuplicate = strstr(argv[2], "noduplicate");
  const bool initialRefit = strstr(argv[2], "initial-refit");
  const bool copy = strstr(argv[2], "copy");
  const bool copyLate = strstr(argv[2], "copy-late");
  const bool separate = strstr(argv[2], "separate");
  const bool nilDestination = strstr(argv[2], "nil");
  @autoreleasepool
  {
    RENDERDOC_API_1_7_0 *api = nullptr;
    auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    if(get) get(eRENDERDOC_API_Version_1_7_0, (void **)&api);
    auto d = MTLCreateSystemDefaultDevice(); auto q = [d newCommandQueue];
    NSError *error = nil;
    auto library = [d newLibraryWithSource:@"#include <metal_stdlib>\nusing namespace metal; using namespace metal::raytracing;\n"
      "kernel void trace(acceleration_structure<> a [[buffer(0)]], device uint *out [[buffer(1)]]) {"
      "intersector<triangle_data> query; query.assume_geometry_type(geometry_type::triangle);"
      "auto hit=query.intersect(ray(float3(0,0,-2),float3(0,0,1)),a);"
      "out[0]=hit.type==intersection_type::triangle?1:0;}" options:nil error:&error];
    auto pipeline = library ? [d newComputePipelineStateWithFunction:[library newFunctionWithName:@"trace"] error:&error] : nil;
    if(!pipeline) {fprintf(stderr,"%s\n",error.description.UTF8String); return 2;}
    const unsigned stride = formatted ? 5 : 3;
    float data[15] = {};
    const float triangle[9] = {-1,-1,0,1,-1,0,0,1,0};
    for(unsigned i=0;i<3;i++) {memcpy(data+i*stride,triangle+i*3,12); if(formatted) {data[i*stride+3]=1;data[i*stride+4]=99;}}
    auto vertices = [d newBufferWithBytes:data length:3*stride*4 options:MTLResourceStorageModeShared];
    vertices.label=@"Refit current vertices";
    auto geometry = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    geometry.vertexBuffer=vertices; geometry.vertexStride=stride*4; geometry.triangleCount=1;
    geometry.allowDuplicateIntersectionFunctionInvocation=!noDuplicate;
    if(formatted) geometry.vertexFormat=MTLAttributeFormatFloat4;
    id<MTLBuffer> indices=nil;
    if(indexed)
    {
      unsigned short values16[5]={65535,0,1,2,65535};
      unsigned values32[5]={0xffffffff,0,1,2,0xffffffff};
      indices=[d newBufferWithBytes:u32?(void *)values32:(void *)values16 length:u32?sizeof(values32):sizeof(values16) options:MTLResourceStorageModeShared];
      indices.label=@"Refit current indices";
      geometry.indexBuffer=indices; geometry.indexType=u32?MTLIndexTypeUInt32:MTLIndexTypeUInt16;
      geometry.indexBufferOffset=u32?4:2;
    }
    auto descriptor=[MTLPrimitiveAccelerationStructureDescriptor descriptor];
    descriptor.usage=MTLAccelerationStructureUsageRefit; descriptor.geometryDescriptors=@[geometry];
    auto sizes=[d accelerationStructureSizesWithDescriptor:descriptor];
    auto structure=[d newAccelerationStructureWithSize:sizes.accelerationStructureSize];
    auto scratch=[d newBufferWithLength:MAX(sizes.buildScratchBufferSize,sizes.refitScratchBufferSize) options:MTLResourceStorageModePrivate];
    auto command=[q commandBuffer]; auto encoder=[command accelerationStructureCommandEncoder];
    [encoder buildAccelerationStructure:structure descriptor:descriptor scratchBuffer:scratch scratchBufferOffset:0];
    [encoder endEncoding]; [command commit]; [command waitUntilCompleted]; if(command.error) return 3;
    if(initialRefit)
    {
      for(unsigned i=0;i<3;i++) ((float *)vertices.contents)[i*stride]+=100;
      command=[q commandBuffer]; encoder=[command accelerationStructureCommandEncoder];
      auto destination=separate?[d newAccelerationStructureWithSize:sizes.accelerationStructureSize]:structure;
      [encoder refitAccelerationStructure:structure descriptor:descriptor destination:nilDestination?nil:destination scratchBuffer:scratch scratchBufferOffset:0];
      [encoder endEncoding]; [command commit]; [command waitUntilCompleted]; if(command.error) return 4;
      structure=destination;
    }
    if(copy)
    {
      auto original=structure;
      structure=[d newAccelerationStructureWithSize:sizes.accelerationStructureSize];
      command=[q commandBuffer]; encoder=[command accelerationStructureCommandEncoder];
      [encoder copyAccelerationStructure:original toAccelerationStructure:structure];
      [encoder endEncoding];
      auto copyCommand=command;
      if(!copyLate) {[copyCommand commit]; [copyCommand waitUntilCompleted]; if(copyCommand.error) return 5;}
      // A later update must not change the copy's frozen recipe.
      for(unsigned i=0;i<3;i++) ((float *)vertices.contents)[i*stride]=triangle[i*3]+(initialRefit?0:100);
      command=[q commandBuffer]; encoder=[command accelerationStructureCommandEncoder];
      [encoder refitAccelerationStructure:original descriptor:descriptor destination:original scratchBuffer:scratch scratchBufferOffset:0];
      [encoder endEncoding]; [command commit]; [command waitUntilCompleted]; if(command.error) return 6;
      if(copyLate) {[copyCommand commit]; [copyCommand waitUntilCompleted]; if(copyCommand.error) return 5;}
    }
    // Deliberately differ from the built/refitted geometry at capture start.
    for(unsigned i=0;i<3;i++) ((float *)vertices.contents)[i*stride]=triangle[i*3]+(initialRefit?0:100);
    unsigned sentinel[3]={7,7,7};
    auto output=[d newBufferWithBytes:sentinel length:sizeof(sentinel) options:MTLResourceStorageModeShared];
    output.label=@"Refit ray results";
    auto layer=[CAMetalLayer layer]; layer.device=d; layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.drawableSize=CGSizeMake(2,2); layer.framebufferOnly=NO;
    auto drawable=[layer nextDrawable]; if(!drawable) return 7;
    if(api) {api->SetCaptureFilePathTemplate(argv[1]); api->StartFrameCapture(nullptr,nullptr);}
    auto trace = [&](unsigned slot, bool present) {
      auto cb=[q commandBuffer]; auto cs=[cb computeCommandEncoder];
      [cs setComputePipelineState:pipeline]; [cs setAccelerationStructure:structure atBufferIndex:0];
      [cs setBuffer:output offset:slot*4 atIndex:1];
      [cs dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)]; [cs endEncoding];
      if(present) [cb presentDrawable:drawable]; [cb commit]; [cb waitUntilCompleted];
      return cb.error==nil;
    };
    if(!trace(0,false)) return 8;
    command=[q commandBuffer]; encoder=[command accelerationStructureCommandEncoder];
    auto destination=separate?[d newAccelerationStructureWithSize:sizes.accelerationStructureSize]:structure;
    [encoder refitAccelerationStructure:structure descriptor:descriptor destination:nilDestination?nil:destination scratchBuffer:scratch scratchBufferOffset:0];
    [encoder endEncoding]; [command commit]; [command waitUntilCompleted]; if(command.error) return 9;
    structure=destination;
    if(!trace(1,false) || !trace(2,true)) return 10;
    auto values=(unsigned *)output.contents;
    if(values[0]!=(copyLate?(initialRefit?1U:0U):(initialRefit?0U:1U)) || values[1]!=(initialRefit?1U:0U) || values[2]!=values[1]) {
      fprintf(stderr,"Unexpected refit results %u/%u/%u\n",values[0],values[1],values[2]); return 11;}
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 12;
    printf("PASS native %s rays %u/%u/%u\n",argv[2],values[0],values[1],values[2]);
  }
}
