// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"
#include <atomic>
#include <objc/message.h>

RD_TEST(Metal_Mesh, MetalGraphicsTest)
{
  static constexpr const char *Description = "Native mesh pipeline and direct mesh draw.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct MeshVertex { float4 position [[position]]; };
struct MeshPrimitive { float value; };
using Triangle = metal::mesh<MeshVertex, MeshPrimitive, 3, 1, metal::topology::triangle>;
[[mesh]] void mesh_main(Triangle outputMesh,
                        uint tid [[thread_index_in_threadgroup]]) {
  if(tid < 3) {
    const float2 positions[] = {float2(-0.8,-0.8),float2(0.8,-0.8),float2(0,0.8)};
    MeshVertex v; v.position = float4(positions[tid],0,1);
    outputMesh.set_vertex(tid,v);
    outputMesh.set_index(tid,tid);
  }
  if(tid == 0) {
    MeshPrimitive p; p.value = 1.0;
    outputMesh.set_primitive(0,p);
    outputMesh.set_primitive_count(1);
  }
}
fragment float4 mesh_fs() { return float4(0.2,0.7,0.3,1.0); }
struct LayerMeshPrimitive { float value; uint layer [[render_target_array_index]]; };
using LayerTriangle = metal::mesh<MeshVertex, LayerMeshPrimitive, 3, 1, metal::topology::triangle>;
[[mesh]] void mesh_layered(LayerTriangle outputMesh,
                           uint tid [[thread_index_in_threadgroup]],
                           uint3 group [[threadgroup_position_in_grid]]) {
  if(tid < 3) {
    const float2 positions[] = {float2(-0.8,-0.8),float2(0.8,-0.8),float2(0,0.8)};
    MeshVertex v; v.position = float4(positions[tid],0,1);
    outputMesh.set_vertex(tid,v);
    outputMesh.set_index(tid,tid);
  }
  if(tid == 0) {
    LayerMeshPrimitive p; p.value = 1.0; p.layer = group.x;
    outputMesh.set_primitive(0,p);
    outputMesh.set_primitive_count(1);
  }
}
kernel void write_grid(device uint *words [[buffer(0)]],
                       uint tid [[thread_position_in_grid]]) {
  if(tid == 0) { words[4] = 1; words[5] = 1; words[6] = 1; }
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),
                                           NULL,&error);
    if(!lib)
    {
      TEST_WARN("Mesh library: %s", error ? error->localizedDescription()->utf8String() : "unknown");
      return 4;
    }
    const bool rateLayerOne = !GetEnvVar("RENDERDOC_METAL_T99_RATE_MAP_LAYER_ONE").empty();
    MTL::Function *mesh = lib->newFunction(NS::String::string(
        rateLayerOne ? "mesh_layered" : "mesh_main", NS::UTF8StringEncoding));
    MTL::Function *fs = lib->newFunction(MTLSTR("mesh_fs"));
    MTL::MeshRenderPipelineDescriptor *desc = MTL::MeshRenderPipelineDescriptor::alloc()->init();
    desc->setMeshFunction(mesh);
    desc->setFragmentFunction(fs);
    desc->setMaxTotalThreadsPerMeshThreadgroup(32);
    if(rateLayerOne) desc->setMaxTotalThreadgroupsPerMeshGrid(2);
    desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    const bool archiveMode = !GetEnvVar("RENDERDOC_METAL_MESH_ARCHIVE").empty();
    MTL::BinaryArchive *archive = NULL;
    if(archiveMode)
    {
      MTL::BinaryArchiveDescriptor *archiveDescriptor = MTL::BinaryArchiveDescriptor::alloc()->init();
      archive = device->newBinaryArchive(archiveDescriptor, &error);
      archiveDescriptor->release();
      SEL method = sel_registerName("addMeshRenderPipelineFunctionsWithDescriptor:error:");
      bool added = archive &&
          ((BOOL (*)(id, SEL, SEL))objc_msgSend)((id)archive,
              sel_registerName("respondsToSelector:"), method) &&
          ((BOOL (*)(id, SEL, id, NS::Error **))objc_msgSend)((id)archive,
              method, (id)desc, &error);
      if(!added)
      {
        TEST_WARN("Mesh archive native add unsupported: %s",
                  error ? error->localizedDescription()->utf8String() : "unknown");
        return 6;
      }
      if(!GetEnvVar("RENDERDOC_METAL_MESH_ARCHIVE_BIND").empty())
      {
        NS::Object *archiveObject = (NS::Object *)archive;
        NS::Array *archives = NS::Array::array(&archiveObject, 1);
        ((void (*)(id, SEL, id))objc_msgSend)(
            (id)desc, sel_registerName("setBinaryArchives:"), (id)archives);
      }
    }
    const bool asyncMesh = !GetEnvVar("RENDERDOC_METAL_T82_MESH_ASYNC").empty();
    MTL::RenderPipelineState *pso = NULL;
    std::atomic<uint32_t> asyncCalls{0};
    std::atomic<bool> asyncFailed{false};
    if(asyncMesh)
    {
      dispatch_group_t group = dispatch_group_create();
      dispatch_group_enter(group);
      auto *calls = &asyncCalls;
      auto *failed = &asyncFailed;
      auto *pipelineSlot = &pso;
      device->newRenderPipelineState(desc,MTL::PipelineOptionArgumentInfo,
          ^(MTL::RenderPipelineState *result, MTL::RenderPipelineReflection *, NS::Error *asyncError) {
            if(calls->fetch_add(1) || !result || asyncError || result->device() != device)
              *failed = true;
            if(result) *pipelineSlot = result->retain();
            dispatch_group_leave(group);
          });
      desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
      desc->setMeshFunction(nullptr);
      if(dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC)))
        asyncFailed = true;
      if(asyncCalls != 1) asyncFailed = true;
    }
    else
      pso = device->newRenderPipelineState(desc,
          !GetEnvVar("RENDERDOC_METAL_MESH_ARCHIVE_BIND").empty() ?
              MTL::PipelineOptionFailOnBinaryArchiveMiss : MTL::PipelineOptionNone,
          NULL, &error);
    desc->release();
    if(!pso || asyncFailed)
    {
      TEST_WARN("Mesh pipeline: %s", error ? error->localizedDescription()->utf8String() : "unknown");
      return 4;
    }
    const bool threadGrid = !GetEnvVar("RENDERDOC_METAL_T81_MESH_THREADS").empty();
    const bool indirectGrid = !GetEnvVar("RENDERDOC_METAL_T88_MESH_INDIRECT").empty();
    const bool gpuIndirectGrid = !GetEnvVar("RENDERDOC_METAL_T89_MESH_INDIRECT_GPU").empty();
    const bool rateHalf = !GetEnvVar("RENDERDOC_METAL_T96_RATE_MAP_HALF").empty();
    const bool rateTwoLayersBound =
        rateLayerOne || !GetEnvVar("RENDERDOC_METAL_T98_RATE_MAP_ARRAY_PASS").empty();
    const bool rateTwoLayers = rateTwoLayersBound ||
        !GetEnvVar("RENDERDOC_METAL_T97_RATE_MAP_TWO_LAYERS").empty();
    const bool rateMapProbe = rateHalf || rateTwoLayers ||
        !GetEnvVar("RENDERDOC_METAL_T95_RATE_MAP_PROBE").empty();
    if(rateMapProbe)
    {
      TEST_LOG("T95/T96 rate map support: one=%d two=%d",
               device->supportsRasterizationRateMap(1),
               device->supportsRasterizationRateMap(2));
      if(!device->supportsRasterizationRateMap(1)) return 6;
    }
    MTL::Buffer *indirect = nullptr;
    MTL::Function *writerFunction = nullptr;
    MTL::ComputePipelineState *writerPipeline = nullptr;
    if(gpuIndirectGrid)
    {
      indirect = device->newBuffer(28, MTL::ResourceStorageModePrivate);
      writerFunction = lib->newFunction(MTLSTR("write_grid"));
      if(writerFunction && archiveMode)
      {
        MTL::ComputePipelineDescriptor *writerDescriptor =
            MTL::ComputePipelineDescriptor::alloc()->init();
        writerDescriptor->setComputeFunction(writerFunction);
        if(!archive->addComputePipelineFunctions(writerDescriptor, &error)) return 4;
        NS::Object *archiveObject = (NS::Object *)archive;
        writerDescriptor->setBinaryArchives(NS::Array::array(&archiveObject, 1));
        writerPipeline = device->newComputePipelineState(
            writerDescriptor, MTL::PipelineOptionFailOnBinaryArchiveMiss, NULL, &error);
        writerDescriptor->release();
      }
      else
        writerPipeline = writerFunction ? device->newComputePipelineState(writerFunction,&error) : nullptr;
      if(!indirect || !writerPipeline) return 4;
    }
    else if(indirectGrid)
    {
      struct IndirectPacket
      {
        uint32_t padding[4];
        MTL::DispatchThreadgroupsIndirectArguments args;
      } packet = {{0, 0, 0, 0}, {1, 1, 1}};
      indirect = device->newBuffer(&packet, sizeof(packet), MTL::ResourceStorageModeShared);
      if(!indirect) return 4;
    }
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(drawable)
      {
        MTL::CommandBuffer *cb = queue->commandBuffer();
        if(gpuIndirectGrid)
        {
          MTL::ComputeCommandEncoder *compute = cb->computeCommandEncoder();
          compute->setComputePipelineState(writerPipeline);
          compute->setBuffer(indirect,0,0);
          compute->dispatchThreads(MTL::Size::Make(1,1,1),MTL::Size::Make(1,1,1));
          compute->endEncoding();
        }
        MTL::Texture *arrayTarget = nullptr;
        MTL::RenderPassDescriptor *pass = nullptr;
        if(rateTwoLayersBound)
        {
          MTL::TextureDescriptor *targetDescriptor = MTL::TextureDescriptor::texture2DDescriptor(
              MTL::PixelFormatBGRA8Unorm,drawable->texture()->width(),
              drawable->texture()->height(),false);
          targetDescriptor->setTextureType(MTL::TextureType2DArray);
          targetDescriptor->setArrayLength(2);
          targetDescriptor->setStorageMode(MTL::StorageModePrivate);
          targetDescriptor->setUsage(MTL::TextureUsageRenderTarget);
          arrayTarget = device->newTexture(targetDescriptor);
          if(!arrayTarget) return 6;
          pass = MTL::RenderPassDescriptor::renderPassDescriptor();
          auto *colour = pass->colorAttachments()->object(0);
          colour->setTexture(arrayTarget);
          colour->setLoadAction(MTL::LoadActionClear);
          colour->setStoreAction(MTL::StoreActionStore);
          colour->setClearColor(MTL::ClearColor::Make(0,0,0,1));
          pass->setRenderTargetArrayLength(2);
        }
        else
          pass = MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1));
        MTL::RasterizationRateMap *rateMap = nullptr;
        MTL::Buffer *rateParameters = nullptr;
        if(rateMapProbe)
        {
          const MTL::Size screen = MTL::Size::Make(drawable->texture()->width(),
                                                   drawable->texture()->height(),0);
          MTL::RasterizationRateLayerDescriptor *layer =
              MTL::RasterizationRateLayerDescriptor::alloc()->init(MTL::Size::Make(2,2,0));
          layer->horizontalSampleStorage()[0] = layer->horizontalSampleStorage()[1] =
              rateHalf ? 0.5f : 1.0f;
          layer->verticalSampleStorage()[0] = layer->verticalSampleStorage()[1] = 1.0f;
          MTL::RasterizationRateLayerDescriptor *secondLayer = nullptr;
          MTL::RasterizationRateMapDescriptor *mapDescriptor = nullptr;
          if(rateTwoLayers)
          {
            secondLayer = MTL::RasterizationRateLayerDescriptor::alloc()->init(
                MTL::Size::Make(2,2,0));
            secondLayer->horizontalSampleStorage()[0] =
                secondLayer->horizontalSampleStorage()[1] = 0.5f;
            secondLayer->verticalSampleStorage()[0] =
                secondLayer->verticalSampleStorage()[1] = 1.0f;
            const MTL::RasterizationRateLayerDescriptor *layers[2] = {layer,secondLayer};
            mapDescriptor = MTL::RasterizationRateMapDescriptor::rasterizationRateMapDescriptor(
                screen,2,layers);
          }
          else
            mapDescriptor = MTL::RasterizationRateMapDescriptor::rasterizationRateMapDescriptor(
                screen,layer);
          rateMap = device->newRasterizationRateMap(mapDescriptor);
          layer->release();
          if(secondLayer) secondLayer->release();
          if(!rateMap) return 6;
          const MTL::Size physical = rateMap->physicalSize(0);
          TEST_LOG("T95/T96 rate map screen %lux%lu physical %lux%lu horizontal %.2f",
                   (unsigned long)screen.width,(unsigned long)screen.height,
                   (unsigned long)physical.width,(unsigned long)physical.height,
                   rateHalf ? 0.5f : 1.0f);
          if(rateTwoLayers)
          {
            const MTL::Size secondPhysical = rateMap->physicalSize(1);
            TEST_LOG("T97 second rate-map layer physical %lux%lu",
                     (unsigned long)secondPhysical.width,
                     (unsigned long)secondPhysical.height);
            if(rateMap->layerCount() != 2 || physical.width != screen.width ||
               secondPhysical.width >= screen.width)
              return 6;
          }
          const MTL::SizeAndAlign requirements = rateMap->parameterBufferSizeAndAlign();
          rateParameters = device->newBuffer(requirements.size + requirements.align,
                                             MTL::ResourceStorageModeShared);
          if(!rateParameters) return 6;
          rateMap->copyParameterDataToBuffer(rateParameters,requirements.align);
          if(!rateTwoLayers || rateTwoLayersBound) pass->setRasterizationRateMap(rateMap);
        }
        MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(pass);
        render->setRenderPipelineState(pso);
        if(rateParameters) render->useResource(rateParameters,MTL::ResourceUsageRead);
        if(indirectGrid || gpuIndirectGrid)
          render->drawMeshThreadgroups(indirect, 16, MTL::Size::Make(1,1,1),
                                       MTL::Size::Make(32,1,1));
        else if(threadGrid)
          render->drawMeshThreads(MTL::Size::Make(32,1,1), MTL::Size::Make(1,1,1),
                                  MTL::Size::Make(32,1,1));
        else
          render->drawMeshThreadgroups(MTL::Size::Make(rateLayerOne ? 2 : 1,1,1), MTL::Size::Make(1,1,1),
                                       MTL::Size::Make(32,1,1));
        render->endEncoding();
        if(rateTwoLayersBound)
        {
          MTL::BlitCommandEncoder *blit = cb->blitCommandEncoder();
          blit->copyFromTexture(arrayTarget,rateLayerOne ? 1 : 0,0,MTL::Origin::Make(0,0,0),
                                MTL::Size::Make(rateLayerOne ? rateMap->physicalSize(1).width :
                                                drawable->texture()->width(),
                                                drawable->texture()->height(),1),
                                drawable->texture(),0,0,MTL::Origin::Make(0,0,0));
          blit->endEncoding();
        }
        cb->presentDrawable(drawable);
        cb->commit(); cb->waitUntilCompleted();
        if(arrayTarget) arrayTarget->release();
        if(rateTwoLayersBound && GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty())
        {
          uint8_t pixel[4] = {};
          drawable->texture()->getBytes(pixel,4,MTL::Region::Make2D(
              rateLayerOne ? 100 : 200,150,1,1),0);
          if(abs(int(pixel[0])-77) > 1 || abs(int(pixel[1])-179) > 1 ||
             abs(int(pixel[2])-51) > 1 || pixel[3] != 255)
          {
            TEST_WARN("T98 layer-0 copied pixel BGRA %u/%u/%u/%u",pixel[0],pixel[1],
                      pixel[2],pixel[3]);
            failed = true;
          }
          if(rateLayerOne)
          {
            drawable->texture()->getBytes(pixel,4,MTL::Region::Make2D(175,150,1,1),0);
            if(pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0 || pixel[3] != 255)
            {
              TEST_WARN("T99 layer-1 background pixel BGRA %u/%u/%u/%u",pixel[0],
                        pixel[1],pixel[2],pixel[3]);
              failed = true;
            }
          }
        }
        if(rateMap) rateMap->release();
        if(rateParameters) rateParameters->release();
        if(cb->error())
        {
          TEST_WARN("Mesh draw: %s",cb->error()->localizedDescription()->utf8String());
          failed = true;
        }
      }
      EndCaptureFrame(); pool->drain();
    }
    if(indirect) indirect->release();
    if(writerPipeline) writerPipeline->release();
    if(writerFunction) writerFunction->release();
    if(archive) archive->release();
    pso->release(); mesh->release(); fs->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
