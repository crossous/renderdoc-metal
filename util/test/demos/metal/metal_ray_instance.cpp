// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Ray_Instance, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "A translated top-level instance misses one GPU ray and intersects another.";

  int main()
  {
    if(!Init()) return 3;
    const bool accelerationPassDescriptor =
        !GetEnvVar("RENDERDOC_METAL_T164_AS_PASS_DESCRIPTOR").empty();
    const bool clearBindings =
        !GetEnvVar("RENDERDOC_METAL_T183_CLEAR_AS_BINDINGS").empty();
    const bool instanceDescriptorAllocation =
        !GetEnvVar("RENDERDOC_METAL_T186_INSTANCE_DESCRIPTOR_ALLOCATION").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T296_THREE_DISTINCT_DESCRIPTOR").empty();
    const bool copiedPrimitive =
        !GetEnvVar("RENDERDOC_METAL_T192_COPIED_BLAS_RAY").empty();
    const bool formattedOffsets =
        !GetEnvVar("RENDERDOC_METAL_T246_FORMATTED_TRIANGLE_OFFSETS").empty();
    const bool formattedNoDuplicate =
        !GetEnvVar("RENDERDOC_METAL_T247_FORMATTED_TRIANGLE_NO_DUPLICATE").empty();
    const bool formattedIndexedOffsets =
        !GetEnvVar("RENDERDOC_METAL_T249_INDEXED_FORMATTED_OFFSETS_DESCRIPTOR").empty();
    const bool formattedIndexedFloat3 =
        !GetEnvVar("RENDERDOC_METAL_T250_INDEXED_PADDED_NO_DUPLICATE").empty();
    const bool formattedIndexed = formattedIndexedOffsets || formattedIndexedFloat3 ||
        !GetEnvVar("RENDERDOC_METAL_T248_INDEXED_FORMATTED_TRIANGLE").empty();
    const bool paddedFloat3 = formattedOffsets || formattedIndexedFloat3 ||
        !GetEnvVar("RENDERDOC_METAL_T244_PADDED_FLOAT3_TRIANGLE").empty();
    const bool float4Vertices = formattedNoDuplicate ||
        (formattedIndexed && !formattedIndexedFloat3) ||
        !GetEnvVar("RENDERDOC_METAL_T245_FLOAT4_TRIANGLE").empty();
    const bool indexedCompactPrimitive =
        !GetEnvVar("RENDERDOC_METAL_T194_INDEXED_COMPACT_BLAS_RAY").empty();
    const bool indexedOffset32 =
        !GetEnvVar("RENDERDOC_METAL_T226_INDEXED_TRIANGLE_OFFSET_UINT32").empty();
    const bool indexedExtended32 =
        !GetEnvVar("RENDERDOC_METAL_T228_INDEXED_COMBINED_OFFSETS_UINT32").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T233_INDEXED_TABLE_COMBINED_OFFSETS").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T239_INDEXED_NO_DUPLICATE_COMBINED").empty();
    const bool indexedCombined = indexedExtended32 ||
        !GetEnvVar("RENDERDOC_METAL_T227_INDEXED_COMBINED_OFFSETS").empty();
    const bool indexedVertexOnly =
        !GetEnvVar("RENDERDOC_METAL_T229_INDEXED_VERTEX_OFFSET").empty();
    const bool indexedScratchOnly =
        !GetEnvVar("RENDERDOC_METAL_T230_INDEXED_SCRATCH_OFFSET").empty();
    const bool triangleTableOffset =
        !GetEnvVar("RENDERDOC_METAL_T231_TRIANGLE_TABLE_OFFSET").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T232_INDEXED_TRIANGLE_TABLE_OFFSET").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T233_INDEXED_TABLE_COMBINED_OFFSETS").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T237_TRIANGLE_NO_DUPLICATE").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T238_INDEXED_NO_DUPLICATE").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T239_INDEXED_NO_DUPLICATE_COMBINED").empty();
    const bool noDuplicateIntersections =
        !GetEnvVar("RENDERDOC_METAL_T237_TRIANGLE_NO_DUPLICATE").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T238_INDEXED_NO_DUPLICATE").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T239_INDEXED_NO_DUPLICATE_COMBINED").empty();
    const bool triangleTableWrongSlot = triangleTableOffset &&
        !GetEnvVar("RENDERDOC_METAL_TABLE_OFFSET_WRONG_SLOT_CONTROL").empty();
    const bool indexedExtended = indexedCombined || indexedVertexOnly || indexedScratchOnly;
    const bool indexedVertexOffset = indexedCombined || indexedVertexOnly;
    const bool indexedOffset = indexedOffset32 || indexedCombined ||
        !GetEnvVar("RENDERDOC_METAL_T225_INDEXED_TRIANGLE_OFFSET").empty();
    const bool multiCompactPrimitive =
        !GetEnvVar("RENDERDOC_METAL_T195_MULTI_COMPACT_BLAS_RAY").empty();
    const bool copiedTop = !GetEnvVar("RENDERDOC_METAL_T196_COPIED_TLAS_RAY").empty();
    const bool compactTop = !GetEnvVar("RENDERDOC_METAL_T197_COMPACT_TLAS_RAY").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T297_FOUR_DISTINCT_COMPACT").empty();
    const bool compactPrimitive = indexedCompactPrimitive || multiCompactPrimitive ||
        !GetEnvVar("RENDERDOC_METAL_T193_COMPACT_BLAS_RAY").empty();
    const bool opaqueDescriptor =
        indexedOffset32 || indexedExtended32 ||
        !GetEnvVar("RENDERDOC_METAL_T166_OPAQUE_DESCRIPTOR").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T169_INDEXED_OPAQUE_DESCRIPTOR").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T171_INDEXED_OPAQUE_DESCRIPTOR_UINT32").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T174_VERTEX_OFFSET_DESCRIPTOR").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T177_COMBINED_OFFSETS_DESCRIPTOR").empty();
    const bool indexedOpaque =
        ((indexedOffset || indexedExtended) && !triangleTableOffset) ||
        !GetEnvVar("RENDERDOC_METAL_T167_INDEXED_OPAQUE_TRIANGLE").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T168_INDEXED_OPAQUE_UINT32").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T169_INDEXED_OPAQUE_DESCRIPTOR").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T171_INDEXED_OPAQUE_DESCRIPTOR_UINT32").empty();
    const bool indexedOpaque32 =
        indexedOffset32 || indexedExtended32 ||
        !GetEnvVar("RENDERDOC_METAL_T168_INDEXED_OPAQUE_UINT32").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T171_INDEXED_OPAQUE_DESCRIPTOR_UINT32").empty();
    const bool indexedNonOpaque =
        !GetEnvVar("RENDERDOC_METAL_T232_INDEXED_TRIANGLE_TABLE_OFFSET").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T233_INDEXED_TABLE_COMBINED_OFFSETS").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T238_INDEXED_NO_DUPLICATE").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T239_INDEXED_NO_DUPLICATE_COMBINED").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T170_INDEXED_NONOPAQUE_TRIANGLE").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T172_INDEXED_NONOPAQUE_UINT32").empty();
    const bool indexedGeometry = indexedOpaque || indexedNonOpaque || formattedIndexed;
    const bool indexedNonOpaque32 =
        !GetEnvVar("RENDERDOC_METAL_T172_INDEXED_NONOPAQUE_UINT32").empty();
    const bool indexed32 = indexedOpaque32 || indexedNonOpaque32;
    const bool vertexOffsetTriangle =
        indexedVertexOffset ||
        !GetEnvVar("RENDERDOC_METAL_T173_VERTEX_OFFSET_TRIANGLE").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T174_VERTEX_OFFSET_DESCRIPTOR").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T176_COMBINED_OFFSETS").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T177_COMBINED_OFFSETS_DESCRIPTOR").empty();
    const bool scratchOffsetTriangle =
        indexedCombined || indexedScratchOnly ||
        !GetEnvVar("RENDERDOC_METAL_T175_SCRATCH_OFFSET_TRIANGLE").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T176_COMBINED_OFFSETS").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T177_COMBINED_OFFSETS_DESCRIPTOR").empty();
    const bool opaqueGeometry = !triangleTableOffset &&
        (opaqueDescriptor || vertexOffsetTriangle || scratchOffsetTriangle || indexedOpaque ||
         !GetEnvVar("RENDERDOC_METAL_T165_OPAQUE_TRIANGLE").empty());
    const bool fragmentIntersectionRange =
        !GetEnvVar("RENDERDOC_METAL_T151_FRAGMENT_INTERSECTION_RANGE").empty();
    const bool vertexIntersectionRange =
        !GetEnvVar("RENDERDOC_METAL_T152_VERTEX_INTERSECTION_RANGE").empty();
    const bool tileIntersectionRange =
        !GetEnvVar("RENDERDOC_METAL_T153_TILE_INTERSECTION_RANGE").empty();
    const bool opaqueTriangleRange =
        !GetEnvVar("RENDERDOC_METAL_T157_OPAQUE_INTERSECTION_RANGE").empty();
    const bool opaqueTriangle = opaqueTriangleRange ||
        !GetEnvVar("RENDERDOC_METAL_T156_OPAQUE_INTERSECTION").empty();
    const bool emptyIntersection =
        !GetEnvVar("RENDERDOC_METAL_T156_EMPTY_INTERSECTION").empty();
    const bool intersectionBufferRange =
        !GetEnvVar("RENDERDOC_METAL_T160_INTERSECTION_BUFFER_RANGE").empty();
    const bool nestedVisibleRange =
        !GetEnvVar("RENDERDOC_METAL_T162_INTERSECTION_VISIBLE_RANGE").empty();
    const bool nestedVisibleResidency =
        !GetEnvVar("RENDERDOC_METAL_T163_INTERSECTION_VISIBLE_RESIDENCY").empty();
    const bool nestedVisible = nestedVisibleRange ||
        !GetEnvVar("RENDERDOC_METAL_T161_INTERSECTION_VISIBLE").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T161_ARG_ZERO").empty() || nestedVisibleResidency;
    const bool intersectionBuffer = triangleTableOffset || intersectionBufferRange ||
        !GetEnvVar("RENDERDOC_METAL_T159_INTERSECTION_BUFFER").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T159_ARG_ZERO").empty() || nestedVisible;
    const bool intersectionArgZero =
        !GetEnvVar("RENDERDOC_METAL_T159_ARG_ZERO").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T161_ARG_ZERO").empty();
    const bool vertexIntersection = vertexIntersectionRange ||
        !GetEnvVar("RENDERDOC_METAL_T149_VERTEX_INTERSECTION").empty();
    const bool tileIntersection = tileIntersectionRange ||
        !GetEnvVar("RENDERDOC_METAL_T150_TILE_INTERSECTION").empty();
    const bool vertexRay = vertexIntersection ||
                           !GetEnvVar("RENDERDOC_METAL_T146_VERTEX_AS").empty();
    const bool tileRay = tileIntersection ||
                         !GetEnvVar("RENDERDOC_METAL_T147_TILE_AS").empty();
    const bool customIntersection = fragmentIntersectionRange || vertexIntersection || tileIntersection ||
        !GetEnvVar("RENDERDOC_METAL_T148_INTERSECTION_TABLE").empty() || intersectionBuffer ||
        opaqueGeometry || indexedNonOpaque;
    const bool useIntersectionTable = customIntersection || opaqueTriangle || emptyIntersection;
    const bool renderRay = (useIntersectionTable && !tileIntersection) || vertexRay ||
                           (clearBindings && !tileRay) ||
                           !GetEnvVar("RENDERDOC_METAL_T145_FRAGMENT_AS").empty();
    const bool fourDistinct =
        !GetEnvVar("RENDERDOC_METAL_T295_FOUR_DISTINCT_INSTANCES").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T297_FOUR_DISTINCT_COMPACT").empty();
    const bool fiveRepeated =
        !GetEnvVar("RENDERDOC_METAL_T300_FIVE_INSTANCES_TWO_CHILDREN").empty();
    const bool repeatedTwo = fiveRepeated ||
        !GetEnvVar("RENDERDOC_METAL_T299_THREE_INSTANCES_TWO_CHILDREN").empty();
    const bool distinct = paddedFloat3 || float4Vertices || tileRay || renderRay ||
                          fourDistinct || repeatedTwo ||
                          !GetEnvVar("RENDERDOC_METAL_T296_THREE_DISTINCT_DESCRIPTOR").empty() ||
                          !GetEnvVar("RENDERDOC_METAL_T294_THREE_DISTINCT_INSTANCES").empty() ||
                          !GetEnvVar("RENDERDOC_METAL_T144_DISTINCT").empty();
    const bool fourInstances =
        fourDistinct || !GetEnvVar("RENDERDOC_METAL_T291_FOUR_INSTANCES").empty();
    const bool eightInstances =
        !GetEnvVar("RENDERDOC_METAL_T298_EIGHT_INSTANCES").empty();
    const bool threeDistinct = fourDistinct ||
        !GetEnvVar("RENDERDOC_METAL_T296_THREE_DISTINCT_DESCRIPTOR").empty() ||
        !GetEnvVar("RENDERDOC_METAL_T294_THREE_DISTINCT_INSTANCES").empty();
    const bool threeInstances = repeatedTwo || eightInstances || fourInstances ||
        !GetEnvVar("RENDERDOC_METAL_T290_THREE_INSTANCES").empty();
    const bool multiple = threeInstances || threeDistinct || distinct ||
        !GetEnvVar("RENDERDOC_METAL_T143_MULTI").empty();
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
using namespace metal::raytracing;
kernel void trace_instance(acceleration_structure<instancing> structure [[buffer(0)]],
                           device uint *result [[buffer(1)]],
                           constant uint &phase [[buffer(2)]],
                           constant uint &variant [[buffer(3)]])
{
  intersector<triangle_data, instancing> query;
  float x = variant == 6 ? (phase == 0 ? -2.0f : phase == 1 ? 3.0f :
                            phase == 2 ? 0.0f : phase == 3 ? 4.0f : 7.0f) :
            variant == 5 ? (phase == 0 ? -2.0f : phase == 1 ? 2.0f :
                            phase == 2 ? 0.0f : float(phase * 2 - 2)) :
            variant >= 3 ? (phase == 0 ? -2.0f : phase == 1 ? 0.0f :
                            phase == 2 ? 3.0f : 4.0f) :
            variant == 2 ? (phase == 0 ? -2.0f : float(phase + 1)) :
                          (float(phase) - (variant ? 1.0f : 0.0f)) * 2.0f;
  ray testRay(float3(x, 0, -1), float3(0, 0, 1));
  auto hit = query.intersect(testRay, structure);
  result[phase] = hit.type == intersection_type::triangle ? 1 : 0;
}
vertex float4 ray_vertex(uint vertexID [[vertex_id]])
{
  float2 positions[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
  return float4(positions[vertexID], 0, 1);
}
fragment float4 ray_fragment(acceleration_structure<instancing> structure [[buffer(0)]],
                             device uint *result [[buffer(1)]])
{
  intersector<triangle_data, instancing> query;
  ray testRay(float3(3, 0, -1), float3(0, 0, 1));
  auto hit = query.intersect(testRay, structure);
  uint value = hit.type == intersection_type::triangle ? 1 : 0;
  result[0] = value;
  return value ? float4(0, 1, 0, 1) : float4(1, 0, 0, 1);
}
[[intersection(triangle, triangle_data, instancing)]]
bool reject_triangle()
{
  return false;
}
[[intersection(triangle, triangle_data, instancing)]]
bool allow_triangle(device const uint *allow [[buffer(0)]])
{
  return allow[0] != 0;
}
[[visible]] bool visible_allow(uint value) { return value != 0; }
[[intersection(triangle, triangle_data, instancing)]]
bool nested_visible_triangle(device const uint *allow [[buffer(0)]],
    visible_function_table<bool(uint)> table [[buffer(1)]])
{
  return table[0](allow[0]);
}
fragment float4 ray_fragment_table(acceleration_structure<instancing> structure [[buffer(0)]],
                                   device uint *result [[buffer(1)]],
                                   intersection_function_table<triangle_data, instancing> table [[buffer(2)]])
{
  intersector<triangle_data, instancing> query;
  ray testRay(float3(3, 0, -1), float3(0, 0, 1));
  auto hit = query.intersect(testRay, structure, table);
  uint value = hit.type == intersection_type::triangle ? 1 : 0;
  result[0] = value;
  return value ? float4(0, 1, 0, 1) : float4(1, 0, 0, 1);
}
struct RayVertexOutput
{
  float4 position [[position]];
  float4 color;
};
vertex RayVertexOutput ray_vertex_as(acceleration_structure<instancing> structure [[buffer(0)]],
                                      device uint *result [[buffer(1)]],
                                      uint vertexID [[vertex_id]])
{
  intersector<triangle_data, instancing> query;
  ray testRay(float3(3, 0, -1), float3(0, 0, 1));
  auto hit = query.intersect(testRay, structure);
  uint value = hit.type == intersection_type::triangle ? 1 : 0;
  result[0] = value;
  float2 positions[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
  RayVertexOutput output;
  output.position = float4(positions[vertexID], 0, 1);
  output.color = value ? float4(0, 1, 0, 1) : float4(1, 0, 0, 1);
  return output;
}
vertex RayVertexOutput ray_vertex_table(acceleration_structure<instancing> structure [[buffer(0)]],
                                         device uint *result [[buffer(1)]],
                                         intersection_function_table<triangle_data, instancing> table [[buffer(2)]],
                                         uint vertexID [[vertex_id]])
{
  intersector<triangle_data, instancing> query;
  ray testRay(float3(3, 0, -1), float3(0, 0, 1));
  auto hit = query.intersect(testRay, structure, table);
  uint value = hit.type == intersection_type::triangle ? 1 : 0;
  result[0] = value;
  float2 positions[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
  RayVertexOutput output;
  output.position = float4(positions[vertexID], 0, 1);
  output.color = value ? float4(0, 1, 0, 1) : float4(1, 0, 0, 1);
  return output;
}
fragment float4 ray_vertex_fragment(RayVertexOutput input [[stage_in]])
{
  return input.color;
}
kernel void ray_tile(acceleration_structure<instancing> structure [[buffer(0)]],
                     device uint *result [[buffer(1)]],
                     ushort2 tid [[thread_position_in_threadgroup]])
{
  if(tid.x == 0 && tid.y == 0)
  {
    intersector<triangle_data, instancing> query;
    ray testRay(float3(3, 0, -1), float3(0, 0, 1));
    auto hit = query.intersect(testRay, structure);
    result[0] = hit.type == intersection_type::triangle ? 1 : 0;
  }
}
kernel void ray_tile_table(acceleration_structure<instancing> structure [[buffer(0)]],
                           device uint *result [[buffer(1)]],
                           intersection_function_table<triangle_data, instancing> table [[buffer(2)]],
                           ushort2 tid [[thread_position_in_threadgroup]])
{
  if(tid.x == 0 && tid.y == 0)
  {
    intersector<triangle_data, instancing> query;
    ray testRay(float3(3, 0, -1), float3(0, 0, 1));
    auto hit = query.intersect(testRay, structure, table);
    result[0] = hit.type == intersection_type::triangle ? 1 : 0;
  }
}
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!library)
    {
      TEST_WARN("Ray instance library: %s", error ? error->localizedDescription()->utf8String() : "nil");
      return 4;
    }
    MTL::Function *function = library->newFunction(MTLSTR("trace_instance"));
    MTL::ComputePipelineState *pipeline = device->newComputePipelineState(function, &error);
    if(!pipeline)
    {
      TEST_WARN("Ray instance pipeline: %s", error ? error->localizedDescription()->utf8String() : "nil");
      return 4;
    }
    MTL::Function *vertexFunction = renderRay ? library->newFunction(
        NS::String::string(vertexIntersection ? "ray_vertex_table" :
                           vertexRay ? "ray_vertex_as" : "ray_vertex", NS::UTF8StringEncoding)) : NULL;
    MTL::Function *fragmentFunction = renderRay ? library->newFunction(
        NS::String::string(vertexRay ? "ray_vertex_fragment" :
                           useIntersectionTable ? "ray_fragment_table" : "ray_fragment",
                           NS::UTF8StringEncoding)) : NULL;
    MTL::RenderPipelineState *renderPipeline = NULL;
    MTL::Function *intersectionFunction = customIntersection ?
        library->newFunction(NS::String::string(
            nestedVisible ? "nested_visible_triangle" :
            intersectionBuffer ? "allow_triangle" : "reject_triangle",
            NS::UTF8StringEncoding)) : NULL;
    MTL::Function *nestedVisibleFunction = nestedVisible ?
        library->newFunction(MTLSTR("visible_allow")) : NULL;
    MTL::FunctionHandle *intersectionHandle = NULL;
    MTL::IntersectionFunctionTable *intersectionTable = NULL;
    MTL::FunctionHandle *nestedVisibleHandle = NULL;
    MTL::VisibleFunctionTable *nestedVisibleTable = NULL;
    const uint32_t allowValue = intersectionArgZero ? 0 : 1;
    MTL::Buffer *intersectionArg = intersectionBuffer ?
        device->newBuffer(&allowValue, sizeof(allowValue), MTL::ResourceStorageModeShared) : NULL;
    MTL::Function *tileFunction = tileRay ? library->newFunction(
        NS::String::string(tileIntersection ? "ray_tile_table" : "ray_tile",
                           NS::UTF8StringEncoding)) : NULL;
    MTL::RenderPipelineState *tilePipeline = NULL;
    if(renderRay)
    {
      MTL::RenderPipelineDescriptor *renderDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
      renderDescriptor->setVertexFunction(vertexFunction);
      renderDescriptor->setFragmentFunction(fragmentFunction);
      if(customIntersection)
      {
        MTL::LinkedFunctions *linked = MTL::LinkedFunctions::linkedFunctions();
        if(nestedVisible)
        {
          const NS::Object *functions[] = {intersectionFunction, nestedVisibleFunction};
          linked->setFunctions(NS::Array::array(functions, 2));
        }
        else
          linked->setFunctions(NS::Array::array(intersectionFunction));
        if(vertexIntersection)
          renderDescriptor->setVertexLinkedFunctions(linked);
        else
          renderDescriptor->setFragmentLinkedFunctions(linked);
      }
      renderDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
      renderPipeline = device->newRenderPipelineState(renderDescriptor, &error);
      renderDescriptor->release();
      if(!renderPipeline)
      {
        TEST_WARN("Ray fragment pipeline: %s", error ? error->localizedDescription()->utf8String() : "nil");
        return 4;
      }
      if(useIntersectionTable)
      {
        if(nestedVisible)
        {
          MTL::VisibleFunctionTableDescriptor *visibleDescriptor =
              MTL::VisibleFunctionTableDescriptor::visibleFunctionTableDescriptor();
          visibleDescriptor->setFunctionCount(1);
          nestedVisibleTable = renderPipeline->newVisibleFunctionTable(
              visibleDescriptor, MTL::RenderStageFragment);
          nestedVisibleHandle = renderPipeline->functionHandle(nestedVisibleFunction,
                                                               MTL::RenderStageFragment);
          if(!nestedVisibleTable || !nestedVisibleHandle) return 4;
          nestedVisibleTable->setFunction(nestedVisibleHandle, 0);
        }
        MTL::IntersectionFunctionTableDescriptor *tableDescriptor =
            MTL::IntersectionFunctionTableDescriptor::intersectionFunctionTableDescriptor();
        tableDescriptor->setFunctionCount(triangleTableOffset ? 2 : 1);
        const MTL::RenderStages tableStage = vertexIntersection ?
            MTL::RenderStageVertex : MTL::RenderStageFragment;
        intersectionTable = renderPipeline->newIntersectionFunctionTable(tableDescriptor,
                                                                          tableStage);
        if(!intersectionTable) return 4;
        if(customIntersection)
        {
          intersectionHandle = renderPipeline->functionHandle(intersectionFunction, tableStage);
          if(!intersectionHandle) return 4;
          intersectionTable->setFunction(intersectionHandle,
                                         triangleTableOffset && !triangleTableWrongSlot ? 1 : 0);
          if(intersectionBufferRange)
          {
            const MTL::Buffer *buffers[] = {intersectionArg};
            const NS::UInteger offsets[] = {0};
            intersectionTable->setBuffers(buffers, offsets, NS::Range::Make(0, 1));
          }
          else if(intersectionBuffer)
            intersectionTable->setBuffer(intersectionArg, 0, 0);
          if(nestedVisibleRange)
          {
            const MTL::VisibleFunctionTable *tables[] = {nestedVisibleTable};
            intersectionTable->setVisibleFunctionTables(tables, NS::Range::Make(1, 1));
          }
          else if(nestedVisible)
            intersectionTable->setVisibleFunctionTable(nestedVisibleTable, 1);
        }
        else if(opaqueTriangleRange)
          intersectionTable->setOpaqueTriangleIntersectionFunction(
              (MTL::IntersectionFunctionSignature)(MTL::IntersectionFunctionSignatureInstancing |
                  MTL::IntersectionFunctionSignatureTriangleData), NS::Range::Make(0, 1));
        else if(opaqueTriangle)
          intersectionTable->setOpaqueTriangleIntersectionFunction(
              (MTL::IntersectionFunctionSignature)(MTL::IntersectionFunctionSignatureInstancing |
                  MTL::IntersectionFunctionSignatureTriangleData), 0);
      }
    }
    if(tileRay)
    {
      MTL::TileRenderPipelineDescriptor *tileDescriptor =
          MTL::TileRenderPipelineDescriptor::alloc()->init();
      tileDescriptor->setTileFunction(tileFunction);
      if(tileIntersection)
      {
        MTL::LinkedFunctions *linked = MTL::LinkedFunctions::linkedFunctions();
        linked->setFunctions(NS::Array::array(intersectionFunction));
        tileDescriptor->setLinkedFunctions(linked);
      }
      tileDescriptor->setRasterSampleCount(1);
      tileDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
      tilePipeline = device->newRenderPipelineState(tileDescriptor, MTL::PipelineOptionNone,
                                                    NULL, &error);
      tileDescriptor->release();
      if(!tilePipeline)
      {
        TEST_WARN("Ray tile pipeline: %s", error ? error->localizedDescription()->utf8String() : "nil");
        return 4;
      }
      if(tileIntersection)
      {
        MTL::IntersectionFunctionTableDescriptor *tableDescriptor =
            MTL::IntersectionFunctionTableDescriptor::intersectionFunctionTableDescriptor();
        tableDescriptor->setFunctionCount(1);
        intersectionTable = tilePipeline->newIntersectionFunctionTable(
            tableDescriptor, MTL::RenderStageTile);
        intersectionHandle = tilePipeline->functionHandle(intersectionFunction,
                                                           MTL::RenderStageTile);
        if(!intersectionTable || !intersectionHandle) return 4;
        intersectionTable->setFunction(intersectionHandle, 0);
      }
    }
    const float verticesData[9] = {-1, -1, 0, 1, -1, 0, 0, 1, 0};
    const float multiVerticesData[18] = {-1, -1, 0, 1, -1, 0, 0, 1, 0,
                                         100, -1, 0, 102, -1, 0, 101, 1, 0};
    MTL::Buffer *vertices = device->newBuffer(multiCompactPrimitive ?
                                              (const void *)multiVerticesData :
                                              (const void *)verticesData,
                                              multiCompactPrimitive ? sizeof(multiVerticesData) :
                                              sizeof(verticesData),
                                              MTL::ResourceStorageModeShared);
    const uint16_t primaryIndexData[3] = {0, 1, 2};
    MTL::Buffer *primaryIndices = indexedCompactPrimitive ?
        device->newBuffer(primaryIndexData, sizeof(primaryIndexData),
                          MTL::ResourceStorageModeShared) : NULL;
    MTL::AccelerationStructureTriangleGeometryDescriptor *triangle =
        MTL::AccelerationStructureTriangleGeometryDescriptor::descriptor();
    triangle->setVertexBuffer(vertices);
    triangle->setVertexStride(12);
    triangle->setTriangleCount(multiCompactPrimitive ? 2 : 1);
    if(indexedCompactPrimitive)
    {
      triangle->setIndexBuffer(primaryIndices);
      triangle->setIndexType(MTL::IndexTypeUInt16);
    }
    MTL::PrimitiveAccelerationStructureDescriptor *primitiveDescriptor =
        MTL::PrimitiveAccelerationStructureDescriptor::descriptor();
    primitiveDescriptor->setGeometryDescriptors(NS::Array::array(triangle));
    const MTL::AccelerationStructureSizes primitiveSizes =
        device->accelerationStructureSizes(primitiveDescriptor);
    MTL::AccelerationStructure *primitive =
        device->newAccelerationStructure(primitiveSizes.accelerationStructureSize);
    MTL::AccelerationStructure *primitiveCopy = copiedPrimitive ?
        device->newAccelerationStructure(primitiveSizes.accelerationStructureSize) : NULL;
    MTL::AccelerationStructure *primitiveCompact = compactPrimitive ?
        device->newAccelerationStructure(1280) : NULL;
    MTL::Buffer *primitiveCompactSize = compactPrimitive ?
        device->newBuffer(sizeof(uint64_t), MTL::ResourceStorageModeShared) : NULL;
    MTL::Buffer *primitiveScratch = device->newBuffer(primitiveSizes.buildScratchBufferSize,
                                                     MTL::ResourceStorageModePrivate);
    const float otherVerticesData[9] = {0, -1, 0, 2, -1, 0, 1, 1, 0};
    const float otherVerticesStrided[12] = {0, -1, 0, 99, 2, -1, 0, 99,
                                            1, 1, 0, 99};
    const float otherVerticesStridedOffset[16] = {100, 100, 100, 100,
                                                   0, -1, 0, 99, 2, -1, 0, 99,
                                                   1, 1, 0, 99};
    const float otherVerticesPadded[13] = {100, 100, 100, 100,
                                           0, -1, 0, 2, -1, 0, 1, 1, 0};
    const float otherVerticesIndexedPadded[21] = {100, 100, 100, 100, 100, 100,
                                                   100, 100, 100, 100, 100, 100,
                                                   0, -1, 0, 2, -1, 0, 1, 1, 0};
    MTL::Buffer *otherVertices = distinct ? device->newBuffer(
        (formattedOffsets || formattedIndexedOffsets) ?
            (const void *)otherVerticesStridedOffset :
        (paddedFloat3 || float4Vertices) ? (const void *)otherVerticesStrided :
        indexedVertexOffset ? (const void *)otherVerticesIndexedPadded :
        vertexOffsetTriangle ? (const void *)otherVerticesPadded : (const void *)otherVerticesData,
        (formattedOffsets || formattedIndexedOffsets) ? sizeof(otherVerticesStridedOffset) :
        (paddedFloat3 || float4Vertices) ? sizeof(otherVerticesStrided) :
        indexedVertexOffset ? sizeof(otherVerticesIndexedPadded) :
        vertexOffsetTriangle ? sizeof(otherVerticesPadded) : sizeof(otherVerticesData),
        MTL::ResourceStorageModeShared) : NULL;
    const uint16_t otherIndexData[3] = {0, 1, 2};
    const uint16_t otherIndexDataPadded[7] = {100, 100, 100, 100, 0, 1, 2};
    const uint32_t otherIndexData32[3] = {0, 1, 2};
    const uint32_t otherIndexDataPadded32[5] = {100, 100, 0, 1, 2};
    MTL::Buffer *otherIndices = indexedGeometry ? device->newBuffer(
        (indexedOffset32 || indexedExtended32 || formattedIndexedOffsets) ?
            (const void *)otherIndexDataPadded32 :
        indexed32 ? (const void *)otherIndexData32 :
                    indexedOffset ? (const void *)otherIndexDataPadded : (const void *)otherIndexData,
        (indexedOffset32 || indexedExtended32 || formattedIndexedOffsets) ?
            sizeof(otherIndexDataPadded32) :
        indexed32 ? sizeof(otherIndexData32) :
                    indexedOffset ? sizeof(otherIndexDataPadded) : sizeof(otherIndexData),
        MTL::ResourceStorageModeShared) : NULL;
    MTL::AccelerationStructureTriangleGeometryDescriptor *otherTriangle =
        MTL::AccelerationStructureTriangleGeometryDescriptor::descriptor();
    otherTriangle->setVertexBuffer(otherVertices);
    otherTriangle->setVertexStride(paddedFloat3 || float4Vertices ? 16 : 12);
    if(float4Vertices) otherTriangle->setVertexFormat(MTL::AttributeFormatFloat4);
    if(vertexOffsetTriangle || formattedOffsets || formattedIndexedOffsets)
      otherTriangle->setVertexBufferOffset(indexedVertexOffset ? 48 : 16);
    otherTriangle->setTriangleCount(1);
    if(indexedGeometry)
    {
      otherTriangle->setIndexBuffer(otherIndices);
      otherTriangle->setIndexType(indexed32 || formattedIndexedOffsets ?
                                  MTL::IndexTypeUInt32 : MTL::IndexTypeUInt16);
      if(indexedOffset || formattedIndexedOffsets) otherTriangle->setIndexBufferOffset(8);
    }
    if(useIntersectionTable) otherTriangle->setOpaque(opaqueGeometry);
    if(triangleTableOffset) otherTriangle->setIntersectionFunctionTableOffset(1);
    if(noDuplicateIntersections || formattedNoDuplicate || formattedIndexedFloat3)
      otherTriangle->setAllowDuplicateIntersectionFunctionInvocation(false);
    MTL::PrimitiveAccelerationStructureDescriptor *otherDescriptor =
        MTL::PrimitiveAccelerationStructureDescriptor::descriptor();
    otherDescriptor->setGeometryDescriptors(NS::Array::array(otherTriangle));
    const MTL::AccelerationStructureSizes otherSizes = distinct ?
        device->accelerationStructureSizes(otherDescriptor) : MTL::AccelerationStructureSizes{};
    MTL::AccelerationStructure *otherPrimitive = distinct ?
        (opaqueDescriptor || formattedIndexedOffsets ?
                            device->newAccelerationStructure(otherDescriptor) :
                            device->newAccelerationStructure(otherSizes.accelerationStructureSize)) : NULL;
    MTL::AccelerationStructure *thirdPrimitive = threeDistinct ?
        device->newAccelerationStructure(primitiveSizes.accelerationStructureSize) : NULL;
    MTL::AccelerationStructure *fourthPrimitive = fourDistinct ?
        device->newAccelerationStructure(primitiveSizes.accelerationStructureSize) : NULL;
    MTL::Buffer *otherScratch = distinct ? device->newBuffer(
        otherSizes.buildScratchBufferSize +
            (scratchOffsetTriangle || formattedOffsets || formattedIndexedOffsets ? 256 : 0),
        MTL::ResourceStorageModePrivate) : NULL;
    MTL::Buffer *thirdScratch = threeDistinct ?
        device->newBuffer(primitiveSizes.buildScratchBufferSize,
                          MTL::ResourceStorageModePrivate) : NULL;
    MTL::Buffer *fourthScratch = fourDistinct ?
        device->newBuffer(primitiveSizes.buildScratchBufferSize,
                          MTL::ResourceStorageModePrivate) : NULL;

    MTL::AccelerationStructureInstanceDescriptor instance[8] = {};
    instance[0].transformationMatrix = MTL::PackedFloat4x3(
        MTL::PackedFloat3(1, 0, 0), MTL::PackedFloat3(0, 1, 0),
        MTL::PackedFloat3(0, 0, 1), MTL::PackedFloat3(multiple ? -2 : 2, 0, 0));
    instance[0].mask = 0xff;
    instance[0].accelerationStructureIndex = 0;
    instance[1] = instance[0];
    instance[1].transformationMatrix[3] = MTL::PackedFloat3(2, 0, 0);
    if(distinct) instance[1].accelerationStructureIndex = 1;
    instance[2] = instance[0];
    instance[2].transformationMatrix[3] = MTL::PackedFloat3(0, 0, 0);
    if(threeDistinct) instance[2].accelerationStructureIndex = 2;
    instance[3] = instance[0];
    instance[3].transformationMatrix[3] = MTL::PackedFloat3(4, 0, 0);
    if(fourDistinct) instance[3].accelerationStructureIndex = 3;
    for(size_t i = 4; i < 8; i++)
    {
      instance[i] = instance[0];
      instance[i].transformationMatrix[3] = MTL::PackedFloat3(float(i * 2 - 2), 0, 0);
    }
    if(fiveRepeated) instance[4].accelerationStructureIndex = 1;
    const size_t instanceCount = eightInstances ? 8 : fiveRepeated ? 5 : fourInstances ? 4 :
                                 threeInstances || threeDistinct ? 3 :
                                 multiple ? 2 : 1;
    const size_t rayCount = threeInstances || threeDistinct ? instanceCount : multiple ? 3 : 2;
    MTL::Buffer *instances = device->newBuffer(instance, instanceCount * sizeof(instance[0]),
                                               MTL::ResourceStorageModeShared);
    MTL::InstanceAccelerationStructureDescriptor *instanceDescriptor =
        MTL::InstanceAccelerationStructureDescriptor::descriptor();
    instanceDescriptor->setInstanceDescriptorBuffer(instances);
    instanceDescriptor->setInstanceCount(instanceCount);
    if(fourDistinct)
    {
      const NS::Object *children[] = {primitive, otherPrimitive, thirdPrimitive, fourthPrimitive};
      instanceDescriptor->setInstancedAccelerationStructures(NS::Array::array(children, 4));
    }
    else if(threeDistinct)
    {
      const NS::Object *children[] = {primitive, otherPrimitive, thirdPrimitive};
      instanceDescriptor->setInstancedAccelerationStructures(NS::Array::array(children, 3));
    }
    else if(distinct)
    {
      const NS::Object *children[] = {primitive, otherPrimitive};
      instanceDescriptor->setInstancedAccelerationStructures(NS::Array::array(children, 2));
    }
    else
      instanceDescriptor->setInstancedAccelerationStructures(
          NS::Array::array(compactPrimitive ? primitiveCompact :
                           copiedPrimitive ? primitiveCopy : primitive));
    const MTL::AccelerationStructureSizes topSizes =
        device->accelerationStructureSizes(instanceDescriptor);
    if(instanceDescriptorAllocation)
    {
      const MTL::SizeAndAlign layout = device->heapAccelerationStructureSizeAndAlign(instanceDescriptor);
      if(layout.size < topSizes.accelerationStructureSize || !layout.align) return 5;
    }
    MTL::AccelerationStructure *top =
        instanceDescriptorAllocation ? device->newAccelerationStructure(instanceDescriptor) :
                                       device->newAccelerationStructure(topSizes.accelerationStructureSize);
    MTL::AccelerationStructure *topCopy = copiedTop ?
        device->newAccelerationStructure(topSizes.accelerationStructureSize) : NULL;
    MTL::Buffer *topCompactSize = compactTop ?
        device->newBuffer(sizeof(uint64_t), MTL::ResourceStorageModeShared) : NULL;
    MTL::Buffer *topScratch = device->newBuffer(topSizes.buildScratchBufferSize,
                                               MTL::ResourceStorageModePrivate);
    MTL::Buffer *output = device->newBuffer(rayCount * sizeof(uint32_t),
                                           MTL::ResourceStorageModeShared);
    MTL::Buffer *fragmentOutput = (renderRay || tileRay) ? device->newBuffer(sizeof(uint32_t),
        MTL::ResourceStorageModeShared) : NULL;
    if((distinct && (!otherVertices || !otherPrimitive || !otherScratch)) ||
       (threeDistinct && (!thirdPrimitive || !thirdScratch)) ||
       (fourDistinct && (!fourthPrimitive || !fourthScratch)) ||
       (indexedGeometry && !otherIndices) ||
       !vertices || (indexedCompactPrimitive && !primaryIndices) ||
       !primitive || (copiedPrimitive && !primitiveCopy) ||
       (compactPrimitive && (!primitiveCompact || !primitiveCompactSize)) ||
       !primitiveScratch || !instances || !top || (copiedTop && !topCopy) ||
       (compactTop && !topCompactSize) ||
       !topScratch || !output || ((renderRay || tileRay) && !fragmentOutput)) return 5;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      memset(output->contents(), 0, rayCount * sizeof(uint32_t));
      ((uint32_t *)output->contents())[0] = 7;
      ((uint32_t *)output->contents())[1] = 9;
      if(multiple) ((uint32_t *)output->contents())[2] = 11;
      for(size_t i = 3; i < rayCount; i++)
        ((uint32_t *)output->contents())[i] = uint32_t(7 + 2 * i);
      if(renderRay || tileRay) *(uint32_t *)fragmentOutput->contents() = 7;
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *buildPrimitive = queue->commandBuffer();
      MTL::AccelerationStructureCommandEncoder *asEncoder =
          accelerationPassDescriptor ? buildPrimitive->accelerationStructureCommandEncoder(
              MTL::AccelerationStructurePassDescriptor::accelerationStructurePassDescriptor()) :
              buildPrimitive->accelerationStructureCommandEncoder();
      asEncoder->buildAccelerationStructure(primitive, primitiveDescriptor,
                                            primitiveScratch, 0);
      if(copiedPrimitive) asEncoder->copyAccelerationStructure(primitive, primitiveCopy);
      if(compactPrimitive)
        asEncoder->writeCompactedAccelerationStructureSize(primitive, primitiveCompactSize,
                                                            0, MTL::DataTypeULong);
      if(distinct)
        asEncoder->buildAccelerationStructure(otherPrimitive, otherDescriptor,
                                              otherScratch,
                                              scratchOffsetTriangle || formattedOffsets ||
                                                  formattedIndexedOffsets ? 256 : 0);
      if(threeDistinct)
        asEncoder->buildAccelerationStructure(thirdPrimitive, primitiveDescriptor,
                                              thirdScratch, 0);
      if(fourDistinct)
        asEncoder->buildAccelerationStructure(fourthPrimitive, primitiveDescriptor,
                                              fourthScratch, 0);
      asEncoder->endEncoding();
      buildPrimitive->commit(); buildPrimitive->waitUntilCompleted();
      if(buildPrimitive->error()) return 6;
      if(compactPrimitive)
      {
        const uint64_t required = *(const uint64_t *)primitiveCompactSize->contents();
        if(required != primitiveCompact->size()) return 6;
        MTL::CommandBuffer *compactCB = queue->commandBuffer();
        MTL::AccelerationStructureCommandEncoder *compactEncoder =
            compactCB->accelerationStructureCommandEncoder();
        compactEncoder->copyAndCompactAccelerationStructure(primitive, primitiveCompact);
        compactEncoder->endEncoding();
        compactCB->commit(); compactCB->waitUntilCompleted();
        if(compactCB->error()) return 6;
      }
      MTL::CommandBuffer *buildTop = queue->commandBuffer();
      asEncoder = accelerationPassDescriptor ? buildTop->accelerationStructureCommandEncoder(
          MTL::AccelerationStructurePassDescriptor::accelerationStructurePassDescriptor()) :
          buildTop->accelerationStructureCommandEncoder();
      asEncoder->buildAccelerationStructure(top, instanceDescriptor, topScratch, 0);
      if(copiedTop) asEncoder->copyAccelerationStructure(top, topCopy);
      if(compactTop)
        asEncoder->writeCompactedAccelerationStructureSize(top, topCompactSize,
                                                            0, MTL::DataTypeULong);
      asEncoder->endEncoding();
      MTL::AccelerationStructure *topCompact = NULL;
      MTL::CommandBuffer *rayCB = buildTop;
      if(compactTop)
      {
        buildTop->commit(); buildTop->waitUntilCompleted();
        if(buildTop->error()) return 6;
        const uint64_t required = *(const uint64_t *)topCompactSize->contents();
        if(!required || required >= top->size()) return 6;
        topCompact = device->newAccelerationStructure(required);
        if(!topCompact) return 5;
        MTL::CommandBuffer *compactCB = queue->commandBuffer();
        MTL::AccelerationStructureCommandEncoder *compactEncoder =
            compactCB->accelerationStructureCommandEncoder();
        compactEncoder->copyAndCompactAccelerationStructure(top, topCompact);
        compactEncoder->endEncoding();
        compactCB->commit(); compactCB->waitUntilCompleted();
        if(compactCB->error()) return 6;
        rayCB = queue->commandBuffer();
      }
      MTL::AccelerationStructure *rayStructure = compactTop ? topCompact :
          copiedTop ? topCopy : top;
      MTL::ComputeCommandEncoder *compute = rayCB->computeCommandEncoder();
      compute->setComputePipelineState(pipeline);
      compute->setAccelerationStructure(rayStructure, 0);
      compute->setBuffer(output, 0, 1);
      const uint32_t variant = repeatedTwo ? 6 : eightInstances ? 5 :
                               fourDistinct ? 4 : threeDistinct ? 3 :
                               distinct ? 2 : (multiple ? 1 : 0);
      compute->setBytes(&variant, sizeof(variant), 3);
      for(uint32_t phase = 0; phase < rayCount; phase++)
      {
        compute->setBytes(&phase, sizeof(phase), 2);
        compute->dispatchThreads(MTL::Size::Make(1, 1, 1), MTL::Size::Make(1, 1, 1));
      }
      if(clearBindings) compute->setAccelerationStructure(nullptr, 0);
      compute->endEncoding();
      MTL::RenderPassDescriptor *pass = MakeBackbufferRenderPass(
          drawable, MTL::ClearColor::Make(0, 0, 0, 1));
      if(tileRay) { pass->setTileWidth(16); pass->setTileHeight(16); }
      MTL::RenderCommandEncoder *render = rayCB->renderCommandEncoder(pass);
      if(renderRay)
      {
        render->setRenderPipelineState(renderPipeline);
        if(intersectionArg)
          render->useResource(intersectionArg, MTL::ResourceUsageRead,
                              vertexIntersection ? MTL::RenderStageVertex : MTL::RenderStageFragment);
        if(nestedVisibleResidency)
          render->useResource(nestedVisibleTable, MTL::ResourceUsageRead,
                              MTL::RenderStageFragment);
        if(vertexRay)
        {
          render->setVertexAccelerationStructure(rayStructure, 0);
          render->setVertexBuffer(fragmentOutput, 0, 1);
          if(vertexIntersectionRange)
          {
            const MTL::IntersectionFunctionTable *tables[] = {intersectionTable};
            render->setVertexIntersectionFunctionTables(tables, NS::Range::Make(2, 1));
          }
          else if(vertexIntersection)
            render->setVertexIntersectionFunctionTable(intersectionTable, 2);
        }
        else
        {
          render->setFragmentAccelerationStructure(rayStructure, 0);
          render->setFragmentBuffer(fragmentOutput, 0, 1);
          if(fragmentIntersectionRange)
          {
            const MTL::IntersectionFunctionTable *tables[] = {intersectionTable};
            render->setFragmentIntersectionFunctionTables(tables, NS::Range::Make(2, 1));
          }
          else if(useIntersectionTable)
            render->setFragmentIntersectionFunctionTable(intersectionTable, 2);
        }
        render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        if(clearBindings)
        {
          if(vertexRay) render->setVertexAccelerationStructure(nullptr, 0);
          else render->setFragmentAccelerationStructure(nullptr, 0);
        }
      }
      if(tileRay)
      {
        render->setRenderPipelineState(tilePipeline);
        render->setTileAccelerationStructure(rayStructure, 0);
        render->setTileBuffer(fragmentOutput, 0, 1);
        if(tileIntersectionRange)
        {
          const MTL::IntersectionFunctionTable *tables[] = {intersectionTable};
          render->setTileIntersectionFunctionTables(tables, NS::Range::Make(2, 1));
        }
        else if(tileIntersection)
          render->setTileIntersectionFunctionTable(intersectionTable, 2);
        render->dispatchThreadsPerTile(MTL::Size::Make(16, 16, 1));
        if(clearBindings) render->setTileAccelerationStructure(nullptr, 0);
      }
      render->endEncoding();
      rayCB->presentDrawable(drawable);
      rayCB->commit(); rayCB->waitUntilCompleted();
      const uint32_t *bits = (const uint32_t *)output->contents();
      if(renderRay || tileRay)
      {
        TEST_LOG("Render-stage ray: %u vertex=%u tile=%u",
                 *(uint32_t *)fragmentOutput->contents(), unsigned(vertexRay), unsigned(tileRay));
        if(*(uint32_t *)fragmentOutput->contents() !=
           (((customIntersection && !intersectionBuffer && !opaqueGeometry) ||
             intersectionArgZero ||
             emptyIntersection || triangleTableWrongSlot) ? 0U : 1U)) return 8;
      }
      if(threeInstances || threeDistinct)
      {
        TEST_LOG("Ray %zu instances: %u,%u,%u,%u last=%u BLAS=%zu TLAS=%zu",
                 instanceCount, bits[0], bits[1], bits[2],
                 instanceCount >= 4 ? bits[3] : 0U, bits[rayCount - 1],
                 size_t(primitiveSizes.accelerationStructureSize),
                 size_t(topSizes.accelerationStructureSize));
        if(rayCB->error()) return 7;
        for(size_t i = 0; i < rayCount; i++) if(bits[i] != 1) return 7;
      }
      else if(multiple)
      {
        TEST_LOG("Ray instances: left=%u centre=%u right=%u BLAS=%zu TLAS=%zu distinct=%u",
                 bits[0], bits[1], bits[2], size_t(primitiveSizes.accelerationStructureSize),
                 size_t(topSizes.accelerationStructureSize), unsigned(distinct));
        if(rayCB->error() || bits[0] != 1 || bits[1] != 0 || bits[2] != 1) return 7;
      }
      else
      {
        TEST_LOG("Ray instance: origin0=%u origin2=%u BLAS=%zu TLAS=%zu", bits[0], bits[1],
                 size_t(primitiveSizes.accelerationStructureSize),
                 size_t(topSizes.accelerationStructureSize));
        if(rayCB->error() || bits[0] != 0 || bits[1] != 1) return 7;
      }
      EndCaptureFrame();
      if(topCompact) topCompact->release();
      pool->drain();
    }
    output->release(); topScratch->release(); top->release();
    if(topCopy) topCopy->release();
    if(topCompactSize) topCompactSize->release();
    instances->release();
    if(otherScratch) otherScratch->release();
    if(otherPrimitive) otherPrimitive->release();
    if(thirdScratch) thirdScratch->release();
    if(thirdPrimitive) thirdPrimitive->release();
    if(fourthScratch) fourthScratch->release();
    if(fourthPrimitive) fourthPrimitive->release();
    if(otherIndices) otherIndices->release();
    if(otherVertices) otherVertices->release();
    if(fragmentOutput) fragmentOutput->release();
    if(renderPipeline) renderPipeline->release();
    if(intersectionTable) intersectionTable->release();
    if(intersectionArg) intersectionArg->release();
    if(nestedVisibleTable) nestedVisibleTable->release();
    if(nestedVisibleHandle) nestedVisibleHandle->release();
    if(nestedVisibleFunction) nestedVisibleFunction->release();
    if(intersectionHandle) intersectionHandle->release();
    if(intersectionFunction) intersectionFunction->release();
    if(tilePipeline) tilePipeline->release();
    if(tileFunction) tileFunction->release();
    if(vertexFunction) vertexFunction->release();
    if(fragmentFunction) fragmentFunction->release();
    primitiveScratch->release(); primitive->release();
    if(primitiveCopy) primitiveCopy->release();
    if(primitiveCompact) primitiveCompact->release();
    if(primitiveCompactSize) primitiveCompactSize->release();
    if(primaryIndices) primaryIndices->release();
    vertices->release();
    pipeline->release(); function->release(); library->release();
    return 0;
  }
};

REGISTER_TEST();
