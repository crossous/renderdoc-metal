// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Argument_Data, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Two argument packets with pointer arrays, inline constants and inter-submission CPU updates.";
  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct Packet {
  const device uint4 *single [[id(0)]];
  array<const device uint4 *, 2> inputs [[id(2)]];
  array<texture2d<float>, 2> colours [[id(4)]];
  array<sampler, 2> filters [[id(6)]];
  uint4 bias [[id(8)]];
  uint delta [[id(9)]];
};
vertex float4 vs_argument_data(uint id [[vertex_id]]) {
  const float2 p[] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
  return float4(p[id],0,1);
}
fragment float4 fs_argument_data(constant Packet &packet [[buffer(0)]]) {
  uint4 value = packet.single[0] + packet.inputs[0][0] + packet.inputs[1][0] + packet.bias;
  value += uint4(round(packet.colours[0].sample(packet.filters[0],float2(0.5))*255.0));
  value += uint4(round(packet.colours[1].sample(packet.filters[1],float2(0.5))*255.0));
  value.xyz += packet.delta;
  return float4(float3(value.xyz),255.0)/255.0;
}
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),NULL,&error);
    if(!lib) { TEST_WARN("T57 shader: %s",error->localizedDescription()->utf8String()); return 4; }
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_argument_data"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_argument_data"));
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pso = device->newRenderPipelineState(pd,&error);
    pd->release();
    if(!pso) return 4;
    MTL::ArgumentEncoder *encoder = fs->newArgumentEncoder(0);
    if(!encoder) return 4;
    const size_t start = 256, stride = encoder->encodedLength();
    MTL::Buffer *arguments = device->newBuffer(start + 2*stride + 173,MTL::ResourceStorageModeShared);
    memset(arguments->contents(),0xa5,arguments->length());
    MTL::Buffer *inputs[3] = {};
    const size_t lengths[] = {117,181,213};
    const size_t offsets[2][3] = {{16,32,48},{32,48,64}};
    for(size_t i = 0; i < 3; i++)
    {
      inputs[i] = device->newBuffer(lengths[i],MTL::ResourceStorageModeShared);
      memset(inputs[i]->contents(),0xa5,lengths[i]);
      for(size_t packet = 0; packet < 2; packet++)
      {
        const uint32_t v = uint32_t(1 + i*3 + packet*10);
        const uint32_t data[] = {v,v+1,v+2,0};
        memcpy((byte *)inputs[i]->contents()+offsets[packet][i],data,16);
      }
    }
    MTL::Texture *textures[2] = {};
    MTL::SamplerState *samplers[2] = {};
    for(size_t i = 0; i < 2; i++)
    {
      MTL::TextureDescriptor *td = MTL::TextureDescriptor::texture2DDescriptor(
          MTL::PixelFormatRGBA8Unorm,1,1,false);
      td->setStorageMode(MTL::StorageModeShared); td->setUsage(MTL::TextureUsageShaderRead);
      textures[i] = device->newTexture(td);
      const uint8_t pixel[] = {uint8_t(2+i),uint8_t(3+i),uint8_t(4+i),255};
      textures[i]->replaceRegion(MTL::Region::Make2D(0,0,1,1),0,pixel,4);
      MTL::SamplerDescriptor *sd = MTL::SamplerDescriptor::alloc()->init();
      sd->setSupportArgumentBuffers(true);
      sd->setMinFilter(i ? MTL::SamplerMinMagFilterLinear : MTL::SamplerMinMagFilterNearest);
      sd->setMagFilter(i ? MTL::SamplerMinMagFilterLinear : MTL::SamplerMinMagFilterNearest);
      samplers[i] = device->newSamplerState(sd); sd->release();
    }
    uint32_t *delta[2] = {};
    for(size_t packet = 0; packet < 2; packet++)
    {
      encoder->setArgumentBuffer(arguments,start,packet);
      encoder->setBuffer(NULL,0,0);
      encoder->setBuffer(inputs[0],offsets[packet][0],0);
      const MTL::Buffer *batch[] = {inputs[1],inputs[2]};
      const NS::UInteger batchOffsets[] = {offsets[packet][1],offsets[packet][2]};
      const MTL::Buffer *empty[] = {NULL,NULL};
      const NS::UInteger zeros[] = {0,0};
      encoder->setBuffers(empty,zeros,NS::Range::Make(2,2));
      encoder->setBuffers(batch,batchOffsets,NS::Range::Make(2,2));
      encoder->setBuffers(empty,zeros,NS::Range::Make(3,1));
      encoder->setBuffers(batch+1,batchOffsets+1,NS::Range::Make(3,1));
      encoder->setBuffers(batch,batchOffsets,NS::Range::Make(2,0));
      const MTL::Texture *tex[] = {textures[packet],textures[1-packet]};
      const MTL::SamplerState *samp[] = {samplers[packet],samplers[1-packet]};
      encoder->setTextures(tex,NS::Range::Make(4,2));
      encoder->setSamplerStates(samp,NS::Range::Make(6,2));
      uint32_t bias[] = {uint32_t(packet*10),uint32_t(5+packet*10),uint32_t(10+packet*10),0};
      memcpy(encoder->constantData(8),bias,16);
      delta[packet] = (uint32_t *)encoder->constantData(9);
      *delta[packet] = 0;
      TEST_LOG("T57 packet %zu offset %zu, stride %zu, delta offset %zu",packet,start+packet*stride,
               stride,size_t((byte *)delta[packet]-(byte *)arguments->contents()));
    }
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    const bool reencode = !GetEnvVar("RENDERDOC_METAL_T57_REENCODE").empty();
    bool failed = false;
    uint32_t frames = 0;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      for(uint32_t phase = 0; phase < 2; phase++)
      {
        *delta[0] = phase ? 5 : 0; *delta[1] = phase ? 11 : 0;
        const uint32_t values[] = {1+phase,2+phase,3+phase,0};
        memcpy((byte *)inputs[0]->contents()+16,values,16);
        if(reencode)
        {
          encoder->setArgumentBuffer(arguments,start,0);
          encoder->setBuffer(inputs[0],16,0);
        }
        MTL::CommandBuffer *cb = queue->commandBuffer();
        MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
            MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1)));
        render->setRenderPipelineState(pso);
        for(MTL::Buffer *buffer : inputs) render->useResource(buffer,MTL::ResourceUsageRead);
        for(MTL::Texture *texture : textures) render->useResource(texture,MTL::ResourceUsageRead);
        render->setFragmentBuffer(arguments,start,0);
        render->setScissorRect(MTL::ScissorRect{0,0,drawable->texture()->width()/2,drawable->texture()->height()});
        render->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
        render->setFragmentBufferOffset(start+stride,0);
        render->setScissorRect(MTL::ScissorRect{drawable->texture()->width()/2,0,
                                               drawable->texture()->width()/2,drawable->texture()->height()});
        render->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
        render->endEncoding();
        if(phase) cb->presentDrawable(drawable);
        cb->commit(); cb->waitUntilCompleted();
        if(cb->error()) failed = true;
        if(native)
          for(uint32_t packet = 0; packet < 2; packet++)
          {
            byte pixel[4] = {};
            drawable->texture()->getBytes(pixel,4,MTL::Region::Make2D(packet ? 300 : 100,150,1,1),0);
            const uint32_t base = packet ? 57 + phase*11 : 17 + phase*6;
            const byte expected[] = {byte(base+20),byte(base+10),byte(base),255};
            if(memcmp(pixel,expected,4))
            {
              TEST_WARN("T57 phase %u packet %u: BGRA %u/%u/%u/%u expected base %u",phase,packet,
                         pixel[0],pixel[1],pixel[2],pixel[3],base);
              failed = true;
            }
          }
      }
      EndCaptureFrame(); pool->drain(); frames++;
    }
    for(auto *buffer : inputs) buffer->release();
    for(auto *texture : textures) texture->release();
    for(auto *sampler : samplers) sampler->release();
    arguments->release(); encoder->release(); pso->release(); vs->release(); fs->release(); lib->release();
    TEST_LOG("T57 %s: %u frames, two packets, two submissions and four draws",failed ? "FAILED" : "passed",frames);
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
