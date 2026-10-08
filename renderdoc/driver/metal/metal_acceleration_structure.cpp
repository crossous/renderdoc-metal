#include "metal_acceleration_structure.h"
#include "metal_device.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_acceleration_structure_command_encoder.h"
#include <cmath>

WrappedMTLAccelerationStructure::WrappedMTLAccelerationStructure(
    MTL::AccelerationStructure *real, ResourceId id, WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

void WrappedMTLAccelerationStructureCommandEncoder::TrackInitialBuild(
    WrappedMTLAccelerationStructure *structure, WrappedMTLBuffer *vertices,
    rdcarray<uint64_t> parameters, uint32_t kind,
    const rdcarray<WrappedMTLAccelerationStructure *> &children, WrappedMTLBuffer *indices,
    WrappedMTLAccelerationStructure *refitSource)
{
  if(!IsCaptureMode(m_State) || !structure || !m_CommandBuffer) return;
  MetalASInitialCandidate candidate;
  candidate.target = GetResID(structure);
  candidate.encoder = GetResID(this);
  candidate.build = std::make_shared<MetalASInitialBuild>();
  candidate.build->kind = kind;
  candidate.build->source = GetResID(vertices);
  candidate.build->indexSource = GetResID(indices);
  if(refitSource)
  {
    candidate.refitSource = GetResID(refitSource);
    candidate.priorBuild = refitSource->m_CapturedInitialBuild;
    GetRecord(m_CommandBuffer)->MarkASInitialReferences(refitSource);
  }
  if(indices && Unwrap(indices)) candidate.indexInput = NS::RetainPtr(Unwrap(indices));
  if(indices)
  {
    GetRecord(structure)->AddParent(GetRecord(indices));
    GetResourceManager()->MarkDirtyResource(GetResID(indices));
    GetRecord(m_CommandBuffer)->MarkResourceFrameReferenced(GetResID(indices), eFrameRef_Read);
  }
  for(auto child : children)
  {
    candidate.build->children.push_back(GetResID(child));
    if(kind == 9) candidate.build->childGPUIdentities.push_back(child->m_CapturedGPUResourceID);
    GetRecord(structure)->AddParent(GetRecord(child));
    GetRecord(m_CommandBuffer)->MarkASInitialReferences(child);
  }
  candidate.build->parameters = parameters;
  if(vertices && Unwrap(vertices)) candidate.input = NS::RetainPtr(Unwrap(vertices));
  GetRecord(m_CommandBuffer)->cmdInfo->initialASBuilds.push_back(candidate);
  GetResourceManager()->MarkDirtyResource(GetResID(structure));
  if(vertices)
  {
    GetRecord(structure)->AddParent(GetRecord(vertices));
    GetResourceManager()->MarkDirtyResource(GetResID(vertices));
    GetRecord(m_CommandBuffer)->MarkResourceFrameReferenced(GetResID(vertices), eFrameRef_Read);
  }
}

void WrappedMTLAccelerationStructureCommandEncoder::TrackInitialCopy(
    WrappedMTLAccelerationStructure *source, WrappedMTLAccelerationStructure *destination, bool compact)
{
  if(!IsCaptureMode(m_State) || !source || !destination || !m_CommandBuffer) return;
  MetalASInitialCandidate candidate;
  candidate.target = GetResID(destination);
  candidate.copySource = GetResID(source);
  candidate.compactCopy = compact;
  candidate.priorBuild = source->m_CapturedInitialBuild;
  candidate.build = std::make_shared<MetalASInitialBuild>();
  GetRecord(m_CommandBuffer)->cmdInfo->initialASBuilds.push_back(candidate);
  GetRecord(destination)->AddParent(GetRecord(source));
  GetRecord(m_CommandBuffer)->MarkASInitialReferences(source);
  GetResourceManager()->MarkDirtyResource(GetResID(destination));
}

struct ASInputWrites : ResourceRecordHandler
{
  std::set<ResourceId> writes;
  void MarkDirtyResource(ResourceId) override {}
  void RemoveResourceRecord(ResourceId) override {}
  void DestroyResourceRecord(ResourceRecord *) override {}
  void MarkResourceFrameReferenced(ResourceId id, FrameRefType type) override
  {
    if(IncludesWrite(type)) writes.insert(id);
  }
};

void WrappedMTLAccelerationStructureCommandEncoder::TrackInitialBufferWrite(WrappedMTLBuffer *buffer)
{
  if(!IsCaptureMode(m_State) || !buffer || !Unwrap(buffer)) return;
  for(const auto &old : m_InitialBufferWrites)
    if(old.get() == Unwrap(buffer)) return;
  m_InitialBufferWrites.push_back(NS::RetainPtr(Unwrap(buffer)));
}

void WrappedMTLAccelerationStructureCommandEncoder::SnapshotInitialInputsAtEnd()
{
  if(!IsCaptureMode(m_State) || !m_CommandBuffer || !GetRecord(m_CommandBuffer)) return;
  // Multi-geometry inputs can be produced by an earlier encoder in this very
  // submission. Freeze them after AS consumption, before the app can encode a
  // later writer. Tracked standalone Private buffers provide native hazard
  // ordering and cannot acquire a distinct NoCopy CPU-allocation alias. Shared
  // aliases in a later encoder may escape object-based hazard tracking, even if
  // this encoder does not write them. Typed Private geometry and instance inputs
  // permit placement heaps whose whole heap is tracked, including distinct aliases.
  // Untracked/refit and CPU-backed inputs still require separate proofs.
  auto command = Unwrap(m_CommandBuffer);
  std::map<MTL::Buffer *, NS::SharedPtr<MTL::Buffer>> copies;
  uint64_t total = 0;
  MTL::BlitCommandEncoder *blit = NULL;
  auto valid = [&](MTL::Buffer *input) {
    if(!input || input->storageMode() != MTL::StorageModePrivate ||
       !input->length() ||
       input->length() > 64*1024*1024 ||
       input->hazardTrackingMode() != MTL::HazardTrackingModeTracked) return false;
    if(input->heap())
    {
      if(input->heap()->type() != MTL::HeapTypePlacement ||
         input->heap()->hazardTrackingMode() != MTL::HazardTrackingModeTracked) return false;
    }
    else if(input->isAliasable()) return false;
    for(const auto &entry : m_InitialBufferWrites)
    {
      auto write = entry.get();
      if(!write || write == input) return false;
      if(input->heap() && write->heap() == input->heap() &&
         input->heapOffset() < write->heapOffset()+write->length() &&
         write->heapOffset() < input->heapOffset()+input->length()) return false;
    }
    for(const auto &candidate : GetRecord(m_CommandBuffer)->cmdInfo->initialASBuilds)
    {
      if(candidate.encoder != GetResID(this)) continue;
      auto object = GetResourceManager()->GetResource(candidate.target, true);
      auto structure = object && object->m_Type == eResAccelerationStructure ?
          Unwrap((WrappedMTLAccelerationStructure *)object) : NULL;
      if(input->heap() && structure && structure->heap() == input->heap() &&
         input->heapOffset() < structure->heapOffset()+structure->size() &&
         structure->heapOffset() < input->heapOffset()+input->length()) return false;
    }
    return true;
  };
  auto copy = [&](MTL::Buffer *input) -> NS::SharedPtr<MTL::Buffer> {
    auto old = copies.find(input);
    if(old != copies.end()) return old->second;
    if(input->length() > 64*1024*1024-total) return {};
    auto staging = NS::TransferPtr(Unwrap(m_Device)->newBuffer(input->length(), MTL::ResourceStorageModeShared));
    if(!staging) return {};
    if(!blit) blit = command->blitCommandEncoder();
    if(!blit) return {};
    blit->copyFromBuffer(input, 0, staging.get(), 0, input->length());
    total += input->length(); copies[input] = staging;
    return staging;
  };
  for(auto &candidate : GetRecord(m_CommandBuffer)->cmdInfo->initialASBuilds)
  {
    if(candidate.encoder != GetResID(this) || !candidate.build ||
       (candidate.build->kind != 1 && candidate.build->kind != 2 && candidate.build->kind != 8 && candidate.build->kind != 9 && candidate.build->kind != 11) || candidate.copySource != ResourceId() ||
       candidate.refitSource != ResourceId() || candidate.build->inputReadback) continue;
    auto input = candidate.input.get(), index = candidate.indexInput.get();
    uint64_t bytes = 0;
    const auto kind = candidate.build->kind;
    const auto &p = candidate.build->parameters;
    const bool triangle = kind == 1 || kind == 2;
    // A retained native allocation alone does not establish a live captured object.
    // Explicit retirement is different from placement isAliasable at creation.
    auto sourceObject = GetResourceManager()->GetResource(candidate.build->source, true);
    const bool knownInput = (sourceObject && sourceObject->m_Type == eResBuffer &&
         Unwrap((WrappedMTLBuffer *)sourceObject) == input &&
         ValidMetalASPrivateInstanceInput((WrappedMTLBuffer *)sourceObject));
    auto indexObject = GetResourceManager()->GetResource(candidate.build->indexSource, true);
    const bool knownIndex = kind != 2 && kind != 8 ? true :
        indexObject && indexObject->m_Type == eResBuffer &&
        Unwrap((WrappedMTLBuffer *)indexObject) == index &&
        ValidMetalASPrivateInstanceInput((WrappedMTLBuffer *)indexObject);
    if(getenv("RENDERDOC_METAL_TRACE_AS_INPUT_SNAPSHOTS"))
      fprintf(stderr, "Metal AS input snapshot candidate target=%s source=%s kind=%u count=%llu known=%d nativeValid=%d heap=%d hazard=%u heapHazard=%u writes=%zu\n",
          ToStr(candidate.target).c_str(), ToStr(candidate.build->source).c_str(), kind,
          (unsigned long long)(p.size() >= 8 ? p[3] : 0), knownInput, valid(input),
          input && input->heap(), input ? unsigned(input->hazardTrackingMode()) : 0,
          input && input->heap() ? unsigned(input->heap()->hazardTrackingMode()) : 0, m_InitialBufferWrites.size());
    if(!knownInput || !knownIndex || !valid(input) ||
       (kind == 2 && !valid(index)) ||
       (triangle ? p.size() != (kind == 2 ? 10U : 8U) : kind == 8 ?
        !valid(index) || !ValidMetalASMultiIndexedParameters(p, input->length(), index->length()) :
        !MetalASIndirectInstanceSpan(p, input->length(), bytes))) continue;
    auto inputCopy = copy(input);
    NS::SharedPtr<MTL::Buffer> indexCopy;
    if(kind == 2 || kind == 8) indexCopy = copy(index);
    if(!inputCopy || ((kind == 2 || kind == 8) && !indexCopy)) continue;
    if(getenv("RENDERDOC_METAL_TRACE_AS_INPUT_SNAPSHOTS"))
      fprintf(stderr, "Metal AS input frozen target=%s source=%s bytes=%llu\n", ToStr(candidate.target).c_str(),
          ToStr(candidate.build->source).c_str(), (unsigned long long)input->length());
    candidate.build->inputReadback = inputCopy;
    candidate.build->indexReadback = indexCopy;
    candidate.build->submission = NS::RetainPtr(command);
    // Also safe with unretained command buffers or capture cancellation.
    const auto sourceOwner = candidate.input, indexOwner = candidate.indexInput;
    command->addCompletedHandler([inputCopy, sourceOwner, indexCopy, indexOwner](MTL::CommandBuffer *) {});
  }
  // Query output may be Private even when the app later copies it to a CPU
  // readback. Freeze the typed value at this encoder boundary, before reuse.
  for(ResourceId id : m_PrivateCompactedSizeQueries)
  {
    auto object = GetResourceManager()->GetResource(id, true);
    auto structure = object && object->m_Type == eResAccelerationStructure ?
        (WrappedMTLAccelerationStructure *)object : NULL;
    auto bufferObject = structure ? GetResourceManager()->GetResource(structure->m_LastCompactedSizeBuffer, true) : NULL;
    auto input = bufferObject && bufferObject->m_Type == eResBuffer ? Unwrap((WrappedMTLBuffer *)bufferObject) : NULL;
    const uint64_t bytes = structure && structure->m_LastCompactedSizeType == MTL::DataTypeUInt ? 4 :
        structure && structure->m_LastCompactedSizeType == MTL::DataTypeULong ? 8 : 0;
    if(!structure || !bytes || !input ||
       !ValidMetalASPrivateInstanceInput((WrappedMTLBuffer *)bufferObject) ||
       structure->m_LastCompactedSizeOffset > input->length() ||
       bytes > input->length()-structure->m_LastCompactedSizeOffset) continue;
    bool overlaps = false;
    for(ResourceId other : m_PrivateCompactedSizeQueries)
    {
      if(other == id) continue;
      auto otherObject = GetResourceManager()->GetResource(other, true);
      auto otherAS = otherObject && otherObject->m_Type == eResAccelerationStructure ?
          (WrappedMTLAccelerationStructure *)otherObject : NULL;
      auto otherBuffer = otherAS ? GetResourceManager()->GetResource(otherAS->m_LastCompactedSizeBuffer, true) : NULL;
      auto nativeOther = otherBuffer && otherBuffer->m_Type == eResBuffer ? Unwrap((WrappedMTLBuffer *)otherBuffer) : NULL;
      const uint64_t otherBytes = otherAS && otherAS->m_LastCompactedSizeType == MTL::DataTypeUInt ? 4 : 8;
      if(!nativeOther) { overlaps = true; break; }
      if(input == nativeOther || (input->heap() && input->heap() == nativeOther->heap()))
      {
        const uint64_t begin = structure->m_LastCompactedSizeOffset + (input->heap() ? input->heapOffset() : 0);
        const uint64_t otherBegin = otherAS->m_LastCompactedSizeOffset + (nativeOther->heap() ? nativeOther->heapOffset() : 0);
        overlaps |= begin < otherBegin + otherBytes && otherBegin < begin + bytes;
      }
    }
    if(overlaps) continue;
    auto snapshot = NS::TransferPtr(Unwrap(m_Device)->newBuffer(bytes, MTL::ResourceStorageModeShared));
    if(!snapshot) continue;
    if(!blit) blit = command->blitCommandEncoder();
    if(!blit) continue;
    blit->copyFromBuffer(input, structure->m_LastCompactedSizeOffset, snapshot.get(), 0, bytes);
    structure->m_CapturedCompactedSizeReadback = snapshot;
    structure->m_CapturedCompactedSizeSubmission = NS::RetainPtr(command);
    auto owner = NS::RetainPtr(input);
    command->addCompletedHandler([snapshot, owner](MTL::CommandBuffer *) {});
  }
  m_PrivateCompactedSizeQueries.clear();
  if(blit) blit->endEncoding();
  m_InitialBufferWrites.clear();
}

bool ValidMetalASInstance(const MTL::AccelerationStructureInstanceDescriptor &data, size_t children)
{
  // Vulkan/DX12 equivalents: culling, winding, force opaque/nonopaque, instance
  // mask and hit-group offset. Keep offsets within the current Metal table limit.
  const uint32_t flags = uint32_t(data.options);
  if(flags & ~0xfU || (flags & 0xcU) == 0xcU || data.mask > 0xff ||
     data.intersectionFunctionTableOffset > 31 || data.accelerationStructureIndex >= children)
    return false;
  for(int column = 0; column < 4; column++)
    for(int row = 0; row < 3; row++)
      if(!std::isfinite(data.transformationMatrix[column][row])) return false;
  return true;
}

bool ValidMetalASInitialInstances(const bytebuf &bytes, uint64_t count, size_t children,
    uint64_t descriptorType, size_t childLimit)
{
  RDCCOMPILE_ASSERT(sizeof(MTL::AccelerationStructureInstanceDescriptor) == 64 &&
      sizeof(MTL::AccelerationStructureUserIDInstanceDescriptor) == 68 &&
      offsetof(MTL::AccelerationStructureUserIDInstanceDescriptor, userID) == 64,
      "Metal UserID instances must preserve the default descriptor prefix");
  const uint64_t stride = descriptorType == uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeDefault) ?
      sizeof(MTL::AccelerationStructureInstanceDescriptor) :
      descriptorType == uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeUserID) ?
      sizeof(MTL::AccelerationStructureUserIDInstanceDescriptor) : 0;
  if(!children || children > childLimit || childLimit > MetalMaxIndirectASChildren ||
     count < children || count > 65536 ||
     !stride || bytes.size() != count * stride) return false;
  for(uint64_t i = 0; i < count; i++)
  {
    MTL::AccelerationStructureInstanceDescriptor data = {};
    memcpy(&data, bytes.data() + i * stride, sizeof(data));
    if(!ValidMetalASInstance(data, children)) return false;
  }
  return true;
}

