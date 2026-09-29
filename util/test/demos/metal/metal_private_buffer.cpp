// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Private_Buffer, MetalGraphicsTest)
{
  static constexpr const char *Description = "GPU-private buffer writes, staged readback and event rewind.";
  int main()
  {
    if(!Init()) return 3;
    const bool computeArgumentTableRange =
        !GetEnvVar("RENDERDOC_METAL_T127_COMPUTE_ARGUMENT_TABLE_RANGE").empty();
    const bool computeArgumentTable = computeArgumentTableRange ||
        !GetEnvVar("RENDERDOC_METAL_T126_COMPUTE_ARGUMENT_TABLE").empty();
    const bool computeTableRange =
        !GetEnvVar("RENDERDOC_METAL_T125_COMPUTE_VISIBLE_TABLE_RANGE").empty();
    const bool computeTable = computeArgumentTable || computeTableRange ||
        !GetEnvVar("RENDERDOC_METAL_T124_COMPUTE_VISIBLE_TABLE").empty();
    const bool visibleTableRange =
        !GetEnvVar("RENDERDOC_METAL_T121_VISIBLE_TABLE_RANGE").empty();
    const bool vertexTableRange =
        !GetEnvVar("RENDERDOC_METAL_T123_VERTEX_VISIBLE_TABLE_RANGE").empty();
    const bool vertexTable = vertexTableRange ||
        !GetEnvVar("RENDERDOC_METAL_T122_VERTEX_VISIBLE_TABLE").empty();
    const bool visibleTable = vertexTable || visibleTableRange ||
        !GetEnvVar("RENDERDOC_METAL_T120_VISIBLE_TABLE").empty();
    const char *source = computeArgumentTable ? R"(
#include <metal_stdlib>
using namespace metal;
[[visible]] uchar probe_byte(uint i) { return uchar(i * 7 + 3); }
struct Args { visible_function_table<uchar(uint)> table [[id(0)]]; };
kernel void cs_main(device uchar *bytes [[buffer(0)]],
                    constant Args &args [[buffer(1)]],
                    uint i [[thread_position_in_grid]])
{
  if(i < 516) bytes[i] = args.table[0](i);
}
vertex float4 vs_main(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_main(const device uchar *bytes [[buffer(0)]])
{
  return float4(bytes[0], bytes[256], bytes[515], 255) / 255.0;
}
)" : computeTable ? R"(
#include <metal_stdlib>
using namespace metal;
[[visible]] uchar probe_byte(uint i) { return uchar(i * 7 + 3); }
kernel void cs_main(device uchar *bytes [[buffer(0)]],
                    visible_function_table<uchar(uint)> table [[buffer(1)]],
                    uint i [[thread_position_in_grid]])
{
  if(i < 516) bytes[i] = table[0](i);
}
vertex float4 vs_main(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_main(const device uchar *bytes [[buffer(0)]])
{
  return float4(bytes[0], bytes[256], bytes[515], 255) / 255.0;
}
)" : vertexTable ? R"(
#include <metal_stdlib>
using namespace metal;
kernel void cs_main(device uchar *bytes [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  if(i < 516) bytes[i] = uchar(i * 7 + 3);
}
struct VertexOut { float4 position [[position]]; float4 color; };
[[visible]] float4 probe_green(float3 c) { return float4(c, 1.0); }
vertex VertexOut vs_main(uint id [[vertex_id]],
                         visible_function_table<float4(float3)> table [[buffer(0)]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  VertexOut out;
  out.position = float4(p[id],0,1);
  out.color = table[0](float3(0,1,0));
  return out;
}
fragment float4 fs_main(VertexOut in [[stage_in]]) { return in.color; }
)" : visibleTable ? R"(
#include <metal_stdlib>
using namespace metal;
kernel void cs_main(device uchar *bytes [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  if(i < 516) bytes[i] = uchar(i * 7 + 3);
}
vertex float4 vs_main(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
[[visible]] float4 probe_red(float3 c) { return float4(c, 1.0); }
fragment float4 fs_main(visible_function_table<float4(float3)> table [[buffer(0)]])
{
  return table[0](float3(1,0,0));
}
)" : R"(
#include <metal_stdlib>
using namespace metal;
kernel void cs_main(device uchar *bytes [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  if(i < 516) bytes[i] = uchar(i * 7 + 3);
}
vertex float4 vs_main(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_main(const device uchar *bytes [[buffer(0)]])
{
  return float4(bytes[0], bytes[256], bytes[515], 255) / 255.0;
}
[[visible]] float probe_visible(float value) { return value + 1.0; }
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!lib) return 4;
    MTL::Function *cs = lib->newFunction(MTLSTR("cs_main"));
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_main"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_main"));
    const bool visibleLink = visibleTable ||
        !GetEnvVar("RENDERDOC_METAL_T119_VISIBLE_LINK").empty();
    MTL::Function *visible = (visibleLink || computeTable) ?
        lib->newFunction(computeTable ? MTLSTR("probe_byte") :
                         vertexTable ? MTLSTR("probe_green") :
                         visibleTable ? MTLSTR("probe_red") : MTLSTR("probe_visible")) : NULL;
    MTL::ComputePipelineState *cp = NULL;
    if(computeTable)
    {
      MTL::ComputePipelineDescriptor *cd = MTL::ComputePipelineDescriptor::alloc()->init();
      cd->setComputeFunction(cs);
      MTL::LinkedFunctions *links = MTL::LinkedFunctions::alloc()->init();
      const NS::Object *functions[] = {visible};
      links->setFunctions(NS::Array::array(functions, 1));
      cd->setLinkedFunctions(links);
      cp = device->newComputePipelineState(cd, MTL::PipelineOptionArgumentInfo, NULL, &error);
      links->release();
      cd->release();
    }
    else cp = device->newComputePipelineState(cs, &error);
    MTL::FunctionHandle *computeHandle = NULL;
    MTL::VisibleFunctionTable *computeTableObject = NULL;
    MTL::ArgumentEncoder *computeArgumentEncoder = NULL;
    MTL::Buffer *computeArgumentBuffer = NULL;
    if(computeTable && cp)
    {
      computeHandle = cp->functionHandle(visible);
      MTL::VisibleFunctionTableDescriptor *td =
          MTL::VisibleFunctionTableDescriptor::visibleFunctionTableDescriptor();
      td->setFunctionCount(1);
      computeTableObject = cp->newVisibleFunctionTable(td);
      if(!computeHandle || !computeTableObject) return 4;
      if(computeTableRange)
      {
        const MTL::FunctionHandle *handles[] = {computeHandle};
        computeTableObject->setFunctions(handles, NS::Range::Make(0, 1));
      }
      else computeTableObject->setFunction(computeHandle, 0);
      if(computeArgumentTable)
      {
        computeArgumentEncoder = cs->newArgumentEncoder(1);
        if(!computeArgumentEncoder) return 4;
        computeArgumentBuffer = device->newBuffer(
            computeArgumentEncoder->encodedLength(), MTL::ResourceStorageModeShared);
        if(!computeArgumentBuffer) return 4;
        computeArgumentEncoder->setArgumentBuffer(computeArgumentBuffer, 0);
        if(computeArgumentTableRange)
        {
          const MTL::VisibleFunctionTable *tables[] = {computeTableObject};
          computeArgumentEncoder->setVisibleFunctionTables(tables, NS::Range::Make(0, 1));
        }
        else computeArgumentEncoder->setVisibleFunctionTable(computeTableObject, 0);
      }
    }
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    if(visibleLink)
    {
      if(!visible || visible->functionType() != MTL::FunctionTypeVisible) return 4;
      MTL::LinkedFunctions *links = MTL::LinkedFunctions::alloc()->init();
      const NS::Object *functions[] = {visible};
      links->setFunctions(NS::Array::array(functions, 1));
      if(vertexTable) pd->setVertexLinkedFunctions(links);
      else pd->setFragmentLinkedFunctions(links);
      links->release();
    }
    MTL::RenderPipelineState *rp = device->newRenderPipelineState(pd, &error);
    pd->release();
    MTL::FunctionHandle *handle = NULL;
    MTL::VisibleFunctionTable *table = NULL;
    if(visibleTable && rp)
    {
      const MTL::RenderStages stage = vertexTable ?
          MTL::RenderStageVertex : MTL::RenderStageFragment;
      handle = rp->functionHandle(visible, stage);
      MTL::VisibleFunctionTableDescriptor *td =
          MTL::VisibleFunctionTableDescriptor::visibleFunctionTableDescriptor();
      td->setFunctionCount(1);
      table = rp->newVisibleFunctionTable(td, stage);
      if(!handle || !table) return 4;
      if(visibleTableRange || vertexTableRange)
      {
        const MTL::FunctionHandle *handles[] = {handle};
        table->setFunctions(handles, NS::Range::Make(0, 1));
      }
      else table->setFunction(handle, 0);
    }
    const bool releaseReuse =
        !GetEnvVar("RENDERDOC_METAL_T133_RELEASE_REUSE_PROBE").empty();
    const bool aliasReuse = releaseReuse ||
        !GetEnvVar("RENDERDOC_METAL_T133_ALIAS_REUSE_PROBE").empty();
    const bool placementBuffer = aliasReuse ||
        !GetEnvVar("RENDERDOC_METAL_T116_PLACEMENT_BUFFER").empty();
    const bool aliasBuffer = !GetEnvVar("RENDERDOC_METAL_T131_ALIAS_BUFFER").empty();
    const bool heapBuffer = placementBuffer || aliasBuffer ||
        !GetEnvVar("RENDERDOC_METAL_T71_HEAP_BUFFER").empty();
    MTL::Heap *heap = NULL;
    NS::UInteger placementOffset = 0;
    if(heapBuffer)
    {
      MTL::HeapDescriptor *descriptor = MTL::HeapDescriptor::alloc()->init();
      descriptor->setSize(64 * 1024);
      descriptor->setStorageMode(MTL::StorageModePrivate);
      descriptor->setHazardTrackingMode(MTL::HazardTrackingModeTracked);
      if(placementBuffer)
      {
        descriptor->setType(MTL::HeapTypePlacement);
        MTL::SizeAndAlign layout = device->heapBufferSizeAndAlign(
            516, MTL::ResourceStorageModePrivate);
        if(!layout.size || !layout.align || layout.size >= 32 * 1024)
          return 6;
        placementOffset = ((layout.size + layout.align - 1) / layout.align) * layout.align;
      }
      heap = device->newHeap(descriptor);
      descriptor->release();
      if(!heap) return 6;
    }
    MTL::Buffer *gpu = placementBuffer
                                  ? heap->newBuffer(516, MTL::ResourceStorageModePrivate,
                                                    placementOffset)
                                  : heapBuffer ? heap->newBuffer(516, MTL::ResourceStorageModePrivate)
                                  : device->newBuffer(516, MTL::ResourceStorageModePrivate);
    MTL::Buffer *cpu = device->newBuffer(516, MTL::ResourceStorageModeShared);
    const bool multiIndexedDescriptorAS =
        !GetEnvVar("RENDERDOC_METAL_T182_MULTI_INDEXED_AS_DESCRIPTOR").empty();
    const bool indexedAS = multiIndexedDescriptorAS ||
        !GetEnvVar("RENDERDOC_METAL_T136_AS_INDEXED").empty();
    const bool boxDescriptorAS = !GetEnvVar("RENDERDOC_METAL_T178_BOX_AS_DESCRIPTOR").empty();
    const bool multiBoxDescriptorAS =
        !GetEnvVar("RENDERDOC_METAL_T179_MULTI_BOX_AS_DESCRIPTOR").empty();
    const bool tripleBoxDescriptorAS =
        !GetEnvVar("RENDERDOC_METAL_T180_TRIPLE_BOX_AS_DESCRIPTOR").empty();
    const bool boxOffsetDescriptorAS =
        !GetEnvVar("RENDERDOC_METAL_T212_BOX_OFFSET_DESCRIPTOR").empty();
    const bool boxStrideOffsetAS =
        !GetEnvVar("RENDERDOC_METAL_T215_BOX_STRIDE_OFFSET").empty();
    const bool boxStridePairAS = boxStrideOffsetAS ||
        !GetEnvVar("RENDERDOC_METAL_T214_BOX_STRIDE_PAIR").empty();
    const bool boxStrideAS = boxStridePairAS ||
        !GetEnvVar("RENDERDOC_METAL_T213_BOX_STRIDE").empty();
    const bool boxBufferOffset =
        boxOffsetDescriptorAS || boxStrideOffsetAS ||
        !GetEnvVar("RENDERDOC_METAL_T209_BOX_BUFFER_OFFSET").empty();
    const bool boxScratchOffset =
        boxStrideOffsetAS || !GetEnvVar("RENDERDOC_METAL_T210_BOX_SCRATCH_OFFSET").empty();
    const bool boxAS = boxDescriptorAS || multiBoxDescriptorAS || tripleBoxDescriptorAS ||
        boxBufferOffset || boxScratchOffset || boxStrideAS ||
        !GetEnvVar("RENDERDOC_METAL_T137_AS_BOX").empty();
    const bool descriptorAS = boxDescriptorAS || multiBoxDescriptorAS || tripleBoxDescriptorAS ||
        boxOffsetDescriptorAS || boxStridePairAS ||
        multiIndexedDescriptorAS ||
        !GetEnvVar("RENDERDOC_METAL_T138_AS_DESCRIPTOR").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T181_MULTI_TRIANGLE_DESCRIPTOR").empty();
    const bool multiTriangleDescriptorAS =
        !GetEnvVar("RENDERDOC_METAL_T181_MULTI_TRIANGLE_DESCRIPTOR").empty();
    const bool copyAS = !GetEnvVar("RENDERDOC_METAL_T139_AS_COPY").empty();
    const bool compactAS = !GetEnvVar("RENDERDOC_METAL_T140_AS_COMPACT").empty();
    const bool buildAS = compactAS || copyAS || descriptorAS || boxAS || indexedAS ||
                         !GetEnvVar("RENDERDOC_METAL_T135_AS_BUILD").empty();
    MTL::Buffer *asVertices = NULL;
    MTL::Buffer *asIndices = NULL;
    MTL::Buffer *asScratch = NULL;
    MTL::Buffer *asCompacted = NULL;
    MTL::Buffer *asCompactedOutput = NULL;
    MTL::AccelerationStructure *as = NULL;
    MTL::AccelerationStructure *asCopied = NULL;
    MTL::AccelerationStructure *asCompactDestination = NULL;
    MTL::PrimitiveAccelerationStructureDescriptor *asBuildDescriptor = NULL;
    if(buildAS)
    {
      if(boxAS)
      {
        const float bounds[6] = {-1, -1, -1, 1, 1, 1};
        const float multiBounds[12] = {-1, -1, -1, 1, 1, 1,
                                        2, 2, 2, 3, 3, 3};
        const float tripleBounds[18] = {-1, -1, -1, 1, 1, 1,
                                         2, 2, 2, 3, 3, 3,
                                         4, 4, 4, 5, 5, 5};
        const float offsetBounds[18] = {20, 20, 20, 21, 21, 21,
                                        30, 30, 30, 31, 31, 31,
                                        -1, -1, -1, 1, 1, 1};
        const float stridedBounds[8] = {-1, -1, -1, 1, 1, 1, 99, 99};
        const float stridedPairBounds[16] = {20, 20, 20, 21, 21, 21, 99, 99,
                                              -1, -1, -1, 1, 1, 1, 99, 99};
        const float stridedOffsetBounds[28] = {40, 40, 40, 41, 41, 41,
                                                50, 50, 50, 51, 51, 51,
                                                20, 20, 20, 21, 21, 21, 99, 99,
                                                -1, -1, -1, 1, 1, 1, 99, 99};
        asVertices = device->newBuffer(boxStrideOffsetAS ? (const void *)stridedOffsetBounds :
                                      boxStridePairAS ? (const void *)stridedPairBounds :
                                      boxStrideAS ? (const void *)stridedBounds :
                                      boxBufferOffset ? (const void *)offsetBounds :
                                      tripleBoxDescriptorAS ? (const void *)tripleBounds :
                                      multiBoxDescriptorAS ? (const void *)multiBounds :
                                      (const void *)bounds,
                                       boxStrideOffsetAS ? sizeof(stridedOffsetBounds) :
                                       boxStridePairAS ? sizeof(stridedPairBounds) :
                                       boxStrideAS ? sizeof(stridedBounds) :
                                       boxBufferOffset ? sizeof(offsetBounds) :
                                       tripleBoxDescriptorAS ? sizeof(tripleBounds) :
                                       multiBoxDescriptorAS ? sizeof(multiBounds) : sizeof(bounds),
                                       MTL::ResourceStorageModeShared);
      }
      else
      {
        const float triangleVertices[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
        const float multiTriangleVertices[18] = {0, 0, 0, 1, 0, 0, 0, 1, 0,
                                                  2, 0, 0, 3, 0, 0, 2, 1, 0};
        asVertices = device->newBuffer((multiTriangleDescriptorAS || multiIndexedDescriptorAS) ?
                                      (const void *)multiTriangleVertices :
                                      (const void *)triangleVertices,
                                      (multiTriangleDescriptorAS || multiIndexedDescriptorAS) ?
                                      sizeof(multiTriangleVertices) :
                                      sizeof(triangleVertices),
                                       MTL::ResourceStorageModeShared);
      }
      if(indexedAS)
      {
        const uint16_t triangleIndices[3] = {0, 1, 2};
        const uint16_t multiTriangleIndices[6] = {0, 1, 2, 3, 4, 5};
        asIndices = device->newBuffer(multiIndexedDescriptorAS ?
                                      (const void *)multiTriangleIndices :
                                      (const void *)triangleIndices,
                                      multiIndexedDescriptorAS ? sizeof(multiTriangleIndices) :
                                      sizeof(triangleIndices),
                                       MTL::ResourceStorageModeShared);
      }
      asBuildDescriptor = MTL::PrimitiveAccelerationStructureDescriptor::descriptor()->retain();
      if(boxAS)
      {
        MTL::AccelerationStructureBoundingBoxGeometryDescriptor *geometry =
            MTL::AccelerationStructureBoundingBoxGeometryDescriptor::descriptor();
        geometry->setBoundingBoxBuffer(asVertices);
        geometry->setBoundingBoxBufferOffset(boxBufferOffset ? 48 : 0);
        if(boxStrideAS) geometry->setBoundingBoxStride(32);
        geometry->setBoundingBoxCount(tripleBoxDescriptorAS ? 3 :
                                      (multiBoxDescriptorAS || boxStridePairAS) ? 2 : 1);
        asBuildDescriptor->setGeometryDescriptors(NS::Array::array(geometry));
      }
      else
      {
        MTL::AccelerationStructureTriangleGeometryDescriptor *geometry =
            MTL::AccelerationStructureTriangleGeometryDescriptor::descriptor();
        geometry->setVertexBuffer(asVertices);
        geometry->setVertexStride(3 * sizeof(float));
        geometry->setVertexFormat(MTL::AttributeFormatFloat3);
        geometry->setTriangleCount((multiTriangleDescriptorAS || multiIndexedDescriptorAS) ? 2 : 1);
        if(indexedAS)
        {
          geometry->setIndexBuffer(asIndices);
          geometry->setIndexType(MTL::IndexTypeUInt16);
        }
        asBuildDescriptor->setGeometryDescriptors(NS::Array::array(geometry));
      }
      MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(asBuildDescriptor);
      as = descriptorAS ? device->newAccelerationStructure(asBuildDescriptor) :
                          device->newAccelerationStructure(sizes.accelerationStructureSize);
      if(copyAS) asCopied = device->newAccelerationStructure(sizes.accelerationStructureSize);
      asScratch = device->newBuffer(sizes.buildScratchBufferSize +
                                    (boxScratchOffset ? 256 : 0),
                                    MTL::ResourceStorageModePrivate);
      asCompacted = device->newBuffer(sizeof(uint64_t), MTL::ResourceStorageModeShared);
      if(compactAS)
        asCompactedOutput = device->newBuffer(sizeof(uint64_t), MTL::ResourceStorageModeShared);
      if(!asVertices || (indexedAS && !asIndices) || !as || (copyAS && !asCopied) ||
         !asScratch || !asCompacted || (compactAS && !asCompactedOutput) ||
         as->size() != sizes.accelerationStructureSize || as->device() != device ||
         as->storageMode() != MTL::StorageModePrivate || as->heap() ||
         as->heapOffset() != 0 || as->allocatedSize() < as->size() || as->isAliasable())
        return 8;
    }
    if(!GetEnvVar("RENDERDOC_METAL_T134_AS_SIZE_QUERY").empty())
    {
      MTL::AccelerationStructureTriangleGeometryDescriptor *triangle =
          MTL::AccelerationStructureTriangleGeometryDescriptor::descriptor();
      triangle->setVertexBuffer(cpu);
      triangle->setVertexStride(3 * sizeof(float));
      triangle->setVertexFormat(MTL::AttributeFormatFloat3);
      triangle->setTriangleCount(1);
      MTL::PrimitiveAccelerationStructureDescriptor *asDescriptor =
          MTL::PrimitiveAccelerationStructureDescriptor::descriptor();
      asDescriptor->setGeometryDescriptors(NS::Array::array(triangle));
      MTL::AccelerationStructureSizes asSizes = device->accelerationStructureSizes(asDescriptor);
      MTL::SizeAndAlign asLayout = device->heapAccelerationStructureSizeAndAlign(asDescriptor);
      if(!asSizes.accelerationStructureSize || !asSizes.buildScratchBufferSize ||
         asLayout.size < asSizes.accelerationStructureSize || !asLayout.align)
        return 8;
      TEST_LOG("T134 AS query: size=%zu scratch=%zu heap=%zu align=%zu",
               size_t(asSizes.accelerationStructureSize), size_t(asSizes.buildScratchBufferSize),
               size_t(asLayout.size), size_t(asLayout.align));
      triangle->setIndexBuffer(cpu);
      triangle->setIndexType(MTL::IndexTypeUInt16);
      MTL::AccelerationStructureSizes indexedSizes = device->accelerationStructureSizes(asDescriptor);
      MTL::SizeAndAlign indexedLayout = device->heapAccelerationStructureSizeAndAlign(asDescriptor);
      if(!indexedSizes.accelerationStructureSize || !indexedSizes.buildScratchBufferSize ||
         indexedLayout.size < indexedSizes.accelerationStructureSize || !indexedLayout.align)
        return 8;
      TEST_LOG("T134 indexed AS query: size=%zu scratch=%zu heap=%zu align=%zu",
               size_t(indexedSizes.accelerationStructureSize),
               size_t(indexedSizes.buildScratchBufferSize), size_t(indexedLayout.size),
               size_t(indexedLayout.align));
      MTL::AccelerationStructureBoundingBoxGeometryDescriptor *box =
          MTL::AccelerationStructureBoundingBoxGeometryDescriptor::descriptor();
      box->setBoundingBoxBuffer(cpu);
      box->setBoundingBoxCount(1);
      asDescriptor->setGeometryDescriptors(NS::Array::array(box));
      MTL::AccelerationStructureSizes boxSizes = device->accelerationStructureSizes(asDescriptor);
      MTL::SizeAndAlign boxLayout = device->heapAccelerationStructureSizeAndAlign(asDescriptor);
      if(!boxSizes.accelerationStructureSize || !boxSizes.buildScratchBufferSize ||
         boxLayout.size < boxSizes.accelerationStructureSize || !boxLayout.align)
        return 8;
      TEST_LOG("T134 box AS query: size=%zu scratch=%zu heap=%zu align=%zu",
               size_t(boxSizes.accelerationStructureSize), size_t(boxSizes.buildScratchBufferSize),
               size_t(boxLayout.size), size_t(boxLayout.align));
    }
    MTL::Buffer *alias = aliasBuffer ?
        heap->newBuffer(256, MTL::ResourceStorageModePrivate) : NULL;
    if(!cp || !rp || !gpu || !cpu || (aliasBuffer && !alias)) return 4;
    if(heapBuffer && gpu->heap() != heap) return 7;
    if(placementBuffer && gpu->heapOffset() != placementOffset) return 7;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    uint32_t frameIndex = 0;
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      if(compactAS)
      {
        MTL::CommandBuffer *sizeCB = queue->commandBuffer();
        MTL::AccelerationStructureCommandEncoder *sizeEncoder =
            sizeCB->accelerationStructureCommandEncoder();
        sizeEncoder->buildAccelerationStructure(as, asBuildDescriptor, asScratch,
                                                boxScratchOffset ? 256 : 0);
        sizeEncoder->writeCompactedAccelerationStructureSize(as, asCompacted, 0,
                                                              MTL::DataTypeULong);
        sizeEncoder->endEncoding();
        sizeCB->commit(); sizeCB->waitUntilCompleted();
        const uint64_t required = *(const uint64_t *)asCompacted->contents();
        if(sizeCB->error() || !required || required >= as->size()) return 8;
        if(!asCompactDestination)
          asCompactDestination = device->newAccelerationStructure(required);
        if(!asCompactDestination || asCompactDestination->size() < required) return 8;
      }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      if(buildAS)
      {
        MTL::AccelerationStructureCommandEncoder *asEncoder =
            cb->accelerationStructureCommandEncoder();
        if(!asEncoder) return 8;
        if(compactAS)
          asEncoder->copyAndCompactAccelerationStructure(as, asCompactDestination);
        else
        {
          asEncoder->buildAccelerationStructure(as, asBuildDescriptor, asScratch,
                                                boxScratchOffset ? 256 : 0);
          if(copyAS) asEncoder->copyAccelerationStructure(as, asCopied);
        }
        asEncoder->writeCompactedAccelerationStructureSize(
            compactAS ? asCompactDestination : copyAS ? asCopied : as,
            compactAS ? asCompactedOutput : asCompacted, 0,
                                                            MTL::DataTypeULong);
        asEncoder->endEncoding();
      }
      MTL::BlitCommandEncoder *blit = cb->blitCommandEncoder();
      blit->fillBuffer(gpu, NS::Range::Make(0, 516), 0xa5);
      blit->endEncoding();
      MTL::ComputeCommandEncoder *compute = cb->computeCommandEncoder();
      compute->setComputePipelineState(cp); compute->setBuffer(gpu, 0, 0);
      if(computeArgumentTable) compute->setBuffer(computeArgumentBuffer, 0, 1);
      else if(computeTableRange)
      {
        const MTL::VisibleFunctionTable *tables[] = {computeTableObject};
        compute->setVisibleFunctionTables(tables, NS::Range::Make(1, 1));
      }
      else if(computeTable) compute->setVisibleFunctionTable(computeTableObject, 1);
      compute->dispatchThreads(MTL::Size::Make(516, 1, 1), MTL::Size::Make(4, 1, 1));
      compute->endEncoding();
      blit = cb->blitCommandEncoder();
      blit->copyFromBuffer(gpu, 0, cpu, 0, 516);
      blit->endEncoding();
      MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->setRenderPipelineState(rp);
      if(vertexTableRange)
      {
        const MTL::VisibleFunctionTable *tables[] = {table};
        render->setVertexVisibleFunctionTables(tables, NS::Range::Make(0, 1));
      }
      else if(vertexTable) render->setVertexVisibleFunctionTable(table, 0);
      else if(visibleTableRange)
      {
        const MTL::VisibleFunctionTable *tables[] = {table};
        render->setFragmentVisibleFunctionTables(tables, NS::Range::Make(0, 1));
      }
      else if(visibleTable) render->setFragmentVisibleFunctionTable(table, 0);
      else render->setFragmentBuffer(gpu, 0, 0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      render->endEncoding(); cb->presentDrawable(drawable); cb->commit(); cb->waitUntilCompleted();
      if(buildAS)
      {
        const uint64_t compacted = *(const uint64_t *)(compactAS ?
            asCompactedOutput->contents() : asCompacted->contents());
        if(cb->error() || !compacted || compacted > as->size())
        {
          TEST_WARN("T%d AS build failed: compacted size %llu",
                    compactAS ? 140 : copyAS ? 139 : multiIndexedDescriptorAS ? 182 :
                    multiTriangleDescriptorAS ? 181 :
                    tripleBoxDescriptorAS ? 180 :
                    multiBoxDescriptorAS ? 179 :
                    boxDescriptorAS ? 178 : boxOffsetDescriptorAS ? 212 :
                    boxStrideOffsetAS ? 215 : boxStridePairAS ? 214 : boxStrideAS ? 213 :
                    descriptorAS ? 138 :
                    boxBufferOffset && boxScratchOffset ? 211 :
                    boxScratchOffset ? 210 : boxBufferOffset ? 209 :
                    boxAS ? 137 : indexedAS ? 136 : 135,
                    compacted);
          return 8;
        }
        TEST_LOG("T%d AS build: size=%zu compacted=%llu",
                 compactAS ? 140 : copyAS ? 139 : multiIndexedDescriptorAS ? 182 :
                 multiTriangleDescriptorAS ? 181 :
                 tripleBoxDescriptorAS ? 180 :
                 multiBoxDescriptorAS ? 179 :
                 boxDescriptorAS ? 178 : boxOffsetDescriptorAS ? 212 :
                 boxStrideOffsetAS ? 215 : boxStridePairAS ? 214 : boxStrideAS ? 213 :
                 descriptorAS ? 138 :
                 boxBufferOffset && boxScratchOffset ? 211 :
                 boxScratchOffset ? 210 : boxBufferOffset ? 209 :
                 boxAS ? 137 : indexedAS ? 136 : 135,
                 size_t(compactAS ? asCompactDestination->size() : as->size()), compacted);
      }
      if(aliasReuse && frameIndex == 2)
      {
        if(releaseReuse)
        {
          gpu->release();
          gpu = NULL;
        }
        else
          gpu->makeAliasable();
        MTL::Buffer *reused = heap->newBuffer(516, MTL::ResourceStorageModePrivate,
                                               placementOffset);
        if(!reused || reused->heapOffset() != placementOffset) return 7;
        MTL::CommandBuffer *reuseCB = queue->commandBuffer();
        MTL::ComputeCommandEncoder *reuseCompute = reuseCB->computeCommandEncoder();
        reuseCompute->setComputePipelineState(cp);
        reuseCompute->setBuffer(reused, 0, 0);
        reuseCompute->dispatchThreads(MTL::Size::Make(516, 1, 1),
                                      MTL::Size::Make(4, 1, 1));
        reuseCompute->endEncoding();
        MTL::BlitCommandEncoder *reuseBlit = reuseCB->blitCommandEncoder();
        reuseBlit->copyFromBuffer(reused, 0, cpu, 0, 516);
        reuseBlit->endEncoding();
        reuseCB->commit(); reuseCB->waitUntilCompleted();
        if(reuseCB->error()) return 7;
        for(uint32_t i = 0; i < 516; i++)
          if(((const byte *)cpu->contents())[i] != byte(i * 7 + 3)) return 7;
        reused->release();
      }
      if(alias && frameIndex == (native ? 0U : 2U))
      {
        alias->makeAliasable();
        if(!alias->isAliasable()) return 7;
      }
      EndCaptureFrame();
      frameIndex++;
      if(native)
      {
        for(uint32_t i = 0; i < 516; i++)
          if(((const byte *)cpu->contents())[i] != byte(i * 7 + 3))
          {
            TEST_WARN("T39 readback byte %u: got %u expected %u", i,
                      ((const byte *)cpu->contents())[i], byte(i * 7 + 3));
            failed = true;
            break;
          }
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(200, 150, 1, 1), 0);
        const byte expected[] = {byte(visibleTable ? 0 : 24),
                                 byte(vertexTable ? 255 : visibleTable ? 0 : 3),
                                 byte(vertexTable ? 0 : visibleTable ? 255 : 3), 255};
        if(memcmp(pixel, expected, 4) != 0)
        {
          TEST_WARN("T39 pixel BGRA got %u/%u/%u/%u expected %u/%u/%u/%u", pixel[0],
                    pixel[1], pixel[2], pixel[3], expected[0], expected[1], expected[2], expected[3]);
          failed = true;
        }
      }
      pool->drain();
    }
    if(gpu) gpu->release(); cpu->release();
    if(as) as->release();
    if(asCopied) asCopied->release();
    if(asCompactDestination) asCompactDestination->release();
    if(asVertices) asVertices->release();
    if(asIndices) asIndices->release();
    if(asScratch) asScratch->release();
    if(asCompacted) asCompacted->release();
    if(asCompactedOutput) asCompactedOutput->release();
    if(asBuildDescriptor) asBuildDescriptor->release();
    if(alias) alias->release();
    if(computeArgumentBuffer) computeArgumentBuffer->release();
    if(computeArgumentEncoder) computeArgumentEncoder->release();
    if(computeTableObject) computeTableObject->release();
    cp->release();
    if(table) table->release();
    rp->release();
    if(heap) heap->release();
    cs->release(); vs->release(); fs->release(); lib->release();
    if(visible) visible->release();
    if(failed) TEST_WARN("T39 private buffer bytes or framebuffer differ");
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
