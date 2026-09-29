// SPDX-License-Identifier: MIT
#include "metal_test.h"

RD_TEST(Metal_UE_Blit_Counter, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "UE-style blit pass descriptor with stage counter attachments and GPU resolve.";

  int main()
  {
    if(!Init()) return 3;
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
    bool failed = false;
    uint32_t frames = 0;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CounterSampleBufferDescriptor *sampleDesc =
          MTL::CounterSampleBufferDescriptor::alloc()->init();
      sampleDesc->setCounterSet(timestamps);
      sampleDesc->setStorageMode(MTL::StorageModeShared);
      sampleDesc->setSampleCount(4);
      NS::Error *error = NULL;
      MTL::CounterSampleBuffer *samples = device->newCounterSampleBuffer(sampleDesc, &error);
      sampleDesc->release();
      MTL::Buffer *target = device->newBuffer(64, MTL::ResourceStorageModeShared);
      MTL::Buffer *resolved = device->newBuffer(32, MTL::ResourceStorageModeShared);
      if(!samples || !target || !resolved) return 6;
      MTL::BlitPassDescriptor *pass = MTL::BlitPassDescriptor::alloc()->init();
      auto *attachment = pass->sampleBufferAttachments()->object(0);
      attachment->setSampleBuffer(samples);
      attachment->setStartOfEncoderSampleIndex(1);
      attachment->setEndOfEncoderSampleIndex(2);
      MTL::CommandBuffer *command = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = command->blitCommandEncoder(pass);
      pass->release();
      if(!blit) return 5;
      blit->fillBuffer(target, NS::Range::Make(0, 64), 0x5a);
      blit->endEncoding();
      command->presentDrawable(drawable);
      command->commit();
      command->waitUntilCompleted();
      MTL::CommandBuffer *resolveCommand = queue->commandBuffer();
      MTL::BlitCommandEncoder *resolve = resolveCommand->blitCommandEncoder();
      resolve->resolveCounters(samples, NS::Range::Make(1, 2), resolved, 0);
      resolve->endEncoding();
      resolveCommand->commit();
      resolveCommand->waitUntilCompleted();
      if(command->error() || resolveCommand->error()) failed = true;
      if(native)
      {
        const byte *bytes = (const byte *)target->contents();
        for(size_t i = 0; i < 64; i++) failed |= bytes[i] != 0x5a;
        const uint64_t *times = (const uint64_t *)resolved->contents();
        failed |= !times[0] || times[0] >= times[1];
      }
      EndCaptureFrame();
      samples->release(); target->release(); resolved->release();
      pool->drain(); frames++;
    }
    TEST_LOG("UE blit counter %s: %u frames", failed ? "FAILED" : "passed", frames);
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
