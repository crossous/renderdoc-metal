// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Resource_Barriers, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Compute/render resource declarations and dependent GPU writes separated by barriers.";
  int main()
  {
    if(!Init()) return 3;
    const bool renderMode = GetEnvVar("RENDERDOC_METAL_RENDER_BARRIERS") == "1";
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
kernel void cs_main(device uint *data [[buffer(0)]], constant uint4 &p [[buffer(1)]],
                    uint i [[thread_position_in_grid]])
{
  if(i >= 68) return;
  if(p.x == 0) data[i] = i + 1;
  else if(p.x == 1) data[i] = data[i] * 3 + 7;
  else data[i] += 11;
}
vertex void vs_writer(uint id [[vertex_id]], device uint *data [[buffer(0)]],
                         constant uint4 &p [[buffer(1)]])
{
  if(p.x == 0) data[id] = 20 * (id + 1);
  else data[id] += 7;
}
vertex float4 vs_main(uint id [[vertex_id]])
{
  const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_main(const device uint *data [[buffer(0)]],
                         constant uint4 &p [[buffer(1)]])
{
  return p.x == 0 ? float4(data[0], data[32], data[67], 255) / 255.0
                  : float4(data[0], data[1], data[2], 255) / 255.0;
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!lib)
    {
      TEST_WARN("Resource barrier shader compilation failed: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }
    MTL::Function *cs = lib->newFunction(MTLSTR("cs_main"));
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_main"));
    MTL::Function *writerVS = lib->newFunction(MTLSTR("vs_writer"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_main"));
    MTL::ComputePipelineState *cp = device->newComputePipelineState(cs, &error);
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *rp = device->newRenderPipelineState(pd, &error);
    // This invalid pipeline must remain nil when capture is injected, just as in native Metal.
    pd->setFragmentFunction(NULL); pd->setRasterizationEnabled(false);
    error = NULL;
    MTL::RenderPipelineState *invalid = device->newRenderPipelineState(pd, &error);
    if(invalid || !error)
    { TEST_WARN("Failed render pipeline creation did not preserve nil/error"); return 5; }
    error = NULL;
    pd->setVertexFunction(writerVS);
    MTL::RenderPipelineState *writer = device->newRenderPipelineState(pd, &error);
    pd->release();
    const uint32_t byteLength = renderMode ? 112 : 272;
    MTL::Buffer *data = device->newBuffer(byteLength, MTL::ResourceStorageModeShared);
    byte sentinel[52]; memset(sentinel, 0x6d, sizeof(sentinel));
    MTL::Buffer *declaredBuffer = device->newBuffer(sentinel, renderMode ? 52 : 44,
                                                    MTL::ResourceStorageModeShared);
    const uint32_t width = renderMode ? 12 : 11;
    MTL::TextureDescriptor *td = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm, width, 9, false);
    td->setStorageMode(MTL::StorageModeShared); td->setUsage(MTL::TextureUsageShaderRead);
    MTL::Texture *declaredTexture = device->newTexture(td);
    if(!cp || !rp || !writer || !data || !declaredBuffer || !declaredTexture)
    {
      TEST_WARN("Resource barrier creation failed (compute %d, render %d, writer %d): %s",
                cp != NULL, rp != NULL, writer != NULL,
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }
    byte pixels[12 * 9 * 4];
    for(size_t i = 0; i < sizeof(pixels); i += 4)
    { pixels[i] = 34; pixels[i + 1] = 68; pixels[i + 2] = 102; pixels[i + 3] = 255; }
    declaredTexture->replaceRegion(MTL::Region::Make2D(0, 0, width, 9), 0, pixels, width * 4);
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = cb->blitCommandEncoder();
      blit->fillBuffer(data, NS::Range::Make(0, byteLength), 0); blit->endEncoding();
      const MTL::Resource *resources[] = {data, declaredBuffer, declaredTexture};
      const MTL::Resource *dependency[] = {data};
      uint32_t params[4] = {};
      if(!renderMode)
      {
        MTL::ComputeCommandEncoder *enc = cb->computeCommandEncoder(MTL::DispatchTypeConcurrent);
        enc->pushDebugGroup(MTLSTR("T42 dependent compute"));
        enc->setComputePipelineState(cp); enc->setBuffer(data, 0, 0);
        enc->useResource(data, MTL::ResourceUsage(MTL::ResourceUsageRead | MTL::ResourceUsageWrite));
        enc->useResource(declaredTexture, MTL::ResourceUsage(MTL::ResourceUsageRead | MTL::ResourceUsageSample));
        enc->useResources(resources, 3, MTL::ResourceUsageRead);
        enc->useResources(resources, 0, MTL::ResourceUsageRead);
        for(uint32_t step = 0; step < 3; step++)
        {
          params[0] = step;
          enc->setBytes(params, sizeof(params), 1);
          enc->dispatchThreads(MTL::Size::Make(68, 1, 1), MTL::Size::Make(4, 1, 1));
          if(step == 0) enc->memoryBarrier(MTL::BarrierScopeBuffers);
          if(step == 1)
          {
            enc->insertDebugSignpost(MTLSTR("T42 resource barrier"));
            enc->memoryBarrier(dependency, 1);
          }
        }
        enc->popDebugGroup(); enc->endEncoding();
      }
      MTL::RenderCommandEncoder *enc = cb->renderCommandEncoder(
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0, 0, 0, 1)));
      if(renderMode)
      {
        enc->useResources(resources, 3, MTL::ResourceUsageRead);
        enc->useResources(resources, 0, MTL::ResourceUsageRead);
        enc->useResource(data, MTL::ResourceUsage(MTL::ResourceUsageRead | MTL::ResourceUsageWrite), MTL::RenderStageVertex);
        enc->useResource(declaredTexture, MTL::ResourceUsage(MTL::ResourceUsageRead | MTL::ResourceUsageSample),
                           MTL::RenderStageFragment);
        enc->useResources(resources, 3, MTL::ResourceUsageRead,
                            MTL::RenderStages(MTL::RenderStageVertex | MTL::RenderStageFragment));
        enc->useResources(resources, 0, MTL::ResourceUsageRead, MTL::RenderStageVertex);
        enc->setRenderPipelineState(writer); enc->setVertexBuffer(data, 0, 0);
        params[0] = 0; enc->setVertexBytes(params, sizeof(params), 1);
        enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        enc->memoryBarrier(MTL::BarrierScopeBuffers, MTL::RenderStageVertex, MTL::RenderStageVertex);
        params[0] = 1; enc->setVertexBytes(params, sizeof(params), 1);
        enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        enc->memoryBarrier(dependency, 1, MTL::RenderStageVertex, MTL::RenderStageFragment);
      }
      enc->setRenderPipelineState(rp); enc->setFragmentBuffer(data, 0, 0);
      params[0] = renderMode ? 1 : 0;
      enc->setFragmentBytes(params, sizeof(params), 1);
      enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      enc->endEncoding(); cb->presentDrawable(drawable); cb->commit(); cb->waitUntilCompleted();
      EndCaptureFrame();
      if(native)
      {
        const uint32_t *values = (const uint32_t *)data->contents();
        for(uint32_t i = 0; i < byteLength / 4; i++)
        {
          const uint32_t expected = renderMode ? (i < 3 ? 27 + 20 * i : 0) : 3 * i + 21;
          if(values[i] != expected)
          { TEST_WARN("Resource barrier result[%u]=%u, expected %u", i, values[i], expected); failed = true; }
        }
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel, 4, MTL::Region::Make2D(200, 150, 1, 1), 0);
        const byte computePixel[] = {222,117,21,255}, renderPixel[] = {67,47,27,255};
        if(memcmp(pixel, renderMode ? renderPixel : computePixel, 4) != 0)
        { TEST_WARN("Resource barrier pixel BGRA %u,%u,%u,%u", pixel[0],pixel[1],pixel[2],pixel[3]); failed = true; }
      }
      pool->drain();
    }
    data->release(); declaredBuffer->release(); declaredTexture->release();
    cp->release(); rp->release(); writer->release();
    cs->release(); vs->release(); writerVS->release(); fs->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
