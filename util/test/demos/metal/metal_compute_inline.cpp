// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Compute_Inline, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Compute inline bytes, buffer offset updates and dynamic threadgroup memory.";
  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
kernel void cs_main(device uint *result [[buffer(0)]], constant uint4 &p [[buffer(2)]],
                    threadgroup uint *a [[threadgroup(0)]],
                    threadgroup uint *b [[threadgroup(1)]],
                    uint t [[thread_index_in_threadgroup]])
{
  a[t] = t + p.x;
  b[t] = 2 * t;
  threadgroup_barrier(mem_flags::mem_threadgroup);
  if(t == 0)
  {
    uint sum = 0;
    for(uint i = 0; i < 8; i++) sum += a[i] + b[i];
    result[0] = sum * p.y;
  }
}
vertex float4 vs_main(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_main(const device uint *result [[buffer(0)]])
{
  return float4(result[0], result[4], result[8], 255) / 255.0;
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!lib) return 4;
    MTL::Function *cs = lib->newFunction(MTLSTR("cs_main"));
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_main"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_main"));
    MTL::ComputePipelineState *cp = device->newComputePipelineState(cs, &error);
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *rp = device->newRenderPipelineState(pd, &error);
    pd->release();
    uint32_t constants[64] = {3, 1, 0, 0, 4, 2, 0, 0};
    MTL::Buffer *params = device->newBuffer(constants, sizeof(constants), MTL::ResourceStorageModeShared);
    MTL::Buffer *output = device->newBuffer(80, MTL::ResourceStorageModeShared);
    if(!cp || !rp || !params || !output) return 4;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = cb->blitCommandEncoder();
      blit->fillBuffer(output, NS::Range::Make(0, 80), 0);
      blit->endEncoding();
      MTL::ComputeCommandEncoder *enc = cb->computeCommandEncoder();
      enc->setComputePipelineState(cp);
      enc->setBuffer(output, 0, 0);
      enc->setBuffer(params, 0, 2);
      uint32_t inlineData[8] = {1, 1, 0, 0, 2, 2, 0, 0};
      enc->setBytes(inlineData, 16, 2);
      inlineData[0] = 99; // setBytes must copy immediately, not retain this CPU pointer.
      enc->setThreadgroupMemoryLength(32, 0);
      enc->setThreadgroupMemoryLength(32, 1);
      enc->setThreadgroupMemoryLength(16, 5);
      enc->setThreadgroupMemoryLength(0, 5);
      enc->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(8, 1, 1));
      enc->setBytes(inlineData + 4, 16, 2);
      enc->setBufferOffset(16, 0);
      enc->dispatchThreads(MTL::Size::Make(8, 1, 1), MTL::Size::Make(8, 1, 1));
      enc->setBuffer(params, 0, 2);
      enc->setBufferOffset(32, 0);
      enc->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(8, 1, 1));
      enc->setBufferOffset(16, 2);
      enc->setBufferOffset(48, 0);
      enc->setThreadgroupMemoryLength(64, 0);
      enc->setThreadgroupMemoryLength(48, 1);
      enc->dispatchThreads(MTL::Size::Make(8, 1, 1), MTL::Size::Make(8, 1, 1));
      enc->endEncoding();
      enc = cb->computeCommandEncoder();
      enc->setComputePipelineState(cp);
      enc->setBuffer(output, 64, 0);
      inlineData[0] = 5; inlineData[1] = 1;
      enc->setBytes(inlineData, 16, 2);
      enc->setThreadgroupMemoryLength(32, 0);
      enc->setThreadgroupMemoryLength(32, 1);
      enc->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(8, 1, 1));
      enc->endEncoding();
      MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->setRenderPipelineState(rp); render->setFragmentBuffer(output, 0, 0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      render->endEncoding(); cb->presentDrawable(drawable); cb->commit(); cb->waitUntilCompleted();
      EndCaptureFrame();
      if(native)
      {
        const uint32_t expected[] = {92, 200, 108, 232, 124};
        const uint32_t *values = (const uint32_t *)output->contents();
        for(uint32_t i = 0; i < 20; i++)
          if(values[i] != (i % 4 == 0 ? expected[i / 4] : 0))
          {
            TEST_WARN("T40 result[%u] = %u, expected %u", i, values[i],
                      i % 4 == 0 ? expected[i / 4] : 0);
            failed = true;
          }
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(200, 150, 1, 1), 0);
        const byte expectedPixel[] = {108, 200, 92, 255};
        if(memcmp(pixel, expectedPixel, 4) != 0)
        {
          TEST_WARN("T40 pixel BGRA = %u,%u,%u,%u", pixel[0], pixel[1], pixel[2], pixel[3]);
          failed = true;
        }
      }
      pool->drain();
    }
    output->release(); params->release(); cp->release(); rp->release();
    cs->release(); vs->release(); fs->release(); lib->release();
    if(failed) TEST_WARN("T40 compute inline/threadgroup output differs");
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
