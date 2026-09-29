// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Purgeable_State, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Buffer and texture nonvolatile purgeable-state calls before and during a draw.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 vs_purgeable(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_purgeable(constant uint4 *offset [[buffer(0)]],
                             texture2d<float> image [[texture(0)]]) {
  return float4(image.read(uint2(0,0)).rgb + float3((*offset).xyz)/255.0,1);
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding),
                                           NULL, &error);
    if(!lib) return 4;
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_purgeable"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_purgeable"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs);
    pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pso = device->newRenderPipelineState(pd, &error);
    pd->release();
    if(!pso) return 4;

    const uint32_t offset[4] = {5,6,7,0};
    MTL::Buffer *buffer = device->newBuffer(offset, sizeof(offset), MTL::ResourceStorageModeShared);
    MTL::TextureDescriptor *td = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm, 1, 1, false);
    td->setStorageMode(MTL::StorageModeShared);
    td->setUsage(MTL::TextureUsageShaderRead);
    MTL::Texture *texture = device->newTexture(td);
    td->release();
    if(!buffer || !texture) return 4;
    const byte pixel[] = {10,20,30,255};
    texture->replaceRegion(MTL::Region::Make2D(0,0,1,1),0,pixel,4);

    bool failed = false;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    const bool discardAndRefill = !GetEnvVar("RENDERDOC_METAL_PURGEABLE_EMPTY").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      if(buffer->setPurgeableState(MTL::PurgeableStateKeepCurrent) == MTL::PurgeableStateEmpty ||
         texture->setPurgeableState(MTL::PurgeableStateKeepCurrent) == MTL::PurgeableStateEmpty)
        failed = true;
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      if(discardAndRefill)
      {
        buffer->setPurgeableState(MTL::PurgeableStateEmpty);
        texture->setPurgeableState(MTL::PurgeableStateEmpty);
        buffer->setPurgeableState(MTL::PurgeableStateNonVolatile);
        texture->setPurgeableState(MTL::PurgeableStateNonVolatile);
        memcpy(buffer->contents(), offset, sizeof(offset));
        texture->replaceRegion(MTL::Region::Make2D(0,0,1,1),0,pixel,4);
      }
      buffer->setPurgeableState(MTL::PurgeableStateNonVolatile);
      texture->setPurgeableState(MTL::PurgeableStateNonVolatile);
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0,0,0,1)));
      render->setRenderPipelineState(pso);
      render->setFragmentBuffer(buffer,0,0);
      render->setFragmentTexture(texture,0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
      render->endEncoding();
      cb->presentDrawable(drawable);
      cb->commit(); cb->waitUntilCompleted();
      if(cb->error()) failed = true;
      if(native)
      {
        byte output[4] = {};
        drawable->texture()->getBytes(output,4,MTL::Region::Make2D(200,150,1,1),0);
        const byte expected[] = {37,26,15,255};
        if(memcmp(output,expected,4))
        {
          TEST_WARN("T62 purgeable BGRA %u/%u/%u/%u",output[0],output[1],output[2],output[3]);
          failed = true;
        }
      }
      EndCaptureFrame(); pool->drain();
    }
    texture->release(); buffer->release(); pso->release(); fs->release(); vs->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
