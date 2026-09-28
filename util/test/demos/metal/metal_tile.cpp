// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"
#include <atomic>

RD_TEST(Metal_Tile, MetalGraphicsTest)
{
  static constexpr const char *Description = "Tile pipeline and tile-buffer dispatch.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
kernel void tile_count(device atomic_uint *counter [[buffer(0)]],
                       constant uint &delta [[buffer(1)]],
                       ushort2 tid [[thread_position_in_threadgroup]]) {
  if(tid.x == 0 && tid.y == 0)
    atomic_fetch_add_explicit(counter, delta, memory_order_relaxed);
}
[[visible]] uint tile_step(uint value) { return value + 1; }
kernel void tile_count_visible(device atomic_uint *counter [[buffer(0)]],
                               constant uint &delta [[buffer(1)]],
                               visible_function_table<uint(uint)> table [[buffer(2)]],
                               ushort2 tid [[thread_position_in_threadgroup]]) {
  if(tid.x == 0 && tid.y == 0)
    atomic_fetch_add_explicit(counter, table[0](delta), memory_order_relaxed);
}
kernel void tile_count_sampled(device atomic_uint *counter [[buffer(0)]],
                               constant uint &delta [[buffer(1)]],
                               texture2d<float> image [[texture(0)]],
                               sampler imageSampler [[sampler(0)]],
                               ushort2 tid [[thread_position_in_threadgroup]]) {
  if(tid.x == 0 && tid.y == 0) {
    uint unit = uint(image.sample(imageSampler,float2(0.5),level(0.0)).r * 255.0 + 0.5);
    atomic_fetch_add_explicit(counter, delta * unit, memory_order_relaxed);
  }
}
kernel void tile_count_memory(device atomic_uint *counter [[buffer(0)]],
                              constant uint &delta [[buffer(1)]],
                              threadgroup uint *scratch [[threadgroup(0)]],
                              ushort2 tid [[thread_position_in_threadgroup]]) {
  if(tid.x == 0 && tid.y == 0) {
    scratch[0] = delta;
    atomic_fetch_add_explicit(counter, scratch[0], memory_order_relaxed);
  }
}
vertex float4 tile_vs(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 tile_fs(device uint *counter [[buffer(0)]]) {
  return float4(min(float(counter[0]) / 1536.0, 1.0), 0.0, 0.0, 1.0);
}
fragment float4 tile_fs_sampled(device uint *counter [[buffer(0)]]) {
  return float4(min(float(counter[0]) / 3072.0, 1.0), 0.0, 0.0, 1.0);
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),
                                           NULL,&error);
    if(!lib) { TEST_WARN("Tile shader library: %s", error ? error->localizedDescription()->utf8String() : "unknown"); return 4; }
    const bool sampled = !GetEnvVar("RENDERDOC_METAL_T75_TILE_SAMPLE").empty();
    const bool tileMemory = !GetEnvVar("RENDERDOC_METAL_T76_TILE_MEMORY").empty();
    const bool asyncVisibleTile = !GetEnvVar("RENDERDOC_METAL_T130_TILE_VISIBLE_ASYNC").empty();
    const bool asyncTile = asyncVisibleTile ||
        !GetEnvVar("RENDERDOC_METAL_T77_TILE_ASYNC").empty();
    const bool rangeTile = !GetEnvVar("RENDERDOC_METAL_T129_TILE_VISIBLE_RANGE").empty();
    const bool visibleTile = rangeTile || asyncVisibleTile ||
        !GetEnvVar("RENDERDOC_METAL_T128_TILE_VISIBLE").empty();
    MTL::Function *function = lib->newFunction(
        sampled ? MTLSTR("tile_count_sampled") :
        tileMemory ? MTLSTR("tile_count_memory") :
        visibleTile ? MTLSTR("tile_count_visible") : MTLSTR("tile_count"));
    MTL::Function *visibleFunction = visibleTile ? lib->newFunction(MTLSTR("tile_step")) : NULL;
    MTL::Function *vs = lib->newFunction(MTLSTR("tile_vs"));
    MTL::Function *fs = lib->newFunction(
        sampled ? MTLSTR("tile_fs_sampled") : MTLSTR("tile_fs"));
    MTL::TileRenderPipelineDescriptor *desc = MTL::TileRenderPipelineDescriptor::alloc()->init();
    desc->setTileFunction(function);
    if(visibleTile)
    {
      MTL::LinkedFunctions *links = MTL::LinkedFunctions::linkedFunctions();
      const NS::Object *functions[] = {visibleFunction};
      links->setFunctions(NS::Array::array(functions, 1));
      desc->setLinkedFunctions(links);
    }
    desc->setRasterSampleCount(1);
    desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    const bool archiveMode = !GetEnvVar("RENDERDOC_METAL_TILE_ARCHIVE").empty();
    MTL::BinaryArchive *archive = NULL;
    if(archiveMode)
    {
      MTL::BinaryArchiveDescriptor *archiveDescriptor = MTL::BinaryArchiveDescriptor::alloc()->init();
      archive = device->newBinaryArchive(archiveDescriptor, &error);
      archiveDescriptor->release();
      bool added = archive && archive->addTileRenderPipelineFunctions(desc, &error);
      if(!added)
      {
        TEST_WARN("Tile archive native add unsupported: %s",
                  error ? error->localizedDescription()->utf8String() : "unknown");
        return 6;
      }
      if(!GetEnvVar("RENDERDOC_METAL_TILE_ARCHIVE_BIND").empty())
      {
        NS::Object *archiveObject = (NS::Object *)archive;
        desc->setBinaryArchives(NS::Array::array(&archiveObject, 1));
      }
    }
    MTL::RenderPipelineState *pso = NULL;
    std::atomic<bool> asyncFailed{false};
    std::atomic<uint32_t> asyncCalls{0};
    if(asyncTile)
    {
      dispatch_group_t group = dispatch_group_create();
      dispatch_group_enter(group);
      device->newRenderPipelineState(desc,MTL::PipelineOptionArgumentInfo,
          [&](MTL::RenderPipelineState *result, MTL::RenderPipelineReflection *, NS::Error *asyncError) {
            if(asyncCalls.fetch_add(1) || !result || asyncError || result->device() != device)
              asyncFailed = true;
            if(result) pso = result->retain();
            dispatch_group_leave(group);
          });
      desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
      desc->setTileFunction(nullptr);
      if(asyncVisibleTile) desc->setLinkedFunctions(nullptr);
      if(dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC)))
        asyncFailed = true;
      if(asyncCalls != 1) asyncFailed = true;
    }
    else
      pso = device->newRenderPipelineState(desc,
          !GetEnvVar("RENDERDOC_METAL_TILE_ARCHIVE_BIND").empty() ?
              MTL::PipelineOptionFailOnBinaryArchiveMiss : MTL::PipelineOptionNone,
          NULL,&error);
    desc->release();
    if(!pso || asyncFailed) { TEST_WARN("Tile pipeline: %s", error ? error->localizedDescription()->utf8String() : "unknown"); return 4; }
    MTL::VisibleFunctionTable *visibleTable = NULL;
    if(visibleTile)
    {
      MTL::FunctionHandle *handle = pso->functionHandle(visibleFunction, MTL::RenderStageTile);
      MTL::VisibleFunctionTableDescriptor *tableDesc = MTL::VisibleFunctionTableDescriptor::alloc()->init();
      tableDesc->setFunctionCount(1);
      visibleTable = pso->newVisibleFunctionTable(tableDesc, MTL::RenderStageTile);
      tableDesc->release();
      if(!handle || !visibleTable) return 4;
      visibleTable->setFunction(handle, 0);
    }
    MTL::RenderPipelineDescriptor *drawDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    drawDesc->setVertexFunction(vs); drawDesc->setFragmentFunction(fs);
    drawDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    if(archiveMode)
    {
      if(!archive->addRenderPipelineFunctions(drawDesc, &error)) return 4;
      NS::Object *archiveObject = (NS::Object *)archive;
      drawDesc->setBinaryArchives(NS::Array::array(&archiveObject, 1));
    }
    MTL::RenderPipelineState *drawPSO = archiveMode ?
        device->newRenderPipelineState(drawDesc, MTL::PipelineOptionFailOnBinaryArchiveMiss,
                                       NULL, &error) :
        device->newRenderPipelineState(drawDesc, &error);
    drawDesc->release();
    if(!drawPSO) return 4;
    MTL::Buffer *counter = device->newBuffer(12, MTL::ResourceStorageModeShared);
    MTL::Texture *sampleTexture = NULL;
    MTL::SamplerState *sampleSampler = NULL;
    if(sampled)
    {
      MTL::TextureDescriptor *td = MTL::TextureDescriptor::texture2DDescriptor(
          MTL::PixelFormatRGBA8Unorm,1,1,false);
      td->setStorageMode(MTL::StorageModeShared);
      td->setUsage(MTL::TextureUsageShaderRead);
      sampleTexture = device->newTexture(td);
      const byte pixel[4] = {2,0,0,255};
      if(sampleTexture)
        sampleTexture->replaceRegion(MTL::Region::Make2D(0,0,1,1),0,pixel,4);
      MTL::SamplerDescriptor *sd = MTL::SamplerDescriptor::alloc()->init();
      sd->setMinFilter(MTL::SamplerMinMagFilterNearest);
      sd->setMagFilter(MTL::SamplerMinMagFilterNearest);
      sampleSampler = device->newSamplerState(sd);
      sd->release();
      if(!sampleTexture || !sampleSampler) return 4;
    }
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(drawable)
      {
        memset(counter->contents(),0,12);
        const uint32_t expected = ((drawable->texture()->width() + 15) / 16) *
                                  ((drawable->texture()->height() + 15) / 16);
        for(uint32_t phase = 0; phase < 3; phase++)
        {
          MTL::CommandBuffer *cb = queue->commandBuffer();
          MTL::RenderPassDescriptor *pass = MakeBackbufferRenderPass(
              drawable,MTL::ClearColor::Make(0,0,0,1));
          pass->setTileWidth(16); pass->setTileHeight(16);
          if(tileMemory) pass->setThreadgroupMemoryLength(32);
          MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(pass);
          render->setRenderPipelineState(pso);
          if(phase == 0) render->setTileBuffer(counter,0,0);
          else if(phase == 1)
          {
            render->setTileBuffer(counter,0,0);
            render->setTileBufferOffset(4,0);
          }
          else
          {
            const MTL::Buffer *buffers[] = {counter};
            const NS::UInteger offsets[] = {8};
            render->setTileBuffers(buffers,offsets,NS::Range::Make(0,1));
          }
          const uint32_t delta = phase + 1;
          render->setTileBytes(&delta,sizeof(delta),1);
          if(visibleTile)
          {
            if(rangeTile && phase == 1)
            {
              const MTL::VisibleFunctionTable *tables[] = {visibleTable};
              render->setTileVisibleFunctionTables(tables, NS::Range::Make(2, 1));
            }
            else render->setTileVisibleFunctionTable(visibleTable,2);
          }
          if(tileMemory) render->setThreadgroupMemoryLength(16,phase == 1 ? 16 : 0,0);
          if(sampled)
          {
            const MTL::Texture *textures[] = {sampleTexture};
            const MTL::SamplerState *samplers[] = {sampleSampler};
            const float minLOD[] = {0.0f}, maxLOD[] = {0.0f};
            if(phase == 0)
            {
              render->setTileTexture(sampleTexture,0);
              render->setTileSamplerState(sampleSampler,0);
            }
            else if(phase == 1)
            {
              render->setTileTextures(textures,NS::Range::Make(0,1));
              render->setTileSamplerStates(samplers,NS::Range::Make(0,1));
            }
            else
            {
              render->setTileTexture(sampleTexture,0);
              render->setTileSamplerState(sampleSampler,0.0f,0.0f,0);
              render->setTileSamplerStates(samplers,minLOD,maxLOD,NS::Range::Make(0,1));
            }
          }
          render->dispatchThreadsPerTile(MTL::Size::Make(16,16,1));
          if(tileMemory && phase == 0) render->setThreadgroupMemoryLength(0,0,0);
          if(phase == 0) render->setTileBuffer(nullptr,0,0);
          if(phase == 2)
          {
            const MTL::Buffer *buffers[] = {nullptr};
            const NS::UInteger offsets[] = {0};
            render->setTileBuffers(buffers,offsets,NS::Range::Make(0,1));
          }
          render->endEncoding();
          cb->commit(); cb->waitUntilCompleted();
          const uint32_t actual = ((uint32_t *)counter->contents())[phase];
          if(cb->error() || actual != expected * (delta + (visibleTile ? 1 : 0)) * (sampled ? 2 : 1))
          {
            TEST_WARN("Tile dispatch %u counter=%u expected=%u error=%s",phase,actual,
                      expected * (delta + (visibleTile ? 1 : 0)) * (sampled ? 2 : 1),
                      cb->error() ? cb->error()->localizedDescription()->utf8String() : "none");
            failed = true;
          }
        }
        MTL::CommandBuffer *drawCB = queue->commandBuffer();
        MTL::RenderCommandEncoder *draw = drawCB->renderCommandEncoder(
            MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1)));
        draw->setRenderPipelineState(drawPSO);
        draw->setFragmentBuffer(counter,8,0);
        draw->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
        draw->endEncoding();
        drawCB->presentDrawable(drawable);
        drawCB->commit(); drawCB->waitUntilCompleted();
        if(drawCB->error()) failed = true;
      }
      EndCaptureFrame(); pool->drain();
    }
    counter->release(); drawPSO->release(); pso->release();
    if(archive) archive->release();
    if(visibleTable) visibleTable->release();
    if(visibleFunction) visibleFunction->release();
    if(sampleSampler) sampleSampler->release();
    if(sampleTexture) sampleTexture->release();
    fs->release(); vs->release(); function->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
