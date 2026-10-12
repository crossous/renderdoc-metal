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
  // Optional diagnostic copy of a public logical buffer at its actual GPU use.
  // No interpretation of bytes or replacement of application inputs.
  bool SnapshotBytes(MTL::Device *device, MTL::ComputeCommandEncoder *encoder,
                     MTL::Buffer *source, MetalIndirectReadback &result,
                     uint64_t offset = 0, uint64_t length = 0);
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
  NS::SharedPtr<MTL::ComputePipelineState> m_ByteCopyPipeline;
  Binding m_Bindings[2];
};

class WrappedMTLDevice;
class WrappedMTLCommandBuffer;
// Opt-in stream-boundary observation. Never changes replay eligibility/inputs.
void SnapshotMetalDiagnosticTexture(WrappedMTLDevice *device,
    WrappedMTLCommandBuffer *owner, ResourceId encoder, const char *environmentPrefix);

// Paired capture/replay observation at an original encoder boundary. Requests
// identify public logical resources/ranges; they never establish replay support.
void SnapshotMetalDiagnosticResources(WrappedMTLDevice *device,
    WrappedMTLCommandBuffer *owner, ResourceId encoder, const rdcstr &pipelineLabel,
    bool capture, const char *environmentPrefix, const rdcarray<ResourceId> &residentResources);
