// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Tessellation, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Triangle patch tessellation using factor buffer and dynamic scale.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
[[patch(triangle, 3)]]
vertex float4 vs_tessellation(float3 bary [[position_in_patch]]) {
  const float2 p = bary.x * float2(-1,-1) + bary.y * float2(3,-1) + bary.z * float2(-1,3);
  return float4(p,0,1);
}
fragment float4 fs_tessellation() { return float4(0.25,0.5,0.75,1); }
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),
                                                NULL,&error);
    if(!library) { TEST_WARN("T66 shader compilation failed"); return 4; }
    MTL::Function *vs = library->newFunction(MTLSTR("vs_tessellation"));
    MTL::Function *fs = library->newFunction(MTLSTR("fs_tessellation"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->setMaxTessellationFactor(4);
    pd->setTessellationFactorScaleEnabled(true);
    pd->setTessellationFactorFormat(MTL::TessellationFactorFormatHalf);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pso = device->newRenderPipelineState(pd,&error);
    pd->release(); fs->release(); vs->release(); library->release();
    if(!pso) { TEST_WARN("T66 pipeline creation failed"); return 4; }

    MTL::Buffer *factors = device->newBuffer(256,MTL::ResourceStorageModeShared);
    if(!factors) return 4;
    // half(1.0) for three edge factors and one inside factor.
    const uint16_t ones[4] = {0x3c00,0x3c00,0x3c00,0x3c00};
    memcpy(factors->contents(),ones,sizeof(ones));
    bool failed = false;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1)));
      render->setRenderPipelineState(pso);
      render->setTessellationFactorBuffer(factors,0,0);
      render->setTessellationFactorScale(1.0f);
      render->drawPatches(3,0,1,nullptr,0,1,0);
      render->endEncoding(); cb->presentDrawable(drawable); cb->commit(); cb->waitUntilCompleted();
      if(cb->error()) failed = true;
      if(native)
      {
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel,4,MTL::Region::Make2D(200,150,1,1),0);
        const byte expected[4] = {191,128,64,255};
        if(memcmp(pixel,expected,4))
        {
          TEST_WARN("T66 pixel BGRA %u/%u/%u/%u",pixel[0],pixel[1],pixel[2],pixel[3]);
          failed = true;
        }
      }
      EndCaptureFrame(); pool->drain();
    }
    factors->release(); pso->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
