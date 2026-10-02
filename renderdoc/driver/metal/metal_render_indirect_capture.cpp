// SPDX-License-Identifier: MIT
#include "metal_device.h"
#include "metal_command_buffer.h"
#include "metal_render_command_encoder.h"
#include "metal_parallel_render_command_encoder.h"
#include "metal_buffer.h"
#include "metal_texture.h"

bool WrappedMTLDevice::CapturingRenderIndirectArguments() const
{
  return IsCaptureMode(m_State) &&
      !Process::GetEnvVariable("RENDERDOC_METAL_CAPTURE_RENDER_INDIRECT_ARGUMENTS").empty();
}

void WrappedMTLDevice::CaptureRenderIndirectWrite(WrappedMTLCommandBuffer *command,
    ResourceId pass, WrappedMTLResource *resource)
{
  if(!CapturingRenderIndirectArguments() || !command || !resource)return;
  auto info=GetRecord(command)->cmdInfo;
  SCOPED_LOCK(info->renderIndirectLock);
  const auto wrapped=(WrappedMTLObject *)resource;
  const bool known=wrapped->m_Real && (wrapped->m_Type==eResBuffer || wrapped->m_Type==eResTexture);
  info->renderIndirectWrites[pass].push_back(known?
      GetMetalIndirectWriteFootprint(Unwrap(resource),wrapped->m_Type==eResTexture):MetalIndirectWriteFootprint());
}

void WrappedMTLDevice::CaptureRenderIndirectAttachments(WrappedMTLCommandBuffer *command,
    ResourceId pass, const RDMTL::RenderPassDescriptor &descriptor)
{
  if(!CapturingRenderIndirectArguments())return;
  auto write=[&](const RDMTL::RenderPassAttachmentDescriptor &attachment) {
    if(attachment.texture)CaptureRenderIndirectWrite(command,pass,(WrappedMTLResource *)attachment.texture);
    if(attachment.resolveTexture)CaptureRenderIndirectWrite(command,pass,(WrappedMTLResource *)attachment.resolveTexture);
  };
  for(const auto &attachment:descriptor.colorAttachments)write(attachment);
  write(descriptor.depthAttachment);write(descriptor.stencilAttachment);
  if(descriptor.visibilityResultBuffer)
    CaptureRenderIndirectWrite(command,pass,(WrappedMTLResource *)descriptor.visibilityResultBuffer);
}

void WrappedMTLDevice::CaptureRenderIndirectCall(WrappedMTLCommandBuffer *command,
    ResourceId encoder, ResourceId pass, uint32_t ordinal, WrappedMTLBuffer *buffer,
    uint64_t offset, uint32_t wordCount, bool writesDeclared)
{
  if(!CapturingRenderIndirectArguments() || !IsActiveCapturing(m_State))return;
  MetalCapturedRenderIndirectArguments evidence;
  evidence.command=GetResID(command);evidence.encoder=encoder;evidence.pass=pass;
  evidence.buffer=GetResID(buffer);evidence.epoch=m_CaptureEpoch;evidence.ordinal=ordinal;
  evidence.offset=offset;evidence.wordCount=wordCount;
  evidence.writesDeclared=writesDeclared && buffer &&
      !Atomic::CmpExch32(&buffer->m_CapturedAliasable,0,0);
  evidence.readback.source=NS::RetainPtr(Unwrap(buffer));
  auto info=GetRecord(command)->cmdInfo;
  SCOPED_LOCK(info->renderIndirectLock);
  info->renderIndirectArguments.push_back(evidence);
}

