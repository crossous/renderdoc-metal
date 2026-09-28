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

RD_TEST(Metal_Render_Inline_Batch_Binding, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Exercises render inline bytes, batched buffers, and vertex buffer offset updates.";

  int main()
  {
    if(!Init())
      return 3;

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;

struct VSOut
{
  float4 position [[position]];
  float4 colour;
};

vertex VSOut vs_main(uint vertexID [[vertex_id]],
                     device const float2 *positions [[buffer(0)]],
                     device const float4 *colours [[buffer(1)]],
                     constant float2 &translation [[buffer(2)]])
{
  VSOut output;
  output.position = float4(positions[vertexID] + translation, 0.0, 1.0);
  output.colour = colours[vertexID];
  return output;
}

fragment float4 fs_main(VSOut input [[stage_in]],
                        constant float4 &base [[buffer(2)]],
                        device const float4 *addA [[buffer(3)]],
                        device const float4 *addB [[buffer(4)]])
{
  return input.colour + base + addA[0] + addB[0];
}
)EOSHADER";

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    MTL::Function *vertexFunction = library ? library->newFunction(MTLSTR("vs_main")) : NULL;
    MTL::Function *fragmentFunction = library ? library->newFunction(MTLSTR("fs_main")) : NULL;
    MTL::RenderPipelineDescriptor *pipelineDesc =
        MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vertexFunction);
    pipelineDesc->setFragmentFunction(fragmentFunction);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipeline =
        vertexFunction && fragmentFunction ? device->newRenderPipelineState(pipelineDesc, &error)
                                           : NULL;
    pipelineDesc->release();

    byte positionBytes[16 + sizeof(float) * 6] = {};
    const float positions[] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
    memcpy(positionBytes + 16, positions, sizeof(positions));

    byte colourBytes[256 + sizeof(float) * 12] = {};
    const float colours[] = {0.25f, 0.125f, 0.0625f, 0.5f,
                             0.25f, 0.125f, 0.0625f, 0.5f,
                             0.25f, 0.125f, 0.0625f, 0.5f};
    memcpy(colourBytes + 256, colours, sizeof(colours));
    const float addABytes[] = {0.125f, 0.0625f, 0.03125f, 0.125f};
    byte addBStorage[32] = {};
    const float addBBytes[] = {0.0625f, 0.125f, 0.25f, 0.125f};
    memcpy(addBStorage + 16, addBBytes, sizeof(addBBytes));

    MTL::Buffer *positionBuffer =
        device->newBuffer(positionBytes, sizeof(positionBytes), MTL::ResourceStorageModeShared);
    MTL::Buffer *colourBuffer =
        device->newBuffer(colourBytes, sizeof(colourBytes), MTL::ResourceStorageModeShared);
    MTL::Buffer *addABuffer =
        device->newBuffer(addABytes, sizeof(addABytes), MTL::ResourceStorageModeShared);
    MTL::Buffer *addBBuffer =
        device->newBuffer(addBStorage, sizeof(addBStorage), MTL::ResourceStorageModeShared);

    if(!library || !vertexFunction || !fragmentFunction || !pipeline || !positionBuffer ||
       !colourBuffer || !addABuffer || !addBBuffer)
    {
      TEST_WARN("Failed to create T34 Metal resources: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
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
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0));
      MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
      encoder->setRenderPipelineState(pipeline);
      const MTL::Buffer *vertexBuffers[] = {positionBuffer, colourBuffer};
      const NS::UInteger vertexOffsets[] = {16, 0};
      encoder->setVertexBuffers(vertexBuffers, vertexOffsets, NS::Range::Make(0, 2));
      encoder->setVertexBufferOffset(256, 1);
      const float translation[] = {0.0f, 0.0f};
      encoder->setVertexBytes(translation, sizeof(translation), 2);
      const MTL::Buffer *fragmentBuffers[] = {addABuffer, addBBuffer};
      const NS::UInteger fragmentOffsets[] = {0, 16};
      encoder->setFragmentBuffers(fragmentBuffers, fragmentOffsets, NS::Range::Make(3, 2));
      const float base[] = {0.125f, 0.1875f, 0.15625f, 0.25f};
      encoder->setFragmentBytes(base, sizeof(base), 2);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      encoder->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();
      EndCaptureFrame();

      if(validateNative)
      {
        byte pixel[4] = {};
        drawable->texture()->getBytes(pixel, 4,
                                      MTL::Region::Make2D(screenWidth / 2, screenHeight / 2, 1, 1),
                                      0);
        const byte expected[] = {128, 128, 143, 255};
        for(size_t i = 0; i < 4; i++)
          validationFailed |= abs(int(pixel[i]) - int(expected[i])) > 2;
        if(validationFailed)
          TEST_WARN("T34 native output pixel differs: %u %u %u %u", pixel[0], pixel[1],
                    pixel[2], pixel[3]);
      }
      pool->drain();
    }

    addBBuffer->release();
    addABuffer->release();
    colourBuffer->release();
    positionBuffer->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
