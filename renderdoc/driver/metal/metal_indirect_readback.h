// SPDX-License-Identifier: MIT
#pragma once
#include "api/replay/renderdoc_replay.h"
#include "official/metal-cpp.h"

// Native debug instrumentation mirrors Vulkan's per-use indirect readback.
// No CPU wait, encoder split or replacement of the application's dispatch.
MTL::ComputePipelineState *CreateMetalIndirectReadbackPipeline(MTL::Device *device);
// Preserve the actual GPU arguments and use an identical copy for execution,
// or a zero-work copy on extent overflow / a changed frozen invocation contract. Word 3 records
// the validation result; words 4..6 are the execution copy.
MTL::ComputePipelineState *CreateMetalIndirectReplayPipeline(MTL::Device *device);
struct MetalIndirectReadback
{
  NS::SharedPtr<MTL::Buffer> snapshot;
  NS::SharedPtr<MTL::Buffer> source;
  NS::SharedPtr<MTL::ComputePipelineState> copyPipeline;
};
class MetalComputeIndirectCapture
{
public:
  void BindPipeline(MTL::ComputePipelineState *pipeline);
  void BindBuffer(MTL::Buffer *buffer, uint64_t offset, uint32_t index);
  void BindBytes(const rdcarray<byte> &data, uint32_t index);
  void SetBufferOffset(uint64_t offset, uint32_t index);
  void InvalidateSlot(uint32_t index);
  void Clear();
  bool Snapshot(MTL::Device *device, MTL::ComputeCommandEncoder *encoder,
                MTL::Buffer *source, uint64_t offset, MetalIndirectReadback &result);
private:
  struct Binding
  {
    NS::SharedPtr<MTL::Buffer> buffer;
    uint64_t offset = 0;
    rdcarray<byte> bytes;
    bool known = true;
  };
  NS::SharedPtr<MTL::ComputePipelineState> m_Pipeline;
  NS::SharedPtr<MTL::ComputePipelineState> m_CopyPipeline;
  Binding m_Bindings[2];
};
