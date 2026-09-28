// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Tessellation_Variants, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Direct indexed and two indirect triangle patch draw variants.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
[[patch(triangle, 3)]]
vertex float4 vs_patch_variants(float3 bary [[position_in_patch]]) {
  const float2 p = bary.x * float2(-1,-1) + bary.y * float2(3,-1) + bary.z * float2(-1,3);
  return float4(p,0,1);
}
fragment float4 fs_patch_variants(constant float4 &colour [[buffer(0)]]) { return colour; }
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),
                                                NULL,&error);
    if(!library) { TEST_WARN("T67 shader compilation failed"); return 4; }
    MTL::Function *vs = library->newFunction(MTLSTR("vs_patch_variants"));
    MTL::Function *fs = library->newFunction(MTLSTR("fs_patch_variants"));
    MTL::RenderPipelineState *pipelines[2] = {};
    for(unsigned indexed = 0; indexed < 2; indexed++)
    {
      MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
      pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
      pd->setMaxTessellationFactor(4);
      pd->setTessellationFactorFormat(MTL::TessellationFactorFormatHalf);
      if(indexed)
        pd->setTessellationControlPointIndexType(MTL::TessellationControlPointIndexTypeUInt16);
      pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
      pipelines[indexed] = device->newRenderPipelineState(pd,&error);
      pd->release();
      if(!pipelines[indexed]) { TEST_WARN("T67 pipeline %u creation failed",indexed); return 4; }
    }
    fs->release(); vs->release(); library->release();

    MTL::Buffer *factors = device->newBuffer(256,MTL::ResourceStorageModeShared);
    MTL::Buffer *indices = device->newBuffer(64,MTL::ResourceStorageModeShared);
    MTL::Buffer *indirect = device->newBuffer(64,MTL::ResourceStorageModeShared);
    if(!factors || !indices || !indirect) return 4;
    const uint16_t ones[4] = {0x3c00,0x3c00,0x3c00,0x3c00};
    const uint16_t control[3] = {0,1,2};
    const MTL::DrawPatchIndirectArguments args = {1,1,0,0};
    memcpy(factors->contents(),ones,sizeof(ones));
    memcpy(indices->contents(),control,sizeof(control));
    memset(indirect->contents(),0xa5,indirect->length());
    memcpy(indirect->contents(),&args,sizeof(args));
    memcpy((byte *)indirect->contents()+32,&args,sizeof(args));
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
      render->setTessellationFactorBuffer(factors,0,0);
      for(unsigned draw = 0; draw < 3; draw++)
      {
        render->setRenderPipelineState(pipelines[draw == 0 ? 0 : 1]);
        const float colour[4] = {draw == 0 ? 1.0f : 0.0f,
                                 draw == 1 ? 1.0f : 0.0f,
                                 draw == 2 ? 1.0f : 0.0f,1.0f};
        render->setFragmentBytes(colour,sizeof(colour),0);
        render->setScissorRect(MTL::ScissorRect{draw*133,0,draw == 2 ? 134U : 133U,
                                               drawable->texture()->height()});
        if(draw == 0)
          render->drawPatches(3,nullptr,0,indirect,0);
        else if(draw == 1)
          render->drawIndexedPatches(3,0,1,nullptr,0,indices,0,1,0);
        else
          render->drawIndexedPatches(3,nullptr,0,indices,0,indirect,32);
      }
      render->endEncoding(); cb->presentDrawable(drawable); cb->commit(); cb->waitUntilCompleted();
      if(cb->error()) failed = true;
      if(native)
      {
        const byte expected[3][4] = {{0,0,255,255},{0,255,0,255},{255,0,0,255}};
        for(unsigned draw = 0; draw < 3; draw++)
        {
          byte pixel[4] = {};
          drawable->texture()->getBytes(pixel,4,MTL::Region::Make2D(67+133*draw,150,1,1),0);
          if(memcmp(pixel,expected[draw],4))
          {
            TEST_WARN("T67 draw %u pixel BGRA %u/%u/%u/%u",draw,pixel[0],pixel[1],
                      pixel[2],pixel[3]);
            failed = true;
          }
        }
      }
      EndCaptureFrame(); pool->drain();
    }
    factors->release(); indices->release(); indirect->release();
    pipelines[0]->release(); pipelines[1]->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