void WrappedMTLDevice::FlushRenderIndirectCaptures(WrappedMTLCommandBuffer *command, ResourceId pass)
{
  if(!CapturingRenderIndirectArguments())return;
  auto info=GetRecord(command)->cmdInfo;
  SCOPED_LOCK(info->renderIndirectLock);
  const auto writes=info->renderIndirectWrites.find(pass);
  const rdcarray<MetalIndirectWriteFootprint> empty;
  for(auto &evidence:info->renderIndirectArguments)
  {
    if(evidence.pass!=pass || evidence.epoch!=m_CaptureEpoch || evidence.readback.snapshot)continue;
    if(!evidence.writesDeclared ||
       !CopyMetalRenderIndirectArguments(Unwrap(this),Unwrap(command),evidence.readback.source.get(),
          evidence.offset,evidence.wordCount,writes==info->renderIndirectWrites.end()?empty:writes->second,evidence.readback))
      RDCERR("Cannot preserve immutable Metal render indirect arguments for %s",ToStr(evidence.encoder).c_str());
  }
  // Keep writes only until the pass ends; avoid retaining every background frame's allocations.
  info->renderIndirectWrites.erase(pass);
}

void WrappedMTLRenderCommandEncoder::CaptureIndirectWrite(WrappedMTLResource *resource)
{
  if(!m_Device->CapturingRenderIndirectArguments())return;
  m_CaptureWritesDeclared=false;
  m_Device->CaptureRenderIndirectWrite(m_CommandBuffer,
      m_ParallelParent?GetResID(m_ParallelParent):m_ID,resource);
}

void WrappedMTLRenderCommandEncoder::CaptureIndirectArguments(WrappedMTLBuffer *buffer,
    uint64_t offset, uint32_t wordCount)
{
  if(!m_Device->CapturingRenderIndirectArguments() || !IsActiveCapturing(m_State))return;
  m_Device->CaptureRenderIndirectCall(m_CommandBuffer,m_ID,
      m_ParallelParent?GetResID(m_ParallelParent):m_ID,m_CaptureIndirectOrdinal++,buffer,offset,
      wordCount,m_CaptureWritesDeclared);
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_CaptureRenderIndirectArgumentsCount(SerialiserType &ser,uint32_t count)
{
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_CHECK_READ_ERRORS();
  return !ser.IsReading() || IsStructuredExporting(m_State) ||
      (m_HasCapturedRenderIndirectArguments && count==m_CapturedRenderIndirectArgumentsCount);
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_CaptureRenderIndirectArguments(SerialiserType &ser,
    ResourceId command,ResourceId encoder,ResourceId pass,uint32_t ordinal,
    ResourceId buffer,uint64_t offset,uint32_t wordCount,bool writesDeclared,
    rdcarray<uint32_t> arguments)
{
  SERIALISE_ELEMENT(command).Important();
  SERIALISE_ELEMENT(encoder).Important();
  SERIALISE_ELEMENT(pass).Important();
  SERIALISE_ELEMENT(ordinal).Important();
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(wordCount).Important();
  SERIALISE_ELEMENT(writesDeclared).Important();
  SERIALISE_ELEMENT(arguments).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(ser.IsReading() && !IsStructuredExporting(m_State))
  {
    const auto found=m_CapturedRenderIndirectArguments.find(make_rdcpair(encoder,ordinal));
    return writesDeclared && found!=m_CapturedRenderIndirectArguments.end() &&
        found->second.command==command && found->second.pass==pass && found->second.buffer==buffer &&
        found->second.offset==offset && found->second.wordCount==wordCount && found->second.arguments==arguments;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_CaptureRenderIndirectArgumentsCount(ReadSerialiser &,uint32_t);
template bool WrappedMTLDevice::Serialise_CaptureRenderIndirectArgumentsCount(WriteSerialiser &,uint32_t);
template bool WrappedMTLDevice::Serialise_CaptureRenderIndirectArguments(ReadSerialiser &,ResourceId,ResourceId,ResourceId,uint32_t,ResourceId,uint64_t,uint32_t,bool,rdcarray<uint32_t>);
template bool WrappedMTLDevice::Serialise_CaptureRenderIndirectArguments(WriteSerialiser &,ResourceId,ResourceId,ResourceId,uint32_t,ResourceId,uint64_t,uint32_t,bool,rdcarray<uint32_t>);