bool MetalASInitialInstanceSpan(const rdcarray<uint64_t> &p, uint64_t length,
    uint64_t &packedBytes)
{
  if(p.size() != 8 || !p[3] || p[3] > 65536 || p[4] || p[5] || p[6] || p[7]) return false;
  const uint64_t stride = p[2] == uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeDefault) ?
      sizeof(MTL::AccelerationStructureInstanceDescriptor) :
      p[2] == uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeUserID) ?
      sizeof(MTL::AccelerationStructureUserIDInstanceDescriptor) : 0;
  if(!stride || p[0] % 4 || p[1] < stride || p[1] > 1024*1024 || p[1] % 4 ||
     (p[2] == 0 && (p[0] || p[1] != stride))) return false;
  const uint64_t span = (p[3]-1)*p[1]+stride;
  if(span > 64*1024*1024 || p[0] > length || span > length-p[0]) return false;
  packedBytes = p[3]*stride;
  return true;
}

bool PackMetalASInitialInstances(const byte *data, uint64_t length,
    const rdcarray<uint64_t> &p, size_t children, bytebuf &packed)
{
  uint64_t bytes = 0;
  if(!data || !MetalASInitialInstanceSpan(p, length, bytes)) return false;
  packed.resize(size_t(bytes));
  const size_t stride = size_t(bytes/p[3]);
  for(uint64_t i = 0; i < p[3]; i++)
    memcpy(packed.data()+i*stride, data+p[0]+i*p[1], stride);
  return ValidMetalASInitialInstances(packed, p[3], children, p[2]);
}

