// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Device_Argument_Encoder, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Device-created argument encoder with texture/sampler members and two packets.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct Packet { texture2d<float> colour [[id(0)]]; sampler filter [[id(1)]]; };
vertex float4 vs_device_arg(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_device_arg(constant Packet &packet [[buffer(0)]]) {
  return packet.colour.sample(packet.filter,float2(0.5));
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),NULL,&error);
    if(!lib) return 4;
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_device_arg"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_device_arg"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    const bool bindingEncoder = !GetEnvVar("RENDERDOC_METAL_T102_BINDING_ENCODER").empty();
    MTL::AutoreleasedRenderPipelineReflection reflection = NULL;
    MTL::RenderPipelineState *pso = bindingEncoder ?
        device->newRenderPipelineState(pd,MTL::PipelineOptionArgumentInfo,&reflection,&error) :
        device->newRenderPipelineState(pd,&error);
    pd->release();
    if(!pso) return 4;

    MTL::ArgumentDescriptor *texture = MTL::ArgumentDescriptor::alloc()->init();
    texture->setIndex(0); texture->setDataType(MTL::DataTypeTexture);
    texture->setArrayLength(1); texture->setAccess(MTL::BindingAccessReadOnly);
    texture->setTextureType(MTL::TextureType2D);
    MTL::ArgumentDescriptor *sampler = MTL::ArgumentDescriptor::alloc()->init();
    sampler->setIndex(1); sampler->setDataType(MTL::DataTypeSampler);
    sampler->setArrayLength(1); sampler->setAccess(MTL::BindingAccessReadOnly);
    MTL::ArgumentDescriptor *entries[] = {texture,sampler};
    MTL::ArgumentEncoder *encoder = NULL;
    if(bindingEncoder)
    {
      NS::Array *bindings = reflection ? reflection->fragmentBindings() : NULL;
      if(!bindings || bindings->count() != 1) return 4;
      MTL::BufferBinding *binding = bindings->object<MTL::BufferBinding>(0);
      if(!binding || binding->type() != MTL::BindingTypeBuffer) return 4;
      encoder = device->newArgumentEncoder(binding);
    }
    else
      encoder = device->newArgumentEncoder(NS::Array::array(
          (const NS::Object *const *)entries,2));
    texture->release(); sampler->release();
    if(!encoder) return 4;
    const size_t start = 256, stride = encoder->encodedLength();
    MTL::Buffer *arguments = device->newBuffer(start+2*stride+37,MTL::ResourceStorageModeShared);
    memset(arguments->contents(),0xa5,arguments->length());
    MTL::Texture *textures[2] = {};
    MTL::SamplerDescriptor *sd = MTL::SamplerDescriptor::alloc()->init();
    sd->setSupportArgumentBuffers(true);
    MTL::SamplerState *filter = device->newSamplerState(sd); sd->release();
    for(size_t i = 0; i < 2; i++)
    {
      MTL::TextureDescriptor *td = MTL::TextureDescriptor::texture2DDescriptor(
          MTL::PixelFormatRGBA8Unorm,1,1,false);
      td->setStorageMode(MTL::StorageModeShared); td->setUsage(MTL::TextureUsageShaderRead);
      textures[i] = device->newTexture(td);
      const uint8_t pixel[] = {uint8_t(20+50*i),uint8_t(40+40*i),uint8_t(60+30*i),255};
      textures[i]->replaceRegion(MTL::Region::Make2D(0,0,1,1),0,pixel,4);
      encoder->setArgumentBuffer(arguments,start+i*stride);
      encoder->setTexture(textures[i],0);
      encoder->setSamplerState(filter,1);
    }
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    bool failed = false;
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
      render->useResource(textures[0],MTL::ResourceUsageRead);
      render->useResource(textures[1],MTL::ResourceUsageRead);
      for(size_t i = 0; i < 2; i++)
      {
        render->setFragmentBuffer(arguments,start+i*stride,0);
        render->setScissorRect(MTL::ScissorRect{i*drawable->texture()->width()/2,0,
                               drawable->texture()->width()/2,drawable->texture()->height()});
        render->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
      }
      render->endEncoding(); cb->presentDrawable(drawable); cb->commit(); cb->waitUntilCompleted();
      if(cb->error()) failed = true;
      if(native)
        for(size_t i = 0; i < 2; i++)
        {
          byte actual[4] = {};
          drawable->texture()->getBytes(actual,4,MTL::Region::Make2D(i ? 300 : 100,150,1,1),0);
          const byte expected[] = {byte(60+30*i),byte(40+40*i),byte(20+50*i),255};
          if(memcmp(actual,expected,4))
          {
            TEST_WARN("T60 draw %zu BGRA %u/%u/%u/%u",i,actual[0],actual[1],actual[2],actual[3]);
            failed = true;
          }
        }
      EndCaptureFrame(); pool->drain();
    }
    textures[0]->release(); textures[1]->release(); filter->release();
    arguments->release(); encoder->release(); pso->release(); fs->release(); vs->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
