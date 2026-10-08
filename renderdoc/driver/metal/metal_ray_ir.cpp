// SPDX-License-Identifier: MIT
#include "metal_device.h"
#include "metal_buffer.h"
#include "metal_acceleration_structure.h"
#include "metal_visible_function_table.h"
#include "metal_function.h"
#include "metal_texture.h"
#include "metal_sampler_state.h"
#include "metal_replay.h"

static void MarkASHeaderRecipeReferences(MetalResourceManager *manager, ResourceId structure);

bool WrappedMTLDevice::IsIRDescriptorMetadataAddress(ResourceId buffer,uint64_t offset) const
{
  for(const auto &table:m_DescriptorTables)
    if(table.buffer==buffer && table.stride && offset>=table.offset &&
       (offset-table.offset)/table.stride<table.count)return true;
  return false;
}

bool WrappedMTLDevice::HasRestoredRuntimeTableBinding(ResourceId pipeline, uint32_t stage,
    uint32_t slot, ResourceId buffer, uint64_t offset) const
{
  const auto proof=m_IRRuntimeRestoredBindings.find(make_rdcpair(m_CurChunkOffset,stage));
  if(proof==m_IRRuntimeRestoredBindings.end() || proof->second.pipeline!=pipeline)return false;
  const auto binding=proof->second.tables.find(slot);
  if(binding==proof->second.tables.end() || binding->second.resourceId!=buffer ||
     binding->second.byteOffset!=offset || !IsIRDescriptorMetadataAddress(buffer,offset))return false;
  auto object=m_ResourceManager->GetResource(buffer,true);
  return object && object->m_Type==eResBuffer && object->m_Real &&
      offset<Unwrap((WrappedMTLBuffer *)object)->length();
}

bool WrappedMTLDevice::ReadProvenIRComputeUniformBytes(uint64_t fileOffset, ResourceId buffer,
    uint64_t offset, uint32_t bytes, bytebuf &data) const
{
  const auto reads=m_IRComputeUniformReadContents.find(fileOffset);
  if(reads==m_IRComputeUniformReadContents.end() || !bytes || bytes>8)return false;
  for(const auto &range:reads->second)
    if(range.first.first==buffer && offset>=range.first.second &&
       offset-range.first.second<=range.second.size() &&
       bytes<=range.second.size()-(offset-range.first.second))
    {
      data.assign(range.second.data()+offset-range.first.second,bytes);
      if(Process::GetEnvVariable("RENDERDOC_METAL_TRACE_UNIFORM_PROOFS")=="1")
      {
        uint64_t value=0;memcpy(&value,data.data(),bytes);
        RDCLOG("MetalUniformReadProof chunk=%llu buffer=%s at=%llu bytes=%u value=%llu",
            (unsigned long long)fileOffset,ToStr(buffer).c_str(),(unsigned long long)offset,bytes,
            (unsigned long long)value);
      }
      return true;
    }
  return false;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_CaptureIRComputeReflection(SerialiserType &ser,
    ResourceId pipeline, const rdcstr &reflection)
{
  SERIALISE_ELEMENT(pipeline).Important();
  SERIALISE_ELEMENT(reflection).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && !IsStructuredExporting(m_State))
  {
    const auto expected = m_IRComputeReflections.find(pipeline);
    auto object = GetResourceManager()->GetResource(pipeline, true);
    if(expected == m_IRComputeReflections.end() || expected->second != reflection ||
       !object || object->m_Type != eResComputePipelineState || !object->m_Real)
      return false;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_CaptureIRComputeReflection(ReadSerialiser &,
    ResourceId, const rdcstr &);
template bool WrappedMTLDevice::Serialise_CaptureIRComputeReflection(WriteSerialiser &,
    ResourceId, const rdcstr &);

uint32_t WrappedMTLDevice::CaptureRayASHeader(void *object, const RENDERDOC_AnnotationValue *value)
{
  if(!IsCaptureMode(m_State)) return 2;
  auto buffer = GetResourceManager()->FindAnnotationObject(object);
  auto structure = GetResourceManager()->FindAnnotationObject((void *)(uintptr_t)value->vector.uint64[1]);
  auto contributions = GetResourceManager()->FindAnnotationObject((void *)(uintptr_t)value->vector.uint64[2]);
  const uint64_t offset = value->vector.uint64[0], contributionOffset = value->vector.uint64[3];
  if(!buffer || buffer->m_Type != eResBuffer || !buffer->m_Real || buffer->m_CapturedAliasable ||
     !structure || structure->m_Type != eResAccelerationStructure || !structure->m_Real ||
     !contributions || contributions->m_Type != eResBuffer || !contributions->m_Real ||
     buffer == contributions || offset % 8 || contributionOffset % 4) return 2;
  auto native = Unwrap((WrappedMTLBuffer *)buffer);
  auto contribution = Unwrap((WrappedMTLBuffer *)contributions);
  // Capture public header bytes from a known Shared placement buffer as well.
  // Replay still requires the independently validated immutable standalone contract.
  if(native->storageMode() != MTL::StorageModeShared || !native->contents() ||
     native->length() > 128ULL*1024*1024 || offset > native->length() || 64 > native->length()-offset ||
     contributionOffset > contribution->length() || 4 > contribution->length()-contributionOffset ||
     contribution->gpuAddress() > UINT64_MAX-contributionOffset) return 2;
  bytebuf bytes((byte *)native->contents()+offset,64);
  uint64_t words[8]; memcpy(words,bytes.data(),64);
  if(words[0] != Unwrap((WrappedMTLAccelerationStructure *)structure)->gpuResourceID()._impl ||
     !words[0] || words[1] != contribution->gpuAddress()+contributionOffset) return 2;
  for(unsigned i=2;i<8;i++) if(words[i]) return 2;
  const auto key = make_rdcpair(GetResID(buffer),offset);
  if(m_RayASHeaders.size()>=128)
  {
    for(auto it=m_RayASHeaders.begin();it!=m_RayASHeaders.end();)
      if(!GetResourceManager()->HasResource(it->second.buffer) ||
         !GetResourceManager()->HasResource(it->second.structure) ||
         !GetResourceManager()->HasResource(it->second.contributions)) it=m_RayASHeaders.erase(it);
      else ++it;
  }
  if(m_RayASHeaders.size()>=128 && !m_RayASHeaders.count(key)) return 2;
  // Background facts are snapshotted at capture start, never appended to a
  // resource's immutable creation record. A later capture needs the latest version.
  if(IsActiveCapturing(m_State))
  {
    WriteSerialiser &ser=GetThreadSerialiser();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_DeclareRayASHeader);
    Serialise_DeclareRayASHeader(ser,GetResID(buffer),offset,GetResID(structure),
        GetResID(contributions),contributionOffset,bytes);
    AddFrameCaptureRecordChunk(scope.Get());
    for(auto resource:{buffer,structure,contributions})
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(resource),eFrameRef_Read);
    MarkASHeaderRecipeReferences(GetResourceManager(),GetResID(structure));
  }
  m_RayASHeaders[key]={GetResID(buffer),GetResID(structure),GetResID(contributions),offset,contributionOffset,bytes};
  return 0;
}

static void MarkASHeaderRecipeReferences(MetalResourceManager *manager, ResourceId structure)
{
  // A Header references an opaque AS even when the current shader does not
  // consume it. Its captured reconstruction recipe still needs the primitive
  // children and their input resources, just like encoder AS references do.
  auto mark=[&](ResourceId id) {
    if(id!=ResourceId())manager->MarkResourceFrameReferenced(id,eFrameRef_Read);
  };
  mark(structure);
  auto object=manager->GetResource(structure,true);
  if(!object || object->m_Type!=eResAccelerationStructure)return;
  auto build=((WrappedMTLAccelerationStructure *)object)->m_CapturedInitialBuild;
  if(!build)return;
  mark(build->source);mark(build->indexSource);
  mark(build->sizeSource);
  for(ResourceId child:build->children)
  {
    mark(child);
    auto childObject=manager->GetResource(child,true);
    if(!childObject || childObject->m_Type!=eResAccelerationStructure)continue;
    auto input=((WrappedMTLAccelerationStructure *)childObject)->m_CapturedInitialBuild;
    if(input){mark(input->source);mark(input->indexSource);mark(input->sizeSource);}
  }
}

void WrappedMTLDevice::SnapshotRayASHeaders()
{
  for(auto it=m_RayASHeaders.begin();it!=m_RayASHeaders.end();)
  {
    const auto &h=it->second;
    if(!GetResourceManager()->HasResource(h.buffer) ||
       !GetResourceManager()->HasResource(h.structure) ||
       !GetResourceManager()->HasResource(h.contributions))
    { it=m_RayASHeaders.erase(it); continue; }
    for(ResourceId id:{h.buffer,h.structure,h.contributions})
      GetResourceManager()->MarkResourceFrameReferenced(id,eFrameRef_Read);
    MarkASHeaderRecipeReferences(GetResourceManager(),h.structure);
    WriteSerialiser &ser=GetThreadSerialiser();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_DeclareRayASHeader);
    Serialise_DeclareRayASHeader(ser,h.buffer,h.offset,h.structure,h.contributions,h.contributionOffset,h.bytes);
    m_DescriptorHistorySnapshot.push_back(scope.Get());
    ++it;
  }
}

bool WrappedMTLDevice::ValidateRayASHeader(const RayASHeader &h)
{
  auto b = GetResourceManager()->GetResource(h.buffer,true);
  auto a = GetResourceManager()->GetResource(h.structure,true);
  auto c = GetResourceManager()->GetResource(h.contributions,true);
  auto identity = m_ReplayGPUIdentities.find(h.contributions);
  auto future=m_DescriptorFrameBuffers.find(h.buffer);
  const bool born=m_DescriptorPreflight && future!=m_DescriptorFrameBuffers.end();
  if(h.bytes.size()!=64 || (!born && (!b || b->m_Type!=eResBuffer || !b->m_Real)) ||
     !a || a->m_Type!=eResAccelerationStructure || !a->m_Real ||
     !c || c->m_Type!=eResBuffer || !c->m_Real || b==c ||
     (!born && b->m_CapturedAliasable) || a->m_CapturedAliasable || c->m_CapturedAliasable ||
     identity==m_ReplayGPUIdentities.end() || identity->second.kind!=0) return false;
  auto native = born ? NULL : Unwrap((WrappedMTLBuffer *)b);
  auto contribution = Unwrap((WrappedMTLBuffer *)c);
  const uint64_t length=born?future->second.length:native->length();
  if((born ? (m_DescriptorCoverage!=65 ||
         !m_DescriptorPreflightLiveBuffers.count(h.buffer) ||
         m_DescriptorPreflightAliasedBuffers.count(h.buffer) || length!=64 || h.offset ||
         (future->second.options & 0xf0ULL)!=MTL::ResourceStorageModeShared) :
       native->storageMode()!=MTL::StorageModeShared || !native->contents()) ||
     length>16*1024 ||
     (contribution->storageMode()!=MTL::StorageModeShared && contribution->storageMode()!=MTL::StorageModePrivate) ||
     h.offset%8 || h.offset>length || 64>length-h.offset ||
     h.contributionOffset%4 || h.contributionOffset>contribution->length() ||
     4>contribution->length()-h.contributionOffset ||
     identity->second.value>UINT64_MAX-h.contributionOffset) return false;
  uint64_t words[8]; memcpy(words,h.bytes.data(),64);
  // Publishing a typed GPU pointer does not consume the contribution allocation.
  // The shader consumer separately proves its read range and initial contents.
  if(!words[0] || words[0]!=((WrappedMTLAccelerationStructure *)a)->m_CapturedGPUResourceID ||
     words[1]!=identity->second.value+h.contributionOffset) return false;
  for(unsigned i=2;i<8;i++) if(words[i]) return false;
  return true;
}