bool ValidMetalASPrivateInstanceInput(WrappedMTLBuffer *input)
{
  if(!input || !Unwrap(input) || Atomic::CmpExch32(&input->m_CapturedAliasable, 0, 0)) return false;
  auto native = Unwrap(input);
  if(native->storageMode() != MTL::StorageModePrivate || !native->length() ||
     native->length() > 64*1024*1024 ||
     native->hazardTrackingMode() != MTL::HazardTrackingModeTracked) return false;
  auto heap = native->heap();
  // Placement resources report isAliasable from birth; it is not retirement.
  // Tracking on the whole heap orders even a different resource at the same
  // address after our encoder-local readback. Explicit retirement stays rejected.
  return heap ? heap->type() == MTL::HeapTypePlacement &&
      heap->hazardTrackingMode() == MTL::HazardTrackingModeTracked : !native->isAliasable();
}

bool ValidMetalASEmptyIndirectParameters(const rdcarray<uint64_t> &p, uint64_t length)
{
  return p.size() == 8 &&
      p[2] == uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeIndirect) &&
      p[3] == 0 && !p[4] && !p[5] && !p[6] && !p[7] &&
      p[0] % 8 == 0 && p[0] < length && length && length <= 64*1024*1024 &&
      p[1] >= 72 && p[1] <= 1024*1024 && p[1] % 8 == 0;
}

bool MetalASIndirectInstanceSpan(const rdcarray<uint64_t> &p, uint64_t length,
    uint64_t &packedBytes)
{
  RDCCOMPILE_ASSERT(sizeof(MTL::IndirectAccelerationStructureInstanceDescriptor) == 72,
      "Metal indirect instance layout changed");
  if(p.size() != 8 || p[2] != uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeIndirect) ||
     !p[3] || p[3] > 65536 || p[4] || p[5] || p[6] || p[7] ||
     p[0] % 8 || p[1] < 72 || p[1] > 1024*1024 || p[1] % 8) return false;
  const uint64_t span = (p[3]-1)*p[1]+72;
  if(span > 64*1024*1024 || p[0] > length || span > length-p[0]) return false;
  packedBytes = p[3]*72;
  return true;
}

bool ValidMetalASInactiveInstances(const bytebuf &raw, uint64_t count)
{
  if(!count || count > 65536 || raw.size() != count*72) return false;
  for(uint64_t i = 0; i < count; i++)
  {
    MTL::IndirectAccelerationStructureInstanceDescriptor data = {};
    memcpy(&data, raw.data()+i*72, sizeof(data));
    if(data.accelerationStructureID._impl || data.mask) return false;
    MTL::AccelerationStructureInstanceDescriptor base = {};
    base.transformationMatrix = data.transformationMatrix; base.options = data.options;
    base.intersectionFunctionTableOffset = data.intersectionFunctionTableOffset;
    if(!ValidMetalASInstance(base, 1)) return false;
  }
  return true;
}

bool PackMetalASInactiveInstances(const byte *data, uint64_t length,
    const rdcarray<uint64_t> &p, bytebuf &raw)
{
  uint64_t bytes = 0;
  if(!data || !MetalASIndirectInstanceSpan(p, length, bytes)) return false;
  raw.resize(size_t(bytes));
  for(uint64_t i = 0; i < p[3]; i++) memcpy(raw.data()+i*72, data+p[0]+i*p[1], 72);
  return ValidMetalASInactiveInstances(raw, p[3]);
}

bool ConvertMetalASIndirectInstances(const bytebuf &raw, uint64_t count,
    const rdcarray<uint64_t> &identities, bytebuf &userIDInstances)
{
  if(identities.empty() || identities.size() > MetalMaxIndirectASChildren || count < identities.size() || count > 65536 ||
     raw.size() != count*sizeof(MTL::IndirectAccelerationStructureInstanceDescriptor)) return false;
  std::map<uint64_t, uint32_t> childIndices;
  for(size_t i = 0; i < identities.size(); i++)
    if(!identities[i] || !childIndices.emplace(identities[i], uint32_t(i)).second) return false;
  userIDInstances.resize(size_t(count*sizeof(MTL::AccelerationStructureUserIDInstanceDescriptor)));
  for(uint64_t i = 0; i < count; i++)
  {
    MTL::IndirectAccelerationStructureInstanceDescriptor indirect = {};
    memcpy(&indirect, raw.data()+i*sizeof(indirect), sizeof(indirect));
    const auto child = childIndices.find(indirect.accelerationStructureID._impl);
    if(child == childIndices.end()) return false;
    MTL::AccelerationStructureUserIDInstanceDescriptor direct = {};
    direct.transformationMatrix = indirect.transformationMatrix;
    direct.options = indirect.options; direct.mask = indirect.mask;
    direct.intersectionFunctionTableOffset = indirect.intersectionFunctionTableOffset;
    direct.accelerationStructureIndex = child->second; direct.userID = indirect.userID;
    memcpy(userIDInstances.data()+i*sizeof(direct), &direct, sizeof(direct));
  }
  return ValidMetalASInitialInstances(userIDInstances, count, identities.size(),
      uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeUserID), MetalMaxIndirectASChildren);
}

