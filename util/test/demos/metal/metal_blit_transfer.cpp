// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson

#include "metal_test.h"

RD_TEST(Metal_Blit_Transfer, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Pitched buffer uploads/readbacks and whole/ranged texture copies.";

  int main()
  {
    if(!Init())
      return 3;
    const bool optimize = GetEnvVar("RENDERDOC_METAL_BLIT_HINTS") == "1";
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 vs_main(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_main(texture2d_array<float> image [[texture(0)]])
{
  return image.read(uint2(1,1), 0, 1);
}
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!library)
      return 4;
    MTL::Function *vs = library->newFunction(MTLSTR("vs_main"));
    MTL::Function *fs = library->newFunction(MTLSTR("fs_main"));
    MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vs);
    pipelineDesc->setFragmentFunction(fs);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(pipelineDesc, &error);
    pipelineDesc->release();
    byte data[4096] = {};
    for(uint32_t y = 0; y < 2; y++)
      for(uint32_t x = 0; x < 3; x++)
      {
        const byte pixel[] = {byte(64 + x), byte(128 + y), 192, 255};
        memcpy(data + 256 + y * 256 + x * 4, pixel, 4);
      }
    MTL::Buffer *upload = device->newBuffer(data, sizeof(data), MTL::ResourceStorageModeShared);
    MTL::Buffer *readback = device->newBuffer(sizeof(data), MTL::ResourceStorageModeShared);
    MTL::TextureDescriptor *desc = MTL::TextureDescriptor::alloc()->init();
    desc->setTextureType(MTL::TextureType2DArray);
    desc->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
    desc->setWidth(16);
    desc->setHeight(8);
    desc->setArrayLength(2);
    desc->setMipmapLevelCount(2);
    desc->setStorageMode(MTL::StorageModeShared);
    desc->setUsage(MTL::TextureUsageShaderRead);
    MTL::Texture *a = device->newTexture(desc);
    MTL::Texture *b = device->newTexture(desc);
    MTL::Texture *c = device->newTexture(desc);
    MTL::Texture *hintOnly = NULL;
    if(optimize)
    {
      desc->setTextureType(MTL::TextureType2D);
      desc->setWidth(13); desc->setHeight(7);
      desc->setArrayLength(1); desc->setMipmapLevelCount(1);
      hintOnly = device->newTexture(desc);
    }
    desc->release();
    if(!pipeline || !upload || !readback || !a || !b || !c || (optimize && !hintOnly))
      return 4;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      byte zeros[512] = {};
      for(MTL::Texture *texture : {a, b, c})
        for(uint32_t slice = 0; slice < 2; slice++)
          for(uint32_t mip = 0; mip < 2; mip++)
            texture->replaceRegion(MTL::Region::Make2D(0, 0, 16 >> mip, 8 >> mip), mip, slice,
                                   zeros, (16 >> mip) * 4, (16 >> mip) * (8 >> mip) * 4);
      if(hintOnly)
      {
        byte pixels[13 * 7 * 4];
        for(size_t i = 0; i < sizeof(pixels); i += 4)
        {
          pixels[i] = 17; pixels[i + 1] = 34; pixels[i + 2] = 51; pixels[i + 3] = 255;
        }
        hintOnly->replaceRegion(MTL::Region::Make2D(0, 0, 13, 7), 0, pixels, 13 * 4);
      }
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = optimize
          ? cb->blitCommandEncoder(MTL::BlitPassDescriptor::blitPassDescriptor())
          : cb->blitCommandEncoder();
      blit->pushDebugGroup(MTLSTR("T37 pitched transfers"));
      blit->fillBuffer(readback, NS::Range::Make(0, 4096), 0xa5);
      blit->copyFromBuffer(upload, 256, 256, 512, MTL::Size::Make(3, 2, 1),
                           a, 1, 1, MTL::Origin::Make(1, 1, 0));
      blit->copyFromBuffer(upload, 256, 256, 512, MTL::Size::Make(3, 2, 1),
                           a, 0, 1, MTL::Origin::Make(4, 1, 0), MTL::BlitOptionNone);
      if(optimize)
      {
        blit->optimizeContentsForGPUAccess(hintOnly);
        blit->optimizeContentsForGPUAccess(a, 1, 1);
      }
      blit->copyFromTexture(a, b);
      blit->copyFromTexture(b, 1, 1, c, 0, 1, 1, 1);
      blit->insertDebugSignpost(MTLSTR("T37 readback"));
      if(optimize)
      {
        blit->optimizeContentsForCPUAccess(b);
        blit->optimizeContentsForCPUAccess(c, 0, 1);
      }
      blit->copyFromTexture(c, 0, 1, MTL::Origin::Make(1, 1, 0), MTL::Size::Make(3, 2, 1),
                            readback, 512, 256, 512);
      blit->copyFromTexture(b, 0, 1, MTL::Origin::Make(4, 1, 0), MTL::Size::Make(3, 2, 1),
                            readback, 1536, 256, 512, MTL::BlitOptionNone);
      blit->popDebugGroup();
      blit->endEncoding();
      MTL::RenderCommandEncoder *render =
          cb->renderCommandEncoder(
              MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->setRenderPipelineState(pipeline);
      render->setFragmentTexture(c, 0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      render->endEncoding();
      cb->presentDrawable(drawable);
      cb->commit();
      cb->waitUntilCompleted();
      EndCaptureFrame();
      if(native)
      {
        const byte *actual = (const byte *)readback->contents();
        for(size_t i = 0; i < 4096; i++)
        {
          byte expected = 0xa5;
          for(size_t base : {size_t(512), size_t(1536)})
            for(size_t y = 0; y < 2; y++)
              if(i >= base + y * 256 && i < base + y * 256 + 12)
                expected = data[256 + y * 256 + i - (base + y * 256)];
          failed |= actual[i] != expected;
        }
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(200, 150, 1, 1), 0);
        const byte expectedPixel[] = {192, 128, 64, 255};
        failed |= memcmp(pixel, expectedPixel, 4) != 0;
      }
      pool->drain();
    }
    c->release(); b->release(); a->release();
    if(hintOnly) hintOnly->release();
    readback->release(); upload->release();
    pipeline->release(); fs->release(); vs->release(); library->release();
    if(failed) TEST_WARN("T37 pitched transfer bytes or output differ");
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
