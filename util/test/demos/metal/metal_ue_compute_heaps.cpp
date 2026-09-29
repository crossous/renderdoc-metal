// SPDX-License-Identifier: MIT
#include "metal_test.h"
#include <objc/message.h>

RD_TEST(Metal_UE_Compute_Heaps, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Compute useHeap/useHeaps with an actual heap buffer and GPU write.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
kernel void write_heap(device uint *buffer [[buffer(0)]], uint tid [[thread_position_in_grid]])
{
  buffer[tid] = 0x600d0000u + tid;
}
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    MTL::Function *function = library ? library->newFunction(MTLSTR("write_heap")) : NULL;
    MTL::ComputePipelineState *pipeline = function ?
        device->newComputePipelineState(function, &error) : NULL;
    MTL::HeapDescriptor *heapDesc = MTL::HeapDescriptor::alloc()->init();
    heapDesc->setSize(65536);
    heapDesc->setStorageMode(MTL::StorageModePrivate);
    heapDesc->setHazardTrackingMode(MTL::HazardTrackingModeTracked);
    MTL::Heap *heap = device->newHeap(heapDesc);
    heapDesc->release();
    MTL::Buffer *heapBuffer = heap ? heap->newBuffer(256, MTL::ResourceStorageModePrivate) : NULL;
    MTL::Buffer *readback = device->newBuffer(256, MTL::ResourceStorageModeShared);
    const bool sharedPlacement = !GetEnvVar("RENDERDOC_METAL_SHARED_PLACEMENT").empty();
    MTL::Heap *sharedHeap = NULL;
    MTL::Buffer *sharedBuffer = NULL;
    if(sharedPlacement)
    {
      MTL::HeapDescriptor *sharedDesc = MTL::HeapDescriptor::alloc()->init();
      sharedDesc->setSize(65536);
      sharedDesc->setStorageMode(MTL::StorageModeShared);
      sharedDesc->setHazardTrackingMode(MTL::HazardTrackingModeTracked);
      sharedDesc->setType(MTL::HeapTypePlacement);
      sharedHeap = device->newHeap(sharedDesc);
      sharedDesc->release();
      MTL::SizeAndAlign layout =
          device->heapBufferSizeAndAlign(256, MTL::ResourceStorageModeShared);
      if(sharedHeap && layout.align && 1024 % layout.align == 0)
        sharedBuffer = sharedHeap->newBuffer(256, MTL::ResourceStorageModeShared, 1024);
      if(!sharedBuffer) return 6;
    }
    MTL::TextureDescriptor *textureDesc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm, 4, 4, false);
    MTL::Texture *texture = device->newTexture(textureDesc);
    if(!pipeline || !heapBuffer || !readback || !texture) return 6;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    const bool directPresent = !GetEnvVar("RENDERDOC_METAL_DIRECT_PRESENT").empty();
    const bool triggerCapture = !GetEnvVar("RENDERDOC_METAL_TRIGGER_CAPTURE").empty();
    const bool interleavedBuffers =
        !GetEnvVar("RENDERDOC_METAL_INTERLEAVED_COMMAND_BUFFERS").empty();
    const bool drainBeforeCaptureEnd =
        !GetEnvVar("RENDERDOC_METAL_DRAIN_COMMAND_POOL_BEFORE_END").empty();
    // Metal can hand callers a native heap pointer even when the device created a proxy.
    // Exercise the proxy association lookup in both compute and render residency calls.
    MTL::Heap *heapInput = heap;
    MTL::Resource *bufferInput = heapBuffer;
    MTL::Resource *readbackInput = readback;
    MTL::Resource *textureInput = texture;
    if(!native)
    {
      using RealHeap = id (*)(id, SEL);
      heapInput = (MTL::Heap *)((RealHeap)objc_msgSend)((id)heap, sel_registerName("real"));
      bufferInput = (MTL::Resource *)((RealHeap)objc_msgSend)((id)heapBuffer, sel_registerName("real"));
      readbackInput = (MTL::Resource *)((RealHeap)objc_msgSend)((id)readback, sel_registerName("real"));
      textureInput = (MTL::Resource *)((RealHeap)objc_msgSend)((id)texture, sel_registerName("real"));
      if(!heapInput || heapInput == heap || !bufferInput || !readbackInput ||
         !textureInput || bufferInput == heapBuffer || textureInput == texture) return 6;
    }
    bool failed = false;
    uint32_t frames = 0;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      if(triggerCapture && frames == 2 && rdoc)
        rdoc->TriggerCapture();
      if(!triggerCapture) BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      // Model UE's short command-buffer autorelease pool. EndFrameCapture must not follow a
      // record's wrapper pointer after the application has drained this pool.
      NS::AutoreleasePool *commandPool =
          drainBeforeCaptureEnd ? NS::AutoreleasePool::alloc()->init() : NULL;
      MTL::CommandBuffer *command = queue->commandBuffer();
      // UE creates later buffers before it begins encoding this earlier one.
      MTL::CommandBuffer *later = interleavedBuffers ? queue->commandBuffer() : NULL;
      MTL::ComputeCommandEncoder *compute = command->computeCommandEncoder();
      const MTL::Heap *heaps[] = {heapInput};
      compute->useHeap(heapInput);
      compute->useHeaps(heaps, 1);
      const MTL::Resource *computeResources[] = {bufferInput, textureInput};
      compute->useResources(computeResources, 2, MTL::ResourceUsageRead);
      compute->useResource(readbackInput, MTL::ResourceUsageRead);
      compute->setComputePipelineState(pipeline);
      compute->setBuffer(heapBuffer, 0, 0);
      compute->dispatchThreads(MTL::Size::Make(64, 1, 1), MTL::Size::Make(64, 1, 1));
      if(sharedBuffer)
      {
        compute->useHeap(sharedHeap);
        compute->setBuffer(sharedBuffer, 0, 0);
        compute->dispatchThreads(MTL::Size::Make(64, 1, 1), MTL::Size::Make(64, 1, 1));
      }
      compute->endEncoding();
      MTL::BlitCommandEncoder *blit = command->blitCommandEncoder();
      blit->copyFromBuffer(heapBuffer, 0, readback, 0, 256);
      blit->endEncoding();
      MTL::RenderPassDescriptor *pass = MakeBackbufferRenderPass(
          drawable, MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0));
      MTL::RenderCommandEncoder *render = command->renderCommandEncoder(pass);
      render->useHeap(heapInput);
      render->useHeaps(heaps, 1, MTL::RenderStageFragment);
      const MTL::Resource *renderResources[] = {bufferInput, textureInput};
      render->useResources(renderResources, 2, MTL::ResourceUsageRead,
                           MTL::RenderStageFragment);
      render->useResource(readbackInput, MTL::ResourceUsageRead, MTL::RenderStageFragment);
      render->endEncoding();
      if(directPresent)
        command->addScheduledHandler([drawable](MTL::CommandBuffer *) { drawable->present(); });
      else
        command->presentDrawable(drawable);
      command->commit();
      command->waitUntilCompleted();
      if(later)
      {
        later->commit();
        later->waitUntilCompleted();
        if(later->error()) failed = true;
      }
      if(command->error()) failed = true;
      if(native)
      {
        const uint32_t *values = (const uint32_t *)readback->contents();
        for(uint32_t i = 0; i < 64; i++) failed |= values[i] != 0x600d0000u + i;
      }
      if(sharedBuffer)
      {
        const uint32_t *values = (const uint32_t *)sharedBuffer->contents();
        for(uint32_t i = 0; i < 64; i++) failed |= values[i] != 0x600d0000u + i;
      }
      if(commandPool) commandPool->drain();
      if(!triggerCapture) EndCaptureFrame();
      pool->drain(); frames++;
    }
    TEST_LOG("UE compute heap %s: %u frames", failed ? "FAILED" : "passed", frames);
    if(sharedBuffer) sharedBuffer->release();
    if(sharedHeap) sharedHeap->release();
    texture->release(); readback->release(); heapBuffer->release(); heap->release();
    pipeline->release(); function->release(); library->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
