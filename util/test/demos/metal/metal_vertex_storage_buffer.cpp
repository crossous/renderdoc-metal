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

RD_TEST(Metal_Vertex_Storage_Buffer, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Reads four quadrant positions through a vertex device pointer at buffer slot 4 and offset 256.";

  int main()
  {
    if(!Init())
      return 3;

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;

struct VSOut { float4 position [[position]]; float4 colour; };

vertex VSOut vs_main(uint vertexID [[vertex_id]],
                     device const float4 *positions [[buffer(4)]],
                     device const float4 *unusedPositions [[buffer(6)]])
{
  const float4 colours[] = {float4(1.0, 0.125, 0.0625, 1.0),
                            float4(0.0625, 0.875, 0.1875, 1.0),
                            float4(0.125, 0.25, 1.0, 1.0),
                            float4(0.9375, 0.8125, 0.0, 1.0)};
  VSOut output;
  output.position = positions[vertexID];
  output.colour = colours[vertexID / 6];
  return output;
}

fragment float4 fs_main(VSOut input [[stage_in]])
{
  return input.colour;
}
)EOSHADER";

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(!library)
    {
      TEST_WARN("Failed to compile T19 shader: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }
    MTL::Function *vertexFunction = library->newFunction(MTLSTR("vs_main"));
    MTL::Function *fragmentFunction = library->newFunction(MTLSTR("fs_main"));
    MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vertexFunction);
    pipelineDesc->setFragmentFunction(fragmentFunction);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipeline =
        vertexFunction && fragmentFunction ? device->newRenderPipelineState(pipelineDesc, &error)
                                           : NULL;
    pipelineDesc->release();

    byte storageBytes[768] = {};
    const float guard[4] = {2.0f, 2.0f, 2.0f, 2.0f};
    const float positions[24][4] = {
        {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
        {-1.0f, -1.0f, 0.0f, 1.0f}, {-1.0f, -1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, -1.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, -1.0f, 0.0f, 1.0f},
        {-1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f},
        {-1.0f, 0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
    };
    memcpy(storageBytes, guard, sizeof(guard));
    memcpy(storageBytes + 256, positions, sizeof(positions));
    memcpy(storageBytes + 256 + sizeof(positions), guard, sizeof(guard));
    MTL::Buffer *storage =
        device->newBuffer(storageBytes, sizeof(storageBytes), MTL::ResourceStorageModeShared);
    if(!pipeline || !storage)
    {
      TEST_WARN("Failed to create T19 pipeline or storage buffer");
      return 4;
    }

    const bool validateNative = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    bool validationFailed = false;
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
      render->setRenderPipelineState(pipeline);
      render->setVertexBuffer(storage, 256, 4);
      render->setVertexBuffer(storage, 320, 6);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(24));
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
        const byte expected[4][4] = {{255, 64, 32, 255}, {0, 207, 239, 255},
                                     {16, 32, 255, 255}, {48, 223, 16, 255}};
        for(size_t i = 0; i < 4; i++)
        {
          const size_t x = (i % 2 ? 3 : 1) * drawable->texture()->width() / 4;
          const size_t y = (i / 2 ? 3 : 1) * drawable->texture()->height() / 4;
          const byte *actual = pixels + y * rowPitch + x * 4;
          validationFailed |= memcmp(actual, expected[i], 4) != 0;
          if(memcmp(actual, expected[i], 4) != 0)
            TEST_WARN("T19 pixel %zu actual %02x%02x%02x%02x expected %02x%02x%02x%02x", i,
                      actual[0], actual[1], actual[2], actual[3], expected[i][0], expected[i][1],
                      expected[i][2], expected[i][3]);
        }
        if(validationFailed)
          TEST_WARN("T19 native vertex storage-buffer output did not match fixed BGRA pixels");
        readback->release();
      }
      pool->drain();
    }

    storage->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
