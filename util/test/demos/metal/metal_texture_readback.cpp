// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Texture_Readback, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Texture CPU readback, padded rows/images, mip/slice synchronization and GPU reuse.";

  static bool Check(const byte *bytes, size_t length, uint32_t width, uint32_t height,
                    uint32_t depth, uint32_t rowPitch, uint32_t imagePitch, byte value)
  {
    for(size_t i = 0; i < length; i++)
    {
      byte expected = 0xa5;
      for(uint32_t z = 0; z < depth; z++)
        for(uint32_t y = 0; y < height; y++)
        {
          size_t start = 8 + z * imagePitch + y * rowPitch;
          if(i >= start && i < start + width * 4)
            expected = (i - start) % 4 == 3 ? 255 : byte(value + z);
        }
      if(bytes[i] != expected) return false;
    }
    return true;
  }

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 vs_readback(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_readback(const device uint *values [[buffer(0)]])
{
  return float4(values[0], values[1], values[2], 255) / 255.0;
}
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!library) return 4;
    MTL::Function *vs = library->newFunction(MTLSTR("vs_readback"));
    MTL::Function *fs = library->newFunction(MTLSTR("fs_readback"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(pd, &error);
    pd->release(); vs->release(); fs->release(); library->release();
    MTL::Buffer *parameters = device->newBuffer(68, MTL::ResourceStorageModeShared);
    MTL::TextureDescriptor *td = MTL::TextureDescriptor::alloc()->init();
    td->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
    td->setWidth(11); td->setHeight(7); td->setMipmapLevelCount(2);
    td->setStorageMode(MTL::StorageModeManaged);
    td->setUsage(MTL::TextureUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead));
    MTL::Texture *plain = device->newTexture(td);
    td->setTextureType(MTL::TextureType2DArray); td->setArrayLength(2);
    td->setWidth(18); td->setHeight(10);
    MTL::Texture *array = device->newTexture(td);
    td->setTextureType(MTL::TextureType2D); td->setArrayLength(1);
    td->setWidth(7); td->setHeight(5); td->setMipmapLevelCount(1);
    td->setUsage(MTL::TextureUsageShaderRead);
    td->setStorageMode(MTL::StorageModeShared);
    MTL::Texture *readOnly = device->newTexture(td);
    td->setWidth(19); td->setHeight(3); td->setStorageMode(MTL::StorageModeManaged);
    MTL::Texture *syncOnly = device->newTexture(td);
    td->setTextureType(MTL::TextureType3D);
    td->setStorageMode(MTL::StorageModeShared);
    td->setWidth(8); td->setHeight(4); td->setDepth(2);
    MTL::Texture *volume = device->newTexture(td);
    td->release();
    if(!pipeline || !parameters || !plain || !array || !readOnly || !syncOnly || !volume) return 4;
    bool failed = false;
    uint32_t frames = 0;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      memset(parameters->contents(), 0, 68);
      byte initial[18 * 10 * 4] = {};
      for(MTL::Texture *texture : {plain, array})
        for(uint32_t slice = 0; slice < texture->arrayLength(); slice++)
          for(uint32_t mip = 0; mip < 2; mip++)
          {
            uint32_t w = uint32_t(texture->width()) >> mip, h = uint32_t(texture->height()) >> mip;
            texture->replaceRegion(MTL::Region::Make2D(0, 0, w, h), mip, slice,
                                   initial, w * 4, w * h * 4);
          }
      for(uint32_t i = 0; i < 7 * 5 * 4; i++) initial[i] = i % 4 == 3 ? 255 : 113;
      readOnly->replaceRegion(MTL::Region::Make2D(0, 0, 7, 5), 0, initial, 28);
      for(uint32_t i = 0; i < 19 * 3 * 4; i++) initial[i] = i % 4 == 3 ? 255 : 197;
      syncOnly->replaceRegion(MTL::Region::Make2D(0, 0, 19, 3), 0, initial, 76);
      // Upload one plane at a time; this fixture does not depend on multi-image replaceRegion.
      for(uint32_t z = 0; z < 2; z++)
      {
        for(uint32_t i = 0; i < 8 * 4 * 4; i++) initial[i] = i % 4 == 3 ? 255 : byte(151 + z);
        volume->replaceRegion(MTL::Region::Make3D(0, 0, z, 8, 4, 1), 0, 0, initial, 32, 128);
      }
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *first = queue->commandBuffer();
      for(uint32_t i = 0; i < 2; i++)
      {
        MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
        auto *attachment = pass->colorAttachments()->object(0);
        attachment->setTexture(i ? array : plain);
        attachment->setLevel(1); attachment->setSlice(i);
        attachment->setLoadAction(MTL::LoadActionClear);
        attachment->setStoreAction(MTL::StoreActionStore);
        double color = (i ? 79.0 : 43.0) / 255.0;
        attachment->setClearColor(MTL::ClearColor::Make(color, color, color, 1));
        first->renderCommandEncoder(pass)->endEncoding();
      }
      MTL::BlitCommandEncoder *blit = first->blitCommandEncoder();
      blit->synchronizeTexture(plain, 0, 1);
      blit->synchronizeTexture(array, 1, 1);
      // This texture is only referenced by synchronization during the captured frame.
      blit->synchronizeTexture(syncOnly, 0, 0);
      blit->endEncoding(); first->commit(); first->waitUntilCompleted();
      if(first->error()) failed = true;
      byte a[64], b[96], c[48], d[256];
      memset(a, 0xa5, sizeof(a)); memset(b, 0xa5, sizeof(b));
      memset(c, 0xa5, sizeof(c)); memset(d, 0xa5, sizeof(d));
      plain->getBytes(a + 8, 20, MTL::Region::Make2D(1, 1, 3, 2), 1);
      array->getBytes(b + 8, 28, 84, MTL::Region::Make2D(2, 1, 4, 3), 1, 1);
      readOnly->getBytes(c + 8, 16, MTL::Region::Make2D(3, 2, 2, 2), 0);
      volume->getBytes(d + 8, 32, 128, MTL::Region::Make3D(1, 1, 0, 3, 2, 2), 0, 0);
      failed |= !Check(a, sizeof(a), 3, 2, 1, 20, 0, 43);
      failed |= !Check(b, sizeof(b), 4, 3, 1, 28, 84, 79);
      failed |= !Check(c, sizeof(c), 2, 2, 1, 16, 0, 113);
      failed |= !Check(d, sizeof(d), 3, 2, 2, 32, 128, 151);
      uint32_t *values = (uint32_t *)parameters->contents();
      values[0] = a[8]; values[1] = b[8]; values[2] = c[8];
      values[3] = d[8]; values[4] = d[136];
      MTL::CommandBuffer *second = queue->commandBuffer();
      MTL::RenderCommandEncoder *render = second->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      render->setRenderPipelineState(pipeline); render->setFragmentBuffer(parameters, 0, 0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      render->endEncoding(); second->presentDrawable(drawable);
      second->commit(); second->waitUntilCompleted();
      EndCaptureFrame();
      if(second->error()) failed = true;
      if(native)
      {
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(200, 150, 1, 1), 0);
        const byte expected[] = {113, 79, 43, 255};
        failed |= memcmp(pixel, expected, 4) != 0;
      }
      pool->drain(); frames++;
    }
    plain->release(); array->release(); readOnly->release(); volume->release();
    syncOnly->release();
    parameters->release(); pipeline->release();
    if(failed) TEST_WARN("T50 readback/padding/GPU reuse validation failed");
    else TEST_LOG("T50 passed %u frames / %u CPU reads including row/image padding", frames, frames * 4);
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
