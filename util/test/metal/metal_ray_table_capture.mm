// SPDX-License-Identifier: MIT
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <set>
#include "renderdoc/api/app/renderdoc_app.h"

// Test-only access to the wrapper's native object verifies exact forwarding,
// without assuming GPU identities are equal between separate processes.
@protocol NativeAccelerationStructureProbe
- (id<MTLAccelerationStructure>)real;
@end
@protocol NativeVisibleFunctionTableProbe
- (id<MTLVisibleFunctionTable>)real;
@end
@protocol NativeIntersectionFunctionTableProbe
- (id<MTLIntersectionFunctionTable>)real;
@end

int main(int argc, char **argv)
{
  if(argc != 2 && argc != 3) return 1;
  const bool fences=argc==3 && (strcmp(argv[2],"fences")==0 || strcmp(argv[2],"background-fences")==0);
  const bool background=argc==3 && strncmp(argv[2],"background",10)==0;
  const bool arrayTables=argc==3 && strstr(argv[2],"array-bindings");
  const bool heapAS=argc==3 && strstr(argv[2],"heap");
  const bool automaticHeap=heapAS && strstr(argv[2],"auto");
  const bool frameHeap=heapAS && strstr(argv[2],"heap-frame");
  const bool identity=argc==3 && strstr(argv[2],"identity");
  const bool tableIdentity=argc==3 && strstr(argv[2],"table-identity");
  const bool frameBornTables=tableIdentity && strstr(argv[2],"frame-born");
  const bool largeVisible=argc==3 && strstr(argv[2],"large-visible");
  const unsigned visibleCount=largeVisible?strtoul(strrchr(argv[2],'-')+1,nullptr,10):0;
  if(largeVisible && (visibleCount<33 || visibleCount>65536)) return 20;
  const bool mutated=argc==3 && (strcmp(argv[2],"background-mutated")==0 || strstr(argv[2],"compact-rebuilt"));
  const bool late=argc==3 && strcmp(argv[2],"background-late")==0;
  const bool tlas=background && strstr(argv[2],"tlas");
  const bool indirect=tlas && strstr(argv[2],"indirect");
  const bool emptyIndirect=indirect && strstr(argv[2],"empty");
  const bool inactiveIndirect=indirect && strstr(argv[2],"inactive");
  const char *manyTag=indirect?strstr(argv[2],"many-"):nullptr;
  const unsigned manyChildren=manyTag?strtoul(manyTag+5,nullptr,10):0;
  if(manyTag && (manyChildren<5 || manyChildren>1024)) return 29;
  const bool userID=tlas && strstr(argv[2],"user-id");
  const unsigned instanceUserID=(userID || indirect) && strstr(argv[2],"high")?0xf0000049U:73U;
  const bool privateInstances=indirect && strstr(argv[2],"private");
  const bool placementInstances=privateInstances && strstr(argv[2],"placement-input");
  const bool sameCBInstances=privateInstances && strstr(argv[2],"same-cb");
  const bool unusedBuiltInstance=privateInstances && strstr(argv[2],"unused-built");
  const bool repeated=tlas && strstr(argv[2],"repeated");
  const bool tlasMutated=tlas && !indirect && strstr(argv[2],"mutated");
  const bool tlasLate=tlas && strstr(argv[2],"late");
  const bool stale=tlas && strstr(argv[2],"stale");
  const bool frameTLAS=tlas && strstr(argv[2],"frame");
  const bool frameRebuild=frameTLAS && privateInstances && strstr(argv[2],"frame-rebuild");
  const bool multiIndexed=argc==3 && strstr(argv[2],"multi-indexed");
  const bool privateInputs=multiIndexed && strstr(argv[2],"private");
  const bool sameCBGeometry=multiIndexed && strstr(argv[2],"same-cb");
  const bool indexed=multiIndexed || (background && strstr(argv[2],"indexed"));
  const bool index32=indexed && strstr(argv[2],"u32");
  const bool indexMutated=indexed && !multiIndexed && strstr(argv[2],"mutated");
  const bool indexLate=indexed && strstr(argv[2],"late");
  const bool formatted=background && strstr(argv[2],"formatted");
  const bool alias=background && !indirect && strstr(argv[2],"alias");
  const bool managed=background && strstr(argv[2],"managed");
  const bool instanceFlags=tlas && strstr(argv[2],"flags");
  const bool instanceOpaque=tlas && strstr(argv[2],"instance-opaque");
  const bool instanceMaskZero=tlas && strstr(argv[2],"mask-zero");
  const bool instanceTable=tlas && strstr(argv[2],"instance-table");
  const bool compactChild=background && strstr(argv[2],"child-compact");
  const bool compact=background && strstr(argv[2],"compact") && !compactChild;
  const bool compactCopy=compact && strstr(argv[2],"copy");
  const bool frameCompact=compact && !tlas && strstr(argv[2],"frame");
  const bool refittable=background && strstr(argv[2],"refittable");
  @autoreleasepool
  {
    RENDERDOC_API_1_7_0 *api = nullptr;
    auto get = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    if(get) get(eRENDERDOC_API_Version_1_7_0, (void **)&api);
    auto d = MTLCreateSystemDefaultDevice();
    auto q = [d newCommandQueue];
    NSError *error = nil;
    NSString *source = @"#include <metal_stdlib>\nusing namespace metal; using namespace metal::raytracing;\n"
      "[[intersection(triangle, triangle_data)]] bool accept_triangle() {return true;}\n"
      "kernel void trace(acceleration_structure<> a [[buffer(0)]], device uint *out [[buffer(1)]], "
      "intersection_function_table<triangle_data> table [[buffer(2)]]) {"
      "intersector<triangle_data> query; query.assume_geometry_type(geometry_type::triangle); "
      "auto hit=query.intersect(ray(float3(0,0,-2),float3(0,0,1)),a,table); "
      "out[0]=hit.type==intersection_type::triangle?1:0;}";
    if(largeVisible)
    {
      source=[source stringByReplacingOccurrencesOfString:@"table [[buffer(2)]]) {"
          withString:@"table [[buffer(2)]], visible_function_table<uint()> visible [[buffer(3)]], constant uint &slot [[buffer(4)]]) {"];
      source=[source stringByReplacingOccurrencesOfString:@"?1:0;}" withString:@"?visible[slot]():0;}"];
      source=[source stringByAppendingString:@" [[visible]] uint ray_value() {return 3;}"];
    }
    if(tlas)
      source=[[source stringByReplacingOccurrencesOfString:@"acceleration_structure<>" withString:@"acceleration_structure<instancing>"]
          stringByReplacingOccurrencesOfString:@"intersector<triangle_data>" withString:@"intersector<triangle_data, instancing>"];
    if(tlas) source=[[source stringByReplacingOccurrencesOfString:@"intersection_function_table<triangle_data>" withString:@"intersection_function_table<triangle_data, instancing>"]
        stringByReplacingOccurrencesOfString:@"intersection(triangle, triangle_data)" withString:@"intersection(triangle, triangle_data, instancing)"];
    if(indirect || userID)
      source=[source stringByReplacingOccurrencesOfString:@"?1:0;}" withString:@"?hit.user_instance_id:0;}"];
    if(alias) source=[source stringByReplacingOccurrencesOfString:@"float3(0,0,-2)" withString:@"float3(0.25,0,-2)"];
    auto lib = [d newLibraryWithSource:source options:nil error:&error];
    if(!lib) {fprintf(stderr,"%s\n",error.description.UTF8String);return 2;}
    if(api && frameBornTables)
    {
      api->SetCaptureFilePathTemplate(argv[1]);
      api->StartFrameCapture(nullptr,nullptr);
    }
    auto pd = [MTLComputePipelineDescriptor new];
    pd.computeFunction = [lib newFunctionWithName:@"trace"];
    auto intersection = [lib newFunctionWithName:@"accept_triangle"];
    auto visibleFunction=largeVisible?[lib newFunctionWithName:@"ray_value"]:nil;
    auto linked = [MTLLinkedFunctions new];
    linked.functions=largeVisible?@[intersection,visibleFunction]:@[intersection]; pd.linkedFunctions = linked;
    auto pso = [d newComputePipelineStateWithDescriptor:pd options:MTLPipelineOptionNone reflection:nil error:&error];
    if(!pso) {fprintf(stderr,"%s\n",error.description.UTF8String); return 2;}
    auto handle = [pso functionHandleWithFunction:intersection];
    id<MTLVisibleFunctionTable> visibleTable=nil;
    const unsigned visibleSlot=largeVisible?visibleCount-1:0;
    if(largeVisible)
    {
      auto desc=[MTLVisibleFunctionTableDescriptor new]; desc.functionCount=visibleCount;
      visibleTable=[pso newVisibleFunctionTableWithDescriptor:desc];
      [desc release]; if(!visibleTable) return 21;
      auto visibleHandle=[pso functionHandleWithFunction:visibleFunction];
      if(!visibleHandle) return 22;
      [visibleTable setFunction:visibleHandle atIndex:0];
      id<MTLFunctionHandle> tail[2]={nil,visibleHandle};
      [visibleTable setFunctions:tail withRange:NSMakeRange(visibleCount-2,2)];
      [visibleTable setFunction:nil atIndex:visibleSlot];
      [visibleTable setFunction:visibleHandle atIndex:visibleSlot];
    }
    auto tableDescriptor = [MTLIntersectionFunctionTableDescriptor new]; tableDescriptor.functionCount = 2;
    NSMutableArray *tables = [NSMutableArray array];
    for(unsigned i=0;i<4;i++)
    {
      auto table = [pso newIntersectionFunctionTableWithDescriptor:tableDescriptor];
      table.label = [NSString stringWithFormat:@"Ray table %u",i];
      [table setFunction:handle atIndex:0]; [table setFunction:handle atIndex:1];
      if(i<2) [table setFunction:nil atIndex:1];
      else {id<MTLFunctionHandle> values[2]={handle,nil}; [table setFunctions:values withRange:NSMakeRange(0,2)];}
      if(i==1) [table setFunction:handle atIndex:1];
      if(i==3) {id<MTLFunctionHandle> values[2]={nil,handle}; [table setFunctions:values withRange:NSMakeRange(0,2)];}
      // A zero-length range is a legal no-op and must not dereference the pointer.
      id<MTLFunctionHandle> empty[1]={nil};
      [table setFunctions:empty withRange:NSMakeRange(2,0)];
      [tables addObject:table];
    }
    id<MTLVisibleFunctionTable> unusedVisible=nil;
    id<MTLIntersectionFunctionTable> unusedIntersection=nil;
    if(tableIdentity && strstr(argv[2],"unused"))
    {
      auto desc=[MTLVisibleFunctionTableDescriptor new]; desc.functionCount=visibleCount;
      unusedVisible=[pso newVisibleFunctionTableWithDescriptor:desc]; [desc release];
      unusedIntersection=[pso newIntersectionFunctionTableWithDescriptor:tableDescriptor];
      if(!unusedVisible || !unusedIntersection) return 30;
    }
    auto checkTableIdentities = [&]() {
      if(!tableIdentity) return true;
      if(@available(macOS 13.0, *))
      {
        std::set<uint64_t> visibleValues, intersectionValues;
        for(id<MTLVisibleFunctionTable> table in (unusedVisible?@[visibleTable,unusedVisible]:@[visibleTable]))
        {
          const auto value=table.gpuResourceID._impl;
          if(!value || table.gpuResourceID._impl!=value || !visibleValues.insert(value).second) return false;
          if(api)
          {
            if(![(id)table respondsToSelector:@selector(real)]) return false;
            auto native=[(id<NativeVisibleFunctionTableProbe>)table real];
            if(!native || native.gpuResourceID._impl!=value) return false;
          }
          printf("PASS exact stable visible table GPU identity %llu\n",(unsigned long long)value);
        }
        NSMutableArray *queried=[NSMutableArray arrayWithArray:tables];
        if(unusedIntersection) [queried addObject:unusedIntersection];
        for(id<MTLIntersectionFunctionTable> table in queried)
        {
          const auto value=table.gpuResourceID._impl;
          if(!value || table.gpuResourceID._impl!=value || !intersectionValues.insert(value).second) return false;
          if(api)
          {
            if(![(id)table respondsToSelector:@selector(real)]) return false;
            auto native=[(id<NativeIntersectionFunctionTableProbe>)table real];
            if(!native || native.gpuResourceID._impl!=value) return false;
          }
          printf("PASS exact stable intersection table GPU identity %llu\n",(unsigned long long)value);
        }
        return true;
      }
      return false;
    };
    float triangle[9]={-1,-1,0, 1,-1,0, 0,1,0};
    if(manyChildren) for(unsigned i=0;i<3;i++) triangle[i*3]+=100;
    float zeroTriangle[9]={};
    float indexedTriangle[18]={-1,-1,0,1,-1,0,0,1,0,99,-1,0,101,-1,0,100,1,0};
    float formattedTriangle[19]={42,42,42,42,-1,-1,0,1,99,1,-1,0,1,99,0,1,0,1,99};
    auto vertices = [d newBufferWithBytes:formatted?formattedTriangle:(indexed?indexedTriangle:(fences?zeroTriangle:triangle))
      length:formatted?sizeof(formattedTriangle):(indexed?sizeof(indexedTriangle):sizeof(triangle))
      options:managed?MTLResourceStorageModeManaged:
          fences?(MTLResourceStorageModeShared | MTLResourceHazardTrackingModeUntracked):MTLResourceStorageModeShared];
    if(managed) [vertices didModifyRange:NSMakeRange(0,vertices.length)];
    id<MTLBuffer> aliasOutput=nil;
    if(alias)
    {
      void *memory=nullptr; const size_t page=getpagesize();
      if(posix_memalign(&memory,page,page)) return 9;
      memset(memory,0,page); memcpy(memory,triangle,sizeof(triangle));
      vertices=[d newBufferWithBytesNoCopy:memory length:page options:MTLResourceStorageModeShared deallocator:nil];
      aliasOutput=[d newBufferWithBytesNoCopy:memory length:page options:MTLResourceStorageModeShared deallocator:nil];
      if(!vertices || !aliasOutput || vertices.contents!=memory || aliasOutput.contents!=memory) return 10;
      aliasOutput.label=@"Ray AS aliased compacted-size output";
    }
    vertices.label=@"Ray triangle vertices";
    auto triangleSource=fences?[d newBufferWithBytes:triangle length:sizeof(triangle) options:MTLResourceStorageModeShared]:nil;
    auto uploaded=fences?[d newFence]:nil;
    auto built=fences?[d newFence]:nil;
    unsigned initial[4]={7,7,7,7};
    auto output = [d newBufferWithBytes:initial length:sizeof(initial) options:MTLResourceStorageModeShared];
    output.label = @"Ray table clear/rebind results";
    auto geometry = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    geometry.vertexBuffer=vertices; geometry.vertexStride=12; geometry.triangleCount=1;
    id<MTLBuffer> indexBuffer=nil;
    id<MTLBuffer> pendingVertices=nil, pendingIndices=nil;
    if(indexed)
    {
      unsigned short index16[5]={65535,0,1,2,65535};
      unsigned index32Values[5]={0xffffffff,0,1,2,0xffffffff};
      indexBuffer=[d newBufferWithBytes:index32?(void *)index32Values:(void *)index16
          length:index32?sizeof(index32Values):sizeof(index16)
          options:managed?MTLResourceStorageModeManaged:MTLResourceStorageModeShared];
      if(managed) [indexBuffer didModifyRange:NSMakeRange(0,indexBuffer.length)];
      indexBuffer.label=@"Ray initial indices";
      geometry.indexBuffer=indexBuffer; geometry.indexType=index32?MTLIndexTypeUInt32:MTLIndexTypeUInt16;
      geometry.indexBufferOffset=index32?4:2;
    }
    if(multiIndexed)
    {
      unsigned short values16[8]={65535,3,4,5,0,1,2,65535};
      unsigned values32[8]={0xffffffff,3,4,5,0,1,2,0xffffffff};
      indexBuffer=[d newBufferWithBytes:index32?(void *)values32:(void *)values16
          length:index32?sizeof(values32):sizeof(values16) options:MTLResourceStorageModeShared];
      indexBuffer.label=@"Ray initial indices";
      geometry.indexBuffer=indexBuffer;
      if(privateInputs || sameCBGeometry)
      {
        const MTLResourceOptions options=(privateInputs?MTLResourceStorageModePrivate:MTLResourceStorageModeShared) |
            (sameCBGeometry?MTLResourceHazardTrackingModeTracked:MTLResourceHazardTrackingModeDefault);
        auto privateVertices=[d newBufferWithLength:vertices.length options:options];
        auto privateIndices=[d newBufferWithLength:indexBuffer.length options:options];
        if(sameCBGeometry) {pendingVertices=vertices; pendingIndices=indexBuffer;}
        else
        {
          auto upload=[q commandBuffer]; auto blit=[upload blitCommandEncoder];
          [blit copyFromBuffer:vertices sourceOffset:0 toBuffer:privateVertices destinationOffset:0 size:vertices.length];
          [blit copyFromBuffer:indexBuffer sourceOffset:0 toBuffer:privateIndices destinationOffset:0 size:indexBuffer.length];
          [blit endEncoding]; [upload commit]; [upload waitUntilCompleted];
          if(upload.error) return 12;
        }
        vertices=privateVertices; indexBuffer=privateIndices;
        vertices.label=@"Ray triangle vertices"; indexBuffer.label=@"Ray initial indices";
        geometry.vertexBuffer=vertices; geometry.indexBuffer=indexBuffer;
      }
    }
    geometry.opaque=NO; geometry.intersectionFunctionTableOffset=1;
    if(instanceTable) geometry.intersectionFunctionTableOffset=0;
    if(formatted)
    {
      geometry.vertexFormat=MTLAttributeFormatFloat4; geometry.vertexStride=20; geometry.vertexBufferOffset=16;
      geometry.allowDuplicateIntersectionFunctionInvocation=NO;
    }
    auto descriptor = [MTLPrimitiveAccelerationStructureDescriptor descriptor]; descriptor.geometryDescriptors=@[geometry];
    if(multiIndexed)
    {
      MTLAccelerationStructureTriangleGeometryDescriptor *secondGeometry=[geometry copy];
      secondGeometry.indexBufferOffset=index32?16:8;
      secondGeometry.allowDuplicateIntersectionFunctionInvocation=NO;
      descriptor.geometryDescriptors=@[geometry,secondGeometry];
      [secondGeometry release];
    }
    if(refittable) descriptor.usage=MTLAccelerationStructureUsageRefit;
    auto sizes = [d accelerationStructureSizesWithDescriptor:descriptor];
    if(api && frameHeap)
    {
      api->SetCaptureFilePathTemplate(argv[1]);
      api->StartFrameCapture(nullptr,nullptr);
    }
    id<MTLAccelerationStructure> structure=nil;
    id<MTLBuffer> rebuildUpload=nil, rebuildScratch=nil;
    MTLInstanceAccelerationStructureDescriptor *rebuildDescriptor=nil;
    id<MTLHeap> accelerationHeap=nil;
    if(heapAS)
    {
      auto layout=[d heapAccelerationStructureSizeAndAlignWithSize:sizes.accelerationStructureSize];
      auto hd=[MTLHeapDescriptor new];
      hd.type=automaticHeap?MTLHeapTypeAutomatic:MTLHeapTypePlacement;
      hd.storageMode=MTLStorageModePrivate;
      hd.hazardTrackingMode=MTLHazardTrackingModeTracked;
      hd.size=(2*layout.size+layout.align+4095)&~NSUInteger(4095);
      accelerationHeap=[d newHeapWithDescriptor:hd];
      structure=automaticHeap?[accelerationHeap newAccelerationStructureWithSize:sizes.accelerationStructureSize]:
          [accelerationHeap newAccelerationStructureWithSize:sizes.accelerationStructureSize offset:layout.align];
      if(!structure || structure.heap!=accelerationHeap ||
         (!automaticHeap && structure.heapOffset!=layout.align)) return 15;
    }
    else structure=[d newAccelerationStructureWithSize:sizes.accelerationStructureSize];
    structure.label=@"Ray query structure";
    auto second=(repeated || unusedBuiltInstance)?[d newAccelerationStructureWithSize:sizes.accelerationStructureSize]:nil;
    auto manyStructures=[NSMutableArray new];
    if(manyChildren) [manyStructures addObject:structure];
    auto scratch = [d newBufferWithLength:sizes.buildScratchBufferSize options:MTLResourceStorageModePrivate];
    auto compacted = [d newBufferWithLength:8 options:MTLResourceStorageModeShared];
    compacted.label = @"Ray compacted size";
    memset(compacted.contents,0,8);
    if(api) {api->SetCaptureFilePathTemplate(argv[1]); if(!background && !frameHeap && !frameBornTables) api->StartFrameCapture(nullptr,nullptr);}
    if(!checkTableIdentities()) return 31;
    auto checkIdentity = [&](id<MTLAccelerationStructure> object) {
      if(!identity) return true;
      if(@available(macOS 13.0, *))
      {
        const auto value=object.gpuResourceID._impl;
        if(!value || object.gpuResourceID._impl!=value) return false;
        if(api)
        {
          if(![(id)object respondsToSelector:@selector(real)]) return false;
          auto native=[(id<NativeAccelerationStructureProbe>)object real];
          if(!native || native.gpuResourceID._impl!=value) return false;
        }
        printf("PASS exact stable AS GPU identity %llu\n",(unsigned long long)value);
        return true;
      }
      return false;
    };
    if(!checkIdentity(structure)) return 16;
    // A queried but unbuilt AS has allocation/identity metadata, without an AS
    // build recipe. It must remain a capture dependency even without a binding.
    id<MTLAccelerationStructure> unusedIdentity=nil;
    if(identity && strstr(argv[2],"unused"))
    {
      unusedIdentity=[d newAccelerationStructureWithSize:sizes.accelerationStructureSize];
      unusedIdentity.label=@"Ray queried unbuilt structure";
      if(!checkIdentity(unusedIdentity)) return 18;
    }
    auto layer=[CAMetalLayer layer]; layer.device=d; layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.drawableSize=CGSizeMake(2,2); layer.framebufferOnly=NO;
    auto drawable=[layer nextDrawable]; if(!drawable) return 3;
    auto cb=[q commandBuffer];
    if(sameCBGeometry)
    {
      auto blit=[cb blitCommandEncoder];
      [blit copyFromBuffer:pendingVertices sourceOffset:0 toBuffer:vertices destinationOffset:0 size:vertices.length];
      [blit copyFromBuffer:pendingIndices sourceOffset:0 toBuffer:indexBuffer destinationOffset:0 size:indexBuffer.length];
      [blit endEncoding];
    }
    if(fences)
    {
      auto blit=[cb blitCommandEncoder];
      [blit copyFromBuffer:triangleSource sourceOffset:0 toBuffer:vertices destinationOffset:0 size:sizeof(triangle)];
      [blit updateFence:uploaded]; [blit endEncoding];
    }
    auto as=[cb accelerationStructureCommandEncoder];
    if(fences) [as waitForFence:uploaded];
    [as pushDebugGroup:@"Ray AS outer"];
    [as insertDebugSignpost:@"Before BLAS"];
    [as pushDebugGroup:@"Ray AS build"];
    [as buildAccelerationStructure:structure descriptor:descriptor scratchBuffer:scratch scratchBufferOffset:0];
    if(second) [as buildAccelerationStructure:second descriptor:descriptor scratchBuffer:scratch scratchBufferOffset:0];
    for(unsigned i=1;i<manyChildren;i++)
    {
      auto extra=[d newAccelerationStructureWithSize:sizes.accelerationStructureSize];
      extra.label=[NSString stringWithFormat:@"Ray many initial BLAS %u",i];
      [manyStructures addObject:extra];
      float data[9]={-1,-1,0, 1,-1,0, 0,1,0};
      if(i+1<manyChildren) for(unsigned vertex=0;vertex<3;vertex++) data[vertex*3]+=100+i;
      MTLAccelerationStructureTriangleGeometryDescriptor *extraGeometry=[geometry copy];
      extraGeometry.vertexBuffer=[d newBufferWithBytes:data length:sizeof(data) options:MTLResourceStorageModeShared];
      auto extraDescriptor=[MTLPrimitiveAccelerationStructureDescriptor descriptor];
      extraDescriptor.geometryDescriptors=@[extraGeometry];
      [as buildAccelerationStructure:extra descriptor:extraDescriptor scratchBuffer:scratch scratchBufferOffset:0];
    }
    [as popDebugGroup];
    [as pushDebugGroup:@"Ray AS size"];
    [as writeCompactedAccelerationStructureSize:structure toBuffer:compacted offset:0 sizeDataType:MTLDataTypeULong];
    if(alias)
    {
      [as writeCompactedAccelerationStructureSize:structure toBuffer:aliasOutput offset:0 sizeDataType:MTLDataTypeULong];
      [as buildAccelerationStructure:structure descriptor:descriptor scratchBuffer:scratch scratchBufferOffset:0];
    }
    [as insertDebugSignpost:@"After size write"];
    [as popDebugGroup]; [as popDebugGroup];
    if(fences) [as updateFence:built];
    [as endEncoding];
    if(sameCBGeometry && strstr(argv[2],"gpu-mutated"))
    {
      auto blit=[cb blitCommandEncoder];
      [blit fillBuffer:vertices range:NSMakeRange(0,vertices.length) value:0];
      [blit fillBuffer:indexBuffer range:NSMakeRange(0,indexBuffer.length) value:0];
      [blit endEncoding];
    }
    if(late) for(unsigned i=0;i<3;i++) ((float *)vertices.contents)[i*3]+=100;
    if(indexLate)
    {
      for(unsigned i=0;i<3;i++)
      {
        if(index32) ((unsigned *)indexBuffer.contents)[i+1]+=3;
        else ((unsigned short *)indexBuffer.contents)[i+1]+=3;
      }
    }
    [cb commit]; [cb waitUntilCompleted];
    if(cb.error) {fprintf(stderr,"%s\n",cb.error.description.UTF8String);return 4;}
    id<MTLAccelerationStructure> compactOriginal=nil;
    auto compactStructure = [&](id<MTLAccelerationStructure> original) {
      // Complete the size query before allocating and encoding compact copy.
      // Existing capture support ties the query to the build submission. The
      // original BLAS already has that query; TLAS gets it during its build below.
      auto result=[d newAccelerationStructureWithSize:*(unsigned long long *)compacted.contents];
      auto command=[q commandBuffer]; auto encoder=[command accelerationStructureCommandEncoder];
      [encoder copyAndCompactAccelerationStructure:original toAccelerationStructure:result];
      [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
      if(command.error) return (id<MTLAccelerationStructure>)nil;
      return result;
    };
    if(compactChild) {structure=compactStructure(structure); if(!structure) return 11;}
    if(tlas)
    {
      auto child=structure;
      child.label=@"Ray initial BLAS";
      if(second) second.label=@"Ray second initial BLAS";
      const unsigned count=emptyIndirect?0:manyChildren?manyChildren:repeated?3:1;
      std::vector<MTLAccelerationStructureInstanceDescriptor> instances(count);
      for(unsigned i=0;i<count;i++)
      {
        instances[i].transformationMatrix.columns[0].x=1;
        instances[i].transformationMatrix.columns[1].y=1;
        instances[i].transformationMatrix.columns[2].z=1;
        instances[i].transformationMatrix.columns[3].x=!manyChildren && i?100:0;
        instances[i].mask=0xff;
        if(instanceFlags)
        {
          instances[i].options=MTLAccelerationStructureInstanceOptionDisableTriangleCulling |
              MTLAccelerationStructureInstanceOptionTriangleFrontFacingWindingCounterClockwise |
              MTLAccelerationStructureInstanceOptionNonOpaque;
          instances[i].mask=1;
        }
        if(instanceOpaque) instances[i].options=MTLAccelerationStructureInstanceOptionOpaque;
        if(instanceMaskZero) instances[i].mask=0;
        if(instanceTable) instances[i].intersectionFunctionTableOffset=1;
        instances[i].accelerationStructureIndex=repeated?(i%2):0;
      }
      auto instanceBuffer=emptyIndirect?[d newBufferWithLength:64 options:MTLResourceStorageModeShared]:
          [d newBufferWithBytes:instances.data() length:count*sizeof(instances[0])
          options:managed?MTLResourceStorageModeManaged:MTLResourceStorageModeShared];
      if(managed) [instanceBuffer didModifyRange:NSMakeRange(0,instanceBuffer.length)];
      instanceBuffer.label=@"Ray initial instances";
      auto td=[MTLInstanceAccelerationStructureDescriptor descriptor];
      td.instanceDescriptorBuffer=instanceBuffer; td.instanceCount=count;
      td.instancedAccelerationStructures=second?@[child,second]:@[child];
      if(userID)
      {
        const NSUInteger offset=strstr(argv[2],"offset")?64:0;
        const NSUInteger stride=strstr(argv[2],"padded")?80:sizeof(MTLAccelerationStructureUserIDInstanceDescriptor);
        const NSUInteger length=offset+count*stride;
        instanceBuffer=[d newBufferWithLength:length
            options:managed?MTLResourceStorageModeManaged:MTLResourceStorageModeShared];
        memset(instanceBuffer.contents,0x5a,length);
        for(unsigned i=0;i<count;i++)
        {
          MTLAccelerationStructureUserIDInstanceDescriptor data={};
          memcpy(&data,&instances[i],sizeof(instances[i])); data.userID=instanceUserID+i;
          memcpy((char *)instanceBuffer.contents+offset+i*stride,&data,sizeof(data));
        }
        if(managed) [instanceBuffer didModifyRange:NSMakeRange(0,length)];
        instanceBuffer.label=@"Ray initial instances";
        td.instanceDescriptorBuffer=instanceBuffer;
        td.instanceDescriptorBufferOffset=offset; td.instanceDescriptorStride=stride;
        td.instanceDescriptorType=MTLAccelerationStructureInstanceDescriptorTypeUserID;
      }
      id<MTLBuffer> indirectUpload=nil;
      if(indirect)
      {
        if(@available(macOS 14.0, *))
        {
          if(unusedBuiltInstance && !second.gpuResourceID._impl) return 26;
          std::vector<MTLIndirectAccelerationStructureInstanceDescriptor> data(count);
          for(unsigned i=0;i<count;i++)
          {
            data[i].transformationMatrix=instances[i].transformationMatrix;
            data[i].options=instances[i].options;
            data[i].mask=inactiveIndirect?0:instances[i].mask;
            data[i].intersectionFunctionTableOffset=instances[i].intersectionFunctionTableOffset;
            data[i].userID=instanceUserID+i;
            id<MTLAccelerationStructure> instanceChild=manyChildren?manyStructures[count-1-i]:second && i%2?second:child;
            data[i].accelerationStructureID=inactiveIndirect?MTLResourceID{0}:instanceChild.gpuResourceID;
          }
          const NSUInteger offset=emptyIndirect && !strstr(argv[2],"offset")?0:64;
          const NSUInteger stride=strstr(argv[2],"padded")?80:sizeof(data[0]);
          const NSUInteger length=offset+(emptyIndirect?stride:count*stride);
          indirectUpload=[d newBufferWithLength:length options:MTLResourceStorageModeShared];
          memset(indirectUpload.contents,0,length);
          for(unsigned i=0;i<count;i++) memcpy((char *)indirectUpload.contents+offset+i*stride,&data[i],sizeof(data[0]));
          if(placementInstances)
          {
            auto heapDescriptor=[MTLHeapDescriptor new];
            heapDescriptor.type=MTLHeapTypePlacement;
            heapDescriptor.storageMode=MTLStorageModePrivate;
            heapDescriptor.hazardTrackingMode=MTLHazardTrackingModeTracked;
            auto allocation=[d heapBufferSizeAndAlignWithLength:length
                options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked];
            heapDescriptor.size=(allocation.size+allocation.align+4095)&~NSUInteger(4095);
            auto inputHeap=[d newHeapWithDescriptor:heapDescriptor];
            instanceBuffer=[inputHeap newBufferWithLength:length
                options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:allocation.align];
            if(!instanceBuffer || !instanceBuffer.isAliasable) return 27;
          }
          else instanceBuffer=privateInstances?[d newBufferWithLength:length
              options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked]:indirectUpload;
          instanceBuffer.label=@"Ray indirect instances";
          if(privateInstances && !sameCBInstances)
          {
            auto upload=[q commandBuffer]; auto blit=[upload blitCommandEncoder];
            [blit copyFromBuffer:indirectUpload sourceOffset:0 toBuffer:instanceBuffer destinationOffset:0 size:length];
            [blit endEncoding]; [upload commit]; [upload waitUntilCompleted];
            if(upload.error) return 23;
          }
          td.instanceDescriptorBuffer=instanceBuffer;
          td.instanceDescriptorBufferOffset=offset;
          td.instanceDescriptorStride=stride;
          td.instanceDescriptorType=MTLAccelerationStructureInstanceDescriptorTypeIndirect;
          td.instancedAccelerationStructures=nil;
        }
        else return 24;
      }
      auto ts=[d accelerationStructureSizesWithDescriptor:td];
      if(emptyIndirect)
      {
        auto heap=[d heapAccelerationStructureSizeAndAlignWithDescriptor:td];
        if(!ts.accelerationStructureSize || !ts.buildScratchBufferSize || !heap.size || !heap.align ||
           td.instanceCount || td.instanceDescriptorBuffer!=instanceBuffer || td.instancedAccelerationStructures.count)
          return 30;
        printf("empty TLAS query size=%lu scratch=%lu refit=%lu heap=%lu align=%lu\n",
            (unsigned long)ts.accelerationStructureSize, (unsigned long)ts.buildScratchBufferSize,
            (unsigned long)ts.refitScratchBufferSize, (unsigned long)heap.size, (unsigned long)heap.align);
      }
      structure=[d newAccelerationStructureWithSize:ts.accelerationStructureSize];
      structure.label=@"Ray query structure";
      auto temporary=[d newBufferWithLength:ts.buildScratchBufferSize options:MTLResourceStorageModePrivate];
      if(frameRebuild) {rebuildUpload=indirectUpload; rebuildScratch=temporary; rebuildDescriptor=td;}
      if(api && frameTLAS) api->StartFrameCapture(nullptr,nullptr);
      auto command=[q commandBuffer];
      if(sameCBInstances)
      {
        auto blit=[command blitCommandEncoder];
        [blit copyFromBuffer:indirectUpload sourceOffset:0 toBuffer:instanceBuffer destinationOffset:0 size:instanceBuffer.length];
        [blit endEncoding];
      }
      auto encoder=[command accelerationStructureCommandEncoder];
      [encoder buildAccelerationStructure:structure descriptor:td scratchBuffer:temporary scratchBufferOffset:0];
      if(compact) [encoder writeCompactedAccelerationStructureSize:structure toBuffer:compacted offset:0 sizeDataType:MTLDataTypeULong];
      [encoder endEncoding];
      if(sameCBInstances && strstr(argv[2],"gpu-mutated"))
      {
        id<MTLBuffer> destination=instanceBuffer;
        if(placementInstances && strstr(argv[2],"alias"))
          destination=[instanceBuffer.heap newBufferWithLength:instanceBuffer.length
              options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:instanceBuffer.heapOffset];
        if(!destination) return 28;
        auto blit=[command blitCommandEncoder];
        [blit fillBuffer:destination range:NSMakeRange(0,destination.length) value:0];
        [blit endEncoding];
      }
      if(tlasLate) ((MTLAccelerationStructureInstanceDescriptor *)instanceBuffer.contents)[0].transformationMatrix.columns[3].x=100;
      [command commit]; [command waitUntilCompleted];
      if(command.error) return 7;
      if(indirect && strstr(argv[2],"source-erased"))
        memset(instanceBuffer.contents,0,instanceBuffer.length);
      if(indirect && !sameCBInstances && strstr(argv[2],"gpu-mutated"))
      {
        // The built AS must preserve its instance userID and address after a
        // later GPU write erases the source descriptors.
        id<MTLBuffer> destination=instanceBuffer;
        if(placementInstances && strstr(argv[2],"alias"))
          destination=[instanceBuffer.heap newBufferWithLength:instanceBuffer.length
              options:MTLResourceStorageModePrivate|MTLResourceHazardTrackingModeTracked offset:instanceBuffer.heapOffset];
        if(!destination) return 28;
        auto overwrite=[q commandBuffer]; auto blit=[overwrite blitCommandEncoder];
        [blit fillBuffer:destination range:NSMakeRange(0,destination.length) value:0];
        [blit endEncoding]; [overwrite commit]; [overwrite waitUntilCompleted];
        if(overwrite.error) return 25;
      }
      if(tlasMutated)
      {
        auto data=(MTLAccelerationStructureInstanceDescriptor *)((char *)instanceBuffer.contents+td.instanceDescriptorBufferOffset);
        data->transformationMatrix.columns[3].x=100;
        if(userID) ((MTLAccelerationStructureUserIDInstanceDescriptor *)data)->userID=0;
        for(unsigned i=0;i<3;i++) ((float *)vertices.contents)[i*3]+=100;
      }
      if(stale)
      {
        command=[q commandBuffer]; encoder=[command accelerationStructureCommandEncoder];
        [encoder buildAccelerationStructure:child descriptor:descriptor scratchBuffer:scratch scratchBufferOffset:0];
        [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
        if(command.error) return 8;
      }
    }
    if(compact)
    {
      if(api && frameCompact) api->StartFrameCapture(nullptr,nullptr);
      compactOriginal=structure; structure=compactStructure(structure); if(!structure) return 11;
      if(compactCopy)
      {
        auto destination=[d newAccelerationStructureWithSize:structure.size];
        auto command=[q commandBuffer]; auto encoder=[command accelerationStructureCommandEncoder];
        [encoder copyAccelerationStructure:structure toAccelerationStructure:destination];
        [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
        if(command.error) return 12; structure=destination;
      }
    }
    if(mutated) for(unsigned i=0;i<3;i++) ((float *)vertices.contents)[i*3]+=100;
    if(compact && mutated)
    {
      auto command=[q commandBuffer]; auto encoder=[command accelerationStructureCommandEncoder];
      [encoder buildAccelerationStructure:compactOriginal descriptor:descriptor scratchBuffer:scratch scratchBufferOffset:0];
      [encoder endEncoding]; [command commit]; [command waitUntilCompleted]; if(command.error) return 13;
    }
    if(indexMutated)
    {
      for(unsigned i=0;i<3;i++)
      {
        if(index32) ((unsigned *)indexBuffer.contents)[i+1]+=3;
        else ((unsigned short *)indexBuffer.contents)[i+1]+=3;
      }
    }
    if((managed || multiIndexed) && !sameCBGeometry && argc==3 && strstr(argv[2],"gpu-mutated"))
    {
      // Later GPU writes must not replace the bytes consumed by the completed AS build.
      auto zero=[d newBufferWithLength:vertices.length options:MTLResourceStorageModeShared];
      memset(zero.contents,0,zero.length);
      auto command=[q commandBuffer]; auto blit=[command blitCommandEncoder];
      [blit copyFromBuffer:zero sourceOffset:0 toBuffer:vertices destinationOffset:0 size:vertices.length];
      if(indexBuffer)
        [blit copyFromBuffer:zero sourceOffset:0 toBuffer:indexBuffer destinationOffset:0 size:indexBuffer.length];
      [blit endEncoding]; [command commit]; [command waitUntilCompleted];
      if(command.error) return 14;
    }
    if(api && background && !frameTLAS && !frameCompact) api->StartFrameCapture(nullptr,nullptr);
    if(!checkTableIdentities()) return 32;
    if(!checkIdentity(structure)) return 17;
    if(unusedIdentity && !checkIdentity(unusedIdentity)) return 19;
    cb=[q commandBuffer];
    for(unsigned i=0;i<4;i++)
    {
      if(frameRebuild && i==2)
      {
        for(NSUInteger slot=0;slot<rebuildDescriptor.instanceCount;slot++)
        {
          auto data=(MTLIndirectAccelerationStructureInstanceDescriptor *)((char *)rebuildUpload.contents+
              rebuildDescriptor.instanceDescriptorBufferOffset+slot*rebuildDescriptor.instanceDescriptorStride);
          data->userID++;
        }
        auto blit=[cb blitCommandEncoder];
        [blit copyFromBuffer:rebuildUpload sourceOffset:0 toBuffer:rebuildDescriptor.instanceDescriptorBuffer
            destinationOffset:0 size:rebuildUpload.length];
        [blit endEncoding];
        auto encoder=[cb accelerationStructureCommandEncoder];
        [encoder buildAccelerationStructure:structure descriptor:rebuildDescriptor scratchBuffer:rebuildScratch scratchBufferOffset:0];
        [encoder endEncoding];
      }
      auto cs=[cb computeCommandEncoder];
      if(fences && !background) [cs waitForFence:built];
      [cs setComputePipelineState:pso];
      [cs setAccelerationStructure:structure atBufferIndex:0];
      if(emptyIndirect || inactiveIndirect) [cs useResource:vertices usage:MTLResourceUsageRead];
      if(manyChildren)
      {
        std::vector<id<MTLResource>> resources;
        for(id<MTLAccelerationStructure> extra in manyStructures) resources.push_back(extra);
        [cs useResources:resources.data() count:resources.size() usage:MTLResourceUsageRead];
      }
      [cs setBuffer:output offset:i*4 atIndex:1];
      if(largeVisible)
      {
        [cs setVisibleFunctionTable:nil atBufferIndex:3];
        [cs setVisibleFunctionTable:visibleTable atBufferIndex:3];
        [cs setBytes:&visibleSlot length:sizeof(visibleSlot) atIndex:4];
      }
      if(arrayTables)
      {
        id<MTLIntersectionFunctionTable> empty[1]={nil}, bound[1]={tables[i]};
        [cs setIntersectionFunctionTables:empty withBufferRange:NSMakeRange(2,1)];
        [cs setIntersectionFunctionTables:bound withBufferRange:NSMakeRange(2,1)];
      }
      else [cs setIntersectionFunctionTable:tables[i] atBufferIndex:2];
      [cs dispatchThreadgroups:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)]; [cs endEncoding];
    }
    [cb presentDrawable:drawable]; [cb commit]; [cb waitUntilCompleted];
    if(cb.error) {fprintf(stderr,"%s\n",cb.error.description.UTF8String);return 4;}
    unsigned *values=(unsigned *)output.contents;
    const unsigned hitValue=(userID || indirect)?instanceUserID:largeVisible?3:1;
    const unsigned unbound=instanceOpaque?hitValue:0;
    const unsigned bound=(inactiveIndirect||emptyIndirect||late||tlasLate||indexLate||instanceMaskZero)?0:hitValue;
    if(values[0]!=unbound || values[1]!=bound || values[2]!=unbound || values[3]!=(frameRebuild?bound+1:bound)) {
      fprintf(stderr,"Unexpected rays %u/%u/%u/%u\n",values[0],values[1],values[2],values[3]);return 5;}
    if(api && !api->EndFrameCapture(nullptr,nullptr)) return 6;
    printf("PASS native clear/rebind rays %u/%u/%u/%u, compacted size %llu\n",
        values[0],values[1],values[2],values[3],*(unsigned long long *)compacted.contents);
  }
}
