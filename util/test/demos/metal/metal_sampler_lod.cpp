// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Sampler_LOD, MetalGraphicsTest)
{
  static constexpr const char *Description = "Dynamic single/batch sampler LOD in VS, FS and CS.";
  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct Out { float4 pos [[position]]; float4 color; };
vertex Out vs_main(uint id [[vertex_id]], texture2d<float> image [[texture(0)]],
                   sampler samp [[sampler(2)]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return {float4(p[id],0,1), image.sample(samp, float2(0.5), level(0.0))};
}
fragment float4 fs_main(Out input [[stage_in]], texture2d<float> image [[texture(0)]],
                        sampler samp [[sampler(2)]])
{
  float4 c = image.sample(samp, float2(0.5), level(0.0));
  return float4(input.color.g, c.b, c.r, 1);
}
kernel void cs_main(texture2d<float> image [[texture(0)]], sampler samp [[sampler(2)]],
                    device float4 *output [[buffer(0)]])
{
  output[0] = image.sample(samp, float2(0.5), level(0.0));
}
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding),
                                               NULL, &error);
    if(!library) return 4;
    MTL::Function *vs = library->newFunction(MTLSTR("vs_main"));
    MTL::Function *fs = library->newFunction(MTLSTR("fs_main"));
    MTL::Function *cs = library->newFunction(MTLSTR("cs_main"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *renderPipeline = device->newRenderPipelineState(pd, &error);
    pd->release();
    MTL::ComputePipelineState *computePipeline = device->newComputePipelineState(cs, &error);
    MTL::TextureDescriptor *td = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm, 32, 8, true);
    td->setMipmapLevelCount(3);
    td->setStorageMode(MTL::StorageModeShared);
    td->setUsage(MTL::TextureUsageShaderRead);
    MTL::Texture *texture = device->newTexture(td);
    MTL::SamplerDescriptor *sd = MTL::SamplerDescriptor::alloc()->init();
    sd->setMinFilter(MTL::SamplerMinMagFilterNearest);
    sd->setMagFilter(MTL::SamplerMinMagFilterNearest);
    sd->setMipFilter(MTL::SamplerMipFilterNearest);
    sd->setLodMinClamp(0.0f); sd->setLodMaxClamp(2.0f);
    MTL::SamplerState *sampler = device->newSamplerState(sd);
    sd->release();
    MTL::Buffer *output = device->newBuffer(48, MTL::ResourceStorageModeShared);
    if(!renderPipeline || !computePipeline || !texture || !sampler || !output) return 4;
    const MTL::SamplerState *batch[] = {sampler, NULL};
    const float min1[] = {1, 0}, max1[] = {1, 2}, min2[] = {2, 0}, max2[] = {2, 2};
    bool failed = false;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      for(uint32_t mip = 0; mip < 3; mip++)
      {
        byte data[1024] = {};
        const uint32_t width = 32 >> mip, height = 8 >> mip;
        for(uint32_t i = 0; i < width * height; i++) { data[i * 4 + mip] = 255; data[i * 4 + 3] = 255; }
        texture->replaceRegion(MTL::Region::Make2D(0, 0, width, height), mip, data, width * 4);
      }
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = cb->blitCommandEncoder();
      blit->fillBuffer(output, NS::Range::Make(0, 48), 0);
      blit->endEncoding();
      MTL::ComputeCommandEncoder *compute = cb->computeCommandEncoder();
      compute->setComputePipelineState(computePipeline);
      compute->setTexture(texture, 0);
      compute->setBuffer(output, 0, 0);
      compute->setSamplerState(sampler, 1.0f, 1.0f, 2);
      compute->setSamplerState(sampler, 0.0f, 2.0f, 3);
      compute->setSamplerState(NULL, 0.0f, 2.0f, 3);
      compute->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(1, 1, 1));
      compute->setBuffer(output, 16, 0);
      compute->setSamplerStates(batch, min2, max2, NS::Range::Make(2, 2));
      compute->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(1, 1, 1));
      compute->setBuffer(output, 32, 0);
      compute->setSamplerState(sampler, 2);
      compute->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(1, 1, 1));
      compute->endEncoding();
      MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0.1, 0.1, 0.1, 1)));
      render->setRenderPipelineState(renderPipeline);
      render->setVertexTexture(texture, 0); render->setFragmentTexture(texture, 0);
      render->setVertexSamplerState(sampler, 1.0f, 1.0f, 2);
      render->setFragmentSamplerState(sampler, 2.0f, 2.0f, 2);
      render->setVertexSamplerState(sampler, 0.0f, 2.0f, 3);
      render->setFragmentSamplerState(sampler, 0.0f, 2.0f, 3);
      render->setVertexSamplerState(NULL, 0.0f, 2.0f, 3);
      render->setFragmentSamplerState(NULL, 0.0f, 2.0f, 3);
      for(uint32_t draw = 0; draw < 3; draw++)
      {
        if(draw == 1)
        {
          render->setVertexSamplerStates(batch, min2, max2, NS::Range::Make(2, 2));
          render->setFragmentSamplerStates(batch, min1, max1, NS::Range::Make(2, 2));
        }
        if(draw == 2)
        {
          render->setVertexSamplerState(sampler, 2);
          render->setFragmentSamplerState(sampler, 2);
        }
        render->setScissorRect({draw * 128, 0, draw == 2 ? 144UL : 128UL, 300});
        render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      }
      render->endEncoding(); cb->presentDrawable(drawable); cb->commit(); cb->waitUntilCompleted();
      EndCaptureFrame();
      if(native)
      {
        const float expected[] = {0,1,0,1, 0,0,1,1, 1,0,0,1};
        failed |= memcmp(output->contents(), expected, sizeof(expected)) != 0;
        const byte colors[][4] = {{0,255,255,255}, {0,0,0,255}, {255,0,0,255}};
        for(uint32_t i = 0; i < 3; i++)
        {
          byte pixel[4] = {};
          drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(64 + i * 128, 150, 1, 1), 0);
          failed |= memcmp(pixel, colors[i], 4) != 0;
        }
      }
      pool->drain();
    }
    output->release(); sampler->release(); texture->release();
    renderPipeline->release(); computePipeline->release();
    vs->release(); fs->release(); cs->release(); library->release();
    if(failed) TEST_WARN("T38 sampler LOD colors/data differ");
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
