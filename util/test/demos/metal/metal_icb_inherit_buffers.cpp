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

RD_TEST(Metal_ICB_Inherit_Buffers, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Executes one buffer-inheriting ICB command with two outer vertex bindings.";
  struct Vertex { float position[2]; float colour[4]; };
  struct Packet { uint32_t prefix[4]; Vertex vertices[3]; uint32_t suffix[4]; };
  int main()
  {
    if(!Init()) return 3;
    static_assert(sizeof(Packet) == 104, "T27 packet size");
    const char *source = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;
struct In { float2 position [[attribute(0)]]; float4 colour [[attribute(1)]]; };
struct Out { float4 position [[position]]; float4 colour; };
vertex Out vs_main(In input [[stage_in]])
{
  Out output; output.position = float4(input.position, 0, 1);
  output.colour = input.colour; return output;
}
fragment float4 fs_main(Out input [[stage_in]]) { return input.colour; }
)EOSHADER";
    const Packet packets[2] = {
        {{0x27000000, 1, 2, 3},
         {{{-0.80f, -0.40f}, {1, .125f, .0625f, 1}},
          {{-0.30f, -0.40f}, {1, .125f, .0625f, 1}},
          {{-0.55f, 0.40f}, {1, .125f, .0625f, 1}}},
         {0x27ff0000, 4, 5, 6}},
        {{0x27010000, 7, 8, 9},
         {{{0.30f, -0.40f}, {.0625f, .125f, 1, 1}},
          {{0.80f, -0.40f}, {.0625f, .125f, 1, 1}},
          {{0.55f, 0.40f}, {.0625f, .125f, 1, 1}}},
         {0x27ff0100, 10, 11, 12}},
    };
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!library) { TEST_WARN("T27 shader compile failed"); return 4; }
    MTL::Function *vs = library->newFunction(MTLSTR("vs_main"));
    MTL::Function *fs = library->newFunction(MTLSTR("fs_main"));
    MTL::VertexDescriptor *vertexDesc = MTL::VertexDescriptor::alloc()->init();
    vertexDesc->attributes()->object(0)->setFormat(MTL::VertexFormatFloat2);
    vertexDesc->attributes()->object(0)->setBufferIndex(0);
    vertexDesc->attributes()->object(1)->setFormat(MTL::VertexFormatFloat4);
    vertexDesc->attributes()->object(1)->setOffset(8);
    vertexDesc->attributes()->object(1)->setBufferIndex(0);
    vertexDesc->layouts()->object(0)->setStride(sizeof(Vertex));
    MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vs);
    pipelineDesc->setFragmentFunction(fs);
    pipelineDesc->setVertexDescriptor(vertexDesc);
    pipelineDesc->setSupportIndirectCommandBuffers(true);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(pipelineDesc, &error);
    pipelineDesc->release(); vertexDesc->release();
    MTL::Buffer *buffers[2] = {
        device->newBuffer(&packets[0], sizeof(Packet), MTL::ResourceStorageModeShared),
        device->newBuffer(&packets[1], sizeof(Packet), MTL::ResourceStorageModeShared)};
    MTL::IndirectCommandBufferDescriptor *desc = MTL::IndirectCommandBufferDescriptor::alloc()->init();
    desc->setCommandTypes(MTL::IndirectCommandTypeDraw);
    desc->setInheritPipelineState(false);
    desc->setInheritBuffers(true);
    desc->setMaxVertexBufferBindCount(0);
    desc->setMaxFragmentBufferBindCount(0);
    MTL::IndirectCommandBuffer *icb = device->newIndirectCommandBuffer(desc, 1,
                                                                          MTL::ResourceStorageModeShared);
    desc->release();
    if(!pipeline || !buffers[0] || !buffers[1] || !icb)
    { TEST_WARN("T27 resource creation failed"); return 4; }
    MTL::IndirectRenderCommand *command = icb->indirectRenderCommand(0);
    command->setRenderPipelineState(pipeline);
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
          drawable, MTL::ClearColor::Make(.025, .035, .055, 1));
      MTL::RenderCommandEncoder *render = commandBuffer->renderCommandEncoder(pass);
      for(unsigned i = 0; i < 2; ++i)
      {
        render->setVertexBuffer(buffers[i], offsetof(Packet, vertices), 0);
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
        if(validationFailed) TEST_WARN("T27 native inherited buffer pixels differ");
        readback->release();
      }
      pool->drain();
    }
    icb->release(); buffers[0]->release(); buffers[1]->release(); pipeline->release();
    fs->release(); vs->release(); library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
