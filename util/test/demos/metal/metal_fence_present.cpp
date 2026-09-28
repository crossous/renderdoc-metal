// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Fence_Present, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Untracked buffers synchronized by blit/compute/render fences, timed presents and annotations.";

  int main()
  {
    if(!Init()) return 3;
    const std::string mode = GetEnvVar("RENDERDOC_METAL_TIMED_PRESENT");
    const bool annotations = mode == "time" || mode == "duration";
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
kernel void cs_main(device uint *data [[buffer(0)]], constant uint4 &p [[buffer(1)]],
                    uint i [[thread_position_in_grid]])
{
  if(i < 76) data[i] += p.x == 0 ? i + 3 : 5;
}
vertex void vs_writer(uint id [[vertex_id]], device uint *data [[buffer(0)]])
{
  data[id] += 7;
}
vertex float4 vs_main(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_main(const device uint *data [[buffer(0)]])
{
  return float4(data[0], data[1], data[2], 255) / 255.0;
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding),
                                          NULL, &error);
    if(!lib) return 4;
    MTL::Function *cs = lib->newFunction(MTLSTR("cs_main"));
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_main"));
    MTL::Function *writerVS = lib->newFunction(MTLSTR("vs_writer"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_main"));
    MTL::ComputePipelineState *cp = device->newComputePipelineState(cs, &error);
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs);
    pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *rp = device->newRenderPipelineState(pd, &error);
    pd->setVertexFunction(writerVS);
    pd->setFragmentFunction(NULL);
    pd->setRasterizationEnabled(false);
    MTL::RenderPipelineState *writer = device->newRenderPipelineState(pd, &error);
    pd->release();
    const MTL::ResourceOptions options = MTL::ResourceOptions(
        MTL::ResourceStorageModeShared | MTL::ResourceHazardTrackingModeUntracked);
    MTL::Buffer *data = device->newBuffer(304, options);
    MTL::Buffer *readback = device->newBuffer(308, options);
    MTL::Fence *fences[4] = {};
    for(MTL::Fence *&fence : fences)
    {
      fence = device->newFence();
      if(!fence || fence->device() != device) return 4;
      fence->setLabel(MTLSTR("T44 fence"));
    }
    byte sentinel[172];
    memset(sentinel, 0x72, sizeof(sentinel));
    MTL::Buffer *markerBuffer = annotations ? device->newBuffer(sentinel, sizeof(sentinel),
                                                               MTL::ResourceStorageModeShared) : NULL;
    if(!cp || !rp || !writer || !data || !readback || (annotations && !markerBuffer)) return 4;
    if(markerBuffer) markerBuffer->addDebugMarker(MTLSTR("setup range"), NS::Range::Make(0, 16));
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      if(markerBuffer)
      {
        markerBuffer->removeAllDebugMarkers();
        markerBuffer->addDebugMarker(MTLSTR("payload range"), NS::Range::Make(4, 16));
        markerBuffer->removeAllDebugMarkers();
        markerBuffer->addDebugMarker(MTLSTR("final range"), NS::Range::Make(32, 12));
      }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = cb->blitCommandEncoder();
      blit->fillBuffer(data, NS::Range::Make(0, 304), 0);
      blit->fillBuffer(readback, NS::Range::Make(0, 308), 0);
      blit->updateFence(fences[0]);
      blit->endEncoding();
      MTL::ComputeCommandEncoder *compute = cb->computeCommandEncoder();
      compute->waitForFence(fences[0]);
      compute->setComputePipelineState(cp);
      compute->setBuffer(data, 0, 0);
      uint32_t p[4] = {};
      compute->setBytes(p, sizeof(p), 1);
      compute->dispatchThreads(MTL::Size::Make(76, 1, 1), MTL::Size::Make(4, 1, 1));
      compute->updateFence(fences[1]);
      compute->endEncoding();
      MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->waitForFence(fences[1], MTL::RenderStageVertex);
      render->setRenderPipelineState(writer);
      render->setVertexBuffer(data, 0, 0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      render->updateFence(fences[2], MTL::RenderStageVertex);
      render->endEncoding();
      blit = cb->blitCommandEncoder();
      blit->waitForFence(fences[2]);
      blit->copyFromBuffer(data, 0, readback, 0, 304);
      blit->updateFence(fences[3]);
      blit->endEncoding();
      compute = cb->computeCommandEncoder();
      compute->waitForFence(fences[3]);
      compute->setComputePipelineState(cp);
      compute->setBuffer(readback, 0, 0);
      p[0] = 1;
      compute->setBytes(p, sizeof(p), 1);
      compute->dispatchThreads(MTL::Size::Make(76, 1, 1), MTL::Size::Make(4, 1, 1));
      compute->updateFence(fences[0]); // reuse a fence after its earlier consumer
      compute->endEncoding();
      render = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->waitForFence(fences[0], MTL::RenderStageFragment);
      render->setRenderPipelineState(rp);
      render->setFragmentBuffer(readback, 0, 0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      render->endEncoding();
      if(mode == "time") cb->presentDrawableAtTime(drawable, 1.25); // a valid, already-past time
      else if(mode == "duration") cb->presentDrawableAfterMinimumDuration(drawable, 0.001);
      else cb->presentDrawable(drawable);
      cb->commit();
      cb->waitUntilCompleted();
      EndCaptureFrame();
      if(cb->error()) { TEST_WARN("Fence command buffer failed"); failed = true; }
      if(native)
      {
        const uint32_t *a = (const uint32_t *)data->contents();
        const uint32_t *b = (const uint32_t *)readback->contents();
        for(uint32_t i = 0; i < 76; i++)
          if(a[i] != i + 3 + (i < 3 ? 7 : 0) || b[i] != a[i] + 5) failed = true;
        if(b[76] != 0) failed = true;
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(200, 150, 1, 1), 0);
        const byte expected[] = {17, 16, 15, 255};
        if(memcmp(pixel, expected, 4)) failed = true;
      }
      pool->drain();
    }
    if(failed) TEST_WARN("Fence buffer/pixel/padding mismatch");
    for(MTL::Fence *fence : fences) fence->release();
    if(markerBuffer) markerBuffer->release();
    data->release(); readback->release();
    cp->release(); rp->release(); writer->release();
    cs->release(); vs->release(); writerVS->release(); fs->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
