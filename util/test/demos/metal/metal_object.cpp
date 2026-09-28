// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"
#include <atomic>

RD_TEST(Metal_Object, MetalGraphicsTest)
{
  static constexpr const char *Description = "Object shader payload drives a mesh triangle.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct Payload { float shift; };
struct MeshVertex { float4 position [[position]]; };
struct MeshPrimitive { float value; };
using Triangle = metal::mesh<MeshVertex, MeshPrimitive, 3, 1, metal::topology::triangle>;
[[object]] void object_main(object_data Payload *payload [[payload]],
                            device const float *shift [[buffer(0)]],
                            mesh_grid_properties grid,
                            uint tid [[thread_index_in_threadgroup]]) {
  if(tid == 0) {
    payload->shift = shift[0];
    grid.set_threadgroups_per_grid(uint3(1,1,1));
  }
}
[[object]] void object_bindings(object_data Payload *payload [[payload]],
                            device const float *shift [[buffer(0)]],
                            constant float &delta [[buffer(1)]],
                            mesh_grid_properties grid,
                            uint tid [[thread_index_in_threadgroup]]) {
  if(tid == 0) {
    payload->shift = shift[0] + delta;
    grid.set_threadgroups_per_grid(uint3(1,1,1));
  }
}
[[object]] void object_sampled(object_data Payload *payload [[payload]],
                            device const float *shift [[buffer(0)]],
                            constant float &delta [[buffer(1)]],
                            texture2d<float> image [[texture(0)]],
                            sampler imageSampler [[sampler(0)]],
                            mesh_grid_properties grid,
                            uint tid [[thread_index_in_threadgroup]]) {
  if(tid == 0) {
    payload->shift = shift[0] + delta +
        image.sample(imageSampler,float2(0.5),level(0.0)).r * 0.5;
    grid.set_threadgroups_per_grid(uint3(1,1,1));
  }
}
[[object]] void object_memory(object_data Payload *payload [[payload]],
                            device const float *shift [[buffer(0)]],
                            constant float &delta [[buffer(1)]],
                            threadgroup float *scratch [[threadgroup(0)]],
                            mesh_grid_properties grid,
                            uint tid [[thread_index_in_threadgroup]]) {
  if(tid == 0) {
    scratch[0] = shift[0] + delta;
    payload->shift = scratch[0];
    grid.set_threadgroups_per_grid(uint3(1,1,1));
  }
}
[[mesh]] void mesh_main(Triangle outputMesh,
                        const object_data Payload *payload [[payload]],
                        uint tid [[thread_index_in_threadgroup]]) {
  if(tid < 3) {
    const float2 positions[] = {float2(-0.7,-0.7),float2(0.7,-0.7),float2(0,0.7)};
    MeshVertex v; v.position = float4(positions[tid].x + payload->shift,
                                      positions[tid].y,0,1);
    outputMesh.set_vertex(tid,v);
    outputMesh.set_index(tid,tid);
  }
  if(tid == 0) {
    MeshPrimitive p; p.value = 1.0;
    outputMesh.set_primitive(0,p);
    outputMesh.set_primitive_count(1);
  }
}
[[mesh]] void mesh_bindings(Triangle outputMesh,
                        const object_data Payload *payload [[payload]],
                        uint tid [[thread_index_in_threadgroup]]) {
  if(tid < 3) {
    const float2 positions[] = {float2(-0.2,-0.2),float2(0.2,-0.2),float2(0,0.2)};
    MeshVertex v; v.position = float4(positions[tid].x + payload->shift,
                                      positions[tid].y,0,1);
    outputMesh.set_vertex(tid,v);
    outputMesh.set_index(tid,tid);
  }
  if(tid == 0) {
    MeshPrimitive p; p.value = 1.0;
    outputMesh.set_primitive(0,p);
    outputMesh.set_primitive_count(1);
  }
}
fragment float4 object_fs() { return float4(0.7,0.2,0.4,1.0); }
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),
                                           NULL,&error);
    if(!lib)
    {
      TEST_WARN("Object library: %s", error ? error->localizedDescription()->utf8String() : "unknown");
      return 4;
    }
    const bool sampled = !GetEnvVar("RENDERDOC_METAL_T85_OBJECT_SAMPLE").empty();
    const bool memory = !GetEnvVar("RENDERDOC_METAL_T86_OBJECT_MEMORY").empty();
    const bool bindings = sampled || memory || !GetEnvVar("RENDERDOC_METAL_T84_OBJECT_BINDINGS").empty();
    MTL::Function *object = lib->newFunction(memory ? MTLSTR("object_memory") :
                                             sampled ? MTLSTR("object_sampled") :
                                             bindings ? MTLSTR("object_bindings") :
                                                        MTLSTR("object_main"));
    MTL::Function *mesh = lib->newFunction(bindings ? MTLSTR("mesh_bindings") :
                                                   MTLSTR("mesh_main"));
    MTL::Function *fs = lib->newFunction(MTLSTR("object_fs"));
    MTL::MeshRenderPipelineDescriptor *desc = MTL::MeshRenderPipelineDescriptor::alloc()->init();
    desc->setObjectFunction(object);
    desc->setMeshFunction(mesh);
    desc->setFragmentFunction(fs);
    desc->setPayloadMemoryLength(16);
    desc->setMaxTotalThreadsPerObjectThreadgroup(32);
    desc->setMaxTotalThreadsPerMeshThreadgroup(32);
    desc->setMaxTotalThreadgroupsPerMeshGrid(1);
    desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    const bool asyncObject = !GetEnvVar("RENDERDOC_METAL_T87_OBJECT_ASYNC").empty();
    MTL::RenderPipelineState *pso = NULL;
    std::atomic<uint32_t> asyncCalls{0};
    std::atomic<bool> asyncFailed{false};
    if(asyncObject)
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
      desc->setObjectFunction(nullptr);
      if(dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC)))
        asyncFailed = true;
      if(asyncCalls != 1) asyncFailed = true;
    }
    else
      pso = device->newRenderPipelineState(desc, MTL::PipelineOptionNone, NULL, &error);
    desc->release();
    if(!pso || asyncFailed)
    {
      TEST_WARN("Object pipeline: %s", error ? error->localizedDescription()->utf8String() : "unknown");
      return 4;
    }
    MTL::Buffer *shift = device->newBuffer(bindings ? 48 : sizeof(float),
                                           MTL::ResourceStorageModeShared);
    const bool indirectDraw = !GetEnvVar("RENDERDOC_METAL_T90_OBJECT_INDIRECT").empty();
    const bool objectThreads4 = !GetEnvVar("RENDERDOC_METAL_T91_OBJECT_THREADS4").empty();
    const bool objectThreadGrid4 = !GetEnvVar("RENDERDOC_METAL_T92_OBJECT_THREAD_GRID4").empty();
    const bool objectGroups2 = !GetEnvVar("RENDERDOC_METAL_T93_OBJECT_GROUPS2").empty();
    const bool objectThreadGrid64 = !GetEnvVar("RENDERDOC_METAL_T94_OBJECT_THREAD_GRID64").empty();
    MTL::Buffer *indirect = nullptr;
    if(indirectDraw)
    {
      struct IndirectPacket
      {
        uint32_t padding[4];
        MTL::DispatchThreadgroupsIndirectArguments args;
      } packet = {{0, 0, 0, 0}, {1, 1, 1}};
      indirect = device->newBuffer(&packet,sizeof(packet),MTL::ResourceStorageModeShared);
      if(!indirect) return 4;
    }
    const float sampledShifts[3] = {-0.75f,-0.1f,0.5f};
    for(uint32_t phase = 0; phase < (bindings ? 3U : 1U); phase++)
      *(float *)((byte *)shift->contents() + phase * 16) =
          sampled ? sampledShifts[phase] :
          bindings ? (float(phase) - 1.0f) * 0.7f : 0.0f;
    MTL::Texture *sampleTexture = NULL;
    MTL::SamplerState *sampleSampler = NULL;
    if(sampled)
    {
      MTL::TextureDescriptor *td = MTL::TextureDescriptor::texture2DDescriptor(
          MTL::PixelFormatRGBA8Unorm,1,1,false);
      td->setStorageMode(MTL::StorageModeShared);
      td->setUsage(MTL::TextureUsageShaderRead);
      sampleTexture = device->newTexture(td);
      const byte pixel[4] = {128,0,0,255};
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
        for(uint32_t phase = 0; phase < (bindings ? 3U : 1U); phase++)
        {
          MTL::CommandBuffer *cb = queue->commandBuffer();
          MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
              MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1)));
          render->setRenderPipelineState(pso);
          if(phase == 0) render->setObjectBuffer(shift,0,0);
          else if(phase == 1)
          {
            render->setObjectBuffer(shift,0,0);
            render->setObjectBufferOffset(16,0);
          }
          else
          {
            const MTL::Buffer *buffers[] = {shift};
            const NS::UInteger offsets[] = {32};
            render->setObjectBuffers(buffers,offsets,NS::Range::Make(0,1));
          }
          if(bindings)
          {
            const float delta = 0.1f;
            render->setObjectBytes(&delta,sizeof(delta),1);
          }
          if(memory)
            render->setObjectThreadgroupMemoryLength(16 * (phase + 1),0);
          if(sampled)
          {
            if(phase == 0)
            {
              render->setObjectTexture(sampleTexture,0);
              render->setObjectSamplerState(sampleSampler,0);
            }
            else if(phase == 1)
            {
              const MTL::Texture *textures[] = {sampleTexture};
              const MTL::SamplerState *samplers[] = {sampleSampler};
              render->setObjectTextures(textures,NS::Range::Make(0,1));
              render->setObjectSamplerStates(samplers,NS::Range::Make(0,1));
            }
            else
            {
              render->setObjectTexture(sampleTexture,0);
              render->setObjectSamplerState(sampleSampler,0.0f,0.0f,0);
              const MTL::SamplerState *samplers[] = {sampleSampler};
              const float minimums[] = {0.0f}, maximums[] = {0.0f};
              render->setObjectSamplerStates(samplers,minimums,maximums,NS::Range::Make(0,1));
            }
          }
          if(indirectDraw)
            render->drawMeshThreadgroups(indirect,16,MTL::Size::Make(1,1,1),
                                         MTL::Size::Make(32,1,1));
          else if(objectThreadGrid64)
            render->drawMeshThreads(MTL::Size::Make(64,1,1),MTL::Size::Make(4,1,1),
                                    MTL::Size::Make(32,1,1));
          else if(objectThreadGrid4)
            render->drawMeshThreads(MTL::Size::Make(4,1,1),MTL::Size::Make(4,1,1),
                                    MTL::Size::Make(32,1,1));
          else
            render->drawMeshThreadgroups(MTL::Size::Make(objectGroups2 ? 2 : 1,1,1),
                                         MTL::Size::Make(objectThreads4 || objectGroups2 ? 4 : 1,1,1),
                                         MTL::Size::Make(32,1,1));
          render->endEncoding();
          if(!bindings || phase == 2) cb->presentDrawable(drawable);
          cb->commit(); cb->waitUntilCompleted();
          if(cb->error())
          {
            TEST_WARN("Object draw %u: %s",phase,
                      cb->error()->localizedDescription()->utf8String());
            failed = true;
          }
        }
      }
      EndCaptureFrame(); pool->drain();
    }
    if(sampleTexture) sampleTexture->release();
    if(sampleSampler) sampleSampler->release();
    if(indirect) indirect->release();
    shift->release(); pso->release(); object->release(); mesh->release(); fs->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
