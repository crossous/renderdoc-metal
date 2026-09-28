// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Buffer_Texture, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Shared buffer-backed texture: CPU changes, GPU writes, padding and replay seeks.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 vs_buffer_texture(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_buffer_texture(texture2d<float> image [[texture(0)]]) {
  return image.read(uint2(0,0));
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding),
                                           NULL, &error);
    if(!lib) return 4;
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_buffer_texture"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_buffer_texture"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs);
    pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pso = device->newRenderPipelineState(pd, &error);
    pd->release();
    if(!pso) return 4;

    const size_t linearAlignment = device->minimumLinearTextureAlignmentForPixelFormat(
        MTL::PixelFormatRGBA8Unorm);
    const size_t bufferAlignment = device->minimumTextureBufferAlignmentForPixelFormat(
        MTL::PixelFormatRGBA8Unorm);
    const size_t alignment = linearAlignment > bufferAlignment ? linearAlignment : bufferAlignment;
    if(!linearAlignment || !bufferAlignment || alignment > 4096) return 4;
    const size_t offset = alignment * 2, rowPitch = alignment;
    const size_t length = offset + rowPitch * 2 + 89;
    MTL::Buffer *buffer = device->newBuffer(length, MTL::ResourceStorageModeShared);
    MTL::TextureDescriptor *td = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm, 3, 2, false);
    td->setStorageMode(MTL::StorageModeShared);
    td->setUsage(MTL::TextureUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget));
    MTL::Texture *texture = buffer ? buffer->newTexture(td, offset, rowPitch) : NULL;
    td->release();
    if(!buffer || !texture)
    {
      TEST_WARN("T59 buffer-backed texture unavailable (alignment %zu)", alignment);
      return 4;
    }
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      byte *bytes = (byte *)buffer->contents();
      memset(bytes, 0xa5, length);
      for(size_t y = 0; y < 2; y++)
        for(size_t x = 0; x < 3; x++)
        {
          byte *pixel = bytes + offset + y * rowPitch + x * 4;
          pixel[0] = 10; pixel[1] = 20; pixel[2] = 30; pixel[3] = 255;
        }
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      for(uint32_t phase = 0; phase < 3; phase++)
      {
        if(phase == 1)
        {
          byte *pixel = (byte *)buffer->contents() + offset;
          pixel[0] = 40; pixel[1] = 50; pixel[2] = 60;
        }
        MTL::CommandBuffer *cb = queue->commandBuffer();
        if(phase == 2)
        {
          MTL::RenderPassDescriptor *offscreen = MTL::RenderPassDescriptor::renderPassDescriptor();
          offscreen->colorAttachments()->object(0)->setTexture(texture);
          offscreen->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
          offscreen->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
          offscreen->colorAttachments()->object(0)->setClearColor(
              MTL::ClearColor::Make(70.0/255.0,80.0/255.0,90.0/255.0,1));
          cb->renderCommandEncoder(offscreen)->endEncoding();
        }
        MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
            MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0,0,0,1)));
        render->setRenderPipelineState(pso);
        render->setFragmentTexture(texture, 0);
        render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        render->endEncoding();
        if(phase == 2) cb->presentDrawable(drawable);
        cb->commit(); cb->waitUntilCompleted();
        if(cb->error()) failed = true;
        if(native)
        {
          byte output[4] = {};
          drawable->texture()->getBytes(output, 4, MTL::Region::Make2D(200,150,1,1),0);
          const byte red = phase == 0 ? 10 : phase == 1 ? 40 : 70;
          const byte green = phase == 0 ? 20 : phase == 1 ? 50 : 80;
          const byte blue = phase == 0 ? 30 : phase == 1 ? 60 : 90;
          const byte expected[] = {blue,green,red,255};
          if(memcmp(output,expected,4))
          {
            TEST_WARN("T59 phase %u BGRA %u/%u/%u/%u",phase,
                      output[0],output[1],output[2],output[3]);
            failed = true;
          }
          const byte *data = (const byte *)buffer->contents();
          const byte *pixel = data + offset;
          if(pixel[0] != red || pixel[1] != green || pixel[2] != blue || pixel[3] != 255 ||
             data[offset + 12] != 0xa5 || data[length-1] != 0xa5)
          {
            TEST_WARN("T59 phase %u buffer alias/padding mismatch",phase);
            failed = true;
          }
        }
      }
      EndCaptureFrame(); pool->drain();
    }
    texture->release(); buffer->release(); pso->release(); fs->release(); vs->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
