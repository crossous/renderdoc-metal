// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Counter_Stage, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Render stage-boundary timestamp counter samples resolved by a blit encoder.";

  int main()
  {
    if(!Init()) return 3;
    const bool explicitBlit = !GetEnvVar("RENDERDOC_METAL_T154_BLIT_COUNTER_SAMPLE").empty();
    const bool explicitRender = !GetEnvVar("RENDERDOC_METAL_T155_RENDER_COUNTER_SAMPLE").empty();
    if(explicitBlit || explicitRender)
    {
      const bool blit = device->supportsCounterSampling(MTL::CounterSamplingPointAtBlitBoundary);
      const bool draw = device->supportsCounterSampling(MTL::CounterSamplingPointAtDrawBoundary);
      TEST_LOG("Explicit counter sampling support: blit=%u draw=%u", unsigned(blit), unsigned(draw));
      if((explicitBlit && !blit) || (explicitRender && !draw)) return 6;
    }
    if(!device->supportsCounterSampling(MTL::CounterSamplingPointAtStageBoundary)) return 6;
    MTL::CounterSet *timestampSet = nullptr;
    NS::Array *sets = device->counterSets();
    for(NS::UInteger i = 0; sets && i < sets->count(); i++)
    {
      MTL::CounterSet *set = sets->object<MTL::CounterSet>(i);
      if(set && set->name() && strcmp(set->name()->utf8String(),"timestamp") == 0)
        timestampSet = set;
    }
    if(!timestampSet) return 6;

    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 counter_vs(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 counter_fs() { return float4(0.2,0.7,0.3,1); }
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),NULL,&error);
    if(!lib) return 4;
    MTL::Function *vs = lib->newFunction(MTLSTR("counter_vs"));
    MTL::Function *fs = lib->newFunction(MTLSTR("counter_fs"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pso = device->newRenderPipelineState(pd,&error);
    pd->release();
    if(!pso) return 4;

    bool failed = false;
    uint32_t frames = 0;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    const bool offsetSamples = !GetEnvVar("RENDERDOC_METAL_T103_COUNTER_OFFSET").empty();
    const NS::UInteger firstSample = offsetSamples ? 2 : 0;
    const NS::UInteger destinationOffset = offsetSamples ? 16 : 0;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }

      MTL::CounterSampleBufferDescriptor *cd = MTL::CounterSampleBufferDescriptor::alloc()->init();
      cd->setCounterSet(timestampSet);
      cd->setStorageMode(MTL::StorageModeShared);
      cd->setSampleCount(offsetSamples ? 8 : 4);
      MTL::CounterSampleBuffer *samples = device->newCounterSampleBuffer(cd,&error);
      cd->release();
      MTL::Buffer *resolved = device->newBuffer(offsetSamples ? 80 : 64,
                                                MTL::ResourceStorageModeShared);
      if(!samples || !resolved) return 6;
      memset(resolved->contents(),0,resolved->length());

      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::RenderPassDescriptor *pass = MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1));
      auto *attachment = pass->sampleBufferAttachments()->object(0);
      attachment->setSampleBuffer(samples);
      attachment->setStartOfVertexSampleIndex(firstSample);
      attachment->setEndOfVertexSampleIndex(firstSample+1);
      attachment->setStartOfFragmentSampleIndex(firstSample+2);
      attachment->setEndOfFragmentSampleIndex(firstSample+3);
      MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(pass);
      render->setRenderPipelineState(pso);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
      render->endEncoding();
      cb->presentDrawable(drawable);
      cb->commit(); cb->waitUntilCompleted();
      MTL::CommandBuffer *resolveCommand = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = resolveCommand->blitCommandEncoder();
      blit->resolveCounters(samples,NS::Range::Make(firstSample,4),resolved,destinationOffset);
      blit->endEncoding();
      resolveCommand->commit(); resolveCommand->waitUntilCompleted();
      if(cb->error() || resolveCommand->error()) failed = true;
      if(native)
      {
        NS::Data *direct = samples->resolveCounterRange(NS::Range::Make(firstSample,4));
        const uint64_t *values = (const uint64_t *)((const byte *)resolved->contents()+destinationOffset);
        if(!values[0] || values[0] >= values[1] || values[1] >= values[2] ||
           values[2] >= values[3] || !direct || direct->length() < 32 ||
           memcmp(direct->bytes(),values,32))
        {
          TEST_WARN("T101 timestamps %llu/%llu/%llu/%llu",
                    (unsigned long long)values[0],(unsigned long long)values[1],
                    (unsigned long long)values[2],(unsigned long long)values[3]);
          failed = true;
        }
        if(offsetSamples)
        {
          const byte *bytes = (const byte *)resolved->contents();
          for(NS::UInteger i = 0; i < resolved->length(); i++)
            if((i < destinationOffset || i >= destinationOffset+32) && bytes[i] != 0)
              failed = true;
        }
        uint8_t pixel[4] = {};
        drawable->texture()->getBytes(pixel,4,MTL::Region::Make2D(200,150,1,1),0);
        if(abs(int(pixel[0])-77)>1 || abs(int(pixel[1])-179)>1 ||
           abs(int(pixel[2])-51)>1 || pixel[3]!=255)
          failed = true;
      }
      samples->release(); resolved->release();
      EndCaptureFrame(); pool->drain(); frames++;
    }
    pso->release(); vs->release(); fs->release(); lib->release();
    TEST_LOG("T%d %s: %u timestamped render frames",offsetSamples ? 103 : 101,
             failed ? "FAILED" : "passed",frames);
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
