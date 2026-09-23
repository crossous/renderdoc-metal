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

RD_TEST(Metal_Point_Line, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Draws a point, a line and a line strip from nonzero vertex starts with sentinel vertices.";

  struct Vertex
  {
    float position[2];
    float colour[4];
  };

  int main()
  {
    if(!Init())
      return 3;

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;
struct VertexIn
{
  float2 position [[attribute(0)]];
  float4 colour [[attribute(1)]];
};
struct VSOut
{
  float4 position [[position]];
  float4 colour;
  float pointSize [[point_size]];
};
vertex VSOut vs_main(VertexIn input [[stage_in]])
{
  VSOut output;
  output.position = float4(input.position, 0.0, 1.0);
  output.colour = input.colour;
  output.pointSize = 9.0;
  return output;
}
fragment float4 fs_main(VSOut input [[stage_in]]) { return input.colour; }
)EOSHADER";

    // Coordinates are for a 640x480 drawable. Sentinels are outside the viewport and
    // separate all three nonzero vertex ranges in the same interleaved buffer.
    const Vertex vertices[] = {
        {{2.0f, 2.0f}, {1, 0, 1, 1}},
        {{-0.5f, 0.5f}, {1.0f, 0.125f, 0.0625f, 1}},
        {{2.0f, 2.0f}, {1, 0, 1, 1}},
        {{-0.75f, 0.0f}, {0.0625f, 0.875f, 0.1875f, 1}},
        {{-0.25f, 0.0f}, {0.0625f, 0.875f, 0.1875f, 1}},
        {{2.0f, 2.0f}, {1, 0, 1, 1}},
        {{0.25f, -0.5f}, {0.09375f, 0.25f, 1.0f, 1}},
        {{0.5f, -0.5f}, {0.09375f, 0.25f, 1.0f, 1}},
        {{0.5f, -0.8333333f}, {0.09375f, 0.25f, 1.0f, 1}},
        {{2.0f, 2.0f}, {1, 0, 1, 1}},
        {{2.0f, 2.0f}, {1, 0, 1, 1}},
    };

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(library == NULL)
    {
      TEST_WARN("Failed to compile T15 shader: %s",
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
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipeline =
        vertexFunction && fragmentFunction ? device->newRenderPipelineState(pipelineDesc, &error)
                                           : NULL;
    pipelineDesc->release();
    vertexDesc->release();
    MTL::Buffer *vertexBuffer =
        device->newBuffer(vertices, sizeof(vertices), MTL::ResourceStorageModeShared);
    if(pipeline == NULL || vertexBuffer == NULL)
    {
      TEST_WARN("Failed to create T15 resources");
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
      render->setVertexBuffer(vertexBuffer, 0, 0);
      render->drawPrimitives(MTL::PrimitiveTypePoint, 1, 1);
      render->drawPrimitives(MTL::PrimitiveTypeLine, 3, 2);
      render->drawPrimitives(MTL::PrimitiveTypeLineStrip, 6, 3);
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
                              readback, 0, rowPitch,
                              rowPitch * drawable->texture()->height());
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
        const byte green[] = {48, 223, 16, 255};
        const byte blue[] = {255, 64, 24, 255};
        const byte background[] = {14, 9, 6, 255};
        const size_t width = drawable->texture()->width();
        const size_t height = drawable->texture()->height();
        auto matches = [&](size_t x, size_t y, const byte *expected, size_t radius) {
          for(size_t yy = y - radius; yy <= y + radius; yy++)
            for(size_t xx = x - radius; xx <= x + radius; xx++)
              if(memcmp(pixels + yy * rowPitch + xx * 4, expected, 4) == 0)
                return true;
          return false;
        };
        validationFailed = !matches(width / 4, height / 4, red, 1) ||
                           !matches(width / 4, height / 2, green, 1) ||
                           !matches(width * 11 / 16, height * 3 / 4, blue, 1) ||
                           !matches(width * 3 / 4, height * 5 / 6, blue, 1) ||
                           !matches(width / 2, height / 2, background, 0);
        if(validationFailed)
        {
          const byte *point = pixels + (height / 4) * rowPitch + (width / 4) * 4;
          const byte *line = pixels + (height / 2) * rowPitch + (width / 4) * 4;
          const byte *strip = pixels + (height * 3 / 4) * rowPitch + (width * 11 / 16) * 4;
          const byte *corner = pixels + (height * 5 / 6) * rowPitch + (width * 3 / 4) * 4;
          TEST_WARN("T15 native pixels %zux%zu: point=%02x%02x%02x%02x line=%02x%02x%02x%02x strip=%02x%02x%02x%02x corner=%02x%02x%02x%02x",
                    width, height, point[0], point[1], point[2], point[3], line[0], line[1],
                    line[2], line[3], strip[0], strip[1], strip[2], strip[3], corner[0],
                    corner[1], corner[2], corner[3]);
        }
        readback->release();
      }
      pool->drain();
    }

    vertexBuffer->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
