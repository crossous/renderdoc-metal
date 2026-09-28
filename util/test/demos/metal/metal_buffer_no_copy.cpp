// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"
#include <atomic>
#include <cstdlib>

static std::atomic<unsigned> noCopyDeallocations{0};

RD_TEST(Metal_Buffer_No_Copy, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Page-aligned no-copy buffer, native deallocator and inter-submission CPU writes.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 vs_no_copy(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_no_copy(constant uint4 *pixel [[buffer(0)]]) {
  return float4(float3((*pixel).xyz),255.0)/255.0;
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),NULL,&error);
    if(!lib) return 4;
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_no_copy"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_no_copy"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pso = device->newRenderPipelineState(pd,&error);
    pd->release();
    if(!pso) return 4;

    void *memory = NULL;
    const size_t length = 4096, offset = 256;
    if(posix_memalign(&memory, length, length)) return 4;
    memset(memory,0xa5,length);
    void (^deallocator)(void *,NS::UInteger) = ^(void *ptr,NS::UInteger size) {
      if(size == 4096) noCopyDeallocations++;
      free(ptr);
    };
    MTL::Buffer *buffer = device->newBuffer(memory,length,MTL::ResourceStorageModeShared,deallocator);
    if(!buffer) { free(memory); return 4; }
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    bool failed = buffer->contents() != memory;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      const uint32_t initial[] = {10,20,30,0};
      memcpy((byte *)memory+offset,initial,16);
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      for(unsigned phase = 0; phase < 3; phase++)
      {
        const uint32_t value[] = {10+30*phase,20+30*phase,30+30*phase,0};
        memcpy((byte *)memory+offset,value,16);
        MTL::CommandBuffer *cb = queue->commandBuffer();
        MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
            MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1)));
        render->setRenderPipelineState(pso);
        render->setFragmentBuffer(buffer,offset,0);
        render->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
        render->endEncoding();
        if(phase == 2) cb->presentDrawable(drawable);
        cb->commit(); cb->waitUntilCompleted();
        if(cb->error()) failed = true;
        if(native)
        {
          byte actual[4] = {};
          drawable->texture()->getBytes(actual,4,MTL::Region::Make2D(200,150,1,1),0);
          const byte expected[] = {byte(value[2]),byte(value[1]),byte(value[0]),255};
          if(memcmp(actual,expected,4))
          {
            TEST_WARN("T61 phase %u BGRA %u/%u/%u/%u",phase,
                      actual[0],actual[1],actual[2],actual[3]);
            failed = true;
          }
        }
      }
      EndCaptureFrame(); pool->drain();
    }
    buffer->release();
    TEST_LOG("T61 no-copy deallocator calls after buffer release: %u",noCopyDeallocations.load());
    if(native && noCopyDeallocations.load() != 1)
    {
      TEST_WARN("T61 no-copy deallocator invoked %u times",noCopyDeallocations.load());
      failed = true;
    }
    pso->release(); fs->release(); vs->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
