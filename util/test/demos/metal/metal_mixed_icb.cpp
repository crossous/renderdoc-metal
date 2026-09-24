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

RD_TEST(Metal_Mixed_ICB, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Executes non-indexed and indexed draws from one mixed Metal ICB.";

  struct DirectVertex
  {
    float position[2];
    float colour[4];
  };
  struct DirectPacket
  {
    uint32_t prefix[4];
    DirectVertex vertices[3];
    uint32_t suffix[4];
  };
  struct Position
  {
    float position[2];
  };
  struct Instance
  {
    float offset[2];
    float colour[4];
  };

  int main()
  {
    if(!Init())
      return 3;

    static_assert(sizeof(DirectPacket) == 104, "T25 direct packet size");
    static_assert(offsetof(DirectPacket, vertices) == 16, "T25 direct packet offset");

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;
struct DirectIn { float2 position [[attribute(0)]]; float4 colour [[attribute(1)]]; };
struct IndexedIn
{
  float2 position [[attribute(0)]];
  float2 instanceOffset [[attribute(1)]];
  float4 instanceColour [[attribute(2)]];
};
struct VSOut { float4 position [[position]]; float4 colour; };
vertex VSOut vs_direct(DirectIn input [[stage_in]])
{
  VSOut output;
  output.position = float4(input.position, 0.0, 1.0);
  output.colour = input.colour;
  return output;
}
vertex VSOut vs_indexed(IndexedIn input [[stage_in]])
{
  VSOut output;
  output.position = float4(input.position + input.instanceOffset, 0.0, 1.0);
  output.colour = input.instanceColour;
  return output;
}
fragment float4 fs_main(VSOut input [[stage_in]]) { return input.colour; }
)EOSHADER";

    const DirectPacket direct = {
        {0x25000000, 0x25000001, 0x25000002, 0x25000003},
        {
            {{-0.95f, -0.55f}, {1, 0.125f, 0.0625f, 1}},
            {{-0.35f, -0.55f}, {1, 0.125f, 0.0625f, 1}},
            {{-0.65f, 0.55f}, {1, 0.125f, 0.0625f, 1}},
        },
        {0x25ff0000, 0x25ff0001, 0x25ff0002, 0x25ff0003},
    };
    const Position positions[] = {
        {{2.0f, 2.0f}}, {{-0.12f, -0.18f}}, {{0.12f, -0.18f}},
        {{0.0f, 0.22f}}, {{-2.0f, -2.0f}},
    };
    const uint16_t indices[] = {4, 4, 0, 1, 2, 4};
    const Instance instances[] = {
        {{0.0f, 1.4f}, {1.0f, 0.0f, 1.0f, 1.0f}},
        {{0.35f, 0.0f}, {0.0625f, 1.0f, 0.125f, 1.0f}},
        {{0.72f, 0.0f}, {0.09375f, 0.25f, 1.0f, 1.0f}},
        {{0.0f, -1.4f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    };

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(!library)
    {
      TEST_WARN("Failed to compile T25 shader: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }
    MTL::Function *directFunction = library->newFunction(MTLSTR("vs_direct"));
    MTL::Function *indexedFunction = library->newFunction(MTLSTR("vs_indexed"));
    MTL::Function *fragmentFunction = library->newFunction(MTLSTR("fs_main"));

    MTL::VertexDescriptor *directVertexDesc = MTL::VertexDescriptor::alloc()->init();
    directVertexDesc->attributes()->object(0)->setFormat(MTL::VertexFormatFloat2);
    directVertexDesc->attributes()->object(0)->setOffset(offsetof(DirectVertex, position));
    directVertexDesc->attributes()->object(0)->setBufferIndex(0);
    directVertexDesc->attributes()->object(1)->setFormat(MTL::VertexFormatFloat4);
    directVertexDesc->attributes()->object(1)->setOffset(offsetof(DirectVertex, colour));
    directVertexDesc->attributes()->object(1)->setBufferIndex(0);
    directVertexDesc->layouts()->object(0)->setStride(sizeof(DirectVertex));
    directVertexDesc->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);
    directVertexDesc->layouts()->object(0)->setStepRate(1);

    MTL::VertexDescriptor *indexedVertexDesc = MTL::VertexDescriptor::alloc()->init();
    indexedVertexDesc->attributes()->object(0)->setFormat(MTL::VertexFormatFloat2);
    indexedVertexDesc->attributes()->object(0)->setOffset(offsetof(Position, position));
    indexedVertexDesc->attributes()->object(0)->setBufferIndex(0);
    indexedVertexDesc->attributes()->object(1)->setFormat(MTL::VertexFormatFloat2);
    indexedVertexDesc->attributes()->object(1)->setOffset(offsetof(Instance, offset));
    indexedVertexDesc->attributes()->object(1)->setBufferIndex(1);
    indexedVertexDesc->attributes()->object(2)->setFormat(MTL::VertexFormatFloat4);
    indexedVertexDesc->attributes()->object(2)->setOffset(offsetof(Instance, colour));
    indexedVertexDesc->attributes()->object(2)->setBufferIndex(1);
    indexedVertexDesc->layouts()->object(0)->setStride(sizeof(Position));
    indexedVertexDesc->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);
    indexedVertexDesc->layouts()->object(0)->setStepRate(1);
    indexedVertexDesc->layouts()->object(1)->setStride(sizeof(Instance));
    indexedVertexDesc->layouts()->object(1)->setStepFunction(MTL::VertexStepFunctionPerInstance);
    indexedVertexDesc->layouts()->object(1)->setStepRate(1);

    auto makePipeline = [&](MTL::Function *vertex, MTL::VertexDescriptor *vertexDesc) {
      MTL::RenderPipelineDescriptor *desc = MTL::RenderPipelineDescriptor::alloc()->init();
      desc->setVertexFunction(vertex);
      desc->setFragmentFunction(fragmentFunction);
      desc->setVertexDescriptor(vertexDesc);
      desc->setSupportIndirectCommandBuffers(true);
      desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
      MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(desc, &error);
      desc->release();
      return pipeline;
    };
    MTL::RenderPipelineState *directPipeline = makePipeline(directFunction, directVertexDesc);
    MTL::RenderPipelineState *indexedPipeline = makePipeline(indexedFunction, indexedVertexDesc);
    directVertexDesc->release();
    indexedVertexDesc->release();

    MTL::Buffer *directBuffer =
        device->newBuffer(&direct, sizeof(direct), MTL::ResourceStorageModeShared);
    MTL::Buffer *positionBuffer =
        device->newBuffer(positions, sizeof(positions), MTL::ResourceStorageModeShared);
    MTL::Buffer *instanceBuffer =
        device->newBuffer(instances, sizeof(instances), MTL::ResourceStorageModeShared);
    MTL::Buffer *indexBuffer =
        device->newBuffer(indices, sizeof(indices), MTL::ResourceStorageModeShared);
    MTL::IndirectCommandBufferDescriptor *icbDesc =
        MTL::IndirectCommandBufferDescriptor::alloc()->init();
    icbDesc->setCommandTypes((MTL::IndirectCommandType)(MTL::IndirectCommandTypeDraw |
                                                        MTL::IndirectCommandTypeDrawIndexed));
    icbDesc->setInheritPipelineState(false);
    icbDesc->setInheritBuffers(false);
    icbDesc->setMaxVertexBufferBindCount(2);
    icbDesc->setMaxFragmentBufferBindCount(0);
    MTL::IndirectCommandBuffer *icb =
        device->newIndirectCommandBuffer(icbDesc, 2, MTL::ResourceStorageModeShared);
    icbDesc->release();
    if(!directPipeline || !indexedPipeline || !directBuffer || !positionBuffer ||
       !instanceBuffer || !indexBuffer || !icb)
    {
      TEST_WARN("Failed to create T25 mixed ICB resources");
      return 4;
    }

    MTL::IndirectRenderCommand *directCommand = icb->indirectRenderCommand(0);
    directCommand->setRenderPipelineState(directPipeline);
    directCommand->setVertexBuffer(directBuffer, offsetof(DirectPacket, vertices), 0);
    directCommand->drawPrimitives(MTL::PrimitiveTypeTriangle, 0, 3, 1, 0);
    MTL::IndirectRenderCommand *indexedCommand = icb->indirectRenderCommand(1);
    indexedCommand->setRenderPipelineState(indexedPipeline);
    indexedCommand->setVertexBuffer(positionBuffer, 0, 0);
    indexedCommand->setVertexBuffer(instanceBuffer, 0, 1);
    indexedCommand->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, 3,
                                          MTL::IndexTypeUInt16, indexBuffer, 4, 2, 1, 1);

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
      render->useResource(directBuffer, MTL::ResourceUsageRead);
      render->useResource(positionBuffer, MTL::ResourceUsageRead);
      render->useResource(instanceBuffer, MTL::ResourceUsageRead);
      render->useResource(indexBuffer, MTL::ResourceUsageRead);
      render->executeCommandsInBuffer(icb, NS::Range::Make(0, 2));
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
        const byte green[] = {32, 255, 16, 255};
        const byte blue[] = {255, 64, 24, 255};
        validationFailed = !matches(70, 150, red) || !matches(270, 150, green) ||
                           !matches(344, 150, blue) || !matches(200, 150, clear) ||
                           !matches(20, 20, clear);
        if(validationFailed)
          TEST_WARN("T25 native mixed ICB pixels did not match CPU reference");
        readback->release();
      }
      pool->drain();
    }

    icb->release();
    indexBuffer->release();
    instanceBuffer->release();
    positionBuffer->release();
    directBuffer->release();
    indexedPipeline->release();
    directPipeline->release();
    fragmentFunction->release();
    indexedFunction->release();
    directFunction->release();
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
