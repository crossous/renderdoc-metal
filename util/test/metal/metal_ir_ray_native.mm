// SPDX-License-Identifier: MIT
// Native converted DXR pipeline; uses UE's actual Apache-2.0 IR runtime header.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#define IR_PRIVATE_IMPLEMENTATION
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>
#include <metal_irconverter_runtime/ir_raytracing.h>
#include <cstdio>
#include <algorithm>
#include <initializer_list>
#include <dlfcn.h>
#include "renderdoc/api/app/renderdoc_app.h"

static id<MTLFunction> Load(id<MTLDevice> device, NSString *directory, NSString *name,
                           NSMutableArray *libraries)
{
  NSError *error = nil;
  auto library = [device newLibraryWithURL:[NSURL fileURLWithPath:
      [directory stringByAppendingPathComponent:[name stringByAppendingString:@".metallib"]]] error:&error];
  if(!library || library.functionNames.count != 1)
  {
    fprintf(stderr, "library %s: %s\n", name.UTF8String, error.description.UTF8String);
    return nil;
  }
  printf("function %s -> %s\n", name.UTF8String, library.functionNames[0].UTF8String);
  [libraries addObject:library];
  return [library newFunctionWithName:library.functionNames[0]];
}

int main(int argc, char **argv)
{
  if(argc != 2 && argc != 3) return 1;
  setvbuf(stdout, nullptr, _IONBF, 0);
  @autoreleasepool
  {
    __attribute__((objc_precise_lifetime)) id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if(!device.supportsRaytracing) return 2;
    auto queue = [device newCommandQueue];
    RENDERDOC_API_1_7_0 *api = nullptr;
    auto getAPI = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    if(getAPI) getAPI(eRENDERDOC_API_Version_1_7_0, (void **)&api);
    __attribute__((objc_precise_lifetime)) NSMutableArray *libraries = [NSMutableArray array];
    NSString *directory = [NSString stringWithUTF8String:argv[1]];
    auto raygen = Load(device, directory, @"raygen", libraries);
    auto hit = Load(device, directory, @"closest_hit", libraries);
    auto miss = Load(device, directory, @"miss", libraries);
    auto anyHit = Load(device, directory, @"any_hit", libraries);
    auto intersect = Load(device, directory, @"intersection", libraries);
    auto dispatch = Load(device, directory, @"dispatch", libraries);
    if(!raygen || !hit || !miss || !anyHit || !intersect || !dispatch) return 3;
    auto linked = [MTLLinkedFunctions new]; linked.functions = @[raygen, hit, miss, intersect, anyHit];
    auto descriptor = [MTLComputePipelineDescriptor new]; descriptor.computeFunction = dispatch;
    descriptor.label = @"Converted DXR IR pipeline";
    descriptor.linkedFunctions = linked; descriptor.maxCallStackDepth = 2;
    NSError *error = nil;
    printf("begin converted IR pipeline compilation\n");
    auto pipeline = [device newComputePipelineStateWithDescriptor:descriptor
        options:MTLPipelineOptionArgumentInfo reflection:nil error:&error];
    if(!pipeline) {fprintf(stderr, "pipeline %s\n", error.description.UTF8String); return 4;}
    printf("end converted IR pipeline compilation\n");
    auto vd = [MTLVisibleFunctionTableDescriptor visibleFunctionTableDescriptor]; vd.functionCount = 5;
    auto visible = [pipeline newVisibleFunctionTableWithDescriptor:vd];
    id<MTLFunctionHandle> handles[] = {[pipeline functionHandleWithFunction:raygen],
        [pipeline functionHandleWithFunction:miss], [pipeline functionHandleWithFunction:hit],
        [pipeline functionHandleWithFunction:anyHit]};
    for(unsigned i = 0; i < 4; i++) [visible setFunction:handles[i] atIndex:i+1];
    const char *mode = getenv("RENDERDOC_IR_RAY_MODE"); if(!mode) mode = "default";
    const bool multiGeometry = strstr(mode,"geometry") && strstr(mode,"multi");
    const unsigned geometryCount = multiGeometry ? (strstr(mode,"multi64") ? 64 : 2) : 1;
    auto idesc = [MTLIntersectionFunctionTableDescriptor intersectionFunctionTableDescriptor]; idesc.functionCount = multiGeometry ? 2 : 1;
    auto table = [pipeline newIntersectionFunctionTableWithDescriptor:idesc];
    [table setFunction:[pipeline functionHandleWithFunction:intersect] atIndex:0];
    if(multiGeometry)[table setFunction:[pipeline functionHandleWithFunction:intersect] atIndex:1];
    const bool localRoot = strstr(mode,"local-root");
    const uint64_t samplerCount = strstr(mode,"six-samplers") ? 6 : 1;
    const bool nullHit = strstr(mode, "null-hit") || strstr(mode, "null-both") || strstr(mode, "no-closest");
    const bool nullMiss = strstr(mode, "null-miss") || strstr(mode, "null-both");
    const bool any = strstr(mode, "any-hit");
    const bool globalRoot = strstr(mode,"global-");
    const bool descriptorHeaps = strstr(mode,"descriptor-heaps");
    const bool heapAS = strstr(mode,"heap-as");
    const bool heapOnly = strstr(mode,"heap-as-only");
    const bool indirect = strstr(mode,"indirect-tlas");
    const bool emptyAS = indirect && strstr(mode,"-empty");
    const bool maskedAS = indirect && strstr(mode,"-masked");
    const bool privateAS = indirect && strstr(mode,"-private");
    const bool frameBuild = strstr(mode,"frame-build");
    const bool frameGeometry = strstr(mode,"geometry");
    const bool indexedGeometry = frameGeometry && strstr(mode,"indexed");
    const bool newGeometryTarget = frameGeometry && strstr(mode,"new-target");
    const NSUInteger rayScratchOffset = strstr(mode,"scratch-offset") ? 256 : 0;
    id<MTLBuffer> frameVerify=nil;
    const uint64_t indirectCount = emptyAS ? 0 : 1;
    const float triangle[] = {-1,-1,0, 1,-1,0, 0,1,0};
    auto vertex = [device newBufferWithBytes:triangle length:sizeof(triangle) options:MTLResourceStorageModeShared];
    auto geometry = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    geometry.vertexBuffer = vertex; geometry.vertexStride = 12; geometry.triangleCount = 1; geometry.opaque = NO;
    auto primitive = [MTLPrimitiveAccelerationStructureDescriptor descriptor]; primitive.geometryDescriptors = @[geometry];
    auto sizes = [device accelerationStructureSizesWithDescriptor:primitive];
    const NSUInteger blasCapacity=multiGeometry ? 1048576 : sizes.accelerationStructureSize;
    auto blas = [device newAccelerationStructureWithSize:blasCapacity];
    auto scratch = [device newBufferWithLength:sizes.buildScratchBufferSize options:MTLResourceStorageModePrivate];
    auto command = [queue commandBuffer]; auto acceleration = [command accelerationStructureCommandEncoder];
    [acceleration buildAccelerationStructure:blas descriptor:primitive scratchBuffer:scratch scratchBufferOffset:0];
    [acceleration endEncoding]; [command commit]; [command waitUntilCompleted];
    if(command.error) return 5;
    id<MTLAccelerationStructure> frameBLAS = newGeometryTarget ?
        [device newAccelerationStructureWithSize:blasCapacity] : blas;
    if(newGeometryTarget)frameBLAS.label=@"IR frame BLAS target";
    id<MTLBuffer> geometryVertices=nil,geometryIndices=nil,geometryUpload=nil,indexUpload=nil,geometryVerify=nil,indexVerify=nil;
    MTLPrimitiveAccelerationStructureDescriptor *framePrimitive=nil;
    id<MTLBuffer> geometryScratch=nil;
    if(frameGeometry)
    {
      const NSUInteger vertexLength=multiGeometry?256:72,indexLength=multiGeometry?64:24;
      uint8_t data[256]={};float shifted[3][3]={{-1,-1,0},{1,-1,0},{0,1,0}};
      for(unsigned i=0;i<3;i++){shifted[i][0]+=heapAS?-100:100;memcpy(data+(multiGeometry?160:16)+i*16,shifted[i],12);}
      if(multiGeometry)
      {
        float distant[3][4]={{-201,-1,0,1},{-199,-1,0,1},{-200,1,0,1}};
        for(unsigned i=0;i<3;i++)memcpy(data+16+i*32,distant[i],16);
        // Unreferenced padding is deliberately non-finite; only vertices used
        // by the indexed geometry must be finite.
        const uint32_t nan=0x7fc00000;memcpy(data+236,&nan,4);
      }
      geometryUpload=[device newBufferWithBytes:data length:vertexLength options:MTLResourceStorageModeShared];
      geometryUpload.label=@"IR geometry vertex upload";
      geometryVertices=[device newBufferWithLength:vertexLength options:MTLResourceStorageModePrivate];
      geometryVertices.label=@"IR geometry vertex source";
      uint8_t zero[256]={};geometryVerify=[device newBufferWithBytes:zero length:vertexLength options:MTLResourceStorageModeShared];
      geometryVerify.label=@"IR geometry vertex verification";
      auto g=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
      g.vertexBuffer=geometryVertices;g.vertexBufferOffset=16;g.vertexStride=16;g.triangleCount=1;g.opaque=NO;
      if(indexedGeometry)
      {
        uint8_t raw[64]={};uint32_t indices[3]={2,0,1};memcpy(raw+(multiGeometry?24:4),indices,12);
        if(multiGeometry){uint16_t small[3]={2,0,1};memcpy(raw+4,small,6);}
        indexUpload=[device newBufferWithBytes:raw length:indexLength options:MTLResourceStorageModeShared];
        indexUpload.label=@"IR geometry index upload";
        geometryIndices=[device newBufferWithLength:indexLength options:MTLResourceStorageModePrivate];
        geometryIndices.label=@"IR geometry index source";
        uint8_t empty[64]={};indexVerify=[device newBufferWithBytes:empty length:indexLength options:MTLResourceStorageModeShared];
        indexVerify.label=@"IR geometry index verification";
        g.indexBuffer=geometryIndices;g.indexBufferOffset=4;g.indexType=MTLIndexTypeUInt32;
      }
      NSMutableArray *descriptors=[NSMutableArray array];
      if(multiGeometry)
      {
        for(unsigned i=0;i<geometryCount;i++)
        {
          auto item=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
          const bool last=i+1==geometryCount;
          item.vertexBuffer=geometryVertices;item.vertexBufferOffset=last?160:16;
          item.vertexStride=last?16:32;item.vertexFormat=last?MTLAttributeFormatFloat3:MTLAttributeFormatFloat4;
          item.indexBuffer=geometryIndices;item.indexBufferOffset=last?24:4;item.indexType=last?MTLIndexTypeUInt32:MTLIndexTypeUInt16;
          item.triangleCount=1;item.opaque=NO;item.allowDuplicateIntersectionFunctionInvocation=YES;
          item.intersectionFunctionTableOffset=last?1:0;[descriptors addObject:item];
        }
      }
      else [descriptors addObject:g];
      framePrimitive=[MTLPrimitiveAccelerationStructureDescriptor descriptor];framePrimitive.geometryDescriptors=descriptors;
      auto s=[device accelerationStructureSizesWithDescriptor:framePrimitive];
      geometryScratch=[device newBufferWithLength:s.buildScratchBufferSize+256 options:MTLResourceStorageModePrivate];
      if(s.accelerationStructureSize>blasCapacity)return 50;
    }
    MTLAccelerationStructureInstanceDescriptor instance = {};
    instance.transformationMatrix.columns[0].x = 1;
    instance.transformationMatrix.columns[1].y = 1;
    instance.transformationMatrix.columns[2].z = 1; instance.mask = 0xff;
    auto indirectInput = [&](decltype(instance.transformationMatrix) transform) -> id<MTLBuffer> {
      MTLIndirectAccelerationStructureInstanceDescriptor data = {};
      data.transformationMatrix=transform;data.mask=maskedAS?0:0xff;data.userID=73;
      if(!emptyAS && !maskedAS) data.accelerationStructureID=blas.gpuResourceID;
      uint8_t backing[88] = {};memcpy(backing+8,&data,sizeof(data));
      auto upload=[device newBufferWithBytes:backing length:sizeof(backing) options:MTLResourceStorageModeShared];
      if(!privateAS)return upload;
      auto target=[device newBufferWithLength:sizeof(backing) options:MTLResourceStorageModePrivate];
      auto copy=[queue commandBuffer];auto blit=[copy blitCommandEncoder];
      [blit copyFromBuffer:upload sourceOffset:0 toBuffer:target destinationOffset:0 size:sizeof(backing)];
      [blit endEncoding];[copy commit];[copy waitUntilCompleted];if(copy.error)return nil;
      return target;
    };
    auto clearIndirectInput = [&](id<MTLBuffer> source) {
      if(!privateAS)return true;
      auto verify=frameVerify?frameVerify:[device newBufferWithLength:88 options:MTLResourceStorageModeShared];
      auto clear=[queue commandBuffer];auto blit=[clear blitCommandEncoder];
      [blit fillBuffer:source range:NSMakeRange(0,88) value:0];
      [blit copyFromBuffer:source sourceOffset:0 toBuffer:verify destinationOffset:0 size:88];
      [blit endEncoding];[clear commit];[clear waitUntilCompleted];if(clear.error)return false;
      const auto bytes=(const uint8_t *)verify.contents;
      for(unsigned i=0;i<88;i++)if(bytes[i])return false;
      printf("PASS Private/GPU indirect input cleared after AS build: 88 bytes zero\n");
      return true;
    };
    auto instances = [device newBufferWithBytes:&instance length:sizeof(instance) options:MTLResourceStorageModeShared];
    auto top = [MTLInstanceAccelerationStructureDescriptor descriptor];
    top.instanceDescriptorBuffer = instances; top.instanceCount = 1; top.instancedAccelerationStructures = @[blas];
    if(indirect && !heapAS)
    {
      instances=indirectInput(instance.transformationMatrix);if(!instances)return 35;
      instances.label=@"IR indirect instance source";
      top.instanceDescriptorBuffer=instances;top.instanceDescriptorBufferOffset=8;
      top.instanceDescriptorStride=80;top.instanceDescriptorType=MTLAccelerationStructureInstanceDescriptorTypeIndirect;
      top.instanceCount=indirectCount;top.instancedAccelerationStructures=nil;
    }
    blas.label=@"IR shared BLAS";
    sizes = [device accelerationStructureSizesWithDescriptor:top];
    if(frameBuild)
    {
      const auto count=top.instanceCount;top.instanceCount=1;
      const auto active=[device accelerationStructureSizesWithDescriptor:top];top.instanceCount=count;
      sizes.accelerationStructureSize=std::max(sizes.accelerationStructureSize,active.accelerationStructureSize);
      sizes.buildScratchBufferSize=std::max(sizes.buildScratchBufferSize,active.buildScratchBufferSize);
    }
    uint8_t guardBytes[256];memset(guardBytes,0xa5,sizeof(guardBytes));
    id<MTLBuffer> scratchGuardReadback = rayScratchOffset ?
        [device newBufferWithBytes:guardBytes length:sizeof(guardBytes) options:MTLResourceStorageModeShared] : nil;
    if(scratchGuardReadback)scratchGuardReadback.label=@"IR scratch guard readback";
    auto initializeScratchGuard = [&](id<MTLBuffer> target) {
      if(!rayScratchOffset)return true;
      auto cb=[queue commandBuffer];auto blit=[cb blitCommandEncoder];
      [blit fillBuffer:target range:NSMakeRange(0,256) value:0xa5];
      [blit endEncoding];[cb commit];[cb waitUntilCompleted];return cb.error==nil;
    };
    auto verifyScratchGuard = [&](id<MTLBuffer> target) {
      if(!rayScratchOffset)return true;
      auto cb=[queue commandBuffer];auto blit=[cb blitCommandEncoder];
      [blit copyFromBuffer:target sourceOffset:0 toBuffer:scratchGuardReadback destinationOffset:0 size:256];
      [blit endEncoding];[cb commit];[cb waitUntilCompleted];
      if(cb.error || memcmp(scratchGuardReadback.contents,guardBytes,256))return false;
      printf("PASS scratchOffset=256 preserves prefix guard: 256 bytes 0xa5\n");return true;
    };
    auto tlas = [device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
    scratch = [device newBufferWithLength:sizes.buildScratchBufferSize+(heapAS?0:rayScratchOffset) options:MTLResourceStorageModePrivate];
    if(!heapAS && !initializeScratchGuard(scratch))return 47;
    command = [queue commandBuffer]; acceleration = [command accelerationStructureCommandEncoder];
    [acceleration buildAccelerationStructure:tlas descriptor:top scratchBuffer:scratch scratchBufferOffset:heapAS?0:rayScratchOffset];
    [acceleration endEncoding]; [command commit]; [command waitUntilCompleted];
    if(command.error || (!heapAS && !verifyScratchGuard(scratch))) return 6;
    if(indirect && !heapAS && !clearIndirectInput(instances))return 37;
    __attribute__((objc_precise_lifetime)) NSMutableArray *globalSamplers = [NSMutableArray array];
    id<MTLBuffer> globalCB=nil,globalRaw=nil,globalTextures=nil,globalSamplerTable=nil;
    id<MTLTexture> globalTexture=nil;
    const uint32_t initial[] = {7,7};
    auto output = [device newBufferWithBytes:initial length:sizeof(initial) options:MTLResourceStorageModeShared];
    output.label = @"IR ray results";
    auto contributions = [device newBufferWithLength:4 options:MTLResourceStorageModeShared];
    auto header = [device newBufferWithLength:sizeof(IRRaytracingAccelerationStructureGPUHeader) options:MTLResourceStorageModeShared];
    contributions.label = @"IR instance contributions"; header.label = @"IR AS header";
    memset(header.contents, 0, header.length); const uint32_t contribution = 0;
    IRRaytracingSetAccelerationStructure((uint8_t *)header.contents, tlas.gpuResourceID,
        (uint8_t *)contributions.contents, contributions.gpuAddress, &contribution, indirect && !heapAS && !frameBuild ? indirectCount : 1);
    id<MTLAccelerationStructure> heapTLAS=nil;
    id<MTLBuffer> heapHeader=nil,heapContributions=nil,heapInstances=nil;
    MTLInstanceAccelerationStructureDescriptor *heapTop=nil;
    id<MTLBuffer> heapScratch=nil;
    if(heapAS)
    {
      auto translated=instance;translated.transformationMatrix.columns[3].x=100;
      heapInstances=[device newBufferWithBytes:&translated length:sizeof(translated) options:MTLResourceStorageModeShared];
      heapTop=[MTLInstanceAccelerationStructureDescriptor descriptor];
      heapTop.instanceDescriptorBuffer=heapInstances;heapTop.instanceCount=1;heapTop.instancedAccelerationStructures=@[blas];
      if(indirect)
      {
        heapInstances=indirectInput(translated.transformationMatrix);if(!heapInstances)return 36;
        heapInstances.label=@"IR heap indirect instance source";
        heapTop.instanceDescriptorBuffer=heapInstances;heapTop.instanceDescriptorBufferOffset=8;
        heapTop.instanceDescriptorStride=80;heapTop.instanceDescriptorType=MTLAccelerationStructureInstanceDescriptorTypeIndirect;
        heapTop.instanceCount=indirectCount;heapTop.instancedAccelerationStructures=nil;
      }
      auto heapSizes=[device accelerationStructureSizesWithDescriptor:heapTop];
      if(frameBuild)
      {
        const auto count=heapTop.instanceCount;heapTop.instanceCount=1;
        const auto active=[device accelerationStructureSizesWithDescriptor:heapTop];heapTop.instanceCount=count;
        heapSizes.accelerationStructureSize=std::max(heapSizes.accelerationStructureSize,active.accelerationStructureSize);
        heapSizes.buildScratchBufferSize=std::max(heapSizes.buildScratchBufferSize,active.buildScratchBufferSize);
      }
      heapTLAS=[device newAccelerationStructureWithSize:heapSizes.accelerationStructureSize];
      heapScratch=[device newBufferWithLength:heapSizes.buildScratchBufferSize+rayScratchOffset options:MTLResourceStorageModePrivate];
      if(!initializeScratchGuard(heapScratch))return 48;
      command=[queue commandBuffer];acceleration=[command accelerationStructureCommandEncoder];
      [acceleration buildAccelerationStructure:heapTLAS descriptor:heapTop scratchBuffer:heapScratch scratchBufferOffset:rayScratchOffset];
      [acceleration endEncoding];[command commit];[command waitUntilCompleted];if(command.error || !verifyScratchGuard(heapScratch))return 30;
      if(indirect && !clearIndirectInput(heapInstances))return 38;
      blas.label=@"IR shared BLAS";auto blasIdentity=blas.gpuResourceID;if(!blasIdentity._impl)return 34;
      heapContributions=[device newBufferWithLength:8 options:MTLResourceStorageModeShared];heapContributions.label=@"IR heap instance contributions";memset(heapContributions.contents,0,8);
      heapHeader=[device newBufferWithLength:64 options:MTLResourceStorageModeShared];heapHeader.label=@"IR heap AS header";
      memset(heapHeader.contents,0,64);
      IRRaytracingSetAccelerationStructure((uint8_t *)heapHeader.contents,heapTLAS.gpuResourceID,
          (uint8_t *)heapContributions.contents,heapContributions.gpuAddress,&contribution,indirect && !frameBuild ? indirectCount : 1);
    }
    uint64_t root[8] = {header.gpuAddress,output.gpuAddress};
    if(globalRoot)
    {
      uint32_t cb[128]={};cb[64]=1000;const uint32_t raw[8]={0,5};
      globalCB=[device newBufferWithBytes:cb length:sizeof(cb) options:MTLResourceStorageModeShared];
      globalRaw=[device newBufferWithBytes:raw length:sizeof(raw) options:MTLResourceStorageModeShared];
      globalCB.label=@"IR global CBV";globalRaw.label=@"IR global SRV";
      auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float width:4 height:1 mipmapped:NO];
      td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;
      globalTexture=[device newTextureWithDescriptor:td];globalTexture.label=@"IR global texture";
      const float pixels[]={3,9,15,21};
      [globalTexture replaceRegion:MTLRegionMake2D(0,0,4,1) mipmapLevel:0 withBytes:pixels bytesPerRow:16];
      IRDescriptorTableEntry textureEntry={},samplers[6]={};
      IRDescriptorTableSetTexture(&textureEntry,globalTexture,0.0f,0);
      for(unsigned i=0;i<6;i++)
      {
        auto sd=[MTLSamplerDescriptor new];sd.minFilter=sd.magFilter=i<2?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
        sd.mipFilter=i<4?MTLSamplerMipFilterNearest:MTLSamplerMipFilterLinear;
        sd.sAddressMode=sd.tAddressMode=sd.rAddressMode=i%2?MTLSamplerAddressModeClampToEdge:MTLSamplerAddressModeRepeat;
        sd.supportArgumentBuffers=YES;sd.lodMaxClamp=1000;
        auto sampler=[device newSamplerStateWithDescriptor:sd];[globalSamplers addObject:sampler];
        IRDescriptorTableSetSampler(&samplers[i],sampler,0.0f);
      }
      globalTextures=[device newBufferWithBytes:&textureEntry length:sizeof(textureEntry) options:MTLResourceStorageModeShared];
      globalSamplerTable=[device newBufferWithBytes:samplers length:sizeof(samplers) options:MTLResourceStorageModeShared];
      globalTextures.label=@"IR global texture table";globalSamplerTable.label=@"IR global static sampler table";
      root[2]=globalCB.gpuAddress+256;
      const uint32_t constants[]={200,0,0,1};memcpy(&root[3],constants,16);
      root[5]=globalRaw.gpuAddress+4;root[6]=globalTextures.gpuAddress;root[7]=globalSamplerTable.gpuAddress;
    }
    if(heapOnly){root[0]=output.gpuAddress;root[1]=0;}
    const uint64_t rootWords=globalRoot?8:2;
    auto grs=[device newBufferWithBytes:root length:rootWords*8 options:MTLResourceStorageModeShared];
    grs.label = @"IR global root";
    id<MTLBuffer> localCB = nil, localRaw = nil, localTextures = nil, localSamplers = nil;
    __attribute__((objc_precise_lifetime)) NSMutableArray *staticSamplers = [NSMutableArray array];
    id<MTLTexture> localTexture = nil; id<MTLSamplerState> localSampler = nil;
    IRShaderIdentifier records[3];
    IRShaderIdentifierInit(&records[0], 1); IRShaderIdentifierInit(&records[1], 2);
    IRShaderIdentifierInit(&records[2], 3);
    const uint64_t recordPad = strstr(mode, "ue-") ? ~0ULL :
        !strcmp(mode, "pattern-pad") ? 0xa5a5d00d98761234ULL : 0;
    for(auto &record : records) record.pad0 = recordPad;
    if(nullHit) records[2].shaderHandle = 0;
    if(nullMiss) records[1].shaderHandle = 0;
    if(any) records[2].intersectionShaderHandle = 4;
    struct LocalRecord
    {
      IRShaderIdentifier shader;
      uint64_t constantBuffer;
      uint32_t constants[4];
      uint64_t rawBuffer, textureTable;
      uint8_t alignment[24];
    };
    static_assert(sizeof(LocalRecord) == 96, "SBT root packing");
    LocalRecord localRecords[3] = {};
    if(localRoot)
    {
      uint32_t cbData[128] = {}; cbData[0] = 200; cbData[64] = 100;
      const uint32_t rawData[8] = {0,0,5,7};
      localCB = [device newBufferWithBytes:cbData length:sizeof(cbData) options:MTLResourceStorageModeShared];
      localRaw = [device newBufferWithBytes:rawData length:sizeof(rawData) options:MTLResourceStorageModeShared];
      localCB.label = @"IR local CBV"; localRaw.label = @"IR local SRV";
      auto td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float width:4 height:1 mipmapped:NO];
      td.storageMode = MTLStorageModeShared; td.usage = MTLTextureUsageShaderRead;
      localTexture = [device newTextureWithDescriptor:td]; localTexture.label = @"IR local texture";
      const float pixels[] = {3,9,15,21};
      [localTexture replaceRegion:MTLRegionMake2D(0,0,4,1) mipmapLevel:0 withBytes:pixels bytesPerRow:16];
      IRDescriptorTableEntry textureEntry = {}, samplerEntries[6] = {};
      IRDescriptorTableSetTexture(&textureEntry,localTexture,0.0f,0);
      for(unsigned i=0;i<samplerCount;i++)
      {
        auto sd = [MTLSamplerDescriptor new];
        sd.minFilter = sd.magFilter = samplerCount==6 && i<2 ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
        sd.mipFilter = samplerCount==6 && i>=4 ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
        sd.sAddressMode = sd.tAddressMode = sd.rAddressMode = samplerCount==6 && i%2==0 ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToEdge;
        sd.supportArgumentBuffers = YES; sd.lodMaxClamp = 1000; sd.label = @"IR local static sampler";
        localSampler = [device newSamplerStateWithDescriptor:sd]; [staticSamplers addObject:localSampler];
        IRDescriptorTableSetSampler(&samplerEntries[i],localSampler,0.0f);
      }
      localTextures = [device newBufferWithBytes:&textureEntry length:sizeof(textureEntry) options:MTLResourceStorageModeShared];
      localSamplers = [device newBufferWithBytes:samplerEntries length:samplerCount*sizeof(IRDescriptorTableEntry) options:MTLResourceStorageModeShared];
      localTextures.label = @"IR local texture table"; localSamplers.label = @"IR local static sampler table";
      for(unsigned i=0; i<3; i++)
      {
        localRecords[i].shader = records[i];
        if(i && (records[i].shaderHandle || records[i].intersectionShaderHandle))
        {
          localRecords[i].shader.localRootSignatureSamplersBuffer = localSamplers.gpuAddress;
          localRecords[i].constantBuffer = localCB.gpuAddress + (i == 2 ? 256 : 0);
          localRecords[i].constants[0] = i == 2 ? 20 : 30; localRecords[i].constants[3] = 1;
          localRecords[i].rawBuffer = localRaw.gpuAddress + (i == 2 ? 8 : 12);
          localRecords[i].textureTable = localTextures.gpuAddress;
        }
      }
    }
    const uint64_t recordStride = localRoot ? sizeof(LocalRecord) : sizeof(IRShaderIdentifier);
    auto sbt = [device newBufferWithBytes:localRoot ? (void *)localRecords : (void *)records
        length:localRoot ? sizeof(localRecords) : sizeof(records) options:MTLResourceStorageModeShared];
    sbt.label = @"IR shader records";
    id<MTLBuffer> resourceHeap=nil,samplerHeap=nil,heapRaw=nil;
    id<MTLTexture> heapTexture=nil;id<MTLSamplerState> heapSampler=nil;
    if(descriptorHeaps)
    {
      const uint32_t raw[]={0,17,0,31,0,0,0,0};
      heapRaw=[device newBufferWithBytes:raw length:sizeof(raw) options:MTLResourceStorageModeShared];heapRaw.label=@"IR heap SRV";
      auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float width:4 height:1 mipmapped:NO];
      td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;
      heapTexture=[device newTextureWithDescriptor:td];heapTexture.label=@"IR heap texture";
      const float pixels[]={3,9,15,21};[heapTexture replaceRegion:MTLRegionMake2D(0,0,4,1) mipmapLevel:0 withBytes:pixels bytesPerRow:16];
      auto sd=[MTLSamplerDescriptor new];sd.minFilter=sd.magFilter=MTLSamplerMinMagFilterLinear;sd.mipFilter=MTLSamplerMipFilterNearest;
      sd.sAddressMode=sd.tAddressMode=sd.rAddressMode=MTLSamplerAddressModeClampToEdge;sd.supportArgumentBuffers=YES;sd.lodMaxClamp=1000;
      heapSampler=[device newSamplerStateWithDescriptor:sd];
      IRDescriptorTableEntry entries[4]={},samplers[2]={};
      IRDescriptorTableSetBuffer(&entries[0],heapRaw.gpuAddress+4,4);
      IRDescriptorTableSetTexture(&entries[1],heapTexture,0.0f,0);
      IRDescriptorTableSetBuffer(&entries[3],heapRaw.gpuAddress+12,4);
      if(heapAS) IRDescriptorTableSetAccelerationStructure(&entries[2],heapHeader.gpuAddress);
      IRDescriptorTableSetSampler(&samplers[1],heapSampler,0.0f);
      resourceHeap=[device newBufferWithBytes:entries length:sizeof(entries) options:MTLResourceStorageModeShared];resourceHeap.label=@"IR resource descriptor heap";
      samplerHeap=[device newBufferWithBytes:samplers length:sizeof(samplers) options:MTLResourceStorageModeShared];samplerHeap.label=@"IR sampler descriptor heap";
    }
    IRDispatchRaysArgument packet = {};
    packet.DispatchRaysDesc.RayGenerationShaderRecord = {sbt.gpuAddress, sizeof(records[0])};
    packet.DispatchRaysDesc.MissShaderTable = {sbt.gpuAddress + recordStride, recordStride, recordStride};
    packet.DispatchRaysDesc.HitGroupTable = {sbt.gpuAddress + recordStride*2, recordStride, localRoot ? recordStride : 0};
    packet.DispatchRaysDesc.Width = 2; packet.DispatchRaysDesc.Height = 1; packet.DispatchRaysDesc.Depth = 1;
    packet.GRS = grs.gpuAddress; packet.VisibleFunctionTable = visible.gpuResourceID;
    packet.IntersectionFunctionTable = table.gpuResourceID;
    if(descriptorHeaps){packet.ResDescHeap=resourceHeap.gpuAddress;packet.SmpDescHeap=samplerHeap.gpuAddress;}
    auto arguments = [device newBufferWithBytes:&packet length:sizeof(packet) options:MTLResourceStorageModeShared];
    arguments.label = @"IR dispatch packet";
    id<MTLBuffer> beforeOutput=nil,beforeRoots=nil,beforeArguments=nil,frameUpload=nil;
    if(frameBuild)
    {
      if(!indirect || !privateAS)return 39;
      beforeOutput=[device newBufferWithBytes:initial length:sizeof(initial) options:MTLResourceStorageModeShared];
      beforeOutput.label=@"IR before ray results";
      beforeRoots=[device newBufferWithBytes:grs.contents length:grs.length options:MTLResourceStorageModeShared];
      beforeRoots.label=@"IR before global root";
      ((uint64_t *)beforeRoots.contents)[heapOnly?0:1]=beforeOutput.gpuAddress;
      auto beforePacket=packet;beforePacket.GRS=beforeRoots.gpuAddress;
      beforeArguments=[device newBufferWithBytes:&beforePacket length:sizeof(beforePacket) options:MTLResourceStorageModeShared];
      beforeArguments.label=@"IR before dispatch packet";
      MTLIndirectAccelerationStructureInstanceDescriptor data={};
      data.transformationMatrix=instance.transformationMatrix;
      if(heapAS)data.transformationMatrix.columns[3].x=100;
      data.mask=0xff;data.userID=74;data.accelerationStructureID=frameBLAS.gpuResourceID;
      uint8_t backing[88]={};memcpy(backing+8,&data,sizeof(data));
      frameUpload=[device newBufferWithBytes:backing length:sizeof(backing) options:MTLResourceStorageModeShared];
      frameUpload.label=@"IR frame instance upload";
      const uint8_t zeros[88]={};
      frameVerify=[device newBufferWithBytes:zeros length:sizeof(zeros) options:MTLResourceStorageModeShared];
      frameVerify.label=@"IR frame source verification";
      if(!frameVerify.gpuAddress)return 47;
    }
    if(api && getenv("RENDERDOC_IR_RAY_TYPED") && !strcmp(getenv("RENDERDOC_IR_RAY_TYPED"), "1"))
    {
      RENDERDOC_AnnotationValue coverage = {}; coverage.uint32 = 3;
      if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)device,
          "metal.descriptorCoverage", eRENDERDOC_UInt32, 0, &coverage)) return 12;
      auto annotate = [&](id object, const char *key, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
        RENDERDOC_AnnotationValue value = {};
        value.vector.uint64[0] = a; value.vector.uint64[1] = b;
        value.vector.uint64[2] = c; value.vector.uint64[3] = d;
        return api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)object,
            key, eRENDERDOC_UInt64, 4, &value);
      };
      if(annotate(pipeline, "metal.rayIRDispatch", (uint64_t)(__bridge void *)arguments, 0, 1, rootWords)) return 13;
      if(frameBuild)
      {
        if(annotate(pipeline,"metal.rayIRDispatch",(uint64_t)(__bridge void *)beforeArguments,0,1,rootWords))return 40;
        for(uint64_t at:{0ULL,16ULL,40ULL,64ULL,104ULL,112ULL,120ULL,144ULL})
          if(annotate(beforeArguments,"metal.descriptorTable",0,at,1,8))return 41;
        if(annotate(beforeArguments,"metal.descriptorTable",6,128,1,8) ||
           annotate(beforeArguments,"metal.descriptorTable",5,136,1,8))return 41;
        for(uint64_t at: globalRoot ? std::initializer_list<uint64_t>{0,8,16,40,48,56} :
                                    heapOnly ? std::initializer_list<uint64_t>{0} : std::initializer_list<uint64_t>{0,8})
          if(annotate(beforeRoots,"metal.descriptorTable",0,at,1,8))return 42;
      }
      if(descriptorHeaps)
      {
        for(const auto &definition:{std::initializer_list<uint64_t>{0,0,0,4},{0,1,1,0},{0,3,0,4},{1,1,2,0}})
        {
          const auto d=definition.begin();
          if(annotate(pipeline,"metal.rayIRHeapEntry",d[0],d[1],d[2],d[3]) ||
             annotate(pipeline,"metal.rayIRHeapEntry",d[0],d[1],d[2],d[3])) return 27;
          if(annotate(pipeline,"metal.rayIRHeapEntry",d[0],d[1],d[2],d[3]+1)!=2) return 28;
        }
        if(heapAS)
        {
          if(annotate(pipeline,"metal.rayIRHeapEntry",0,2,3,64) || annotate(pipeline,"metal.rayIRHeapEntry",0,2,3,64)) return 31;
          if(annotate(pipeline,"metal.rayIRHeapEntry",0,2,3,63)!=2) return 32;
          if(annotate(resourceHeap,"metal.descriptorTable",1,48,1,24) ||
             annotate(heapHeader,"metal.descriptorTable",4,0,1,8) ||
             annotate(heapHeader,"metal.descriptorTable",0,8,1,8)) return 33;
        }
        if(annotate(resourceHeap,"metal.descriptorTable",1,0,2,24) ||
           annotate(resourceHeap,"metal.descriptorTable",1,72,1,24) ||
           annotate(samplerHeap,"metal.descriptorTable",2,24,1,24)) return 29;
      }
      if(heapOnly)
      {
        if(annotate(pipeline,"metal.rayIRGlobalRoot",0,6,1,8) || annotate(pipeline,"metal.rayIRGlobalRoot",8,1,2,8)) return 35;
      }
      if(globalRoot)
      {
        for(const auto &definition:{std::initializer_list<uint64_t>{0,5,1,64},{8,6,1,8},
            {16,4,1,16},{24,1,4,16},{40,0,1,4},{48,2,1,24},{56,3,6,144}})
        {
          const auto d=definition.begin();
          if(annotate(pipeline,"metal.rayIRGlobalRoot",d[0],d[1],d[2],d[3]) ||
             annotate(pipeline,"metal.rayIRGlobalRoot",d[0],d[1],d[2],d[3])) return 24;
          if(annotate(pipeline,"metal.rayIRGlobalRoot",d[0],d[1],d[2],d[3]+1)!=2) return 25;
        }
        for(uint64_t at:{16ULL,40ULL,48ULL,56ULL})
          if(annotate(grs,"metal.descriptorTable",0,at,1,8)) return 26;
        if(annotate(globalTextures,"metal.descriptorTable",1,0,1,24) ||
           annotate(globalSamplerTable,"metal.descriptorTable",2,0,6,24)) return 27;
      }
      for(uint64_t at : {0ULL,16ULL,40ULL,64ULL,104ULL,112ULL,120ULL,144ULL})
        if(annotate(arguments, "metal.descriptorTable", 0, at, 1, 8)) return 14;
      if(annotate(arguments, "metal.descriptorTable", 6, 128, 1, 8) ||
         annotate(arguments, "metal.descriptorTable", 5, 136, 1, 8) ||
         annotate(grs, "metal.descriptorTable", 0, 0, heapOnly?1:2, 8) ||
         annotate(header, "metal.descriptorTable", 4, 0, 1, 8) ||
         annotate(header, "metal.descriptorTable", 0, 8, 1, 8)) return 15;
      for(unsigned role = 0; role < 4; role++)
      {
        auto handle = handles[role];
        RENDERDOC_AnnotationValue value = {}; value.uint32 = role;
        if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)handle,
            "metal.rayIRShaderRole", eRENDERDOC_UInt32, 0, &value)) return 17;
        // Same immutable role is idempotent; conflicting roles cannot silently
        // reinterpret a VFT entry or create another metadata chunk.
        if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)handle,
            "metal.rayIRShaderRole", eRENDERDOC_UInt32, 0, &value)) return 18;
        value.uint32 = (role+1)%4;
        if(api->SetObjectAnnotation((__bridge void *)device, (__bridge void *)handle,
            "metal.rayIRShaderRole", eRENDERDOC_UInt32, 0, &value) != 2) return 19;
      }
      for(uint64_t i=0;i<3;i++)
        if(annotate(sbt, "metal.descriptorTable", 0, i*recordStride+16, 1, 8)) return 16;
      if(localRoot)
      {
        for(unsigned role=1;role<4;role++)
          for(const auto &root : {std::initializer_list<uint64_t>{32,4,1,16}, {40,1,4,16},
              {56,0,1,4}, {64,2,1,24}, {16,3,1,24}})
          {
            auto at=root.begin(); const auto a=at[0],b=at[1],c=b==3 ? samplerCount : at[2],d=b==3 ? samplerCount*24 : at[3];
            if(annotate(handles[role],"metal.rayIRLocalRoot",a,b,c,d) ||
               annotate(handles[role],"metal.rayIRLocalRoot",a,b,c,d)) return 20;
            if(annotate(handles[role],"metal.rayIRLocalRoot",a,b,c,d+1) != 2) return 23;
          }
        for(uint64_t i=1;i<3;i++) for(uint64_t at : {32ULL,56ULL,64ULL})
          if(annotate(sbt,"metal.descriptorTable",0,i*recordStride+at,1,8)) return 21;
        if(annotate(localTextures,"metal.descriptorTable",1,0,1,24) ||
           annotate(localSamplers,"metal.descriptorTable",2,0,samplerCount,24)) return 22;
      }
    }

    auto layer = [CAMetalLayer layer]; layer.device = device; layer.pixelFormat = MTLPixelFormatRGBA16Float;
    layer.drawableSize = CGSizeMake(16,16); layer.framebufferOnly = NO;
    auto drawable = [layer nextDrawable]; if(!drawable) return 9;
    if(api && argc == 3)
    {
      api->SetCaptureFilePathTemplate(argv[2]);
      api->StartFrameCapture(nullptr, nullptr);
    }
    auto encodeRay = [&](id<MTLCommandBuffer> command,id<MTLBuffer> args,id<MTLBuffer> result) {
    auto compute = [command computeCommandEncoder];
    [compute setComputePipelineState:pipeline];
    [compute useResources:(const id<MTLResource>[]){tlas,visible,table,contributions,header,grs,sbt}
        count:7 usage:MTLResourceUsageRead];
    if(globalRoot) [compute useResources:(const id<MTLResource>[]){globalCB,globalRaw,globalTextures,globalSamplerTable,globalTexture}
        count:5 usage:MTLResourceUsageRead];
    if(heapAS) [compute useResources:(const id<MTLResource>[]){heapTLAS,heapHeader,heapContributions}
        count:3 usage:MTLResourceUsageRead];
    if(descriptorHeaps) [compute useResources:(const id<MTLResource>[]){heapRaw,heapTexture,resourceHeap,samplerHeap}
        count:4 usage:MTLResourceUsageRead];
    if(localRoot) [compute useResources:(const id<MTLResource>[]){localCB,localRaw,localTextures,localSamplers,localTexture}
        count:5 usage:MTLResourceUsageRead];
    if(frameBuild)[compute useResource:beforeRoots usage:MTLResourceUsageRead];
    [compute useResource:result usage:MTLResourceUsageWrite];
    [compute setBuffer:args offset:0 atIndex:kIRRayDispatchArgumentsBindPoint];
    [compute dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
    [compute endEncoding];
    };
    if(frameBuild)
    {
      command=[queue commandBuffer];encodeRay(command,beforeArguments,beforeOutput);
      [command commit];[command waitUntilCompleted];if(command.error)return 43;
      if(frameGeometry)
      {
        command=[queue commandBuffer];auto upload=[command blitCommandEncoder];
        [upload copyFromBuffer:geometryUpload sourceOffset:0 toBuffer:geometryVertices destinationOffset:0 size:geometryVertices.length];
        if(indexedGeometry)[upload copyFromBuffer:indexUpload sourceOffset:0 toBuffer:geometryIndices destinationOffset:0 size:geometryIndices.length];
        [upload endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 51;
        command=[queue commandBuffer];auto build=[command accelerationStructureCommandEncoder];
        [build buildAccelerationStructure:frameBLAS descriptor:framePrimitive scratchBuffer:geometryScratch scratchBufferOffset:256];
        [build endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 52;
        command=[queue commandBuffer];auto clear=[command blitCommandEncoder];
        [clear fillBuffer:geometryVertices range:NSMakeRange(0,geometryVertices.length) value:0];
        [clear copyFromBuffer:geometryVertices sourceOffset:0 toBuffer:geometryVerify destinationOffset:0 size:geometryVertices.length];
        if(indexedGeometry)
        {
          [clear fillBuffer:geometryIndices range:NSMakeRange(0,geometryIndices.length) value:0];
          [clear copyFromBuffer:geometryIndices sourceOffset:0 toBuffer:indexVerify destinationOffset:0 size:geometryIndices.length];
        }
        [clear endEncoding];[command commit];[command waitUntilCompleted];
        uint8_t zero[256]={};if(command.error || memcmp(geometryVerify.contents,zero,geometryVertices.length) ||
            (indexedGeometry && memcmp(indexVerify.contents,zero,geometryIndices.length)))return 53;
        printf("PASS Private/GPU frame geometry cleared after BLAS: %lu vertex bytes/%lu index bytes\n",(unsigned long)geometryVertices.length,(unsigned long)(indexedGeometry?geometryIndices.length:0));
      }
      command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
      auto source=heapAS?heapInstances:instances;
      [blit copyFromBuffer:frameUpload sourceOffset:0 toBuffer:source destinationOffset:0 size:88];
      [blit endEncoding];[command commit];[command waitUntilCompleted];if(command.error)return 44;
      command=[queue commandBuffer];acceleration=[command accelerationStructureCommandEncoder];
      MTLInstanceAccelerationStructureDescriptor *descriptor=heapAS?heapTop:top;descriptor.instanceCount=1;
      [acceleration buildAccelerationStructure:heapAS?heapTLAS:tlas descriptor:descriptor
          scratchBuffer:heapAS?heapScratch:scratch scratchBufferOffset:rayScratchOffset];
      [acceleration endEncoding];[command commit];[command waitUntilCompleted];if(command.error || !verifyScratchGuard(heapAS?heapScratch:scratch))return 45;
      if(!clearIndirectInput(source))return 46;
      const auto before=(const uint32_t *)beforeOutput.contents;
      printf("FRAME before TraceRay=%u/%u; rebuilt UserID74 from GPU/Private source, cleared afterward\n",before[0],before[1]);
    }
    command=[queue commandBuffer];encodeRay(command,arguments,output);
    [command presentDrawable:drawable]; [command commit]; [command waitUntilCompleted];
    if(!verifyScratchGuard(heapAS?heapScratch:scratch))return 49;
    if(command.error) {fprintf(stderr, "dispatch %s\n", command.error.description.UTF8String); return 7;}
    const auto values = (const uint32_t *)output.contents;
    const uint32_t bias=globalRoot?1211:0;
    const uint32_t baseMiss = nullMiss ? 7 : localRoot ? 243 : 11;
    const uint32_t baseHit = any?baseMiss:nullHit?7:(localRoot?131:73)+(indirect?(frameBuild?74:73):0)+(multiGeometry?17*(geometryCount-1):0);
    const uint32_t wantMiss = (!frameBuild&&(emptyAS||maskedAS)?baseMiss:(heapAS!=frameGeometry)?baseHit:baseMiss)+bias+(descriptorHeaps?37:0);
    const uint32_t wantHit = (!frameBuild&&(emptyAS||maskedAS)?baseMiss:(heapAS!=frameGeometry)?baseMiss:baseHit)+bias+(descriptorHeaps?23:0);
    bool ok = values[0] == wantHit && values[1] == wantMiss;
    if(frameBuild)
    {
      const auto before=(const uint32_t *)beforeOutput.contents;
      const uint32_t oldHit=any?baseMiss:nullHit?7:(localRoot?131:73)+73;
      ok &= before[0]==((emptyAS||maskedAS)?baseMiss:heapAS?baseMiss:oldHit)+bias+(descriptorHeaps?23:0) &&
            before[1]==((emptyAS||maskedAS)?baseMiss:heapAS?oldHit:baseMiss)+bias+(descriptorHeaps?37:0);
    }
    printf("%s converted TraceRay hit/miss=%u/%u dispatchArgument=%zu shaderID=%zu ASHeader=%zu\n",
        ok ? "PASS" : "FAIL", values[0], values[1], sizeof(packet), sizeof(IRShaderIdentifier), sizeof(IRRaytracingAccelerationStructureGPUHeader));
    printf("IR mode=%s pad=%llu any-hit=%d null-hit=%d null-miss=%d\n",mode,recordPad,any,nullHit,nullMiss);
    if(api && argc == 3 && !api->EndFrameCapture(nullptr, nullptr)) return 11;
    return ok ? 0 : 8;
  }
}
