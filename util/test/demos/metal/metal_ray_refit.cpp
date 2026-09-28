// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Ray_Refit, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "A GPU ray changes from hit to miss after an in-place acceleration-structure refit.";

  int main()
  {
    if(!Init()) return 3;
    const bool formattedOutOfPlace =
        !GetEnvVar("RENDERDOC_METAL_T253_REFIT_FLOAT4_OUT_OF_PLACE").empty();
    const bool formattedNoDuplicate =
        !GetEnvVar("RENDERDOC_METAL_T254_REFIT_PADDED_NO_DUPLICATE_OFFSET").empty();
    const bool formattedCompact =
        !GetEnvVar("RENDERDOC_METAL_T255_REFIT_FLOAT4_COMPACT").empty();
    const bool alternateFormattedVertices =
        !GetEnvVar("RENDERDOC_METAL_T263_REFIT_ALTERNATE_FLOAT4_VERTICES").empty();
    const bool alternateNoDuplicateVertices =
        !GetEnvVar("RENDERDOC_METAL_T264_REFIT_ALTERNATE_NO_DUPLICATE").empty();
    const bool alternateOutOfPlaceVertices =
        !GetEnvVar("RENDERDOC_METAL_T265_REFIT_ALTERNATE_OUT_OF_PLACE").empty();
    const bool indexedIndexOffset32 =
        !GetEnvVar("RENDERDOC_METAL_T281_INDEXED_REFIT_UINT32_INDEX_OFFSET").empty();
    const bool indexedFloat4VertexOffset =
        !GetEnvVar("RENDERDOC_METAL_T282_INDEXED_REFIT_FLOAT4_VERTEX_OFFSET").empty();
    const bool indexedMultiTriangle =
        !GetEnvVar("RENDERDOC_METAL_T284_INDEXED_REFIT_TWO_TRIANGLES").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T285_INDEXED_REFIT_TWO_TRIANGLES_UINT32").empty();
    const bool indexedRefit32 = indexedIndexOffset32 ||
        !GetEnvVar("RENDERDOC_METAL_T285_INDEXED_REFIT_TWO_TRIANGLES_UINT32").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T267_INDEXED_REFIT_UINT32").empty();
    const bool alternateIndices =
        !GetEnvVar("RENDERDOC_METAL_T268_INDEXED_REFIT_ALTERNATE_INDICES").empty();
    const bool indexedOutOfPlace =
        !GetEnvVar("RENDERDOC_METAL_T269_INDEXED_REFIT_OUT_OF_PLACE").empty();
    const bool indexedCompact =
        !GetEnvVar("RENDERDOC_METAL_T270_INDEXED_REFIT_COMPACT").empty();
    const bool indexedDescriptorAllocation =
        !GetEnvVar("RENDERDOC_METAL_T271_INDEXED_REFIT_DESCRIPTOR_ALLOCATION").empty();
    const bool indexedFormattedFloat4 =
        !GetEnvVar("RENDERDOC_METAL_T272_INDEXED_REFIT_FLOAT4").empty();
    const bool indexedNoDuplicate =
        !GetEnvVar("RENDERDOC_METAL_T273_INDEXED_REFIT_NO_DUPLICATE").empty();
    const bool indexedScratchOffset =
        !GetEnvVar("RENDERDOC_METAL_T274_INDEXED_REFIT_SCRATCH_OFFSET").empty();
    const bool indexedIndexOffset = indexedIndexOffset32 ||
        !GetEnvVar("RENDERDOC_METAL_T275_INDEXED_REFIT_INDEX_OFFSET").empty();
    const bool indexedVertexOffset =
        !GetEnvVar("RENDERDOC_METAL_T276_INDEXED_REFIT_VERTEX_OFFSET").empty();
    const bool indexedTableOffset =
        !GetEnvVar("RENDERDOC_METAL_T278_INDEXED_REFIT_TABLE_OFFSET").empty();
    const bool indexedOpaqueToggle =
        !GetEnvVar("RENDERDOC_METAL_T279_INDEXED_REFIT_OPAQUE_TOGGLE").empty();
    const bool indexedRefit = indexedRefit32 || alternateIndices || indexedOutOfPlace ||
        indexedCompact || indexedDescriptorAllocation || indexedFormattedFloat4 ||
        indexedNoDuplicate || indexedScratchOffset || indexedIndexOffset ||
        indexedVertexOffset || indexedFloat4VertexOffset || indexedTableOffset ||
        indexedOpaqueToggle || indexedMultiTriangle ||
        !GetEnvVar("RENDERDOC_METAL_T266_INDEXED_REFIT_UINT16").empty();
    const bool alternateVertices = alternateFormattedVertices ||
        alternateNoDuplicateVertices || alternateOutOfPlaceVertices ||
        !GetEnvVar("RENDERDOC_METAL_T262_REFIT_ALTERNATE_VERTICES").empty();
    const bool formattedFloat3 = formattedNoDuplicate ||
        !GetEnvVar("RENDERDOC_METAL_T251_REFIT_PADDED_FLOAT3").empty();
    const bool formattedFloat4 = formattedOutOfPlace || formattedCompact ||
        indexedFormattedFloat4 || indexedFloat4VertexOffset ||
        alternateFormattedVertices ||
        !GetEnvVar("RENDERDOC_METAL_T252_REFIT_FLOAT4").empty();
    const bool formatted = formattedFloat3 || formattedFloat4;
    const bool multiCompactRefit =
        !GetEnvVar("RENDERDOC_METAL_T201_MULTI_REFIT_COMPACT_RAY").empty();
    const bool compactRefit = multiCompactRefit || formattedCompact || indexedCompact ||
        !GetEnvVar("RENDERDOC_METAL_T200_COMPACT_REFIT_RAY").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T243_REFIT_NO_DUPLICATE_COMPACT").empty();
    const bool descriptorAllocation = formattedOutOfPlace || alternateOutOfPlaceVertices ||
        indexedDescriptorAllocation ||
        !GetEnvVar("RENDERDOC_METAL_T202_REFIT_DESCRIPTOR_ALLOCATION").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T241_REFIT_NO_DUPLICATE_OUT_OF_PLACE").empty();
    const bool extendedOffset = formattedNoDuplicate || indexedScratchOffset ||
        !GetEnvVar("RENDERDOC_METAL_T204_REFIT_SCRATCH_OFFSET").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T206_OUT_OF_PLACE_OFFSET_REFIT").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T242_REFIT_NO_DUPLICATE_SCRATCH_OFFSET").empty();
    const bool outOfPlace = formattedOutOfPlace || alternateOutOfPlaceVertices ||
        indexedOutOfPlace ||
        !GetEnvVar("RENDERDOC_METAL_T205_OUT_OF_PLACE_REFIT").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T206_OUT_OF_PLACE_OFFSET_REFIT").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T241_REFIT_NO_DUPLICATE_OUT_OF_PLACE").empty();
    const bool tightRefitScratch =
        !GetEnvVar("RENDERDOC_METAL_T208_TIGHT_REFIT_SCRATCH").empty();
    const bool noDuplicate = formattedNoDuplicate || alternateNoDuplicateVertices ||
        indexedNoDuplicate ||
        !GetEnvVar("RENDERDOC_METAL_T240_REFIT_NO_DUPLICATE").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T241_REFIT_NO_DUPLICATE_OUT_OF_PLACE").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T242_REFIT_NO_DUPLICATE_SCRATCH_OFFSET").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T243_REFIT_NO_DUPLICATE_COMPACT").empty();
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
using namespace metal::raytracing;
kernel void trace_rays(acceleration_structure<> structure [[buffer(0)]],
                       device uint *result [[buffer(1)]],
                       constant uint &phase [[buffer(2)]])
{
  intersector<triangle_data> query;
  ray testRay(float3(0, 0, -1), float3(0, 0, 1));
  auto hit = query.intersect(testRay, structure);
  result[phase] = hit.type == intersection_type::triangle ? 1 : 0;
}
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!library)
    {
      TEST_WARN("Ray refit library: %s", error ? error->localizedDescription()->utf8String() : "nil");
      return 4;
    }
    MTL::Function *function = library->newFunction(MTLSTR("trace_rays"));
    MTL::ComputePipelineState *pipeline = device->newComputePipelineState(function, &error);
    if(!pipeline)
    {
      TEST_WARN("Ray refit pipeline: %s", error ? error->localizedDescription()->utf8String() : "nil");
      return 4;
    }
    const float initial[18] = {-1, -1, 0, 1, -1, 0, 0, 1, 0,
                               10, -1, 0, 12, -1, 0, 11, 1, 0};
    const float shifted[18] = {1, -1, 0, 3, -1, 0, 2, 1, 0,
                               10, -1, 0, 12, -1, 0, 11, 1, 0};
    const float offsetInitial[18] = {10, -1, 0, 12, -1, 0, 11, 1, 0,
                                     -1, -1, 0, 1, -1, 0, 0, 1, 0};
    const float offsetShifted[18] = {10, -1, 0, 12, -1, 0, 11, 1, 0,
                                     1, -1, 0, 3, -1, 0, 2, 1, 0};
    const float formattedInitial[12] = {-1, -1, 0, 99, 1, -1, 0, 99,
                                         0, 1, 0, 99};
    const float formattedShifted[12] = {1, -1, 0, 99, 3, -1, 0, 99,
                                         2, 1, 0, 99};
    const float formattedOffsetInitial[16] = {10, -1, 0, 99,
                                               -1, -1, 0, 99, 1, -1, 0, 99,
                                               0, 1, 0, 99};
    const float formattedOffsetShifted[16] = {10, -1, 0, 99,
                                               1, -1, 0, 99, 3, -1, 0, 99,
                                               2, 1, 0, 99};
    const size_t geometryBytes = indexedFloat4VertexOffset ? sizeof(formattedOffsetInitial) :
                                 formatted ? sizeof(formattedInitial) :
                                 (multiCompactRefit || indexedRefit ? 2 : 1) * 9 * sizeof(float);
    const void *initialGeometry = indexedFloat4VertexOffset ?
        (const void *)formattedOffsetInitial : formatted ? (const void *)formattedInitial :
        indexedVertexOffset ? (const void *)offsetInitial : (const void *)initial;
    const void *shiftedGeometry = indexedFloat4VertexOffset ?
        (const void *)formattedOffsetShifted : formatted ? (const void *)formattedShifted :
        indexedVertexOffset ? (const void *)offsetShifted : (const void *)shifted;
    MTL::Buffer *vertices = device->newBuffer(initialGeometry, geometryBytes,
                                              MTL::ResourceStorageModeShared);
    MTL::Buffer *movedVertices = device->newBuffer(shiftedGeometry, geometryBytes,
                                                   MTL::ResourceStorageModeShared);
    const uint16_t triangleIndices[3] = {0, 1, 2};
    const uint16_t twoTriangleIndices[6] = {0, 1, 2, 3, 4, 5};
    const uint16_t prefixedIndices[4] = {99, 0, 1, 2};
    const uint16_t shiftedIndices[3] = {3, 4, 5};
    const uint16_t shiftedTwoTriangleIndices[6] = {3, 4, 5, 3, 4, 5};
    const uint32_t triangleIndices32[3] = {0, 1, 2};
    const uint32_t twoTriangleIndices32[6] = {0, 1, 2, 3, 4, 5};
    const uint32_t prefixedIndices32[5] = {99, 99, 0, 1, 2};
    MTL::Buffer *indices = indexedRefit ? device->newBuffer(
        indexedIndexOffset32 ? (const void *)prefixedIndices32 :
        indexedMultiTriangle && indexedRefit32 ? (const void *)twoTriangleIndices32 :
        indexedRefit32 ? (const void *)triangleIndices32 :
        indexedMultiTriangle ? (const void *)twoTriangleIndices :
        indexedIndexOffset ? (const void *)prefixedIndices : (const void *)triangleIndices,
        indexedIndexOffset32 ? sizeof(prefixedIndices32) :
        indexedMultiTriangle && indexedRefit32 ? sizeof(twoTriangleIndices32) :
        indexedRefit32 ? sizeof(triangleIndices32) :
        indexedMultiTriangle ? sizeof(twoTriangleIndices) :
        indexedIndexOffset ? sizeof(prefixedIndices) : sizeof(triangleIndices),
        MTL::ResourceStorageModeShared) : NULL;
    MTL::Buffer *movedIndices = alternateIndices ?
        device->newBuffer(indexedMultiTriangle ? (const void *)shiftedTwoTriangleIndices :
                          (const void *)shiftedIndices,
                          indexedMultiTriangle ? sizeof(shiftedTwoTriangleIndices) :
                                                 sizeof(shiftedIndices),
                          MTL::ResourceStorageModeShared) : NULL;
    MTL::Buffer *output = device->newBuffer(2 * sizeof(uint32_t),
                                           MTL::ResourceStorageModeShared);
    MTL::Buffer *compactedSize = compactRefit ?
        device->newBuffer(sizeof(uint64_t), MTL::ResourceStorageModeShared) : NULL;
    MTL::AccelerationStructureTriangleGeometryDescriptor *triangle =
        MTL::AccelerationStructureTriangleGeometryDescriptor::descriptor();
    triangle->setVertexBuffer(vertices);
    if(indexedVertexOffset) triangle->setVertexBufferOffset(36);
    if(indexedFloat4VertexOffset) triangle->setVertexBufferOffset(16);
    triangle->setVertexStride(formatted ? 16 : 12);
    if(indexedRefit)
    {
      triangle->setIndexBuffer(indices);
      triangle->setIndexType(indexedRefit32 ? MTL::IndexTypeUInt32 : MTL::IndexTypeUInt16);
      if(indexedIndexOffset) triangle->setIndexBufferOffset(indexedIndexOffset32 ? 8 : 2);
    }
    if(formattedFloat4) triangle->setVertexFormat(MTL::AttributeFormatFloat4);
    if(indexedTableOffset) triangle->setIntersectionFunctionTableOffset(1);
    if(indexedOpaqueToggle) triangle->setOpaque(!triangle->opaque());
    triangle->setTriangleCount(multiCompactRefit || indexedMultiTriangle ? 2 : 1);
    if(noDuplicate) triangle->setAllowDuplicateIntersectionFunctionInvocation(false);
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
        MTL::PrimitiveAccelerationStructureDescriptor::descriptor();
    descriptor->setUsage(MTL::AccelerationStructureUsageRefit);
    descriptor->setGeometryDescriptors(NS::Array::array(triangle));
    const MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(descriptor);
    if(descriptorAllocation)
    {
      const MTL::SizeAndAlign layout = device->heapAccelerationStructureSizeAndAlign(descriptor);
      if(layout.size < sizes.accelerationStructureSize || !layout.align) return 5;
    }
    MTL::AccelerationStructure *structure =
        descriptorAllocation ? device->newAccelerationStructure(descriptor) :
                               device->newAccelerationStructure(sizes.accelerationStructureSize);
    MTL::Buffer *scratch = device->newBuffer(
        sizes.buildScratchBufferSize > sizes.refitScratchBufferSize ?
            sizes.buildScratchBufferSize + (extendedOffset ? 256 : 0) :
            sizes.refitScratchBufferSize + (extendedOffset ? 256 : 0),
        MTL::ResourceStorageModePrivate);
    MTL::Buffer *refitScratch = tightRefitScratch ?
        device->newBuffer(sizes.refitScratchBufferSize + (extendedOffset ? 256 : 0),
                          MTL::ResourceStorageModePrivate) : scratch;
    MTL::CommandQueue *queue = device->newCommandQueue();
    if(!vertices || !movedVertices || !output || !structure || !scratch ||
       !refitScratch || !queue || (indexedRefit && !indices) ||
       (alternateIndices && !movedIndices) ||
       (compactRefit && !compactedSize) ||
       !sizes.refitScratchBufferSize) return 5;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      memcpy(vertices->contents(), initialGeometry, geometryBytes);
      if(alternateVertices) triangle->setVertexBuffer(vertices);
      if(alternateIndices) triangle->setIndexBuffer(indices);
      memset(output->contents(), 0, 2 * sizeof(uint32_t));
      ((uint32_t *)output->contents())[1] = 9;
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *build = queue->commandBuffer();
      MTL::AccelerationStructureCommandEncoder *asEncoder =
          build->accelerationStructureCommandEncoder();
      asEncoder->buildAccelerationStructure(structure, descriptor, scratch, 0);
      asEncoder->endEncoding();
      MTL::ComputeCommandEncoder *compute = build->computeCommandEncoder();
      const uint32_t first = 0;
      compute->setComputePipelineState(pipeline);
      compute->setAccelerationStructure(structure, 0);
      compute->setBuffer(output, 0, 1);
      compute->setBytes(&first, sizeof(first), 2);
      compute->dispatchThreads(MTL::Size::Make(1, 1, 1), MTL::Size::Make(1, 1, 1));
      compute->endEncoding();
      build->commit(); build->waitUntilCompleted();
      if(build->error()) return 6;
      MTL::CommandBuffer *refit = queue->commandBuffer();
      MTL::AccelerationStructure *refitTarget = outOfPlace ?
          device->newAccelerationStructure(sizes.accelerationStructureSize) : structure;
      if(!refitTarget) return 5;
      if(alternateVertices) triangle->setVertexBuffer(movedVertices);
      if(alternateIndices) triangle->setIndexBuffer(movedIndices);
      if(!alternateVertices && !alternateIndices)
      {
        MTL::BlitCommandEncoder *blit = refit->blitCommandEncoder();
        blit->copyFromBuffer(movedVertices, 0, vertices, 0, geometryBytes);
        blit->endEncoding();
      }
      asEncoder = refit->accelerationStructureCommandEncoder();
      asEncoder->refitAccelerationStructure(structure, descriptor, refitTarget, refitScratch,
                                            extendedOffset ? 256 : 0);
      if(compactRefit)
        asEncoder->writeCompactedAccelerationStructureSize(refitTarget, compactedSize,
                                                            0, MTL::DataTypeULong);
      asEncoder->endEncoding();
      MTL::AccelerationStructure *compacted = NULL;
      MTL::CommandBuffer *finalCB = refit;
      if(compactRefit)
      {
        refit->commit(); refit->waitUntilCompleted();
        if(refit->error()) return 6;
        const uint64_t required = *(const uint64_t *)compactedSize->contents();
        if(!required || required >= refitTarget->size()) return 6;
        compacted = device->newAccelerationStructure(required);
        if(!compacted) return 5;
        MTL::CommandBuffer *compactCB = queue->commandBuffer();
        MTL::AccelerationStructureCommandEncoder *compactEncoder =
            compactCB->accelerationStructureCommandEncoder();
        compactEncoder->copyAndCompactAccelerationStructure(refitTarget, compacted);
        compactEncoder->endEncoding();
        compactCB->commit(); compactCB->waitUntilCompleted();
        if(compactCB->error()) return 6;
        finalCB = queue->commandBuffer();
      }
      compute = finalCB->computeCommandEncoder();
      const uint32_t second = 1;
      compute->setComputePipelineState(pipeline);
      compute->setAccelerationStructure(compacted ? compacted : refitTarget, 0);
      compute->setBuffer(output, 0, 1);
      compute->setBytes(&second, sizeof(second), 2);
      compute->dispatchThreads(MTL::Size::Make(1, 1, 1), MTL::Size::Make(1, 1, 1));
      compute->endEncoding();
      MTL::RenderCommandEncoder *render = finalCB->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->endEncoding();
      finalCB->presentDrawable(drawable);
      finalCB->commit(); finalCB->waitUntilCompleted();
      const uint32_t *bits = (const uint32_t *)output->contents();
      TEST_LOG("Ray refit: build=%u refit=%u AS=%zu refitScratch=%zu", bits[0], bits[1],
               size_t(structure->size()), size_t(sizes.refitScratchBufferSize));
      if(finalCB->error() || bits[0] != 1 || bits[1] != 0) return 7;
      EndCaptureFrame();
      if(compacted) compacted->release();
      if(outOfPlace) refitTarget->release();
      pool->drain();
    }
    queue->release(); scratch->release(); structure->release(); output->release();
    if(tightRefitScratch) refitScratch->release();
    if(compactedSize) compactedSize->release();
    movedVertices->release(); vertices->release(); pipeline->release(); function->release();
    if(indices) indices->release();
    if(movedIndices) movedIndices->release();
    library->release();
    return 0;
  }
};

REGISTER_TEST();