bool PackMetalASIndirectInstances(const byte *data, uint64_t length,
    const rdcarray<uint64_t> &p, const rdcarray<uint64_t> &identities, bytebuf &packed)
{
  uint64_t bytes = 0;
  if(!data || !MetalASIndirectInstanceSpan(p, length, bytes)) return false;
  packed.resize(size_t(bytes));
  for(uint64_t i = 0; i < p[3]; i++) memcpy(packed.data()+i*72, data+p[0]+i*p[1], 72);
  bytebuf userIDInstances;
  return ConvertMetalASIndirectInstances(packed, p[3], identities, userIDInstances);
}

bool MetalASInitialIndexedSpan(const rdcarray<uint64_t> &p, const bytebuf &indices,
    uint64_t &vertexBytes)
{
  if(p.size() != 10 || !p[3] || p[3] > 1000000 || p[1] < 12 || p[1] > 1024*1024 ||
     p[1] % 4 || p[0] % 4 || p[4] > 31 || p[5] > 1 || p[6] > 1 ||
     (p[7] != 0 && p[7] != uint64_t(MTL::AccelerationStructureUsageRefit))) return false;
  const uint64_t formatBytes = p[2] == uint64_t(MTL::AttributeFormatFloat3) ? 12 :
                               p[2] == uint64_t(MTL::AttributeFormatFloat4) ? 16 : 0;
  const uint64_t indexBytes = p[9] == uint64_t(MTL::IndexTypeUInt16) ? 2 :
                              p[9] == uint64_t(MTL::IndexTypeUInt32) ? 4 : 0;
  if(!formatBytes || p[1] < formatBytes || !indexBytes || p[8] % indexBytes ||
     indices.size() != p[3] * 3 * indexBytes) return false;
  uint64_t maximum = 0;
  for(uint64_t i = 0; i < p[3] * 3; i++)
  {
    uint32_t index = 0;
    memcpy(&index, indices.data() + i * indexBytes, size_t(indexBytes));
    maximum = RDCMAX(maximum, uint64_t(index));
  }
  vertexBytes = maximum * p[1] + formatBytes;
  return vertexBytes <= 64*1024*1024;
}

bool MetalASInitialBoxSpan(const rdcarray<uint64_t> &p, uint64_t &bytes)
{
  if(p.size() != 8 || p[0] % 4 || p[1] < 24 || p[1] > 1024*1024 || p[1] % 4 ||
     p[2] || !p[3] || p[3] > 1000000 || p[4] > 31 || p[5] > 1 || p[6] > 1 ||
     (p[7] && p[7] != uint64_t(MTL::AccelerationStructureUsageRefit))) return false;
  bytes = (p[3] - 1) * p[1] + 24;
  return bytes + p[1] - 24 <= 64*1024*1024;
}

bool ValidMetalASInitialBoxes(const rdcarray<uint64_t> &p, const bytebuf &boxes)
{
  uint64_t bytes = 0;
  if(!MetalASInitialBoxSpan(p, bytes) || boxes.size() != bytes) return false;
  for(uint64_t i = 0; i < p[3]; i++)
  {
    float bounds[6]; memcpy(bounds, boxes.data() + i * p[1], sizeof(bounds));
    for(unsigned axis = 0; axis < 3; axis++)
      if(!std::isfinite(bounds[axis]) || !std::isfinite(bounds[axis+3]) ||
         bounds[axis] > bounds[axis+3]) return false;
  }
  return true;
}

bool ValidMetalASMultiIndexedParameters(const rdcarray<uint64_t> &p,
    uint64_t vertexLength, uint64_t indexLength)
{
  if(p.size() < 20 || p.size() > 640 || p.size() % 10 ||
     !vertexLength || !indexLength) return false;
  uint64_t totalTriangles = 0;
  for(size_t base = 0; base < p.size(); base += 10)
  {
    const uint64_t *g = p.data() + base;
    const uint64_t format = g[2] == uint64_t(MTL::AttributeFormatFloat3) ? 12 :
                            g[2] == uint64_t(MTL::AttributeFormatFloat4) ? 16 : 0;
    const uint64_t indexSize = g[9] == uint64_t(MTL::IndexTypeUInt16) ? 2 :
                               g[9] == uint64_t(MTL::IndexTypeUInt32) ? 4 : 0;
    if(!format || !indexSize || !g[3] || g[3] > 1000000 ||
       g[1] < format || g[1] > 1024*1024 || g[1] % 4 || g[0] % 4 ||
       g[4] > 31 || g[5] > 1 || g[6] > 1 || g[7] != p[7] ||
       (g[7] && g[7] != uint64_t(MTL::AccelerationStructureUsageRefit)) ||
       g[0] > vertexLength || g[1] > vertexLength - g[0] ||
       g[8] % indexSize || g[8] > indexLength ||
       g[3] * 3 * indexSize > indexLength - g[8]) return false;
    totalTriangles += g[3];
  }
  return totalTriangles <= 1000000;
}

bool ValidMetalASMultiIndexedInputs(const rdcarray<uint64_t> &p,
    const bytebuf &vertices, const bytebuf &indices)
{
  if(vertices.size() > 64*1024*1024 || indices.size() > 64*1024*1024 ||
     !ValidMetalASMultiIndexedParameters(p, vertices.size(), indices.size())) return false;
  for(size_t base = 0; base < p.size(); base += 10)
  {
    rdcarray<uint64_t> geometry(p.data() + base, 10);
    const uint64_t indexSize = geometry[9] == uint64_t(MTL::IndexTypeUInt16) ? 2 : 4;
    bytebuf span(indices.data() + geometry[8], size_t(geometry[3] * 3 * indexSize));
    uint64_t bytes = 0;
    const uint64_t format = geometry[2] == uint64_t(MTL::AttributeFormatFloat4) ? 16 : 12;
    if(!MetalASInitialIndexedSpan(geometry, span, bytes) ||
       bytes + geometry[1] - format > vertices.size() - geometry[0]) return false;
  }
  return true;
}

MTL::PrimitiveAccelerationStructureDescriptor *MetalASMultiIndexedDescriptor(
    MTL::Buffer *vertices, MTL::Buffer *indices, const rdcarray<uint64_t> &p)
{
  rdcarray<NS::Object *> geometries;
  for(size_t base = 0; base < p.size(); base += 10)
  {
    const uint64_t *g = p.data() + base;
    auto triangle = MTL::AccelerationStructureTriangleGeometryDescriptor::descriptor();
    triangle->setVertexBuffer(vertices); triangle->setVertexBufferOffset(g[0]);
    triangle->setVertexStride(g[1]); triangle->setVertexFormat(MTL::AttributeFormat(g[2]));
    triangle->setTriangleCount(g[3]); triangle->setIntersectionFunctionTableOffset(g[4]);
    triangle->setOpaque(g[5] != 0);
    triangle->setAllowDuplicateIntersectionFunctionInvocation(g[6] != 0);
    triangle->setIndexBuffer(indices); triangle->setIndexBufferOffset(g[8]);
    triangle->setIndexType(MTL::IndexType(g[9]));
    geometries.push_back(triangle);
  }
  auto descriptor = MTL::PrimitiveAccelerationStructureDescriptor::descriptor();
  descriptor->setUsage(MTL::AccelerationStructureUsage(p[7]));
  descriptor->setGeometryDescriptors(NS::Array::array(
      (const NS::Object *const *)geometries.data(), geometries.size()));
  return descriptor;
}

