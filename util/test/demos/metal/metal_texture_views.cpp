// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Texture_Views, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Three texture-view overloads, subresource/swizzle identity and shared GPU writes.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 vs_view(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_view(texture2d<float> image [[texture(0)]]) {
  return image.read(uint2(0,0));
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding),
                                           NULL, &error);
    if(!lib) return 4;
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_view"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_view"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs);
    pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pso = device->newRenderPipelineState(pd, &error);
    pd->release();
    if(!pso) return 4;

    MTL::TextureDescriptor *td = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm, 4, 4, true);
    td->setStorageMode(MTL::StorageModeShared);
    td->setUsage(MTL::TextureUsage(MTL::TextureUsageShaderRead |
                                   MTL::TextureUsageRenderTarget |
                                   MTL::TextureUsagePixelFormatView));
    MTL::Texture *plain = device->newTexture(td);
    td->setTextureType(MTL::TextureType2DArray);
    td->setArrayLength(2);
    MTL::Texture *array = device->newTexture(td);
    if(!plain || !array) return 4;
    const auto writeInitial = [&]() {
      byte pixels[4 * 4 * 4] = {};
      for(size_t i = 0; i < sizeof(pixels); i += 4)
      {
        pixels[i] = 12; pixels[i + 1] = 34; pixels[i + 2] = 56; pixels[i + 3] = 255;
      }
      plain->replaceRegion(MTL::Region::Make2D(0, 0, 4, 4), 0, pixels, 16);
      for(size_t i = 0; i < 2 * 2 * 4; i += 4)
      {
        pixels[i] = 21; pixels[i + 1] = 43; pixels[i + 2] = 65; pixels[i + 3] = 255;
      }
      array->replaceRegion(MTL::Region::Make2D(0, 0, 2, 2), 1, 1, pixels, 8, 16);
    };
    writeInitial();

    MTL::Texture *simple = plain->newTextureView(MTL::PixelFormatRGBA8Unorm);
    MTL::Texture *subset = array->newTextureView(
        MTL::PixelFormatRGBA8Unorm, MTL::TextureType2D,
        NS::Range::Make(1, 1), NS::Range::Make(1, 1));
    MTL::TextureSwizzleChannels swap = {MTL::TextureSwizzleBlue, MTL::TextureSwizzleGreen,
                                        MTL::TextureSwizzleRed, MTL::TextureSwizzleAlpha};
    MTL::Texture *swizzled = array->newTextureView(
        MTL::PixelFormatRGBA8Unorm, MTL::TextureType2D,
        NS::Range::Make(1, 1), NS::Range::Make(1, 1), swap);
    td->release();
    if(!simple || !subset || !swizzled) return 4;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      writeInitial();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      for(uint32_t phase = 0; phase < 3; phase++)
      {
        MTL::CommandBuffer *cb = queue->commandBuffer();
        if(phase)
        {
          MTL::RenderPassDescriptor *a = MTL::RenderPassDescriptor::renderPassDescriptor();
          a->colorAttachments()->object(0)->setTexture(simple);
          a->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
          a->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
          a->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(
              (34 + phase * 10) / 255.0, (56 + phase * 10) / 255.0,
              (78 + phase * 10) / 255.0, 1));
          cb->renderCommandEncoder(a)->endEncoding();
          MTL::RenderPassDescriptor *b = MTL::RenderPassDescriptor::renderPassDescriptor();
          b->colorAttachments()->object(0)->setTexture(array);
          b->colorAttachments()->object(0)->setLevel(1);
          b->colorAttachments()->object(0)->setSlice(1);
          b->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
          b->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
          b->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(
              (21 + phase * 10) / 255.0, (43 + phase * 10) / 255.0,
              (65 + phase * 10) / 255.0, 1));
          cb->renderCommandEncoder(b)->endEncoding();
        }
        MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
            MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
        render->setRenderPipelineState(pso);
        MTL::Texture *textures[] = {plain, subset, swizzled};
        const NS::UInteger width = drawable->texture()->width();
        for(uint32_t i = 0; i < 3; i++)
        {
          render->setFragmentTexture(textures[i], 0);
          render->setScissorRect(MTL::ScissorRect{i * width / 3, 0,
                                                 (i + 1) * width / 3 - i * width / 3,
                                                 drawable->texture()->height()});
          render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        }
        render->endEncoding();
        if(phase == 2) cb->presentDrawable(drawable);
        cb->commit(); cb->waitUntilCompleted();
        if(cb->error()) failed = true;
        if(native)
          for(uint32_t i = 0; i < 3; i++)
          {
            byte actual[4] = {};
            drawable->texture()->getBytes(actual, 4,
                MTL::Region::Make2D((2 * i + 1) * width / 6, 150, 1, 1), 0);
            const byte red = byte(i == 0 ? (phase ? 34 + phase * 10 : 12) : 21 + phase * 10);
            const byte green = byte(i == 0 ? (phase ? 56 + phase * 10 : 34) : 43 + phase * 10);
            const byte blue = byte(i == 0 ? (phase ? 78 + phase * 10 : 56) : 65 + phase * 10);
            const byte expected[] = {i == 2 ? red : blue, green, i == 2 ? blue : red, 255};
            if(memcmp(actual, expected, 4))
            {
              TEST_WARN("T58 phase %u draw %u BGRA %u/%u/%u/%u expected %u/%u/%u/%u",
                        phase, i, actual[0], actual[1], actual[2], actual[3],
                        expected[0], expected[1], expected[2], expected[3]);
              failed = true;
            }
          }
      }
      EndCaptureFrame(); pool->drain();
    }
    swizzled->release(); subset->release(); simple->release();
    array->release(); plain->release(); pso->release(); fs->release(); vs->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
