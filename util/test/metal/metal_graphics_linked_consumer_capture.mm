// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "renderdoc/api/app/renderdoc_app.h"
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
struct Entry { uint64_t buffer,texture,length; };
struct Input { uint64_t buffer; uint32_t length,stride; };
int main(int argc,char **argv)
{
  if(argc!=4)return 1;
  @autoreleasepool
  {
    const bool pointerOrigins=getenv("RENDERDOC_METAL_GRAPHICS_POINTER_ORIGINS")!=nullptr;
    const bool gpuIndices=getenv("RENDERDOC_METAL_GRAPHICS_GPU_INDICES")!=nullptr;
    const bool early=getenv("RENDERDOC_METAL_GRAPHICS_EARLY_FRAGMENT")!=nullptr;
    const bool independent=getenv("RENDERDOC_METAL_GRAPHICS_FRAGMENT_INDEPENDENT")!=nullptr;
    const unsigned width=independent?37:16,height=independent?23:16;
    const unsigned fragmentSlot=independent?11:3;
    __attribute__((objc_precise_lifetime)) id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    auto queue=[device newCommandQueue];NSError *error=nil;
    auto main=[device newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[2]]] error:&error];
    auto linked=[device newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[3]]] error:&error];
    auto descriptor=[MTLRenderPipelineDescriptor new];descriptor.vertexFunction=[main newFunctionWithName:@"linked_vertex"];
    if(early){descriptor.fragmentFunction=[main newFunctionWithName:@"linked_fragment"];
      descriptor.colorAttachments[0].pixelFormat=independent?MTLPixelFormatRGBA16Float:MTLPixelFormatBGRA8Unorm;}
    descriptor.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float;
    descriptor.vertexLinkedFunctions=[MTLLinkedFunctions new];
    descriptor.vertexLinkedFunctions.functions=@[[linked newFunctionWithName:@"stage_input"]];
    auto pipeline=[device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if(!pipeline){fprintf(stderr,"%s\n",error.description.UTF8String);return 2;}
    auto depthStateDesc=[MTLDepthStencilDescriptor new];depthStateDesc.depthCompareFunction=MTLCompareFunctionAlways;
    depthStateDesc.depthWriteEnabled=YES;auto depthState=[device newDepthStencilStateWithDescriptor:depthStateDesc];
    const float triangle[9]={-1,-1,0,1,-1,0,0,1,0};
    auto geometry=[device newBufferWithBytes:triangle length:sizeof(triangle) options:MTLResourceStorageModeShared];
    auto triangles=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    triangles.vertexBuffer=geometry;triangles.vertexStride=12;triangles.vertexFormat=MTLAttributeFormatFloat3;triangles.triangleCount=1;
    auto blasDesc=[MTLPrimitiveAccelerationStructureDescriptor descriptor];blasDesc.geometryDescriptors=@[triangles];
    auto sizes=[device accelerationStructureSizesWithDescriptor:blasDesc];auto blas=[device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
    auto scratch=[device newBufferWithLength:sizes.buildScratchBufferSize options:MTLResourceStorageModePrivate];
    auto instances=[device newBufferWithLength:sizeof(MTLAccelerationStructureInstanceDescriptor) options:MTLResourceStorageModeShared];
    auto instance=(MTLAccelerationStructureInstanceDescriptor *)instances.contents;memset(instance,0,sizeof(*instance));
    instance->transformationMatrix.columns[0].x=1;instance->transformationMatrix.columns[1].y=1;instance->transformationMatrix.columns[2].z=1;instance->mask=255;
    auto tlasDesc=[MTLInstanceAccelerationStructureDescriptor descriptor];tlasDesc.instanceDescriptorBuffer=instances;
    tlasDesc.instanceDescriptorStride=sizeof(*instance);tlasDesc.instanceCount=1;tlasDesc.instancedAccelerationStructures=@[blas];
    auto tlasSizes=[device accelerationStructureSizesWithDescriptor:tlasDesc];auto tlas=[device newAccelerationStructureWithSize:tlasSizes.accelerationStructureSize];
    auto tlasScratch=[device newBufferWithLength:tlasSizes.buildScratchBufferSize options:MTLResourceStorageModePrivate];
    auto initial=[queue commandBuffer];auto build=[initial accelerationStructureCommandEncoder];
    [build buildAccelerationStructure:blas descriptor:blasDesc scratchBuffer:scratch scratchBufferOffset:0];
    [build endEncoding];[initial commit];[initial waitUntilCompleted];if(initial.status!=MTLCommandBufferStatusCompleted)return 3;
    initial=[queue commandBuffer];build=[initial accelerationStructureCommandEncoder];
    [build buildAccelerationStructure:tlas descriptor:tlasDesc scratchBuffer:tlasScratch scratchBufferOffset:0];[build endEncoding];
    [initial commit];[initial waitUntilCompleted];if(initial.status!=MTLCommandBufferStatusCompleted)return 3;
    auto contributions=[device newBufferWithLength:4 options:MTLResourceStorageModeShared];memset(contributions.contents,0,4);
    auto header=[device newBufferWithLength:64 options:MTLResourceStorageModeShared];memset(header.contents,0,64);
    ((uint64_t *)header.contents)[0]=tlas.gpuResourceID._impl;((uint64_t *)header.contents)[1]=contributions.gpuAddress;
    const float depths[4]={.9f,.25f,.75f,.9f},positions[12]={-1,-1,0,1,3,-1,0,1,-1,3,0,1};
    const uint32_t selectors[2]={1,2},settingsValue=1;const uint16_t indexValues[3]={0,1,2};
    auto data=[device newBufferWithBytes:depths length:sizeof(depths) options:MTLResourceStorageModeShared];data.label=@"Linked graphics depth values";
    auto vertices=[device newBufferWithBytes:positions length:sizeof(positions) options:MTLResourceStorageModeShared];vertices.label=@"Linked graphics positions";
    auto selections=[device newBufferWithBytes:selectors length:sizeof(selectors) options:MTLResourceStorageModeShared];selections.label=@"Linked graphics instance selectors";
    const uint32_t guardedSettings[2]={settingsValue,independent?UINT32_MAX:1024};
    auto settings=[device newBufferWithBytes:pointerOrigins?guardedSettings:&settingsValue length:pointerOrigins?8:4 options:MTLResourceStorageModeShared];settings.label=@"Linked graphics settings";
    const unsigned indexOffset=gpuIndices && independent?16:0;
    const bool wideIndices=gpuIndices && independent;
    unsigned char indexCreation[32]={};
    for(unsigned i=0;i<3;i++) {
      const uint32_t initial=i;memcpy(indexCreation+indexOffset+i*(wideIndices?4:2),&initial,wideIndices?4:2);
    }
    const unsigned indexLength=wideIndices?32:6;
    auto indices=[device newBufferWithBytes:indexCreation length:indexLength options:MTLResourceStorageModeShared];indices.label=@"Linked graphics indices";
    id<MTLComputePipelineState> indexPipeline=nil;
    if(gpuIndices){indexPipeline=[device newComputePipelineStateWithFunction:[main newFunctionWithName:@"native_indices"] error:&error];
      if(!indexPipeline)return 12;}
    auto heap=[device newBufferWithLength:48 options:MTLResourceStorageModeShared];heap.label=@"Linked graphics resource heap";
    ((Entry *)heap.contents)[0]={header.gpuAddress,0,0};((Entry *)heap.contents)[1]={data.gpuAddress,0,sizeof(depths)};
    auto textureDesc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:width height:height mipmapped:NO];
    textureDesc.storageMode=MTLStorageModePrivate;textureDesc.usage=MTLTextureUsageRenderTarget;
    auto depth=[device newTextureWithDescriptor:textureDesc];depth.label=@"Linked graphics depth output";
    const unsigned pitch=(width*8+255)&~255u;
    auto readback=[device newBufferWithLength:pitch*height options:MTLResourceStorageModeShared];
    id<MTLTexture> color=nil;
    if(early){auto cd=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:descriptor.colorAttachments[0].pixelFormat width:width height:height mipmapped:NO];
      cd.storageMode=MTLStorageModePrivate;cd.usage=MTLTextureUsageRenderTarget;
      color=[device newTextureWithDescriptor:cd];color.label=@"Linked graphics fragment output";}
    auto makePass=[&]() {auto p=[MTLRenderPassDescriptor renderPassDescriptor];p.depthAttachment.texture=depth;
      p.depthAttachment.loadAction=MTLLoadActionClear;p.depthAttachment.storeAction=MTLStoreActionStore;p.depthAttachment.clearDepth=1;
      if(early){p.colorAttachments[0].texture=color;p.colorAttachments[0].loadAction=MTLLoadActionClear;p.colorAttachments[0].storeAction=MTLStoreActionStore;p.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,0);}
      return p;};
    auto warm=[queue commandBuffer];auto clear=[warm renderCommandEncoderWithDescriptor:makePass()];[clear endEncoding];
    [warm commit];[warm waitUntilCompleted];if(warm.status!=MTLCommandBufferStatusCompleted)return 3;
    RENDERDOC_API_1_7_0 *api=nullptr;auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");if(get)get(eRENDERDOC_API_Version_1_7_0,(void **)&api);
    auto annotate=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
      if(!api)return 0U;RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=a;value.vector.uint64[1]=b;value.vector.uint64[2]=c;value.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&value);};
    if(api)
    {
      RENDERDOC_AnnotationValue coverage={};coverage.uint32=65;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage) ||
         annotate(heap,"metal.descriptorTable",2,0,2,24) ||
         annotate(header,"metal.rayASHeader",0,(uint64_t)(__bridge void *)tlas,(uint64_t)(__bridge void *)contributions,0))return 4;
      for(unsigned i=0;i<2;i++)
        if(annotate(heap,"metal.descriptorSlotEvent",24*i,1,0,i?0:4) ||
           annotate(heap,"metal.descriptorSlotEvent",24*i,1,2,i?0:4) ||
           annotate(heap,"metal.descriptorSlotBinding",24*i,i?0:3,(uint64_t)(__bridge void *)(i?data:header),0))return 4;
      api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);
    }
    auto command=[queue commandBuffer];
    if(gpuIndices) {
      auto compute=[command computeCommandEncoder];[compute setComputePipelineState:indexPipeline];
      [compute setBuffer:indices offset:indexOffset atIndex:independent?10:8];
      const uint32_t rotation=independent?1:0;[compute setBytes:&rotation length:4 atIndex:2];
      [compute useResource:indices usage:MTLResourceUsageWrite];
      [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(3,1,1)];[compute endEncoding];
    }
    auto render=[command renderCommandEncoderWithDescriptor:makePass()];
    [render setRenderPipelineState:pipeline];[render setDepthStencilState:depthState];[render setCullMode:MTLCullModeNone];
    [render setViewport:MTLViewport{0,0,double(width),double(height),0,1}];[render setVertexBuffer:heap offset:0 atIndex:0];
    if(early)[render setFragmentBuffer:heap offset:0 atIndex:fragmentSlot];
    const uint64_t root=settings.gpuAddress;const Input inputs[2]={{vertices.gpuAddress,sizeof(positions),16},{selections.gpuAddress,sizeof(selectors),4}};
    if(annotate(render,"metal.descriptorInlineLayout",1,2,1,8) ||
       annotate(render,"metal.descriptorInlineBinding",(1ULL<<32)|2,0,(uint64_t)(__bridge void *)settings,0) ||
       annotate(render,"metal.descriptorInlineLayout",1,6,2,16) ||
       annotate(render,"metal.descriptorInlineBinding",(1ULL<<32)|6,0,(uint64_t)(__bridge void *)vertices,0) ||
       annotate(render,"metal.descriptorInlineBinding",(1ULL<<32)|6,1,(uint64_t)(__bridge void *)selections,0)){[render endEncoding];return 4;}
    [render setVertexBytes:&root length:8 atIndex:2];[render setVertexBytes:inputs length:sizeof(inputs) atIndex:6];
    for(id<MTLResource> resource in @[vertices,selections,data,settings])[render useResource:resource usage:MTLResourceUsageRead];
    for(unsigned base=0;base<2;base++)
    {
      const uint32_t arguments[5]={3,1,indexOffset,0,base};const uint16_t kind=wideIndices?2:1;
      if(annotate(render,"metal.inlineDrawConstants",1,4,20,0) ||
         annotate(render,"metal.inlineDrawConstants",1,5,2,0)){[render endEncoding];return 4;}
      [render setVertexBytes:arguments length:20 atIndex:4];[render setVertexBytes:&kind length:2 atIndex:5];
      [render drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:3 indexType:wideIndices?MTLIndexTypeUInt32:MTLIndexTypeUInt16 indexBuffer:indices indexBufferOffset:indexOffset instanceCount:1 baseVertex:0 baseInstance:base];
    }
    [render endEncoding];[command commit];[command waitUntilCompleted];if(command.status!=MTLCommandBufferStatusCompleted)return 5;
    if(gpuIndices)for(unsigned i=0;i<3;i++) {
      uint32_t actual=0;memcpy(&actual,(char *)indices.contents+indexOffset+i*(wideIndices?4:2),wideIndices?4:2);
      if(actual!=(1+i+(independent?1:0))%3)return 13;
    }
    if(api && !api->EndFrameCapture(nullptr,nullptr))return 6;
    // Native verification readback is outside the captured rendering workload.
    // Replay reads the actual depth attachment through the public texture API.
    auto final=[queue commandBuffer];auto read=[final blitCommandEncoder];
    [read copyFromTexture:depth sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1) toBuffer:readback destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:pitch*height];
    [read endEncoding];[final commit];[final waitUntilCompleted];if(final.status!=MTLCommandBufferStatusCompleted)return 5;
    for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)if(*(float *)((char *)readback.contents+y*pitch+x*4)!=.75f)return 7;
    if(early){final=[queue commandBuffer];read=[final blitCommandEncoder];
      [read copyFromTexture:color sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1) toBuffer:readback destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:pitch*height];
      [read endEncoding];[final commit];[final waitUntilCompleted];if(final.status!=MTLCommandBufferStatusCompleted)return 5;
      for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){
        const char *pixel=(char *)readback.contents+y*pitch+x*(independent?8:4);
        const uint8_t bgra[]={191,128,uint8_t(x%2?191:64),255};
        const uint16_t half[]={uint16_t(x%2?0x3a00:0x3400),0x3800,0x3a00,0x3c00};
        if(memcmp(pixel,independent?(const void *)half:(const void *)bgra,independent?8:4))return 8;
      }
    }
    printf("PASS Native linked graphics: two indexed draws/base instances, all %u depth pixels=0.75; fragment=%d color pixels checked=%u; no RT dispatch\n",width*height,int(early),early?width*height:0);
  }
}
