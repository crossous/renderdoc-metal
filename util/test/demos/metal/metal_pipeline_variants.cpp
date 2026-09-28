// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Pipeline_Variants, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Synchronous render/compute pipeline reflection and compute descriptor variants.";
  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct Params { uint bias; };
kernel void cs_main(device uint *result [[buffer(0)]], constant Params &params [[buffer(1)]],
                    uint id [[thread_position_in_grid]])
{
  result[id] = params.bias + id;
}
vertex float4 vs_main(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_main(const device uint *result [[buffer(0)]])
{
  return float4(result[0], result[32], result[64], 255) / 255.0;
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!lib) return 4;
    MTL::Function *cs = lib->newFunction(MTLSTR("cs_main"));
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_main"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_main"));
    MTL::ComputePipelineState *cp[4] = {};
    MTL::RenderPipelineState *rp[2] = {};
    const MTL::PipelineOption withReflection =
        (MTL::PipelineOption)(MTL::PipelineOptionArgumentInfo | MTL::PipelineOptionBufferTypeInfo);
    MTL::AutoreleasedComputePipelineReflection cr = NULL;
    cp[0] = device->newComputePipelineState(cs, withReflection, &cr, &error);
    if(!cp[0] || !cr || cr->arguments()->count() != 2) return 4;
    MTL::Argument *parameter = cr->arguments()->object<MTL::Argument>(1);
    if(parameter->index() != 1 || parameter->bufferDataSize() != 4 ||
       !parameter->bufferStructType() || parameter->bufferStructType()->members()->count() != 1)
      return 4;
    cp[1] = device->newComputePipelineState(cs, MTL::PipelineOptionNone, NULL, &error);
    MTL::ComputePipelineDescriptor *cd = MTL::ComputePipelineDescriptor::alloc()->init();
    cd->setComputeFunction(cs);
    cd->setLabel(MTLSTR("T47 bounded compute descriptor"));
    cd->setMaxTotalThreadsPerThreadgroup(64);
    cd->setThreadGroupSizeIsMultipleOfThreadExecutionWidth(true);
    cd->buffers()->object(0)->setMutability(MTL::MutabilityMutable);
    cd->buffers()->object(1)->setMutability(MTL::MutabilityImmutable);
    cr = NULL;
    cp[2] = device->newComputePipelineState(cd, withReflection, &cr, &error);
    if(!cp[2] || !cr || cr->arguments()->count() != 2 ||
       cp[2]->maxTotalThreadsPerThreadgroup() != 64) return 4;
    parameter = cr->arguments()->object<MTL::Argument>(1);
    if(!parameter->bufferStructType() || parameter->bufferStructType()->members()->count() != 1)
      return 4;
    cp[3] = device->newComputePipelineState(cd, MTL::PipelineOptionNone, NULL, &error);
    // Wrapping must not replace functions in the caller's descriptor.
    if(cd->computeFunction() != cs) return 4;
    cd->release();
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::AutoreleasedRenderPipelineReflection rr = NULL;
    rp[0] = device->newRenderPipelineState(pd, withReflection, &rr, &error);
    if(!rp[0] || !rr || rr->fragmentArguments()->count() != 1) return 4;
    rp[1] = device->newRenderPipelineState(pd, MTL::PipelineOptionNone, NULL, &error);
    if(pd->vertexFunction() != vs || pd->fragmentFunction() != fs) return 4;
    pd->release();
    MTL::Buffer *output = device->newBuffer(532, MTL::ResourceStorageModeShared);
    if(!cp[1] || !cp[3] || !rp[1] || !output) return 4;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    const uint32_t biases[] = {17, 41, 73, 109};
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = cb->blitCommandEncoder();
      blit->fillBuffer(output, NS::Range::Make(0, 532), 0);
      blit->endEncoding();
      MTL::ComputeCommandEncoder *enc = cb->computeCommandEncoder();
      for(uint32_t i = 0; i < 4; i++)
      {
        enc->setComputePipelineState(cp[i]);
        enc->setBuffer(output, i * 128, 0);
        enc->setBytes(&biases[i], 4, 1);
        if(i == 3)
          enc->dispatchThreads(MTL::Size::Make(32, 1, 1), MTL::Size::Make(32, 1, 1));
        else
          enc->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(32, 1, 1));
      }
      enc->endEncoding();
      MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->setFragmentBuffer(output, 0, 0);
      for(uint32_t i = 0; i < 2; i++)
      {
        render->setRenderPipelineState(rp[i]);
        render->setScissorRect(MTL::ScissorRect{i * 200, 0, 200, 300});
        render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      }
      render->endEncoding(); cb->presentDrawable(drawable); cb->commit(); cb->waitUntilCompleted();
      EndCaptureFrame();
      if(native)
      {
        const uint32_t *values = (const uint32_t *)output->contents();
        for(uint32_t i = 0; i < 133; i++)
          if(values[i] != (i < 128 ? biases[i / 32] + i % 32 : 0)) failed = true;
        for(uint32_t x : {100U, 300U})
        {
          byte pixel[4] = {};
          drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(x, 150, 1, 1), 0);
          const byte expected[] = {73, 41, 17, 255};
          if(memcmp(pixel, expected, 4)) failed = true;
        }
      }
      pool->drain();
    }
    output->release();
    for(auto p : cp) p->release();
    for(auto p : rp) p->release();
    cs->release(); vs->release(); fs->release(); lib->release();
    if(failed) TEST_WARN("T47 pipeline variants native output differs");
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
