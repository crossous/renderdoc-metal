// SPDX-License-Identifier: MIT
#include "metal_indirect_readback.h"
#include <cstring>

MTL::ComputePipelineState *CreateMetalIndirectReadbackPipeline(MTL::Device *device)
{
  if(!device) return NULL;
  const char *code=R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void rdoc_indirect_readback(device const uint *source [[buffer(0)]],
                                  device uint *destination [[buffer(1)]])
{ destination[0]=source[0]; destination[1]=source[1]; destination[2]=source[2];
  destination[3]=0x52444349u; }
)MSL";
  NS::Error *error = NULL;
  auto library = NS::TransferPtr(device->newLibrary(NS::String::string(code, NS::UTF8StringEncoding), NULL, &error));
  if(!library) return NULL;
  auto function = NS::TransferPtr(library->newFunction(NS::String::string("rdoc_indirect_readback", NS::UTF8StringEncoding)));
  return function ? device->newComputePipelineState(function.get(), &error) : NULL;
}

void MetalComputeIndirectCapture::BindPipeline(MTL::ComputePipelineState *pipeline)
{
  m_Pipeline = NS::RetainPtr(pipeline);
}
void MetalComputeIndirectCapture::BindBuffer(MTL::Buffer *buffer, uint64_t offset, uint32_t index)
{
  if(index >= 2) return;
  auto &binding=m_Bindings[index];
  binding.buffer=NS::RetainPtr(buffer); binding.offset=offset; binding.bytes.clear(); binding.known=true;
}
void MetalComputeIndirectCapture::BindBytes(const rdcarray<byte> &data, uint32_t index)
{
  if(index >= 2) return;
  auto &binding=m_Bindings[index];
  binding.buffer.reset(); binding.offset=0; binding.bytes=data; binding.known=true;
}
void MetalComputeIndirectCapture::SetBufferOffset(uint64_t offset, uint32_t index)
{
  if(index >= 2) return;
  auto &binding=m_Bindings[index];
  binding.offset=offset;
  if(!binding.buffer || !binding.bytes.empty()) binding.known=false;
}
void MetalComputeIndirectCapture::InvalidateSlot(uint32_t index)
{
  if(index < 2) m_Bindings[index].known=false;
}
void MetalComputeIndirectCapture::Clear()
{
  m_Pipeline.reset(); m_CopyPipeline.reset();
  for(auto &binding : m_Bindings) binding=Binding();
}

bool MetalComputeIndirectCapture::Snapshot(MTL::Device *device, MTL::ComputeCommandEncoder *encoder,
    MTL::Buffer *source, uint64_t offset, MetalIndirectReadback &result)
{
  if(!device || !encoder || !m_Pipeline || m_Pipeline->device()!=device || !source || source->device()!=device ||
     (offset & 3) || offset > source->length() || 12 > source->length()-offset)
    return false;
  for(const auto &binding : m_Bindings)
    if(!binding.known || binding.bytes.size()>4096 || (!binding.buffer && binding.offset) ||
       (binding.buffer && (binding.buffer->device()!=device || binding.offset>=binding.buffer->length())))
      return false;
  if(!m_CopyPipeline) m_CopyPipeline=NS::TransferPtr(CreateMetalIndirectReadbackPipeline(device));
  if(!m_CopyPipeline) return false;
  auto snapshot=NS::TransferPtr(device->newBuffer(16, MTL::ResourceStorageModeShared));
  if(!snapshot) return false;
  memset(snapshot->contents(), 0, 16);
  encoder->memoryBarrier(MTL::BarrierScopeBuffers);
  encoder->setComputePipelineState(m_CopyPipeline.get());
  encoder->setBuffer(source, offset, 0); encoder->setBuffer(snapshot.get(), 0, 1);
  encoder->dispatchThreadgroups(MTL::Size(1,1,1), MTL::Size(1,1,1));
  encoder->memoryBarrier(MTL::BarrierScopeBuffers);
  encoder->setComputePipelineState(m_Pipeline.get());
  for(uint32_t slot=0;slot<2;slot++) {
    const auto &binding=m_Bindings[slot];
    if(!binding.bytes.empty()) encoder->setBytes(binding.bytes.data(), binding.bytes.size(), slot);
    else encoder->setBuffer(binding.buffer.get(), binding.offset, slot);
  }
  result.snapshot=snapshot; result.source=NS::RetainPtr(source); result.copyPipeline=m_CopyPipeline;
  return true;
}
