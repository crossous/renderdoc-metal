// SPDX-License-Identifier: MIT
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "renderdoc/api/app/renderdoc_app.h"
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <string>
struct Entry { uint64_t buffer, texture, length; };
struct Roots { uint64_t constants, samplers; };
int main(int argc,char **argv)
{
  if(argc<3 || argc>6)return 1;
  const char *mode=argc>3?argv[3]:"direct";
  const bool queryDynamicHeap=getenv("RENDERDOC_METAL_RUNTIME_QUERY_DYNAMIC_HEAP")!=nullptr;
  const bool queryRetiredRow=getenv("RENDERDOC_METAL_RUNTIME_QUERY_RETIRED_ROW")!=nullptr;
  const unsigned queryNullRows=getenv("RENDERDOC_METAL_RUNTIME_QUERY_NULL_ROWS")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_NULL_ROWS")):0;
  const bool queryNullGPUPublish=getenv("RENDERDOC_METAL_RUNTIME_QUERY_NULL_GPU_PUBLISH")!=nullptr;
  const bool queryNullOpaqueWriter=getenv("RENDERDOC_METAL_RUNTIME_QUERY_NULL_OPAQUE_WRITER")!=nullptr;
  if(queryNullRows>4096 || (queryNullRows && !queryDynamicHeap) ||
     (queryNullGPUPublish && !queryNullRows) ||
     (queryNullOpaqueWriter && (!queryNullRows || queryNullGPUPublish)))return 1;
  const unsigned queryCreationInput=getenv("RENDERDOC_METAL_RUNTIME_QUERY_CREATION_INPUT")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_CREATION_INPUT")):0;
  const bool queryBirthPrefix=getenv("RENDERDOC_METAL_RUNTIME_QUERY_BIRTH_PREFIX")!=nullptr;
  const bool queryBirthPending=getenv("RENDERDOC_METAL_RUNTIME_QUERY_BIRTH_PENDING")!=nullptr;
  if((queryBirthPrefix || queryBirthPending) && queryCreationInput!=4)return 1;
  const bool queryPartialAlias=getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_ALIAS")!=nullptr;
  const unsigned queryAliasOffset=getenv("RENDERDOC_METAL_RUNTIME_QUERY_ALIAS_OFFSET")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_ALIAS_OFFSET")):256;
  const bool queryGroupIndices=getenv("RENDERDOC_METAL_RUNTIME_QUERY_GROUP_INDICES")!=nullptr;
  const unsigned queryColdRows=getenv("RENDERDOC_METAL_RUNTIME_QUERY_COLD_ROWS")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_COLD_ROWS")):0;
  const bool queryPartialHeap=getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_HEAP")!=nullptr;
  if(queryRetiredRow && (!queryDynamicHeap || !queryPartialHeap))return 1;
  const bool queryPartialGPUProducer=getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_GPU_PRODUCER")!=nullptr;
  const bool queryPartialGPUSameSubmit=getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_GPU_SAME_SUBMIT")!=nullptr;
  const unsigned queryProducerSlot=getenv("RENDERDOC_METAL_RUNTIME_QUERY_PRODUCER_SLOT")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_PRODUCER_SLOT")):2;
  if(queryProducerSlot>=31 || (queryPartialGPUSameSubmit && !queryPartialGPUProducer && !getenv("RENDERDOC_METAL_RUNTIME_QUERY_CONTRIBUTION_PRODUCER")))return 1;
  const unsigned queryPartialOffset=queryPartialHeap && getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_OFFSET")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_OFFSET")):0;
  const unsigned queryPartialSize=queryPartialHeap && getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_SIZE")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_PARTIAL_SIZE")):0;
  if(queryPartialHeap && (!queryDynamicHeap || queryPartialOffset%4 || queryPartialSize<4 || queryPartialOffset>queryPartialSize-4))return 1;
  const bool contributionProducer=getenv("RENDERDOC_METAL_RUNTIME_QUERY_CONTRIBUTION_PRODUCER")!=nullptr;
  const bool nativeRayQuery=getenv("RENDERDOC_METAL_RUNTIME_NATIVE_QUERY")!=nullptr;
  const bool readModifyWrite=getenv("RENDERDOC_METAL_RUNTIME_READ_MODIFY_WRITE")!=nullptr;
  const unsigned rmwOffset=(readModifyWrite || nativeRayQuery) && getenv("RENDERDOC_METAL_RUNTIME_RMW_OFFSET")?atoi(getenv("RENDERDOC_METAL_RUNTIME_RMW_OFFSET")):0;
  if(rmwOffset>32)return 1;
  const bool guardedIndex=getenv("RENDERDOC_METAL_RUNTIME_GUARDED_INDEX")!=nullptr;
  const bool callEffects=getenv("RENDERDOC_METAL_RUNTIME_CALL_EFFECTS")!=nullptr;
  const bool literalProjection=getenv("RENDERDOC_METAL_RUNTIME_LITERAL_PROJECTION")!=nullptr;
  const bool textureDimensions=getenv("RENDERDOC_METAL_RUNTIME_TEXTURE_DIMENSIONS")!=nullptr;
  const bool textureBufferView=getenv("RENDERDOC_METAL_RUNTIME_TEXTURE_BUFFER_VIEW")!=nullptr;
  const bool viewSharedRoot=getenv("RENDERDOC_METAL_RUNTIME_VIEW_SHARED_ROOT")!=nullptr;
  const bool partialViewInit=getenv("RENDERDOC_METAL_RUNTIME_PARTIAL_VIEW_INIT")!=nullptr;
  if(partialViewInit && !textureBufferView)return 1;
  const bool textureAtomics=getenv("RENDERDOC_METAL_RUNTIME_TEXTURE_ATOMICS")!=nullptr;
  const bool nativeSamplerHeap=getenv("RENDERDOC_METAL_RUNTIME_NATIVE_SAMPLER_HEAP")!=nullptr;
  if(nativeSamplerHeap && !textureAtomics)return 1;
  const bool gpuDescriptors=getenv("RENDERDOC_METAL_RUNTIME_GPU_DESCRIPTORS")!=nullptr;
  const bool backgroundGPUDescriptors=getenv("RENDERDOC_METAL_RUNTIME_BACKGROUND_GPU_DESCRIPTORS")!=nullptr;
  const bool republishGPUDescriptors=getenv("RENDERDOC_METAL_RUNTIME_REPUBLISH_GPU_DESCRIPTORS")!=nullptr;
  if(backgroundGPUDescriptors && !gpuDescriptors)return 1;
  if(republishGPUDescriptors && !backgroundGPUDescriptors)return 1;
  const bool scalarConditional=getenv("RENDERDOC_METAL_RUNTIME_SCALAR_CONDITIONAL")!=nullptr;
  const bool scalarOverwrite=getenv("RENDERDOC_METAL_RUNTIME_SCALAR_OVERWRITE")!=nullptr;
  const bool scalarCounter=getenv("RENDERDOC_METAL_RUNTIME_SCALAR_COUNTER")!=nullptr;
  const bool groupBuiltins=getenv("RENDERDOC_METAL_RUNTIME_GROUP_BUILTINS")!=nullptr;
  const bool loop=argc>=5 && strcmp(argv[4],"loop")==0;
  const bool bufferTypes=argc==6 && strcmp(argv[5],"buffer-types")==0;
  if((argc>=5 && !loop) || (argc==6 && !bufferTypes))return 1;
  const bool chain=strcmp(mode,"direct")!=0;
  const bool opaque=chain && strcmp(mode,"opaque")==0;
  const bool frame=chain && !opaque && strcmp(mode,"background")!=0;
  const bool separate=chain && strcmp(mode,"separate")==0;
  if(loop && opaque)return 1;
  if(chain && !frame && !opaque && strcmp(mode,"background")!=0)return 1;
  if(frame && !separate && strcmp(mode,"frame")!=0)return 1;
  @autoreleasepool
  {
    __attribute__((objc_precise_lifetime)) id<MTLDevice> device=MTLCreateSystemDefaultDevice();
    auto queue=[device newCommandQueue];NSError *error=nil;
    auto library=[device newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[2]]] error:&error];
    auto contributionPipeline=contributionProducer?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"runtime_contribution_producer"] error:&error]:nil;
    if(contributionProducer && !contributionPipeline)return 2;
    auto partialWriter=queryPartialGPUProducer?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"runtime_partial_gpu_producer"] error:&error]:nil;
    if(queryPartialGPUProducer && !partialWriter)return 2;
    auto pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"runtime_clear"] error:&error];
    auto nullWriter=queryNullOpaqueWriter?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"runtime_null_unqualified_writer"] error:&error]:nil;
    if(queryNullOpaqueWriter && !nullWriter)return 2;
    auto counterPipeline=scalarCounter?[device newComputePipelineStateWithFunction:[library newFunctionWithName:scalarConditional?@"runtime_counter_conditional":@"runtime_counter"] error:&error]:nil;
    auto textureProducer=textureAtomics?[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"runtime_texture_producer"] error:&error]:nil;
    if(textureAtomics && !textureProducer)return 2;
    if(scalarCounter && !counterPipeline)return 2;
    if(!pipeline){fprintf(stderr,"%s\n",error.description.UTF8String);return 2;}
    const auto privateOptions=MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked;
    auto counter=scalarCounter?[device newBufferWithLength:64 options:privateOptions]:nil;counter.label=@"Runtime scalar counter";
    auto outputA=[device newBufferWithLength:256 options:privateOptions];outputA.label=@"Runtime output A";
    auto outputB=[device newBufferWithLength:256 options:privateOptions];outputB.label=@"Runtime output B";
    auto cbv=[device newBufferWithLength:64 options:privateOptions];cbv.label=@"Runtime private CBV";
    id<MTLBuffer> staging=chain && !frame?[device newBufferWithLength:64 options:privateOptions]:nil;
    staging.label=@"Runtime Private copy staging";
    id<MTLBuffer> indices=loop?[device newBufferWithLength:64 options:privateOptions]:nil;
    id<MTLBuffer> source=loop?[device newBufferWithLength:64 options:privateOptions]:nil;
    indices.label=@"Runtime loop indices";source.label=@"Runtime loop source";
    auto upload=[device newBufferWithLength:256 options:MTLResourceStorageModeShared];memset(upload.contents,7,256);
    auto settings=[device newBufferWithLength:loop?24:16 options:MTLResourceStorageModeShared];
    const NSUInteger scalarRootBytes=getenv("RENDERDOC_METAL_RUNTIME_SCALAR_ROOT_BYTES")?
        strtoull(getenv("RENDERDOC_METAL_RUNTIME_SCALAR_ROOT_BYTES"),NULL,10):0;
    const NSUInteger scalarRootOffset=getenv("RENDERDOC_METAL_RUNTIME_SCALAR_ROOT_OFFSET")?
        strtoull(getenv("RENDERDOC_METAL_RUNTIME_SCALAR_ROOT_OFFSET"),NULL,10):0;
    if(scalarRootBytes && (scalarRootBytes>48ULL*1024*1024 || scalarRootOffset%4 ||
        scalarRootOffset>scalarRootBytes || 24>scalarRootBytes-scalarRootOffset))return 4;
    id<MTLBuffer> scalarRoot=scalarRootBytes?[device newBufferWithLength:scalarRootBytes options:MTLResourceStorageModeShared]:nil;
    if(scalarRoot)memset(scalarRoot.contents,0,scalarRootBytes);
    auto indexUpload=[device newBufferWithLength:16 options:MTLResourceStorageModeShared];
    auto sourceUpload=[device newBufferWithLength:64 options:MTLResourceStorageModeShared];
    memset(sourceUpload.contents,9,64);
    const uint32_t sourceValues[4]={11,22,33,44};memcpy(sourceUpload.contents,sourceValues,16);
    auto writerSettings=[device newBufferWithLength:16 options:MTLResourceStorageModeShared];
    auto readA=[device newBufferWithLength:256 options:MTLResourceStorageModeShared];
    auto readB=[device newBufferWithLength:256 options:MTLResourceStorageModeShared];
    auto readImage=textureAtomics?[device newBufferWithLength:2112 options:MTLResourceStorageModeShared]:nil;
    if(readImage){memset(readImage.contents,9,2112);readImage.label=@"Runtime texture readback";}
    auto readView=textureBufferView?[device newBufferWithLength:1024 options:MTLResourceStorageModeShared]:nil;
    auto viewUpload=textureBufferView?[device newBufferWithLength:1024 options:MTLResourceStorageModeShared]:nil;
    if(viewUpload)memset(viewUpload.contents,9,1024);
    const float vertices[12]={-1,-1,0,0,1,-1,0,0,0,1,0,0};
    auto vertex=[device newBufferWithBytes:vertices length:sizeof(vertices) options:MTLResourceStorageModeShared];
    auto geometry=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    geometry.vertexBuffer=vertex;geometry.vertexStride=16;geometry.triangleCount=1;
    auto blasDesc=[MTLPrimitiveAccelerationStructureDescriptor descriptor];blasDesc.geometryDescriptors=@[geometry];
    auto blasSizes=[device accelerationStructureSizesWithDescriptor:blasDesc];
    auto blas=[device newAccelerationStructureWithSize:blasSizes.accelerationStructureSize];
    auto blasScratch=[device newBufferWithLength:blasSizes.buildScratchBufferSize options:privateOptions];
    const unsigned queryInstances=nativeRayQuery && getenv("RENDERDOC_METAL_RUNTIME_QUERY_INSTANCES")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_INSTANCES")):1;
    const unsigned queryActiveInstance=nativeRayQuery && getenv("RENDERDOC_METAL_RUNTIME_QUERY_ACTIVE_INSTANCE")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_ACTIVE_INSTANCE")):0;
    const unsigned queryHeaderOffset=nativeRayQuery && getenv("RENDERDOC_METAL_RUNTIME_QUERY_HEADER_OFFSET")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_HEADER_OFFSET")):0;
    const unsigned queryContributionOffset=nativeRayQuery && getenv("RENDERDOC_METAL_RUNTIME_QUERY_CONTRIBUTION_OFFSET")?atoi(getenv("RENDERDOC_METAL_RUNTIME_QUERY_CONTRIBUTION_OFFSET")):0;
    if(queryActiveInstance>=queryInstances || queryInstances<1 || queryInstances>3 || queryHeaderOffset>128 || queryHeaderOffset%8 || queryContributionOffset>64 || queryContributionOffset%4)return 3;
    auto instances=[device newBufferWithLength:queryInstances*sizeof(MTLAccelerationStructureInstanceDescriptor) options:MTLResourceStorageModeShared];
    auto instance=(MTLAccelerationStructureInstanceDescriptor *)instances.contents;memset(instance,0,instances.length);
    for(unsigned i=0;i<queryInstances;i++)
    {
      instance[i].transformationMatrix.columns[0].x=1;instance[i].transformationMatrix.columns[1].y=1;
      instance[i].transformationMatrix.columns[2].z=1;instance[i].transformationMatrix.columns[3].x=i==queryActiveInstance?0:4*(i+1);instance[i].mask=255;
    }
    auto tlasDesc=[MTLInstanceAccelerationStructureDescriptor descriptor];tlasDesc.instanceDescriptorBuffer=instances;
    tlasDesc.instanceDescriptorStride=sizeof(*instance);tlasDesc.instanceCount=queryInstances;tlasDesc.instancedAccelerationStructures=@[blas];
    auto tlasSizes=[device accelerationStructureSizesWithDescriptor:tlasDesc];
    auto tlas=[device newAccelerationStructureWithSize:tlasSizes.accelerationStructureSize];tlas.label=@"Runtime query TLAS";
    auto tlasScratch=[device newBufferWithLength:tlasSizes.buildScratchBufferSize options:privateOptions];
    id<MTLBuffer> initialA=upload,initialB=upload;
    if(queryDynamicHeap)
    {
      uint32_t values[64];for(auto &v:values)v=0x09090909u;
      values[63]=1011;initialA=[device newBufferWithBytes:values length:256 options:MTLResourceStorageModeShared];
      values[63]=2022;initialB=[device newBufferWithBytes:values length:256 options:MTLResourceStorageModeShared];
    }
    auto initial=[queue commandBuffer];auto blit=[initial blitCommandEncoder];
    [blit copyFromBuffer:initialA sourceOffset:0 toBuffer:outputA destinationOffset:0 size:256];
    memset(upload.contents,9,256); // One initial submission supplies identical padding to both outputs.
    [blit copyFromBuffer:initialB sourceOffset:0 toBuffer:outputB destinationOffset:0 size:256];
    [blit copyFromBuffer:upload sourceOffset:0 toBuffer:cbv destinationOffset:0 size:64];[blit endEncoding];
    if(counter){auto seed=[initial blitCommandEncoder];[seed copyFromBuffer:upload sourceOffset:0 toBuffer:counter destinationOffset:0 size:64];[seed endEncoding];}
    if(staging)
    {
      auto stage=[initial blitCommandEncoder];
      [stage copyFromBuffer:upload sourceOffset:0 toBuffer:staging destinationOffset:0 size:64];[stage endEncoding];
    }
    if(loop)
    {
      auto inputs=[initial blitCommandEncoder];
      [inputs copyFromBuffer:upload sourceOffset:0 toBuffer:indices destinationOffset:0 size:64];
      [inputs copyFromBuffer:sourceUpload sourceOffset:0 toBuffer:source destinationOffset:0 size:64];[inputs endEncoding];
    }
    [initial commit];[initial waitUntilCompleted];
    if(initial.status!=MTLCommandBufferStatusCompleted)return 3;
    initial=[queue commandBuffer];
    auto as=[initial accelerationStructureCommandEncoder];
    [as buildAccelerationStructure:blas descriptor:blasDesc scratchBuffer:blasScratch scratchBufferOffset:0];
    [as buildAccelerationStructure:tlas descriptor:tlasDesc scratchBuffer:tlasScratch scratchBufferOffset:0];
    [as endEncoding];[initial commit];[initial waitUntilCompleted];
    if(initial.status!=MTLCommandBufferStatusCompleted)return 3;
    auto contributions=[device newBufferWithLength:queryContributionOffset+queryInstances*4 options:MTLResourceStorageModeShared];memset(contributions.contents,contributionProducer?0x6a:0,contributions.length);
    if(contributionProducer){contributions.label=@"Runtime query contributions";
      memset((char *)contributions.contents+queryContributionOffset,0xff,queryInstances*4);}
    auto header=[device newBufferWithLength:queryHeaderOffset+64 options:MTLResourceStorageModeShared];memset(header.contents,0,header.length);
    auto words=(uint64_t *)((uint8_t *)header.contents+queryHeaderOffset);words[0]=tlas.gpuResourceID._impl;words[1]=contributions.gpuAddress+queryContributionOffset;
    const unsigned baseSlots=loop?5:opaque || scalarCounter || queryPartialHeap?4:3;
    const unsigned slots=queryNullRows?3+queryNullRows:textureDimensions?5:baseSlots+queryColdRows+(queryRetiredRow?1:0);
    auto heap=[device newBufferWithLength:slots*sizeof(Entry) options:MTLResourceStorageModeShared];heap.label=@"Runtime resource heap";
    memset(heap.contents,0,heap.length);
    auto entries=(Entry *)heap.contents;entries[0]={header.gpuAddress+queryHeaderOffset,0,0};
    entries[1]={gpuDescriptors?outputB.gpuAddress:outputA.gpuAddress,0,256};entries[2]={gpuDescriptors?outputA.gpuAddress:outputB.gpuAddress,0,256};
    auto payload=gpuDescriptors?[device newBufferWithLength:48 options:MTLResourceStorageModeShared]:nil;payload.label=@"Runtime descriptor payload";
    if(payload){auto p=(Entry *)payload.contents;p[0]={outputA.gpuAddress,0,256};p[1]={outputB.gpuAddress,0,256};}
    if(scalarCounter)entries[3]={counter.gpuAddress,0,64};
    if(opaque)entries[3]={staging.gpuAddress,0,64};
    if(loop){entries[3]={indices.gpuAddress+16,0,16};entries[4]={source.gpuAddress,0,16};}
    if(textureDimensions)entries[4]={0,0,0};
    if(queryPartialHeap)entries[3]={0,0,0};
    id<MTLBuffer> retiredSource=queryRetiredRow?[device newBufferWithLength:256 options:MTLResourceStorageModeShared]:nil;
    const unsigned retiredRow=baseSlots+queryColdRows;
    if(retiredSource){retiredSource.label=@"Runtime retired namespace source";entries[retiredRow]={retiredSource.gpuAddress,0,256};}
    id<MTLTexture> nullTexture=nil;
    id<MTLBuffer> nullPayload=nil;
    if(queryNullRows)
    {
      const unsigned width=queryNullRows<16?2:7,height=queryNullRows<16?3:5;
      auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:queryNullRows<16?MTLPixelFormatR32Uint:MTLPixelFormatRGBA8Unorm width:width height:height mipmapped:NO];
      td.storageMode=MTLStorageModePrivate;td.usage=MTLTextureUsageShaderRead;
      nullTexture=[device newTextureWithDescriptor:td];
      auto pixels=[device newBufferWithLength:256*height options:MTLResourceStorageModeShared];memset(pixels.contents,3,pixels.length);
      auto seed=[queue commandBuffer];auto fill=[seed blitCommandEncoder];
      [fill copyFromBuffer:pixels sourceOffset:0 sourceBytesPerRow:256 sourceBytesPerImage:256*height
           sourceSize:MTLSizeMake(width,height,1) toTexture:nullTexture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
      [fill endEncoding];[seed commit];[seed waitUntilCompleted];if(seed.status!=MTLCommandBufferStatusCompleted)return 5;
      entries[3]={0,nullTexture.gpuResourceID._impl,0};
      nullPayload=[device newBufferWithLength:24 options:MTLResourceStorageModeShared];memset(nullPayload.contents,0,24);
    }
    const uint32_t partialWord=3033;
    auto partialUpload=queryPartialHeap?[device newBufferWithBytes:&partialWord length:4 options:MTLResourceStorageModeShared]:nil;
    auto samplerDesc=[MTLSamplerDescriptor new];samplerDesc.supportArgumentBuffers=YES;
    auto sampler=[device newSamplerStateWithDescriptor:samplerDesc];
    const unsigned samplerSlots=nativeSamplerHeap?2:1;
    auto samplers=[device newBufferWithLength:samplerSlots*24 options:MTLResourceStorageModeShared];memset(samplers.contents,0,samplerSlots*24);
    ((uint64_t *)samplers.contents)[0]=sampler.gpuResourceID._impl;
    if(nativeSamplerHeap)((uint64_t *)samplers.contents)[3]=sampler.gpuResourceID._impl;
    RENDERDOC_API_1_7_0 *api=nullptr;auto get=(pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT,"RENDERDOC_GetAPI");
    if(get)get(eRENDERDOC_API_Version_1_7_0,(void **)&api);
    auto annotate=[&](id object,const char *key,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
      if(!api)return 0U;RENDERDOC_AnnotationValue value={};value.vector.uint64[0]=a;value.vector.uint64[1]=b;
      value.vector.uint64[2]=c;value.vector.uint64[3]=d;
      return api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)object,key,eRENDERDOC_UInt64,4,&value);
    };
    if(api)
    {
      RENDERDOC_AnnotationValue coverage={};coverage.uint32=65;
      if(!getenv("RENDERDOC_METAL_RUNTIME_NO_COVERAGE") && api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)device,"metal.descriptorCoverage",eRENDERDOC_UInt32,0,&coverage))return 4;
      RENDERDOC_AnnotationValue abi={};abi.string="{\"Origin\":\"MetalIRRuntimeBindings\",\"ShaderType\":\"Compute\",\"RootBindPoint\":2,\"ResourceHeapBindPoint\":0,\"SamplerHeapBindPoint\":1,\"StaticSamplerCount\":1,\"TopLevelArgumentBuffer\":[{\"EltOffset\":0,\"Size\":8,\"Slot\":0,\"Space\":0,\"Type\":\"CBV\"},{\"EltOffset\":8,\"Size\":8,\"Type\":\"Table\"}],\"state\":{\"tg_size\":[1,1,1]}}";
      const std::string groupABI=groupBuiltins || guardedIndex?std::string(abi.string):std::string();
      std::string actualABI=groupABI;
      if(groupBuiltins || guardedIndex){const auto at=actualABI.find("[1,1,1]");if(at==std::string::npos)return 4;actualABI.replace(at,7,guardedIndex?"[64,1,1]":"[2,1,1]");abi.string=actualABI.c_str();}
      if(scalarCounter && api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)counterPipeline,"metal.irComputeReflection",eRENDERDOC_String,0,&abi))return 4;
      if(textureProducer && api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)textureProducer,"metal.irComputeReflection",eRENDERDOC_String,0,&abi))return 4;
      if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)pipeline,"metal.irComputeReflection",eRENDERDOC_String,0,&abi) ||
          annotate(heap,"metal.descriptorTable",2,0,slots,24) || annotate(samplers,"metal.descriptorTable",2,0,samplerSlots,24) ||
          annotate(header,"metal.rayASHeader",queryHeaderOffset,(uint64_t)(__bridge void *)tlas,(uint64_t)(__bridge void *)contributions,queryContributionOffset))return 4;
      if(gpuDescriptors)
      {
        RENDERDOC_AnnotationValue writes={};writes.uint32=1;
        if(api->SetObjectAnnotation((__bridge void *)device,(__bridge void *)heap,"metal.descriptorGPUWrites",eRENDERDOC_UInt32,0,&writes) ||
           annotate(payload,"metal.descriptorTable",2,0,2,24))return 4;
        for(unsigned i=0;i<2;i++)if(annotate(payload,"metal.descriptorSlotEvent",i*24,1,0,5) ||
           annotate(payload,"metal.descriptorSlotEvent",i*24,1,2,5) ||
           annotate(payload,"metal.descriptorSlotBinding",i*24,0,(uint64_t)(__bridge void *)(i?outputB:outputA),0))return 4;
      }
      for(unsigned i=0;i<baseSlots;i++)
      {
        if(queryPartialHeap && i==3)continue;
        const unsigned legacyType=i==1 || i==2 || ((opaque || scalarCounter) && i==3)?5:4;
        const unsigned bufferType=i==1?1:i==2?3:i==3?0:i==4?2:4;
        const unsigned type=bufferTypes?bufferType:legacyType;
        id buffer=i==0?header:i==1?(gpuDescriptors?outputB:outputA):i==2?(gpuDescriptors?outputA:outputB):scalarCounter?counter:opaque?staging:i==3?indices:source;
        if(annotate(heap,"metal.descriptorSlotEvent",i*24,1,0,type) ||
           annotate(heap,"metal.descriptorSlotEvent",i*24,1,2,type) ||
           annotate(heap,"metal.descriptorSlotBinding",i*24,i?0:3,(uint64_t)(__bridge void *)buffer,i==0?queryHeaderOffset:loop && i==3?16:0))return 4;
      }
      if(queryRetiredRow && (annotate(heap,"metal.descriptorSlotEvent",retiredRow*24,1,0,4) ||
         annotate(heap,"metal.descriptorSlotEvent",retiredRow*24,1,2,4) ||
         annotate(heap,"metal.descriptorSlotBinding",retiredRow*24,0,(uint64_t)(__bridge void *)retiredSource,0) ||
         annotate(heap,"metal.descriptorSlotEvent",retiredRow*24,1,1,4)))return 4;
      if(annotate(samplers,"metal.descriptorSlotEvent",0,1,0,7) || annotate(samplers,"metal.descriptorSlotEvent",0,1,2,7) ||
         annotate(samplers,"metal.descriptorSlotBinding",0,2,(uint64_t)(__bridge void *)sampler,0))return 4;
      if(queryNullRows && (annotate(heap,"metal.descriptorSlotEvent",72,1,0,4) ||
         annotate(heap,"metal.descriptorSlotEvent",72,1,2,4) ||
         annotate(heap,"metal.descriptorSlotBinding",72,1,(uint64_t)(__bridge void *)nullTexture,0)))return 4;
      if(queryNullGPUPublish && (annotate(nullPayload,"metal.descriptorTable",2,0,1,24) ||
         annotate(nullPayload,"metal.descriptorSlotEvent",0,1,0,4) ||
         annotate(nullPayload,"metal.descriptorSlotEvent",0,1,2,4)))return 4;
      if(nativeSamplerHeap && (annotate(samplers,"metal.descriptorSlotEvent",24,1,0,7) ||
         annotate(samplers,"metal.descriptorSlotEvent",24,1,2,7) ||
         annotate(samplers,"metal.descriptorSlotBinding",24,2,(uint64_t)(__bridge void *)sampler,0)))return 4;
    }
    if(backgroundGPUDescriptors)
    {
      // Establish actual GPU-owned descriptor bytes before the capture boundary.
      // The initial CPU entries deliberately point to the opposite outputs.
      auto publish=[queue commandBuffer];auto copy=[publish blitCommandEncoder];
      for(unsigned i=0;i<2;i++)
        [copy copyFromBuffer:payload sourceOffset:(i^(republishGPUDescriptors?1U:0U))*24
                   toBuffer:heap destinationOffset:(i+1)*24 size:24];
      [copy endEncoding];[publish commit];[publish waitUntilCompleted];
      if(publish.status!=MTLCommandBufferStatusCompleted ||
         entries[1].buffer!=(republishGPUDescriptors?outputB:outputA).gpuAddress ||
         entries[2].buffer!=(republishGPUDescriptors?outputA:outputB).gpuAddress)return 5;
      for(unsigned i=0;i<2;i++)
      {
        id<MTLBuffer> target=(i^(republishGPUDescriptors?1U:0U))?outputB:outputA;
        if(annotate(heap,"metal.descriptorSlotGPUValue",(i+1)*24,target.gpuAddress,0,256) ||
           annotate(heap,"metal.descriptorSlotBinding",(i+1)*24,0,(uint64_t)(__bridge void *)target,0))return 4;
      }
      printf("PASS background GPU descriptors: two actual copied identities\n");
    }
    id<MTLHeap> partialHeap=nil;
    id<MTLBuffer> birthPrefix=nil;
    if(queryPartialHeap && (!queryCreationInput || queryCreationInput==4))
    {
      auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;
      hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
      const auto allocation=[device heapBufferSizeAndAlignWithLength:queryPartialSize+queryAliasOffset options:privateOptions];
      if(queryAliasOffset%allocation.align)return 1;
      hd.size=allocation.size*2+allocation.align;partialHeap=[device newHeapWithDescriptor:hd];if(!partialHeap)return 3;
      if(queryBirthPrefix || queryBirthPending)
      {
        birthPrefix=[partialHeap newBufferWithLength:256 options:privateOptions offset:allocation.size];
        if(!birthPrefix)return 3;
        birthPrefix.label=@"Runtime disjoint birth prefix";
      }
    }
    if(queryCreationInput==4)
    {
      // Original heap memory is filled before capture, then its old logical
      // owner retires. A new frame buffer must recover that real backing input.
      auto carrier=[partialHeap newBufferWithLength:queryPartialSize+queryAliasOffset options:privateOptions offset:0];
      NSMutableData *data=[NSMutableData dataWithLength:queryPartialSize+queryAliasOffset];memset(data.mutableBytes,9,data.length);
      memcpy((char *)data.mutableBytes+queryAliasOffset+queryPartialOffset,&partialWord,4);
      auto source=[device newBufferWithBytes:data.bytes length:data.length options:MTLResourceStorageModeShared];
      if(!carrier || !source)return 3;
      auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
      [blit copyFromBuffer:source sourceOffset:0 toBuffer:carrier destinationOffset:0 size:data.length];[blit endEncoding];
      [command commit];[command waitUntilCompleted];if(command.status!=MTLCommandBufferStatusCompleted)return 5;
      [carrier makeAliasable];
    }
    // This invalid, released descriptor is never accessed by the Native query.
    // Keep its old raw field to distinguish initial invalidation from zero input.
    retiredSource=nil;
    if(queryRetiredRow)printf("Native namespace retired row=%u oldAddress=%llu before capture\n",retiredRow,(unsigned long long)entries[retiredRow].buffer);
    id<MTLHeap> coldHeap=nil;
    if(queryColdRows)
    {
      auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;
      hd.storageMode=MTLStorageModePrivate;hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
      const auto layout=[device heapBufferSizeAndAlignWithLength:queryPartialSize options:privateOptions];
      hd.size=layout.size*queryColdRows;coldHeap=[device newHeapWithDescriptor:hd];if(!coldHeap)return 3;
    }
    if(api){api->SetCaptureFilePathTemplate(argv[1]);api->StartFrameCapture(nullptr,nullptr);}
    if(queryBirthPrefix || queryBirthPending)
    {
      // A completed operation on a disjoint allocation in this same physical
      // heap cannot disqualify a later new buffer's original birth input.
      auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
      // Finite repeated copies keep the GPU prefix in flight without a CPU
      // readback or wait. The final bytes and the actual query stay unchanged.
      for(unsigned i=0;i<(queryBirthPending?512U:1U);i++)
        [blit copyFromBuffer:partialUpload sourceOffset:0 toBuffer:birthPrefix destinationOffset:0 size:4];
      [blit endEncoding];[command commit];
      if(!queryBirthPending)
      {
        [command waitUntilCompleted];
        if(command.status!=MTLCommandBufferStatusCompleted)return 5;
        puts("PASS Native completed disjoint same-heap prefix before new logical birth");
      }
      else printf("Native pending GPU prefix status=%lu before new logical birth\n",(unsigned long)command.status);
    }
    if(queryNullRows && !queryNullGPUPublish && annotate(heap,"metal.descriptorSlotEvent",72,1,1,4))return 4;
    NSMutableArray *coldBuffers=[NSMutableArray array];
    for(unsigned row=4;row<4+queryColdRows;row++)
    {
      // Published and relocated, but never reachable by this dispatch's
      // Native group coordinate. No contents or producer is invented.
      const auto layout=[device heapBufferSizeAndAlignWithLength:queryPartialSize options:privateOptions];
      auto cold=[coldHeap newBufferWithLength:queryPartialSize options:privateOptions offset:layout.size*(row-4)];
      if(!cold)return 3;
      [coldBuffers addObject:cold];entries[row]={cold.gpuAddress,0,queryPartialSize};
      if(annotate(heap,"metal.descriptorSlotEvent",row*24,1,0,4) ||
         annotate(heap,"metal.descriptorSlotEvent",row*24,1,2,4) ||
         annotate(heap,"metal.descriptorSlotBinding",row*24,0,(uint64_t)(__bridge void *)cold,0))return 4;
    }
    id<MTLCommandBuffer> pendingPartialCommand=nil;
    id<MTLBuffer> partialInput=nil;
    if(queryPartialHeap)
    {
      // Heap contents require a real producer. Device allocation independently
      // supplies its documented zero bytes or the original newBufferWithBytes data.
      if(queryPartialAlias)
      {
        auto former=[partialHeap newBufferWithLength:queryPartialSize+queryAliasOffset options:privateOptions offset:0];
        if(!former)return 3;
        auto write=[queue commandBuffer];
        if(queryPartialGPUProducer)
        {
          auto writer=[write computeCommandEncoder];[writer setComputePipelineState:partialWriter];
          if(annotate(writer,"metal.descriptorInlineLayout",0,queryProducerSlot,2,8) ||
             annotate(writer,"metal.descriptorInlineBinding",queryProducerSlot,0,(uint64_t)(__bridge void *)former,0) ||
             annotate(writer,"metal.descriptorInlineBinding",queryProducerSlot,1,(uint64_t)(__bridge void *)samplers,0))return 4;
          Roots producerRoots={former.gpuAddress,samplers.gpuAddress};
          [writer setBytes:&producerRoots length:sizeof(producerRoots) atIndex:queryProducerSlot];
          [writer useResource:former usage:MTLResourceUsageWrite];
          [writer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[writer endEncoding];
        }
        else
        {
          auto fill=[write blitCommandEncoder];
          [fill copyFromBuffer:partialUpload sourceOffset:0 toBuffer:former destinationOffset:queryAliasOffset+queryPartialOffset size:4];[fill endEncoding];
        }
        [write commit];[write waitUntilCompleted];if(write.status!=MTLCommandBufferStatusCompleted)return 5;
        [former makeAliasable];
        partialInput=[partialHeap newBufferWithLength:queryPartialSize options:privateOptions offset:queryAliasOffset];
      }
      else if(queryCreationInput==4)
        partialInput=[partialHeap newBufferWithLength:queryPartialSize options:privateOptions offset:queryAliasOffset];
      else if(queryCreationInput==3)
      {
        NSMutableData *input=[NSMutableData dataWithLength:queryPartialSize];memset(input.mutableBytes,9,queryPartialSize);
        memcpy((char *)input.mutableBytes+queryPartialOffset,&partialWord,4);
        partialInput=[device newBufferWithBytes:input.bytes length:queryPartialSize options:MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked];
      }
      else if(queryCreationInput)
        partialInput=[device newBufferWithLength:queryPartialSize options:queryCreationInput==1?privateOptions:MTLResourceStorageModeShared|MTLResourceHazardTrackingModeTracked];
      else partialInput=[partialHeap newBufferWithLength:queryPartialSize options:privateOptions offset:0];
      if(!partialInput)return 3;
      entries[3]={partialInput.gpuAddress,0,queryPartialSize};
      if(annotate(heap,"metal.descriptorSlotEvent",72,1,0,4) ||
         annotate(heap,"metal.descriptorSlotEvent",72,1,2,4) ||
         annotate(heap,"metal.descriptorSlotBinding",72,0,(uint64_t)(__bridge void *)partialInput,0))return 4;
      if(!queryPartialAlias && !queryCreationInput)
      {
      auto initialize=[queue commandBuffer];
      if(queryPartialGPUProducer)
      {
        auto writer=[initialize computeCommandEncoder];[writer setComputePipelineState:partialWriter];
        if(annotate(writer,"metal.descriptorInlineLayout",0,queryProducerSlot,2,8) ||
           annotate(writer,"metal.descriptorInlineBinding",queryProducerSlot,0,(uint64_t)(__bridge void *)partialInput,0) ||
           annotate(writer,"metal.descriptorInlineBinding",queryProducerSlot,1,(uint64_t)(__bridge void *)samplers,0))return 4;
        Roots producerRoots={partialInput.gpuAddress,samplers.gpuAddress};
        [writer setBytes:&producerRoots length:sizeof(producerRoots) atIndex:queryProducerSlot];
        [writer useResource:partialInput usage:MTLResourceUsageWrite];
        [writer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[writer endEncoding];
      }
      else
      {
        auto fill=[initialize blitCommandEncoder];
        [fill copyFromBuffer:partialUpload sourceOffset:0 toBuffer:partialInput destinationOffset:queryPartialOffset size:4];[fill endEncoding];
      }
      if(queryPartialGPUSameSubmit)pendingPartialCommand=initialize;
      else
      {
        [initialize commit];[initialize waitUntilCompleted];
        if(initialize.status!=MTLCommandBufferStatusCompleted)return 5;
      }
      }
    }
    id<MTLTexture> dimensionTexture=nil;
    id<MTLHeap> dimensionHeap=nil;
    id<MTLBuffer> dimensionBacking=nil;
    if(textureDimensions)
    {
      auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Uint width:8 height:8 mipmapped:NO];
      td.storageMode=MTLStorageModePrivate;td.hazardTrackingMode=MTLHazardTrackingModeTracked;
      td.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;
      if(textureAtomics)td.usage|=MTLTextureUsageShaderAtomic;
      if(textureBufferView)
      {
        const auto allocation=[device heapBufferSizeAndAlignWithLength:1024 options:privateOptions];
        auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;
        hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=allocation.align+allocation.size;
        dimensionHeap=[device newHeapWithDescriptor:hd];
        dimensionBacking=[dimensionHeap newBufferWithLength:1024 options:privateOptions offset:allocation.align];
        dimensionBacking.label=@"Runtime frame view backing";
        td.textureType=MTLTextureTypeTextureBuffer;td.width=64;td.height=1;
        td.resourceOptions=MTLResourceStorageModePrivate;td.storageMode=MTLStorageModePrivate;
        td.hazardTrackingMode=MTLHazardTrackingModeDefault;td.allowGPUOptimizedContents=NO;
        dimensionTexture=[dimensionBacking newTextureWithDescriptor:td offset:256 bytesPerRow:256];
      }
      else
      {
      const auto allocation=[device heapTextureSizeAndAlignWithDescriptor:td];
      auto hd=[MTLHeapDescriptor new];hd.type=MTLHeapTypePlacement;hd.storageMode=MTLStorageModePrivate;
      hd.hazardTrackingMode=MTLHazardTrackingModeTracked;hd.size=allocation.align+allocation.size;
      dimensionHeap=[device newHeapWithDescriptor:hd];
      dimensionTexture=[dimensionHeap newTextureWithDescriptor:td offset:allocation.align];
      }
      if(!dimensionTexture)return 8;
      dimensionTexture.label=@"Runtime frame texture dimensions";
      entries[4]={dimensionBacking?dimensionBacking.gpuAddress+256:0,dimensionTexture.gpuResourceID._impl,dimensionBacking?256U:0U};
      const unsigned tag=textureBufferView?3:textureAtomics?5:4;
      if(annotate(heap,"metal.descriptorSlotEvent",96,1,0,tag) ||
         annotate(heap,"metal.descriptorSlotEvent",96,1,2,tag) ||
         annotate(heap,"metal.descriptorSlotBinding",96,1,(uint64_t)(__bridge void *)dimensionTexture,0))return 4;
      if(dimensionBacking)
      {
        if(annotate(heap,"metal.descriptorSlotBinding",96,0,(uint64_t)(__bridge void *)dimensionBacking,256))return 4;
        auto initialize=[queue commandBuffer];auto fill=[initialize blitCommandEncoder];
        if(partialViewInit)
        {
          [fill copyFromBuffer:viewUpload sourceOffset:0 toBuffer:dimensionBacking destinationOffset:0 size:256];
          [fill copyFromBuffer:viewUpload sourceOffset:256 toBuffer:dimensionBacking destinationOffset:256 size:16];
          [fill copyFromBuffer:viewUpload sourceOffset:512 toBuffer:dimensionBacking destinationOffset:512 size:512];
        }
        else [fill copyFromBuffer:viewUpload sourceOffset:0 toBuffer:dimensionBacking destinationOffset:0 size:1024];
        [fill endEncoding];[initialize commit];[initialize waitUntilCompleted];
        if(initialize.status!=MTLCommandBufferStatusCompleted)return 5;
      }
    }
    if(chain && frame)
    {staging=[device newBufferWithLength:64 options:privateOptions];staging.label=@"Runtime Private copy staging";}
    for(unsigned i=0;i<2;i++)
    {
      const uint32_t values[6]={i?2U:1U,(i?7U:3U)+rmwOffset,callEffects?(i?568U:901U):(i?456U:123U),loop?4U:groupBuiltins?16U:guardedIndex?1U:0U,3,4};
      const unsigned settingsBytes=loop?24:16;
      memcpy(settings.contents,values,settingsBytes);
      if(scalarRoot)memcpy((unsigned char *)scalarRoot.contents+scalarRootOffset,values,settingsBytes);
      const uint32_t writerValues[4]={3,8,values[0],0};memcpy(writerSettings.contents,writerValues,sizeof(writerValues));
      auto command=!i && pendingPartialCommand?pendingPartialCommand:[queue commandBuffer];
      if(contributionProducer && !i)
      {
        auto producer=[command computeCommandEncoder];[producer setComputePipelineState:contributionPipeline];
        [producer setBuffer:contributions offset:queryContributionOffset atIndex:queryProducerSlot];
        const uint32_t count=queryInstances;[producer setBytes:&count length:4 atIndex:queryProducerSlot==2?9:3];
        [producer useResource:contributions usage:MTLResourceUsageWrite];
        [producer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(queryInstances,1,1)];
        [producer endEncoding];
        if(!queryPartialGPUSameSubmit){[command commit];[command waitUntilCompleted];
          if(command.status!=MTLCommandBufferStatusCompleted)return 5;command=[queue commandBuffer];}
      }
      if(queryNullGPUPublish && !i)
      {
        auto publish=[command blitCommandEncoder];
        [publish copyFromBuffer:nullPayload sourceOffset:0 toBuffer:heap destinationOffset:72 size:24];[publish endEncoding];
        if(annotate(heap,"metal.descriptorSlotGPUValue",72,0,0,0) ||
           annotate(heap,"metal.descriptorSlotEvent",72,1,1,4))return 4;
      }
      if(gpuDescriptors && (!backgroundGPUDescriptors || republishGPUDescriptors))
      {
        auto publish=[command blitCommandEncoder];
        // The first replacement copies the restored GPU initial slot itself;
        // the second comes from the independently typed CPU payload table.
        id<MTLBuffer> source=republishGPUDescriptors && !i?heap:payload;
        [publish copyFromBuffer:source sourceOffset:republishGPUDescriptors && !i?48:i*24
                     toBuffer:heap destinationOffset:(i+1)*24 size:24];[publish endEncoding];
        if(republishGPUDescriptors && !i)printf("PASS frame copy from initial GPU descriptor\n");
        id<MTLBuffer> target=i?outputB:outputA;
        if(annotate(heap,"metal.descriptorSlotGPUValue",(i+1)*24,target.gpuAddress,0,256) ||
           annotate(heap,"metal.descriptorSlotBinding",(i+1)*24,0,(uint64_t)(__bridge void *)target,0))return 4;
      }
      auto copy=[command blitCommandEncoder];
      if(loop)
      {
        const uint32_t positionsA[4]={3,12,27,55},positionsB[4]={7,19,35,63};
        memcpy(indexUpload.contents,i?positionsB:positionsA,16);
        [copy copyFromBuffer:indexUpload sourceOffset:0 toBuffer:indices destinationOffset:16 size:16];
      }
      if(chain)
      {
        [copy copyFromBuffer:settings sourceOffset:0 toBuffer:staging destinationOffset:32 size:settingsBytes];
        if(opaque)
        {
          [copy endEncoding];
          auto writer=[command computeCommandEncoder];[writer setComputePipelineState:pipeline];
          [writer setBuffer:heap offset:0 atIndex:0];
          if(annotate(writer,"metal.descriptorInlineLayout",0,2,2,8) ||
             annotate(writer,"metal.descriptorInlineBinding",2,0,(uint64_t)(__bridge void *)writerSettings,0) ||
             annotate(writer,"metal.descriptorInlineBinding",2,1,(uint64_t)(__bridge void *)samplers,0))return 4;
          Roots writerRoots={writerSettings.gpuAddress,samplers.gpuAddress};
          [writer setBytes:&writerRoots length:sizeof(writerRoots) atIndex:2];
          [writer useResource:writerSettings usage:MTLResourceUsageRead];[writer useResource:staging usage:MTLResourceUsageWrite];
          [writer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[writer endEncoding];
          copy=[command blitCommandEncoder];
        }
        if(separate)
        {
          [copy endEncoding];[command commit];[command waitUntilCompleted];
          if(command.status!=MTLCommandBufferStatusCompleted)return 5;
          command=[queue commandBuffer];copy=[command blitCommandEncoder];
        }
        [copy copyFromBuffer:staging sourceOffset:32 toBuffer:cbv destinationOffset:16 size:8];
        [copy copyFromBuffer:staging sourceOffset:40 toBuffer:cbv destinationOffset:24 size:8];
        if(loop)[copy copyFromBuffer:staging sourceOffset:48 toBuffer:cbv destinationOffset:32 size:8];
      }
      else [copy copyFromBuffer:settings sourceOffset:0 toBuffer:cbv destinationOffset:16 size:settingsBytes];
      if(viewSharedRoot)[copy copyFromBuffer:settings sourceOffset:0 toBuffer:dimensionBacking destinationOffset:16 size:settingsBytes];
      [copy endEncoding];
      if(scalarCounter)
      {
        auto producer=[command computeCommandEncoder];[producer setComputePipelineState:counterPipeline];[producer setBuffer:heap offset:0 atIndex:0];
        if(annotate(producer,"metal.descriptorInlineLayout",0,2,2,8) ||
           annotate(producer,"metal.descriptorInlineBinding",2,0,(uint64_t)(__bridge void *)cbv,16) ||
           annotate(producer,"metal.descriptorInlineBinding",2,1,(uint64_t)(__bridge void *)samplers,0))return 4;
        Roots producerRoots={cbv.gpuAddress+16,samplers.gpuAddress};[producer setBytes:&producerRoots length:sizeof(producerRoots) atIndex:2];
        [producer useResource:counter usage:MTLResourceUsageWrite];[producer useResource:cbv usage:MTLResourceUsageRead];
        [producer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:guardedIndex?MTLSizeMake(64,1,1):MTLSizeMake(1,1,1)];[producer endEncoding];
      }
      if(scalarOverwrite)
      {
        auto overwrite=[command blitCommandEncoder];
        [overwrite copyFromBuffer:settings sourceOffset:12 toBuffer:counter destinationOffset:0 size:4];[overwrite endEncoding];
      }
      if(textureProducer)
      {
        auto initialize=[command computeCommandEncoder];
        [initialize setComputePipelineState:textureProducer];[initialize setBuffer:heap offset:0 atIndex:0];
        if(annotate(initialize,"metal.descriptorInlineLayout",0,2,2,8) ||
           annotate(initialize,"metal.descriptorInlineBinding",2,0,(uint64_t)(__bridge void *)cbv,16) ||
           annotate(initialize,"metal.descriptorInlineBinding",2,1,(uint64_t)(__bridge void *)samplers,0))return 4;
        Roots producerRoots={cbv.gpuAddress+16,samplers.gpuAddress};
        [initialize setBytes:&producerRoots length:sizeof(producerRoots) atIndex:2];
        [initialize useResource:dimensionTexture usage:MTLResourceUsageWrite];
        [initialize useResource:cbv usage:MTLResourceUsageRead];
        [initialize dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
        [initialize endEncoding];
        if(getenv("RENDERDOC_METAL_RUNTIME_TEXTURE_PRODUCER_SUBMIT"))
        {
          [command commit];[command waitUntilCompleted];
          if(command.status!=MTLCommandBufferStatusCompleted)return 5;
          command=[queue commandBuffer];
        }
      }
      auto compute=[command computeCommandEncoder];[compute setComputePipelineState:pipeline];[compute setBuffer:heap offset:0 atIndex:0];
      if(queryNullOpaqueWriter && !i)
      {
        [compute endEncoding];
        auto writer=[command computeCommandEncoder];[writer setComputePipelineState:nullWriter];[writer setBuffer:heap offset:0 atIndex:0];
        // Supply the same genuine typed inline resource declarations as an
        // ordinary Native consumer. No typed descriptor producer is declared.
        if(annotate(writer,"metal.descriptorInlineLayout",0,2,2,8) ||
           annotate(writer,"metal.descriptorInlineBinding",2,0,(uint64_t)(__bridge void *)cbv,16) ||
           annotate(writer,"metal.descriptorInlineBinding",2,1,(uint64_t)(__bridge void *)samplers,0))return 4;
        Roots writerRoots={cbv.gpuAddress+16,samplers.gpuAddress};
        [writer setBytes:&writerRoots length:sizeof(writerRoots) atIndex:2];
        [writer dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];[writer endEncoding];
        compute=[command computeCommandEncoder];[compute setComputePipelineState:pipeline];[compute setBuffer:heap offset:0 atIndex:0];
      }
      if(nativeSamplerHeap)[compute setBuffer:samplers offset:0 atIndex:1];
      id<MTLBuffer> mainRoot=scalarRoot?scalarRoot:viewSharedRoot?dimensionBacking:cbv;
      const uint64_t mainRootOffset=scalarRoot?scalarRootOffset:16;
      if(annotate(compute,"metal.descriptorInlineLayout",0,2,2,8) ||
         annotate(compute,"metal.descriptorInlineBinding",2,0,(uint64_t)(__bridge void *)mainRoot,mainRootOffset) ||
         annotate(compute,"metal.descriptorInlineBinding",2,1,(uint64_t)(__bridge void *)samplers,0))return 4;
      Roots roots={mainRoot.gpuAddress+mainRootOffset,samplers.gpuAddress};
      [compute setBytes:&roots length:sizeof(roots) atIndex:2];
      // Write residency is permission, not evidence that this shader writes CBV bytes.
      [compute useResource:cbv usage:MTLResourceUsageRead|MTLResourceUsageWrite];
      if(viewSharedRoot || scalarRoot)[compute useResource:mainRoot usage:MTLResourceUsageRead];
      [compute useResource:outputA usage:MTLResourceUsageRead|MTLResourceUsageWrite];
      if(partialInput)[compute useResource:partialInput usage:MTLResourceUsageRead];
      [compute useResource:outputB usage:MTLResourceUsageRead|MTLResourceUsageWrite];
      if(dimensionTexture)[compute useResource:dimensionTexture usage:textureAtomics || textureBufferView?
          MTLResourceUsageRead|MTLResourceUsageWrite:MTLResourceUsageRead];
      if(scalarCounter)[compute useResource:counter usage:MTLResourceUsageRead];
      if(nativeRayQuery){[compute useResource:header usage:MTLResourceUsageRead];[compute useResource:contributions usage:MTLResourceUsageRead];[compute useResource:tlas usage:MTLResourceUsageRead];[compute useResource:blas usage:MTLResourceUsageRead];}
      if(loop){[compute useResource:indices usage:MTLResourceUsageRead];[compute useResource:source usage:MTLResourceUsageRead];}
      [compute dispatchThreadgroups:queryGroupIndices?MTLSizeMake(3,1,1):groupBuiltins?MTLSizeMake(2,2,2):MTLSizeMake(1,1,1) threadsPerThreadgroup:groupBuiltins?MTLSizeMake(2,1,1):guardedIndex?MTLSizeMake(64,1,1):MTLSizeMake(1,1,1)];[compute endEncoding];
      [command commit];[command waitUntilCompleted];if(command.status!=MTLCommandBufferStatusCompleted)return 5;
    }
    auto final=[queue commandBuffer];auto read=[final blitCommandEncoder];
    [read copyFromBuffer:outputA sourceOffset:0 toBuffer:readA destinationOffset:0 size:256];
    [read copyFromBuffer:outputB sourceOffset:0 toBuffer:readB destinationOffset:0 size:256];[read endEncoding];
    if(readView)
    {
      auto viewRead=[final blitCommandEncoder];
      [viewRead copyFromBuffer:dimensionBacking sourceOffset:0 toBuffer:readView destinationOffset:0 size:1024];
      [viewRead endEncoding];
    }
    if(readImage)
    {
      auto imageRead=[final blitCommandEncoder];
      [imageRead copyFromTexture:dimensionTexture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
          sourceSize:MTLSizeMake(8,8,1) toBuffer:readImage destinationOffset:32 destinationBytesPerRow:256 destinationBytesPerImage:2048];
      [imageRead endEncoding];
    }
    [final commit];[final waitUntilCompleted];if(final.status!=MTLCommandBufferStatusCompleted)return 5;
    if(api && !api->EndFrameCapture(nullptr,nullptr))return 6;
    if(readView)for(unsigned i=0;i<256;i++)
    {
      const uint32_t settings[4]={2,7,callEffects?568U:456U,1};
      const uint32_t expected=viewSharedRoot && i>=4 && i<8?settings[i-4]:
          i>=64 && i<128?0x12347+i-64:0x09090909;
      if(((uint32_t *)readView.contents)[i]!=expected)return 9;
    }
    if(readImage)for(unsigned i=0;i<2112;i++)
    {
      const unsigned relative=i>=32?i-32:2112;
      const unsigned row=relative/256,column=relative%256;
      unsigned char expected=9;
      if(row<8 && column<32)expected=(0x12346+row*8+column/4)>>(8*(column%4));
      if(((unsigned char *)readImage.contents)[i]!=expected)return 9;
    }
    if(queryCreationInput)
    {
      auto read=[device newBufferWithLength:queryPartialSize options:MTLResourceStorageModeShared];
      auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
      [blit copyFromBuffer:partialInput sourceOffset:0 toBuffer:read destinationOffset:0 size:queryPartialSize];[blit endEncoding];
      [command commit];[command waitUntilCompleted];if(command.status!=MTLCommandBufferStatusCompleted)return 9;
      for(unsigned i=0;i<queryPartialSize;i++)
      {
        unsigned char expected=queryCreationInput>=3?9:0;
        if(queryCreationInput>=3 && i>=queryPartialOffset && i<queryPartialOffset+4)expected=(partialWord>>(8*(i-queryPartialOffset)))&255;
        if(((unsigned char *)read.contents)[i]!=expected)return 9;
      }
      printf("PASS Native buffer birth input: kind=%u bytes=%u offset=%u; complete API-defined contents\n",queryCreationInput,queryPartialSize,queryPartialOffset);
    }
    if(contributionProducer)for(unsigned i=0;i<contributions.length;i++)
    {const unsigned char expected=i<queryContributionOffset?0x6a:0;
      if(((unsigned char *)contributions.contents)[i]!=expected)return 10;}
    for(unsigned i=0;i<64;i++)
    {
      uint32_t expectedA=i==3+rmwOffset?123U+unsigned(readModifyWrite):0x09090909U,expectedB=i==7+rmwOffset?456U+unsigned(readModifyWrite):0x09090909U;
      if(queryGroupIndices)
      {if(i>=3+rmwOffset && i<6+rmwOffset)expectedA=123;
       if(i>=7+rmwOffset && i<10+rmwOffset)expectedB=456;}
      if(queryDynamicHeap && i==63){expectedA=1011;expectedB=2022;}
      if(literalProjection)
      {
        if(i>=3+rmwOffset && i<=6+rmwOffset)expectedA=123+i-3-rmwOffset+unsigned(readModifyWrite);
        if(i>=7+rmwOffset && i<=10+rmwOffset)expectedB=456+i-7-rmwOffset+unsigned(readModifyWrite);
      }
      if(loop)
      {
        expectedA=expectedB=0x09090909U;
        const uint32_t positionsA[4]={3,12,27,55},positionsB[4]={7,19,35,63};
        for(unsigned j=0;j<4;j++){if(i==positionsA[j])expectedA=sourceValues[j]+123;if(i==positionsB[j])expectedB=sourceValues[j]+456;}
      }
      if(((uint32_t *)readA.contents)[i]!=expectedA || ((uint32_t *)readB.contents)[i]!=expectedB)return 7;
    }
    if(loop)printf("PASS runtime indexed loop: 4 published indices and source values, two complete outputs/padding; no RT dispatch\n");
    else if(nativeRayQuery)printf("PASS runtime Native query: actual hit/distance/instance and miss, A[%u]=123 B[%u]=456, full outputs/padding; instances=%u header=%u contribution=%u\n",3+rmwOffset,7+rmwOffset,queryInstances,queryHeaderOffset,queryContributionOffset);
    else if(readModifyWrite)printf("PASS runtime Native read-modify-write: A[%u]=124 B[%u]=457, full padding; no RT dispatch\n",3+rmwOffset,7+rmwOffset);
    else printf("PASS runtime dynamic heap stores: A[3]=123 B[7]=456, full padding, GPU-published Private CBV, unused AS slot; no RT dispatch\n");
    if(chain)printf("PASS Private copy chain: %s staging, split nonzero ranges, %s submission\n",frame?"frame-born":"background",separate?"separate":"same");
    if(opaque)printf("PASS native opaque writer between copies; replay must reject unknown scalar publication\n");
  }
}
