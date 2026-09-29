// SPDX-License-Identifier: MIT
#include "metal_test.h"

RD_TEST(Metal_UE_Parallel_Render, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Two child encoders of one parallel render pass draw separate viewport halves.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 parallel_vs(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 parallel_fs(constant float4 &color [[buffer(0)]]) { return color; }
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    MTL::Function *vs = library ? library->newFunction(MTLSTR("parallel_vs")) : NULL;
    MTL::Function *fs = library ? library->newFunction(MTLSTR("parallel_fs")) : NULL;
    MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vs);
    pipelineDesc->setFragmentFunction(fs);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(pipelineDesc, &error);
    pipelineDesc->release();
    if(!pipeline) return 6;
    if(!device->supportsCounterSampling(MTL::CounterSamplingPointAtStageBoundary)) return 6;
    MTL::CounterSet *timestamps = NULL;
    NS::Array *sets = device->counterSets();
    for(NS::UInteger i = 0; sets && i < sets->count(); i++)
    {
      MTL::CounterSet *set = sets->object<MTL::CounterSet>(i);
      if(set && set->name() && strcmp(set->name()->utf8String(), "timestamp") == 0)
        timestamps = set;
    }
    if(!timestamps) return 6;

    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    bool failed = false;
    uint32_t frames = 0;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::RenderPassDescriptor *pass = MakeBackbufferRenderPass(
          drawable, MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0));
      pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionUnknown);
      MTL::CounterSampleBufferDescriptor *sampleDesc =
          MTL::CounterSampleBufferDescriptor::alloc()->init();
      sampleDesc->setCounterSet(timestamps);
      sampleDesc->setStorageMode(MTL::StorageModeShared);
      sampleDesc->setSampleCount(4);
      MTL::CounterSampleBuffer *samples = device->newCounterSampleBuffer(sampleDesc, &error);
      sampleDesc->release();
      MTL::Buffer *resolved = device->newBuffer(32, MTL::ResourceStorageModeShared);
      if(!samples || !resolved) return 6;
      auto *counterAttachment = pass->sampleBufferAttachments()->object(0);
      counterAttachment->setSampleBuffer(samples);
      counterAttachment->setStartOfVertexSampleIndex(1);
      counterAttachment->setEndOfFragmentSampleIndex(2);
      MTL::CommandBuffer *command = queue->commandBuffer();
      MTL::ParallelRenderCommandEncoder *parallel =
          command->parallelRenderCommandEncoder(pass);
      if(!parallel) return 5;
      const float colors[2][4] = {{1.0f, 0.0f, 0.0f, 1.0f},
                                  {0.0f, 1.0f, 0.0f, 1.0f}};
      for(uint32_t i = 0; i < 2; i++)
      {
        MTL::RenderCommandEncoder *child = parallel->renderCommandEncoder();
        if(!child) return 5;
        child->setRenderPipelineState(pipeline);
        child->setViewport(MTL::Viewport{double(i * 200), 0.0, 200.0, 300.0, 0.0, 1.0});
        child->setFragmentBytes(colors[i], sizeof(colors[i]), 0);
        child->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        child->endEncoding();
      }
      parallel->setColorStoreAction(MTL::StoreActionStore, 0);
      parallel->endEncoding();
      command->presentDrawable(drawable);
      command->commit();
      command->waitUntilCompleted();
      failed |= command->error() != NULL;
      MTL::CommandBuffer *resolveCommand = queue->commandBuffer();
      MTL::BlitCommandEncoder *resolve = resolveCommand->blitCommandEncoder();
      resolve->resolveCounters(samples, NS::Range::Make(1, 2), resolved, 0);
      resolve->endEncoding();
      resolveCommand->commit();
      resolveCommand->waitUntilCompleted();
      failed |= resolveCommand->error() != NULL;
      if(native)
      {
        const uint64_t *times = (const uint64_t *)resolved->contents();
        failed |= !times[0] || times[0] >= times[1];
      }
      EndCaptureFrame();
      samples->release(); resolved->release();
      pool->drain(); frames++;
    }
    TEST_LOG("UE parallel render %s: %u frames", failed ? "FAILED" : "passed", frames);
    pipeline->release(); fs->release(); vs->release(); library->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
