// SPDX-License-Identifier: MIT
// Native argument-buffer AS/IFT/VFT descriptors: hit, any-hit reject, miss, rebind.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include "renderdoc/api/app/renderdoc_app.h"
int main(int argc,char **argv)
{
  if(argc!=3)return 1;
  const bool deviceLayout=strstr(argv[2],"device");
  const bool managed=strstr(argv[2],"managed");
  const bool arrays=strstr(argv[2],"arrays");
  const bool frameEncoding=strstr(argv[2],"frame");
  const bool instanceLayout=strstr(argv[2],"tlas");
  @autoreleasepool
  {
    auto d=MTLCreateSystemDefaultDevice();auto queue=[d newCommandQueue];NSError *error=nil;
    RENDERDOC_API_1_7_0 *api=nullptr;
    auto getAPI=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    if(getAPI)getAPI(eRENDERDOC_API_Version_1_7_0,(void **)&api);
    NSString *source=@"#include <metal_stdlib>\nusing namespace metal;using namespace metal::raytracing;\n"
      "[[intersection(triangle, triangle_data)]] bool accept_triangle(){return true;}\n"
      "[[intersection(triangle, triangle_data)]] bool reject_triangle(){return false;}\n"
      "[[visible]] uint three(){return 3;}\n[[visible]] uint nine(){return 9;}\n"
      "struct Packet {acceleration_structure<> scene [[id(0)]];"
      "intersection_function_table<triangle_data> table [[id(1)]];"
      "visible_function_table<uint()> value [[id(2)]];};\n"
      "kernel void trace(constant Packet &p [[buffer(0)]],device uint *out [[buffer(1)]]){"
      "intersector<triangle_data> q;q.assume_geometry_type(geometry_type::triangle);"
      "auto hit=q.intersect(ray(float3(0,0,-2),float3(0,0,1)),p.scene,p.table);"
      "out[0]=hit.type==intersection_type::triangle?p.value[0]():0;}";
    if(instanceLayout){
      source=[source stringByReplacingOccurrencesOfString:@"acceleration_structure<>" withString:@"acceleration_structure<instancing>"];
      source=[source stringByReplacingOccurrencesOfString:@"intersector<triangle_data>" withString:@"intersector<triangle_data, instancing>"];
      source=[source stringByReplacingOccurrencesOfString:@"intersection_function_table<triangle_data>" withString:@"intersection_function_table<triangle_data, instancing>"];
      source=[source stringByReplacingOccurrencesOfString:@"intersection(triangle, triangle_data)" withString:@"intersection(triangle, triangle_data, instancing)"];
    }
    source=[source stringByAppendingString:[[source substringFromIndex:[source rangeOfString:@"kernel void trace("].location]
      stringByReplacingOccurrencesOfString:@"kernel void trace(" withString:@"kernel void foreign_trace("]];
    auto library=[d newLibraryWithSource:source options:nil error:&error];
    if(!library){fprintf(stderr,"library %s\n",error.description.UTF8String);return 2;}
    auto trace=[library newFunctionWithName:@"trace"];
    id<MTLFunction> functions[4]={[library newFunctionWithName:@"accept_triangle"],
      [library newFunctionWithName:@"reject_triangle"],[library newFunctionWithName:@"three"],
      [library newFunctionWithName:@"nine"]};
    auto links=[MTLLinkedFunctions new];links.functions=@[functions[0],functions[1],functions[2],functions[3]];
    auto pd=[MTLComputePipelineDescriptor new];pd.computeFunction=trace;pd.linkedFunctions=links;
    auto pipeline=[d newComputePipelineStateWithDescriptor:pd options:MTLPipelineOptionArgumentInfo reflection:nil error:&error];
    if(!pipeline){fprintf(stderr,"pipeline %s\n",error.description.UTF8String);return 3;}
    id<MTLIntersectionFunctionTable> tables[2];id<MTLVisibleFunctionTable> values[2];
    for(unsigned i=0;i<2;i++){
      auto it=[MTLIntersectionFunctionTableDescriptor intersectionFunctionTableDescriptor];it.functionCount=1;
      tables[i]=[pipeline newIntersectionFunctionTableWithDescriptor:it];
      [tables[i] setFunction:[pipeline functionHandleWithFunction:functions[i]] atIndex:0];
      auto vt=[MTLVisibleFunctionTableDescriptor visibleFunctionTableDescriptor];vt.functionCount=1;
      values[i]=[pipeline newVisibleFunctionTableWithDescriptor:vt];
      [values[i] setFunction:[pipeline functionHandleWithFunction:functions[i+2]] atIndex:0];
    }
    pd.computeFunction=[library newFunctionWithName:@"foreign_trace"];
    auto foreign=[d newComputePipelineStateWithDescriptor:pd options:MTLPipelineOptionArgumentInfo reflection:nil error:&error];
    if(!foreign){fprintf(stderr,"foreign pipeline %s\n",error.description.UTF8String);return 3;}
    auto foreignIT=[MTLIntersectionFunctionTableDescriptor intersectionFunctionTableDescriptor];foreignIT.functionCount=1;
    auto foreignIFT=[foreign newIntersectionFunctionTableWithDescriptor:foreignIT];
    [foreignIFT setFunction:[foreign functionHandleWithFunction:functions[0]] atIndex:0];
    auto foreignVT=[MTLVisibleFunctionTableDescriptor visibleFunctionTableDescriptor];foreignVT.functionCount=1;
    auto foreignVFT=[foreign newVisibleFunctionTableWithDescriptor:foreignVT];
    [foreignVFT setFunction:[foreign functionHandleWithFunction:functions[2]] atIndex:0];
    id<MTLAccelerationStructure> structures[2];id<MTLBuffer> vertices[2];
    for(unsigned i=0;i<2;i++){
      const float shift=i?100:0;const float triangle[]={-1+shift,-1,0,1+shift,-1,0,shift,1,0};
      vertices[i]=[d newBufferWithBytes:triangle length:sizeof(triangle) options:MTLResourceStorageModeShared];
      auto geometry=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];geometry.vertexBuffer=vertices[i];
      geometry.vertexStride=12;geometry.triangleCount=1;geometry.opaque=NO;
      auto desc=[MTLPrimitiveAccelerationStructureDescriptor descriptor];desc.geometryDescriptors=@[geometry];
      auto sizes=[d accelerationStructureSizesWithDescriptor:desc];structures[i]=[d newAccelerationStructureWithSize:sizes.accelerationStructureSize];
      auto scratch=[d newBufferWithLength:sizes.buildScratchBufferSize options:MTLResourceStorageModePrivate];
      auto command=[queue commandBuffer];auto acceleration=[command accelerationStructureCommandEncoder];
      [acceleration buildAccelerationStructure:structures[i] descriptor:desc scratchBuffer:scratch scratchBufferOffset:0];
      [acceleration endEncoding];[command commit];[command waitUntilCompleted];
      if(command.error){fprintf(stderr,"build %s\n",command.error.description.UTF8String);return 4;}
    }
    id<MTLAccelerationStructure> children[2]={structures[0],structures[1]};
    // Both AS kinds are retained in TLAS captures for wrong-kind replay negatives.
    if(instanceLayout)for(unsigned i=0;i<2;i++){
      MTLAccelerationStructureInstanceDescriptor instance={};
      instance.transformationMatrix.columns[0].x=1;
      instance.transformationMatrix.columns[1].y=1;
      instance.transformationMatrix.columns[2].z=1;
      instance.mask=0xff;
      auto input=[d newBufferWithBytes:&instance length:sizeof(instance) options:MTLResourceStorageModeShared];
      auto desc=[MTLInstanceAccelerationStructureDescriptor descriptor];
      desc.instanceDescriptorBuffer=input;desc.instanceCount=1;
      desc.instancedAccelerationStructures=@[children[i]];
      auto sizes=[d accelerationStructureSizesWithDescriptor:desc];
      structures[i]=[d newAccelerationStructureWithSize:sizes.accelerationStructureSize];
      auto scratch=[d newBufferWithLength:sizes.buildScratchBufferSize options:MTLResourceStorageModePrivate];
      auto command=[queue commandBuffer];auto acceleration=[command accelerationStructureCommandEncoder];
      [acceleration buildAccelerationStructure:structures[i] descriptor:desc scratchBuffer:scratch scratchBufferOffset:0];
      [acceleration endEncoding];[command commit];[command waitUntilCompleted];
      if(command.error){fprintf(stderr,"TLAS build %s\n",command.error.description.UTF8String);return 4;}
    }
    auto unbuilt=[d newAccelerationStructureWithSize:structures[0].size];
    id<MTLArgumentEncoder> encoder=nil;
    if(deviceLayout){
      NSMutableArray *args=[NSMutableArray array];
      const MTLDataType types[]={instanceLayout?MTLDataTypeInstanceAccelerationStructure:MTLDataTypePrimitiveAccelerationStructure,MTLDataTypeIntersectionFunctionTable,MTLDataTypeVisibleFunctionTable};
      for(unsigned i=0;i<3;i++){
        auto a=[MTLArgumentDescriptor argumentDescriptor];a.index=i;a.dataType=types[i];a.access=MTLBindingAccessReadOnly;[args addObject:a];
      }
      encoder=[d newArgumentEncoderWithArguments:args];
    }else encoder=[trace newArgumentEncoderWithBufferIndex:0];
    if(!encoder)return 5;
    const NSUInteger alignment=encoder.alignment?encoder.alignment:1;
    const NSUInteger stride=(encoder.encodedLength+alignment-1)/alignment*alignment;
    const NSUInteger prefix=stride*2;
    auto packet=[d newBufferWithLength:prefix+stride*5 options:managed?MTLResourceStorageModeManaged:MTLResourceStorageModeShared];
    packet.label=@"Ray AS IFT VFT argument packets";
    memset(packet.contents,0xa5,packet.length);
    if(api&&frameEncoding){api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);}
    for(unsigned i=0;i<5;i++){
      [encoder setArgumentBuffer:packet offset:prefix+stride*i];
      [encoder setAccelerationStructure:i==4?nil:structures[i==2?1:0] atIndex:0];
      if(arrays){
        const id<MTLIntersectionFunctionTable> t[]={i==4?nil:tables[i==1?1:0]};
        const id<MTLVisibleFunctionTable> v[]={i==4?nil:values[i==3?1:0]};
        [encoder setIntersectionFunctionTables:t withRange:NSMakeRange(1,1)];
        [encoder setVisibleFunctionTables:v withRange:NSMakeRange(2,1)];
      }else{
        [encoder setIntersectionFunctionTable:i==4?nil:tables[i==1?1:0] atIndex:1];
        [encoder setVisibleFunctionTable:i==4?nil:values[i==3?1:0] atIndex:2];
      }
    }
    if(managed)[packet didModifyRange:NSMakeRange(0,packet.length)];
    const unsigned initial[]={7,7,7,7};auto output=[d newBufferWithBytes:initial length:sizeof(initial) options:MTLResourceStorageModeShared];
    output.label=@"Ray argument packet results";
    auto layer=[CAMetalLayer layer];layer.device=d;layer.pixelFormat=MTLPixelFormatRGBA16Float;
    layer.drawableSize=CGSizeMake(16,16);layer.framebufferOnly=NO;
    auto drawable=[layer nextDrawable];if(!drawable)return 6;
    if(api&&!frameEncoding){api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);}
    auto command=[queue commandBuffer];auto compute=[command computeCommandEncoder];
    [compute setComputePipelineState:pipeline];
    [compute useResources:(const id<MTLResource>[]){structures[0],structures[1],tables[0],tables[1],values[0],values[1],foreignIFT,foreignVFT,children[0],children[1],unbuilt}
                    count:11 usage:MTLResourceUsageRead];
    for(unsigned i=0;i<4;i++){
      [compute setBuffer:packet offset:prefix+stride*i atIndex:0];[compute setBuffer:output offset:i*4 atIndex:1];
      [compute dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
    }
    [compute endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
    if(command.error){fprintf(stderr,"dispatch %s\n",command.error.description.UTF8String);return 7;}
    auto result=(unsigned *)output.contents;
    printf("%s %s encodedLength=%lu alignment=%lu prefix=%lu results=%u/%u/%u/%u\n",
      result[0]==3&&result[1]==0&&result[2]==0&&result[3]==9?"PASS":"FAIL",argv[2],encoder.encodedLength,alignment,prefix,
      result[0],result[1],result[2],result[3]);
    if(result[0]!=3||result[1]!=0||result[2]!=0||result[3]!=9)return 8;
    if(api&&!api->EndFrameCapture(nullptr,nullptr))return 9;
  }
  return 0;
}
