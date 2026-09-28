// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"
#include <atomic>

RD_TEST(Metal_Async_Creation, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Native async library and five pipeline completions, retained results and descriptor snapshots.";
  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct AsyncParams { uint bias; };
kernel void cs_async(device uint *output [[buffer(0)]], constant AsyncParams &p [[buffer(1)]],
                     uint id [[thread_position_in_grid]]) { output[id] = p.bias + id; }
vertex float4 vs_async(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_async(const device uint *output [[buffer(0)]])
{ return float4(output[0], output[32], output[64], 255) / 255.0; }
)";
    dispatch_group_t group = dispatch_group_create();
    std::atomic<bool> failed{false};
    std::atomic<uint32_t> calls[7] = {};
    MTL::Library *library = NULL;
    MTL::ComputePipelineState *cp[3] = {};
    MTL::RenderPipelineState *rp[2] = {};
    NS::AutoreleasePool *setup = NS::AutoreleasePool::alloc()->init();
    dispatch_group_enter(group);
    device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), NULL,
        [&](MTL::Library *result, NS::Error *error) {
          if(calls[0].fetch_add(1) || !result || error || result->device() != device) failed = true;
          if(result) library = result->retain();
          dispatch_group_leave(group);
        });
    dispatch_group_enter(group);
    device->newLibrary(MTLSTR("not valid Metal source!"), NULL,
        [&](MTL::Library *result, NS::Error *error) {
          if(calls[1].fetch_add(1) || result || !error) failed = true;
          dispatch_group_leave(group);
        });
    if(dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC)) || !library)
      return 4;
    MTL::Function *vs = library->newFunction(MTLSTR("vs_async"));
    MTL::Function *fs = library->newFunction(MTLSTR("fs_async"));
    MTL::Function *cs = library->newFunction(MTLSTR("cs_async"));
    auto checkCompute = [&](uint32_t slot, MTL::ComputePipelineState *result,
                            MTL::ComputePipelineReflection *reflection, NS::Error *error) {
      if(calls[slot + 2].fetch_add(1) || !result || error || result->device() != device ||
         (slot > 0 && (!reflection || reflection->arguments()->count() != 2))) failed = true;
      if(result) cp[slot] = result->retain();
      dispatch_group_leave(group);
    };
    dispatch_group_enter(group);
    device->newComputePipelineState(cs, [&](MTL::ComputePipelineState *result, NS::Error *error) {
      checkCompute(0, result, NULL, error);
    });
    const auto options = MTL::PipelineOption(MTL::PipelineOptionArgumentInfo | MTL::PipelineOptionBufferTypeInfo);
    dispatch_group_enter(group);
    device->newComputePipelineState(cs, options,
        [&](MTL::ComputePipelineState *result, MTL::ComputePipelineReflection *reflection, NS::Error *error) {
          checkCompute(1, result, reflection, error);
        });
    MTL::ComputePipelineDescriptor *cd = MTL::ComputePipelineDescriptor::alloc()->init();
    cd->setComputeFunction(cs); cd->setMaxTotalThreadsPerThreadgroup(64);
    cd->setThreadGroupSizeIsMultipleOfThreadExecutionWidth(true);
    dispatch_group_enter(group);
    device->newComputePipelineState(cd, options,
        [&](MTL::ComputePipelineState *result, MTL::ComputePipelineReflection *reflection, NS::Error *error) {
          checkCompute(2, result, reflection, error);
        });
    if(cd->computeFunction() != cs) failed = true;
    cd->setMaxTotalThreadsPerThreadgroup(1); cd->release();
    for(uint32_t i = 0; i < 2; i++)
    {
      MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
      pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
      pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
      auto checkRender = [&, i](MTL::RenderPipelineState *result,
                                MTL::RenderPipelineReflection *reflection, NS::Error *error) {
        if(calls[i + 5].fetch_add(1) || !result || error || result->device() != device ||
           (i && (!reflection || reflection->fragmentArguments()->count() != 1))) failed = true;
        if(result) rp[i] = result->retain();
        dispatch_group_leave(group);
      };
      dispatch_group_enter(group);
      if(i) device->newRenderPipelineState(pd, options, checkRender);
      else device->newRenderPipelineState(pd, [checkRender](MTL::RenderPipelineState *r, NS::Error *e) {
        checkRender(r, NULL, e);
      });
      if(pd->vertexFunction() != vs || pd->fragmentFunction() != fs) failed = true;
      pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
      pd->release();
    }
    // Bridge snapshots must hold wrapped inputs, not just native pipeline compiler references.
    vs->release(); fs->release(); cs->release(); library->release();
    if(dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC))) return 4;
    for(auto &count : calls) if(count != 1) failed = true;
    setup->drain();
    for(auto *pipeline : cp) if(!pipeline) return 4;
    for(auto *pipeline : rp) if(!pipeline) return 4;
    MTL::Buffer *output = device->newBuffer(428, MTL::ResourceStorageModeShared);
    const uint32_t biases[] = {31, 83, 127};
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    uint32_t frames = 0;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = cb->blitCommandEncoder();
      blit->fillBuffer(output, NS::Range::Make(0, 428), 0); blit->endEncoding();
      MTL::ComputeCommandEncoder *compute = cb->computeCommandEncoder();
      for(uint32_t i = 0; i < 3; i++)
      {
        compute->setComputePipelineState(cp[i]); compute->setBuffer(output, i * 128, 0);
        compute->setBytes(&biases[i], 4, 1);
        compute->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(32, 1, 1));
      }
      compute->endEncoding();
      MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->setFragmentBuffer(output, 0, 0);
      for(uint32_t i = 0; i < 2; i++)
      {
        render->setRenderPipelineState(rp[i]);
        render->setScissorRect(MTL::ScissorRect{i * 200, 0, 200, 300});
        render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      }
      render->endEncoding(); cb->presentDrawable(drawable); cb->commit(); cb->waitUntilCompleted();
      EndCaptureFrame();
      if(cb->error()) failed = true;
      const uint32_t *values = (const uint32_t *)output->contents();
      for(uint32_t i = 0; i < 107; i++)
        if(values[i] != (i < 96 ? biases[i / 32] + i % 32 : 0)) failed = true;
      if(native)
        for(uint32_t x : {100U, 300U})
        {
          byte pixel[4] = {};
          drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(x, 150, 1, 1), 0);
          const byte expected[] = {127, 83, 31, 255};
          if(memcmp(pixel, expected, 4)) failed = true;
        }
      pool->drain(); frames++;
    }
    for(auto *pipeline : cp) pipeline->release();
    for(auto *pipeline : rp) pipeline->release();
    output->release(); dispatch_release(group);
    if(failed) TEST_WARN("T51 asynchronous identity/reflection/snapshot/result validation failed");
    else TEST_LOG("T51 passed 7 completions (including one error) and %u frames", frames);
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
