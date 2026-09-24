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

RD_TEST(Metal_Indirect_Command_Buffer, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Executes one CPU-encoded non-indexed draw from a two-slot Metal indirect command buffer.";

  struct Vertex
  {
    float position[2];
    float colour[4];
  };

  struct VertexPacket
  {
    uint32_t prefix[4];
    Vertex vertices[5];
    uint32_t suffix[4];
  };

  int main()
  {
    if(!Init())
      return 3;

    static_assert(offsetof(VertexPacket, vertices) == 16, "T20 vertex buffer offset");
    static_assert(sizeof(Vertex) == 24, "T20 vertex stride");

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

    const VertexPacket packet = {
        {0x13572468, 0x24681357, 0x89abcdef, 0xfedcba98},
        {
            {{2.0f, 2.0f}, {0, 1, 0, 1}},
            {{-0.6f, -0.5f}, {1, 0.125f, 0.0625f, 1}},
            {{0.6f, -0.5f}, {1, 0.125f, 0.0625f, 1}},
            {{0.0f, 0.65f}, {1, 0.125f, 0.0625f, 1}},
            {{-2.0f, -2.0f}, {0, 0, 1, 1}},
        },
        {0x10203040, 0x50607080, 0x90a0b0c0, 0xd0e0f000},
    };

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(library == NULL)
    {
      TEST_WARN("Failed to compile T20 shader: %s",
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
        device->newIndirectCommandBuffer(icbDesc, 2, MTL::ResourceStorageModeShared);
    icbDesc->release();
    MTL::Buffer *vertexBuffer =
        device->newBuffer(&packet, sizeof(packet), MTL::ResourceStorageModeShared);
    if(!pipeline || !icb || !vertexBuffer)
    {
      TEST_WARN("Failed to create T20 pipeline, ICB or vertex buffer");
      return 4;
    }

    MTL::IndirectRenderCommand *command = icb->indirectRenderCommand(0);
    command->setRenderPipelineState(pipeline);
    command->setVertexBuffer(vertexBuffer, offsetof(VertexPacket, vertices), 0);
    command->drawPrimitives(MTL::PrimitiveTypeTriangle, 1, 3, 1, 0);

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
      render->useResource(vertexBuffer, MTL::ResourceUsageRead);
      render->executeCommandsInBuffer(icb, NS::Range::Make(0, 1));
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
        const size_t height = drawable->texture()->height();
        const size_t width = drawable->texture()->width();
        const byte *center = pixels + (height / 2) * rowPitch + (width / 2) * 4;
        const byte *corner = pixels + (height / 8) * rowPitch + (width / 8) * 4;
        const byte expectedCenter[] = {16, 32, 255, 255};
        const byte expectedCorner[] = {14, 9, 6, 255};
        validationFailed = memcmp(center, expectedCenter, 4) != 0 ||
                           memcmp(corner, expectedCorner, 4) != 0;
        if(validationFailed)
          TEST_WARN("T20 native ICB draw did not match fixed BGRA pixels");
        readback->release();
      }
      pool->drain();
    }

    vertexBuffer->release();
    icb->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
