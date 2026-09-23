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

RD_TEST(Metal_Vertex_Texture, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Samples an RGBA8 texture and point sampler in the vertex shader for four solid quads.";

  struct Vertex
  {
    float position[2];
    float uv[2];
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
  float2 uv [[attribute(1)]];
};
struct VSOut
{
  float4 position [[position]];
  float4 colour;
};
vertex VSOut vs_main(VertexIn input [[stage_in]],
                     texture2d<float> vertexTexture [[texture(0)]],
                     sampler vertexSampler [[sampler(0)]])
{
  VSOut output;
  output.position = float4(input.position, 0.0, 1.0);
  output.colour = vertexTexture.sample(vertexSampler, input.uv);
  return output;
}
fragment float4 fs_main(VSOut input [[stage_in]]) { return input.colour; }
)EOSHADER";

    // All six vertices in each quadrant address the same texel, so interpolation cannot
    // change the expected colour. The clear border remains visible around the quads.
    const Vertex vertices[] = {
        {{-0.8f, 0.0f}, {0.25f, 0.25f}}, {{0.0f, 0.0f}, {0.25f, 0.25f}},
        {{-0.8f, 0.8f}, {0.25f, 0.25f}}, {{0.0f, 0.0f}, {0.25f, 0.25f}},
        {{0.0f, 0.8f}, {0.25f, 0.25f}}, {{-0.8f, 0.8f}, {0.25f, 0.25f}},
        {{0.0f, 0.0f}, {0.75f, 0.25f}}, {{0.8f, 0.0f}, {0.75f, 0.25f}},
        {{0.0f, 0.8f}, {0.75f, 0.25f}}, {{0.8f, 0.0f}, {0.75f, 0.25f}},
        {{0.8f, 0.8f}, {0.75f, 0.25f}}, {{0.0f, 0.8f}, {0.75f, 0.25f}},
        {{-0.8f, -0.8f}, {0.25f, 0.75f}}, {{0.0f, -0.8f}, {0.25f, 0.75f}},
        {{-0.8f, 0.0f}, {0.25f, 0.75f}}, {{0.0f, -0.8f}, {0.25f, 0.75f}},
        {{0.0f, 0.0f}, {0.25f, 0.75f}}, {{-0.8f, 0.0f}, {0.25f, 0.75f}},
        {{0.0f, -0.8f}, {0.75f, 0.75f}}, {{0.8f, -0.8f}, {0.75f, 0.75f}},
        {{0.0f, 0.0f}, {0.75f, 0.75f}}, {{0.8f, -0.8f}, {0.75f, 0.75f}},
        {{0.8f, 0.0f}, {0.75f, 0.75f}}, {{0.0f, 0.0f}, {0.75f, 0.75f}},
    };
    const uint8_t texels[] = {
        255, 32, 16, 255, 16, 224, 48, 255,
        24, 64, 255, 255, 240, 208, 32, 255,
    };

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(library == NULL)
    {
      TEST_WARN("Failed to compile T16 shader: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }
    MTL::Function *vertexFunction = library->newFunction(MTLSTR("vs_main"));
    MTL::Function *fragmentFunction = library->newFunction(MTLSTR("fs_main"));
    MTL::VertexDescriptor *vertexDesc = MTL::VertexDescriptor::alloc()->init();
    vertexDesc->attributes()->object(0)->setFormat(MTL::VertexFormatFloat2);
    vertexDesc->attributes()->object(0)->setOffset(offsetof(Vertex, position));
    vertexDesc->attributes()->object(0)->setBufferIndex(0);
    vertexDesc->attributes()->object(1)->setFormat(MTL::VertexFormatFloat2);
    vertexDesc->attributes()->object(1)->setOffset(offsetof(Vertex, uv));
    vertexDesc->attributes()->object(1)->setBufferIndex(0);
    vertexDesc->layouts()->object(0)->setStride(sizeof(Vertex));
    vertexDesc->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);
    vertexDesc->layouts()->object(0)->setStepRate(1);
    MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vertexFunction);
    pipelineDesc->setFragmentFunction(fragmentFunction);
    pipelineDesc->setVertexDescriptor(vertexDesc);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(pipelineDesc, &error);
    pipelineDesc->release();
    vertexDesc->release();

    MTL::Buffer *vertexBuffer = device->newBuffer(vertices, sizeof(vertices),
                                                   MTL::ResourceStorageModeShared);
    MTL::TextureDescriptor *textureDesc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm, 2, 2, false);
    textureDesc->setStorageMode(MTL::StorageModeShared);
    textureDesc->setUsage(MTL::TextureUsageShaderRead);
    MTL::Texture *texture = device->newTexture(textureDesc);
    if(texture)
      texture->replaceRegion(MTL::Region::Make2D(0, 0, 2, 2), 0, texels, 8);
    MTL::SamplerDescriptor *samplerDesc = MTL::SamplerDescriptor::alloc()->init();
    samplerDesc->setMinFilter(MTL::SamplerMinMagFilterNearest);
    samplerDesc->setMagFilter(MTL::SamplerMinMagFilterNearest);
    samplerDesc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    samplerDesc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    MTL::SamplerState *sampler = device->newSamplerState(samplerDesc);
    samplerDesc->release();
    if(pipeline == NULL || vertexBuffer == NULL || texture == NULL || sampler == NULL)
    {
      TEST_WARN("Failed to create T16 resources");
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
      render->setVertexTexture(texture, 0);
      render->setVertexSamplerState(sampler, 0);
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
        const size_t width = drawable->texture()->width();
        const size_t height = drawable->texture()->height();
        const byte expected[5][4] = {{16, 32, 255, 255}, {48, 224, 16, 255},
                                     {255, 64, 24, 255}, {32, 208, 240, 255},
                                     {14, 9, 6, 255}};
        const size_t x[5] = {width / 4, width * 3 / 4, width / 4, width * 3 / 4, width / 2};
        const size_t y[5] = {height / 4, height / 4, height * 3 / 4,
                             height * 3 / 4, height / 16};
        for(size_t i = 0; i < 5; i++)
          if(memcmp(pixels + y[i] * rowPitch + x[i] * 4, expected[i], 4) != 0)
          {
            const byte *pixel = pixels + y[i] * rowPitch + x[i] * 4;
            TEST_WARN("T16 native pixel %zu: %02x%02x%02x%02x", i, pixel[0], pixel[1],
                      pixel[2], pixel[3]);
            validationFailed = true;
          }
        readback->release();
      }
      pool->drain();
    }

    sampler->release();
    texture->release();
    vertexBuffer->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
