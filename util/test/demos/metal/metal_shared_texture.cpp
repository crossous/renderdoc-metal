// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Shared_Texture, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Descriptor-backed Private shared texture and GPU clears across submissions.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 vs_shared(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_shared(texture2d<float> image [[texture(0)]]) {
  return image.read(uint2(0,0));
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),
                                           NULL,&error);
    if(!lib) return 4;
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_shared"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_shared"));
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
    const bool heapUse = !GetEnvVar("RENDERDOC_METAL_T73_HEAP_USE").empty();
    const bool aliasTexture = !GetEnvVar("RENDERDOC_METAL_T132_ALIAS_TEXTURE").empty();
    const bool placementTexture = !GetEnvVar("RENDERDOC_METAL_T117_PLACEMENT_TEXTURE").empty();
    const bool heapTexture = placementTexture || heapUse || aliasTexture ||
                             !GetEnvVar("RENDERDOC_METAL_T72_HEAP_TEXTURE").empty();
    MTL::Heap *heap = NULL;
    NS::UInteger placementOffset = 0;
    if(heapTexture)
    {
      MTL::HeapDescriptor *descriptor = MTL::HeapDescriptor::alloc()->init();
      descriptor->setSize(64 * 1024);
      descriptor->setStorageMode(MTL::StorageModePrivate);
      descriptor->setHazardTrackingMode(MTL::HazardTrackingModeTracked);
      if(placementTexture)
      {
        descriptor->setType(MTL::HeapTypePlacement);
        MTL::SizeAndAlign layout = device->heapTextureSizeAndAlign(td);
        if(!layout.size || !layout.align || layout.size >= 32 * 1024)
          return 6;
        placementOffset = ((layout.size + layout.align - 1) / layout.align) * layout.align;
      }
      heap = device->newHeap(descriptor);
      descriptor->release();
      if(!heap) return 6;
    }
    MTL::Texture *texture = placementTexture ? heap->newTexture(td, placementOffset)
                           : heapTexture ? heap->newTexture(td) : device->newSharedTexture(td);
    MTL::Texture *alias = aliasTexture ? heap->newTexture(td) : NULL;
    td->release();
    if(!texture || (aliasTexture && !alias))
    { TEST_WARN("T64 descriptor-backed shared texture unavailable"); return 4; }
    if(heapTexture && texture->heap() != heap) return 7;
    if(placementTexture && texture->heapOffset() != placementOffset) return 7;
    bool failed = false;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    uint32_t frameIndex = 0;
    const byte rgb[3][3] = {{10,20,30},{40,50,60},{70,80,90}};
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      for(unsigned phase = 0; phase < 3; phase++)
      {
        MTL::CommandBuffer *cb = queue->commandBuffer();
        MTL::RenderPassDescriptor *offscreen = MTL::RenderPassDescriptor::renderPassDescriptor();
        offscreen->colorAttachments()->object(0)->setTexture(texture);
        offscreen->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
        offscreen->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
        offscreen->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(
            rgb[phase][0]/255.0,rgb[phase][1]/255.0,rgb[phase][2]/255.0,1));
        cb->renderCommandEncoder(offscreen)->endEncoding();
        MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
            MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1)));
        if(heapUse)
        {
          const MTL::Heap *heaps[] = {heap};
          if(phase == 0)
            render->useHeap(heap);
          else if(phase == 1)
            render->useHeaps(heaps,1);
          else
          {
            render->useHeap(heap,MTL::RenderStageFragment);
            render->useHeaps(heaps,1,MTL::RenderStageFragment);
          }
        }
        render->setRenderPipelineState(pso);
        render->setFragmentTexture(texture,0);
        render->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
        render->endEncoding();
        if(phase == 2) cb->presentDrawable(drawable);
        cb->commit(); cb->waitUntilCompleted();
        if(cb->error()) failed = true;
        if(native)
        {
          byte actual[4] = {};
          drawable->texture()->getBytes(actual,4,MTL::Region::Make2D(200,150,1,1),0);
          const byte expected[4] = {rgb[phase][2],rgb[phase][1],rgb[phase][0],255};
          if(memcmp(actual,expected,4))
          {
            TEST_WARN("T64 phase %u BGRA %u/%u/%u/%u",phase,
                      actual[0],actual[1],actual[2],actual[3]);
            failed = true;
          }
        }
      }
      if(alias && frameIndex == (native ? 0U : 2U))
      {
        alias->makeAliasable();
        if(!alias->isAliasable()) return 7;
      }
      EndCaptureFrame(); frameIndex++; pool->drain();
    }
    if(alias) alias->release();
    texture->release(); pso->release(); fs->release(); vs->release(); lib->release();
    if(heap) heap->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
