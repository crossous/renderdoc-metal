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

RD_TEST(Metal_Render_Dynamic_State, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "Exercises viewport/scissor arrays, depth clip/bias, triangle fill, and blend constants.";

  int main()
  {
    if(!Init())
      return 3;

    const char *shaderSource = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;

vertex float4 vs_main(uint vertexID [[vertex_id]])
{
  const float2 positions[] = {float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};
  return float4(positions[vertexID], 2.0, 1.0);
}

fragment float4 fs_main()
{
  return float4(1.0);
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
    pipelineDesc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float_Stencil8);
    pipelineDesc->setStencilAttachmentPixelFormat(MTL::PixelFormatDepth32Float_Stencil8);
    MTL::RenderPipelineColorAttachmentDescriptor *attachment =
        pipelineDesc->colorAttachments()->object(0);
    attachment->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    attachment->setBlendingEnabled(true);
    attachment->setSourceRGBBlendFactor(MTL::BlendFactorBlendColor);
    attachment->setDestinationRGBBlendFactor(MTL::BlendFactorZero);
    attachment->setSourceAlphaBlendFactor(MTL::BlendFactorBlendAlpha);
    attachment->setDestinationAlphaBlendFactor(MTL::BlendFactorZero);
    MTL::RenderPipelineState *pipeline =
        vertexFunction && fragmentFunction ? device->newRenderPipelineState(pipelineDesc, &error)
                                           : NULL;
    pipelineDesc->release();
    MTL::TextureDescriptor *depthDesc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatDepth32Float_Stencil8, screenWidth, screenHeight, false);
    depthDesc->setStorageMode(MTL::StorageModePrivate);
    depthDesc->setUsage(MTL::TextureUsageRenderTarget);
    MTL::Texture *depthStencil = device->newTexture(depthDesc);
    if(!library || !vertexFunction || !fragmentFunction || !pipeline || !depthStencil)
    {
      TEST_WARN("Failed to create T36 Metal resources: %s",
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
      commandBuffer->pushDebugGroup(MTLSTR("T36 command debug group"));
      const uint64_t initialVisibility = 0;
      MTL::Buffer *visibility =
          device->newBuffer(&initialVisibility, sizeof(initialVisibility),
                             MTL::ResourceStorageModeShared);
      if(!visibility)
      {
        pool->drain();
        return 4;
      }
      MTL::RenderPassDescriptor *pass =
          MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0));
      pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionUnknown);
      pass->depthAttachment()->setTexture(depthStencil);
      pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
      pass->depthAttachment()->setStoreAction(MTL::StoreActionUnknown);
      pass->depthAttachment()->setClearDepth(0.75);
      pass->stencilAttachment()->setTexture(depthStencil);
      pass->stencilAttachment()->setLoadAction(MTL::LoadActionClear);
      pass->stencilAttachment()->setStoreAction(MTL::StoreActionUnknown);
      pass->stencilAttachment()->setClearStencil(23);
      pass->setVisibilityResultBuffer(visibility);
      MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
      encoder->pushDebugGroup(MTLSTR("T36 render debug group"));
      encoder->insertDebugSignpost(MTLSTR("T36 dynamic state"));
      encoder->setRenderPipelineState(pipeline);
      const MTL::Viewport viewport = {0.0, 0.0, (double)screenWidth, (double)screenHeight,
                                      0.0, 1.0};
      encoder->setViewports(&viewport, 1);
      const MTL::ScissorRect scissor = {
          64, 48, NS::UInteger(screenWidth - 128), NS::UInteger(screenHeight - 96)};
      encoder->setScissorRects(&scissor, 1);
      encoder->setDepthClipMode(MTL::DepthClipModeClamp);
      encoder->setDepthBias(1.25f, 2.5f, 3.75f);
      encoder->setTriangleFillMode(MTL::TriangleFillModeFill);
      encoder->setBlendColor(0.2f, 0.4f, 0.6f, 0.8f);
      encoder->setVisibilityResultMode(MTL::VisibilityResultModeCounting, 0);
      encoder->setColorStoreAction(MTL::StoreActionStore, 0);
      encoder->setDepthStoreAction(MTL::StoreActionStore);
      encoder->setStencilStoreAction(MTL::StoreActionStore);
      encoder->setColorStoreActionOptions(MTL::StoreActionOptionNone, 0);
      encoder->setDepthStoreActionOptions(MTL::StoreActionOptionNone);
      encoder->setStencilStoreActionOptions(MTL::StoreActionOptionNone);
      if(!GetEnvVar("RENDERDOC_METAL_T36_MODERN_BARRIER").empty())
        encoder->memoryBarrier(
            MTL::BarrierScope(MTL::BarrierScopeTextures | MTL::BarrierScopeRenderTargets),
            MTL::RenderStages(MTL::RenderStageVertex | MTL::RenderStageFragment),
            MTL::RenderStages(MTL::RenderStageVertex | MTL::RenderStageFragment));
      else
        encoder->textureBarrier();
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
      encoder->popDebugGroup();
      encoder->endEncoding();
      commandBuffer->popDebugGroup();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();
      commandBuffer->waitUntilCompleted();
      EndCaptureFrame();

      if(validateNative)
      {
        byte center[4] = {}, corner[4] = {};
        drawable->texture()->getBytes(center, 4,
                                      MTL::Region::Make2D(screenWidth / 2, screenHeight / 2, 1, 1),
                                      0);
        drawable->texture()->getBytes(corner, 4, MTL::Region::Make2D(8, 8, 1, 1), 0);
        const byte expectedCenter[] = {153, 102, 51, 204};
        const byte expectedCorner[] = {0, 0, 0, 255};
        for(size_t i = 0; i < 4; i++)
          validationFailed |= abs(int(center[i]) - int(expectedCenter[i])) > 2 ||
                              abs(int(corner[i]) - int(expectedCorner[i])) > 2;
        if(validationFailed)
          TEST_WARN("T36 native pixels differ: center %u %u %u %u, corner %u %u %u %u",
                    center[0], center[1], center[2], center[3], corner[0], corner[1], corner[2],
                    corner[3]);
        if(*(const uint64_t *)visibility->contents() == 0)
        {
          TEST_WARN("T36 visibility counter was not written");
          validationFailed = true;
        }
      }
      visibility->release();
      pool->drain();
    }

    depthStencil->release();
    pipeline->release();
    fragmentFunction->release();
    vertexFunction->release();
    library->release();
    return validationFailed ? 5 : 0;
  }
};

REGISTER_TEST();
