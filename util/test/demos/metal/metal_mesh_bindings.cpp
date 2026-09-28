// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Mesh_Bindings, MetalGraphicsTest)
{
  static constexpr const char *Description = "Mesh buffer/bytes bindings move a triangle across three draws.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct MeshVertex { float4 position [[position]]; };
struct MeshPrimitive { float value; };
using Triangle = metal::mesh<MeshVertex, MeshPrimitive, 3, 1, metal::topology::triangle>;
[[mesh]] void mesh_bind(Triangle outputMesh,
                        device const float *shift [[buffer(0)]],
                        constant float &radius [[buffer(1)]],
                        uint tid [[thread_index_in_threadgroup]]) {
  if(tid < 3) {
    const float2 positions[] = {float2(-1,-1),float2(1,-1),float2(0,1)};
    MeshVertex v;
    v.position = float4(shift[0] + positions[tid].x * radius,
                        positions[tid].y * radius,0,1);
    outputMesh.set_vertex(tid,v);
    outputMesh.set_index(tid,tid);
  }
  if(tid == 0) {
    MeshPrimitive p; p.value = 1.0;
    outputMesh.set_primitive(0,p);
    outputMesh.set_primitive_count(1);
  }
}
[[mesh]] void mesh_bind_sampled(Triangle outputMesh,
                        device const float *shift [[buffer(0)]],
                        constant float &radius [[buffer(1)]],
                        texture2d<float> image [[texture(0)]],
                        sampler imageSampler [[sampler(0)]],
                        uint tid [[thread_index_in_threadgroup]]) {
  if(tid < 3) {
    const float2 positions[] = {float2(-1,-1),float2(1,-1),float2(0,1)};
    const float red = image.sample(imageSampler,float2(0.5),level(0.0)).r;
    MeshVertex v;
    v.position = float4(shift[0] + positions[tid].x * radius + red * 0.5,
                        positions[tid].y * radius,0,1);
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
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),
                                           NULL,&error);
    if(!lib)
    {
      TEST_WARN("Mesh binding library: %s", error ? error->localizedDescription()->utf8String() : "unknown");
      return 4;
    }
    const bool sampled = !GetEnvVar("RENDERDOC_METAL_T80_MESH_SAMPLE").empty();
    MTL::Function *mesh = lib->newFunction(sampled ? MTLSTR("mesh_bind_sampled") :
                                                   MTLSTR("mesh_bind"));
    MTL::Function *fs = lib->newFunction(MTLSTR("mesh_fs"));
    MTL::MeshRenderPipelineDescriptor *desc = MTL::MeshRenderPipelineDescriptor::alloc()->init();
    desc->setMeshFunction(mesh);
    desc->setFragmentFunction(fs);
    desc->setMaxTotalThreadsPerMeshThreadgroup(32);
    desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pso = device->newRenderPipelineState(
        desc, MTL::PipelineOptionNone, NULL, &error);
    desc->release();
    if(!pso) return 4;
    MTL::Buffer *shifts = device->newBuffer(48, MTL::ResourceStorageModeShared);
    for(uint32_t phase = 0; phase < 3; phase++)
      *(float *)((byte *)shifts->contents() + phase * 16) = (float(phase) - 1.0f) * 0.5f;
    MTL::Texture *sampleTexture = NULL;
    MTL::SamplerState *sampleSampler = NULL;
    if(sampled)
    {
      MTL::TextureDescriptor *td = MTL::TextureDescriptor::texture2DDescriptor(
          MTL::PixelFormatRGBA8Unorm,1,1,false);
      td->setStorageMode(MTL::StorageModeShared);
      td->setUsage(MTL::TextureUsageShaderRead);
      sampleTexture = device->newTexture(td);
      const byte pixel[4] = {178,0,0,255};
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
        for(uint32_t phase = 0; phase < 3; phase++)
        {
          MTL::CommandBuffer *cb = queue->commandBuffer();
          MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
              MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1)));
          render->setRenderPipelineState(pso);
          if(phase == 0) render->setMeshBuffer(shifts,0,0);
          else if(phase == 1)
          {
            render->setMeshBuffer(shifts,0,0);
            render->setMeshBufferOffset(16,0);
          }
          else
          {
            const MTL::Buffer *buffers[] = {shifts};
            const NS::UInteger offsets[] = {32};
            render->setMeshBuffers(buffers,offsets,NS::Range::Make(0,1));
          }
          const float radius = 0.25f;
          render->setMeshBytes(&radius,sizeof(radius),1);
          if(sampled)
          {
            if(phase == 0)
            {
              render->setMeshTexture(sampleTexture,0);
              render->setMeshSamplerState(sampleSampler,0);
            }
            else if(phase == 1)
            {
              const MTL::Texture *textures[] = {sampleTexture};
              const MTL::SamplerState *samplers[] = {sampleSampler};
              render->setMeshTextures(textures,NS::Range::Make(0,1));
              render->setMeshSamplerStates(samplers,NS::Range::Make(0,1));
            }
            else
            {
              render->setMeshTexture(sampleTexture,0);
              render->setMeshSamplerState(sampleSampler,0.0f,0.0f,0);
              const MTL::SamplerState *samplers[] = {sampleSampler};
              const float minimums[] = {0.0f}, maximums[] = {0.0f};
              render->setMeshSamplerStates(samplers,minimums,maximums,NS::Range::Make(0,1));
            }
          }
          render->drawMeshThreadgroups(MTL::Size::Make(1,1,1), MTL::Size::Make(1,1,1),
                                       MTL::Size::Make(32,1,1));
          render->endEncoding();
          if(phase == 2) cb->presentDrawable(drawable);
          cb->commit(); cb->waitUntilCompleted();
          if(cb->error())
          {
            TEST_WARN("Mesh binding draw %u: %s",phase,
                      cb->error()->localizedDescription()->utf8String());
            failed = true;
          }
        }
      }
      EndCaptureFrame(); pool->drain();
    }
    if(sampleTexture) sampleTexture->release();
    if(sampleSampler) sampleSampler->release();
    shifts->release(); pso->release(); mesh->release(); fs->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
