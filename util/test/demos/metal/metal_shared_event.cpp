// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Shared_Event, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Shared event with CPU initial value, then GPU-only two-queue timeline and epoch reset.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 vs_shared_event(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_shared_event(const device uchar *colour [[buffer(0)]]) {
  return float4(colour[0],colour[1],colour[2],255)/255.0;
}
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),
                                                NULL,&error);
    if(!library) return 4;
    MTL::Function *vs = library->newFunction(MTLSTR("vs_shared_event"));
    MTL::Function *fs = library->newFunction(MTLSTR("fs_shared_event"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pso = device->newRenderPipelineState(pd,&error);
    pd->release(); fs->release(); vs->release(); library->release();
    MTL::SharedEvent *event = device->newSharedEvent();
    MTL::CommandQueue *other = device->newCommandQueue();
    MTL::Buffer *colour = device->newBuffer(256,MTL::ResourceStorageModeShared);
    if(!pso || !event || !other || !colour) return 4;
    event->setLabel(MTLSTR("T65 shared event with initial value"));
    event->setSignaledValue(100);
    MTL::SharedEvent *imported = NULL;
    if(GetEnvVar("RENDERDOC_METAL_SHARED_EVENT_HANDLE_IMPORT") == "1")
    {
      MTL::SharedEventHandle *handle = event->newSharedEventHandle();
      if(handle)
      {
        imported = device->newSharedEvent(handle);
        handle->release();
      }
      if(!imported || imported->signaledValue() != 100) return 4;
    }
    if(GetEnvVar("RENDERDOC_METAL_SHARED_EVENT_HANDLE_EXPORT") == "1")
    {
      MTL::SharedEventHandle *handle = event->newSharedEventHandle();
      if(handle) handle->release();
    }
    bool failed = false;
    uint64_t frame = 0;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      const uint64_t base = 100 + frame * 10;
      MTL::CommandBuffer *first = queue->commandBuffer();
      first->encodeWait(event,100);
      MTL::BlitCommandEncoder *blit = first->blitCommandEncoder();
      blit->fillBuffer(colour,NS::Range::Make(0,256),10);
      blit->endEncoding();
      first->encodeSignalEvent(event,base+1);
      first->commit(); first->waitUntilCompleted();
      if(event->signaledValue() != base+1) failed = true;
      if(GetEnvVar("RENDERDOC_METAL_SHARED_EVENT_HOST_MUTATION") == "1")
        event->setSignaledValue(base+1);

      MTL::CommandBuffer *second = other->commandBuffer();
      second->encodeWait(imported ? imported : event,base+1);
      MTL::RenderCommandEncoder *render = second->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1)));
      render->setRenderPipelineState(pso);
      render->setFragmentBuffer(colour,0,0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
      render->endEncoding(); second->encodeSignalEvent(imported ? imported : event,base+2);
      second->commit(); second->waitUntilCompleted();
      if(event->signaledValue() != base+2) failed = true;
      if(native)
      {
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel,4,MTL::Region::Make2D(200,150,1,1),0);
        const byte expected[] = {10,10,10,255};
        if(memcmp(pixel,expected,4)) failed = true;
      }

      MTL::CommandBuffer *third = queue->commandBuffer();
      third->encodeWait(event,base+2);
      blit = third->blitCommandEncoder();
      blit->fillBuffer(colour,NS::Range::Make(0,256),80);
      blit->endEncoding();
      render = third->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1)));
      render->setRenderPipelineState(pso);
      render->setFragmentBuffer(colour,0,0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
      render->endEncoding(); third->presentDrawable(drawable);
      third->commit(); third->waitUntilCompleted();
      if(first->error() || second->error() || third->error()) failed = true;
      if(native)
      {
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel,4,MTL::Region::Make2D(200,150,1,1),0);
        const byte expected[] = {80,80,80,255};
        if(memcmp(pixel,expected,4)) failed = true;
      }
      EndCaptureFrame(); pool->drain(); frame++;
    }
    colour->release(); other->release();
    if(imported) imported->release();
    event->release(); pso->release();
    if(failed) TEST_WARN("T65 shared event GPU-only validation failed");
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
