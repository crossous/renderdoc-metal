/******************************************************************************
 * The MIT License (MIT)
 * Copyright (c) 2026 Baldur Karlsson
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "metal_test.h"

RD_TEST(Metal_ICB_Inherit_Pipeline, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Executes one pipeline-inheriting ICB command under two distinct render pipelines.";

  struct Vertex { float position[2]; };
  int main()
  {
    if(!Init()) return 3;
    const char *source = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;
struct In { float2 position [[attribute(0)]]; };
struct Out { float4 position [[position]]; };
vertex Out vs_left(In input [[stage_in]])
{
  Out output; output.position = float4(input.position + float2(-0.45, 0), 0, 1); return output;
}
vertex Out vs_right(In input [[stage_in]])
{
  Out output; output.position = float4(input.position + float2(0.45, 0), 0, 1); return output;
}
fragment float4 fs_red() { return float4(1, 0.125, 0.0625, 1); }
fragment float4 fs_blue() { return float4(0.0625, 0.125, 1, 1); }
)EOSHADER";
    const Vertex vertices[] = {{{-0.22f, -0.40f}}, {{0.22f, -0.40f}}, {{0, 0.40f}}};
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!library) { TEST_WARN("T26 shader compile failed"); return 4; }
    MTL::Function *vs[2] = {library->newFunction(MTLSTR("vs_left")),
                            library->newFunction(MTLSTR("vs_right"))};
    MTL::Function *fs[2] = {library->newFunction(MTLSTR("fs_red")),
                            library->newFunction(MTLSTR("fs_blue"))};
    MTL::VertexDescriptor *vertexDesc = MTL::VertexDescriptor::alloc()->init();
    vertexDesc->attributes()->object(0)->setFormat(MTL::VertexFormatFloat2);
    vertexDesc->attributes()->object(0)->setBufferIndex(0);
    vertexDesc->layouts()->object(0)->setStride(sizeof(Vertex));
    MTL::RenderPipelineState *pipelines[2] = {};
    for(unsigned i = 0; i < 2; ++i)
    {
      MTL::RenderPipelineDescriptor *desc = MTL::RenderPipelineDescriptor::alloc()->init();
      desc->setVertexFunction(vs[i]);
      desc->setFragmentFunction(fs[i]);
      desc->setVertexDescriptor(vertexDesc);
      desc->setSupportIndirectCommandBuffers(true);
      desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
      pipelines[i] = device->newRenderPipelineState(desc, &error);
      desc->release();
    }
    vertexDesc->release();
    MTL::Buffer *buffer = device->newBuffer(vertices, sizeof(vertices), MTL::ResourceStorageModeShared);
    MTL::IndirectCommandBufferDescriptor *desc = MTL::IndirectCommandBufferDescriptor::alloc()->init();
    desc->setCommandTypes(MTL::IndirectCommandTypeDraw);
    desc->setInheritPipelineState(true);
    desc->setInheritBuffers(false);
    desc->setMaxVertexBufferBindCount(1);
    desc->setMaxFragmentBufferBindCount(0);
    MTL::IndirectCommandBuffer *icb = device->newIndirectCommandBuffer(desc, 1,
                                                                          MTL::ResourceStorageModeShared);
    desc->release();
    if(!pipelines[0] || !pipelines[1] || !buffer || !icb)
    { TEST_WARN("T26 resource creation failed"); return 4; }
    MTL::IndirectRenderCommand *command = icb->indirectRenderCommand(0);
    command->setVertexBuffer(buffer, 0, 0);
    command->drawPrimitives(MTL::PrimitiveTypeTriangle, 0, 3, 1, 0);

    bool validationFailed = false;
    const bool validateNative = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *commandBuffer = queue->commandBuffer();
      MTL::RenderPassDescriptor *pass = MakeBackbufferRenderPass(
          drawable, MTL::ClearColor::Make(0.025, 0.035, 0.055, 1));
      MTL::RenderCommandEncoder *render = commandBuffer->renderCommandEncoder(pass);
      render->useResource(buffer, MTL::ResourceUsageRead);
      for(unsigned i = 0; i < 2; ++i)
      {
        render->setRenderPipelineState(pipelines[i]);
        render->executeCommandsInBuffer(icb, NS::Range::Make(0, 1));
      }
      render->endEncoding();
      MTL::Buffer *readback = NULL;
      size_t rowPitch = 0;
      if(validateNative)
      {
        rowPitch = AlignUp<size_t>(drawable->texture()->width() * 4, 256);
        readback = device->newBuffer(rowPitch * drawable->texture()->height(),
                                     MTL::ResourceStorageModeShared);
        MTL::BlitCommandEncoder *blit = commandBuffer->blitCommandEncoder();
        blit->copyFromTexture(drawable->texture(), 0, 0, MTL::Origin::Make(0, 0, 0),
                              MTL::Size::Make(drawable->texture()->width(),
                                              drawable->texture()->height(), 1),
                              readback, 0, rowPitch, rowPitch * drawable->texture()->height());
        blit->endEncoding();
      }
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();
      EndCaptureFrame();
      if(readback)
      {
        const byte *pixels = (const byte *)readback->contents();
        const byte red[] = {16, 32, 255, 255};
        const byte blue[] = {255, 32, 16, 255};
        const byte clear[] = {14, 9, 6, 255};
        auto matches = [pixels, rowPitch](size_t x, size_t y, const byte expected[4]) {
          return memcmp(pixels + y * rowPitch + x * 4, expected, 4) == 0;
        };
        validationFailed = !matches(110, 150, red) || !matches(290, 150, blue) ||
                           !matches(200, 150, clear);
        if(validationFailed) TEST_WARN("T26 native inherited pipeline pixels differ");
        readback->release();
      }
      pool->drain();
    }
    icb->release(); buffer->release();
    for(unsigned i = 0; i < 2; ++i) { pipelines[i]->release(); vs[i]->release(); fs[i]->release(); }
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
