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

RD_TEST(Metal_Compute_Indirect_Dispatch, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Dispatches a compute shader from a CPU-written offset threadgroup argument record.";

  static constexpr size_t ArgumentOffset = 16;
  static constexpr size_t ArgumentSize = sizeof(MTL::DispatchThreadgroupsIndirectArguments);

  struct Vertex
  {
    float position[2];
    float uv[2];
  };

  static void FillSource(uint8_t *pixels)
  {
    for(uint32_t y = 0; y < 8; y++)
    {
      for(uint32_t x = 0; x < 8; x++)
      {
        uint8_t *pixel = pixels + (y * 8 + x) * 4;
        pixel[0] = uint8_t(32 + 24 * x);
        pixel[1] = uint8_t(24 + 24 * y);
        pixel[2] = uint8_t(16 + 8 * (x + y));
        pixel[3] = 255;
      }
    }
  }

  static void FilterReference(const uint8_t *source, uint8_t *input, uint8_t *destination)
  {
    for(uint32_t pixel = 0; pixel < 64; pixel++)
    {
      destination[pixel * 4 + 0] = uint8_t(input[pixel * 4] + 3);
      destination[pixel * 4 + 1] = source[pixel * 4 + 0];
      destination[pixel * 4 + 2] = source[pixel * 4 + 1];
      destination[pixel * 4 + 3] = 255;
    }
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

kernel void filter_main(texture2d<float, access::read> source [[texture(0)]],
                        texture2d<float, access::write> destination [[texture(1)]],
                        device const uint *inputBuffer [[buffer(2)]],
                        device uint *outputBuffer [[buffer(4)]],
                        uint2 position [[thread_position_in_grid]])
{
  if(position.x >= destination.get_width() || position.y >= destination.get_height())
    return;
  const uint index = position.y * 8 + position.x;
  outputBuffer[index] = inputBuffer[index] + 3;
  const float4 input = source.read(position);
  destination.write(float4(float(outputBuffer[index]) / 255.0, input.r, input.g, 1.0), position);
}

vertex VSOut vs_main(VertexIn input [[stage_in]])
{
  VSOut output;
  output.position = float4(input.position, 0.0, 1.0);
  output.uv = input.uv;
  return output;
}

fragment float4 fs_main(VSOut input [[stage_in]],
                        texture2d<float> filteredTexture [[texture(0)]],
                        sampler nearestSampler [[sampler(0)]])
{
  return filteredTexture.sample(nearestSampler, input.uv);
}
)EOSHADER";

    const Vertex vertices[] = {
        {{-1.0f, -1.0f}, {0.0f, 1.0f}}, {{1.0f, -1.0f}, {1.0f, 1.0f}},
        {{-1.0f, 1.0f}, {0.0f, 0.0f}},  {{1.0f, 1.0f}, {1.0f, 0.0f}},
    };

    uint8_t sourcePixels[8 * 8 * 4] = {};
    uint8_t referencePixels[sizeof(sourcePixels)] = {};
    uint8_t zeroPixels[sizeof(sourcePixels)] = {};
    uint8_t inputBytes[32 + 256 + 16] = {};
    uint8_t outputBytes[64 + 256 + 16] = {};
    uint8_t expectedOutput[sizeof(outputBytes)] = {};
    uint8_t argumentBytes[ArgumentOffset + ArgumentSize + 16] = {};
    memset(inputBytes, 0x3c, sizeof(inputBytes));
    memset(outputBytes, 0xa5, sizeof(outputBytes));
    memset(expectedOutput, 0xa5, sizeof(expectedOutput));
    memset(argumentBytes, 0x6d, sizeof(argumentBytes));
    const MTL::DispatchThreadgroupsIndirectArguments arguments = {{2, 2, 1}};
    memcpy(argumentBytes + ArgumentOffset, &arguments, ArgumentSize);
    for(uint32_t i = 0; i < 64; i++)
    {
      const uint32_t value = 16 + i * 2;
      memcpy(inputBytes + 32 + i * 4, &value, 4);
      const uint32_t expected = value + 3;
      memcpy(expectedOutput + 64 + i * 4, &expected, 4);
    }
    FillSource(sourcePixels);
    FilterReference(sourcePixels, inputBytes + 32, referencePixels);

    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(
        NS::String::string(shaderSource, NS::UTF8StringEncoding), NULL, &error);
    if(library == NULL)
    {
      TEST_WARN("Failed to compile T32 Metal shader: %s",
                error ? error->localizedDescription()->utf8String() : "unknown error");
      return 4;
    }

    MTL::Function *computeFunction = library->newFunction(MTLSTR("filter_main"));
    MTL::Function *vertexFunction = library->newFunction(MTLSTR("vs_main"));
    MTL::Function *fragmentFunction = library->newFunction(MTLSTR("fs_main"));
    MTL::ComputePipelineState *computePipeline =
        computeFunction ? device->newComputePipelineState(computeFunction, &error) : NULL;

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

    MTL::RenderPipelineDescriptor *renderDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    renderDesc->setVertexFunction(vertexFunction);
    renderDesc->setFragmentFunction(fragmentFunction);
    renderDesc->setVertexDescriptor(vertexDesc);
    renderDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *renderPipeline =
        vertexFunction && fragmentFunction ? device->newRenderPipelineState(renderDesc, &error) : NULL;
    renderDesc->release();
    vertexDesc->release();

    MTL::Buffer *vertexBuffer =
        device->newBuffer(vertices, sizeof(vertices), MTL::ResourceStorageModeShared);
    MTL::Buffer *inputBuffer =
        device->newBuffer(inputBytes, sizeof(inputBytes), MTL::ResourceStorageModeShared);
    MTL::Buffer *outputBuffer =
        device->newBuffer(outputBytes, sizeof(outputBytes), MTL::ResourceStorageModeShared);
    MTL::Buffer *argumentBuffer =
        device->newBuffer(argumentBytes, sizeof(argumentBytes), MTL::ResourceStorageModeShared);
    MTL::TextureDescriptor *textureDesc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm, 8, 8, false);
    textureDesc->setStorageMode(MTL::StorageModeShared);
    textureDesc->setUsage(MTL::TextureUsage(MTL::TextureUsageShaderRead |
                                      MTL::TextureUsageShaderWrite));
    MTL::Texture *sourceTexture = device->newTexture(textureDesc);
    MTL::Texture *destinationTexture = device->newTexture(textureDesc);

    MTL::SamplerDescriptor *samplerDesc = MTL::SamplerDescriptor::alloc()->init();
    samplerDesc->setMinFilter(MTL::SamplerMinMagFilterNearest);
    samplerDesc->setMagFilter(MTL::SamplerMinMagFilterNearest);
    samplerDesc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    samplerDesc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    MTL::SamplerState *sampler = device->newSamplerState(samplerDesc);
    samplerDesc->release();

    if(computeFunction == NULL || vertexFunction == NULL || fragmentFunction == NULL ||
       computePipeline == NULL || renderPipeline == NULL || vertexBuffer == NULL ||
       sourceTexture == NULL || destinationTexture == NULL || sampler == NULL ||
       inputBuffer == NULL || outputBuffer == NULL || argumentBuffer == NULL)
    {
      TEST_WARN("Failed to create T32 Metal resources");
      return 4;
    }

    sourceTexture->replaceRegion(MTL::Region::Make2D(0, 0, 8, 8), 0, sourcePixels, 8 * 4);
    bool validationFailed = false;
    const bool validateNative = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();

    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      memcpy(outputBuffer->contents(), outputBytes, sizeof(outputBytes));
      BeginCaptureFrame();
      // Record the reset inside the frame so event seeks replay it before the dispatch.
      destinationTexture->replaceRegion(MTL::Region::Make2D(0, 0, 8, 8), 0, zeroPixels, 8 * 4);
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(drawable == NULL)
      {
        pool->drain();
        continue;
      }

      MTL::CommandBuffer *commandBuffer = queue->commandBuffer();
      MTL::BlitCommandEncoder *reset = commandBuffer->blitCommandEncoder();
      reset->fillBuffer(outputBuffer, NS::Range::Make(0, sizeof(outputBytes)), 0xa5);
      reset->endEncoding();
      MTL::ComputeCommandEncoder *compute = commandBuffer->computeCommandEncoder();
      compute->setComputePipelineState(computePipeline);
      compute->setTexture(sourceTexture, 0);
      compute->setTexture(destinationTexture, 1);
      compute->setBuffer(inputBuffer, 32, 2);
      compute->setBuffer(outputBuffer, 64, 4);
      compute->dispatchThreadgroups(argumentBuffer, ArgumentOffset, MTL::Size::Make(4, 4, 1));
      compute->endEncoding();

      MTL::RenderPassDescriptor *pass =
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0.025, 0.035, 0.055, 1.0));
      MTL::RenderCommandEncoder *render = commandBuffer->renderCommandEncoder(pass);
      render->setRenderPipelineState(renderPipeline);
      render->setVertexBuffer(vertexBuffer, 0, 0);
      render->setFragmentTexture(destinationTexture, 0);
      render->setFragmentSamplerState(sampler, 0);
      render->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
      render->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();
      EndCaptureFrame();

      if(validateNative)
      {
        uint8_t actualPixels[sizeof(referencePixels)] = {};
        destinationTexture->getBytes(actualPixels, 8 * 4, MTL::Region::Make2D(0, 0, 8, 8), 0);
        if(memcmp(actualPixels, referencePixels, sizeof(referencePixels)) != 0 ||
           memcmp(outputBuffer->contents(), expectedOutput, sizeof(expectedOutput)) != 0 ||
           memcmp(argumentBuffer->contents(), argumentBytes, sizeof(argumentBytes)) != 0)
        {
          TEST_WARN("T32 native indirect dispatch did not match the CPU reference");
          validationFailed = true;
        }
      }
      pool->drain();
    }

    argumentBuffer->release();
    outputBuffer->release();
    inputBuffer->release();
    sampler->release();
    destinationTexture->release();
    sourceTexture->release();
    vertexBuffer->release();
    renderPipeline->release();
    computePipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    computeFunction->release();
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