bool MaterialiseMetalASInitialBuild(const std::shared_ptr<MetalASInitialBuild> &build)
{
  if(!build || !build->submission ||
     build->submission->status() != MTL::CommandBufferStatusCompleted ||
     build->submission->error()) return false;
  for(const auto &dependency : build->dependencies)
    if(!dependency || dependency->status() != MTL::CommandBufferStatusCompleted ||
       dependency->error()) return false;
  // An empty TLAS reads no instance packet and owns no BLAS dependencies.
  // Its explicit recipe distinguishes completed empty state from missing bytes.
  if(build->kind == 10)
    return ValidMetalASEmptyIndirectParameters(build->parameters, 64*1024*1024) &&
        build->children.empty() && build->childGPUIdentities.empty() && build->childBuilds.empty() &&
        build->vertices.empty() && build->indices.empty() && !build->inputReadback;
  if(build->kind == 11)
  {
    if(!build->children.empty() || !build->childBuilds.empty() ||
       !build->childGPUIdentities.empty() || !build->indices.empty() || build->parameters.size() != 8)
      return false;
    if(build->vertices.empty())
    {
      auto input = build->inputReadback.get();
      if(!input || !input->contents() || !PackMetalASInactiveInstances((const byte *)input->contents(),
          input->length(), build->parameters, build->vertices))
      { RDCERR("Metal no-child indirect snapshot is not an all-masked null-ID packet"); return false; }
      build->inputReadback.reset();
    }
    return ValidMetalASInactiveInstances(build->vertices, build->parameters[3]);
  }
  if(build->kind == 9 && build->vertices.empty())
  {
    auto input = build->inputReadback.get();
    uint64_t bytes = 0;
    const auto &p = build->parameters;
    if(!input || !input->contents() ||
       !MetalASIndirectInstanceSpan(p, input->length(), bytes) ||
       build->children.size() != build->childGPUIdentities.size() ||
       build->children.size() != build->childBuilds.size()) return false;
    bytebuf inactive;
    if(PackMetalASInactiveInstances((const byte *)input->contents(), input->length(), p, inactive))
    {
      build->kind = 11; build->vertices = inactive; build->children.clear();
      build->childGPUIdentities.clear(); build->childBuilds.clear(); build->inputReadback.reset();
      return true;
    }
    bytebuf raw; raw.resize(size_t(bytes));
    rdcarray<size_t> used;
    std::set<size_t> usedIndices;
    std::map<uint64_t, size_t> childIndices;
    if(build->children.empty() || build->children.size() > MetalMaxIndirectASChildren) return false;
    for(size_t i = 0; i < build->childGPUIdentities.size(); i++)
      if(!build->childGPUIdentities[i] ||
         !childIndices.emplace(build->childGPUIdentities[i], i).second) return false;
    for(uint64_t i = 0; i < p[3]; i++)
    {
      memcpy(raw.data()+i*72, (const byte *)input->contents()+p[0]+i*p[1], 72);
      MTL::IndirectAccelerationStructureInstanceDescriptor data = {};
      memcpy(&data, raw.data()+i*72, sizeof(data));
      const auto child = childIndices.find(data.accelerationStructureID._impl);
      if(child == childIndices.end())
      {
        RDCERR("Metal indirect initial snapshot has unknown child GPU identity %llu at instance %llu",
               (unsigned long long)data.accelerationStructureID._impl, (unsigned long long)i);
        return false;
      }
      if(usedIndices.insert(child->second).second) used.push_back(child->second);
    }
    rdcarray<ResourceId> children;
    rdcarray<uint64_t> identities;
    rdcarray<std::shared_ptr<MetalASInitialBuild>> recipes;
    for(size_t child : used)
    {
      children.push_back(build->children[child]); identities.push_back(build->childGPUIdentities[child]);
      recipes.push_back(build->childBuilds[child]);
    }
    bytebuf direct;
    if(!ConvertMetalASIndirectInstances(raw, p[3], identities, direct)) return false;
    build->children = children; build->childGPUIdentities = identities; build->childBuilds = recipes;
    build->vertices = raw; build->inputReadback.reset();
  }
  for(const auto &child : build->childBuilds)
    if(!MaterialiseMetalASInitialBuild(child)) return false;
  if(!build->vertices.empty()) return true;
  auto input = build->inputReadback.get();
  const auto &p = build->parameters;
  if(build->kind == 8)
  {
    auto index = build->indexReadback.get();
    if(!input || !input->contents() || !index || !index->contents() ||
       input->length() > 64*1024*1024 || index->length() > 64*1024*1024) return false;
    bytebuf vertices((byte *)input->contents(), input->length());
    bytebuf indices((byte *)index->contents(), index->length());
    if(!ValidMetalASMultiIndexedInputs(p, vertices, indices)) return false;
    build->vertices = vertices; build->indices = indices;
    build->inputReadback.reset(); build->indexReadback.reset();
    return true;
  }
  if(!input || !input->contents() || p.size() != (build->kind == 2 ? 10U : 8U) ||
     p[0] > input->length()) return false;
  bytebuf indices;
  uint64_t bytes = 0;
  if(build->kind == 5 || build->kind == 9)
  {
    bytebuf instances;
    const bool valid = build->kind == 9 ?
        PackMetalASIndirectInstances((const byte *)input->contents(), input->length(), p,
            build->childGPUIdentities, instances) :
        PackMetalASInitialInstances((const byte *)input->contents(), input->length(), p,
            build->children.size(), instances);
    if(!valid) return false;
    build->vertices = instances;
  }
  else if(build->kind == 3)
  {
    if(!MetalASInitialBoxSpan(p, bytes) || bytes + p[1] - 24 > input->length() - p[0])
      return false;
    bytebuf boxes((byte *)input->contents() + p[0], size_t(bytes));
    if(!ValidMetalASInitialBoxes(p, boxes)) return false;
    build->vertices = boxes;
  }
  else
  {
    if((build->kind != 1 && build->kind != 2) || !p[3] || p[3] > 1000000 ||
       p[0] % 4 || p[1] < 12 || p[1] > 1024*1024 || p[1] % 4 ||
       p[4] > 31 || p[5] > 1 || p[6] > 1 ||
       (p[7] && p[7] != uint64_t(MTL::AccelerationStructureUsageRefit))) return false;
    const uint64_t format = p[2] == uint64_t(MTL::AttributeFormatFloat3) ? 12 :
                            p[2] == uint64_t(MTL::AttributeFormatFloat4) ? 16 : 0;
    if(!format || p[1] < format) return false;
    if(build->kind == 2)
    {
      auto index = build->indexReadback.get();
      const uint64_t size = p[9] == uint64_t(MTL::IndexTypeUInt16) ? 2 :
                            p[9] == uint64_t(MTL::IndexTypeUInt32) ? 4 : 0;
      if(!index || !index->contents() || !size || p[8] > index->length() ||
         p[3] * 3 * size > index->length() - p[8]) return false;
      indices = bytebuf((byte *)index->contents() + p[8], size_t(p[3] * 3 * size));
      if(!MetalASInitialIndexedSpan(p, indices, bytes)) return false;
    }
    else bytes = (p[3] * 3 - 1) * p[1] + format;
    if(bytes + p[1] - format > 64*1024*1024 ||
       bytes + p[1] - format > input->length() - p[0]) return false;
    build->vertices = bytebuf((byte *)input->contents() + p[0], size_t(bytes));
    build->indices = indices;
  }
  build->inputReadback.reset();
  build->indexReadback.reset();
  return true;
}

