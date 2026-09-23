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

RD_TEST(Metal_Argument_Buffer, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Samples an RGBA8 texture and sampler through one directly encoded argument buffer.";

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
  float2 uv;
};

struct FragmentArguments
{
  texture2d<float> colourTexture [[id(0)]];
  sampler colourSampler [[id(1)]];
};

vertex VSOut vs_main(VertexIn input [[stage_in]])
{
  VSOut output;
  output.position = float4(input.position, 0.0, 1.0);
  output.uv = input.uv;
  return output;
}

fragment float4 fs_main(VSOut input [[stage_in]],
                        constant FragmentArguments &arguments [[buffer(0)]])
{
  return arguments.colourTexture.sample(arguments.colourSampler, input.uv);
}
)EOSHADER";

    const Vertex vertices[] = {
        {{-0.80f, -0.80f}, {0.0f, 1.0f}}, {{0.80f, -0.80f}, {1.0f, 1.0f}},
        {{-0.80f, 0.80f}, {0.0f, 0.0f}},  {{0.80f, 0.80f}, {1.0f, 0.0f}},
    };

    const uint8_t textureData[] = {
        248, 40, 24, 255, 248, 40, 24, 255, 24, 216, 56, 255, 24, 216, 56, 255,
        248, 40, 24, 255, 248, 40, 24, 255, 24, 216, 56, 255, 24, 216, 56, 255,
        32, 72, 248, 255, 32, 72, 248, 255, 232, 200, 40, 255, 232, 200, 40, 255,
        32, 72, 248, 255, 32, 72, 248, 255, 232, 200, 40, 255, 232, 200, 40, 255,
    };

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(library == NULL)
    {
      TEST_WARN("Failed to compile T12 Metal shader: %s",
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
    MTL::RenderPipelineState *pipeline =
        vertexFunction && fragmentFunction ? device->newRenderPipelineState(pipelineDesc, &error)
                                           : NULL;
    pipelineDesc->release();
    vertexDesc->release();

    MTL::Buffer *vertexBuffer =
        device->newBuffer(vertices, sizeof(vertices), MTL::ResourceStorageModeShared);
    MTL::TextureDescriptor *textureDesc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm, 4, 4, false);
    textureDesc->setStorageMode(MTL::StorageModeShared);
    textureDesc->setUsage(MTL::TextureUsageShaderRead);
    MTL::Texture *texture = device->newTexture(textureDesc);
    if(texture)
      texture->replaceRegion(MTL::Region::Make2D(0, 0, 4, 4), 0, textureData, 4 * 4);

    MTL::SamplerDescriptor *samplerDesc = MTL::SamplerDescriptor::alloc()->init();
    samplerDesc->setMinFilter(MTL::SamplerMinMagFilterNearest);
    samplerDesc->setMagFilter(MTL::SamplerMinMagFilterNearest);
    samplerDesc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    samplerDesc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    samplerDesc->setSupportArgumentBuffers(true);
    MTL::SamplerState *sampler = device->newSamplerState(samplerDesc);
    samplerDesc->release();

    MTL::ArgumentEncoder *argumentEncoder =
        fragmentFunction ? fragmentFunction->newArgumentEncoder(0) : NULL;
    MTL::Buffer *argumentBuffer = argumentEncoder
                                      ? device->newBuffer(argumentEncoder->encodedLength(),
                                                          MTL::ResourceStorageModeShared)
                                      : NULL;
    if(argumentEncoder && argumentBuffer)
    {
      argumentEncoder->setArgumentBuffer(argumentBuffer, 0);
      argumentEncoder->setTexture(texture, 0);
      argumentEncoder->setSamplerState(sampler, 1);
    }

    if(pipeline == NULL || vertexBuffer == NULL || texture == NULL || sampler == NULL ||
       argumentEncoder == NULL || argumentBuffer == NULL)
    {
      TEST_WARN("Failed to create T12 Metal resources");
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
      render->setFragmentBuffer(argumentBuffer, 0, 0);
      render->useResource(texture, MTL::ResourceUsageRead);
      render->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
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
        const size_t width = drawable->texture()->width();
        const size_t height = drawable->texture()->height();
        const byte *topLeft = pixels + (height / 4) * readbackRowPitch + (width / 4) * 4;
        const byte *topRight = pixels + (height / 4) * readbackRowPitch + (width * 3 / 4) * 4;
        const byte *bottomLeft = pixels + (height * 3 / 4) * readbackRowPitch + (width / 4) * 4;
        const byte *bottomRight =
            pixels + (height * 3 / 4) * readbackRowPitch + (width * 3 / 4) * 4;
        const byte expected[][4] = {{24, 40, 248, 255}, {56, 216, 24, 255},
                                    {248, 72, 32, 255}, {40, 200, 232, 255}};
        const byte *actual[] = {topLeft, topRight, bottomLeft, bottomRight};
        for(size_t i = 0; i < ARRAY_COUNT(actual); i++)
          validationFailed |= memcmp(actual[i], expected[i], 4) != 0;
        if(validationFailed)
          TEST_WARN("T12 native argument-buffer draw did not match fixed BGRA pixels");
        readback->release();
      }
      pool->drain();
    }

    argumentBuffer->release();
    argumentEncoder->release();
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
