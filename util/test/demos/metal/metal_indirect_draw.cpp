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

RD_TEST(Metal_Indirect_Draw, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Draws two coloured instances from one non-indexed indirect argument record.";

  struct Position
  {
    float position[2];
  };

  struct Instance
  {
    float offset[2];
    float colour[4];
  };

  struct IndirectPacket
  {
    uint32_t prefix[4];
    MTL::DrawPrimitivesIndirectArguments arguments;
    uint32_t suffix[4];
  };

  int main()
  {
    if(!Init())
      return 3;

    static_assert(sizeof(MTL::DrawPrimitivesIndirectArguments) == 16,
                  "Metal draw indirect arguments must remain four uint32 values");
    static_assert(offsetof(IndirectPacket, arguments) == 16,
                  "T13 indirect argument offset must remain deterministic");

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;

struct VertexIn
{
  float2 position [[attribute(0)]];
  float2 instanceOffset [[attribute(1)]];
  float4 instanceColour [[attribute(2)]];
};

struct VSOut
{
  float4 position [[position]];
  float4 colour;
};

vertex VSOut vs_main(VertexIn input [[stage_in]])
{
  VSOut output;
  output.position = float4(input.position + input.instanceOffset, 0.0, 1.0);
  output.colour = input.instanceColour;
  return output;
}

fragment float4 fs_main(VSOut input [[stage_in]])
{
  return input.colour;
}
)EOSHADER";

    // vertexStart=1 skips the off-screen first record. The final record is deliberately unused.
    const Position positions[] = {
        {{2.0f, 2.0f}},
        {{-0.18f, -0.18f}},
        {{0.18f, -0.18f}},
        {{0.0f, 0.22f}},
        {{-2.0f, -2.0f}},
    };

    // baseInstance=1 skips the magenta record and instanceCount=2 draws the two visible records.
    const Instance instances[] = {
        {{0.0f, 1.4f}, {1.0f, 0.0f, 1.0f, 1.0f}},
        {{-0.55f, 0.0f}, {1.0f, 0.125f, 0.0625f, 1.0f}},
        {{0.55f, 0.0f}, {0.09375f, 0.25f, 1.0f, 1.0f}},
        {{0.0f, -1.4f}, {1.0f, 1.0f, 0.0f, 1.0f}},
    };

    const IndirectPacket packet = {
        {0x13572468, 0x24681357, 0x89abcdef, 0xfedcba98},
        {3, 2, 1, 1},
        {0x10203040, 0x50607080, 0x90a0b0c0, 0xd0e0f000},
    };

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(library == NULL)
    {
      TEST_WARN("Failed to compile T13 Metal shader: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }

    MTL::Function *vertexFunction = library->newFunction(MTLSTR("vs_main"));
    MTL::Function *fragmentFunction = library->newFunction(MTLSTR("fs_main"));

    MTL::VertexDescriptor *vertexDesc = MTL::VertexDescriptor::alloc()->init();
    vertexDesc->attributes()->object(0)->setFormat(MTL::VertexFormatFloat2);
    vertexDesc->attributes()->object(0)->setOffset(offsetof(Position, position));
    vertexDesc->attributes()->object(0)->setBufferIndex(0);
    vertexDesc->attributes()->object(1)->setFormat(MTL::VertexFormatFloat2);
    vertexDesc->attributes()->object(1)->setOffset(offsetof(Instance, offset));
    vertexDesc->attributes()->object(1)->setBufferIndex(1);
    vertexDesc->attributes()->object(2)->setFormat(MTL::VertexFormatFloat4);
    vertexDesc->attributes()->object(2)->setOffset(offsetof(Instance, colour));
    vertexDesc->attributes()->object(2)->setBufferIndex(1);
    vertexDesc->layouts()->object(0)->setStride(sizeof(Position));
    vertexDesc->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);
    vertexDesc->layouts()->object(0)->setStepRate(1);
    vertexDesc->layouts()->object(1)->setStride(sizeof(Instance));
    vertexDesc->layouts()->object(1)->setStepFunction(MTL::VertexStepFunctionPerInstance);
    vertexDesc->layouts()->object(1)->setStepRate(1);

    MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vertexFunction);
    pipelineDesc->setFragmentFunction(fragmentFunction);
    pipelineDesc->setVertexDescriptor(vertexDesc);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipeline =
        vertexFunction && fragmentFunction ? device->newRenderPipelineState(pipelineDesc, &error)
                                           : NULL;
    pipelineDesc->release();
    vertexDesc->release();

    MTL::Buffer *positionBuffer =
        device->newBuffer(positions, sizeof(positions), MTL::ResourceStorageModeShared);
    MTL::Buffer *instanceBuffer =
        device->newBuffer(instances, sizeof(instances), MTL::ResourceStorageModeShared);
    MTL::Buffer *indirectBuffer =
        device->newBuffer(&packet, sizeof(packet), MTL::ResourceStorageModeShared);
    if(pipeline == NULL || positionBuffer == NULL || instanceBuffer == NULL ||
       indirectBuffer == NULL)
    {
      TEST_WARN("Failed to create T13 Metal resources");
      return 4;
    }

    bool validationFailed = false;
    const bool validateNative = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(drawable == NULL)
      {
        pool->drain();
        continue;
      }

      MTL::CommandBuffer *commandBuffer = queue->commandBuffer();
      MTL::RenderPassDescriptor *pass =
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0.025, 0.035, 0.055, 1.0));
      MTL::RenderCommandEncoder *render = commandBuffer->renderCommandEncoder(pass);
      render->setRenderPipelineState(pipeline);
      render->setVertexBuffer(positionBuffer, 0, 0);
      render->setVertexBuffer(instanceBuffer, 0, 1);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, indirectBuffer,
                             offsetof(IndirectPacket, arguments));
      render->endEncoding();

      MTL::Buffer *readback = NULL;
      size_t readbackRowPitch = 0;
      if(validateNative)
      {
        readbackRowPitch = AlignUp<size_t>(drawable->texture()->width() * 4, 256);
        readback = device->newBuffer(readbackRowPitch * drawable->texture()->height(),
                                     MTL::ResourceStorageModeShared);
        MTL::BlitCommandEncoder *blit = commandBuffer->blitCommandEncoder();
        blit->copyFromTexture(drawable->texture(), 0, 0, MTL::Origin::Make(0, 0, 0),
                              MTL::Size::Make(drawable->texture()->width(),
                                              drawable->texture()->height(), 1),
                              readback, 0, readbackRowPitch,
                              readbackRowPitch * drawable->texture()->height());
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
        const byte *left = pixels + (height / 2) * readbackRowPitch + (width / 4) * 4;
        const byte *right = pixels + (height / 2) * readbackRowPitch + (width * 3 / 4) * 4;
        const byte expectedLeft[] = {16, 32, 255, 255};
        const byte expectedRight[] = {255, 64, 24, 255};
        validationFailed = memcmp(left, expectedLeft, 4) != 0 ||
                           memcmp(right, expectedRight, 4) != 0;
        if(validationFailed)
          TEST_WARN("T13 native indirect draw did not match fixed BGRA pixels");
        readback->release();
      }
      pool->drain();
    }

    indirectBuffer->release();
    instanceBuffer->release();
    positionBuffer->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
