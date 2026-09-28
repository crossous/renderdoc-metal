// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"
#include <atomic>
#include <memory>

RD_TEST(Metal_Command_Handlers, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Scheduled/completed callback identity, lifetime and CPU-to-GPU dependency.";

  struct CallbackState
  {
    std::atomic<uint32_t> counts[8] = {};
    std::atomic<bool> failed{false};
    dispatch_group_t group = dispatch_group_create();
    ~CallbackState() { dispatch_release(group); }
  };

  void Register(MTL::CommandBuffer *cb, uint32_t base, MTL::Buffer *producer,
                MTL::Buffer *parameters, const std::shared_ptr<CallbackState> &state)
  {
    MTL::Device *expectedDevice = device;
    MTL::CommandQueue *expectedQueue = queue;
    for(uint32_t i = 0; i < 4; i++)
    {
      const bool completed = i >= 2;
      dispatch_group_enter(state->group);
      auto handler = [=](MTL::CommandBuffer *actual) {
        if(actual != cb || actual->device() != expectedDevice ||
           actual->commandQueue() != expectedQueue || !actual->label() ||
           strcmp(actual->label()->utf8String(), base == 0 ? "T49 producer" : "T49 consumer") ||
           actual->error() || actual->status() < MTL::CommandBufferStatusScheduled ||
           actual->status() > MTL::CommandBufferStatusCompleted ||
           (completed && actual->status() != MTL::CommandBufferStatusCompleted))
          state->failed = true;
        if(state->counts[base + i].fetch_add(1) != 0) state->failed = true;
        if(base == 0 && completed)
        {
          const byte *bytes = (const byte *)producer->contents();
          for(uint32_t offset = 0; offset < 300; offset++)
            if(bytes[offset] != 29) state->failed = true;
          uint32_t *values = (uint32_t *)parameters->contents();
          if(i == 2) values[0] = bytes[0] + 12;
          else { values[1] = bytes[1] + 38; values[2] = bytes[2] + 72; }
        }
        dispatch_group_leave(state->group);
      };
      if(completed) cb->addCompletedHandler(handler);
      else cb->addScheduledHandler(handler);
    }
  }

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct Params { uint r; uint g; uint b; };
kernel void cs_handlers(device uint *result [[buffer(0)]], constant Params &p [[buffer(1)]],
                        uint id [[thread_position_in_grid]])
{
  result[id] = p.r + id;
  result[32 + id] = p.g + id;
  result[64 + id] = p.b + id;
}
vertex float4 vs_handlers(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_handlers(const device uint *result [[buffer(0)]])
{
  return float4(result[0], result[32], result[64], 255) / 255.0;
}
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!library) return 4;
    MTL::Function *cs = library->newFunction(MTLSTR("cs_handlers"));
    MTL::Function *vs = library->newFunction(MTLSTR("vs_handlers"));
    MTL::Function *fs = library->newFunction(MTLSTR("fs_handlers"));
    MTL::ComputePipelineState *compute = device->newComputePipelineState(cs, &error);
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *renderPipeline = device->newRenderPipelineState(pd, &error);
    pd->release();
    MTL::Buffer *producer = device->newBuffer(300, MTL::ResourceStorageModeShared);
    MTL::Buffer *parameters = device->newBuffer(12, MTL::ResourceStorageModeShared);
    MTL::Buffer *output = device->newBuffer(412, MTL::ResourceStorageModeShared);
    if(!compute || !renderPipeline || !producer || !parameters || !output) return 4;
    cs->release(); vs->release(); fs->release(); library->release();
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    bool failed = false;
    uint32_t frames = 0;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      const uint32_t initialParameters[] = {7, 11, 13};
      memcpy(parameters->contents(), initialParameters, 12);
      auto state = std::make_shared<CallbackState>();
      std::weak_ptr<CallbackState> weakState = state;
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *first = queue->commandBuffer();
      first->setLabel(MTLSTR("T49 producer"));
      Register(first, 0, producer, parameters, state);
      MTL::BlitCommandEncoder *blit = first->blitCommandEncoder();
      blit->fillBuffer(producer, NS::Range::Make(0, 300), 29);
      blit->fillBuffer(output, NS::Range::Make(0, 412), 0);
      blit->endEncoding();
      MTL::ComputeCommandEncoder *enc = first->computeCommandEncoder();
      enc->setComputePipelineState(compute);
      enc->setBuffer(output, 0, 0); enc->setBuffer(parameters, 0, 1);
      enc->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(32, 1, 1));
      enc->endEncoding(); first->commit(); first->waitUntilCompleted();
      if(dispatch_group_wait(state->group, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)))
        return 6;

      const uint32_t *firstValues = (const uint32_t *)output->contents();
      for(uint32_t i = 0; i < 103; i++)
        if(firstValues[i] != (i < 96 ? initialParameters[i / 32] + i % 32 : 0)) failed = true;

      MTL::CommandBuffer *second = queue->commandBuffer();
      second->setLabel(MTLSTR("T49 consumer"));
      Register(second, 4, producer, parameters, state);
      blit = second->blitCommandEncoder();
      blit->fillBuffer(output, NS::Range::Make(0, 412), 0);
      blit->endEncoding();
      enc = second->computeCommandEncoder();
      enc->setComputePipelineState(compute);
      enc->setBuffer(output, 0, 0); enc->setBuffer(parameters, 0, 1);
      enc->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(32, 1, 1));
      enc->endEncoding();
      MTL::RenderCommandEncoder *render = second->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->setRenderPipelineState(renderPipeline); render->setFragmentBuffer(output, 0, 0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      render->endEncoding(); second->presentDrawable(drawable);
      second->commit(); second->waitUntilCompleted();
      if(dispatch_group_wait(state->group, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)))
        return 6;
      EndCaptureFrame();
      for(auto &count : state->counts) if(count != 1) failed = true;
      failed |= state->failed;
      const uint32_t biases[] = {41, 67, 101};
      const uint32_t *values = (const uint32_t *)output->contents();
      for(uint32_t i = 0; i < 103; i++)
        if(values[i] != (i < 96 ? biases[i / 32] + i % 32 : 0)) failed = true;
      if(native)
      {
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(200, 150, 1, 1), 0);
        const byte expected[] = {101, 67, 41, 255};
        if(memcmp(pixel, expected, 4)) failed = true;
      }
      state.reset();
      pool->drain();
      if(!weakState.expired())
      {
        TEST_WARN("T49 callback captures were not released after command buffer completion");
        failed = true;
      }
      frames++;
    }
    producer->release(); parameters->release(); output->release();
    compute->release(); renderPipeline->release();
    if(failed) TEST_WARN("T49 callback identity/count/status/data validation failed");
    else TEST_LOG("T49 passed %u frames / %u callbacks including captured-block destruction", frames, frames * 8);
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
