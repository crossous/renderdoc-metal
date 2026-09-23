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

RD_TEST(Metal_Fragment_Storage_Buffer, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Reads four quadrant colours through a fragment device pointer at buffer slot 3 and offset 256.";

  int main()
  {
    if(!Init())
      return 3;

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;

struct VSOut { float4 position [[position]]; float2 uv; };

vertex VSOut vs_main(uint vertexID [[vertex_id]])
{
  const float2 positions[] = {float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};
  VSOut output;
  output.position = float4(positions[vertexID], 0.0, 1.0);
  output.uv = (positions[vertexID] + 1.0) * 0.5;
  return output;
}

fragment float4 fs_main(VSOut input [[stage_in]],
                        device const float4 *colours [[buffer(3)]],
                        device const float4 *unusedColours [[buffer(5)]])
{
  uint quadrant = uint(input.uv.x >= 0.5) + 2 * uint(input.uv.y >= 0.5);
  return colours[quadrant];
}
)EOSHADER";

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(!library)
    {
      TEST_WARN("Failed to compile T18 shader: %s",
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

    byte storageBytes[640] = {};
    const float guard[4] = {1.0f, 0.0f, 1.0f, 1.0f};
    const float colours[4][4] = {{1.0f, 0.125f, 0.0625f, 1.0f},
                                 {0.0625f, 0.875f, 0.1875f, 1.0f},
                                 {0.125f, 0.25f, 1.0f, 1.0f},
                                 {0.9375f, 0.8125f, 0.0f, 1.0f}};
    memcpy(storageBytes, guard, sizeof(guard));
    memcpy(storageBytes + 256, colours, sizeof(colours));
    memcpy(storageBytes + 256 + sizeof(colours), guard, sizeof(guard));
    MTL::Buffer *storage =
        device->newBuffer(storageBytes, sizeof(storageBytes), MTL::ResourceStorageModeShared);
    if(!pipeline || !storage)
    {
      TEST_WARN("Failed to create T18 pipeline or storage buffer");
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
      render->setFragmentBuffer(storage, 256, 3);
      render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
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
            TEST_WARN("T18 pixel %zu actual %02x%02x%02x%02x expected %02x%02x%02x%02x", i,
                      actual[0], actual[1], actual[2], actual[3], expected[i][0], expected[i][1],
                      expected[i][2], expected[i][3]);
        }
        if(validationFailed)
          TEST_WARN("T18 native storage-buffer output did not match fixed BGRA pixels");
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