bool WrappedMTLDevice::ApplyRayASHeaderWrite(const RayASHeader &h)
{
  const bool typedHeaders=m_DescriptorCoverage==65;
  if((m_DescriptorCoverage!=3 && !typedHeaders) || m_RayASHeaderFrameCursor>=m_RayASHeaderFrameWrites.size() ||
     !ValidateRayASHeader(h)) return false;
  const auto &expected=m_RayASHeaderFrameWrites[m_RayASHeaderFrameCursor];
  if(h.buffer!=expected.buffer || h.offset!=expected.offset || h.structure!=expected.structure ||
     h.contributions!=expected.contributions || h.contributionOffset!=expected.contributionOffset ||
     h.bytes!=expected.bytes) return false;
  const auto key=make_rdcpair(h.buffer,h.offset);
  if(!m_RayASHeaders.count(key))
  {
    auto born=m_DescriptorFrameBuffers.find(h.buffer);
    if(!typedHeaders || born==m_DescriptorFrameBuffers.end() || born->second.length!=64 || h.offset ||
       m_RayASHeaderCurrent.count(key)) return false;
    // The declaration owns all 64 bytes of this new public runtime header.
    // Padding in a larger allocation needs its own full submission snapshot.
    m_DescriptorRawContents[h.buffer]=h.bytes;
  }
  auto raw=m_DescriptorRawContents.find(h.buffer);
  if(raw==m_DescriptorRawContents.end() || h.offset>raw->second.size() ||
     64>raw->second.size()-h.offset) return false;
  m_RayASHeaderCurrent[key]=h;
  memcpy(raw->second.data()+h.offset,h.bytes.data(),64);
  m_RayASHeaderFrameCursor++;
  if(m_DescriptorPreflight) return typedHeaders || ValidateDescriptorValues(h.buffer,raw->second);
  // A CPU header write must not race a prior committed reader, just as a
  // descriptor update on the existing D3D12/Vulkan submission paths.
  for(const auto &command:m_ReplayCommandBuffers)
    if(command.second.buffer && IsReplayCommandBufferCommitted(command.second.buffer) &&
       !WaitReplayCommandBuffer(command.second.buffer,"typed AS header publication")) return false;
  auto buffer=(WrappedMTLBuffer *)GetResourceManager()->GetResource(h.buffer);
  if(!ReplayCPUBufferUpdate(buffer,h.offset,h.bytes)) return false;
  // Active replay normally restores commit-owned CPU snapshots. This explicit
  // public-header write also needs its in-stream event to be observable.
  if(IsActiveReplaying(m_State))
    return RestoreDescriptorTable(h.buffer,raw->second);
  return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareRayASHeader(SerialiserType &ser, ResourceId buffer,
    uint64_t offset, ResourceId structure, ResourceId contributions,
    uint64_t contributionOffset, bytebuf bytes)
{
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(contributions).Important();
  SERIALISE_ELEMENT(contributionOffset).Important();
  SERIALISE_ELEMENT(bytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && !IsStructuredExporting(m_State))
  {
    RayASHeader h={buffer,structure,contributions,offset,contributionOffset,bytes};
    if(m_DescriptorPreflight || GetReplayEpoch()!=0) return ApplyRayASHeaderWrite(h);
    auto initial=m_RayASHeaders.find(make_rdcpair(buffer,offset));
    if((m_DescriptorCoverage!=3 && !(m_DescriptorCoverage==65)) || initial==m_RayASHeaders.end() ||
       initial->second.structure!=structure || initial->second.contributions!=contributions ||
       initial->second.contributionOffset!=contributionOffset || initial->second.bytes!=bytes ||
       !ValidateRayASHeader(h)) return false;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_DeclareRayASHeader(ReadSerialiser &, ResourceId,
    uint64_t, ResourceId, ResourceId, uint64_t, bytebuf);
template bool WrappedMTLDevice::Serialise_DeclareRayASHeader(WriteSerialiser &, ResourceId,
    uint64_t, ResourceId, ResourceId, uint64_t, bytebuf);

bool WrappedMTLDevice::PatchRayASHeaders(ResourceId buffer, const bytebuf &raw, bytebuf &patched)
{
  for(const auto &entry : m_RayASHeaderCurrent)
  {
    const auto &h = entry.second;
    if(h.buffer != buffer) continue;
    if(h.offset > raw.size() || 64 > raw.size()-h.offset || patched.size() != raw.size() ||
       h.bytes.size() != 64 || memcmp(raw.data()+h.offset,h.bytes.data(),64)) return false;
    auto a = GetResourceManager()->GetResource(h.structure,true);
    auto c = GetResourceManager()->GetResource(h.contributions,true);
    auto identity = m_ReplayGPUIdentities.find(h.contributions);
    if(!a || a->m_Type != eResAccelerationStructure || !a->m_Real || a->m_CapturedAliasable ||
       !c || c->m_Type != eResBuffer || !c->m_Real || c->m_CapturedAliasable ||
       identity == m_ReplayGPUIdentities.end() || identity->second.kind != 0) return false;
    auto contribution = Unwrap((WrappedMTLBuffer *)c);
    if(h.contributionOffset > contribution->length() || 4 > contribution->length()-h.contributionOffset ||
       identity->second.value > UINT64_MAX-h.contributionOffset ||
       contribution->gpuAddress() > UINT64_MAX-h.contributionOffset) return false;
    uint64_t words[8]; memcpy(words,h.bytes.data(),64);
    auto structure = (WrappedMTLAccelerationStructure *)a;
    if(!words[0] || words[0] != structure->m_CapturedGPUResourceID ||
       words[1] != identity->second.value+h.contributionOffset) return false;
    for(unsigned i=2;i<8;i++) if(words[i]) return false;
    words[0] = Unwrap(structure)->gpuResourceID()._impl;
    words[1] = contribution->gpuAddress()+h.contributionOffset;
    if(!words[0] || !words[1]) return false;
    memcpy(patched.data()+h.offset,words,64);
  }
  return true;
}

bool WrappedMTLDevice::PrepareRayASHeaderInitialContents()
{
  if(m_RayASHeaders.empty()) return true;
  if(m_DescriptorCoverage!=65) return false;
  for(const auto &entry:m_RayASHeaders)
  {
    const auto &h=entry.second;
    if(!ValidateRayASHeader(h)) return false;
    auto native=Unwrap((WrappedMTLBuffer *)GetResourceManager()->GetResource(h.buffer));
    auto saved=m_ReplayBufferInitialContents.find(h.buffer);
    if(saved==m_ReplayBufferInitialContents.end() && m_ReplayBuffersWithCreationContents.count(h.buffer))
    {
      m_ReplayBufferInitialContents[h.buffer]=bytebuf((byte *)native->contents(),native->length());
      saved=m_ReplayBufferInitialContents.find(h.buffer);
    }
    if(saved==m_ReplayBufferInitialContents.end() || saved->second.size()!=native->length() ||
       memcmp(saved->second.data()+h.offset,h.bytes.data(),64)) return false;
    m_DescriptorRawContents[h.buffer]=saved->second;
    m_ReplayCPUUpdatedBuffers.insert(h.buffer);
    if(!RestoreDescriptorTable(h.buffer,saved->second)) return false;
  }
  return true;
}

bool WrappedMTLDevice::IsRayQueryHeapOutputResource(WrappedMTLObject *object) const
{
  if(!object || !object->m_Real || object->m_CapturedAliasable) return false;
  if(object->m_Type==eResBuffer)
  {
    auto native=Unwrap((WrappedMTLBuffer *)object);
    const bool shared=native->storageMode()==MTL::StorageModeShared && !native->heap();
    const bool priv=native->storageMode()==MTL::StorageModePrivate &&
        (!native->heap() || native->hazardTrackingMode()==MTL::HazardTrackingModeTracked);
    return (shared || priv) && native->length() && native->length()<=16*1024;
  }
  return IsRayQueryHeapTextureResource(object,true);
}

bool WrappedMTLDevice::HasRayQueryHeapTextureInitialContents(ResourceId texture) const
{
  if(HasReplayTextureInitialContents(texture)) return true;
  const ResourceId parent=m_Replay->GetBufferTextureSource(texture);
  auto object=m_ResourceManager->GetResource(parent,true);
  auto view=m_ResourceManager->GetResource(texture,true);
  if(!object || object->m_Type!=eResBuffer || !object->m_Real ||
     !view || view->m_Type!=eResTexture || !view->m_Real ||
     IsReplayResourceAliasable(parent) || object->m_CapturedAliasable) return false;
  auto native=Unwrap((WrappedMTLBuffer *)object);
  auto real=Unwrap((WrappedMTLTexture *)view);
  const auto initial=m_ReplayBufferInitialContents.find(parent);
  return real->textureType()==MTL::TextureTypeTextureBuffer && real->buffer()==native &&
      initial!=m_ReplayBufferInitialContents.end() && initial->second.size()==native->length();
}

bool WrappedMTLDevice::IsRayQueryHeapTextureResource(WrappedMTLObject *object, bool write,
                                                    uint64_t maximumBytes) const
{
  if(!object || !object->m_Real || object->m_Type!=eResTexture || object->m_CapturedAliasable) return false;
  auto native=Unwrap((WrappedMTLTexture *)object);
  uint32_t bw=0,bh=0,bytes=0;
  const auto format=MakeResourceFormat(native->pixelFormat());
  uint64_t usage=native->usage();
  const auto capturedUsage=m_DescriptorTextureUsages.find(GetResID(object));
  if(capturedUsage!=m_DescriptorTextureUsages.end()) usage=capturedUsage->second;
  const bool texel=native->textureType()==MTL::TextureTypeTextureBuffer;
  if((native->textureType()!=MTL::TextureType2D && !texel) || !native->width() || !native->height() ||
     native->width()>(texel?128ULL*1024*1024:8192) || native->height()>8192 || native->depth()!=1 ||
     native->arrayLength()!=1 || native->mipmapLevelCount()!=1 || native->sampleCount()!=1 ||
     native->framebufferOnly() || !(usage & (write?MTL::TextureUsageShaderWrite:MTL::TextureUsageShaderRead)) ||
     (native->storageMode()!=MTL::StorageModeShared && native->storageMode()!=MTL::StorageModePrivate) ||
     (native->heap() && native->hazardTrackingMode()!=MTL::HazardTrackingModeTracked) ||
     !GetTextureDataBlockShape(native->pixelFormat(),bw,bh,bytes) || !bw || !bh || !bytes) return false;
  if(texel)
  {
    const ResourceId parent=GetBufferTextureParent(GetResID(object));
    auto backing=m_ResourceManager->GetResource(parent,true);
    auto buffer=backing && backing->m_Type==eResBuffer && backing->m_Real?
        Unwrap((WrappedMTLBuffer *)backing):NULL;
    if(!buffer || native->buffer()!=buffer || native->height()!=1 || bw!=1 || bh!=1 ||
       buffer->storageMode()!=MTL::StorageModePrivate || buffer->length()>128ULL*1024*1024 ||
       backing->m_CapturedAliasable || IsReplayResourceAliasable(parent) ||
       (m_DescriptorPreflight && m_DescriptorPreflightAliasedBuffers.count(parent)) ||
       native->bufferOffset()>buffer->length() || native->width()>(buffer->length()-native->bufferOffset())/bytes)
      return false;
  }
  const uint64_t blocks=((native->width()+bw-1)/bw)*((native->height()+bh-1)/bh);
  // Creation usage grants permission, while the declared SRV/UAV role determines
  // shader access. Read dependencies must still pass the submission write proof.
  return write ? format.compType!=CompType::Depth && format.compCount && bw==1 && bh==1 && blocks<=maximumBytes/bytes :
      blocks<=128ULL*1024*1024/bytes;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareRayQueryHeapDispatch(SerialiserType &ser,
    ResourceId pipeline, ResourceId heap, uint64_t slotOffset, ResourceId output)
{
  SERIALISE_ELEMENT(pipeline).Important();
  SERIALISE_ELEMENT(heap).Important();
  SERIALISE_ELEMENT(slotOffset).Important();
  SERIALISE_ELEMENT(output).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && !IsStructuredExporting(m_State))
  {
    if(m_DescriptorCoverage!=65) return false;
    bool declared=false;
    for(const auto &d:m_RayQueryHeapDispatches)
      declared |= d.pipeline==pipeline && d.heap==heap && d.slotOffset==slotOffset && d.output==output;
    auto p=GetResourceManager()->GetResource(pipeline,true);
    if(!declared || !p || p->m_Type!=eResComputePipelineState || !p->m_Real ||
       HasRayIRPipeline(pipeline) || HasRayQueryPipeline(pipeline) || heap==output) return false;
    auto h=GetResourceManager()->GetResource(heap,true);
    if(!h || h->m_Type!=eResBuffer || !h->m_Real || h->m_CapturedAliasable ||
       !IsRayQueryHeapOutputResource(GetResourceManager()->GetResource(output,true))) return false;
    auto heapBuffer=Unwrap((WrappedMTLBuffer *)h);
    if(heapBuffer->heap() || heapBuffer->storageMode()!=MTL::StorageModeShared ||
       !heapBuffer->length() || heapBuffer->length()>16*1024) return false;
    auto native=Unwrap((WrappedMTLBuffer *)GetResourceManager()->GetResource(heap));
    if(slotOffset%24 || slotOffset>native->length() || 24>native->length()-slotOffset) return false;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_DeclareRayQueryHeapDispatch(ReadSerialiser &, ResourceId,
    ResourceId, uint64_t, ResourceId);
template bool WrappedMTLDevice::Serialise_DeclareRayQueryHeapDispatch(WriteSerialiser &, ResourceId,
    ResourceId, uint64_t, ResourceId);

bool WrappedMTLDevice::AddRayQueryHeapCBVRoot(ResourceId pipeline,
    const RayQueryHeapCBVRoot &root, bool idempotent)
{
  const RayQueryHeapDispatch *query=NULL;
  for(const auto &d:m_RayQueryHeapDispatches) if(d.pipeline==pipeline) query=&d;
  if(!query || !root.rootCount || root.rootCount>16 || root.offset%8 ||
     root.offset>=root.rootCount*8 || !root.bytes || root.bytes>64*1024 ||
     root.outputSlot%24 || root.outputSlot==query->slotOffset || root.outputSlot>16*1024-24)
    return false;
  auto &roots=m_RayQueryHeapCBVRoots[pipeline];
  for(const auto &old:roots)
  {
    if(old.outputSlot!=root.outputSlot || old.rootCount!=root.rootCount) return false;
    if(old.offset==root.offset) return idempotent && old.bytes==root.bytes;
  }
  if(roots.size()>=root.rootCount) return false;
  roots.push_back(root);return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareRayQueryHeapCBVRoot(SerialiserType &ser, ResourceId pipeline,
    uint64_t offset, uint64_t bytes, uint64_t outputSlot, uint64_t rootCount)
{
  SERIALISE_ELEMENT(pipeline).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(bytes).Important();
  SERIALISE_ELEMENT(outputSlot).Important();
  SERIALISE_ELEMENT(rootCount).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && !IsStructuredExporting(m_State))
  {
    auto p=GetResourceManager()->GetResource(pipeline,true);
    if(m_DescriptorCoverage!=65 || !p || p->m_Type!=eResComputePipelineState || !p->m_Real ||
       !HasRayQueryHeapPipeline(pipeline)) return false;
    bool declared=false;
    for(const auto &root:m_RayQueryHeapCBVRoots[pipeline])
      declared |= root.offset==offset && root.bytes==bytes && root.outputSlot==outputSlot && root.rootCount==rootCount;
    if(!declared) return false;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_DeclareRayQueryHeapCBVRoot(ReadSerialiser &, ResourceId,
    uint64_t,uint64_t,uint64_t,uint64_t);
template bool WrappedMTLDevice::Serialise_DeclareRayQueryHeapCBVRoot(WriteSerialiser &, ResourceId,
    uint64_t,uint64_t,uint64_t,uint64_t);

bool WrappedMTLDevice::AddIRComputeRoot(ResourceId pipeline, const RayIRLocalRoot &root,
                                       bool idempotent)
{
  auto ranges=m_RayQueryHeapCBVRoots.find(pipeline);
  if(!HasRayQueryHeapPipeline(pipeline) || ranges==m_RayQueryHeapCBVRoots.end() ||
     (root.kind!=3 && root.kind!=4) || !root.count || root.count>64 || !root.bytes ||
     root.bytes>64*1024 || root.offset%8 ||
     (root.kind==3 ? root.bytes!=root.count*24 : root.count!=1)) return false;
  bool range=false;
  for(const auto &r:ranges->second) range |= r.offset==root.offset && r.bytes==root.bytes;
  if(!range) return false;
  auto &roots=m_IRComputeRoots[pipeline];
  for(const auto &old:roots)
    if(old.offset==root.offset)
      return idempotent && old.kind==root.kind && old.count==root.count && old.bytes==root.bytes;
  if(roots.size()>=16) return false;
  roots.push_back(root);return true;
}

bool WrappedMTLDevice::AddIRComputeHeapEntry(ResourceId pipeline, const RayIRHeapEntry &entry,
                                            bool idempotent)
{
  // Texture SRV/UAV roles share the same native identity namespace; the
  // explicit access kind determines consumer validation and event usage.
  // kind8 is a texture UAV whose identity is resolved from the current slot,
  // independently of the PSO's frame-initial output association.
  if(!HasRayQueryHeapPipeline(pipeline) || entry.heap || (entry.kind!=1 && entry.kind!=4 && entry.kind!=8) || entry.bytes ||
     entry.index>=16*1024/24) return false;
  const auto roots=m_RayQueryHeapCBVRoots.find(pipeline);
  if(roots==m_RayQueryHeapCBVRoots.end() || roots->second.empty()) return false;
  for(const auto &q:m_RayQueryHeapDispatches)
    if(q.pipeline==pipeline && q.slotOffset==entry.index*24) return false;
  if(entry.kind!=8 && (roots->second[0].outputSlot==entry.index*24)!=(entry.kind==4)) return false;
  auto &entries=m_IRComputeHeapEntries[pipeline];
  for(const auto &old:entries)
    if(old.heap==entry.heap && old.index==entry.index)
      return idempotent && old.kind==entry.kind && old.bytes==entry.bytes;
  if(entries.size()>=64) return false;
  entries.push_back(entry);return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareIRComputeRoot(SerialiserType &ser, ResourceId pipeline,
    uint64_t offset, uint64_t kind, uint64_t count, uint64_t bytes)
{
  SERIALISE_ELEMENT(pipeline).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(kind).Important();
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_ELEMENT(bytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && !IsStructuredExporting(m_State))
  {
    auto object=GetResourceManager()->GetResource(pipeline,true);
    if(m_DescriptorCoverage!=65 || !object || object->m_Type!=eResComputePipelineState || !object->m_Real)
      return false;
    bool found=false;
    for(const auto &r:m_IRComputeRoots[pipeline])
      found |= r.offset==offset && r.kind==kind && r.count==count && r.bytes==bytes;
    if(!found) return false;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_DeclareIRComputeRoot(ReadSerialiser &, ResourceId,
    uint64_t,uint64_t,uint64_t,uint64_t);
template bool WrappedMTLDevice::Serialise_DeclareIRComputeRoot(WriteSerialiser &, ResourceId,
    uint64_t,uint64_t,uint64_t,uint64_t);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareIRComputeHeapEntry(SerialiserType &ser, ResourceId pipeline,
    uint64_t heap, uint64_t index, uint64_t kind, uint64_t bytes)
{
  SERIALISE_ELEMENT(pipeline).Important();
  SERIALISE_ELEMENT(heap).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_ELEMENT(kind).Important();
  SERIALISE_ELEMENT(bytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && !IsStructuredExporting(m_State))
  {
    auto object=GetResourceManager()->GetResource(pipeline,true);
    if(m_DescriptorCoverage!=65 || !object || object->m_Type!=eResComputePipelineState || !object->m_Real)
      return false;
    bool found=false;
    for(const auto &e:m_IRComputeHeapEntries[pipeline])
      found |= e.heap==heap && e.index==index && e.kind==kind && e.bytes==bytes;
    if(!found) return false;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_DeclareIRComputeHeapEntry(ReadSerialiser &, ResourceId,
    uint64_t,uint64_t,uint64_t,uint64_t);
template bool WrappedMTLDevice::Serialise_DeclareIRComputeHeapEntry(WriteSerialiser &, ResourceId,
    uint64_t,uint64_t,uint64_t,uint64_t);

bool WrappedMTLDevice::HasRayQueryHeapPipeline(ResourceId pipeline) const
{
  for(const auto &d:m_RayQueryHeapDispatches) if(d.pipeline==pipeline) return true;
  return false;
}

bool WrappedMTLDevice::ValidateRayQueryHeapDispatch(ResourceId pipeline, ResourceId heap,
    uint64_t heapOffset, const rdcarray<byte> &roots, MTL::Size groups, MTL::Size threads, bool indirect,
    const std::function<bool(ResourceId,uint64_t,uint64_t)> &frameCBVProof)
{
  auto reject=[](uint32_t line) { RDCERR("Invalid typed Metal heap-query closure at line %u",line); return false; };
  m_RayIRReadResources.clear(); m_RayIROutput=ResourceId(); m_IRComputeWriteResources.clear();
  m_IRComputeFrameCBVResources.clear();
  const RayQueryHeapDispatch *query=NULL;
  for(const auto &d:m_RayQueryHeapDispatches) if(d.pipeline==pipeline && d.heap==heap) query=&d;
  auto cbv=m_RayQueryHeapCBVRoots.find(pipeline);
  const bool cbvRoots=cbv!=m_RayQueryHeapCBVRoots.end() && !cbv->second.empty();
  const auto declaredEntries=m_IRComputeHeapEntries.find(pipeline);
  bool currentOutput=false;
  if(cbvRoots && declaredEntries!=m_IRComputeHeapEntries.end())
    for(const auto &entry:declaredEntries->second)
      currentOutput |= entry.kind==8 && entry.index*24==cbv->second[0].outputSlot;
  const uint64_t workLimit=currentOutput?262144:4096;
  uint64_t work=groups.width*threads.width;
  if(cbvRoots)
  {
    const uint64_t grid[]={groups.width,groups.height,groups.depth};
    const uint64_t group[]={threads.width,threads.height,threads.depth};
    uint64_t gridWork=1,groupWork=1;
    for(unsigned i=0;i<3;i++)
    {
      if(grid[i]>workLimit || !group[i] || group[i]>1024 || groupWork>1024/group[i])
        return reject(__LINE__);
      groupWork*=group[i];
      if(!grid[i])gridWork=0;
      else
      {
        if(gridWork>workLimit/grid[i])return reject(__LINE__);
        gridWork*=grid[i];
      }
    }
    if(gridWork>workLimit/groupWork)return reject(__LINE__);
    work=gridWork*groupWork;
  }
  if(!query || m_DescriptorCoverage!=65 || heapOffset ||
     (cbvRoots ? cbv->second.size()!=cbv->second[0].rootCount || roots.size()!=cbv->second.size()*8 : roots.size()!=16) ||
     (!cbvRoots && ((!groups.width && !indirect) || groups.width>4096 || groups.height!=1 || groups.depth!=1 ||
       !threads.width || threads.width>1024 || threads.height!=1 || threads.depth!=1 ||
       groups.width>4096/threads.width))) return reject(__LINE__);
  const auto mixed=m_IRComputeRoots.find(pipeline);
  if(mixed!=m_IRComputeRoots.end() && (!cbvRoots || mixed->second.size()!=cbv->second.size()))
    return reject(__LINE__);
  uint64_t words[2]={}; if(!cbvRoots) memcpy(words,roots.data(),16);
  ResourceId outputId=query->output;
  if(currentOutput)
  {
    const auto slot=m_DescriptorSlotShadow.find(make_rdcpair(heap,cbv->second[0].outputSlot));
    if(slot==m_DescriptorSlotShadow.end() || !slot->second.live || slot->second.type!=5 ||
       slot->second.sources.size()!=1 || !slot->second.sources.count(1)) return reject(__LINE__);
    outputId=slot->second.sources.at(1).resource;
  }
  auto output=GetResourceManager()->GetResource(outputId,true);
  auto identity=m_ReplayGPUIdentities.find(outputId);
  if(!(currentOutput?IsRayQueryHeapTextureResource(output,true,128ULL*1024*1024):IsRayQueryHeapOutputResource(output)) || identity==m_ReplayGPUIdentities.end() ||
     IsReplayResourceAliasable(outputId)) return reject(__LINE__);
  const bool textureOutput=output->m_Type==eResTexture;
  MTL::Buffer *native=textureOutput?NULL:Unwrap((WrappedMTLBuffer *)output);
  if(textureOutput)
  {
    auto texture=Unwrap((WrappedMTLTexture *)output);
    if(!cbvRoots || identity->second.kind!=1 || !HasRayQueryHeapTextureInitialContents(outputId) ||
       work>texture->width()*texture->height()) return reject(__LINE__);
  }
  else
  {
    auto saved=m_ReplayBufferInitialContents.find(outputId);
    if(m_DescriptorPreflight && saved==m_ReplayBufferInitialContents.end() &&
       native->storageMode()==MTL::StorageModeShared && native->contents() &&
       m_ReplayBuffersWithCreationContents.count(outputId))
    {
      m_ReplayBufferInitialContents[outputId]=bytebuf((byte *)native->contents(),native->length());
      saved=m_ReplayBufferInitialContents.find(outputId);
    }
    if(identity->second.kind!=0 || work>native->length()/4 || saved==m_ReplayBufferInitialContents.end() ||
       saved->second.size()!=native->length() ||
       (!cbvRoots && (words[1]!=query->slotOffset/24 ||
         words[0]!=(m_DescriptorPreflight?identity->second.value:native->gpuAddress())))) return reject(__LINE__);
  }
  if(cbvRoots)
  {
    // DX12 root CBVs resolve a declared live resource and byte range. These
    // converted roots use the same captured identity map and current native VA.
    for(const auto &root:cbv->second)
    {
      const RayIRLocalRoot *typed=NULL;
      if(mixed!=m_IRComputeRoots.end())
      {
        for(const auto &definition:mixed->second) if(definition.offset==root.offset) typed=&definition;
        if(!typed || typed->bytes!=root.bytes) return reject(__LINE__);
      }
      uint64_t address=0; memcpy(&address,roots.data()+root.offset,8);
      ResourceId resolved; uint64_t at=0,length=0,options=0; MTL::Buffer *buffer=NULL;
      bool frameBuffer=false,placement=false;
      for(const auto &id:m_ReplayGPUIdentities)
      {
        if(id.second.kind!=0) continue;
        auto object=GetResourceManager()->GetResource(id.first,true);
        auto future=m_DescriptorFrameBuffers.find(id.first);
        const bool born=m_DescriptorPreflight && future!=m_DescriptorFrameBuffers.end();
        auto real=object && object->m_Type==eResBuffer && object->m_Real ? Unwrap((WrappedMTLBuffer *)object):NULL;
        if(born && !m_DescriptorPreflightLiveBuffers.count(id.first)) continue;
        if(!born && !real) continue;
        const uint64_t capacity=born?future->second.length:real->length();
        const uint64_t base=m_DescriptorPreflight?id.second.value:real->gpuAddress();
        if(!base || address<base || address-base>=capacity) continue;
        if(resolved!=ResourceId() || (object && object->m_CapturedAliasable) ||
           m_DescriptorPreflightAliasedBuffers.count(id.first)) return reject(__LINE__);
        resolved=id.first; at=address-base;buffer=real;length=capacity;
        frameBuffer=future!=m_DescriptorFrameBuffers.end();
        placement=born?future->second.heap!=ResourceId():real->heap()!=NULL;
        options=born?future->second.options:real->resourceOptions();
      }
      if(resolved==ResourceId() || at%16 || root.bytes>length-at || length>128ULL*1024*1024 ||
         resolved==heap || resolved==outputId || m_DescriptorGPUWrittenBuffers.count(resolved) ||
         (buffer && IsReplayResourceAliasable(resolved))) return reject(__LINE__);
      const bool constantBuffer=!typed || typed->kind==4;
      const bool shared=(options & 0xf0ULL)==MTL::ResourceStorageModeShared;
      const bool privateInput=constantBuffer && (options & 0xf0ULL)==MTL::ResourceStorageModePrivate &&
          (!placement || (options & 0x300ULL)==MTL::ResourceHazardTrackingModeTracked);
      if(!privateInput && (!shared || placement || (buffer && !buffer->contents()))) return reject(__LINE__);
      if(frameBuffer && constantBuffer)
      {
        const auto key=make_rdcpair(resolved,at);
        if(m_DescriptorPreflight)
        {
          if(!frameCBVProof || !frameCBVProof(resolved,at,root.bytes)) return reject(__LINE__);
          m_IRComputeFrameCBVReadRanges[key]=RDCMAX(m_IRComputeFrameCBVReadRanges[key],root.bytes);
        }
        else
        {
          auto proof=m_IRComputeFrameCBVReadRanges.find(key);
          if(proof==m_IRComputeFrameCBVReadRanges.end() || root.bytes>proof->second) return reject(__LINE__);
        }
        m_IRComputeFrameCBVResources.insert(resolved);m_RayIRReadResources.insert(resolved);
        continue;
      }
      auto raw=m_ReplayBufferInitialContents.find(resolved);
      if(raw==m_ReplayBufferInitialContents.end() && m_DescriptorPreflight && !privateInput &&
         m_ReplayBuffersWithCreationContents.count(resolved))
      {
        m_ReplayBufferInitialContents[resolved]=bytebuf((byte *)buffer->contents(),buffer->length());
        raw=m_ReplayBufferInitialContents.find(resolved);
      }
      if(raw==m_ReplayBufferInitialContents.end() || raw->second.size()!=buffer->length())
        return reject(__LINE__);
      if(typed && typed->kind==3)
      {
        for(uint64_t i=0;i<typed->count;i++)
        {
          const uint64_t entryAt=at+i*24; bool field=false;
          for(const auto &table:m_DescriptorTables)
            field |= table.buffer==resolved && table.schema==2 && entryAt>=table.offset &&
                (entryAt-table.offset)%table.stride==0 &&
                (entryAt-table.offset)/table.stride<table.count;
          uint64_t entry[3];memcpy(entry,raw->second.data()+entryAt,24);
          const auto slot=m_DescriptorSlotShadow.find(make_rdcpair(resolved,entryAt));bytebuf relocated;
          if(!field || !entry[0] || entry[1] || entry[2] ||
             slot==m_DescriptorSlotShadow.end() || !slot->second.live || slot->second.type!=7 ||
             slot->second.gpuExpected || slot->second.data.size()!=24 ||
             slot->second.sources.size()!=1 || !slot->second.sources.count(2) ||
             slot->second.sources.at(2).offset || memcmp(slot->second.data.data(),entry,24) ||
             !PatchDescriptorSlot(slot->second,relocated)) return reject(__LINE__);
          ResourceId sampler;uint64_t nativeID=0;
          for(const auto &id:m_ReplayGPUIdentities)
          {
            if(id.second.kind!=2 || id.second.value!=entry[0]) continue;
            auto object=GetResourceManager()->GetResource(id.first,true);
            if(!object || object->m_Type!=eResSamplerState || !object->m_Real || object->m_CapturedAliasable)
              return reject(__LINE__);
            const uint64_t value=Unwrap((WrappedMTLSamplerState *)object)->gpuResourceID()._impl;
            if(!value || (sampler!=ResourceId() && nativeID!=value)) return reject(__LINE__);
            sampler=id.first;nativeID=value;m_RayIRReadResources.insert(sampler);
          }
          if(sampler==ResourceId()) return reject(__LINE__);
          if(!m_DescriptorPreflight)
          {
            uint64_t actual[3];memcpy(actual,(byte *)buffer->contents()+entryAt,24);
            if(actual[0]!=nativeID || actual[1] || actual[2]) return reject(__LINE__);
          }
        }
      }
      // Root buffers carry opaque CBV data; nested GPU fields still need their
      // existing descriptor-table declarations and relocation closure.
      m_RayIRReadResources.insert(resolved);
    }
    auto out=m_DescriptorSlotShadow.find(make_rdcpair(heap,cbv->second[0].outputSlot));
    bytebuf relocated;
    const uint32_t outputKind=textureOutput?1:0;
    if(out==m_DescriptorSlotShadow.end() || !out->second.live || out->second.type!=(textureOutput?5U:1U) ||
       out->second.gpuExpected || out->second.sources.size()!=1 || !out->second.sources.count(outputKind) ||
       out->second.sources.at(outputKind).resource!=outputId || out->second.sources.at(outputKind).offset ||
       out->second.data.size()!=24 || !PatchDescriptorSlot(out->second,relocated)) return reject(__LINE__);
    uint64_t descriptor[3];memcpy(descriptor,out->second.data.data(),24);
    if(textureOutput ? (descriptor[0] || descriptor[1]!=identity->second.value || descriptor[2]) :
        (descriptor[0]!=identity->second.value || descriptor[1] || descriptor[2]!=native->length()))
      return reject(__LINE__);
  }
  bool textureWriteDeclared=false;
  const auto readEntries=m_IRComputeHeapEntries.find(pipeline);
  if(readEntries!=m_IRComputeHeapEntries.end()) for(const auto &entry:readEntries->second)
  {
    if(entry.kind==4 || entry.kind==8)
    {
      if(entry.kind==4)
      {
        if(!textureOutput || entry.index*24!=cbv->second[0].outputSlot) return reject(__LINE__);
        textureWriteDeclared=true;m_IRComputeWriteResources.insert(outputId);continue;
      }
      auto write=m_DescriptorSlotShadow.find(make_rdcpair(heap,entry.index*24));bytebuf patched;
      if(write==m_DescriptorSlotShadow.end() || !write->second.live || write->second.type!=5 || write->second.gpuExpected ||
         write->second.data.size()!=24 || write->second.sources.size()!=1 || !write->second.sources.count(1) ||
         write->second.sources.at(1).offset || !PatchDescriptorSlot(write->second,patched)) return reject(__LINE__);
      const ResourceId id=write->second.sources.at(1).resource;
      auto object=GetResourceManager()->GetResource(id,true);
      const auto writeIdentity=m_ReplayGPUIdentities.find(id);uint64_t writeWords[3];memcpy(writeWords,write->second.data.data(),24);
      if(!IsRayQueryHeapTextureResource(object,true,128ULL*1024*1024) || IsReplayResourceAliasable(id) ||
         writeIdentity==m_ReplayGPUIdentities.end() || writeIdentity->second.kind!=1 || writeWords[0] || writeWords[1]!=writeIdentity->second.value || writeWords[2] ||
         !HasRayQueryHeapTextureInitialContents(id) || !m_IRComputeWriteResources.insert(id).second)
        return reject(__LINE__);
      auto texture=Unwrap((WrappedMTLTexture *)object);
      if(work>texture->width()*texture->height()) return reject(__LINE__);
      if(entry.index*24==cbv->second[0].outputSlot) textureWriteDeclared=true;
      continue;
    }
    auto s=m_DescriptorSlotShadow.find(make_rdcpair(heap,entry.index*24));bytebuf patched;
    if(entry.kind!=1 || s==m_DescriptorSlotShadow.end() || !s->second.live || s->second.type!=4 || s->second.gpuExpected ||
       s->second.data.size()!=24 || s->second.sources.size()!=1 || !s->second.sources.count(1) ||
       s->second.sources.at(1).offset || !PatchDescriptorSlot(s->second,patched)) return reject(__LINE__);
    const ResourceId texture=s->second.sources.at(1).resource;
    auto object=GetResourceManager()->GetResource(texture,true);
    auto id=m_ReplayGPUIdentities.find(texture);
    uint64_t textureWords[3];memcpy(textureWords,s->second.data.data(),24);
    if(texture==outputId || !object || object->m_Type!=eResTexture || !object->m_Real || object->m_CapturedAliasable ||
       id==m_ReplayGPUIdentities.end() || id->second.kind!=1 || textureWords[0] || textureWords[1]!=id->second.value || textureWords[2] ||
       !HasRayQueryHeapTextureInitialContents(texture)) return reject(__LINE__);
    if(!IsRayQueryHeapTextureResource(object,false) || IsReplayResourceAliasable(texture)) return reject(__LINE__);
    m_RayIRReadResources.insert(texture);
  }
  if(textureOutput && !textureWriteDeclared) return reject(__LINE__);
  auto slot=m_DescriptorSlotShadow.find(make_rdcpair(heap,query->slotOffset));
  if(slot==m_DescriptorSlotShadow.end() || !slot->second.live || slot->second.type!=4 ||
     slot->second.data.size()!=24 || slot->second.sources.size()!=1 || !slot->second.sources.count(3) ||
     slot->second.gpuExpected) return reject(__LINE__);
  const auto &source=slot->second.sources[3];
  auto h=m_RayASHeaderCurrent.find(make_rdcpair(source.resource,source.offset));
  bytebuf patched;
  if(h==m_RayASHeaderCurrent.end() || !ValidateRayASHeader(h->second) ||
     !PatchDescriptorSlot(slot->second,patched) || source.resource==outputId ||
     h->second.contributions==outputId || source.resource==heap || h->second.contributions==heap)
    return reject(__LINE__);
  uint64_t header[8]; memcpy(header,h->second.bytes.data(),64);
  ResourceId structure; uint64_t count=0;
  if(!ValidateRayQueryStructure(header[0],structure,count) || structure!=h->second.structure) return reject(__LINE__);
  auto contribution=GetResourceManager()->GetResource(h->second.contributions,true);
  auto contents=m_ReplayBufferInitialContents.find(h->second.contributions);
  auto real=Unwrap((WrappedMTLBuffer *)contribution);
  if(m_DescriptorPreflight && contents==m_ReplayBufferInitialContents.end() &&
     real->storageMode()==MTL::StorageModeShared && m_ReplayBuffersWithCreationContents.count(h->second.contributions))
  {
    m_ReplayBufferInitialContents[h->second.contributions]=bytebuf((byte *)real->contents(),real->length());
    contents=m_ReplayBufferInitialContents.find(h->second.contributions);
  }
  if(contents==m_ReplayBufferInitialContents.end() || contents->second.size()!=real->length() ||
     h->second.contributionOffset>real->length() || RDCMAX(count,1ULL)*4>real->length()-h->second.contributionOffset ||
     m_DescriptorGPUWrittenBuffers.count(h->second.contributions)) return reject(__LINE__);
  for(const auto &table:m_DescriptorTables) if(table.buffer==outputId) return reject(__LINE__);
  for(const auto &recipe:m_RayIRASCurrentContents)
    if(recipe.second && (recipe.second->source==outputId || recipe.second->indexSource==outputId)) return reject(__LINE__);
  if(m_IRComputeWriteResources.empty())m_IRComputeWriteResources.insert(outputId);
  // Texel views alias a buffer allocation, as DX12/Vulkan buffer views do.
  // A shader write through a view invalidates later scalar proofs of its parent.
  std::set<ResourceId> readParents,writeParents;
  for(ResourceId id:m_RayIRReadResources)
  {const ResourceId parent=m_Replay->GetBufferTextureSource(id);if(parent!=ResourceId())readParents.insert(parent);}
  for(ResourceId id:m_IRComputeWriteResources)
  {const ResourceId parent=m_Replay->GetBufferTextureSource(id);if(parent!=ResourceId())writeParents.insert(parent);}
  m_RayIRReadResources.insert(readParents.begin(),readParents.end());
  m_IRComputeWriteResources.insert(writeParents.begin(),writeParents.end());
  for(ResourceId written:m_IRComputeWriteResources)
  {
    if(m_RayIRReadResources.count(written) || written==heap || written==h->second.buffer ||
       written==h->second.contributions) return reject(__LINE__);
    for(const auto &table:m_DescriptorTables) if(table.buffer==written) return reject(__LINE__);
    for(const auto &recipe:m_RayIRASCurrentContents)
      if(recipe.second && (recipe.second->source==written || recipe.second->indexSource==written)) return reject(__LINE__);
  }
  m_RayIRReadResources.insert(heap); m_RayIRReadResources.insert(h->second.buffer);
  m_RayIRReadResources.insert(h->second.contributions); m_RayIROutput=outputId;
  return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareRayQueryDispatch(SerialiserType &ser, ResourceId pipeline,
    ResourceId roots, uint64_t offset, ResourceId header, ResourceId output)
{
  SERIALISE_ELEMENT(pipeline).Important();
  SERIALISE_ELEMENT(roots).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(header).Important();
  SERIALISE_ELEMENT(output).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    auto p = GetResourceManager()->GetResource(pipeline,true);
    if(m_DescriptorCoverage != 3 || !p || p->m_Type != eResComputePipelineState || !p->m_Real ||
       HasRayIRPipeline(pipeline) || roots == header || roots == output || header == output || offset % 8) return false;
    bool typedHeader=false;
    for(const auto &h:m_RayASHeaders) typedHeader |= h.second.buffer==header;
    for(auto id : {roots,header,output})
    {
      auto b = GetResourceManager()->GetResource(id,true);
      if(!b || b->m_Type != eResBuffer || !b->m_Real || b->m_CapturedAliasable) return false;
      auto real = Unwrap((WrappedMTLBuffer *)b);
      if(real->storageMode() != MTL::StorageModeShared ||
         (real->heap() && (id!=header || !typedHeader)) || !real->length() || real->length() > 16*1024 ||
         (id == header && real->length() < 64) || (id == output && real->length()%4) ||
         (id == roots && (offset > real->length() || 16 > real->length()-offset))) return false;
    }
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_DeclareRayQueryDispatch(ReadSerialiser &, ResourceId,
    ResourceId, uint64_t, ResourceId, ResourceId);
template bool WrappedMTLDevice::Serialise_DeclareRayQueryDispatch(WriteSerialiser &, ResourceId,
    ResourceId, uint64_t, ResourceId, ResourceId);

bool WrappedMTLDevice::HasRayQueryPipeline(ResourceId pipeline) const
{
  for(const auto &query : m_RayQueryDispatches) if(query.pipeline == pipeline) return true;
  return false;
}

bool WrappedMTLDevice::ValidateRayQueryDispatch(ResourceId pipeline, ResourceId roots, uint64_t offset,
    MTL::Size groups, MTL::Size threads)
{
  auto reject = [](uint32_t line) {
    RDCERR("Invalid Metal query ABI closure at validation line %u",line);
    fprintf(stderr,"Invalid Metal query ABI closure at validation line %u\n",line); return false;
  };
  m_RayIRReadResources.clear(); m_RayIROutput = ResourceId(); m_IRComputeWriteResources.clear();
  const RayQueryDispatch *query = NULL;
  for(const auto &d : m_RayQueryDispatches)
    if(d.pipeline == pipeline && d.roots == roots && d.offset == offset) query = &d;
  if(!query || m_DescriptorCoverage != 3 || HasRayIRPipeline(pipeline) ||
     !groups.width || groups.width > 4096 || groups.height != 1 || groups.depth != 1 ||
     !threads.width || threads.width > 1024 || threads.height != 1 || threads.depth != 1 ||
     groups.width > 4096/threads.width) return reject(__LINE__);
  // As with DX12/Vulkan AS descriptors, require an explicit namespace and
  // immutable roots. Inline queries have no shader records or function tables.
  auto initial = [&](ResourceId id, uint64_t at, uint64_t bytes, const bytebuf *&data) {
    auto object = GetResourceManager()->GetResource(id,true);
    auto saved = m_ReplayBufferInitialContents.find(id);
    if(!object || object->m_Type != eResBuffer || !object->m_Real || object->m_CapturedAliasable ||
       m_DescriptorGPUWrittenBuffers.count(id)) return false;
    auto real = Unwrap((WrappedMTLBuffer *)object);
    if(real->storageMode() != MTL::StorageModeShared ||
       (real->heap() && !m_RayASHeaderCurrent.count(make_rdcpair(id,at))) || !real->contents() ||
       !real->length() || real->length() > 16*1024 || at > real->length() || bytes > real->length()-at) return false;
    // Untouched creation bytes are authoritative, as on the existing converted
    // TraceRay path. Cache them only during preflight, before any dispatch.
    if(saved == m_ReplayBufferInitialContents.end() && m_DescriptorPreflight && m_ReplayBuffersWithCreationContents.count(id))
    {
      m_ReplayBufferInitialContents[id]=bytebuf((byte *)real->contents(),real->length());
      m_ReplayCPUUpdatedBuffers.insert(id); saved=m_ReplayBufferInitialContents.find(id);
    }
    if(saved == m_ReplayBufferInitialContents.end() || saved->second.size()!=real->length()) return false;
    auto current=m_DescriptorRawContents.find(id);
    data = current!=m_DescriptorRawContents.end() && m_RayASHeaders.count(make_rdcpair(id,at)) ?
        &current->second : &saved->second;
    return true;
  };
  auto field = [&](ResourceId id, uint64_t at, uint32_t schema) {
    for(const auto &layout : m_DescriptorTables)
      if(layout.buffer == id && layout.schema == schema && at >= layout.offset &&
         (at-layout.offset)%layout.stride == 0 && (at-layout.offset)/layout.stride < layout.count) return true;
    return false;
  };
  auto address = [&](ResourceId id, uint64_t captured, uint64_t at=0) {
    auto identity = m_ReplayGPUIdentities.find(id);
    if(identity == m_ReplayGPUIdentities.end() || identity->second.kind != 0 || captured < identity->second.value || captured-identity->second.value != at || !captured)
      return false;
    uint32_t matches=0;
    for(const auto &candidate : m_ReplayGPUIdentities)
    {
      if(candidate.second.kind != 0 || captured < candidate.second.value) continue;
      auto object = GetResourceManager()->GetResource(candidate.first,true);
      if(object && object->m_Type == eResBuffer && object->m_Real &&
         captured-candidate.second.value < Unwrap((WrappedMTLBuffer *)object)->length()) matches++;
    }
    return matches == 1;
  };
  const bytebuf *rootBytes=NULL,*headerBytes=NULL,*outputBytes=NULL;
  const uint64_t work=groups.width*threads.width;
  if(!initial(roots,offset,16,rootBytes) || !initial(query->output,0,work*4,outputBytes) ||
     !field(roots,offset,0) || !field(roots,offset+8,0)) return reject(__LINE__);
  uint64_t rootValues[2],headerValues[8];
  memcpy(rootValues,rootBytes->data()+offset,16);
  auto headerIdentity=m_ReplayGPUIdentities.find(query->header);
  if(headerIdentity==m_ReplayGPUIdentities.end() || headerIdentity->second.kind!=0 ||
     rootValues[0]<headerIdentity->second.value) return reject(__LINE__);
  const uint64_t headerOffset=rootValues[0]-headerIdentity->second.value;
  // Resolve a typed range in the declared buffer, as buffer+offset AS SRVs on
  // DX12. Only this 64-byte header is interpreted; surrounding bytes are data.
  if(headerOffset%8 || !address(query->header,rootValues[0],headerOffset) ||
     !address(query->output,rootValues[1]) || !initial(query->header,headerOffset,64,headerBytes) ||
     !field(query->header,headerOffset,4) || !field(query->header,headerOffset+8,0)) return reject(__LINE__);
  memcpy(headerValues,headerBytes->data()+headerOffset,64);
  for(uint32_t i=2;i<8;i++) if(headerValues[i]) return reject(__LINE__);
  ResourceId as; uint64_t count=0;
  if(!ValidateRayQueryStructure(headerValues[0],as,count)) return reject(__LINE__);
  // The contribution pointer is part of the public header even though query AIR
  // reads only word0. Validate a declared live backing without interpreting SBT data.
  const auto declaredHeader=m_RayASHeaderCurrent.find(make_rdcpair(query->header,headerOffset));
  if(declaredHeader != m_RayASHeaderCurrent.end())
  {
    const auto &h=declaredHeader->second;
    if(h.structure != as || h.bytes.size()!=64 || memcmp(h.bytes.data(),headerValues,64) ||
       h.contributions==query->output || h.contributions==roots || h.contributions==query->header)
      return reject(__LINE__);
    auto object=GetResourceManager()->GetResource(h.contributions,true);
    auto identity=m_ReplayGPUIdentities.find(h.contributions);
    auto contents=m_ReplayBufferInitialContents.find(h.contributions);
    if(!object || object->m_Type!=eResBuffer || !object->m_Real || object->m_CapturedAliasable ||
       identity==m_ReplayGPUIdentities.end() || identity->second.kind!=0 ||
       identity->second.value>UINT64_MAX-h.contributionOffset ||
       headerValues[1]!=identity->second.value+h.contributionOffset ||
       m_DescriptorGPUWrittenBuffers.count(h.contributions)) return reject(__LINE__);
    auto native=Unwrap((WrappedMTLBuffer *)object);
    if(native->length()>16*1024 ||
       (native->storageMode()!=MTL::StorageModeShared && native->storageMode()!=MTL::StorageModePrivate)) return reject(__LINE__);
    if(native->storageMode()==MTL::StorageModeShared && contents==m_ReplayBufferInitialContents.end() &&
       m_DescriptorPreflight && m_ReplayBuffersWithCreationContents.count(h.contributions))
    {
      m_ReplayBufferInitialContents[h.contributions]=bytebuf((byte *)native->contents(),native->length());
      m_ReplayCPUUpdatedBuffers.insert(h.contributions);
      contents=m_ReplayBufferInitialContents.find(h.contributions);
    }
    const uint64_t required=RDCMAX(count,1ULL)*4;
    if(contents==m_ReplayBufferInitialContents.end() || contents->second.size()!=native->length() ||
       h.contributionOffset>native->length() || required>native->length()-h.contributionOffset)
      return reject(__LINE__);
    m_RayIRReadResources.insert(h.contributions);
  }
  else if(headerValues[1])
  {
    ResourceId contribution; uint32_t matches=0;
    for(const auto &identity : m_ReplayGPUIdentities)
      if(identity.second.kind==0 && headerValues[1]>=identity.second.value)
      {
        const uint64_t at=headerValues[1]-identity.second.value; const bytebuf *bytes=NULL;
        if(at%4==0 && initial(identity.first,at,RDCMAX(count,1ULL)*4,bytes))
        { contribution=identity.first; matches++; }
      }
    if(matches!=1 || contribution==query->output || contribution==roots || contribution==query->header) return reject(__LINE__);
    m_RayIRReadResources.insert(contribution);
  }
  for(const auto &entry : m_RayIRASCurrentContents)
    if(entry.second && (entry.second->source==query->output || entry.second->indexSource==query->output)) return reject(__LINE__);
  for(const auto &table : m_DescriptorTables) if(table.buffer==query->output) return reject(__LINE__);
  m_RayIRReadResources.insert(roots);m_RayIRReadResources.insert(query->header);m_RayIRReadResources.insert(as);
  m_RayIROutput=query->output;
  return true;
}

bool WrappedMTLDevice::ValidateRayQueryStructure(uint64_t captured, ResourceId &as, uint64_t &count)
{
  auto reject=[](uint32_t line) { RDCERR("Invalid Metal query AS dependency at line %u",line); return false; };
  uint64_t replacement=0;
  if(!captured || !ResolveRayIRIdentity(4,captured,as,replacement)) return reject(__LINE__);
  // A frame build may initialise a new TLAS. The sequential preflight records
  // its validated frozen recipe only at the build, as for TraceRay; an identity
  // or allocation alone never proves that the query target has been built.
  auto recipe=m_RayIRASCurrentContents.find(as);
  if(recipe==m_RayIRASCurrentContents.end() || !recipe->second ||
     recipe->second->parameters.size()!=8) return reject(__LINE__);
  const auto &build = *recipe->second;
  const uint32_t kind = build.kind;
  count = build.parameters[3];
  if((kind != 5 && kind != 9 && kind != 10 && kind != 11) || count > 64 ||
     (!count && kind != 10)) return reject(__LINE__);
  // Reuse the typed indirect-instance recipe and UserID conversion used by
  // TraceRay and initial-state reconstruction. No SBT/IFT is read by a query.
  if(kind == 9)
  {
    bytebuf direct;
    uint64_t packedBytes = 0;
    if(build.children.size() != build.childGPUIdentities.size() ||
       !MetalASIndirectInstanceSpan(build.parameters,64*1024*1024,packedBytes) ||
       !ConvertMetalASIndirectInstances(build.vertices,count,build.childGPUIdentities,direct))
      return reject(__LINE__);
  }
  else if(kind == 10 || kind == 11)
  {
    uint64_t packedBytes = 0;
    if(!build.children.empty() || !build.childGPUIdentities.empty() ||
       (kind == 10 ? !ValidMetalASEmptyIndirectParameters(build.parameters,64*1024*1024) ||
                     !build.vertices.empty() :
                     !MetalASIndirectInstanceSpan(build.parameters,64*1024*1024,packedBytes) ||
                     !ValidMetalASInactiveInstances(build.vertices,count)))
      return reject(__LINE__);
  }
  auto structure=(WrappedMTLAccelerationStructure *)GetResourceManager()->GetResource(as);
  if(!m_DescriptorPreflight && structure->m_LastBuildKind!=5) return reject(__LINE__);
  for(auto child : recipe->second->children)
  {
    auto primitive=m_RayIRASCurrentContents.find(child);
    if(primitive==m_RayIRASCurrentContents.end() || !primitive->second ||
       (primitive->second->kind!=1 && primitive->second->kind!=2 && primitive->second->kind!=8)) return reject(__LINE__);
    m_RayIRReadResources.insert(child);
  }
  m_RayIRReadResources.insert(as);
  return true;
}

// Unlike a native MTLArgumentEncoder packet, converted DXR has a public runtime ABI.
// Follow DX12 PatchRayDispatch's explicit record/root closure, and Vulkan's typed
// SBT regions. Never infer the namespace from an integer (AS and IFT IDs can collide).
template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareRayIRDispatch(SerialiserType &ser, ResourceId pipeline,
    ResourceId buffer, uint64_t offset, uint64_t rootCount)
{
  SERIALISE_ELEMENT(pipeline).Important();
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(rootCount).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    auto p = GetResourceManager()->GetResource(pipeline, true);
    auto b = GetResourceManager()->GetResource(buffer, true);
    if(m_DescriptorCoverage != 3 || !p || p->m_Type != eResComputePipelineState || !p->m_Real ||
       !b || b->m_Type != eResBuffer || !b->m_Real || offset % 8 || rootCount < 2 || rootCount > 256) return false;
    auto real = Unwrap((WrappedMTLBuffer *)b);
    if(real->storageMode() != MTL::StorageModeShared || offset > real->length() ||
       152 > real->length() - offset) return false;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_DeclareRayIRDispatch(ReadSerialiser &, ResourceId,
    ResourceId, uint64_t, uint64_t);
template bool WrappedMTLDevice::Serialise_DeclareRayIRDispatch(WriteSerialiser &, ResourceId,
    ResourceId, uint64_t, uint64_t);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareRayIRShaderRole(SerialiserType &ser, ResourceId function,
                                                       uint32_t role)
{
  SERIALISE_ELEMENT(function).Important();
  SERIALISE_ELEMENT(role).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    auto object = GetResourceManager()->GetResource(function, true);
    if(m_DescriptorCoverage != 3 || role > 3 || !object || object->m_Type != eResFunctionHandle ||
       !object->m_Real) return false;
    auto handle = (WrappedMTLFunctionHandle *)object;
    auto declared = m_RayIRShaderRoles.find(function);
    if(!handle->m_Pipeline || handle->m_Pipeline->m_Type != eResComputePipelineState ||
       !HasRayIRPipeline(GetResID(handle->m_Pipeline)) || handle->m_Stage != 0 ||
       !handle->m_Function || !handle->m_Function->m_Real ||
       Unwrap(handle->m_Function)->functionType() != MTL::FunctionTypeVisible ||
       declared == m_RayIRShaderRoles.end() || declared->second != role) return false;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_DeclareRayIRShaderRole(ReadSerialiser &, ResourceId, uint32_t);
template bool WrappedMTLDevice::Serialise_DeclareRayIRShaderRole(WriteSerialiser &, ResourceId, uint32_t);

bool WrappedMTLDevice::AddRayIRLocalRoot(ResourceId function, const RayIRLocalRoot &root,
                                         bool idempotent)
{
  if(function == ResourceId() || !m_RayIRShaderRoles.count(function) || root.kind > 4 ||
     !root.count || root.count > 64 || !root.bytes || root.bytes > 64*1024 ||
     (root.kind == 1 && root.bytes != root.count*4) ||
     ((root.kind == 2 || root.kind == 3) && root.bytes != root.count*24) ||
     ((root.kind == 0 || root.kind == 4) && root.count != 1) ||
     (root.kind == 3 ? root.offset != 16 : root.offset < 32) ||
     root.offset % (root.kind == 1 ? 4 : 8)) return false;
  const uint64_t span = root.kind == 1 ? root.bytes : 8;
  if(root.offset > 4096 || span > 4096-root.offset) return false;
  auto &roots = m_RayIRLocalRoots[function];
  for(const auto &old : roots)
  {
    if(old.offset == root.offset && old.kind == root.kind && old.count == root.count &&
       old.bytes == root.bytes) return idempotent;
    const uint64_t oldSpan = old.kind == 1 ? old.bytes : 8;
    if(root.offset < old.offset+oldSpan && old.offset < root.offset+span) return false;
  }
  if(roots.size() >= 16) return false;
  roots.push_back(root);
  return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareRayIRLocalRoot(SerialiserType &ser, ResourceId function,
    uint64_t offset, uint64_t kind, uint64_t count, uint64_t bytes)
{
  SERIALISE_ELEMENT(function).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(kind).Important();
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_ELEMENT(bytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    auto object = GetResourceManager()->GetResource(function,true);
    if(m_DescriptorCoverage != 3 || !object || object->m_Type != eResFunctionHandle ||
       !object->m_Real) return false;
    auto handle = (WrappedMTLFunctionHandle *)object;
    if(!handle->m_Pipeline || !HasRayIRPipeline(GetResID(handle->m_Pipeline)) ||
       !handle->m_Function || !handle->m_Function->m_Real ||
       Unwrap(handle->m_Function)->functionType() != MTL::FunctionTypeVisible || handle->m_Stage != 0)
      return false;
    bool found = false;
    for(const auto &root : m_RayIRLocalRoots[function])
      found |= root.offset == offset && root.kind == kind && root.count == count && root.bytes == bytes;
    if(!found) return false;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_DeclareRayIRLocalRoot(ReadSerialiser &, ResourceId,
    uint64_t,uint64_t,uint64_t,uint64_t);
template bool WrappedMTLDevice::Serialise_DeclareRayIRLocalRoot(WriteSerialiser &, ResourceId,
    uint64_t,uint64_t,uint64_t,uint64_t);

bool WrappedMTLDevice::AddRayIRGlobalRoot(ResourceId pipeline, const RayIRLocalRoot &root,
                                          bool idempotent)
{
  uint64_t words=0;
  for(const auto &dispatch:m_RayIRDispatches)
    if(dispatch.pipeline==pipeline)
    {
      if(words && words!=dispatch.rootCount) return false;
      words=dispatch.rootCount;
    }
  if(!words || words>256 || root.kind>6 || !root.count || root.count>64 ||
     !root.bytes || root.bytes>64*1024 ||
     (root.kind==1 && root.bytes!=root.count*4) ||
     ((root.kind==2 || root.kind==3) && root.bytes!=root.count*24) ||
     ((root.kind==0 || root.kind>=4) && root.count!=1) ||
     (root.kind==5 && root.bytes!=64) || (root.kind==6 && root.bytes%4) ||
     root.offset%(root.kind==1?4:8)) return false;
  const uint64_t span=root.kind==1?root.bytes:8;
  if(root.offset>words*8 || span>words*8-root.offset) return false;
  auto &roots=m_RayIRGlobalRoots[pipeline];
  for(const auto &old:roots)
  {
    if(old.offset==root.offset && old.kind==root.kind && old.count==root.count && old.bytes==root.bytes)
      return idempotent;
    const uint64_t oldSpan=old.kind==1?old.bytes:8;
    if(root.offset<old.offset+oldSpan && old.offset<root.offset+span) return false;
    if((root.kind==5 || root.kind==6) && old.kind==root.kind) return false;
  }
  if(roots.size()>=64) return false;
  roots.push_back(root);return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareRayIRGlobalRoot(SerialiserType &ser,ResourceId pipeline,
    uint64_t offset,uint64_t kind,uint64_t count,uint64_t bytes)
{
  SERIALISE_ELEMENT(pipeline).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(kind).Important();
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_ELEMENT(bytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    auto object=GetResourceManager()->GetResource(pipeline,true);
    if(m_DescriptorCoverage!=3 || !object || object->m_Type!=eResComputePipelineState || !object->m_Real)
      return false;
    bool declared=false;
    for(const auto &root:m_RayIRGlobalRoots[pipeline])
      declared |= root.offset==offset && root.kind==kind && root.count==count && root.bytes==bytes;
    if(!declared) return false;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_DeclareRayIRGlobalRoot(ReadSerialiser &,ResourceId,
    uint64_t,uint64_t,uint64_t,uint64_t);
template bool WrappedMTLDevice::Serialise_DeclareRayIRGlobalRoot(WriteSerialiser &,ResourceId,
    uint64_t,uint64_t,uint64_t,uint64_t);

bool WrappedMTLDevice::AddRayIRHeapEntry(ResourceId pipeline, const RayIRHeapEntry &entry,
                                         bool idempotent)
{
  if(!HasRayIRPipeline(pipeline) || entry.heap>1 || entry.index>=2730 || entry.kind>3 ||
     (entry.heap==1 ? entry.kind!=2 : entry.kind==2) ||
     (entry.kind==0 ? (!entry.bytes || entry.bytes>64*1024) : entry.kind==3 ? entry.bytes!=64 : entry.bytes!=0)) return false;
  auto &entries=m_RayIRHeapEntries[pipeline];
  for(const auto &old:entries)
    if(old.heap==entry.heap && old.index==entry.index)
      return idempotent && old.kind==entry.kind && old.bytes==entry.bytes;
  if(entries.size()>=1024) return false;
  entries.push_back(entry);return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareRayIRHeapEntry(SerialiserType &ser,ResourceId pipeline,
    uint64_t heap,uint64_t index,uint64_t kind,uint64_t bytes)
{
  SERIALISE_ELEMENT(pipeline).Important();
  SERIALISE_ELEMENT(heap).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_ELEMENT(kind).Important();
  SERIALISE_ELEMENT(bytes).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    auto object=GetResourceManager()->GetResource(pipeline,true);
    if(m_DescriptorCoverage!=3 || !object || object->m_Type!=eResComputePipelineState || !object->m_Real)
      return false;
    bool declared=false;
    for(const auto &entry:m_RayIRHeapEntries[pipeline])
      declared |= entry.heap==heap && entry.index==index && entry.kind==kind && entry.bytes==bytes;
    if(!declared) return false;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_DeclareRayIRHeapEntry(ReadSerialiser &,ResourceId,
    uint64_t,uint64_t,uint64_t,uint64_t);
template bool WrappedMTLDevice::Serialise_DeclareRayIRHeapEntry(WriteSerialiser &,ResourceId,
    uint64_t,uint64_t,uint64_t,uint64_t);

bool WrappedMTLDevice::HasRayIRPipeline(ResourceId pipeline) const
{
  for(const auto &d : m_RayIRDispatches) if(d.pipeline == pipeline) return true;
  return false;
}

bool WrappedMTLDevice::ResolveRayIRIdentity(uint32_t schema, uint64_t captured,
    ResourceId &resource, uint64_t &replacement)
{
  resource = ResourceId(); replacement = 0;
  if(schema < 4 || schema > 6) return false;
  if(!captured) return true;
  const auto candidates = schema == 4 ? GetResourceManager()->GetAccelerationStructures() :
      GetResourceManager()->GetFunctionTables(schema == 5 ? eResIntersectionFunctionTable : eResVisibleFunctionTable);
  for(auto object : candidates)
  {
    uint64_t identity = schema == 4 ? ((WrappedMTLAccelerationStructure *)object)->m_CapturedGPUResourceID :
        schema == 5 ? ((WrappedMTLIntersectionFunctionTable *)object)->m_CapturedGPUResourceID :
                      ((WrappedMTLVisibleFunctionTable *)object)->m_CapturedGPUResourceID;
    if(identity != captured) continue;
    if(resource != ResourceId() || !object->m_Real || object->m_CapturedAliasable) return false;
    resource = GetResID(object);
    replacement = schema == 4 ? Unwrap((WrappedMTLAccelerationStructure *)object)->gpuResourceID()._impl :
        schema == 5 ? Unwrap((WrappedMTLIntersectionFunctionTable *)object)->gpuResourceID()._impl :
                      Unwrap((WrappedMTLVisibleFunctionTable *)object)->gpuResourceID()._impl;
  }
  return resource != ResourceId() && replacement != 0;
}

bool WrappedMTLDevice::ValidateRayIRDispatch(ResourceId pipeline, ResourceId buffer, uint64_t offset,
    MTL::Size groups, MTL::Size threads)
{
  auto reject = [](uint32_t line) {
    RDCERR("Invalid Metal IR ray ABI closure at validation line %u", line);
    fprintf(stderr, "Invalid Metal IR ray ABI closure at validation line %u\n", line);
    return false;
  };
  m_RayIRReadResources.clear(); m_RayIROutput = ResourceId(); m_IRComputeWriteResources.clear();
  uint64_t rootWords=0;
  for(const auto &d:m_RayIRDispatches)
    if(d.pipeline==pipeline && d.buffer==buffer && d.offset==offset) rootWords=d.rootCount;
  if(rootWords<2 || rootWords>256 || m_DescriptorCoverage != 3) return reject(__LINE__);
  // Explicit PSO roots and heap entries describe the converted DXR ABI. Local
  // roots are tied to immutable shader handles as in DX12 export associations.
  // Callable records and GPU/frame authored ABI data are not admitted yet.
  struct Source { ResourceId id; uint64_t offset = 0; const bytebuf *raw = NULL; };
  auto sourceBuffer = [&](ResourceId id, uint64_t start, uint64_t bytes, Source &s) {
    auto object = GetResourceManager()->GetResource(id, true);
    if(!object || object->m_Type != eResBuffer || !object->m_Real || object->m_CapturedAliasable) return reject(__LINE__);
    auto real = Unwrap((WrappedMTLBuffer *)object);
    if(real->storageMode() != MTL::StorageModeShared || real->heap() || !real->contents() ||
       real->length() > 64 * 1024 || start > real->length() || bytes > real->length() - start) return reject(__LINE__);
    auto initial = m_ReplayBufferInitialContents.find(id);
    if(initial == m_ReplayBufferInitialContents.end() && m_DescriptorPreflight &&
       m_ReplayBuffersWithCreationContents.count(id))
    {
      m_ReplayBufferInitialContents[id] = bytebuf((byte *)real->contents(), real->length());
      initial = m_ReplayBufferInitialContents.find(id);
      m_ReplayCPUUpdatedBuffers.insert(id);
    }
    if(initial == m_ReplayBufferInitialContents.end() || initial->second.size() != real->length()) return reject(__LINE__);
    s.id = id; s.offset = start; s.raw = &initial->second;
    m_RayIRReadResources.insert(id);
    return true;
  };
  auto address = [&](uint64_t value, uint64_t bytes, Source &s) {
    uint32_t matches = 0;
    for(const auto &identity : m_ReplayGPUIdentities)
    {
      if(identity.second.kind != 0 || value < identity.second.value) continue;
      auto object = GetResourceManager()->GetResource(identity.first, true);
      if(!object || object->m_Type != eResBuffer || !object->m_Real) continue;
      const uint64_t at = value - identity.second.value;
      if(at >= Unwrap((WrappedMTLBuffer *)object)->length()) continue;
      if(++matches != 1 || !sourceBuffer(identity.first, at, bytes, s)) return reject(__LINE__);
    }
    return value && matches == 1;
  };
  auto word = [](const Source &s, uint64_t at) {
    uint64_t value; memcpy(&value, s.raw->data() + s.offset + at, 8); return value;
  };
  auto field = [&](const Source &s, uint64_t at, uint32_t schema) {
    for(const auto &layout : m_DescriptorTables)
      if(layout.buffer == s.id && layout.schema == schema && s.offset + at >= layout.offset &&
         (s.offset + at - layout.offset) % layout.stride == 0 &&
         (s.offset + at - layout.offset) / layout.stride < layout.count) return true;
    return reject(__LINE__);
  };
  Source packet;
  if(!sourceBuffer(buffer, offset, 152, packet)) return reject(__LINE__);
  for(uint64_t at : {0ULL,16ULL,40ULL,64ULL,104ULL,112ULL,120ULL,144ULL})
    if(!field(packet, at, 0)) return reject(__LINE__);
  if(!field(packet,128,6) || !field(packet,136,5) || word(packet,144)) return reject(__LINE__);
  uint32_t dimensions[4]; memcpy(dimensions, packet.raw->data() + packet.offset + 88, 16);
  uint64_t work = 1;
  const uint64_t grid[] = {groups.width,groups.height,groups.depth};
  const uint64_t group[] = {threads.width,threads.height,threads.depth};
  uint64_t groupWork = 1;
  for(uint32_t i = 0; i < 3; i++)
  {
    if(!dimensions[i] || dimensions[i] > 4096 || work > 4096 / dimensions[i] ||
       !group[i] || group[i] > 1024 || groupWork > 1024 / group[i] ||
       grid[i] != (dimensions[i] + group[i] - 1) / group[i]) return reject(__LINE__);
    work *= dimensions[i]; groupWork *= group[i];
  }
  if(dimensions[3]) return reject(__LINE__);
  ResourceId vid, iid; uint64_t live;
  if(!word(packet,128) || !word(packet,136) ||
     !ResolveRayIRIdentity(6,word(packet,128),vid,live) ||
     !ResolveRayIRIdentity(5,word(packet,136),iid,live)) return reject(__LINE__);
  auto visible = (WrappedMTLVisibleFunctionTable *)GetResourceManager()->GetResource(vid);
  auto intersection = (WrappedMTLIntersectionFunctionTable *)GetResourceManager()->GetResource(iid);
  if(GetResID(visible->m_Pipeline) != pipeline || GetResID(intersection->m_Pipeline) != pipeline ||
     visible->m_Stage != 0 || intersection->m_Stage != 0 ||
     !intersection->m_FunctionCount || !intersection->m_RayIRFunctions.count(0) ||
     intersection->m_RayIRFunctions[0] == ResourceId()) return reject(__LINE__);
  m_RayIRReadResources.insert(vid); m_RayIRReadResources.insert(iid);
  bool typedRoles = false;
  for(const auto &role : m_RayIRShaderRoles)
  {
    auto object = GetResourceManager()->GetResource(role.first, true);
    if(object && object->m_Type == eResFunctionHandle &&
       GetResID(((WrappedMTLFunctionHandle *)object)->m_Pipeline) == pipeline) typedRoles = true;
  }
  auto visibleSlot = [&](uint64_t index, uint32_t role, bool requireRole) {
    if(index >= visible->m_FunctionCount) return false;
    auto slot = visible->m_RayIRFunctions.find((uint32_t)index);
    if(slot == visible->m_RayIRFunctions.end() || slot->second == ResourceId()) return false;
    auto declaredRole = m_RayIRShaderRoles.find(slot->second);
    return !requireRole || (declaredRole != m_RayIRShaderRoles.end() && declaredRole->second == role);
  };
  auto objectIdentity = [&](uint32_t kind, uint64_t captured) {
    ResourceId chosen; uint64_t nativeID = 0;
    for(const auto &identity : m_ReplayGPUIdentities)
    {
      if(identity.second.kind != kind || identity.second.value != captured) continue;
      auto object = GetResourceManager()->GetResource(identity.first,true);
      if(!object || !object->m_Real || object->m_CapturedAliasable ||
         object->m_Type != (kind == 1 ? eResTexture : eResSamplerState)) return false;
      const uint64_t liveID = kind == 1 ? Unwrap((WrappedMTLTexture *)object)->gpuResourceID()._impl :
          Unwrap((WrappedMTLSamplerState *)object)->gpuResourceID()._impl;
      if(chosen != ResourceId() && (kind != 2 || nativeID != liveID)) return false;
      chosen = identity.first; nativeID = liveID;
      m_RayIRReadResources.insert(chosen);
    }
    return captured && chosen != ResourceId() && nativeID;
  };
  uint32_t validatedASHeaders=0;
  auto validateASHeader = [&](const Source &header) {
    if(!field(header,0,4) || !field(header,8,0)) return reject(__LINE__);
    ResourceId as;
    if(!word(header,0) || !ResolveRayIRIdentity(4,word(header,0),as,live)) return reject(__LINE__);
    auto structure = (WrappedMTLAccelerationStructure *)GetResourceManager()->GetResource(as);
    auto recipe = m_RayIRASCurrentContents.find(as);
    if(recipe == m_RayIRASCurrentContents.end() || !recipe->second ||
       (recipe->second->kind != 5 && recipe->second->kind != 9 &&
        recipe->second->kind != 10 && recipe->second->kind != 11) ||
       recipe->second->parameters.size() != 8 ||
       (!m_DescriptorPreflight && structure->m_LastBuildKind != 5)) return reject(__LINE__);
    const uint64_t count = recipe->second->parameters[3];
    if(count > 64 || (!count && recipe->second->kind != 10)) return reject(__LINE__);
    Source contributions;
    // An empty TLAS reads no contributions, but a declared non-null pointer
    // still needs a live, aligned backing. Never dereference its unused values.
    if((count || word(header,8)) &&
       (word(header,8)%4 || !address(word(header,8),RDCMAX(count,1ULL)*4,contributions)))
      return reject(__LINE__);
    for(uint64_t i = 16; i < 64; i += 8) if(word(header,i)) return reject(__LINE__);
    const uint64_t hitSize = word(packet,48), hitStride = word(packet,56);
    for(uint64_t i = 0; i < count; i++)
    {
      uint32_t contribution; memcpy(&contribution, contributions.raw->data() + contributions.offset + i*4,4);
      if((hitStride == 0 && contribution) || (hitStride && contribution >= hitSize / hitStride)) return reject(__LINE__);
    }
    m_RayIRReadResources.insert(as);
    for(auto child : recipe->second->children)
    {
      auto primitive = m_RayIRASCurrentContents.find(child);
      if(primitive == m_RayIRASCurrentContents.end() || !primitive->second ||
         (primitive->second->kind != 1 && primitive->second->kind != 2 && primitive->second->kind != 8))
        return reject(__LINE__);
      m_RayIRReadResources.insert(child);
    }
    bytebuf instances = recipe->second->vertices;
    uint64_t instanceStride = recipe->second->parameters[2] ==
        uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeUserID) ? 68 : 64;
    const uint32_t kind = recipe->second->kind;
    if(kind == 9)
    {
      // The frozen 72-byte packets refer to captured BLAS IDs. Use the same
      // typed conversion as initial-state reconstruction, including UserID.
      if(recipe->second->children.size() != recipe->second->childGPUIdentities.size() ||
         !ConvertMetalASIndirectInstances(recipe->second->vertices,count,
             recipe->second->childGPUIdentities,instances)) return reject(__LINE__);
      instanceStride = 68;
    }
    else if(kind == 10 || kind == 11)
    {
      if(!recipe->second->children.empty() || !recipe->second->childGPUIdentities.empty() ||
         (kind == 10 ? !ValidMetalASEmptyIndirectParameters(recipe->second->parameters,64*1024*1024) ||
                       !instances.empty() : !ValidMetalASInactiveInstances(instances,count)))
        return reject(__LINE__);
    }
    if((kind == 5 || kind == 9) && instances.size() != count * instanceStride) return reject(__LINE__);
    for(uint64_t i = 0; (kind == 5 || kind == 9) && i < count; i++)
    {
      MTL::AccelerationStructureInstanceDescriptor instance = {};
      memcpy(&instance, instances.data() + i * instanceStride, sizeof(instance));
      if(instance.accelerationStructureIndex >= recipe->second->children.size()) return reject(__LINE__);
      const auto &primitive = *m_RayIRASCurrentContents[recipe->second->children[instance.accelerationStructureIndex]];
      const uint64_t step = primitive.kind == 1 ? 8 : 10;
      if(primitive.parameters.empty() || primitive.parameters.size() % step) return reject(__LINE__);
      for(size_t g = 0; g < primitive.parameters.size(); g += step)
      {
        const uint64_t slot = uint64_t(instance.intersectionFunctionTableOffset) + primitive.parameters[g+4];
        if(slot >= intersection->m_FunctionCount || !intersection->m_RayIRFunctions.count((uint32_t)slot) ||
           intersection->m_RayIRFunctions[(uint32_t)slot] == ResourceId()) return reject(__LINE__);
      }
    }
    validatedASHeaders++;
    return true;
  };
  // Heap namespaces are explicit; IR buffer metadata is a bounded byte size,
  // not a third resource ID. Reject typed/texture-buffer views and unknown bits.
  auto heapDefinitions=m_RayIRHeapEntries.find(pipeline);
  for(uint64_t heapKind=0;heapKind<2;heapKind++)
  {
    uint64_t extent=0;
    if(heapDefinitions!=m_RayIRHeapEntries.end())
      for(const auto &entry:heapDefinitions->second)
        if(entry.heap==heapKind) extent=RDCMAX(extent,(entry.index+1)*24);
    const uint64_t pointer=word(packet,112+heapKind*8);
    if(!extent) {if(pointer) return reject(__LINE__);continue;}
    Source heap;
    if(!pointer || !address(pointer,extent,heap)) return reject(__LINE__);
    std::set<uint64_t> covered;
    for(const auto &entry:heapDefinitions->second)
    {
      if(entry.heap!=heapKind) continue;
      const uint64_t at=entry.index*24;
      for(uint64_t byteAt=at;byteAt<at+24;byteAt++) covered.insert(byteAt);
      if(!field(heap,at,entry.kind==2?2:1)) return reject(__LINE__);
      if(entry.kind==0)
      {
        Source target;
        if(word(heap,at)%4 || word(heap,at+8) || word(heap,at+16)!=entry.bytes ||
           !address(word(heap,at),entry.bytes,target)) return reject(__LINE__);
      }
      else if(entry.kind==1)
      {
        if(word(heap,at) || word(heap,at+16) || !objectIdentity(1,word(heap,at+8)))
          return reject(__LINE__);
      }
      else if(entry.kind==3)
      {
        Source header;
        if(word(heap,at+8) || word(heap,at+16) || !address(word(heap,at),64,header) ||
           !validateASHeader(header)) return reject(__LINE__);
      }
      else if(word(heap,at+8) || word(heap,at+16) || !objectIdentity(2,word(heap,at)))
        return reject(__LINE__);
    }
    // A static heap backing cannot hide additional live descriptors after the
    // highest declared index. Zero holes and allocation padding are preserved.
    const uint64_t backingBytes=heap.raw->size()-heap.offset;
    for(uint64_t byteAt=0;byteAt<backingBytes;byteAt++)
      if(!covered.count(byteAt) && (*heap.raw)[heap.offset+byteAt]) return reject(__LINE__);
    for(const auto &layout:m_DescriptorTables)
      if(layout.buffer==heap.id)
        for(uint64_t entry=0;entry<layout.count;entry++)
        {
          const uint64_t at=layout.offset+entry*layout.stride;
          if(at>=heap.offset && at<heap.offset+backingBytes &&
             (layout.schema!=(heapKind?2:1) || (at-heap.offset)%24)) return reject(__LINE__);
        }
  }
  auto validateRoots = [&](const rdcarray<RayIRLocalRoot> &definitions, const Source &records, uint64_t record, uint64_t recordSize,
                        bool &samplers, std::set<uint64_t> &covered, std::set<uint64_t> &pointers) {
    for(const auto &root:definitions)
    {
      if(root.kind==5 || root.kind==6) continue;
      const uint64_t span = root.kind == 1 ? root.bytes : 8;
      if(root.offset > recordSize || span > recordSize-root.offset) return false;
      for(uint64_t at=root.offset;at<root.offset+span;at++) covered.insert(at);
      if(root.kind == 1) continue;
      pointers.insert(root.offset);
      if(!field(records,record+root.offset,0)) return false;
      Source target;
      const uint64_t pointer = word(records,record+root.offset);
      if(!address(pointer,root.bytes,target)) return false;
      if(root.kind == 0 || root.kind == 4)
      {
        if(pointer % (root.kind == 4 ? 256 : 4)) return false;

      }
      else for(uint64_t entry=0;entry<root.count;entry++)
      {
        const uint64_t at=entry*24;
        if(root.kind == 2)
        {
          if(!field(target,at,1) || word(target,at) ||
             !objectIdentity(1,word(target,at+8)) || word(target,at+16)) return false;

        }
        else
        {
          // IR sampler entries put a sampler ID in gpuVA; metadata is scalar
          // LOD bias, not a third resource ID. Preserve it, never relocate it.
          if(!field(target,at,2) || word(target,at+8) || word(target,at+16) ||
             !objectIdentity(2,word(target,at))) return false;
          samplers = true;
        }
      }
    }
    return true;
  };
  // VisibleFunction mode preserves indices. Shader identifiers are never GPU resource IDs.
  for(uint32_t region = 0; region < 4; region++)
  {
    const uint64_t at = region == 0 ? 0 : 16 + (region - 1) * 24;
    const uint64_t start = word(packet,at), size = word(packet,at+8);
    const uint64_t stride = region ? word(packet,at+16) : 32;
    if(region == 3)
    {
      if(start || size || stride) return reject(__LINE__);
      continue;
    }
    const uint64_t recordSize = region == 0 || stride == 0 ? size : stride;
    if(!start || start % 32 || !size || recordSize < 32 || recordSize > 4096 || recordSize % 32 ||
       size % recordSize || size / recordSize > 64 || (stride == 0 && region != 2)) return reject(__LINE__);
    Source records; if(!address(start,size,records)) return reject(__LINE__);
    for(uint64_t record = 0; record < size; record += recordSize)
    {
      uint64_t intersectionIndex = word(records,record), shader = word(records,record+8);
      // In VisibleFunction mode this is a VFT index, not an IFT geometry
      // offset. Zero skips any-hit/intersection. Miss and closest-hit may be null;
      // raygen must name an entry. pad0 is unused scalar data (UE writes ~0ull).
      if(!field(records,record+16,0) ||
         (region != 2 && intersectionIndex) ||
         (intersectionIndex && !visibleSlot(intersectionIndex,3,true)) ||
         (!shader && region == 0) ||
         (shader && !visibleSlot(shader,region == 0 ? 0 : region == 1 ? 1 : 2,typedRoles)))
        return reject(__LINE__);
      bool samplers = false; std::set<uint64_t> covered, pointers;
      for(uint64_t slot : {shader, intersectionIndex})
        if(slot && !validateRoots(m_RayIRLocalRoots[visible->m_RayIRFunctions[(uint32_t)slot]],
                                 records,record,recordSize,samplers,covered,pointers)) return reject(__LINE__);
      if(word(records,record+16) && !samplers) return reject(__LINE__);
      // Every declared payload pointer must have a bound root parameter. All
      // remaining nonzero payload bytes must be explicitly scalar constants.
      for(const auto &layout : m_DescriptorTables)
        if(layout.buffer == records.id)
          for(uint64_t entry=0;entry<layout.count;entry++)
          {
            const uint64_t fieldAt=layout.offset+entry*layout.stride;
            const uint64_t begin=records.offset+record;
            if(fieldAt >= begin+32 && fieldAt < begin+recordSize &&
               (layout.schema != 0 || (!pointers.count(fieldAt-begin) &&
                word(records,fieldAt-begin)))) return reject(__LINE__);
          }
      for(uint64_t byteAt=32;byteAt<recordSize;byteAt++)
        if(!covered.count(byteAt) && (*records.raw)[records.offset+record+byteAt]) return reject(__LINE__);
    }
  }
  Source roots,header,output;
  if(!address(word(packet,104),rootWords*8,roots)) return reject(__LINE__);
  rdcarray<RayIRLocalRoot> globalDefinitions;
  auto global=m_RayIRGlobalRoots.find(pipeline);
  if(global!=m_RayIRGlobalRoots.end()) globalDefinitions=global->second;
  else if(rootWords==2) globalDefinitions={{0,5,1,64},{8,6,1,work*4}};
  else return reject(__LINE__);
  uint64_t headerField=~0ULL,outputField=~0ULL,outputBytes=0;
  bool globalSamplers=false;
  std::set<uint64_t> globalCovered,globalPointers;
  if(!validateRoots(globalDefinitions,roots,0,rootWords*8,globalSamplers,globalCovered,globalPointers))
    return reject(__LINE__);
  for(const auto &root:globalDefinitions)
  {
    if(root.kind!=5 && root.kind!=6) continue;
    if(!field(roots,root.offset,0)) return reject(__LINE__);
    for(uint64_t byteAt=root.offset;byteAt<root.offset+8;byteAt++) globalCovered.insert(byteAt);
    globalPointers.insert(root.offset);
    if(root.kind==5) headerField=root.offset;
    else {outputField=root.offset;outputBytes=root.bytes;}
  }
  // All GRS words, including zero or otherwise unused slots, have an explicit
  // scalar/pointer interpretation; arbitrary integers are never guessed as VAs.
  for(uint64_t byteAt=0;byteAt<rootWords*8;byteAt++)
    if(!globalCovered.count(byteAt)) return reject(__LINE__);
  for(const auto &layout:m_DescriptorTables)
    if(layout.buffer==roots.id)
      for(uint64_t entry=0;entry<layout.count;entry++)
      {
        const uint64_t at=layout.offset+entry*layout.stride;
        if(at>=roots.offset && at<roots.offset+rootWords*8 &&
           (layout.schema!=0 || !globalPointers.count(at-roots.offset))) return reject(__LINE__);
      }
  if(outputField==~0ULL || outputBytes<work*4 ||
     (headerField!=~0ULL && (!address(word(roots,headerField),64,header) || !validateASHeader(header))) ||
     !validatedASHeaders)
    return reject(__LINE__);
  const std::set<ResourceId> inputSources=m_RayIRReadResources;
  if(!address(word(roots,outputField),outputBytes,output) || inputSources.count(output.id)) return reject(__LINE__);
  m_RayIRReadResources.erase(output.id);
  if(output.offset % 4) return reject(__LINE__);
  // The UAV cannot overwrite any ABI source or AS construction input, including
  // an unqueried vertex/instance buffer. Capture provenance must remain immutable.
  for(const auto &table : m_DescriptorTables) if(table.buffer == output.id) return reject(__LINE__);
  for(const auto &entry : m_ReplayASInitialContents)
    if(entry.second && (entry.second->source == output.id || entry.second->indexSource == output.id)) return reject(__LINE__);
  for(const auto &entry : m_RayIRASCurrentContents)
    if(entry.second && (entry.second->source == output.id || entry.second->indexSource == output.id)) return reject(__LINE__);
  m_RayIROutput = output.id;
  return true;
}

void WrappedMTLDevice::NoteRayIRUsage()
{
  for(auto id : m_RayIRReadResources) GetReplay()->AddUsage(id, ResourceUsage::CS_Resource);
  if(m_IRComputeWriteResources.empty())
    GetReplay()->AddUsage(m_RayIROutput, ResourceUsage::CS_RWResource);
  else
    for(ResourceId output:m_IRComputeWriteResources)
      GetReplay()->AddUsage(output, ResourceUsage::CS_RWResource);
}
