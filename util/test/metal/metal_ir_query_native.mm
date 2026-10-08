// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#define IR_PRIVATE_IMPLEMENTATION
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>
#include <metal_irconverter_runtime/ir_raytracing.h>
#include "renderdoc/api/app/renderdoc_app.h"
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <vector>

int main(int argc, char **argv)
{
  if(argc != 2 && argc != 3) return 1;
  setvbuf(stdout, nullptr, _IONBF, 0);
  @autoreleasepool
  {
    const char *mode=getenv("RENDERDOC_IR_QUERY_MODE");if(!mode)mode="default";
    const bool indirect=strcmp(mode,"default")!=0, privateInput=strstr(mode,"private"),
        empty=strstr(mode,"empty"), masked=strstr(mode,"masked"), frame=strstr(mode,"frame");
    const unsigned count=getenv("RENDERDOC_IR_QUERY_INSTANCES")?strtoul(getenv("RENDERDOC_IR_QUERY_INSTANCES"),nullptr,10):1;
    if((count!=1 && count!=3 && count!=64 && count!=65) || (!indirect && count!=1))return 17;
    const bool frameGeometry=strstr(mode,"geometry"), indexedGeometry=strstr(mode,"indexed"),
        multiGeometry=strstr(mode,"multi");
    const bool dynamicHeader=getenv("RENDERDOC_IR_QUERY_DYNAMIC_HEADER")!=nullptr,
        privateContribution=getenv("RENDERDOC_IR_QUERY_PRIVATE_CONTRIBUTION")!=nullptr,
        heapHeader=getenv("RENDERDOC_IR_QUERY_HEAP_HEADER")!=nullptr;
    const bool placementFrameGeometry=getenv("RENDERDOC_IR_QUERY_PLACEMENT_FRAME_GEOMETRY")!=nullptr;
    if(placementFrameGeometry && !frameGeometry)return 36;
    const char *initialGeometry=getenv("RENDERDOC_IR_QUERY_PLACEMENT_GEOMETRY");
    const unsigned initialGeometryCount=initialGeometry && strstr(initialGeometry,"multi64")?64:initialGeometry && strstr(initialGeometry,"multi")?2:1;
    const bool initialIndexed=initialGeometry && strcmp(initialGeometry,"triangle");
    const bool ueGeometry=initialGeometry && strcmp(initialGeometry,"ue-indexed")==0;
    const NSUInteger initialIndexLength=ueGeometry?352:64;
    const bool compactPrivateSize=getenv("RENDERDOC_IR_QUERY_COMPACT_PRIVATE_SIZE")!=nullptr;
    const bool emptyFrameBuild=getenv("RENDERDOC_IR_QUERY_EMPTY_FRAME_BUILD")!=nullptr;
    if(emptyFrameBuild && (!frame || !privateInput))return 33;
    const bool placementInput=getenv("RENDERDOC_IR_QUERY_PLACEMENT_INPUT")!=nullptr;
    if(placementInput && (!privateInput || !frame))return 32;
    const bool largePlacementInput=placementInput && !frameGeometry;
    const NSUInteger instanceOffset=largePlacementInput?0:8, instanceStride=largePlacementInput?72:80;
    const NSUInteger instanceLength=largePlacementInput?589824:count*80+8;
    id<MTLHeap> inputHeap=nil,scratchHeap=nil;
    const unsigned cbvCount=getenv("RENDERDOC_IR_QUERY_CBV_ROOTS")?strtoul(getenv("RENDERDOC_IR_QUERY_CBV_ROOTS"),nullptr,10):0;
    const NSUInteger cbvBacking=getenv("RENDERDOC_IR_QUERY_CBV_BACKING")?strtoul(getenv("RENDERDOC_IR_QUERY_CBV_BACKING"),nullptr,10):80;
    const bool frameCBV=getenv("RENDERDOC_IR_QUERY_FRAME_CBV")!=nullptr;
    const char *cbvStorage=getenv("RENDERDOC_IR_QUERY_CBV_STORAGE");
    const bool privateCBV=cbvStorage && strcmp(cbvStorage,"shared");
    const bool placementCBV=cbvStorage && !strcmp(cbvStorage,"private-placement");
    const bool mixedRoots=getenv("RENDERDOC_IR_QUERY_MIXED_ROOTS")!=nullptr;
    if((cbvBacking!=80 && cbvBacking!=2097152) || (cbvBacking!=80 && placementCBV))return 38;
    const NSUInteger cbvOffset=getenv("RENDERDOC_IR_QUERY_CBV_OFFSET")?strtoul(getenv("RENDERDOC_IR_QUERY_CBV_OFFSET"),nullptr,10):16;
    if(cbvCount && ((cbvCount!=5 && cbvCount!=6) || (cbvOffset!=16 && cbvOffset!=64 && !(cbvOffset==58624 && cbvBacking==2097152))))return 38;
    const NSUInteger samplerRootOffset=cbvOffset>64?16:cbvOffset;
    const auto rootEntryOffset=[&](unsigned index){return mixedRoots && index==cbvCount-1?samplerRootOffset:cbvOffset;};
    const char *readTextureStorage=getenv("RENDERDOC_IR_QUERY_READ_TEXTURE_STORAGE");
    const bool privateReadTexture=readTextureStorage && strcmp(readTextureStorage,"shared");
    const bool placementReadTexture=readTextureStorage && !strcmp(readTextureStorage,"private-placement");
    const bool readTextureWritable=getenv("RENDERDOC_IR_QUERY_READ_TEXTURE_WRITABLE")!=nullptr;
    id<MTLHeap> readTextureHeap=nil;
    const char *outputTextureFormat=getenv("RENDERDOC_IR_QUERY_OUTPUT_TEXTURE");
    const bool textureOutput=outputTextureFormat && strcmp(outputTextureFormat,"none");
    const bool floatTextureOutput=textureOutput && !strcmp(outputTextureFormat,"rgba32float");
    const bool texelOutput=textureOutput && !strcmp(outputTextureFormat,"bufferuint");
    const char *outputStorage=getenv("RENDERDOC_IR_QUERY_OUTPUT_STORAGE");
    const bool privateOutput=outputStorage && strcmp(outputStorage,"shared");
    const bool placementOutput=outputStorage && !strcmp(outputStorage,"private-placement");
    const bool heapQuery=getenv("RENDERDOC_IR_QUERY_HEAP_QUERY")!=nullptr;
    const char *queryIndirect=getenv("RENDERDOC_IR_QUERY_INDIRECT_GROUPS");
    const unsigned queryGroups=queryIndirect?strtoul(queryIndirect,nullptr,10):1;
    const unsigned queryGridHeight=getenv("RENDERDOC_IR_QUERY_GRID_HEIGHT")?strtoul(getenv("RENDERDOC_IR_QUERY_GRID_HEIGHT"),nullptr,10):1;
    if(queryGridHeight!=1 && (queryGridHeight!=66 || !cbvCount || queryGroups!=132))return 38;
    const bool sameQueryEncoder=getenv("RENDERDOC_IR_QUERY_SAME_ENCODER")!=nullptr;
    const bool dynamicOutputs=getenv("RENDERDOC_IR_QUERY_DYNAMIC_OUTPUTS")!=nullptr;
    if(dynamicOutputs && (!mixedRoots || !textureOutput || floatTextureOutput ||
        queryGroups!=132 || !queryIndirect || sameQueryEncoder || emptyFrameBuild))return 42;
    if(sameQueryEncoder && !queryIndirect)return 37;
    const NSUInteger queryArgumentOffset=getenv("RENDERDOC_IR_QUERY_INDIRECT_OFFSET")?
        strtoul(getenv("RENDERDOC_IR_QUERY_INDIRECT_OFFSET"),nullptr,10):16;
    if(queryIndirect && (!heapQuery || queryGroups>132 || !queryGroups ||
        queryArgumentOffset%4 || queryArgumentOffset>52))return 37;
    const bool heapFrameHeader=getenv("RENDERDOC_IR_QUERY_HEAP_FRAME_HEADER")!=nullptr;
    if(heapQuery && frame && (!heapFrameHeader || dynamicHeader))return 31;
    if(heapFrameHeader && (!heapQuery || !frame))return 31;
    if(cbvCount && !heapFrameHeader)return 38;
    if(dynamicHeader && (!frame || !strstr(mode,"new-target")))return 28;
    const unsigned geometryCount=multiGeometry?(strstr(mode,"multi64")?64:2):1;
    __attribute__((objc_precise_lifetime)) id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device.supportsRaytracing) return 2;
    auto placementBuffer=[&](NSUInteger length,id<MTLHeap> __strong &heap) {
      const auto options=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;
      auto alloc=[device heapBufferSizeAndAlignWithLength:length options:options];
      auto desc=[MTLHeapDescriptor new];desc.type=MTLHeapTypePlacement;desc.storageMode=MTLStorageModePrivate;
      desc.hazardTrackingMode=MTLHazardTrackingModeTracked;desc.size=MAX(NSUInteger(4096),alloc.size+alloc.align);
      heap=[device newHeapWithDescriptor:desc];
      return [heap newBufferWithLength:length options:options offset:alloc.align];
    };

    NSError *error = nil;
    auto library = [device newLibraryWithURL:[NSURL fileURLWithPath:
        [NSString stringWithUTF8String:argv[1]]] error:&error];
    if(!library || library.functionNames.count != 1) return 3;
    auto function = [library newFunctionWithName:library.functionNames[0]];
    auto pipeline = [device newComputePipelineStateWithFunction:function error:&error];
    if(!pipeline) {fprintf(stderr,"query pipeline: %s\n",error.description.UTF8String);return 4;}
    printf("Query function=%s\n",function.name.UTF8String);
    auto queue = [device newCommandQueue];
    const float vertices[] = {-1,-1,0, 1,-1,0, 0,1,0};
    auto vertex = [device newBufferWithBytes:vertices length:sizeof(vertices) options:MTLResourceStorageModeShared];
    auto geometry = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    geometry.vertexBuffer=vertex;geometry.vertexStride=12;geometry.triangleCount=1;geometry.opaque=YES;
    auto primitive = [MTLPrimitiveAccelerationStructureDescriptor descriptor];primitive.geometryDescriptors=@[geometry];
    id<MTLHeap> initialVertexHeap=nil,initialIndexHeap=nil;
    id<MTLBuffer> initialVertexBuffer=nil,initialIndexBuffer=nil;
    if(initialGeometry)
    {
      uint8_t vertexBytes[256]={},indexBytes[352]={};
      const float near[3][4]={{-1,-1,0,1},{1,-1,0,1},{0,1,0,1}};
      const float far[3][4]={{99,-1,0,1},{101,-1,0,1},{100,1,0,1}};
      for(unsigned i=0;i<3;i++)
      { memcpy(vertexBytes+16+i*32,initialGeometryCount>1?far[i]:near[i],16);memcpy(vertexBytes+160+i*16,near[i],16); }
      const uint32_t nan=0x7fc00000;memcpy(vertexBytes+236,&nan,4);
      const uint16_t idx16[3]={2,0,1};const uint32_t idx32[3]={2,0,1};
      memcpy(indexBytes+4,idx16,6);memcpy(indexBytes+24,idx32,12);
      if(ueGeometry)
      {
        for(unsigned i=0;i<3;i++){memcpy(vertexBytes+i*12,far[i],12);memcpy(vertexBytes+36+i*12,near[i],12);}
        for(unsigned i=0;i<48;i++){const uint16_t indices[3]={uint16_t(i==47?3:0),uint16_t(i==47?4:1),uint16_t(i==47?5:2)};memcpy(indexBytes+i*6,indices,6);}
      }
      initialVertexBuffer=placementBuffer(256,initialVertexHeap);
      if(initialIndexed)initialIndexBuffer=placementBuffer(initialIndexLength,initialIndexHeap);
      auto uploadVertices=[device newBufferWithBytes:vertexBytes length:256 options:MTLResourceStorageModeShared];
      auto uploadIndices=[device newBufferWithBytes:indexBytes length:initialIndexLength options:MTLResourceStorageModeShared];
      auto upload=[queue commandBuffer];auto blit=[upload blitCommandEncoder];
      [blit copyFromBuffer:uploadVertices sourceOffset:0 toBuffer:initialVertexBuffer destinationOffset:0 size:256];
      if(initialIndexed)[blit copyFromBuffer:uploadIndices sourceOffset:0 toBuffer:initialIndexBuffer destinationOffset:0 size:initialIndexLength];
      [blit endEncoding];[upload commit];[upload waitUntilCompleted];if(upload.error)return 35;
      NSMutableArray *items=[NSMutableArray array];
      for(unsigned i=0;i<initialGeometryCount;i++)
      {
        const bool last=initialGeometryCount>1 && i+1==initialGeometryCount;
        auto item=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];item.vertexBuffer=initialVertexBuffer;
        item.vertexBufferOffset=last?160:16;item.vertexStride=last?16:32;item.vertexFormat=last?MTLAttributeFormatFloat3:MTLAttributeFormatFloat4;
        item.triangleCount=1;item.opaque=YES;
        if(initialIndexed){item.indexBuffer=initialIndexBuffer;item.indexBufferOffset=last?24:4;item.indexType=last?MTLIndexTypeUInt32:MTLIndexTypeUInt16;}
        if(ueGeometry){item.vertexBufferOffset=0;item.vertexStride=12;item.vertexFormat=MTLAttributeFormatFloat3;item.indexBufferOffset=0;item.indexType=MTLIndexTypeUInt16;item.triangleCount=48;item.opaque=NO;}
        [items addObject:item];
      }
      primitive.geometryDescriptors=items;
    }
    MTLPrimitiveAccelerationStructureDescriptor *framePrimitive=nil;
    id<MTLHeap> frameVertexHeap=nil,frameIndexHeap=nil,frameScratchHeap=nil;
    id<MTLBuffer> geometryVertices=nil,geometryIndices=nil,vertexUpload=nil,indexUpload=nil,
        vertexVerify=nil,indexVerify=nil,geometryScratch=nil;
    MTLAccelerationStructureSizes frameSizes={};
    if(frameGeometry)
    {
      if(!frame || !privateInput)return 21;
      const NSUInteger vertexLength=multiGeometry?256:72,indexLength=multiGeometry?64:24;
      uint8_t vertices[256]={};
      const float triangle[3][4]={{-1,-1,0.25,1},{1,-1,0.25,1},{0,1,0.25,1}};
      for(unsigned i=0;i<3;i++)memcpy(vertices+(multiGeometry?160:16)+i*16,triangle[i],16);
      if(multiGeometry)
      {
        const float distant[3][4]={{99,-1,0.25,1},{101,-1,0.25,1},{100,1,0.25,1}};
        for(unsigned i=0;i<3;i++)memcpy(vertices+16+i*32,distant[i],16);
        const uint32_t nan=0x7fc00000;memcpy(vertices+236,&nan,4);
      }
      vertexUpload=[device newBufferWithBytes:vertices length:vertexLength options:MTLResourceStorageModeShared];
      geometryVertices=placementFrameGeometry?placementBuffer(vertexLength,frameVertexHeap):[device newBufferWithLength:vertexLength options:MTLResourceStorageModePrivate];
      const uint8_t zero[256]={};
      vertexVerify=[device newBufferWithBytes:zero length:vertexLength options:MTLResourceStorageModeShared];
      if(indexedGeometry)
      {
        uint8_t bytes[64]={};const uint32_t indices[3]={2,0,1};memcpy(bytes+(multiGeometry?24:4),indices,12);
        if(multiGeometry){const uint16_t small[3]={2,0,1};memcpy(bytes+4,small,6);}
        indexUpload=[device newBufferWithBytes:bytes length:indexLength options:MTLResourceStorageModeShared];
        geometryIndices=placementFrameGeometry?placementBuffer(indexLength,frameIndexHeap):[device newBufferWithLength:indexLength options:MTLResourceStorageModePrivate];
        indexVerify=[device newBufferWithBytes:zero length:indexLength options:MTLResourceStorageModeShared];
      }
      NSMutableArray *geometries=[NSMutableArray array];
      for(unsigned i=0;i<geometryCount;i++)
      {
        const bool last=i+1==geometryCount;auto item=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
        item.vertexBuffer=geometryVertices;item.vertexBufferOffset=multiGeometry && last?160:16;
        item.vertexStride=multiGeometry && !last?32:16;item.vertexFormat=multiGeometry && !last?MTLAttributeFormatFloat4:MTLAttributeFormatFloat3;
        item.triangleCount=1;item.opaque=YES;item.allowDuplicateIntersectionFunctionInvocation=YES;
        item.intersectionFunctionTableOffset=last && multiGeometry?1:0;
        if(indexedGeometry){item.indexBuffer=geometryIndices;item.indexBufferOffset=multiGeometry && last?24:4;
          item.indexType=multiGeometry && !last?MTLIndexTypeUInt16:MTLIndexTypeUInt32;}
        [geometries addObject:item];
      }
      framePrimitive=[MTLPrimitiveAccelerationStructureDescriptor descriptor];framePrimitive.geometryDescriptors=geometries;
      frameSizes=[device accelerationStructureSizesWithDescriptor:framePrimitive];
      geometryScratch=placementFrameGeometry?placementBuffer(frameSizes.buildScratchBufferSize+256,frameScratchHeap):[device newBufferWithLength:frameSizes.buildScratchBufferSize+256 options:MTLResourceStorageModePrivate];
    }
    const auto blasSize = [device accelerationStructureSizesWithDescriptor:primitive];
    auto blas = [device newAccelerationStructureWithSize:MAX(blasSize.accelerationStructureSize,frameSizes.accelerationStructureSize)];
    auto blasScratch=[device newBufferWithLength:blasSize.buildScratchBufferSize options:MTLResourceStorageModePrivate];
    if(!blas || !blasScratch)return 5;
    auto command=[queue commandBuffer];auto build=[command accelerationStructureCommandEncoder];
    [build buildAccelerationStructure:blas descriptor:primitive scratchBuffer:blasScratch scratchBufferOffset:0];
    [build endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 6;
    if(initialGeometry)
    {
      auto clear=[queue commandBuffer];auto blit=[clear blitCommandEncoder];
      [blit fillBuffer:initialVertexBuffer range:NSMakeRange(0,256) value:0];
      if(initialIndexed)[blit fillBuffer:initialIndexBuffer range:NSMakeRange(0,initialIndexLength) value:0];
      auto verify=[device newBufferWithLength:256+initialIndexLength options:MTLResourceStorageModeShared];
      [blit copyFromBuffer:initialVertexBuffer sourceOffset:0 toBuffer:verify destinationOffset:0 size:256];
      if(initialIndexed)[blit copyFromBuffer:initialIndexBuffer sourceOffset:0 toBuffer:verify destinationOffset:256 size:initialIndexLength];
      [blit endEncoding];[clear commit];[clear waitUntilCompleted];if(clear.error)return 35;
      for(unsigned i=0;i<(initialIndexed?256+initialIndexLength:256);i++)if(((uint8_t *)verify.contents)[i])return 35;
      printf("PASS initial Private placement geometry frozen then cleared: 256/%u bytes, %u geometries\n",unsigned(initialIndexed?initialIndexLength:0),initialGeometryCount);
    }
    if(compactPrivateSize)
    {
      id<MTLHeap> sizeHeap=nil;
      auto privateSize=placementBuffer(4096,sizeHeap);
      auto readback=[device newBufferWithLength:8 options:MTLResourceStorageModeShared];
      command=[queue commandBuffer];build=[command accelerationStructureCommandEncoder];
      [build writeCompactedAccelerationStructureSize:blas toBuffer:privateSize offset:256 sizeDataType:MTLDataTypeULong];
      [build endEncoding];auto blit=[command blitCommandEncoder];
      [blit copyFromBuffer:privateSize sourceOffset:256 toBuffer:readback destinationOffset:0 size:8];
      [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 34;
      uint64_t compactSize=0;memcpy(&compactSize,readback.contents,8);
      if(!compactSize || compactSize>=blas.size)return 34;
      auto compacted=[device newAccelerationStructureWithSize:compactSize];
      command=[queue commandBuffer];build=[command accelerationStructureCommandEncoder];
      [build copyAndCompactAccelerationStructure:blas toAccelerationStructure:compacted];
      [build endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 34;
      blas=compacted;
      printf("PASS Private placement compact size offset256=%llu; native compact BLAS queried\n",(unsigned long long)compactSize);
    }
    if(!blas.gpuResourceID._impl)return 14;
    MTLAccelerationStructureInstanceDescriptor instance = {};
    instance.transformationMatrix.columns[0].x=1;instance.transformationMatrix.columns[1].y=1;
    instance.transformationMatrix.columns[2].z=1;instance.mask=0xff;
    auto instances=[device newBufferWithBytes:&instance length:sizeof(instance) options:MTLResourceStorageModeShared];
    auto top = [MTLInstanceAccelerationStructureDescriptor descriptor];
    top.instanceDescriptorBuffer=instances;top.instanceCount=1;top.instancedAccelerationStructures=@[blas];
    if(indirect)
    {
      std::vector<uint8_t> backing(instanceLength,0);
      for(unsigned i=0;i<count;i++)
      {
        MTLIndirectAccelerationStructureInstanceDescriptor data={};
        data.transformationMatrix=instance.transformationMatrix;data.mask=masked?0:0xff;data.userID=73+i;
        if(i+1!=count)data.transformationMatrix.columns[3].x=100;
        if(!empty && !masked)data.accelerationStructureID=blas.gpuResourceID;
        memcpy(backing.data()+instanceOffset+i*instanceStride,&data,sizeof(data));
      }
      auto upload=[device newBufferWithBytes:backing.data() length:backing.size() options:MTLResourceStorageModeShared];
      instances=upload;
      if(privateInput)
      {
        instances=placementInput?placementBuffer(backing.size(),inputHeap):[device newBufferWithLength:backing.size() options:MTLResourceStorageModePrivate];
        command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
        [blit copyFromBuffer:upload sourceOffset:0 toBuffer:instances destinationOffset:0 size:backing.size()];
        [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 6;
      }
      top.instanceDescriptorBuffer=instances;top.instanceDescriptorBufferOffset=instanceOffset;top.instanceDescriptorStride=instanceStride;
      top.instanceDescriptorType=MTLAccelerationStructureInstanceDescriptorTypeIndirect;
      top.instanceCount=empty?0:count;top.instancedAccelerationStructures=nil;
    }
    auto tlasSize=[device accelerationStructureSizesWithDescriptor:top];
    if(frame)
    {
      if(!privateInput)return 18;
      const auto initialCount=top.instanceCount;top.instanceCount=count;
      const auto activeSize=[device accelerationStructureSizesWithDescriptor:top];top.instanceCount=initialCount;
      tlasSize.accelerationStructureSize=MAX(tlasSize.accelerationStructureSize,activeSize.accelerationStructureSize);
      tlasSize.buildScratchBufferSize=MAX(tlasSize.buildScratchBufferSize,activeSize.buildScratchBufferSize);
    }
    const bool newTarget=strstr(mode,"new-target");
    if(newTarget && !frame)return 23;
    auto tlas=[device newAccelerationStructureWithSize:tlasSize.accelerationStructureSize];
    auto queryTLAS=newTarget?[device newAccelerationStructureWithSize:tlasSize.accelerationStructureSize]:tlas;
    if(!queryTLAS)return 23;
    auto scratch=placementInput?placementBuffer(largePlacementInput?MAX(NSUInteger(1376256),tlasSize.buildScratchBufferSize):tlasSize.buildScratchBufferSize,scratchHeap):
        [device newBufferWithLength:tlasSize.buildScratchBufferSize options:MTLResourceStorageModePrivate];
    if(placementInput)printf("PASS placement input=%lu scratch=%lu offset=%lu stride=%lu options=%lu\n",
        (unsigned long)instances.length,(unsigned long)scratch.length,(unsigned long)instanceOffset,
        (unsigned long)instanceStride,(unsigned long)instances.resourceOptions);
    if(!blas || !tlas || !scratch) return 5;
    command=[queue commandBuffer];build=[command accelerationStructureCommandEncoder];
    [build buildAccelerationStructure:tlas descriptor:top scratchBuffer:scratch scratchBufferOffset:0];
    [build endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 6;
    auto verify=privateInput?[device newBufferWithLength:instances.length options:MTLResourceStorageModeShared]:nil;
    auto clearInput=[&]() {
      if(!privateInput)return true;
      auto clearCommand=[queue commandBuffer];auto blit=[clearCommand blitCommandEncoder];
      [blit fillBuffer:instances range:NSMakeRange(0,instances.length) value:0];
      [blit copyFromBuffer:instances sourceOffset:0 toBuffer:verify destinationOffset:0 size:instances.length];
      [blit endEncoding];[clearCommand commit];[clearCommand waitUntilCompleted];if(clearCommand.error)return false;
      const std::vector<uint8_t> zero(instances.length,0);if(memcmp(verify.contents,zero.data(),zero.size()))return false;
      printf("PASS Private/GPU query instance input cleared after AS build: %lu bytes zero\n",(unsigned long)instances.length);
      return true;
    };
    if(!clearInput())return 16;
    id<MTLBuffer> frameUpload=nil;
    if(frame)
    {
      std::vector<uint8_t> backing(instanceLength,0);
      for(unsigned i=0;i<count;i++)
      {
        MTLIndirectAccelerationStructureInstanceDescriptor data={};data.transformationMatrix=instance.transformationMatrix;
        data.mask=0xff;data.userID=74+i;data.accelerationStructureID=blas.gpuResourceID;
        if(i+1!=count)data.transformationMatrix.columns[3].x=100;
        memcpy(backing.data()+instanceOffset+i*instanceStride,&data,sizeof(data));
      }
      frameUpload=[device newBufferWithBytes:backing.data() length:backing.size() options:MTLResourceStorageModeShared];
    }
    const uint32_t initial[]={7,7,7,7};
    const std::vector<uint32_t> queryInitial(queryIndirect?queryGroups*4:4,7);
    auto output=[device newBufferWithBytes:queryInitial.data() length:queryInitial.size()*4 options:MTLResourceStorageModeShared];
    id<MTLHeap> outputHeap=nil;
    if(privateOutput && !textureOutput)
    {
      if(!cbvCount || !heapFrameHeader)return 39;
      auto upload=output;
      output=placementOutput?placementBuffer(upload.length,outputHeap):[device newBufferWithLength:upload.length options:MTLResourceStorageModePrivate];
      if(!output)return 39;
      auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
      [blit copyFromBuffer:upload sourceOffset:0 toBuffer:output destinationOffset:0 size:upload.length];
      [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 39;
    }
    id<MTLTexture> resultTexture=nil,texelInput=nil;id<MTLBuffer> texelInputParent=nil;
    NSUInteger texelOffset=256;
    if(texelOutput)
    {
      if(!privateOutput || !cbvCount || !mixedRoots || !heapFrameHeader)return 40;
      const NSUInteger alignment=[device minimumLinearTextureAlignmentForPixelFormat:MTLPixelFormatR32Uint];
      const NSUInteger row=((queryInitial.size()*4+alignment-1)/alignment)*alignment;
      texelOffset=std::max(texelOffset,[device minimumTextureBufferAlignmentForPixelFormat:MTLPixelFormatR32Uint]);
      std::vector<uint8_t> initial(texelOffset+row+256,0xcd);
      memcpy(initial.data()+texelOffset,queryInitial.data(),queryInitial.size()*4);
      auto upload=[device newBufferWithBytes:initial.data() length:initial.size() options:MTLResourceStorageModeShared];
      output=placementOutput?placementBuffer(initial.size(),outputHeap):[device newBufferWithLength:initial.size() options:MTLResourceStorageModePrivate];
      if(!output)return 40;
      auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
      [blit copyFromBuffer:upload sourceOffset:0 toBuffer:output destinationOffset:0 size:initial.size()];
      [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 40;
      auto td=[MTLTextureDescriptor new];td.textureType=MTLTextureTypeTextureBuffer;
      td.pixelFormat=MTLPixelFormatR32Uint;td.width=queryInitial.size();td.height=1;td.depth=1;
      td.storageMode=MTLStorageModePrivate;td.resourceOptions=MTLResourceStorageModePrivate;
      td.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;td.allowGPUOptimizedContents=NO;
      resultTexture=[output newTextureWithDescriptor:td offset:texelOffset bytesPerRow:row];
      if(!resultTexture)return 40;
      resultTexture.label=@"IR query typed buffer UAV";
      std::vector<uint8_t> readInitial(texelOffset+row+256,0xab);
      const uint32_t readValues[4]={5,5,5,5};memcpy(readInitial.data()+texelOffset,readValues,16);
      auto readUpload=[device newBufferWithBytes:readInitial.data() length:readInitial.size() options:MTLResourceStorageModeShared];
      texelInputParent=[device newBufferWithLength:readInitial.size() options:MTLResourceStorageModePrivate];
      command=[queue commandBuffer];blit=[command blitCommandEncoder];
      [blit copyFromBuffer:readUpload sourceOffset:0 toBuffer:texelInputParent destinationOffset:0 size:readInitial.size()];
      [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 40;
      td.width=4;td.usage=MTLTextureUsageShaderRead;
      texelInput=[texelInputParent newTextureWithDescriptor:td offset:texelOffset bytesPerRow:row];if(!texelInput)return 40;

    }
    else if(textureOutput)
    {
      if(!cbvCount || !mixedRoots || !heapFrameHeader)return 40;
      auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:floatTextureOutput?MTLPixelFormatRGBA32Float:MTLPixelFormatR32Uint width:4 height:queryIndirect?queryGroups:1 mipmapped:NO];
      td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
      auto upload=[device newTextureWithDescriptor:td];
      const NSUInteger components=floatTextureOutput?4:1;
      std::vector<uint32_t> values(td.width*td.height*components,7);
      if(floatTextureOutput)for(auto &value:values){float seven=7;memcpy(&value,&seven,4);}
      [upload replaceRegion:MTLRegionMake2D(0,0,td.width,td.height) mipmapLevel:0 withBytes:values.data() bytesPerRow:td.width*components*4];
      resultTexture=upload;
      if(privateOutput)
      {
        td.storageMode=MTLStorageModePrivate;
        if(placementOutput)
        {
          td.hazardTrackingMode=MTLHazardTrackingModeTracked;
          auto size=[device heapTextureSizeAndAlignWithDescriptor:td];auto hd=[MTLHeapDescriptor new];
          hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=size.size+size.align;
          outputHeap=[device newHeapWithDescriptor:hd];resultTexture=[outputHeap newTextureWithDescriptor:td offset:size.align];
        }
        else resultTexture=[device newTextureWithDescriptor:td];
        if(!resultTexture)return 40;
        auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
        [blit copyFromTexture:upload sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(td.width,td.height,1) toTexture:resultTexture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
        [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 40;
      }
      resultTexture.label=@"IR query texture UAV";
    }
    id<MTLResource> queryResult=resultTexture?(id<MTLResource>)resultTexture:output;
    __attribute__((objc_precise_lifetime)) NSMutableArray *dynamicHeaps=[NSMutableArray new];
    __attribute__((objc_precise_lifetime)) NSMutableArray *primaryTextures=[NSMutableArray new],*secondaryTextures=[NSMutableArray new];
    if(dynamicOutputs)
    {
      [primaryTextures addObject:resultTexture];
      auto createTexture=[&](MTLPixelFormat format) -> id<MTLTexture> {
        auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:4 height:queryGroups mipmapped:NO];
        td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
        auto upload=[device newTextureWithDescriptor:td];if(!upload)return nil;
        std::vector<uint32_t> bytes(queryGroups*4,7);
        if(format==MTLPixelFormatRGBA8Unorm)for(auto &v:bytes)v=0xff24180c;
        [upload replaceRegion:MTLRegionMake2D(0,0,4,queryGroups) mipmapLevel:0 withBytes:bytes.data() bytesPerRow:16];
        if(!privateOutput)return upload;
        td.storageMode=MTLStorageModePrivate;
        id<MTLTexture> texture=nil;
        if(placementOutput)
        {
          td.hazardTrackingMode=MTLHazardTrackingModeTracked;
          auto layout=[device heapTextureSizeAndAlignWithDescriptor:td];auto hd=[MTLHeapDescriptor new];
          hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=layout.size+layout.align;
          auto heap=[device newHeapWithDescriptor:hd];if(!heap)return nil;[dynamicHeaps addObject:heap];
          texture=[heap newTextureWithDescriptor:td offset:layout.align];
        }
        else texture=[device newTextureWithDescriptor:td];
        if(!texture)return nil;
        auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
        [blit copyFromTexture:upload sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(4,queryGroups,1) toTexture:texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
        [blit endEncoding];[command commit];[command waitUntilCompleted];return command.error?nil:texture;
      };
      for(unsigned i=1;i<3;i++){auto t=createTexture(MTLPixelFormatR32Uint);if(!t)return 42;[primaryTextures addObject:t];}
      for(unsigned i=0;i<2;i++){auto t=createTexture(MTLPixelFormatRGBA8Unorm);if(!t)return 42;[secondaryTextures addObject:t];}
    }
    id<MTLHeap> queryArgumentHeap=nil;id<MTLBuffer> queryArguments=nil;
    std::vector<id<MTLBuffer>> queryArgumentUploads;
    if(queryIndirect)
    {
      uint8_t initialArguments[64];memset(initialArguments,0xcd,64);memset(initialArguments+queryArgumentOffset,0,12);
      queryArguments=placementBuffer(64,queryArgumentHeap);
      auto upload=[device newBufferWithBytes:initialArguments length:64 options:MTLResourceStorageModeShared];
      auto initialize=[queue commandBuffer];auto blit=[initialize blitCommandEncoder];
      [blit copyFromBuffer:upload sourceOffset:0 toBuffer:queryArguments destinationOffset:0 size:64];
      [blit endEncoding];[initialize commit];[initialize waitUntilCompleted];if(initialize.error)return 37;
      for(uint32_t width:{1U,queryGroups,0U})
      {
        const uint32_t groups[3]={width==1?1:width/queryGridHeight,width==1?1:queryGridHeight,1};
        queryArgumentUploads.push_back([device newBufferWithBytes:groups length:12 options:MTLResourceStorageModeShared]);
      }
      queryArguments.label=@"IR per-use Private query arguments";
    }
    const bool explicitHeader=getenv("RENDERDOC_IR_QUERY_EXPLICIT_HEADER")!=nullptr;
    const NSUInteger contributionOffset=getenv("RENDERDOC_IR_QUERY_CONTRIBUTION_OFFSET")?strtoul(getenv("RENDERDOC_IR_QUERY_CONTRIBUTION_OFFSET"),nullptr,10):0;
    if(contributionOffset%4 || contributionOffset>4096)return 25;
    const NSUInteger headerOffset=getenv("RENDERDOC_IR_QUERY_HEADER_OFFSET")?strtoul(getenv("RENDERDOC_IR_QUERY_HEADER_OFFSET"),nullptr,10):0;
    if(headerOffset%8 || headerOffset>16304 || (heapFrameHeader && headerOffset))return 24;
    const NSUInteger headerLength=headerOffset?headerOffset+80:64;
    id<MTLHeap> headerHeap=nil;id<MTLBuffer> header=nil;
    if(heapHeader) {
      auto desc=[MTLHeapDescriptor new];desc.type=MTLHeapTypePlacement;desc.storageMode=MTLStorageModeShared;desc.hazardTrackingMode=MTLHazardTrackingModeTracked;
      const auto allocation=[device heapBufferSizeAndAlignWithLength:headerLength options:MTLResourceStorageModeShared];
      desc.size=MAX((NSUInteger)4096,allocation.size+allocation.align);headerHeap=[device newHeapWithDescriptor:desc];
      header=[headerHeap newBufferWithLength:headerLength options:MTLResourceStorageModeShared offset:allocation.align];
    } else header=[device newBufferWithLength:headerLength options:MTLResourceStorageModeShared];
    auto contributions=[device newBufferWithLength:count*4+contributionOffset+(dynamicHeader?16:0)
        options:privateContribution?MTLResourceStorageModePrivate:MTLResourceStorageModeShared];
    auto contributionUpload=privateContribution?[device newBufferWithLength:contributions.length options:MTLResourceStorageModeShared]:contributions;
    if(!header || !contributions || !contributionUpload)return 28;
    const std::vector<uint32_t> contribution(count,0);memset(header.contents,0xa5,header.length);
    memset(contributionUpload.contents,0xa5,contributions.length);
    memset((uint8_t *)contributionUpload.contents+contributionOffset,0,contributions.length-contributionOffset);
    memset((uint8_t *)header.contents+headerOffset,0,64);
    IRRaytracingSetAccelerationStructure((uint8_t *)header.contents+headerOffset,(dynamicHeader?tlas:queryTLAS).gpuResourceID,
        (uint8_t *)contributionUpload.contents+contributionOffset,contributions.gpuAddress+contributionOffset,contribution.data(),empty?0:count);
    if(privateContribution) {
      auto upload=[queue commandBuffer];auto blit=[upload blitCommandEncoder];
      [blit copyFromBuffer:contributionUpload sourceOffset:0 toBuffer:contributions destinationOffset:0 size:contributions.length];
      [blit endEncoding];[upload commit];[upload waitUntilCompleted];if(upload.error)return 28;
    }
    const NSUInteger rootOffset=getenv("RENDERDOC_IR_QUERY_ROOT_OFFSET")?strtoul(getenv("RENDERDOC_IR_QUERY_ROOT_OFFSET"),nullptr,10):0;
    if((rootOffset!=0 && rootOffset!=8) || (cbvCount && rootOffset))return 15;
    const uint64_t words[]={heapQuery?output.gpuAddress:header.gpuAddress+headerOffset,heapQuery?1ULL:output.gpuAddress};
    const uint64_t padded[]={0xa5a5d00d12345678ULL,words[0],words[1],0x8877665544332211ULL};
    auto roots=[device newBufferWithBytes:rootOffset?(const void *)padded:(const void *)words
        length:rootOffset?sizeof(padded):sizeof(words) options:MTLResourceStorageModeShared];
    std::vector<id<MTLBuffer>> cbvs,cbvUploads;
    __attribute__((objc_precise_lifetime)) NSMutableArray *querySamplers=[NSMutableArray new];
    __attribute__((objc_precise_lifetime)) NSMutableArray *cbvHeaps=[NSMutableArray new];
    id<MTLTexture> sampledTexture=nil;
    if(mixedRoots && !cbvCount)return 38;
    if(cbvCount)
    {
      uint32_t data[6][4]={{1,queryGroups/queryGridHeight*4,queryGroups*4,0},{2,4,0,0},
        {0xbf800000,0x3f800000,0,0},{0x40000000,0x3f000000,0,0},
        {0x40400000,0x3e800000,11,0},{17,0,0,0}};
      if(mixedRoots)
      {
        if(dynamicOutputs)data[1][2]=4;
        data[0][3]=3;data[2][2]=0x40400000;data[2][3]=0x3e800000;
        data[3][2]=11;data[4][0]=17;data[4][1]=data[4][2]=0;
        auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:2 height:2 mipmapped:NO];
        td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead|(readTextureWritable?MTLTextureUsageShaderWrite:0);
        sampledTexture=[device newTextureWithDescriptor:td];sampledTexture.label=@"IR query sampled texture";
        const uint8_t pixels[16]={12,24,36,255,12,24,36,255,12,24,36,255,12,24,36,255};
        [sampledTexture replaceRegion:MTLRegionMake2D(0,0,2,2) mipmapLevel:0 withBytes:pixels bytesPerRow:8];
        if(privateReadTexture)
        {
          auto upload=sampledTexture;td.storageMode=MTLStorageModePrivate;
          if(placementReadTexture)
          {
            td.hazardTrackingMode=MTLHazardTrackingModeTracked;
            auto layout=[device heapTextureSizeAndAlignWithDescriptor:td];auto hd=[MTLHeapDescriptor new];
            hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=layout.size+layout.align;
            readTextureHeap=[device newHeapWithDescriptor:hd];sampledTexture=[readTextureHeap newTextureWithDescriptor:td offset:layout.align];
          }
          else sampledTexture=[device newTextureWithDescriptor:td];
          if(!sampledTexture)return 41;
          auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
          [blit copyFromTexture:upload sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(2,2,1) toTexture:sampledTexture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
          [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 41;
          printf("PASS Private%s sampled texture with %s creation permission\n",placementReadTexture?" placement":"",readTextureWritable?"ReadWrite":"Read");
        }
      }
      std::vector<uint64_t> addresses(cbvCount);
      for(unsigned i=0;i<cbvCount;i++)
      {
        const bool samplerRoot=mixedRoots && i==cbvCount-1;
        std::vector<uint8_t> guarded(samplerRoot?224:cbvBacking,0xa5);
        memcpy(guarded.data()+rootEntryOffset(i),data[i],16);
        if(samplerRoot)
        {
          IRDescriptorTableEntry entries[6]={};
          for(unsigned j=0;j<6;j++)
          {
            auto sd=[MTLSamplerDescriptor new];sd.minFilter=sd.magFilter=j<2?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
            sd.mipFilter=j<4?MTLSamplerMipFilterNearest:MTLSamplerMipFilterLinear;
            sd.sAddressMode=sd.tAddressMode=sd.rAddressMode=j%2?MTLSamplerAddressModeClampToEdge:MTLSamplerAddressModeRepeat;
            sd.supportArgumentBuffers=YES;sd.lodMaxClamp=1000;
            auto sampler=[device newSamplerStateWithDescriptor:sd];[querySamplers addObject:sampler];
            IRDescriptorTableSetSampler(&entries[j],sampler,0.0f);
          }
          memcpy(guarded.data()+rootEntryOffset(i),entries,sizeof(entries));
        }
        auto cbv=[device newBufferWithBytes:guarded.data() length:guarded.size() options:MTLResourceStorageModeShared|(cbvBacking>80 && !samplerRoot?MTLResourceCPUCacheModeWriteCombined:0)];
        cbvUploads.push_back(samplerRoot?nil:cbv);
        if(privateCBV && !samplerRoot)
        {
          auto upload=cbv;id<MTLHeap> cbvHeap=nil;
          cbv=placementCBV?placementBuffer(cbvBacking,cbvHeap):[device newBufferWithLength:cbvBacking options:MTLResourceStorageModePrivate];
          if(cbvHeap)[cbvHeaps addObject:cbvHeap];
          auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
          for(NSUInteger at=0;at<cbvBacking;at+=1024*1024)
            [blit copyFromBuffer:upload sourceOffset:at toBuffer:cbv destinationOffset:at size:MIN(NSUInteger(1024*1024),cbvBacking-at)];
          [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 38;
        }
        cbv.label=[NSString stringWithFormat:@"IR query CBV %u",i];cbvs.push_back(cbv);
        addresses[i]=cbv.gpuAddress+rootEntryOffset(i);
      }
      roots=[device newBufferWithBytes:addresses.data() length:cbvCount*8 options:MTLResourceStorageModeShared];
    }
    output.label=@"IR query results";header.label=@"IR query AS header";roots.label=@"IR query roots";
    id<MTLBuffer> beforeOutput=nil,beforeRoots=nil,beforeHeader=header;
    if(frame)
    {
      beforeOutput=[device newBufferWithBytes:initial length:sizeof(initial) options:MTLResourceStorageModeShared];
      beforeRoots=[device newBufferWithBytes:roots.contents length:roots.length options:MTLResourceStorageModeShared];
      if(newTarget && !dynamicHeader) {
        beforeHeader=[device newBufferWithBytes:header.contents length:header.length options:MTLResourceStorageModeShared];
        ((uint64_t *)((uint8_t *)beforeHeader.contents+headerOffset))[0]=tlas.gpuResourceID._impl;
        beforeHeader.label=@"IR query before AS header";
      }
      ((uint64_t *)((uint8_t *)beforeRoots.contents+rootOffset))[0]=beforeHeader.gpuAddress+headerOffset;
      ((uint64_t *)((uint8_t *)beforeRoots.contents+rootOffset))[1]=beforeOutput.gpuAddress;
      beforeOutput.label=@"IR query before results";beforeRoots.label=@"IR query before roots";
    }
    blas.label=@"IR query BLAS";tlas.label=@"IR query TLAS";
    RENDERDOC_API_1_7_0 *api=nullptr;
    auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    if(get)get(eRENDERDOC_API_Version_1_7_0,(void **)&api);
    if(api && getenv("RENDERDOC_IR_QUERY_REFLECTION_PATH"))
    {
      NSString *reflection=[NSString stringWithContentsOfFile:
          [NSString stringWithUTF8String:getenv("RENDERDOC_IR_QUERY_REFLECTION_PATH")]
          encoding:NSUTF8StringEncoding error:&error];
      if(!reflection.length) return 34;
      RENDERDOC_AnnotationValue value={}; value.string=reflection.UTF8String;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)pipeline,
          "metal.irComputeReflection",eRENDERDOC_String,0,&value)) return 34;
    }
    id<MTLBuffer> queryHeap=nil;
    if(heapQuery)
    {
      queryHeap=[device newBufferWithLength:(dynamicOutputs || texelOutput?5:mixedRoots?4:cbvCount?3:2)*sizeof(IRDescriptorTableEntry) options:MTLResourceStorageModeShared];
      if(!queryHeap)return 31;memset(queryHeap.contents,0,queryHeap.length);
      if(!heapFrameHeader)IRDescriptorTableSetAccelerationStructure((IRDescriptorTableEntry *)queryHeap.contents+1,header.gpuAddress+headerOffset);
      if(resultTexture)IRDescriptorTableSetTexture((IRDescriptorTableEntry *)queryHeap.contents+2,resultTexture,0.0f,0);
      else if(cbvCount)IRDescriptorTableSetBuffer((IRDescriptorTableEntry *)queryHeap.contents+2,output.gpuAddress,output.length);
      if(mixedRoots)IRDescriptorTableSetTexture((IRDescriptorTableEntry *)queryHeap.contents+3,sampledTexture,0.0f,0);
      if(dynamicOutputs)IRDescriptorTableSetTexture((IRDescriptorTableEntry *)queryHeap.contents+4,secondaryTextures[0],0.0f,0);
      if(texelOutput)IRDescriptorTableSetTexture((IRDescriptorTableEntry *)queryHeap.contents+4,texelInput,0.0f,0);
      queryHeap.label=@"IR query descriptor heap";
      if(api)
      {
        RENDERDOC_AnnotationValue coverage={};coverage.uint32=65;
        if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage))return 31;
        auto annotate=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
          RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=a;value.vector.uint64[1]=b;
          value.vector.uint64[2]=c;value.vector.uint64[3]=d;
          return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&value);
        };
        if(annotate(queryHeap,"metal.descriptorTable",1,0,dynamicOutputs || texelOutput?5:mixedRoots?4:cbvCount?3:2,24) ||
           annotate(pipeline,"metal.rayQueryHeapDispatch",(uint64_t)(__bridge void *)queryHeap,24,(uint64_t)(__bridge void *)queryResult,0))return 31;
        if(cbvCount)
        {
          for(unsigned i=0;i<cbvCount;i++)
          {
            const bool samplerRoot=mixedRoots && i==cbvCount-1;
            if(annotate(pipeline,"metal.rayQueryHeapCBVRoot",i*8,samplerRoot?144:16,48,cbvCount))return 38;
            if(mixedRoots && annotate(pipeline,"metal.irComputeRoot",i*8,samplerRoot?3:4,samplerRoot?6:1,samplerRoot?144:16))return 38;
          }
          if(textureOutput && annotate(pipeline,"metal.irComputeHeapEntry",0,2,dynamicOutputs?8:4,0))return 40;
          if(texelOutput && (annotate(pipeline,"metal.irComputeHeapEntry",0,4,1,0) ||
             annotate(queryHeap,"metal.descriptorSlotEvent",96,1,0,4) ||
             annotate(queryHeap,"metal.descriptorSlotEvent",96,1,2,4) ||
             annotate(queryHeap,"metal.descriptorSlotBinding",96,1,(uint64_t)(__bridge void *)texelInput,0)))return 40;
          if(dynamicOutputs && (annotate(pipeline,"metal.irComputeHeapEntry",0,4,8,0) ||
             annotate(queryHeap,"metal.descriptorSlotEvent",96,1,0,5) ||
             annotate(queryHeap,"metal.descriptorSlotEvent",96,1,2,5) ||
             annotate(queryHeap,"metal.descriptorSlotBinding",96,1,(uint64_t)(__bridge void *)secondaryTextures[0],0)))return 42;
          if(mixedRoots && (annotate(cbvs.back(),"metal.descriptorTable",2,samplerRootOffset,6,24) ||
             annotate(pipeline,"metal.irComputeHeapEntry",0,3,1,0) ||
             annotate(queryHeap,"metal.descriptorSlotEvent",72,1,0,4) ||
             annotate(queryHeap,"metal.descriptorSlotEvent",72,1,2,4) ||
             annotate(queryHeap,"metal.descriptorSlotBinding",72,1,(uint64_t)(__bridge void *)sampledTexture,0)))return 38;
          if(mixedRoots)for(unsigned j=0;j<6;j++)
          {
            auto table=cbvs.back();auto sampler=querySamplers[j];const uint64_t at=samplerRootOffset+j*24;
            if(annotate(table,"metal.descriptorSlotEvent",at,1,0,7) ||
               annotate(table,"metal.descriptorSlotEvent",at,1,2,7) ||
               annotate(table,"metal.descriptorSlotBinding",at,2,(uint64_t)(__bridge void *)sampler,0))return 38;
          }
          if(annotate(queryHeap,"metal.descriptorSlotEvent",48,1,0,textureOutput?5:1) ||
             annotate(queryHeap,"metal.descriptorSlotEvent",48,1,2,textureOutput?5:1) ||
             annotate(queryHeap,"metal.descriptorSlotBinding",48,textureOutput?1:0,(uint64_t)(__bridge void *)queryResult,0))return 38;
        }
        if(!heapFrameHeader && (annotate(header,"metal.rayASHeader",headerOffset,(uint64_t)(__bridge void *)queryTLAS,
            (uint64_t)(__bridge void *)contributions,contributionOffset) ||
           annotate(queryHeap,"metal.descriptorSlotEvent",24,1,0,4) ||
           annotate(queryHeap,"metal.descriptorSlotEvent",24,1,2,4) ||
           annotate(queryHeap,"metal.descriptorSlotBinding",24,3,(uint64_t)(__bridge void *)header,headerOffset)))return 31;
      }
    }
    if(api && getenv("RENDERDOC_IR_QUERY_TYPED") && !heapQuery)
    {
      RENDERDOC_AnnotationValue coverage={};coverage.uint32=3;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage))return 12;
      auto annotate=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
        RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=a;value.vector.uint64[1]=b;
        value.vector.uint64[2]=c;value.vector.uint64[3]=d;
        return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&value);
      };
      if(explicitHeader && getenv("RENDERDOC_IR_QUERY_ANNOTATION_CONTROLS"))
      {
        const uint64_t a=(uint64_t)(__bridge void *)queryTLAS,c=(uint64_t)(__bridge void *)contributions;
        unsigned rejected=0;
        auto bad=[&](id object,uint64_t offset,uint64_t structure,uint64_t contribution,uint64_t memberOffset) {
          if(annotate(object,"metal.rayASHeader",offset,structure,contribution,memberOffset)!=2)return false;
          rejected++;return true;
        };
        if(!bad(pipeline,headerOffset,a,c,contributionOffset) ||
           !bad(header,headerOffset+1,a,c,contributionOffset) ||
           !bad(header,UINT64_MAX-7,a,c,contributionOffset) ||
           !bad(header,header.length,a,c,contributionOffset) ||
           !bad(header,headerOffset,0,c,contributionOffset) ||
           !bad(header,headerOffset,1,c,contributionOffset) ||
           !bad(header,headerOffset,(uint64_t)(__bridge void *)header,c,contributionOffset) ||
           !bad(header,headerOffset,a,0,contributionOffset) ||
           !bad(header,headerOffset,a,(uint64_t)(__bridge void *)header,contributionOffset) ||
           !bad(header,headerOffset,a,c,contributionOffset+1) ||
           !bad(header,headerOffset,a,c,contributions.length))return 27;
        uint64_t *words=(uint64_t *)((uint8_t *)header.contents+headerOffset);
        words[2]=1;const bool reserved=bad(header,headerOffset,a,c,contributionOffset);words[2]=0;
        if(!reserved || annotate(header,"metal.rayASHeader",headerOffset,a,c,contributionOffset) ||
           annotate(header,"metal.rayASHeader",headerOffset,a,c,contributionOffset))return 27;
        printf("PASS explicit header annotation controls %u rejected, duplicate immutable declaration accepted\n",rejected);
      }
      if(explicitHeader && (annotate(header,"metal.rayASHeader",headerOffset,(uint64_t)(__bridge void *)(dynamicHeader?tlas:queryTLAS),
          (uint64_t)(__bridge void *)contributions,contributionOffset) ||
          (newTarget && !dynamicHeader && annotate(beforeHeader,"metal.rayASHeader",headerOffset,(uint64_t)(__bridge void *)tlas,
          (uint64_t)(__bridge void *)contributions,contributionOffset))))return 26;
      if(annotate(pipeline,"metal.rayQueryDispatch",(uint64_t)(__bridge void *)roots,rootOffset,
          (uint64_t)(__bridge void *)header,(uint64_t)(__bridge void *)output) ||
         annotate(roots,"metal.descriptorTable",0,rootOffset,2,8) ||
         annotate(header,"metal.descriptorTable",4,headerOffset,1,8) ||
         annotate(header,"metal.descriptorTable",0,headerOffset+8,1,8))return 13;
      if(frame && (annotate(pipeline,"metal.rayQueryDispatch",(uint64_t)(__bridge void *)beforeRoots,rootOffset,
          (uint64_t)(__bridge void *)beforeHeader,(uint64_t)(__bridge void *)beforeOutput) ||
          annotate(beforeRoots,"metal.descriptorTable",0,rootOffset,2,8)))return 13;
      if(newTarget && !dynamicHeader && (annotate(beforeHeader,"metal.descriptorTable",4,headerOffset,1,8) ||
          annotate(beforeHeader,"metal.descriptorTable",0,headerOffset+8,1,8)))return 13;
    }
    auto layer=[CAMetalLayer layer];layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(2,2);
    auto drawable=[layer nextDrawable];if(!drawable)return 11;
    const bool republishContribution=getenv("RENDERDOC_IR_QUERY_CONTRIBUTION_GPU_REPUBLISH")!=nullptr;
    if(republishContribution && !heapFrameHeader)return 31;
    id<MTLBuffer> contributionRepublishInput=republishContribution?
        [device newBufferWithBytes:contributionUpload.contents length:contributions.length options:MTLResourceStorageModeShared]:nil;
    if(republishContribution && !contributionRepublishInput)return 28;
    if(argc==3) {if(!api)return 7;api->SetCaptureFilePathTemplate(argv[2]);api->StartFrameCapture(nullptr,nullptr);}
    if(frameCBV)
    {
      if(!cbvCount || !mixedRoots)return 38;
      id<MTLCommandBuffer> uploadCommand=privateCBV?[queue commandBuffer]:nil;
      id<MTLBlitCommandEncoder> uploadBlit=privateCBV?[uploadCommand blitCommandEncoder]:nil;
      for(unsigned i=0;i<cbvCount-1;i++)
      {
        if(placementCBV)
        {
          auto layout=[device heapBufferSizeAndAlignWithLength:80 options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked];
          cbvs[i]=[(id<MTLHeap>)cbvHeaps[i] newBufferWithLength:80 options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:2*layout.align];
        }
        else cbvs[i]=[device newBufferWithLength:cbvBacking options:privateCBV?MTLResourceStorageModePrivate:MTLResourceStorageModeShared|(cbvBacking>80?MTLResourceCPUCacheModeWriteCombined:0)];
        cbvs[i].label=[NSString stringWithFormat:@"IR frame-born CBV %u",i];
        if(privateCBV)
          for(NSUInteger at=0;at<cbvBacking;at+=1024*1024)
            [uploadBlit copyFromBuffer:cbvUploads[i] sourceOffset:at toBuffer:cbvs[i] destinationOffset:at size:MIN(NSUInteger(1024*1024),cbvBacking-at)];
        else memcpy(cbvs[i].contents,cbvUploads[i].contents,cbvBacking);
        ((uint64_t *)roots.contents)[i]=cbvs[i].gpuAddress+rootEntryOffset(i);
      }
      if(privateCBV)
      {
        [uploadBlit endEncoding];[uploadCommand commit];[uploadCommand waitUntilCompleted];if(uploadCommand.error)return 38;
      }
      printf("PASS frame-born %u CBVs with %s data publication\n",cbvCount-1,privateCBV?"GPU blit":"CPU snapshot");
    }
    if(heapFrameHeader)
    {
      if(republishContribution)
      {
        auto publication=[queue commandBuffer];auto copy=[publication blitCommandEncoder];
        [copy copyFromBuffer:contributionRepublishInput sourceOffset:0 toBuffer:contributions destinationOffset:0 size:contributions.length];
        [copy endEncoding];[publication commit];[publication waitUntilCompleted];if(publication.error)return 28;
        printf("PASS original GPU contribution publication before typed Header, %lu bytes\n",(unsigned long)contributions.length);
      }
      uint8_t snapshot[64];memcpy(snapshot,header.contents,64);
      const auto allocation=[device heapBufferSizeAndAlignWithLength:64 options:MTLResourceStorageModeShared];
      header=heapHeader?[headerHeap newBufferWithLength:64 options:MTLResourceStorageModeShared offset:2*allocation.align]:
          [device newBufferWithLength:64 options:MTLResourceStorageModeShared];
      if(!header)return 31;memcpy(header.contents,snapshot,64);header.label=@"IR frame-born query Header";
      IRDescriptorTableSetAccelerationStructure((IRDescriptorTableEntry *)queryHeap.contents+1,header.gpuAddress);
      if(api)
      {
        auto annotate=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
          RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=a;value.vector.uint64[1]=b;
          value.vector.uint64[2]=c;value.vector.uint64[3]=d;
          return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&value);
        };
        // Publish the known Header before building its AS, as the UE factory does.
        if(annotate(header,"metal.rayASHeader",0,(uint64_t)(__bridge void *)queryTLAS,
               (uint64_t)(__bridge void *)contributions,contributionOffset) ||
           annotate(queryHeap,"metal.descriptorSlotEvent",24,1,0,4) ||
           annotate(queryHeap,"metal.descriptorSlotEvent",24,1,2,4) ||
           annotate(queryHeap,"metal.descriptorSlotBinding",24,3,(uint64_t)(__bridge void *)header,0))return 31;
      }
    }
    auto encodeQuery=[&](id<MTLCommandBuffer> queryCommand,id<MTLBuffer> queryRoots,id<MTLBuffer> queryOutput) {
    auto compute=[queryCommand computeCommandEncoder];
    [compute setComputePipelineState:pipeline];
    if(heapQuery)
    {
      [compute setBuffer:queryHeap offset:0 atIndex:kIRDescriptorHeapBindPoint];
      if(api) {
        auto annotate=[&](const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
          RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=a;value.vector.uint64[1]=b;
          value.vector.uint64[2]=c;value.vector.uint64[3]=d;
          return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)compute,key,eRENDERDOC_UInt64,4,&value);
        };
        if(cbvCount)
        {
          if(annotate("metal.descriptorInlineLayout",0,kIRArgumentBufferBindPoint,cbvCount,8))abort();
          for(unsigned i=0;i<cbvCount;i++)
            if(annotate("metal.descriptorInlineBinding",kIRArgumentBufferBindPoint,i,(uint64_t)(__bridge void *)cbvs[i],rootEntryOffset(i)))abort();
        }
        else if(annotate("metal.descriptorInlineLayout",0,kIRArgumentBufferBindPoint,1,16) ||
           annotate("metal.descriptorInlineBinding",kIRArgumentBufferBindPoint,0,(uint64_t)(__bridge void *)queryOutput,0))abort();
      }
      [compute setBytes:(uint8_t *)queryRoots.contents+rootOffset length:cbvCount?cbvCount*8:16 atIndex:kIRArgumentBufferBindPoint];
      for(auto cbv:cbvs)[compute useResource:cbv usage:MTLResourceUsageRead];
      if(sampledTexture)[compute useResource:sampledTexture usage:MTLResourceUsageRead];
      if(texelInput)[compute useResource:texelInput usage:MTLResourceUsageRead];
      [compute useResource:queryHeap usage:MTLResourceUsageRead];
    }
    else [compute setBuffer:queryRoots offset:rootOffset atIndex:kIRArgumentBufferBindPoint];
    const bool before=queryRoots==beforeRoots;
    [compute useResources:(const id<MTLResource>[]){before?tlas:queryTLAS,before?beforeHeader:header,contributions} count:3 usage:MTLResourceUsageRead];
    [compute useResource:resultTexture?(id<MTLResource>)resultTexture:queryOutput usage:MTLResourceUsageWrite];
    [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(4,1,1)];
    [compute endEncoding];
    };
    auto encodeIndirectQueries=[&](id<MTLCommandBuffer> queryCommand) {
      id<MTLComputeCommandEncoder> common=nil;
      for(unsigned step=0;step<3;step++)
      {
        id<MTLTexture> currentPrimary=dynamicOutputs?primaryTextures[step]:resultTexture;
        id<MTLTexture> currentSecondary=dynamicOutputs?secondaryTextures[step==1?1:0]:nil;
        id<MTLTexture> currentRead=dynamicOutputs && step?secondaryTextures[step-1]:sampledTexture;
        if(dynamicOutputs && step)
        {
          const id<MTLTexture> textures[]={currentPrimary,currentRead,currentSecondary};
          for(unsigned i=0;i<3;i++)
          {
            const unsigned slot=i==0?2:i==1?3:4;
            IRDescriptorTableSetTexture((IRDescriptorTableEntry *)queryHeap.contents+slot,textures[i],0.0f,0);
            if(api)
            {
              RENDERDOC_AnnotationValue event={},binding={};
              event.vector.uint64[0]=slot*24;event.vector.uint64[1]=1;event.vector.uint64[2]=2;event.vector.uint64[3]=i==1?4:5;
              binding.vector.uint64[0]=slot*24;binding.vector.uint64[1]=1;binding.vector.uint64[2]=(uint64_t)(__bridge void *)textures[i];
              if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)queryHeap,"metal.descriptorSlotEvent",eRENDERDOC_UInt64,4,&event) ||
                 api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)queryHeap,"metal.descriptorSlotBinding",eRENDERDOC_UInt64,4,&binding))abort();
            }
          }
        }
        if(!sameQueryEncoder || !step)
        {
          auto blit=[queryCommand blitCommandEncoder];
          [blit copyFromBuffer:queryArgumentUploads[sameQueryEncoder?1:step] sourceOffset:0 toBuffer:queryArguments destinationOffset:queryArgumentOffset size:12];
          [blit endEncoding];
        }
        // Reuse the existing sourced heap-query bindings, preserving the actual
        // Native indirect command and its GPU-written argument buffer.
        auto compute=common?common:[queryCommand computeCommandEncoder];
        if(sameQueryEncoder)common=compute;
        if(!sameQueryEncoder || !step)
        {
        [compute setComputePipelineState:pipeline];
        [compute setBuffer:queryHeap offset:0 atIndex:kIRDescriptorHeapBindPoint];
        if(api)
        {
          RENDERDOC_AnnotationValue layout={},binding={};
          layout.vector.uint64[0]=0;layout.vector.uint64[1]=kIRArgumentBufferBindPoint;
          layout.vector.uint64[2]=1;layout.vector.uint64[3]=16;
          binding.vector.uint64[0]=kIRArgumentBufferBindPoint;binding.vector.uint64[2]=(uint64_t)(__bridge void *)output;
          if(cbvCount)
          {
            layout.vector.uint64[2]=cbvCount;layout.vector.uint64[3]=8;
            if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)compute,"metal.descriptorInlineLayout",eRENDERDOC_UInt64,4,&layout))abort();
            for(unsigned i=0;i<cbvCount;i++)
            {
              binding.vector.uint64[1]=i;binding.vector.uint64[2]=(uint64_t)(__bridge void *)cbvs[i];binding.vector.uint64[3]=rootEntryOffset(i);
              if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)compute,"metal.descriptorInlineBinding",eRENDERDOC_UInt64,4,&binding))abort();
            }
          }
          else if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)compute,"metal.descriptorInlineLayout",eRENDERDOC_UInt64,4,&layout) ||
             api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)compute,"metal.descriptorInlineBinding",eRENDERDOC_UInt64,4,&binding))abort();
        }
        [compute setBytes:(uint8_t *)roots.contents+rootOffset length:cbvCount?cbvCount*8:16 atIndex:kIRArgumentBufferBindPoint];
        for(auto cbv:cbvs)[compute useResource:cbv usage:MTLResourceUsageRead];
        if(currentRead)[compute useResource:currentRead usage:MTLResourceUsageRead];
        if(texelInput)[compute useResource:texelInput usage:MTLResourceUsageRead];
        [compute useResources:(const id<MTLResource>[]){queryHeap,queryTLAS,header,contributions} count:4 usage:MTLResourceUsageRead];
        [compute useResource:dynamicOutputs?(id<MTLResource>)currentPrimary:queryResult usage:MTLResourceUsageWrite];
        if(currentSecondary)[compute useResource:currentSecondary usage:MTLResourceUsageWrite];
        }
        [compute dispatchThreadgroupsWithIndirectBuffer:queryArguments indirectBufferOffset:queryArgumentOffset threadsPerThreadgroup:MTLSizeMake(4,1,1)];
        if(!sameQueryEncoder || step==2)[compute endEncoding];
        if(dynamicOutputs && step<2)
        {
          [queryCommand commit];[queryCommand waitUntilCompleted];if(queryCommand.error)abort();
          queryCommand=getenv("RENDERDOC_IR_QUERY_UNRETAINED")?[queue commandBufferWithUnretainedReferences]:[queue commandBuffer];
        }
      }
      return queryCommand;
    };
    if(frame)
    {
      if(!heapFrameHeader) {
        command=[queue commandBuffer];encodeQuery(command,beforeRoots,beforeOutput);
        [command commit];[command waitUntilCompleted];if(command.error)return 19;
      }
      if(dynamicHeader)
      {
        uint64_t *words=(uint64_t *)((uint8_t *)header.contents+headerOffset);
        words[0]=queryTLAS.gpuResourceID._impl;words[1]=contributions.gpuAddress+contributionOffset+16;
        if(api) {
          RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=headerOffset;
          value.vector.uint64[1]=(uint64_t)(__bridge void *)queryTLAS;
          value.vector.uint64[2]=(uint64_t)(__bridge void *)contributions;
          value.vector.uint64[3]=contributionOffset+16;
          if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)header,"metal.rayASHeader",eRENDERDOC_UInt64,4,&value))return 29;
        }
        printf("PASS dynamic header write new AS/contribution offset=%lu\n",(unsigned long)(contributionOffset+16));
      }
      if(frameGeometry)
      {
        command=[queue commandBuffer];auto upload=[command blitCommandEncoder];
        [upload copyFromBuffer:vertexUpload sourceOffset:0 toBuffer:geometryVertices destinationOffset:0 size:geometryVertices.length];
        if(indexedGeometry)[upload copyFromBuffer:indexUpload sourceOffset:0 toBuffer:geometryIndices destinationOffset:0 size:geometryIndices.length];
        [upload endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 22;
        command=[queue commandBuffer];build=[command accelerationStructureCommandEncoder];
        [build buildAccelerationStructure:blas descriptor:framePrimitive scratchBuffer:geometryScratch scratchBufferOffset:256];
        [build endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 22;
        command=[queue commandBuffer];auto clear=[command blitCommandEncoder];
        [clear fillBuffer:geometryVertices range:NSMakeRange(0,geometryVertices.length) value:0];
        [clear copyFromBuffer:geometryVertices sourceOffset:0 toBuffer:vertexVerify destinationOffset:0 size:geometryVertices.length];
        if(indexedGeometry){[clear fillBuffer:geometryIndices range:NSMakeRange(0,geometryIndices.length) value:0];
          [clear copyFromBuffer:geometryIndices sourceOffset:0 toBuffer:indexVerify destinationOffset:0 size:geometryIndices.length];}
        [clear endEncoding];[command commit];[command waitUntilCompleted];
        const uint8_t zero[256]={};if(command.error || memcmp(vertexVerify.contents,zero,geometryVertices.length) ||
            (indexedGeometry && memcmp(indexVerify.contents,zero,geometryIndices.length)))return 22;
        printf("PASS Private/GPU query frame geometry cleared after BLAS: %lu vertex/%lu index bytes; geometries=%u\n",
            (unsigned long)geometryVertices.length,(unsigned long)(indexedGeometry?geometryIndices.length:0),geometryCount);
      }
      command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
      [blit copyFromBuffer:frameUpload sourceOffset:0 toBuffer:instances destinationOffset:0 size:instances.length];
      [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 19;
      command=[queue commandBuffer];build=[command accelerationStructureCommandEncoder];top.instanceCount=emptyFrameBuild?0:count;
      [build buildAccelerationStructure:queryTLAS descriptor:top scratchBuffer:scratch scratchBufferOffset:0];
      [build endEncoding];[command commit];[command waitUntilCompleted];if(command.error || !clearInput())return 19;
    }
    command=queryIndirect && getenv("RENDERDOC_IR_QUERY_UNRETAINED")?
        [queue commandBufferWithUnretainedReferences]:[queue commandBuffer];
    if(queryIndirect)command=encodeIndirectQueries(command);else encodeQuery(command,roots,output);
    [command presentDrawable:drawable];[command commit];[command waitUntilCompleted];if(command.error)return 8;
    if(argc==3 && !api->EndFrameCapture(nullptr,nullptr))return 9;
    if(argc==3 && getenv("RENDERDOC_IR_QUERY_REUSE_HEADER"))
    {
      if(!dynamicHeader || frameGeometry)return 30;
      auto declare=[&](id<MTLAccelerationStructure> structure,NSUInteger offset) {
        uint64_t *words=(uint64_t *)((uint8_t *)header.contents+headerOffset);
        words[0]=structure.gpuResourceID._impl;words[1]=contributions.gpuAddress+offset;
        RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=headerOffset;
        value.vector.uint64[1]=(uint64_t)(__bridge void *)structure;
        value.vector.uint64[2]=(uint64_t)(__bridge void *)contributions;value.vector.uint64[3]=offset;
        return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)header,"metal.rayASHeader",eRENDERDOC_UInt64,4,&value);
      };
      if(declare(tlas,contributionOffset))return 30;
      memcpy(output.contents,initial,sizeof(initial));memcpy(beforeOutput.contents,initial,sizeof(initial));
      auto nextDrawable=[layer nextDrawable];if(!nextDrawable)return 30;
      api->StartFrameCapture(nullptr,nullptr);if(!api->IsFrameCapturing())return 30;
      command=[queue commandBuffer];encodeQuery(command,beforeRoots,beforeOutput);
      [command commit];[command waitUntilCompleted];if(command.error || declare(queryTLAS,contributionOffset+16))return 30;
      command=[queue commandBuffer];auto upload=[command blitCommandEncoder];
      [upload copyFromBuffer:frameUpload sourceOffset:0 toBuffer:instances destinationOffset:0 size:instances.length];
      [upload endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 30;
      command=[queue commandBuffer];build=[command accelerationStructureCommandEncoder];
      [build buildAccelerationStructure:queryTLAS descriptor:top scratchBuffer:scratch scratchBufferOffset:0];
      [build endEncoding];[command commit];[command waitUntilCompleted];if(command.error || !clearInput())return 30;
      command=[queue commandBuffer];encodeQuery(command,roots,output);[command presentDrawable:nextDrawable];
      [command commit];[command waitUntilCompleted];if(command.error || !api->EndFrameCapture(nullptr,nullptr))return 30;
      printf("PASS reused same header background refresh and second capture\n");
    }
    id<MTLBuffer> outputReadback=output;
    if(privateOutput && !textureOutput)
    {
      outputReadback=[device newBufferWithLength:output.length options:MTLResourceStorageModeShared];
      auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
      [blit copyFromBuffer:output sourceOffset:0 toBuffer:outputReadback destinationOffset:0 size:output.length];
      [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 39;
      printf("PASS Private%s query output readback: %lu bytes\n",placementOutput?" placement":"",(unsigned long)output.length);
    }
    if(texelOutput)
    {
      auto padded=[device newBufferWithLength:output.length options:MTLResourceStorageModeShared];
      auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
      [blit copyFromBuffer:output sourceOffset:0 toBuffer:padded destinationOffset:0 size:output.length];
      [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 40;
      outputReadback=[device newBufferWithBytes:(const uint8_t *)padded.contents+texelOffset length:queryInitial.size()*4 options:MTLResourceStorageModeShared];
      for(NSUInteger i=0;i<padded.length;i++)if(i<texelOffset || i>=texelOffset+queryInitial.size()*4)
        if(((const uint8_t *)padded.contents)[i]!=0xcd)return 40;
      printf("PASS texture UAV %s %s GPU readback: %lu texels; parent padding/offset%lu verified\n",outputTextureFormat,outputStorage,(unsigned long)queryInitial.size(),(unsigned long)texelOffset);
    }
    else if(textureOutput && !dynamicOutputs)
    {
      const NSUInteger components=floatTextureOutput?4:1,rowPitch=256;
      auto padded=[device newBufferWithLength:rowPitch*resultTexture.height options:MTLResourceStorageModeShared];
      auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
      [blit copyFromTexture:resultTexture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(resultTexture.width,resultTexture.height,1) toBuffer:padded destinationOffset:0 destinationBytesPerRow:rowPitch destinationBytesPerImage:rowPitch*resultTexture.height];
      [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 40;
      outputReadback=[device newBufferWithLength:queryInitial.size()*4 options:MTLResourceStorageModeShared];
      for(NSUInteger i=0;i<queryInitial.size();i++)
      {
        const uint8_t *at=(const uint8_t *)padded.contents+(i/4)*rowPitch+(i%4)*components*4;
        uint32_t value=0;
        if(floatTextureOutput)
        {
          float rgba[4];memcpy(rgba,at,16);value=(uint32_t)rgba[0];
          if(rgba[1]!=rgba[0]+1 || rgba[2]!=rgba[0]+2 || rgba[3]!=1)return 40;
        }
        else memcpy(&value,at,4);
        ((uint32_t *)outputReadback.contents)[i]=value;
      }
      printf("PASS texture UAV %s %s GPU readback: %lu texels\n",outputTextureFormat,outputStorage,(unsigned long)queryInitial.size());
    }
    const uint32_t hit=(texelOutput?5:0)+(cbvCount && !emptyFrameBuild?11+(cbvCount==6?17:0)+(mixedRoots?72:0):0)+(emptyFrameBuild?0:frame?(frameGeometry?1250:1000)+17*(74+count-1)+13*((frameGeometry?geometryCount:initialGeometryCount)-1)+(ueGeometry && !frameGeometry?47:0):empty || masked?0:indirect?1000+17*(73+count-1):1000);
    const uint32_t expected[]={hit,hit,0,0};
    if(dynamicOutputs)
    {
      for(unsigned t=0;t<5;t++)
      {
        id<MTLTexture> texture=t<3?primaryTextures[t]:secondaryTextures[t-3];
        auto padded=[device newBufferWithLength:256*queryGroups options:MTLResourceStorageModeShared];
        auto read=[queue commandBuffer];auto blit=[read blitCommandEncoder];
        [blit copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(4,queryGroups,1) toBuffer:padded destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:256*queryGroups];
        [blit endEncoding];[read commit];[read waitUntilCompleted];if(read.error)return 42;
        for(unsigned i=0;i<queryGroups*4;i++)
        {
          uint32_t value;memcpy(&value,(uint8_t *)padded.contents+i/4*256+i%4*4,4);
          const bool written=t==0 || t==3?i<4:t==1 || t==4;
          const uint32_t want=t<3?(written?(i%4<2?hit+(t==1?312:0):0):7):
              (written?(i%4<2?0xff241840:0xff241820):0xff24180c);
          if(value!=want){fprintf(stderr,"dynamic texture %u texel %u: %u expected %u\n",t,i,value,want);return 42;}
        }
      }
      printf("PASS dynamic multi-UAV %s: five textures/%u texels each, slots A/B/C and D/E/D, GPU-produced SRV hit=%u/%u, zero-work unchanged\n",outputStorage,queryGroups*4,hit,hit+312);
      printf("PASS per-use GPU-written Private query arguments groups=1/%u/0 offset=%lu; all %zu query uints verified\n",queryGroups,(unsigned long)queryArgumentOffset,queryInitial.size());
    }
    if(queryIndirect && !dynamicOutputs)
    {
      const uint32_t *values=(const uint32_t *)outputReadback.contents;
      for(unsigned i=0;i<queryInitial.size();i++)if(values[i]!=expected[i%4])return 37;
      if(sameQueryEncoder)printf("PASS same encoder Private query arguments groups=%u/%u/%u offset=%lu; all %zu query uints verified\n",
          queryGroups,queryGroups,queryGroups,(unsigned long)queryArgumentOffset,queryInitial.size());
      else printf("PASS per-use GPU-written Private query arguments groups=1/%u/0 offset=%lu; all %zu query uints verified\n",
          queryGroups,(unsigned long)queryArgumentOffset,queryInitial.size());
    }
    if(!dynamicOutputs && memcmp(outputReadback.contents,expected,sizeof(expected))) {
      const auto actual=(const uint32_t *)outputReadback.contents;
      fprintf(stderr,"Query result mismatch %u,%u,%u,%u\n",actual[0],actual[1],actual[2],actual[3]);return 10;
    }
    if(frame && !heapFrameHeader)
    {
      const uint32_t beforeHit=empty || masked?0:1000+17*(73+count-1);
      const uint32_t before[]={beforeHit,beforeHit,0,0};
      if(memcmp(beforeOutput.contents,before,sizeof(before)))return 20;
      printf("PASS frame query before=%u/%u/0/0 after=%u/%u/0/0; rebuilt from GPU/Private input\n",beforeHit,beforeHit,hit,hit);
    }
    if(headerOffset) {
      for(NSUInteger at=0;at<header.length;at++)if((at<headerOffset || at>=headerOffset+64) && ((uint8_t *)header.contents)[at]!=0xa5)return 25;
      printf("PASS bounded Shared header offset=%lu length=%lu; padding unchanged\n",(unsigned long)headerOffset,(unsigned long)header.length);
    }
    if(heapFrameHeader)printf("PASS heap-query frame-born Header and Private TLAS build; %s\n",
        sameQueryEncoder?"three nonzero queries on one encoder":queryIndirect?"two nonzero queries and one zero-work query":"one nonzero query");
    if(newTarget)printf("PASS new TLAS target old=%llu new=%llu; only new target built in frame\n",(unsigned long long)tlas.gpuResourceID._impl,(unsigned long long)queryTLAS.gpuResourceID._impl);
    printf("PASS converted RayQuery hit/hit/miss/short=%u,%u,%u,%u; header=%lu roots=%lu; no SBT/function tables\n",
        expected[0],expected[1],expected[2],expected[3],(unsigned long)header.length,(unsigned long)roots.length);
  }
  return 0;
}