void WrappedMTLDevice::CaptureASInitialBuilds(MetalResourceRecord *command)
{
  auto &candidates = command->cmdInfo->initialASBuilds;
  if(candidates.empty()) return;
  auto nativeCommand = Unwrap((WrappedMTLCommandBuffer *)command->m_Resource);
  bool ready = command->HasOnlyASInitialCommands();
  bool ordered = ready;
  // A previous unfinished GPU producer could change Shared bytes after the CPU
  // snapshot. Do not wait on arbitrary reservations or silently infer its result.
  {
    SCOPED_LOCK(m_CapturePendingGPULock);
    for(MTL::CommandBuffer *prior : m_CapturePendingGPU)
    {
      ready &= prior->status() == MTL::CommandBufferStatusCompleted;
      if(prior->status() != MTL::CommandBufferStatusCompleted &&
         prior->commandQueue() != nativeCommand->commandQueue()) ordered = false;
      if(prior->error()) ordered = false;
    }
  }
  for(const auto &queue : m_CaptureCommandBuffersEnqueued)
    for(MetalResourceRecord *record : queue.second)
      if(record != command) ready = ordered = false;
  ASInputWrites references;
  command->AddResourceReferences(&references);
  auto hasInputWrite = [&](MTL::Buffer *input, ResourceId source) {
    if(references.writes.count(source)) return true;
    // Separate NoCopy buffers can cover the same Shared allocation. Resource
    // identity alone does not prove that a compacted-size/scratch write is disjoint.
    for(ResourceId written : references.writes)
    {
      auto object = GetResourceManager()->GetResource(written, true);
      if(!object) return true; // Its native range can no longer be proven.
      if(object->m_Type != eResBuffer) continue;
      auto buffer = Unwrap((WrappedMTLBuffer *)object);
      if(!buffer) return true;
      const auto cpuVisible = [](MTL::Buffer *value) {
        return value->storageMode() == MTL::StorageModeShared ||
               value->storageMode() == MTL::StorageModeManaged;
      };
      if(!cpuVisible(buffer) || !cpuVisible(input) ||
         !buffer->contents() || !input->contents()) continue;
      const uintptr_t readStart = uintptr_t(input->contents());
      const uintptr_t writeStart = uintptr_t(buffer->contents());
      if(readStart <= writeStart ? writeStart - readStart < input->length() :
                                  readStart - writeStart < buffer->length()) return true;
    }
    return false;
  };
  std::map<ResourceId, std::shared_ptr<MetalASInitialBuild>> priorBuilds;
  for(const auto &candidate : candidates)
  {
    ResourceId source = candidate.copySource != ResourceId() ? candidate.copySource : candidate.refitSource;
    auto object = GetResourceManager()->GetResource(source, true);
    if(object && object->m_Type == eResAccelerationStructure)
      priorBuilds[source] = ((WrappedMTLAccelerationStructure *)object)->m_CapturedInitialBuild;
  }
  for(auto &candidate : candidates)
  {
    auto object = GetResourceManager()->GetResource(candidate.target, true);
    if(!object || object->m_Type != eResAccelerationStructure) continue;
    auto structure = (WrappedMTLAccelerationStructure *)object;
    structure->m_CapturedInitialBuild = candidate.build;
    if(candidate.copySource != ResourceId())
    {
      auto sourceObject = GetResourceManager()->GetResource(candidate.copySource, true);
      auto source = sourceObject && sourceObject->m_Type == eResAccelerationStructure ?
          (WrappedMTLAccelerationStructure *)sourceObject : NULL;
      const auto &prior = candidate.priorBuild;
      MaterialiseMetalASInitialBuild(prior);
      // Like Vulkan, copy owns an immutable recipe independent of later source
      // updates. Only a completed, earlier submission can supply this recipe.
      const bool sourceOrdered = prior && prior->submission &&
          (prior->submission->status() == MTL::CommandBufferStatusCompleted ||
           prior->submission->commandQueue() == nativeCommand->commandQueue());
      if(ordered && sourceOrdered && source && candidate.target != candidate.copySource && prior &&
         priorBuilds[candidate.copySource] == prior &&
         (!prior->vertices.empty() || prior->inputReadback) && !prior->submission->error() &&
         prior->dependencies.size() < 4096 &&
         source->m_LastBuildCommandBuffer != GetResID(command->m_Resource) &&
         structure->m_LastBuildCommandBuffer == GetResID(command->m_Resource))
      {
        *candidate.build = *prior;
        candidate.build->dependencies.push_back(prior->submission);
        candidate.build->compacted |= candidate.compactCopy;
        candidate.build->submission = NS::RetainPtr(Unwrap((WrappedMTLCommandBuffer *)command->m_Resource));
      }
      continue;
    }
    const auto &p = candidate.build->parameters;
    auto input = candidate.input.get();
    if(candidate.build->kind == 10)
    {
      if(input && ValidMetalASEmptyIndirectParameters(p, input->length()) &&
         candidate.build->children.empty() &&
         structure->m_LastBuildCommandBuffer == GetResID(command->m_Resource))
        candidate.build->submission = NS::RetainPtr(nativeCommand);
      continue;
    }
    // Encoder-local copies are immutable even when a later encoder in this CB
    // overwrites the input. Materialise only after this submission completes.
    if((candidate.build->kind == 1 || candidate.build->kind == 2 || candidate.build->kind == 8 || candidate.build->kind == 9 || candidate.build->kind == 11) && candidate.build->inputReadback &&
       (candidate.build->kind == 1 || candidate.build->kind == 9 || candidate.build->kind == 11 || candidate.build->indexReadback) &&
       candidate.build->submission.get() == nativeCommand)
    {
      if(structure->m_LastBuildCommandBuffer != GetResID(command->m_Resource))
      {
        candidate.build->submission.reset();
        candidate.build->inputReadback.reset(); candidate.build->indexReadback.reset();
      }
      else if(candidate.build->kind == 9)
      {
        candidate.build->childBuilds.clear();
        for(ResourceId child : candidate.build->children)
        {
          auto childObject = GetResourceManager()->GetResource(child, true);
          auto recipe = childObject && childObject->m_Type == eResAccelerationStructure ?
              ((WrappedMTLAccelerationStructure *)childObject)->m_CapturedInitialBuild : nullptr;
          if(recipe && recipe->submission && !recipe->submission->error() &&
             (recipe->submission->status() == MTL::CommandBufferStatusCompleted ||
              recipe->submission->commandQueue() == nativeCommand->commandQueue()))
            candidate.build->childBuilds.push_back(recipe);
          else candidate.build->childBuilds.push_back(nullptr);
        }
      }
      continue;
    }
    // GPU-visible bytes may differ from Managed CPU contents, even after an earlier
    // producer completes. Copy at the end of this AS-only submission, before any
    // later command buffer can overwrite inputs. Scratch/query alias writes prohibit
    // this path; no arbitrary application callback is waited on here.
    const bool gpuInput = input && (candidate.build->kind == 8 || input->storageMode() != MTL::StorageModeShared ||
        (candidate.indexInput && candidate.indexInput->storageMode() != MTL::StorageModeShared) ||
        !ready);
    if(gpuInput && ordered && structure->m_LastBuildCommandBuffer == GetResID(command->m_Resource) &&
       !input->heap() && input->length() && input->length() <= 64*1024*1024 &&
       !hasInputWrite(input, candidate.build->source) &&
       (candidate.build->kind == 8 ? ValidMetalASMultiIndexedParameters(p, input->length(),
           candidate.indexInput ? candidate.indexInput->length() : 0) :
           p.size() == (candidate.build->kind == 2 ? 10U : 8U)) &&
       (candidate.build->kind == 1 || candidate.build->kind == 2 ||
        candidate.build->kind == 3 || candidate.build->kind == 5 || candidate.build->kind == 8))
    {
      bool valid = candidate.refitSource == ResourceId();
      if(candidate.build->kind == 5)
      {
        const auto &children = candidate.build->children;
        valid &= !children.empty() && children.size() <= 4;
        std::set<ResourceId> ids;
        for(ResourceId child : children)
        {
          auto childObject = GetResourceManager()->GetResource(child, true);
          auto childStructure = childObject && childObject->m_Type == eResAccelerationStructure ?
              (WrappedMTLAccelerationStructure *)childObject : NULL;
          auto build = childStructure ? childStructure->m_CapturedInitialBuild : nullptr;
          if(child == candidate.target || !ids.insert(child).second || !build ||
             (build->kind != 1 && build->kind != 2 && build->kind != 3 && build->kind != 8) || !build->submission ||
             build->submission->error() ||
             (build->vertices.empty() && !build->inputReadback) ||
             (build->submission->status() != MTL::CommandBufferStatusCompleted &&
              build->submission->commandQueue() != nativeCommand->commandQueue()))
          { valid = false; break; }
          candidate.build->childBuilds.push_back(build);
        }
      }
      auto index = candidate.indexInput.get();
      valid &= (candidate.build->kind != 2 && candidate.build->kind != 8) || (index && !index->heap() &&
          index->length() && index->length() <= 64*1024*1024 &&
          !hasInputWrite(index, candidate.build->indexSource));
      if(valid)
      {
        auto readback = NS::TransferPtr(Unwrap(this)->newBuffer(input->length(), MTL::ResourceStorageModeShared));
        NS::SharedPtr<MTL::Buffer> indexReadback;
        if(candidate.build->kind == 2 || candidate.build->kind == 8)
          indexReadback = NS::TransferPtr(Unwrap(this)->newBuffer(index->length(), MTL::ResourceStorageModeShared));
        if(readback && ((candidate.build->kind != 2 && candidate.build->kind != 8) || indexReadback))
        {
          auto blit = nativeCommand->blitCommandEncoder();
          if(blit)
          {
            blit->copyFromBuffer(input, 0, readback.get(), 0, input->length());
            if(indexReadback) blit->copyFromBuffer(index, 0, indexReadback.get(), 0, index->length());
            blit->endEncoding();
            candidate.build->inputReadback = readback;
            candidate.build->indexReadback = indexReadback;
            candidate.build->submission = NS::RetainPtr(nativeCommand);
          }
        }
      }
      continue;
    }
    if(getenv("RENDERDOC_METAL_TRACE_INITIAL_AS"))
      fprintf(stderr, "Metal AS initial candidate id=%s ready=%d input=%d parameters=%zu command=%s last=%s\n",
          ToStr(candidate.target).c_str(), ready, input != NULL, p.size(),
          ToStr(GetResID(command->m_Resource)).c_str(), ToStr(structure->m_LastBuildCommandBuffer).c_str());
    if(!ready || structure->m_LastBuildCommandBuffer != GetResID(command->m_Resource) ||
       !input || input->storageMode() != MTL::StorageModeShared ||
       input->heap() || !input->contents() || hasInputWrite(input, candidate.build->source) ||
       p.size() != (candidate.build->kind == 2 ? 10U : 8U) || p[0] > input->length())
      continue;
    if(candidate.refitSource != ResourceId() &&
       (!candidate.priorBuild || priorBuilds[candidate.refitSource] != candidate.priorBuild)) continue;
    if(candidate.priorBuild &&
       (candidate.priorBuild->parameters != p ||
        candidate.priorBuild->kind != candidate.build->kind ||
        candidate.priorBuild->vertices.empty() || !candidate.priorBuild->submission ||
        candidate.priorBuild->submission->status() != MTL::CommandBufferStatusCompleted ||
        candidate.priorBuild->submission->error())) continue;
    if(candidate.build->kind == 2)
    {
      auto indexInput = candidate.indexInput.get();
      const uint64_t indexBytes = p[9] == uint64_t(MTL::IndexTypeUInt16) ? 2 :
                                  p[9] == uint64_t(MTL::IndexTypeUInt32) ? 4 : 0;
      if(!indexInput || !indexInput->contents() || indexInput->heap() ||
         indexInput->storageMode() != MTL::StorageModeShared ||
         hasInputWrite(indexInput, candidate.build->indexSource) || !indexBytes ||
         !p[3] || p[3] > 1000000 || p[8] > indexInput->length() ||
         p[3] * 3 * indexBytes > indexInput->length() - p[8]) continue;
      bytebuf indices((byte *)indexInput->contents() + p[8], size_t(p[3] * 3 * indexBytes));
      uint64_t bytes = 0;
      const uint64_t vertexSize = p[2] == uint64_t(MTL::AttributeFormatFloat4) ? 16 : 12;
      if(!MetalASInitialIndexedSpan(p, indices, bytes) ||
         bytes + p[1] - vertexSize > 64*1024*1024 ||
         bytes + p[1] - vertexSize > input->length() - p[0]) continue;
      candidate.build->vertices = bytebuf((byte *)input->contents() + p[0], size_t(bytes));
      candidate.build->indices = indices;
      candidate.build->submission = NS::RetainPtr(Unwrap((WrappedMTLCommandBuffer *)command->m_Resource));
      continue;
    }
    if(candidate.build->kind == 11)
    {
      if(!candidate.build->children.empty() || !PackMetalASInactiveInstances(
          (const byte *)input->contents(), input->length(), p, candidate.build->vertices)) continue;
      candidate.build->submission = NS::RetainPtr(nativeCommand);
      continue;
    }
    if(candidate.build->kind == 5 || candidate.build->kind == 9)
    {
      const auto &children = candidate.build->children;
      uint64_t packedBytes = 0;
      const bool span = candidate.build->kind == 9 ?
          MetalASIndirectInstanceSpan(p, input->length(), packedBytes) :
          MetalASInitialInstanceSpan(p, input->length(), packedBytes);
      if(!span ||
         children.empty() || children.size() >
             (candidate.build->kind == 9 ? MetalMaxIndirectASChildren : 4)) continue;
      bool valid = true;
      std::set<ResourceId> ids;
      for(ResourceId child : children)
      {
        auto childObject = GetResourceManager()->GetResource(child, true);
        auto childStructure = childObject && childObject->m_Type == eResAccelerationStructure ?
            (WrappedMTLAccelerationStructure *)childObject : NULL;
        auto build = childStructure ? childStructure->m_CapturedInitialBuild : nullptr;
        // Shared TLAS inputs can reference a completed GPU-frozen BLAS recipe.
        // Materialise only an already-completed submission; never wait here.
        MaterialiseMetalASInitialBuild(build);
        if(child == candidate.target || !ids.insert(child).second || !build ||
           (build->kind != 1 && build->kind != 2 && build->kind != 3 && build->kind != 8) ||
           build->vertices.empty() || !build->submission ||
           (build->submission->status() != MTL::CommandBufferStatusCompleted &&
            build->submission.get() != nativeCommand) || build->submission->error())
        { valid = false; break; }
        // Candidates are visited in encoding order. Same-submission children
        // must already own frozen bytes and a submission; later/missing builds
        // cannot satisfy this proof. The retained recipe is restored before TLAS.
        candidate.build->childBuilds.push_back(build);
      }
      bytebuf instances;
      if(!valid) continue;
      const bool packed = candidate.build->kind == 9 ?
          PackMetalASIndirectInstances((const byte *)input->contents(), input->length(), p,
              candidate.build->childGPUIdentities, instances) :
          PackMetalASInitialInstances((const byte *)input->contents(), input->length(), p,
              children.size(), instances);
      if(!packed) continue;
      candidate.build->vertices = instances;
      candidate.build->submission = NS::RetainPtr(Unwrap((WrappedMTLCommandBuffer *)command->m_Resource));
      continue;
    }
    if(candidate.build->kind == 3)
    {
      uint64_t bytes = 0;
      if(!candidate.build->children.empty() || !MetalASInitialBoxSpan(p, bytes) ||
         bytes + p[1] - 24 > input->length() - p[0]) continue;
      bytebuf boxes((byte *)input->contents() + p[0], size_t(bytes));
      if(!ValidMetalASInitialBoxes(p, boxes)) continue;
      candidate.build->vertices = boxes;
      candidate.build->submission = NS::RetainPtr(nativeCommand);
      continue;
    }
    if(candidate.build->kind != 1 || !candidate.build->children.empty() ||
       p[3] == 0 || p[3] > 1000000 || p[1] < 12 || p[1] > 1024*1024 ||
       p[1] % 4 || p[0] % 4 || p[4] > 31 || p[5] > 1 || p[6] > 1 ||
       (p[7] != 0 && p[7] != uint64_t(MTL::AccelerationStructureUsageRefit))) continue;
    const uint64_t vertexSize = p[2] == uint64_t(MTL::AttributeFormatFloat3) ? 12 :
                                p[2] == uint64_t(MTL::AttributeFormatFloat4) ? 16 : 0;
    if(!vertexSize || p[1] < vertexSize) continue;
    const uint64_t bytes = (p[3] * 3 - 1) * p[1] + vertexSize;
    if(bytes + p[1] - vertexSize > 64*1024*1024 ||
       bytes + p[1] - vertexSize > input->length() - p[0]) continue;
    candidate.build->vertices = bytebuf((byte *)input->contents() + p[0], size_t(bytes));
    candidate.build->submission = NS::RetainPtr(Unwrap((WrappedMTLCommandBuffer *)command->m_Resource));
  }
  candidates.clear();
}

