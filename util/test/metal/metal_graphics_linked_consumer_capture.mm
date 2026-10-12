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
    const bool pendingConsumed=getenv("RENDERDOC_METAL_GRAPHICS_PENDING_CONSUMED")!=nullptr;
    const bool pendingSlot=pendingConsumed || getenv("RENDERDOC_METAL_GRAPHICS_PENDING_SLOT")!=nullptr;
    const bool retiredRow=getenv("RENDERDOC_METAL_GRAPHICS_RETIRED_ROW")!=nullptr;
    const bool dynamicIndex=getenv("RENDERDOC_METAL_GRAPHICS_DYNAMIC_INDEX")!=nullptr;
    const bool fragmentTexture=getenv("RENDERDOC_METAL_GRAPHICS_DYNAMIC_TEXTURE")!=nullptr;
    if(fragmentTexture && (!early || !dynamicIndex))return 14;
    const char *rebindCountText=getenv("RENDERDOC_METAL_GRAPHICS_REBIND_COUNT");
    const unsigned rebindCount=rebindCountText?unsigned(strtoul(rebindCountText,nullptr,10)):0;
    if(rebindCount>32768)return 15;
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
    const uint32_t selectors[2]={1,2},settingsValue=dynamicIndex?0x40000000U:pendingConsumed?2:1;const uint16_t indexValues[3]={0,1,2};
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
    const Entry creationRows[3]={{header.gpuAddress,0,0},{data.gpuAddress,0,sizeof(depths)},
        {data.gpuAddress,0,sizeof(depths)}};
    auto heap=retiredRow?[device newBufferWithBytes:creationRows length:sizeof(creationRows) options:MTLResourceStorageModeShared]:
        [device newBufferWithLength:pendingSlot?72:48 options:MTLResourceStorageModeShared];heap.label=@"Linked graphics resource heap";
    if(!retiredRow)memset(heap.contents,0,heap.length);
    ((Entry *)heap.contents)[0]={header.gpuAddress,0,0};((Entry *)heap.contents)[1]={data.gpuAddress,0,sizeof(depths)};
    id<MTLTexture> fragmentImage=nil,fragmentParent=nil;
    id<MTLBuffer> fragmentTable=nil;
    if(fragmentTexture)
    {
      auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:independent?MTLPixelFormatRGBA16Float:MTLPixelFormatRGBA32Float width:2 height:1 mipmapped:NO];
      td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead|(independent?MTLTextureUsagePixelFormatView:0);
      fragmentParent=[device newTextureWithDescriptor:td];
      const float fp[8]={.25f,0,0,1,.75f,0,0,1};
      const uint16_t hp[8]={0x3400,0,0,0x3c00,0x3a00,0,0,0x3c00};
      [fragmentParent replaceRegion:MTLRegionMake2D(0,0,2,1) mipmapLevel:0 withBytes:independent?(const void *)hp:(const void *)fp bytesPerRow:independent?16:32];
      fragmentImage=independent?[fragmentParent newTextureViewWithPixelFormat:td.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(0,1) slices:NSMakeRange(0,1)]:fragmentParent;
      fragmentImage.label=@"Linked graphics dynamic texture input";
      const Entry rows[2]={{0,fragmentImage.gpuResourceID._impl,0x1111111111111111ULL},
          {0,fragmentImage.gpuResourceID._impl,0x2222222222222222ULL}};
      fragmentTable=[device newBufferWithBytes:rows length:sizeof(rows) options:MTLResourceStorageModeShared];
      fragmentTable.label=@"Linked graphics dynamic texture table";
    }
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
         annotate(heap,"metal.descriptorTable",2,0,pendingSlot||retiredRow?3:2,24) ||
         annotate(header,"metal.rayASHeader",0,(uint64_t)(__bridge void *)tlas,(uint64_t)(__bridge void *)contributions,0))return 4;
      for(unsigned i=0;i<2;i++)
        if(annotate(heap,"metal.descriptorSlotEvent",24*i,1,0,i?0:4) ||
           annotate(heap,"metal.descriptorSlotEvent",24*i,1,2,i?0:4) ||
           annotate(heap,"metal.descriptorSlotBinding",24*i,i?0:3,(uint64_t)(__bridge void *)(i?data:header),0))return 4;
      if(retiredRow)
      {
        ((Entry *)heap.contents)[2]={data.gpuAddress,0,sizeof(depths)};
        if(annotate(heap,"metal.descriptorSlotEvent",48,1,0,0) ||
           annotate(heap,"metal.descriptorSlotEvent",48,1,2,0) ||
           annotate(heap,"metal.descriptorSlotBinding",48,0,(uint64_t)(__bridge void *)data,0))return 4;
      }
      if(fragmentTexture)
      {
        if(annotate(fragmentTable,"metal.descriptorTable",1,0,2,24))return 4;
        for(unsigned i=0;i<2;i++)
          if(annotate(fragmentTable,"metal.descriptorSlotEvent",24*i,1,0,4) ||
             annotate(fragmentTable,"metal.descriptorSlotEvent",24*i,1,2,4) ||
             annotate(fragmentTable,"metal.descriptorSlotBinding",24*i,1,(uint64_t)(__bridge void *)fragmentImage,0))return 4;
      }
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
    if(early)[render setFragmentBuffer:fragmentTexture?fragmentTable:heap offset:0 atIndex:fragmentSlot];
    if(fragmentTexture)
    {
      [render setFragmentBuffer:settings offset:0 atIndex:2];
      [render useResource:fragmentImage usage:MTLResourceUsageRead];
    }
    const uint64_t root=settings.gpuAddress;const Input inputs[2]={{vertices.gpuAddress,sizeof(positions),16},{selections.gpuAddress,sizeof(selectors),4}};
    if(annotate(render,"metal.descriptorInlineLayout",1,2,1,8) ||
       annotate(render,"metal.descriptorInlineBinding",(1ULL<<32)|2,0,(uint64_t)(__bridge void *)settings,0) ||
       annotate(render,"metal.descriptorInlineLayout",1,6,2,16) ||
       annotate(render,"metal.descriptorInlineBinding",(1ULL<<32)|6,0,(uint64_t)(__bridge void *)vertices,0) ||
       annotate(render,"metal.descriptorInlineBinding",(1ULL<<32)|6,1,(uint64_t)(__bridge void *)selections,0)){[render endEncoding];return 4;}
    [render setVertexBytes:&root length:8 atIndex:2];[render setVertexBytes:inputs length:sizeof(inputs) atIndex:6];
    // Legal setters overwrite one binding, without one retained allocation
    // per call or any change to the defined Native output.
    for(unsigned i=0;i<rebindCount;i++)[render setVertexBuffer:heap offset:0 atIndex:0];
    for(id<MTLResource> resource in @[vertices,selections,data,settings])[render useResource:resource usage:MTLResourceUsageRead];
    // Allocation records do not publish bytes. The shaders use entries 0/1;
    // entry 2 is allocated while this encoder is live and published only later.
    if(pendingSlot && annotate(heap,"metal.descriptorSlotEvent",48,1,0,0))return 4;
    // Contract negative: Native bytes are valid, but the address identity is
    // deliberately published after its consuming draw. A later publication
    // must not substitute for the missing per-use restoration fact.
    if(pendingConsumed)((Entry *)heap.contents)[2]={data.gpuAddress,0,sizeof(depths)};
    for(unsigned base=0;base<2;base++)
    {
      // Keep an actual preceding use: a leading retirement can legitimately
      // invalidate the initial stale identity before any shader consumes the
      // table. Here only the second draw sees the retired logical generation,
      // while the Native bytes and selected row remain untouched.
      if(retiredRow && base==1 && annotate(heap,"metal.descriptorSlotEvent",48,1,1,0))return 4;
      const uint32_t arguments[5]={3,1,indexOffset,0,base};const uint16_t kind=wideIndices?2:1;
      if(annotate(render,"metal.inlineDrawConstants",1,4,20,0) ||
         annotate(render,"metal.inlineDrawConstants",1,5,2,0)){[render endEncoding];return 4;}
      [render setVertexBytes:arguments length:20 atIndex:4];[render setVertexBytes:&kind length:2 atIndex:5];
      [render drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:3 indexType:wideIndices?MTLIndexTypeUInt32:MTLIndexTypeUInt16 indexBuffer:indices indexBufferOffset:indexOffset instanceCount:1 baseVertex:0 baseInstance:base];
    }
    [render endEncoding];
    if(pendingSlot)
    {
      ((Entry *)heap.contents)[2]={data.gpuAddress,0,sizeof(depths)};
      if(annotate(heap,"metal.descriptorSlotEvent",48,1,2,0) ||
         annotate(heap,"metal.descriptorSlotBinding",48,0,(uint64_t)(__bridge void *)data,0))return 4;
    }
    [command commit];[command waitUntilCompleted];if(command.status!=MTLCommandBufferStatusCompleted)return 5;
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
