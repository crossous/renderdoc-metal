// SPDX-License-Identifier: MIT
#include "metal_test.h"

RD_TEST(Metal_UE_Compute_Counter, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "UE-style compute pass descriptor with stage counter attachments and GPU resolve.";

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

    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
kernel void write_counter(device uint *buffer [[buffer(0)]], uint id [[thread_position_in_grid]])
{
  buffer[id] = 0xabc00000u + id;
}
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    MTL::Function *function = library ? library->newFunction(MTLSTR("write_counter")) : NULL;
    MTL::ComputePipelineState *pipeline = function ?
        device->newComputePipelineState(function, &error) : NULL;
    if(!pipeline) return 6;

    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    const bool largeCounter = !GetEnvVar("RENDERDOC_METAL_COUNTER_4096").empty();
    const uint64_t sampleCount = largeCounter ? 4096 : 4;
    const uint64_t startIndex = largeCounter ? 4093 : 1;
    const uint64_t endIndex = startIndex + 1;
    bool failed = false;
    uint32_t frames = 0;
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
      sampleDesc->setSampleCount(sampleCount);
      MTL::CounterSampleBuffer *samples = device->newCounterSampleBuffer(sampleDesc, &error);
      sampleDesc->release();
      MTL::Buffer *output = device->newBuffer(64, MTL::ResourceStorageModeShared);
      MTL::Buffer *resolved = device->newBuffer(32, MTL::ResourceStorageModeShared);
      if(!samples || !output || !resolved) return 6;
      MTL::ComputePassDescriptor *pass = MTL::ComputePassDescriptor::alloc()->init();
      pass->setDispatchType(MTL::DispatchTypeSerial);
      auto *attachment = pass->sampleBufferAttachments()->object(0);
      attachment->setSampleBuffer(samples);
      attachment->setStartOfEncoderSampleIndex(startIndex);
      attachment->setEndOfEncoderSampleIndex(endIndex);
      MTL::CommandBuffer *command = queue->commandBuffer();
      MTL::ComputeCommandEncoder *compute = command->computeCommandEncoder(pass);
      pass->release();
      if(!compute) return 5;
      compute->setComputePipelineState(pipeline);
      compute->setBuffer(output, 0, 0);
      compute->dispatchThreads(MTL::Size::Make(16, 1, 1), MTL::Size::Make(16, 1, 1));
      compute->endEncoding();
      command->presentDrawable(drawable);
      command->commit();
      command->waitUntilCompleted();
      MTL::CommandBuffer *resolveCommand = queue->commandBuffer();
      MTL::BlitCommandEncoder *resolve = resolveCommand->blitCommandEncoder();
      resolve->resolveCounters(samples, NS::Range::Make(startIndex, 2), resolved, 0);
      resolve->endEncoding();
      resolveCommand->commit();
      resolveCommand->waitUntilCompleted();
      if(command->error() || resolveCommand->error()) failed = true;
      if(native)
      {
        const uint32_t *values = (const uint32_t *)output->contents();
        for(uint32_t i = 0; i < 16; i++) failed |= values[i] != 0xabc00000u + i;
        const uint64_t *times = (const uint64_t *)resolved->contents();
        failed |= !times[0] || times[0] >= times[1];
      }
      EndCaptureFrame();
      samples->release(); output->release(); resolved->release();
      pool->drain(); frames++;
    }
    TEST_LOG("UE compute counter %s: %u frames", failed ? "FAILED" : "passed", frames);
    pipeline->release(); function->release(); library->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