void WriteMetalASFrameBuild(WriteSerialiser &fileSer, const MetalASFrameBuild &evidence)
{
  const auto &build = evidence.build;
  if(build && (build->kind == 1 || build->kind == 2 || build->kind == 8))
  {
    const bool valid = MaterialiseMetalASInitialBuild(build);
    bytebuf vertexBytes = valid ? build->vertices : bytebuf();
    bytebuf indexBytes = valid ? build->indices : bytebuf();
    if(!valid) RDCERR("Metal frame geometry snapshot missing or invalid; replay will reject this build");
    WriteSerialiser ser(new StreamWriter(1024), Ownership::Stream);
    ser.SetUserData(fileSer.GetUserData());
    auto metadata = evidence.metadata; metadata.chunkID = 0;
    ser.ChunkMetadata() = metadata;
    ser.SetChunkMetadataRecording(evidence.metadataFlags);
    SCOPED_SERIALISE_CHUNK(build->kind == 8 ? MetalChunk::MTLAccelerationStructureCommandEncoder_buildFrozenMultiIndexed :
        MetalChunk::MTLAccelerationStructureCommandEncoder_buildFrozenTriangles,
        1024 + build->parameters.size() * sizeof(uint64_t) + vertexBytes.size() + indexBytes.size());
    SERIALISE_ELEMENT_LOCAL(Encoder, evidence.encoder);
    SERIALISE_ELEMENT_LOCAL(structure, evidence.target).Important();
    SERIALISE_ELEMENT_LOCAL(vertices, evidence.source).Important();
    SERIALISE_ELEMENT_LOCAL(indices, build->indexSource).Important();
    SERIALISE_ELEMENT_LOCAL(kind, build->kind).Important();
    SERIALISE_ELEMENT_LOCAL(parameters, build->parameters).Important();
    SERIALISE_ELEMENT_LOCAL(scratch, evidence.scratch).Important();
    SERIALISE_ELEMENT_LOCAL(scratchOffset, evidence.scratchOffset).Important();
    SERIALISE_ELEMENT(vertexBytes).Important();
    SERIALISE_ELEMENT(indexBytes).Important();
    Chunk *chunk = scope.Get(); chunk->Write(fileSer); chunk->Delete();
    return;
  }
  bytebuf descriptorBytes;
  rdcarray<ResourceId> children;
  bool valid = build && build->submission &&
      build->submission->status() == MTL::CommandBufferStatusCompleted && !build->submission->error();
  const auto input = build ? build->inputReadback.get() : NULL;
  uint64_t bytes = 0;
  bytebuf raw;
  valid = valid && build->parameters.size() == 8 &&
      build->children.size() == build->childGPUIdentities.size();
  if(valid && !build->vertices.empty()) raw = build->vertices;
  else if(valid && input && input->contents() &&
      MetalASIndirectInstanceSpan(build->parameters, input->length(), bytes))
  {
    raw.resize(size_t(bytes));
    for(uint64_t i = 0; i < build->parameters[3]; i++)
      memcpy(raw.data()+i*72, (const byte *)input->contents()+build->parameters[0]+i*build->parameters[1], 72);
  }
  else valid = false;
  valid = valid && MetalASIndirectInstanceSpan(build->parameters, 64*1024*1024, bytes) && raw.size() == bytes;
  if(valid)
  {
    if(ValidMetalASInactiveInstances(raw, build->parameters[3])) descriptorBytes = raw;
    else
    {
      std::map<uint64_t, size_t> candidates;
      for(size_t i = 0; i < build->children.size(); i++)
        valid &= build->childGPUIdentities[i] && candidates.emplace(build->childGPUIdentities[i], i).second;
      std::set<size_t> seen;
      rdcarray<uint64_t> identities;
      for(uint64_t i = 0; valid && i < build->parameters[3]; i++)
      {
        MTL::IndirectAccelerationStructureInstanceDescriptor packet = {};
        memcpy(&packet, raw.data()+i*72, sizeof(packet));
        auto found = candidates.find(packet.accelerationStructureID._impl);
        if(found == candidates.end()) { valid = false; break; }
        if(seen.insert(found->second).second)
        {
          children.push_back(build->children[found->second]);
          identities.push_back(found->first);
        }
      }
      bytebuf relocated;
      valid &= ConvertMetalASIndirectInstances(raw, build->parameters[3], identities, relocated);
      if(valid) descriptorBytes = raw;
    }
  }
  if(getenv("RENDERDOC_METAL_TRACE_AS_INPUT_SNAPSHOTS"))
  {
    fprintf(stderr, "Metal frame AS frozen packet target=%s source=%s valid=%d kind=%u count=%llu rawBytes=%zu candidateChildren=%zu usedChildren=%zu readback=%d submitted=%d\n",
        ToStr(evidence.target).c_str(), ToStr(evidence.source).c_str(), valid, build ? build->kind : 0,
        (unsigned long long)(build && build->parameters.size() == 8 ? build->parameters[3] : 0),
        raw.size(), build ? build->children.size() : 0, children.size(), input != NULL,
        build && build->submission);
    for(size_t i = 0; i + 72 <= raw.size(); i += 72)
    {
      MTL::IndirectAccelerationStructureInstanceDescriptor packet = {};
      memcpy(&packet, raw.data()+i, sizeof(packet));
      fprintf(stderr, "Metal frame AS instance index=%zu id=%llu mask=%u options=%u\n", i/72,
          (unsigned long long)packet.accelerationStructureID._impl, packet.mask, unsigned(packet.options));
      if(i/72 >= 63) break;
    }
  }
  if(!valid)
  { RDCERR("Metal frame indirect AS snapshot missing or invalid; replay will reject this build"); children.clear(); descriptorBytes.clear(); }
  // Chunk metadata flags may only change on a fresh scratch stream. The file
  // serialiser has already emitted the initial state and prior frame chunks.
  WriteSerialiser ser(new StreamWriter(1024), Ownership::Stream);
  ser.SetUserData(fileSer.GetUserData());
  auto metadata = evidence.metadata; metadata.chunkID = 0;
  ser.ChunkMetadata() = metadata;
  ser.SetChunkMetadataRecording(evidence.metadataFlags);
  {
    SCOPED_SERIALISE_CHUNK(evidence.scratchOffset ? MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstancesWithScratchOffset :
        MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstances,
        1024 + children.size()*8 + descriptorBytes.size());
    // ResourceId has the same wire encoding as the wrapped resource fields.
    SERIALISE_ELEMENT_LOCAL(Encoder, evidence.encoder);
    SERIALISE_ELEMENT_LOCAL(structure, evidence.target).Important();
    SERIALISE_ELEMENT(children).Important();
    SERIALISE_ELEMENT_LOCAL(instances, evidence.source).Important();
    SERIALISE_ELEMENT_LOCAL(scratch, evidence.scratch).Important();
    SERIALISE_ELEMENT_LOCAL(parameters, build->parameters).Important();
    SERIALISE_ELEMENT(descriptorBytes).Important();
    if(evidence.scratchOffset)
    {
      SERIALISE_ELEMENT_LOCAL(scratchOffset, evidence.scratchOffset).Important();
    }
    Chunk *chunk = scope.Get();
    chunk->Write(fileSer);
    chunk->Delete();
  }
}
