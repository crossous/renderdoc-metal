// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_Vertex_Dynamic_Stride, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Four vertex dynamic-stride bindings, batch static stride and offset-only update.";

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct Input { float2 position [[attribute(0)]]; float4 color [[attribute(1)]]; };
struct Output { float4 position [[position]]; float4 color; };
vertex Output vs_dynamic_stride(Input input [[stage_in]]) {
  return {float4(input.position,0,1),input.color};
}
fragment float4 fs_dynamic_stride(Output input [[stage_in]]) { return input.color; }
)";
    NS::Error *error = NULL;
    MTL::Library *lib = device->newLibrary(NS::String::string(source,NS::UTF8StringEncoding),
                                           NULL,&error);
    if(!lib) return 4;
    MTL::Function *vs = lib->newFunction(MTLSTR("vs_dynamic_stride"));
    MTL::Function *fs = lib->newFunction(MTLSTR("fs_dynamic_stride"));
    MTL::VertexDescriptor *vd = MTL::VertexDescriptor::alloc()->init();
    vd->attributes()->object(0)->setFormat(MTL::VertexFormatFloat2);
    vd->attributes()->object(0)->setBufferIndex(0);
    vd->attributes()->object(1)->setFormat(MTL::VertexFormatFloat4);
    vd->attributes()->object(1)->setBufferIndex(1);
    vd->layouts()->object(0)->setStride(MTL::BufferLayoutStrideDynamic);
    vd->layouts()->object(1)->setStride(16);
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs); pd->setVertexDescriptor(vd);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pso = device->newRenderPipelineState(pd,&error);
    pd->release(); vd->release();
    if(!pso) { TEST_WARN("T63 dynamic stride pipeline unavailable"); return 4; }

    struct Position { float x,y,pad0,pad1; };
    Position positions[4][3] = {};
    for(unsigned phase = 0; phase < 4; phase++)
    {
      positions[phase][0] = {-1,-1,0,0};
      positions[phase][1] = {3,-1,0,0};
      positions[phase][2] = {-1,3,0,0};
    }
    // Segment 2 is intentionally offscreen; phase 2 must select segment 3 by offset-only update.
    for(Position &position : positions[2]) position.x += 8;
    struct Color { float r,g,b,a; } colors[4][3] = {};
    const byte rgb[4][3] = {{10,20,30},{40,50,60},{70,80,90},{100,110,120}};
    for(unsigned phase = 0; phase < 4; phase++)
      for(Color &color : colors[phase])
        color = {rgb[phase][0]/255.0f,rgb[phase][1]/255.0f,rgb[phase][2]/255.0f,1};
    MTL::Buffer *positionBuffer = device->newBuffer(positions,sizeof(positions),
                                                     MTL::ResourceStorageModeShared);
    MTL::Buffer *colorBuffer = device->newBuffer(colors,sizeof(colors),
                                                  MTL::ResourceStorageModeShared);
    if(!positionBuffer || !colorBuffer) return 4;
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    bool failed = false;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      for(unsigned phase = 0; phase < 4; phase++)
      {
        MTL::CommandBuffer *cb = queue->commandBuffer();
        MTL::RenderCommandEncoder *render = cb->renderCommandEncoder(
            MakeBackbufferRenderPass(drawable,MTL::ClearColor::Make(0,0,0,1)));
        render->setRenderPipelineState(pso);
        if(phase == 0)
        {
          const MTL::VertexAmplificationViewMapping mapping = {0,0};
          render->setVertexAmplificationCount(1,&mapping);
          render->setVertexBuffer(positionBuffer,0,16,0);
          // Rebinding a pipeline in the same encoder must preserve the dynamic stride.
          render->setRenderPipelineState(pso);
          render->setVertexBuffer(colorBuffer,0,1);
        }
        else if(phase == 1)
        {
          const MTL::Buffer *buffers[] = {positionBuffer,colorBuffer};
          const NS::UInteger offsets[] = {48,48};
          const NS::UInteger strides[] = {16,MTL::AttributeStrideStatic};
          render->setVertexBuffers(buffers,offsets,strides,NS::Range::Make(0,2));
        }
        else if(phase == 2)
        {
          render->setVertexBuffer(positionBuffer,96,16,0);
          render->setVertexBufferOffset(144,16,0);
          render->setVertexBuffer(colorBuffer,96,1);
        }
        else
        {
          render->setVertexBytes(positions[0],sizeof(positions[0]),16,0);
          render->setVertexBuffer(colorBuffer,144,1);
        }
        render->drawPrimitives(MTL::PrimitiveTypeTriangle,NS::UInteger(0),NS::UInteger(3));
        render->endEncoding();
        if(phase == 3) cb->presentDrawable(drawable);
        cb->commit(); cb->waitUntilCompleted();
        if(cb->error()) failed = true;
        if(native)
        {
          byte actual[4] = {};
          drawable->texture()->getBytes(actual,4,MTL::Region::Make2D(200,150,1,1),0);
          const byte expected[4] = {rgb[phase][2],rgb[phase][1],rgb[phase][0],255};
          if(memcmp(actual,expected,4))
          {
            TEST_WARN("T63 phase %u BGRA %u/%u/%u/%u",phase,
                      actual[0],actual[1],actual[2],actual[3]);
            failed = true;
          }
        }
      }
      EndCaptureFrame(); pool->drain();
    }
    colorBuffer->release(); positionBuffer->release(); pso->release();
    fs->release(); vs->release(); lib->release();
    return failed ? 5 : 0;
  }
};
REGISTER_TEST();
