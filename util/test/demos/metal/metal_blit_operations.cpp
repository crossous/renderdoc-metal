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

RD_TEST(Metal_Blit_Operations, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Copies and fills buffers, copies an RGBA8 texture, and generates deterministic mipmaps.";

  struct Vertex
  {
    float position[2];
    float uv[2];
  };

  static void FillTexture(uint8_t *data, uint32_t width, uint32_t height)
  {
    static const uint8_t colours[4][4] = {
        {255, 32, 16, 255},
        {16, 224, 48, 255},
        {24, 64, 255, 255},
        {240, 208, 32, 255},
    };

    for(uint32_t y = 0; y < height; y++)
    {
      for(uint32_t x = 0; x < width; x++)
      {
        const uint32_t quadrant = (x >= width / 2 ? 1U : 0U) + (y >= height / 2 ? 2U : 0U);
        memcpy(data + (y * width + x) * 4, colours[quadrant], 4);
      }
    }
  }

  static bool NearByte(uint8_t actual, uint8_t expected)
  {
    return actual >= expected - (expected > 0 ? 1 : 0) && actual <= expected + 1;
  }

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

vertex VSOut vs_main(VertexIn input [[stage_in]])
{
  VSOut output;
  output.position = float4(input.position, 0.0, 1.0);
  output.uv = input.uv;
  return output;
}

fragment float4 fs_main(VSOut input [[stage_in]],
                        constant uchar4 *blitBytes [[buffer(0)]],
                        texture2d<float> copiedTexture [[texture(0)]],
                        texture2d<float> mipTexture [[texture(1)]],
                        sampler nearestSampler [[sampler(0)]])
{
  const uint band = min(uint(input.uv.x * 4.0), 3u);
  if(band == 0)
    return float4(float3(blitBytes[0].xyz) / 255.0, 1.0);
  if(band == 1)
    return float4(float3(blitBytes[4].xyz) / 255.0, 1.0);
  if(band == 2)
    return copiedTexture.sample(nearestSampler, float2(0.25));
  return mipTexture.sample(nearestSampler, float2(0.5), level(3.0));
}
)EOSHADER";

    const Vertex vertices[] = {
        {{-1.0f, -1.0f}, {0.0f, 1.0f}}, {{1.0f, -1.0f}, {1.0f, 1.0f}},
        {{-1.0f, 1.0f}, {0.0f, 0.0f}},  {{1.0f, 1.0f}, {1.0f, 0.0f}},
    };

    uint8_t sourceBytes[64] = {};
    const uint8_t copiedBufferColour[4] = {255, 128, 32, 255};
    for(uint32_t i = 0; i < 4; i++)
      memcpy(sourceBytes + 8 + i * 4, copiedBufferColour, 4);

    uint8_t sourceTexels[8 * 8 * 4] = {};
    FillTexture(sourceTexels, 8, 8);
    uint8_t zeroTexels[8 * 8 * 4] = {};

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(library == NULL)
    {
      TEST_WARN("Failed to compile T10 Metal shader: %s",
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

    MTL::Buffer *vertexBuffer =
        device->newBuffer(vertices, sizeof(vertices), MTL::ResourceStorageModeShared);
    MTL::Buffer *sourceBuffer =
        device->newBuffer(sourceBytes, sizeof(sourceBytes), MTL::ResourceStorageModeShared);
    MTL::Buffer *destinationBuffer =
        device->newBuffer(sizeof(sourceBytes), MTL::ResourceStorageModeShared);

    MTL::TextureDescriptor *textureDesc = MTL::TextureDescriptor::alloc()->init();
    textureDesc->setTextureType(MTL::TextureType2D);
    textureDesc->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
    textureDesc->setWidth(8);
    textureDesc->setHeight(8);
    textureDesc->setDepth(1);
    textureDesc->setMipmapLevelCount(1);
    textureDesc->setArrayLength(1);
    textureDesc->setSampleCount(1);
    textureDesc->setStorageMode(MTL::StorageModeShared);
    textureDesc->setUsage(MTL::TextureUsageShaderRead);
    MTL::Texture *sourceTexture = device->newTexture(textureDesc);
    MTL::Texture *destinationTexture = device->newTexture(textureDesc);
    textureDesc->setMipmapLevelCount(4);
    MTL::Texture *mipTexture = device->newTexture(textureDesc);
    textureDesc->release();

    MTL::SamplerDescriptor *samplerDesc = MTL::SamplerDescriptor::alloc()->init();
    samplerDesc->setMinFilter(MTL::SamplerMinMagFilterNearest);
    samplerDesc->setMagFilter(MTL::SamplerMinMagFilterNearest);
    samplerDesc->setMipFilter(MTL::SamplerMipFilterNearest);
    samplerDesc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    samplerDesc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    MTL::SamplerState *sampler = device->newSamplerState(samplerDesc);
    samplerDesc->release();

    if(vertexFunction == NULL || fragmentFunction == NULL || pipeline == NULL ||
       vertexBuffer == NULL || sourceBuffer == NULL || destinationBuffer == NULL ||
       sourceTexture == NULL || destinationTexture == NULL || mipTexture == NULL || sampler == NULL)
    {
      TEST_WARN("Failed to create T10 Metal resources");
      return 4;
    }

    sourceTexture->replaceRegion(MTL::Region::Make2D(0, 0, 8, 8), 0, sourceTexels, 8 * 4);
    bool validationFailed = false;
    const bool validateNative = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();

    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();

      // Reset all blit destinations before capture begins so their captured initial contents are
      // deterministic even when the fixture has already rendered earlier native frames.
      memset(destinationBuffer->contents(), 0, destinationBuffer->length());
      destinationTexture->replaceRegion(MTL::Region::Make2D(0, 0, 8, 8), 0, zeroTexels, 8 * 4);
      mipTexture->replaceRegion(MTL::Region::Make2D(0, 0, 8, 8), 0, sourceTexels, 8 * 4);
      for(uint32_t mip = 1; mip < 4; mip++)
      {
        const uint32_t size = 8U >> mip;
        mipTexture->replaceRegion(MTL::Region::Make2D(0, 0, size, size), mip, zeroTexels, size * 4);
      }

      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(drawable == NULL)
      {
        pool->drain();
        continue;
      }

      MTL::CommandBuffer *commandBuffer = queue->commandBuffer();
      MTL::BlitCommandEncoder *blit = commandBuffer->blitCommandEncoder();
      blit->copyFromBuffer(sourceBuffer, 8, destinationBuffer, 0, 32);
      blit->fillBuffer(destinationBuffer, NS::Range::Make(16, 16), 0x60);
      blit->copyFromTexture(sourceTexture, 0, 0, MTL::Origin::Make(0, 0, 0),
                            MTL::Size::Make(8, 8, 1), destinationTexture, 0, 0,
                            MTL::Origin::Make(0, 0, 0));
      blit->generateMipmaps(mipTexture);
      blit->endEncoding();

      MTL::RenderPassDescriptor *pass =
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0.025, 0.035, 0.055, 1.0));
      MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
      encoder->setRenderPipelineState(pipeline);
      encoder->setVertexBuffer(vertexBuffer, 0, 0);
      encoder->setFragmentBuffer(destinationBuffer, 0, 0);
      encoder->setFragmentTexture(destinationTexture, 0);
      encoder->setFragmentTexture(mipTexture, 1);
      encoder->setFragmentSamplerState(sampler, 0);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
      encoder->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();
      EndCaptureFrame();

      if(validateNative)
      {
        const uint8_t *destinationBytes = (const uint8_t *)destinationBuffer->contents();
        if(memcmp(destinationBytes, sourceBytes + 8, 16) != 0)
        {
          TEST_WARN("T10 buffer copy result did not match the fixed source range");
          validationFailed = true;
        }
        for(uint32_t i = 16; i < 32; i++)
        {
          if(destinationBytes[i] != 0x60)
          {
            TEST_WARN("T10 buffer fill result did not match 0x60");
            validationFailed = true;
            break;
          }
        }

        uint8_t copiedTexels[sizeof(sourceTexels)] = {};
        destinationTexture->getBytes(copiedTexels, 8 * 4, MTL::Region::Make2D(0, 0, 8, 8), 0);
        if(memcmp(copiedTexels, sourceTexels, sizeof(sourceTexels)) != 0)
        {
          TEST_WARN("T10 texture copy result did not match the fixed RGBA8 source");
          validationFailed = true;
        }

        uint8_t generatedPixel[4] = {};
        mipTexture->getBytes(generatedPixel, 4, MTL::Region::Make2D(0, 0, 1, 1), 3);
        if(!NearByte(generatedPixel[0], 134) || !NearByte(generatedPixel[1], 132) ||
           !NearByte(generatedPixel[2], 88) || generatedPixel[3] != 255)
        {
          TEST_WARN("T10 generated mip 3 was %u,%u,%u,%u instead of the expected average",
                    generatedPixel[0], generatedPixel[1], generatedPixel[2], generatedPixel[3]);
          validationFailed = true;
        }
      }

      pool->drain();
    }

    sampler->release();
    mipTexture->release();
    destinationTexture->release();
    sourceTexture->release();
    destinationBuffer->release();
    sourceBuffer->release();
    vertexBuffer->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
