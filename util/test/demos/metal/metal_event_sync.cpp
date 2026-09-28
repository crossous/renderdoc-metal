// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Event_Sync, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Two Metal events across two queues and three submissions, plus same-buffer signal/wait.";
  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
kernel void cs_events(const device uchar *input [[buffer(0)]], device uint *output [[buffer(1)]],
                      uint id [[thread_position_in_grid]]) { output[id] = input[id] + 48 + id; }
vertex float4 vs_events(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_events(const device uint *output [[buffer(0)]])
{ return float4(output[0], output[32], output[64], 255) / 255.0; }
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!library) return 4;
    MTL::Function *cs = library->newFunction(MTLSTR("cs_events"));
    MTL::Function *vs = library->newFunction(MTLSTR("vs_events"));
    MTL::Function *fs = library->newFunction(MTLSTR("fs_events"));
    MTL::ComputePipelineState *compute = device->newComputePipelineState(cs, &error);
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(pd, &error);
    pd->release(); cs->release(); vs->release(); fs->release(); library->release();
    MTL::CommandQueue *otherQueue = device->newCommandQueue();
    MTL::Event *events[2];
    for(uint32_t i = 0; i < 2; i++)
    {
      events[i] = device->newEvent();
      if(!events[i] || events[i]->device() != device) return 4;
      events[i]->setLabel(i ? MTLSTR("T52 consumer ready") : MTLSTR("T52 producer ready"));
      if(!events[i]->label()) return 4;
    }
    MTL::Buffer *input = device->newBuffer(436, MTL::ResourceStorageModeShared);
    MTL::Buffer *output = device->newBuffer(444, MTL::ResourceStorageModeShared);
    if(!compute || !pipeline || !input || !output || !otherQueue) return 4;
    bool failed = false;
    uint32_t frames = 0;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      const uint64_t value = frames * 10 + 5;
      MTL::CommandBuffer *first = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = first->blitCommandEncoder();
      blit->fillBuffer(input, NS::Range::Make(0, 436), 3);
      blit->endEncoding(); first->encodeSignalEvent(events[0], value); first->commit();
      MTL::CommandBuffer *second = otherQueue->commandBuffer();
      second->encodeWait(events[0], value);
      blit = second->blitCommandEncoder();
      blit->fillBuffer(output, NS::Range::Make(0, 444), 0); blit->endEncoding();
      MTL::ComputeCommandEncoder *enc = second->computeCommandEncoder();
      enc->setComputePipelineState(compute);
      enc->setBuffer(input, 0, 0); enc->setBuffer(output, 0, 1);
      enc->dispatchThreadgroups(MTL::Size::Make(3, 1, 1), MTL::Size::Make(32, 1, 1));
      enc->endEncoding(); second->encodeSignalEvent(events[1], value + 2); second->commit();
      MTL::CommandBuffer *third = queue->commandBuffer();
      third->encodeWait(events[1], value + 2);
      MTL::RenderCommandEncoder *render = third->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->setRenderPipelineState(pipeline); render->setFragmentBuffer(output, 0, 0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      render->endEncoding(); third->encodeSignalEvent(events[0], value + 4);
      third->encodeWait(events[0], value + 4);
      third->presentDrawable(drawable); third->commit(); third->waitUntilCompleted();
      second->waitUntilCompleted(); first->waitUntilCompleted();
      EndCaptureFrame();
      if(first->error() || second->error() || third->error()) failed = true;
      const byte *inputBytes = (const byte *)input->contents();
      for(uint32_t i = 0; i < 436; i++) if(inputBytes[i] != 3) failed = true;
      const uint32_t *values = (const uint32_t *)output->contents();
      for(uint32_t i = 0; i < 111; i++) if(values[i] != (i < 96 ? 51 + i : 0)) failed = true;
      if(native)
      {
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(200, 150, 1, 1), 0);
        const byte expected[] = {115, 83, 51, 255};
        if(memcmp(pixel, expected, 4)) failed = true;
      }
      pool->drain(); frames++;
    }
    input->release(); output->release(); pipeline->release(); compute->release();
    for(auto *event : events) event->release();
    otherQueue->release();
    if(failed) TEST_WARN("T52 event identity/GPU dependency validation failed");
    else TEST_LOG("T52 passed %u frames / %u signals and waits", frames, frames * 6);
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
