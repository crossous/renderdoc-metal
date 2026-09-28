// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"
#include <dispatch/dispatch.h>

RD_TEST(Metal_Stitched_Library, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "A one-node stitched visible function linked into a compute pipeline.";

  int main()
  {
    if(!Init()) return 3;
    NS::Error *error = NULL;
    MTL::Library *source = device->newLibrary(NS::String::string(R"(
#include <metal_stdlib>
using namespace metal;
[[stitchable]] float stitch_scale(float x) { return x * 2.0; }
kernel void stitched_probe(device float *out [[buffer(0)]],
                           visible_function_table<float(float)> table [[buffer(1)]],
                           uint i [[thread_position_in_grid]])
{ out[i] = table[0](float(i + 1)); }
vertex float4 stitched_vs(uint i [[vertex_id]])
{ float2 p[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[i],0,1); }
fragment float4 stitched_fs() { return float4(0.2,0.7,0.3,1); }
)", NS::UTF8StringEncoding), NULL, &error);
    if(!source) return 4;
    MTL::Function *base = source->newFunction(MTLSTR("stitch_scale"));
    MTL::FunctionStitchingInputNode *input = MTL::FunctionStitchingInputNode::alloc()->init(0);
    MTL::FunctionStitchingFunctionNode *output = MTL::FunctionStitchingFunctionNode::alloc()->init(
        MTLSTR("stitch_scale"), NS::Array::array(input), NS::Array::array());
    MTL::FunctionStitchingGraph *graph = MTL::FunctionStitchingGraph::alloc()->init(
        MTLSTR("stitched_scale"), NS::Array::array(output), output, NS::Array::array());
    MTL::StitchedLibraryDescriptor *descriptor = MTL::StitchedLibraryDescriptor::alloc()->init();
    descriptor->setFunctions(NS::Array::array(base));
    descriptor->setFunctionGraphs(NS::Array::array(graph));
    MTL::Library *stitched = NULL;
    const bool async = !GetEnvVar("RENDERDOC_METAL_STITCHED_ASYNC").empty();
    if(async)
    {
      dispatch_group_t group = dispatch_group_create();
      dispatch_group_enter(group);
      device->newLibrary(descriptor, [&](MTL::Library *result, NS::Error *failure) {
        if(result) stitched = result->retain();
        else error = failure;
        dispatch_group_leave(group);
      });
      if(dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC)))
        return 4;
    }
    else stitched = device->newLibrary(descriptor, &error);
    descriptor->release(); graph->release(); output->release(); input->release();
    if(!stitched) { TEST_WARN("Stitched library creation failed"); return 4; }
    MTL::Function *visible = stitched->newFunction(MTLSTR("stitched_scale"));
    MTL::Function *compute = source->newFunction(MTLSTR("stitched_probe"));
    MTL::Function *vertex = source->newFunction(MTLSTR("stitched_vs"));
    MTL::Function *fragment = source->newFunction(MTLSTR("stitched_fs"));
    if(!visible || !compute || !vertex || !fragment) return 4;
    MTL::ComputePipelineDescriptor *computeDescriptor = MTL::ComputePipelineDescriptor::alloc()->init();
    computeDescriptor->setComputeFunction(compute);
    MTL::LinkedFunctions *linked = MTL::LinkedFunctions::alloc()->init();
    linked->setFunctions(NS::Array::array(visible));
    computeDescriptor->setLinkedFunctions(linked);
    MTL::ComputePipelineState *computeState = device->newComputePipelineState(
        computeDescriptor, MTL::PipelineOptionArgumentInfo, NULL, &error);
    linked->release(); computeDescriptor->release();
    if(!computeState) { TEST_WARN("Stitched compute pipeline failed"); return 4; }
    MTL::FunctionHandle *handle = computeState->functionHandle(visible);
    MTL::VisibleFunctionTableDescriptor *tableDescriptor =
        MTL::VisibleFunctionTableDescriptor::visibleFunctionTableDescriptor();
    tableDescriptor->setFunctionCount(1);
    MTL::VisibleFunctionTable *table = computeState->newVisibleFunctionTable(tableDescriptor);
    if(!handle || !table) return 4;
    table->setFunction(handle, 0);
    MTL::RenderPipelineDescriptor *renderDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    renderDescriptor->setVertexFunction(vertex);
    renderDescriptor->setFragmentFunction(fragment);
    renderDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *renderState = device->newRenderPipelineState(renderDescriptor, &error);
    renderDescriptor->release();
    if(!renderState) return 4;
    MTL::Buffer *values = device->newBuffer(128, MTL::ResourceStorageModeShared);
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *command = queue->commandBuffer();
      MTL::ComputeCommandEncoder *encoder = command->computeCommandEncoder();
      encoder->setComputePipelineState(computeState);
      encoder->setBuffer(values, 0, 0);
      encoder->setVisibleFunctionTable(table, 1);
      encoder->dispatchThreads(MTL::Size::Make(32,1,1), MTL::Size::Make(32,1,1));
      encoder->endEncoding();
      MTL::RenderCommandEncoder *render = command->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0,0,0,1)));
      render->setRenderPipelineState(renderState);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      render->endEncoding();
      command->presentDrawable(drawable);
      command->commit(); command->waitUntilCompleted();
      EndCaptureFrame();
      if(command->error()) failed = true;
      if(native)
      {
        float *result = (float *)values->contents();
        for(uint32_t i = 0; i < 32; i++)
          if(result[i] != float((i + 1) * 2)) failed = true;
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel,4,MTL::Region::Make2D(200,150,1,1),0);
        if(pixel[0] < 76 || pixel[0] > 77 || pixel[1] < 178 || pixel[1] > 179 ||
           pixel[2] != 51 || pixel[3] != 255) failed = true;
      }
      pool->drain();
    }
    values->release(); renderState->release(); table->release(); computeState->release();
    fragment->release(); vertex->release(); compute->release(); visible->release();
    stitched->release(); base->release(); source->release();
    if(failed) TEST_WARN("Stitched GPU output mismatch");
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
