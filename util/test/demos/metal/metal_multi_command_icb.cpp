/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "metal_test.h"

RD_TEST(Metal_Multi_Command_ICB, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Executes two ordered draws from a nonzero range of a four-slot Metal ICB.";

  struct Vertex
  {
    float position[2];
    float colour[4];
  };

  struct VertexPacket
  {
    uint32_t prefix[4];
    Vertex vertices[3];
    uint32_t suffix[4];
  };

  int main()
  {
    if(!Init())
      return 3;

    static_assert(offsetof(VertexPacket, vertices) == 16, "T22 vertex buffer offset");
    static_assert(sizeof(Vertex) == 24, "T22 vertex stride");
    static_assert(sizeof(VertexPacket) == 104, "T22 vertex packet size");

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;
struct VertexIn { float2 position [[attribute(0)]]; float4 colour [[attribute(1)]]; };
struct VSOut { float4 position [[position]]; float4 colour; };
vertex VSOut vs_main(VertexIn input [[stage_in]])
{
  VSOut output;
  output.position = float4(input.position, 0.0, 1.0);
  output.colour = input.colour;
  return output;
}
fragment float4 fs_main(VSOut input [[stage_in]]) { return input.colour; }
)EOSHADER";

    const VertexPacket packets[3] = {
        {
            {0x22000000, 0x22000001, 0x22000002, 0x22000003},
            {
                {{0.55f, 0.55f}, {0, 1, 0, 1}},
                {{0.95f, 0.55f}, {0, 1, 0, 1}},
                {{0.75f, 0.95f}, {0, 1, 0, 1}},
            },
            {0x22ff0000, 0x22ff0001, 0x22ff0002, 0x22ff0003},
        },
        {
            {0x22010000, 0x22010001, 0x22010002, 0x22010003},
            {
                {{-0.8f, -0.7f}, {1, 0.125f, 0.0625f, 1}},
                {{0.4f, -0.7f}, {1, 0.125f, 0.0625f, 1}},
                {{-0.2f, 0.8f}, {1, 0.125f, 0.0625f, 1}},
            },
            {0x22ff0100, 0x22ff0101, 0x22ff0102, 0x22ff0103},
        },
        {
            {0x22020000, 0x22020001, 0x22020002, 0x22020003},
            {
                {{-0.4f, -0.7f}, {0.0625f, 0.125f, 1, 1}},
                {{0.8f, -0.7f}, {0.0625f, 0.125f, 1, 1}},
                {{0.2f, 0.8f}, {0.0625f, 0.125f, 1, 1}},
            },
            {0x22ff0200, 0x22ff0201, 0x22ff0202, 0x22ff0203},
        },
    };

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(library == NULL)
    {
      TEST_WARN("Failed to compile T22 shader: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }
    MTL::Function *vertexFunction = library->newFunction(MTLSTR("vs_main"));
    MTL::Function *fragmentFunction = library->newFunction(MTLSTR("fs_main"));
    MTL::VertexDescriptor *vertexDesc = MTL::VertexDescriptor::alloc()->init();
    vertexDesc->attributes()->object(0)->setFormat(MTL::VertexFormatFloat2);
    vertexDesc->attributes()->object(0)->setOffset(offsetof(Vertex, position));
    vertexDesc->attributes()->object(0)->setBufferIndex(0);
    vertexDesc->attributes()->object(1)->setFormat(MTL::VertexFormatFloat4);
    vertexDesc->attributes()->object(1)->setOffset(offsetof(Vertex, colour));
    vertexDesc->attributes()->object(1)->setBufferIndex(0);
    vertexDesc->layouts()->object(0)->setStride(sizeof(Vertex));
    vertexDesc->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);
    vertexDesc->layouts()->object(0)->setStepRate(1);
    MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vertexFunction);
    pipelineDesc->setFragmentFunction(fragmentFunction);
    pipelineDesc->setVertexDescriptor(vertexDesc);
    pipelineDesc->setSupportIndirectCommandBuffers(true);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(pipelineDesc, &error);
    pipelineDesc->release();
    vertexDesc->release();

    MTL::IndirectCommandBufferDescriptor *icbDesc =
        MTL::IndirectCommandBufferDescriptor::alloc()->init();
    icbDesc->setCommandTypes(MTL::IndirectCommandTypeDraw);
    icbDesc->setInheritPipelineState(false);
    icbDesc->setInheritBuffers(false);
    icbDesc->setMaxVertexBufferBindCount(1);
    icbDesc->setMaxFragmentBufferBindCount(0);
    MTL::IndirectCommandBuffer *icb =
        device->newIndirectCommandBuffer(icbDesc, 4, MTL::ResourceStorageModeShared);
    icbDesc->release();
    MTL::Buffer *vertexBuffers[3] = {};
    for(uint32_t i = 0; i < 3; ++i)
      vertexBuffers[i] = device->newBuffer(&packets[i], sizeof(packets[i]),
                                           MTL::ResourceStorageModeShared);
    if(!pipeline || !icb || !vertexBuffers[0] || !vertexBuffers[1] || !vertexBuffers[2])
    {
      TEST_WARN("Failed to create T22 pipeline, ICB or vertex buffers");
      return 4;
    }

    for(uint32_t i = 0; i < 3; ++i)
    {
      MTL::IndirectRenderCommand *command = icb->indirectRenderCommand(i);
      command->setRenderPipelineState(pipeline);
      command->setVertexBuffer(vertexBuffers[i], offsetof(VertexPacket, vertices), 0);
      command->drawPrimitives(MTL::PrimitiveTypeTriangle, 0, 3, 1, 0);
    }

    bool validationFailed = false;
    const bool validateNative = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable)
      {
        pool->drain();
        continue;
      }
      MTL::CommandBuffer *commandBuffer = queue->commandBuffer();
      MTL::RenderPassDescriptor *pass =
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0.025, 0.035, 0.055, 1.0));
      MTL::RenderCommandEncoder *render = commandBuffer->renderCommandEncoder(pass);
      render->useResource(vertexBuffers[1], MTL::ResourceUsageRead);
      render->useResource(vertexBuffers[2], MTL::ResourceUsageRead);
      render->executeCommandsInBuffer(icb, NS::Range::Make(1, 2));
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
        auto matches = [pixels, rowPitch](size_t x, size_t y, const byte expected[4]) {
          return memcmp(pixels + y * rowPitch + x * 4, expected, 4) == 0;
        };
        const byte clear[] = {14, 9, 6, 255};
        const byte red[] = {16, 32, 255, 255};
        const byte blue[] = {255, 32, 16, 255};
        validationFailed = !matches(120, 150, red) || !matches(280, 150, blue) ||
                           !matches(200, 150, blue) || !matches(350, 37, clear) ||
                           !matches(20, 20, clear);
        if(validationFailed)
          TEST_WARN("T22 native ICB range/order pixels did not match CPU reference");
        readback->release();
      }
      pool->drain();
    }

    for(MTL::Buffer *buffer : vertexBuffers)
      buffer->release();
    icb->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
