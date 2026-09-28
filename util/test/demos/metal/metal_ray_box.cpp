// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Ray_Box, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Custom box-ray intersections verify strided bounding-box geometry and buffer offsets.";

  int main()
  {
    if(!Init()) return 3;
    const bool refitCombined =
        !GetEnvVar("RENDERDOC_METAL_T257_BOX_REFIT_COMBINED").empty();
    const bool refitOutOfPlace =
        !GetEnvVar("RENDERDOC_METAL_T258_BOX_REFIT_OUT_OF_PLACE").empty();
    const bool refitCopiedStructure =
        !GetEnvVar("RENDERDOC_METAL_T260_BOX_REFIT_COPIED_STRUCTURE").empty();
    const bool refitAlternateBuffer =
        !GetEnvVar("RENDERDOC_METAL_T261_BOX_REFIT_ALTERNATE_BUFFER").empty();
    const bool refitCopy = refitCopiedStructure ||
        !GetEnvVar("RENDERDOC_METAL_T259_BOX_REFIT_COPY").empty();
    const bool refitBox = refitCombined || refitOutOfPlace || refitCopy ||
        refitAlternateBuffer ||
        !GetEnvVar("RENDERDOC_METAL_T256_BOX_REFIT").empty();
    const bool opaqueCombined =
        !GetEnvVar("RENDERDOC_METAL_T224_OPAQUE_BOX_COMBINED").empty();
    const bool noDuplicateCombined =
        !GetEnvVar("RENDERDOC_METAL_T235_BOX_NO_DUPLICATE_COMBINED").empty();
    const bool noDuplicateOpaque =
        !GetEnvVar("RENDERDOC_METAL_T236_OPAQUE_BOX_NO_DUPLICATE").empty();
    const bool combined = refitCombined || opaqueCombined || noDuplicateCombined ||
        !GetEnvVar("RENDERDOC_METAL_T222_BOX_RAY_COMBINED_OFFSETS").empty();
    const bool offset = combined ||
        !GetEnvVar("RENDERDOC_METAL_T219_BOX_RAY_OFFSET").empty();
    const bool tableRange = !GetEnvVar("RENDERDOC_METAL_T220_BOX_RAY_TABLE_RANGE").empty();
    const bool tableOffset = combined ||
        !GetEnvVar("RENDERDOC_METAL_T221_BOX_RAY_TABLE_OFFSET").empty();
    const bool opaqueBox = opaqueCombined || noDuplicateOpaque ||
        !GetEnvVar("RENDERDOC_METAL_T223_OPAQUE_BOX_RAY").empty();
    const bool noDuplicate = refitCombined || noDuplicateCombined || noDuplicateOpaque ||
        !GetEnvVar("RENDERDOC_METAL_T234_BOX_NO_DUPLICATE").empty();
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
using namespace metal::raytracing;
struct BoxResult
{
  bool accept [[accept_intersection]];
  float distance [[distance]];
};
[[intersection(bounding_box)]]
BoxResult hit_box(float minDistance [[min_distance]], bool opaque [[opaque]],
                  device atomic_uint *callCount [[buffer(0)]])
{
  atomic_fetch_add_explicit(callCount, 1u, memory_order_relaxed);
  BoxResult result;
  result.accept = !opaque;
  result.distance = minDistance;
  return result;
}
kernel void trace_boxes(acceleration_structure<> structure [[buffer(0)]],
                        device uint *result [[buffer(1)]],
                        intersection_function_table<> table [[buffer(2)]],
                        uint tid [[thread_position_in_grid]])
{
  intersector<> query;
  float x = tid == 0 ? 0.0f : tid == 1 ? 20.0f : 40.0f;
  ray testRay(float3(x, x, -2), float3(0, 0, 1));
  auto hit = query.intersect(testRay, structure, table);
  result[tid] = hit.type == intersection_type::bounding_box ? 1 : 0;
}
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!library)
    {
      TEST_WARN("Box ray library: %s", error ? error->localizedDescription()->utf8String() : "nil");
      return 4;
    }
    MTL::Function *function = library->newFunction(MTLSTR("trace_boxes"));
    MTL::Function *intersection = library->newFunction(MTLSTR("hit_box"));
    if(!function || !intersection) return 4;
    MTL::ComputePipelineDescriptor *pipelineDescriptor =
        MTL::ComputePipelineDescriptor::alloc()->init();
    pipelineDescriptor->setComputeFunction(function);
    MTL::LinkedFunctions *links = MTL::LinkedFunctions::linkedFunctions();
    links->setFunctions(NS::Array::array(intersection));
    pipelineDescriptor->setLinkedFunctions(links);
    MTL::ComputePipelineState *pipeline = device->newComputePipelineState(
        pipelineDescriptor, MTL::PipelineOptionNone, NULL, &error);
    pipelineDescriptor->release();
    if(!pipeline)
    {
      TEST_WARN("Box ray pipeline: %s", error ? error->localizedDescription()->utf8String() : "nil");
      return 4;
    }
    MTL::IntersectionFunctionTableDescriptor *tableDescriptor =
        MTL::IntersectionFunctionTableDescriptor::intersectionFunctionTableDescriptor();
    tableDescriptor->setFunctionCount(tableOffset ? 2 : 1);
    MTL::IntersectionFunctionTable *table = pipeline->newIntersectionFunctionTable(tableDescriptor);
    MTL::FunctionHandle *handle = pipeline->functionHandle(intersection);
    if(!table || !handle) return 4;
    table->setFunction(handle, tableOffset ? 1 : 0);
    const float pair[16] = {20, 20, 20, 21, 21, 21, 99, 99,
                            -1, -1, -1, 1, 1, 1, 99, 99};
    const float shiftedPair[16] = {20, 20, 20, 21, 21, 21, 99, 99,
                                   59, 59, 59, 61, 61, 61, 99, 99};
    const float shiftedPrefixed[28] = {40, 40, 40, 41, 41, 41,
                                       50, 50, 50, 51, 51, 51,
                                       20, 20, 20, 21, 21, 21, 99, 99,
                                       59, 59, 59, 61, 61, 61, 99, 99};
    const float prefixed[28] = {40, 40, 40, 41, 41, 41,
                                50, 50, 50, 51, 51, 51,
                                20, 20, 20, 21, 21, 21, 99, 99,
                                -1, -1, -1, 1, 1, 1, 99, 99};
    MTL::Buffer *boxes = device->newBuffer(offset ? (const void *)prefixed : (const void *)pair,
                                           offset ? sizeof(prefixed) : sizeof(pair),
                                           MTL::ResourceStorageModeShared);
    MTL::Buffer *movedBoxes = refitBox ? device->newBuffer(
        offset ? (const void *)shiftedPrefixed : (const void *)shiftedPair,
        offset ? sizeof(shiftedPrefixed) : sizeof(shiftedPair),
        MTL::ResourceStorageModeShared) : NULL;
    MTL::Buffer *initialBoxes = refitCopiedStructure ?
        device->newBuffer(pair, sizeof(pair), MTL::ResourceStorageModeShared) : NULL;
    MTL::Buffer *output = device->newBuffer((refitCopiedStructure ? 12 : refitCopy ? 9 :
                                            refitBox ? 6 : 3) * sizeof(uint32_t),
                                            MTL::ResourceStorageModeShared);
    MTL::Buffer *calls = device->newBuffer(sizeof(uint32_t), MTL::ResourceStorageModeShared);
    if(!calls) return 5;
    table->setBuffer(calls, 0, 0);
    MTL::AccelerationStructureBoundingBoxGeometryDescriptor *geometry =
        MTL::AccelerationStructureBoundingBoxGeometryDescriptor::descriptor();
    geometry->setBoundingBoxBuffer(boxes);
    geometry->setBoundingBoxBufferOffset(offset ? 48 : 0);
    geometry->setBoundingBoxStride(32);
    geometry->setBoundingBoxCount(2);
    geometry->setOpaque(opaqueBox);
    if(noDuplicate) geometry->setAllowDuplicateIntersectionFunctionInvocation(false);
    if(tableOffset) geometry->setIntersectionFunctionTableOffset(1);
    MTL::PrimitiveAccelerationStructureDescriptor *descriptor =
        MTL::PrimitiveAccelerationStructureDescriptor::descriptor();
    if(refitBox) descriptor->setUsage(MTL::AccelerationStructureUsageRefit);
    descriptor->setGeometryDescriptors(NS::Array::array(geometry));
    MTL::AccelerationStructureSizes sizes = device->accelerationStructureSizes(descriptor);
    MTL::AccelerationStructure *structure = device->newAccelerationStructure(descriptor);
    MTL::Buffer *scratch = device->newBuffer(
        std::max(sizes.buildScratchBufferSize, sizes.refitScratchBufferSize) +
            (offset ? 256 : 0),
                                             MTL::ResourceStorageModePrivate);
    MTL::CommandQueue *queue = device->newCommandQueue();
    if(!boxes || !output || !structure || !scratch || !queue ||
       (refitBox && (!movedBoxes || !sizes.refitScratchBufferSize)) ||
       (refitCopiedStructure && !initialBoxes)) return 5;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      if(refitBox) memcpy(boxes->contents(), offset ? (const void *)prefixed :
          (const void *)pair, offset ? sizeof(prefixed) : sizeof(pair));
      if(refitAlternateBuffer) geometry->setBoundingBoxBuffer(boxes);
      memset(output->contents(), 0, (refitCopiedStructure ? 12 : refitCopy ? 9 :
                                     refitBox ? 6 : 3) * sizeof(uint32_t));
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::BlitCommandEncoder *clear = cb->blitCommandEncoder();
      clear->fillBuffer(calls, NS::Range::Make(0, sizeof(uint32_t)), 0);
      clear->endEncoding();
      MTL::AccelerationStructureCommandEncoder *asEncoder = cb->accelerationStructureCommandEncoder();
      asEncoder->buildAccelerationStructure(structure, descriptor, scratch, offset ? 256 : 0);
      asEncoder->endEncoding();
      MTL::ComputeCommandEncoder *compute = cb->computeCommandEncoder();
      compute->setComputePipelineState(pipeline);
      compute->setAccelerationStructure(structure, 0);
      compute->setBuffer(output, 0, 1);
      if(tableRange)
      {
        const MTL::IntersectionFunctionTable *tables[] = {table};
        compute->setIntersectionFunctionTables(tables, NS::Range::Make(2, 1));
      }
      else compute->setIntersectionFunctionTable(table, 2);
      compute->dispatchThreads(MTL::Size::Make(3, 1, 1), MTL::Size::Make(3, 1, 1));
      compute->endEncoding();
      MTL::AccelerationStructure *refitTarget = NULL;
      MTL::AccelerationStructure *copiedTarget = NULL;
      if(refitBox)
      {
        cb->commit(); cb->waitUntilCompleted();
        if(cb->error()) return 6;
        MTL::CommandBuffer *refit = queue->commandBuffer();
        if(refitAlternateBuffer) geometry->setBoundingBoxBuffer(movedBoxes);
        else
        {
          MTL::BlitCommandEncoder *blit = refit->blitCommandEncoder();
          blit->copyFromBuffer(movedBoxes, 0, boxes, 0,
                               offset ? sizeof(shiftedPrefixed) : sizeof(shiftedPair));
          blit->endEncoding();
        }
        refitTarget = refitOutOfPlace ?
            device->newAccelerationStructure(sizes.accelerationStructureSize) : structure;
        if(!refitTarget) return 5;
        asEncoder = refit->accelerationStructureCommandEncoder();
        asEncoder->refitAccelerationStructure(structure, descriptor, refitTarget,
                                               scratch, offset ? 256 : 0);
        asEncoder->endEncoding();
        compute = refit->computeCommandEncoder();
        compute->setComputePipelineState(pipeline);
        compute->setAccelerationStructure(refitTarget, 0);
        compute->setBuffer(output, 3 * sizeof(uint32_t), 1);
        compute->setIntersectionFunctionTable(table, 2);
        compute->dispatchThreads(MTL::Size::Make(3, 1, 1), MTL::Size::Make(3, 1, 1));
        compute->endEncoding();
        if(refitCopy)
        {
          copiedTarget = device->newAccelerationStructure(sizes.accelerationStructureSize);
          if(!copiedTarget) return 5;
          asEncoder = refit->accelerationStructureCommandEncoder();
          asEncoder->copyAccelerationStructure(refitTarget, copiedTarget);
          asEncoder->endEncoding();
          compute = refit->computeCommandEncoder();
          compute->setComputePipelineState(pipeline);
          compute->setAccelerationStructure(copiedTarget, 0);
          compute->setBuffer(output, 6 * sizeof(uint32_t), 1);
          compute->setIntersectionFunctionTable(table, 2);
          compute->dispatchThreads(MTL::Size::Make(3, 1, 1), MTL::Size::Make(3, 1, 1));
          compute->endEncoding();
        }
        cb = refit;
      }
      MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->endEncoding();
      cb->presentDrawable(drawable);
      cb->commit(); cb->waitUntilCompleted();
      if(refitOutOfPlace) refitTarget->release();
      const uint32_t *hits = (const uint32_t *)output->contents();
      TEST_LOG("Box ray: %u/%u/%u offset=%u range=%u tableOffset=%u opaque=%u calls=%u noDuplicate=%u",
               hits[0], hits[1], hits[2], (uint32_t)offset, (uint32_t)tableRange,
               (uint32_t)tableOffset, (uint32_t)opaqueBox,
               *(const uint32_t *)calls->contents(), (uint32_t)noDuplicate);
      if(cb->error() || hits[0] != (opaqueBox ? 0U : 1U) ||
         hits[1] != (opaqueBox ? 0U : 1U) || hits[2] != 0) return 6;
      if(noDuplicate && !refitBox && *(const uint32_t *)calls->contents() != 2) return 6;
      if(refitBox && (hits[3] != 0 || hits[4] != 1 || hits[5] != 0)) return 6;
      if(refitCopy && (hits[6] != 0 || hits[7] != 1 || hits[8] != 0)) return 6;
      if(refitCopiedStructure)
      {
        MTL::CommandBuffer *restore = queue->commandBuffer();
        MTL::BlitCommandEncoder *blit = restore->blitCommandEncoder();
        blit->copyFromBuffer(initialBoxes, 0, boxes, 0, sizeof(pair));
        blit->endEncoding();
        asEncoder = restore->accelerationStructureCommandEncoder();
        asEncoder->refitAccelerationStructure(copiedTarget, descriptor, copiedTarget,
                                              scratch, 0);
        asEncoder->endEncoding();
        compute = restore->computeCommandEncoder();
        compute->setComputePipelineState(pipeline);
        compute->setAccelerationStructure(copiedTarget, 0);
        compute->setBuffer(output, 9 * sizeof(uint32_t), 1);
        compute->setIntersectionFunctionTable(table, 2);
        compute->dispatchThreads(MTL::Size::Make(3, 1, 1), MTL::Size::Make(3, 1, 1));
        compute->endEncoding();
        restore->commit(); restore->waitUntilCompleted();
        if(restore->error() || hits[9] != 1 || hits[10] != 1 || hits[11] != 0) return 6;
      }
      if(copiedTarget) copiedTarget->release();
      EndCaptureFrame();
      pool->drain();
    }
    queue->release(); scratch->release(); structure->release(); output->release(); calls->release();
    boxes->release();
    if(movedBoxes) movedBoxes->release();
    if(initialBoxes) initialBoxes->release();
    handle->release(); table->release(); pipeline->release(); intersection->release();
    function->release(); library->release();
    return 0;
  }
};

REGISTER_TEST();
