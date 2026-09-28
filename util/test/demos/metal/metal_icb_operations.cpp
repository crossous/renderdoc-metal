// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_test.h"

RD_TEST(Metal_ICB_Operations, MetalGraphicsTest)
{
  static constexpr const char *Description =
      "GPU ICB copy/reset/optimize, empty commands, single-command reset and replay restoration.";
  struct Packet
  {
    uint32_t prefix[4];
    float colour[4];
    float transform[4];
    uint32_t suffix[8];
  };

  int main()
  {
    if(!Init()) return 3;
    const char *source = R"(
#include <metal_stdlib>
using namespace metal;
struct Params { float4 colour; float4 transform; };
struct Tint { float4 multiplier; };
struct Out { float4 position [[position]]; float4 colour; };
vertex Out vs_icb_ops(uint id [[vertex_id]], constant Params &p [[buffer(0)]],
                     constant Tint &tint [[buffer(1)]])
{
  const float2 positions[] = {float2(-1,-1),float2(1,-1),float2(-1,1),
                              float2(-1,1),float2(1,-1),float2(1,1)};
  Out o; o.position = float4(positions[id] * p.transform.xy + p.transform.zw,0,1);
  o.colour = p.colour * tint.multiplier; return o;
}
fragment float4 fs_icb_ops(Out o [[stage_in]]) { return o.colour; }
kernel void write_icb_range(device uint2 *range [[buffer(0)]]) { range[0] = uint2(0, 6); }
)";
    NS::Error *error = NULL;
    MTL::Library *library = device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
    if(!library) return 4;
    MTL::Function *vs = library->newFunction(MTLSTR("vs_icb_ops"));
    MTL::Function *fs = library->newFunction(MTLSTR("fs_icb_ops"));
    MTL::Function *rangeWriter = library->newFunction(MTLSTR("write_icb_range"));
    const bool gpuIndirectRange = !GetEnvVar("RENDERDOC_METAL_T70_GPU_INDIRECT_RANGE").empty();
    MTL::ComputePipelineState *rangePipeline =
        gpuIndirectRange ? device->newComputePipelineState(rangeWriter, &error) : NULL;
    MTL::RenderPipelineDescriptor *pd = MTL::RenderPipelineDescriptor::alloc()->init();
    pd->setVertexFunction(vs); pd->setFragmentFunction(fs);
    pd->setSupportIndirectCommandBuffers(true);
    pd->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState *pipelines[2] = {device->newRenderPipelineState(pd, &error), NULL};
    pd->setLabel(MTLSTR("T53 indexed pipeline"));
    pipelines[1] = device->newRenderPipelineState(pd, &error);
    pd->release(); vs->release(); fs->release(); rangeWriter->release(); library->release();
    if(!pipelines[0] || !pipelines[1] || (gpuIndirectRange && !rangePipeline)) return 5;
    const float colours[5][4] = {{1,0,1,1}, {1,0,0,1}, {0,1,0,1}, {0,0,1,1}, {1,1,0,1}};
    MTL::Buffer *params[5] = {};
    for(unsigned i = 0; i < 5; ++i)
    {
      Packet packet = {};
      for(unsigned j = 0; j < 4; ++j) packet.prefix[j] = 0x53000000 + i * 16 + j;
      for(unsigned j = 0; j < 8; ++j) packet.suffix[j] = 0x53ff0000 + i * 16 + j;
      memcpy(packet.colour, colours[i], sizeof(packet.colour));
      packet.transform[0] = i == 0 || i == 4 ? 1.0f : 1.0f / 3;
      packet.transform[1] = 1;
      packet.transform[2] = i >= 1 && i <= 3 ? (float(i) - 2) * 2 / 3 : 0;
      params[i] = device->newBuffer(&packet, sizeof(packet), MTL::ResourceStorageModeShared);
    }
    const float tint[] = {1,1,1,1};
    MTL::Buffer *tintBuffer = device->newBuffer(tint, sizeof(tint), MTL::ResourceStorageModeShared);
    const uint16_t indices[] = {99,99,0,1,2,3,4,5,99};
    MTL::Buffer *indexBuffer = device->newBuffer(indices, sizeof(indices), MTL::ResourceStorageModeShared);
    const uint32_t rangeSentinel[2] = {0xffffffff, 0xffffffff};
    MTL::Buffer *rangeBuffer = gpuIndirectRange ?
        device->newBuffer(rangeSentinel, sizeof(rangeSentinel), MTL::ResourceStorageModeShared) : NULL;
    MTL::IndirectCommandBufferDescriptor *desc = MTL::IndirectCommandBufferDescriptor::alloc()->init();
    desc->setCommandTypes((MTL::IndirectCommandType)(MTL::IndirectCommandTypeDraw | MTL::IndirectCommandTypeDrawIndexed));
    desc->setInheritPipelineState(false); desc->setInheritBuffers(false);
    desc->setMaxVertexBufferBindCount(2); desc->setMaxFragmentBufferBindCount(0);
    MTL::IndirectCommandBuffer *src = device->newIndirectCommandBuffer(desc, 5, MTL::ResourceStorageModeShared);
    MTL::IndirectCommandBuffer *dst = device->newIndirectCommandBuffer(desc, 6, MTL::ResourceStorageModeShared);
    desc->release();
    if(!src || !dst) return 6;
    auto encode = [&](MTL::IndirectRenderCommand *command, unsigned i) {
      command->setRenderPipelineState(pipelines[i == 2 ? 1 : 0]);
      command->setVertexBuffer(params[i], 16, 0);
      command->setVertexBuffer(tintBuffer, 0, 1);
      if(i == 2)
        command->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, 6, MTL::IndexTypeUInt16,
                                       indexBuffer, 4, 1, 0, 0);
      else
        command->drawPrimitives(MTL::PrimitiveTypeTriangle, 0, 6, 1, 0);
    };
    for(unsigned i = 0; i < 5; ++i) encode(src->indirectRenderCommand(i), i);
    src->indirectRenderCommand(4)->reset(); // discarded yellow command must never draw
    MTL::IndirectRenderCommand *replacement = src->indirectRenderCommand(3);
    replacement->reset(); encode(replacement, 3); // reuse the same command after reset
    const bool native = GetEnvVar("RENDERDOC_METAL_CAPTURE_PATH").empty();
    const bool preframeGPU = !GetEnvVar("RENDERDOC_METAL_T53_PREFRAME_GPU").empty();
    unsigned frames = 0;
    while(Running())
    {
      NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
      // Restore the application's starting contents before capture. This is CPU encoding, not a
      // hidden GPU initialization dependency from the previous frame.
      dst->reset(NS::Range::Make(0, 6)); encode(dst->indirectRenderCommand(0), 0);
      if(preframeGPU)
      {
        MTL::CommandBuffer *before = queue->commandBuffer();
        MTL::BlitCommandEncoder *blit = before->blitCommandEncoder();
        blit->copyIndirectCommandBuffer(src, NS::Range::Make(1, 1), dst, 0);
        blit->endEncoding(); before->commit(); before->waitUntilCompleted();
        if(before->status() != MTL::CommandBufferStatusCompleted) return 9;
      }
      BeginCaptureFrame();
      CA::MetalDrawable *drawable = AcquireDrawable();
      if(!drawable) { pool->drain(); continue; }
      MTL::CommandBuffer *cb = queue->commandBuffer();
      MTL::Buffer *readback[4] = {};
      size_t rowPitch = AlignUp<size_t>(drawable->texture()->width() * 4, 256);
      auto render = [&](unsigned stage, NS::Range range) {
        MTL::RenderPassDescriptor *pass = MakeBackbufferRenderPass(drawable, MTL::ClearColor::Make(0,0,0,1));
        MTL::RenderCommandEncoder *encoder = cb->renderCommandEncoder(pass);
        for(auto *buffer : params) encoder->useResource(buffer, MTL::ResourceUsageRead);
        encoder->useResource(tintBuffer, MTL::ResourceUsageRead);
        encoder->useResource(indexBuffer, MTL::ResourceUsageRead);
        if(gpuIndirectRange && stage == 1)
          encoder->executeCommandsInBuffer(dst, rangeBuffer, 0);
        else
          encoder->executeCommandsInBuffer(dst, range);
        encoder->endEncoding();
        if(native)
        {
          readback[stage] = device->newBuffer(rowPitch * drawable->texture()->height(), MTL::ResourceStorageModeShared);
          MTL::BlitCommandEncoder *blit = cb->blitCommandEncoder();
          blit->copyFromTexture(drawable->texture(), 0, 0, MTL::Origin::Make(0,0,0),
              MTL::Size::Make(drawable->texture()->width(), drawable->texture()->height(), 1),
              readback[stage], 0, rowPitch, rowPitch * drawable->texture()->height());
          blit->endEncoding();
        }
      };
      render(0, NS::Range::Make(0, 1));
      MTL::BlitCommandEncoder *blit = cb->blitCommandEncoder();
      blit->resetCommandsInBuffer(dst, NS::Range::Make(0, 1));
      blit->copyIndirectCommandBuffer(src, NS::Range::Make(1, 4), dst, 2);
      blit->optimizeIndirectCommandBuffer(dst, NS::Range::Make(0, 1));
      blit->endEncoding();
      if(gpuIndirectRange)
      {
        MTL::ComputeCommandEncoder *writer = cb->computeCommandEncoder();
        writer->setComputePipelineState(rangePipeline);
        writer->setBuffer(rangeBuffer, 0, 0);
        writer->dispatchThreadgroups(MTL::Size::Make(1, 1, 1), MTL::Size::Make(1, 1, 1));
        writer->endEncoding();
      }
      render(1, NS::Range::Make(0, 6));
      blit = cb->blitCommandEncoder();
      blit->resetCommandsInBuffer(dst, NS::Range::Make(3, 1));
      blit->optimizeIndirectCommandBuffer(dst, NS::Range::Make(3, 1));
      blit->endEncoding();
      render(2, NS::Range::Make(2, 3));
      blit = cb->blitCommandEncoder();
      blit->copyIndirectCommandBuffer(dst, NS::Range::Make(2, 1), dst, 1);
      blit->optimizeIndirectCommandBuffer(dst, NS::Range::Make(1, 2));
      blit->endEncoding();
      render(3, NS::Range::Make(1, 4));
      cb->presentDrawable(drawable); cb->commit(); cb->waitUntilCompleted();
      EndCaptureFrame();
      if(cb->status() != MTL::CommandBufferStatusCompleted) return 7;
      if(native)
      {
        for(unsigned stage = 0; stage < 4; ++stage)
        {
          const byte *bytes = (const byte *)readback[stage]->contents();
          for(unsigned band = 0; band < 3; ++band)
          {
            unsigned x = drawable->texture()->width() * (2 * band + 1) / 6;
            unsigned y = drawable->texture()->height() / 2;
            const byte *pixel = bytes + y * rowPitch + x * 4;
            unsigned r = stage == 0 || band == 0 ? 255 : 0;
            unsigned g = stage == 1 && band == 1 ? 255 : 0;
            unsigned b = stage == 0 || band == 2 ? 255 : 0;
            if(preframeGPU && stage == 0) { r = band == 0 ? 255 : 0; g = b = 0; }
            if(pixel[0] != b || pixel[1] != g || pixel[2] != r || pixel[3] != 255)
            { TEST_WARN("T53 stage %u band %u got %u/%u/%u", stage, band, pixel[2],pixel[1],pixel[0]); return 8; }
          }
          readback[stage]->release();
        }
      }
      ++frames; pool->drain();
    }
    TEST_LOG("T53 passed %u frames / four render phases, mixed indexed commands and seven GPU ICB operations%s",
             frames, gpuIndirectRange ? " with GPU-written indirect range" : "");
    Shutdown(); return 0;
  }
};
REGISTER_TEST();
