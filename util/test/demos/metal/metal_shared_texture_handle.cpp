// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Shared_Texture_Handle, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Same-process shared texture handle export/import and aliasing.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 vs_handle(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_handle(texture2d<float> image [[texture(0)]]) {
  return image.read(uint2(0,0));
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),
                                           NULL,&error);
    if(!lib) return 4;
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_handle"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_handle"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pso = device->newRenderPipelineState(pd,&error);
    pd->release();
    if(!pso) return 4;

    MTL::TextureDescriptor *td = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm,1,1,false);
    td->setStorageMode(MTL::StorageModePrivate);
    td->setUsage(MTL::TextureUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget));
    MTL::Texture *sourceTexture = device->newSharedTexture(td);
    td->release();
    if(!sourceTexture) return 4;
    MTL::SharedTextureHandle *handle = sourceTexture->newSharedTextureHandle();
    if(!handle) { TEST_WARN("T68 shared texture handle unavailable"); return 4; }
    MTL::Texture *imported = device->newSharedTexture(handle);
    handle->release();
    if(!imported) { TEST_WARN("T68 shared texture import unavailable"); return 4; }

    bool failed = false;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    const byte rgb[2][3] = {{20,40,60},{70,90,110}};
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      for(unsigned phase = 0; phase < 2; phase++)
      {
        MTL::CommandBuffer *cb = queue->commandBuffer();
        MTL::RenderPassDescriptor *offscreen = MTL::RenderPassDescriptor::renderPassDescriptor();
        offscreen->colorAttachments()->object(0)->setTexture(sourceTexture);
        offscreen->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
        offscreen->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
        offscreen->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(
            rgb[phase][0]/255.0,rgb[phase][1]/255.0,rgb[phase][2]/255.0,1));
        cb->renderCommandEncoder(offscreen)->endEncoding();
        MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
            MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1)));
        render->setRenderPipelineState(pso);
        render->setFragmentTexture(imported,0);
        render->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
        render->endEncoding();
        if(phase == 1) cb->presentDrawable(drawable);
        cb->commit(); cb->waitUntilCompleted();
        if(cb->error()) failed = true;
        if(native)
        {
          byte actual[4] = {};
          drawable->texture()->getBytes(actual,4,MTL::Region::Make2D(200,150,1,1),0);
          const byte expected[4] = {rgb[phase][2],rgb[phase][1],rgb[phase][0],255};
          if(memcmp(actual,expected,4))
          {
            TEST_WARN("T68 phase %u BGRA %u/%u/%u/%u",phase,
                      actual[0],actual[1],actual[2],actual[3]);
            failed = true;
          }
        }
      }
      EndCaptureFrame(); pool->drain();
    }
    imported->release(); sourceTexture->release(); pso->release();
    fs->release(); vs->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
