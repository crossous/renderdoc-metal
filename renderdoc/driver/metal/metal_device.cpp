/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2026 Baldur Karlsson
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

#include "metal_device.h"
#include "metal_blit_command_encoder.h"
#include "metal_argument_encoder.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_command_queue.h"
#include "metal_compute_command_encoder.h"
#include "metal_compute_pipeline_state.h"
#include "metal_depth_stencil_state.h"
#include "metal_function.h"
#include "metal_library.h"
#include "metal_dynamic_library.h"
#include "metal_binary_archive.h"
#include "metal_manager.h"
#include "metal_render_command_encoder.h"
#include "metal_parallel_render_command_encoder.h"
#include "metal_render_pipeline_state.h"
#include "metal_visible_function_table.h"
#include "metal_sampler_state.h"
#include "metal_fence.h"
#include "metal_acceleration_structure.h"
#include "metal_acceleration_structure_command_encoder.h"
#include "metal_heap.h"
#include "metal_rate_map.h"
#include "metal_indirect_command_buffer.h"
#include "metal_replay.h"
#include "metal_texture.h"
#include "os/os_specific.h"
#include <unistd.h>

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_ResourceLabel(SerialiserType &ser, ResourceId resource,
                                            rdcstr label)
{
  SERIALISE_ELEMENT(resource).Important();
  SERIALISE_ELEMENT(label).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    WrappedMTLObject *object = GetResourceManager()->GetResource(resource, true);
    if(!object || (object->m_Type != eResTexture && object->m_Type != eResBuffer))
      return false;
    ResourceDescription &description = GetReplay()->GetResourceDesc(resource);
    // Match D3D12 SetName: the capture's application name is distinct from its generated ID.
    if(!label.empty()) description.SetCustomName(label);
    AddResourceCurChunk(description);
  }
  return true;
}

void WrappedMTLDevice::CaptureResourceLabel(WrappedMTLObject *resource, NS::String *label)
{
  SCOPED_READLOCK(GetCaptureTransitionLock());
  SCOPED_LOCK(GetCaptureSubmissionLock());
  if(!IsCaptureMode(m_State) || !resource || !GetRecord(resource)) return;
  CACHE_THREAD_SERIALISER();
  SCOPED_SERIALISE_CHUNK(MetalChunk::MTLResource_setLabel);
  Serialise_ResourceLabel(ser, GetResID(resource),
                         label && label->utf8String() ? rdcstr(label->utf8String()) : rdcstr());
  MetalResourceRecord *record = GetRecord(resource);
  // Names belong to resource initialisation, as in D3D12, rather than GPU actions. Collapse
  // consecutive updates without dropping any intervening resource operations.
  record->LockChunks();
  while(record->HasChunks() &&
        record->GetLastChunk()->GetChunkType<MetalChunk>() == MetalChunk::MTLResource_setLabel)
  {
    record->GetLastChunk()->Delete();
    record->PopChunk();
  }
  record->UnlockChunks();
  Chunk *chunk = scope.Get();
  record->AddChunk(chunk);
  // Frame-born placement resource records are omitted from the initialisation prefix. Their
  // label must follow their frame creation; retain the record too for subsequent captures.
  if(IsActiveCapturing(m_State) && IsCapturedFrameResource(GetResID(resource)))
    AddFrameCaptureRecordChunk(chunk->Duplicate());
}

template bool WrappedMTLDevice::Serialise_ResourceLabel(ReadSerialiser &, ResourceId, rdcstr);
template bool WrappedMTLDevice::Serialise_ResourceLabel(WriteSerialiser &, ResourceId, rdcstr);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newFence(SerialiserType &ser, WrappedMTLFence *fence)
{
  SERIALISE_ELEMENT_LOCAL(Fence, GetResID(fence)).TypedAs("MTLFence"_lit);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(Fence == ResourceId() || GetResourceManager()->HasResource(Fence))
      return false;
    MTL::Fence *real = Unwrap(this)->newFence();
    if(!real)
      return false;
    WrappedMTLFence *wrapped = NULL;
    GetResourceManager()->WrapResource(Fence, real, wrapped, true);
    AddResource(Fence, ResourceType::Sync, "Fence");
    DerivedResource(this, Fence);
  }
  return true;
}

WrappedMTLFence *WrappedMTLDevice::newFence()
{
  MTL::Fence *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newFence());
  if(!real)
    return NULL;
  WrappedMTLFence *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newFence);
    Serialise_newFence(ser, wrapped);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLFence *, newFence);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newAccelerationStructureWithSize(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, NS::UInteger size)
{
  SERIALISE_ELEMENT_LOCAL(Structure, GetResID(structure)).TypedAs("MTLAccelerationStructure"_lit);
  SERIALISE_ELEMENT(size).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(Structure == ResourceId() || !size || size > 1024ULL * 1024 * 1024)
    {
      RDCERR("Invalid Metal acceleration structure allocation");
      return false;
    }
    MTL::AccelerationStructure *real = Unwrap(this)->newAccelerationStructure(size);
    if(!real)
      return false;
    WrappedMTLAccelerationStructure *wrapped =
        (WrappedMTLAccelerationStructure *)GetResourceManager()->GetResource(Structure, true);
    if(wrapped)
    {
      if(wrapped->m_Type != eResAccelerationStructure)
      {
        real->release();
        return false;
      }
      GetResourceManager()->ReplaceRealResource(wrapped, real, true);
    }
    else
      GetResourceManager()->WrapResource(Structure, real, wrapped, true);
    wrapped->m_Size = size;
    wrapped->m_LastCompactedSizeBuffer = ResourceId();
    wrapped->m_LastCompactedSizeOffset = 0;
    wrapped->m_LastCompactedSizeType = MTL::DataTypeNone;
    wrapped->m_LastCompactedWriteCommandBuffer = ResourceId();
    wrapped->m_LastBuildKind = 0;
    wrapped->m_LastTriangleCount = 0;
    wrapped->m_LastBuildCommandBuffer = ResourceId();
    if(IsLoading(m_State))
    {
      AddResource(Structure, ResourceType::AccelerationStructure, "Acceleration Structure");
      DerivedResource(this, Structure);
    }
  }
  return true;
}

WrappedMTLAccelerationStructure *WrappedMTLDevice::newAccelerationStructureWithSize(
    NS::UInteger size)
{
  MTL::AccelerationStructure *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newAccelerationStructure(size));
  if(!real)
    return NULL;
  WrappedMTLAccelerationStructure *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->m_Size = size;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newAccelerationStructureWithSize);
    Serialise_newAccelerationStructureWithSize(ser, wrapped, size);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

WrappedMTLAccelerationStructure *WrappedMTLDevice::newAccelerationStructureWithDescriptor(
    MTL::AccelerationStructureDescriptor *descriptor)
{
  if(!descriptor)
    return NULL;
  const MTL::AccelerationStructureSizes sizes =
      Unwrap(this)->accelerationStructureSizes(descriptor);
  const NS::UInteger size = sizes.accelerationStructureSize;
  if(!size || size > 1024ULL * 1024 * 1024)
    return NULL;
  MTL::AccelerationStructure *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newAccelerationStructure(descriptor));
  if(!real || real->size() != size)
  {
    if(real) real->release();
    RDCERR("Metal descriptor AS allocation does not match queried size");
    return NULL;
  }
  WrappedMTLAccelerationStructure *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->m_Size = size;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newAccelerationStructureWithDescriptor);
    Serialise_newAccelerationStructureWithSize(ser, wrapped, size);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLAccelerationStructure *,
                                            newAccelerationStructureWithSize, NS::UInteger size);

// Six scalar fields per descriptor avoid serialising Objective-C objects or process-local pointers.
// Device-created argument buffers currently support only sampled 2D textures and samplers.
static bool ValidArgumentDescriptors(const rdcarray<uint64_t> &fields)
{
  if(fields.empty() || fields.size() > 16 * 6 || fields.size() % 6)
    return false;
  bool occupied[32] = {};
  for(size_t i = 0; i < fields.size(); i += 6)
  {
    const uint64_t index = fields[i], type = fields[i + 1], count = fields[i + 2];
    if(index >= 32 || !count || count > 32 - index ||
       (type != MTL::DataTypeTexture && type != MTL::DataTypeSampler) ||
       fields[i + 3] != MTL::BindingAccessReadOnly ||
       (type == MTL::DataTypeTexture && fields[i + 4] != MTL::TextureType2D) ||
       fields[i + 5] != 0)
      return false;
    for(uint64_t member = index; member < index + count; member++)
    {
      if(occupied[member])
        return false;
      occupied[member] = true;
    }
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newArgumentEncoderWithArguments(
    SerialiserType &ser, WrappedMTLArgumentEncoder *encoder, rdcarray<uint64_t> descriptors)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, GetResID(encoder)).TypedAs("MTLArgumentEncoder"_lit).Important();
  SERIALISE_ELEMENT(descriptors).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(Encoder == ResourceId() || GetResourceManager()->HasResource(Encoder) ||
       !ValidArgumentDescriptors(descriptors))
    {
      RDCERR("Invalid or unsupported Metal device argument encoder descriptors");
      return false;
    }
    rdcarray<MTL::ArgumentDescriptor *> native;
    for(size_t i = 0; i < descriptors.size(); i += 6)
    {
      MTL::ArgumentDescriptor *entry = MTL::ArgumentDescriptor::alloc()->init();
      entry->setIndex(descriptors[i]);
      entry->setDataType(MTL::DataType(descriptors[i + 1]));
      entry->setArrayLength(descriptors[i + 2]);
      entry->setAccess(MTL::BindingAccess(descriptors[i + 3]));
      entry->setTextureType(MTL::TextureType(descriptors[i + 4]));
      native.push_back(entry);
    }
    NS::Array *arguments = NS::Array::array((const NS::Object *const *)native.data(), native.size());
    MTL::ArgumentEncoder *real = Unwrap(this)->newArgumentEncoder(arguments);
    for(MTL::ArgumentDescriptor *entry : native)
      entry->release();
    if(!real)
    {
      RDCERR("Metal failed to recreate device argument encoder");
      return false;
    }
    WrappedMTLArgumentEncoder *wrapped = NULL;
    GetResourceManager()->WrapResource(Encoder, real, wrapped, true);
    wrapped->ConfigureDescriptorLayout(descriptors);
    AddResource(Encoder, ResourceType::StateObject, "Argument Encoder");
    DerivedResource(this, Encoder);
  }
  return true;
}

WrappedMTLArgumentEncoder *WrappedMTLDevice::newArgumentEncoderWithArguments(const NS::Array *arguments)
{
  MTL::ArgumentEncoder *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newArgumentEncoder(arguments));
  if(!real)
    return NULL;
  WrappedMTLArgumentEncoder *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    rdcarray<uint64_t> descriptors;
    for(NS::UInteger i = 0; arguments && i < arguments->count(); i++)
    {
      MTL::ArgumentDescriptor *entry = arguments->object<MTL::ArgumentDescriptor>(i);
      descriptors.push_back(entry->index());
      descriptors.push_back(entry->dataType());
      descriptors.push_back(entry->arrayLength());
      descriptors.push_back(entry->access());
      descriptors.push_back(entry->textureType());
      descriptors.push_back(entry->constantBlockAlignment());
    }
    wrapped->ConfigureDescriptorLayout(descriptors);
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newArgumentEncoderWithArguments);
    Serialise_newArgumentEncoderWithArguments(ser, wrapped, descriptors);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(this));
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newArgumentEncoderWithArguments(
    ReadSerialiser &ser, WrappedMTLArgumentEncoder *encoder, rdcarray<uint64_t> descriptors);
template bool WrappedMTLDevice::Serialise_newArgumentEncoderWithArguments(
    WriteSerialiser &ser, WrappedMTLArgumentEncoder *encoder, rdcarray<uint64_t> descriptors);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newArgumentEncoderWithBufferBinding(
    SerialiserType &ser, WrappedMTLArgumentEncoder *encoder, rdcarray<uint64_t> descriptors,
    uint64_t encodedLength, uint64_t alignment, bool supported)
{
  SERIALISE_ELEMENT_LOCAL(Encoder, GetResID(encoder)).TypedAs("MTLArgumentEncoder"_lit).Important();
  SERIALISE_ELEMENT(descriptors).Important();
  SERIALISE_ELEMENT(encodedLength).Important();
  SERIALISE_ELEMENT(alignment).Important();
  SERIALISE_ELEMENT(supported).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!supported || Encoder == ResourceId() || GetResourceManager()->HasResource(Encoder) ||
       !ValidArgumentDescriptors(descriptors) || !encodedLength || !alignment)
    {
      RDCERR("Invalid or unsupported Metal buffer-binding argument encoder");
      return false;
    }
    rdcarray<MTL::ArgumentDescriptor *> native;
    for(size_t i = 0; i < descriptors.size(); i += 6)
    {
      MTL::ArgumentDescriptor *entry = MTL::ArgumentDescriptor::alloc()->init();
      entry->setIndex(descriptors[i]);
      entry->setDataType(MTL::DataType(descriptors[i + 1]));
      entry->setArrayLength(descriptors[i + 2]);
      entry->setAccess(MTL::BindingAccess(descriptors[i + 3]));
      entry->setTextureType(MTL::TextureType(descriptors[i + 4]));
      native.push_back(entry);
    }
    NS::Array *arguments = NS::Array::array((const NS::Object *const *)native.data(),native.size());
    MTL::ArgumentEncoder *real = Unwrap(this)->newArgumentEncoder(arguments);
    for(MTL::ArgumentDescriptor *entry : native) entry->release();
    if(!real || real->encodedLength() != encodedLength || real->alignment() != alignment)
    {
      if(real) real->release();
      RDCERR("Metal buffer-binding argument layout differs on replay");
      return false;
    }
    WrappedMTLArgumentEncoder *wrapped = NULL;
    GetResourceManager()->WrapResource(Encoder,real,wrapped,true);
    wrapped->ConfigureDescriptorLayout(descriptors);
    AddResource(Encoder,ResourceType::StateObject,"Argument Encoder");
    DerivedResource(this,Encoder);
  }
  return true;
}

WrappedMTLArgumentEncoder *WrappedMTLDevice::newArgumentEncoderWithBufferBinding(
    MTL::BufferBinding *binding)
{
  MTL::ArgumentEncoder *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newArgumentEncoder(binding));
  if(!real) return NULL;
  WrappedMTLArgumentEncoder *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(),real,wrapped);
  if(IsCaptureMode(m_State))
  {
    rdcarray<uint64_t> descriptors;
    bool supported = binding && binding->type() == MTL::BindingTypeBuffer &&
                     binding->access() == MTL::BindingAccessReadOnly &&
                     binding->bufferDataType() == MTL::DataTypeStruct &&
                     binding->bufferDataSize() == real->encodedLength() &&
                     binding->bufferAlignment() == real->alignment();
    MTL::StructType *structure = supported ? binding->bufferStructType() : NULL;
    NS::Array *members = structure ? structure->members() : NULL;
    supported &= members && members->count() > 0 && members->count() <= 16;
    for(NS::UInteger i = 0; supported && i < members->count(); i++)
    {
      MTL::StructMember *member = members->object<MTL::StructMember>(i);
      MTL::DataType type = member->dataType();
      if(member->argumentIndex() != i || member->offset() != i * sizeof(uint64_t) ||
         (type != MTL::DataTypeTexture && type != MTL::DataTypeSampler))
      {
        supported = false;
        break;
      }
      MTL::TextureType textureType = MTL::TextureType2D;
      if(type == MTL::DataTypeTexture)
      {
        MTL::TextureReferenceType *reference = member->textureReferenceType();
        if(!reference || reference->textureType() != MTL::TextureType2D ||
           reference->access() != MTL::BindingAccessReadOnly)
        {
          supported = false;
          break;
        }
      }
      descriptors.push_back(i);
      descriptors.push_back(type);
      descriptors.push_back(1);
      descriptors.push_back(MTL::BindingAccessReadOnly);
      descriptors.push_back(textureType);
      descriptors.push_back(0);
    }
    supported &= ValidArgumentDescriptors(descriptors);
    if(supported) wrapped->ConfigureDescriptorLayout(descriptors);
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newArgumentEncoderWithBufferBinding);
    Serialise_newArgumentEncoderWithBufferBinding(ser,wrapped,descriptors,
                                                 real->encodedLength(),real->alignment(),supported);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(this));
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newArgumentEncoderWithBufferBinding(
    ReadSerialiser &, WrappedMTLArgumentEncoder *, rdcarray<uint64_t>, uint64_t, uint64_t, bool);
template bool WrappedMTLDevice::Serialise_newArgumentEncoderWithBufferBinding(
    WriteSerialiser &, WrappedMTLArgumentEncoder *, rdcarray<uint64_t>, uint64_t, uint64_t, bool);

WrappedMTLDevice::WrappedMTLDevice(MTL::Device *realMTLDevice, ResourceId objId)
    : WrappedMTLObject(realMTLDevice, objId, this, GetStateRef()), m_Capturer(*this)
{
  m_Device = this;
  m_Type = eResDevice;

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = CaptureState::LoadingReplaying;
  }
  else
  {
    m_State = CaptureState::BackgroundCapturing;
  }

  if(realMTLDevice && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);

  m_SectionVersion = MetalInitParams::CurrentVersion;

  threadSerialiserTLSSlot = Threading::AllocateTLSSlot();

  m_ResourceManager = new MetalResourceManager(m_State, this);

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_Replay = new MetalReplay(this);
    m_StructuredFile = m_StoredStructuredData = new SDFile;

    m_DummyBuffer = new WrappedMTLBuffer(NULL, ResourceId(), this);
    m_DummyReplayHeap = new WrappedMTLHeap(NULL, ResourceId(), this);
    m_DummyReplayRateMap = new WrappedMTLRasterizationRateMap(NULL, ResourceId(), this);
    m_DummyReplayTexture = new WrappedMTLTexture(NULL, ResourceId(), this);
    m_DummyReplayCommandBuffer = new WrappedMTLCommandBuffer(NULL, ResourceId(), this);
    m_DummyReplayCommandQueue = new WrappedMTLCommandQueue(NULL, ResourceId(), this);
    m_DummyReplayLibrary = new WrappedMTLLibrary(NULL, ResourceId(), this);
    m_DummyReplayBinaryArchive = new WrappedMTLBinaryArchive(NULL, ResourceId(), this);
    m_DummyReplayRenderPipelineState =
        new WrappedMTLRenderPipelineState(NULL, ResourceId(), this);
    m_DummyReplayComputePipelineState =
        new WrappedMTLComputePipelineState(NULL, ResourceId(), this);
    m_DummyReplayVisibleFunctionTable =
        new WrappedMTLVisibleFunctionTable(NULL, ResourceId(), this);
    m_DummyReplayIntersectionFunctionTable =
        new WrappedMTLIntersectionFunctionTable(NULL, ResourceId(), this);
    m_DummyReplayRenderCommandEncoder =
        new WrappedMTLRenderCommandEncoder(NULL, ResourceId(), this);
    m_DummyReplayParallelRenderCommandEncoder =
        new WrappedMTLParallelRenderCommandEncoder(NULL, ResourceId(), this);
    m_DummyReplayBlitCommandEncoder = new WrappedMTLBlitCommandEncoder(NULL, ResourceId(), this);
    m_DummyReplayAccelerationStructureCommandEncoder =
        new WrappedMTLAccelerationStructureCommandEncoder(NULL, ResourceId(), this);
    m_DummyReplayComputeCommandEncoder =
        new WrappedMTLComputeCommandEncoder(NULL, ResourceId(), this);
    m_DummyReplayArgumentEncoder = new WrappedMTLArgumentEncoder(NULL, ResourceId(), this);
    m_DummyReplayIndirectCommandBuffer =
        new WrappedMTLIndirectCommandBuffer(NULL, ResourceId(), this);
    m_DummyReplayIndirectRenderCommand =
        new WrappedMTLIndirectRenderCommand(NULL, ResourceId(), this);
  }

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_FrameCaptureRecord = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    m_FrameCaptureRecord->DataInSerialiser = false;
    m_FrameCaptureRecord->Length = 0;
    m_FrameCaptureRecord->InternalResource = true;
  }
  else
  {
    m_FrameCaptureRecord = NULL;

    ResourceIDGen::SetReplayResourceIDs();
  }

  RDCASSERT(m_Device == this);
  GetResourceManager()->AddResource(objId, this);

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCreateSystemDefaultDevice);
      Serialise_MTLCreateSystemDefaultDevice(ser);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(this);
    record->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
  }

  if(realMTLDevice)
  {
    RenderDoc::Inst().AddDeviceFrameCapturer(this, &m_Capturer);
    m_mtlCommandQueue = Unwrap(this)->newCommandQueue();
    FirstFrame();
  }
}

WrappedMTLDevice::~WrappedMTLDevice()
{
  for(MTL::CommandBuffer *buffer : m_CapturePendingGPU)
    buffer->release();
  m_CapturePendingGPU.clear();
  ClearDescriptorHistorySnapshot();
  for(const DescriptorHistoryChunk &entry : m_DescriptorHistory)
    entry.chunk->Delete();
  m_DescriptorHistory.clear();
  // A malformed capture can abort initial replay before its pending command buffer reaches the
  // normal completion path. Keep the encoders and their resources alive until the GPU is done.
  if(m_ReplayCommandBuffer || m_ReplayRenderCommandEncoder || m_ReplayComputeCommandEncoder ||
     m_ReplayBlitCommandEncoder || m_ReplayAccelerationStructureCommandEncoder ||
     m_ReplayParallelRenderCommandEncoder)
    FinishReplayCommands();
  ReleaseReplayDiscardResources();
  SAFE_DELETE(m_FrameReader);
  SAFE_DELETE(m_DummyReplayArgumentEncoder);
  SAFE_DELETE(m_DummyReplayIndirectRenderCommand);
  SAFE_DELETE(m_DummyReplayIndirectCommandBuffer);
  SAFE_DELETE(m_DummyReplayBlitCommandEncoder);
  SAFE_DELETE(m_DummyReplayAccelerationStructureCommandEncoder);
  SAFE_DELETE(m_DummyReplayComputeCommandEncoder);
  SAFE_DELETE(m_DummyReplayRenderCommandEncoder);
  SAFE_DELETE(m_DummyReplayParallelRenderCommandEncoder);
  SAFE_DELETE(m_DummyReplayVisibleFunctionTable);
  SAFE_DELETE(m_DummyReplayIntersectionFunctionTable);
  SAFE_DELETE(m_DummyReplayRenderPipelineState);
  SAFE_DELETE(m_DummyReplayComputePipelineState);
  SAFE_DELETE(m_DummyReplayLibrary);
  SAFE_DELETE(m_DummyReplayBinaryArchive);
  SAFE_DELETE(m_DummyReplayCommandQueue);
  SAFE_DELETE(m_DummyReplayCommandBuffer);
  SAFE_DELETE(m_DummyReplayTexture);
  SAFE_DELETE(m_DummyBuffer);
  SAFE_DELETE(m_DummyReplayHeap);
  SAFE_DELETE(m_DummyReplayRateMap);
  SAFE_DELETE(m_Replay);
  SAFE_DELETE(m_StoredStructuredData);
  if(m_ResourceManager && IsReplayMode(m_State))
    m_ResourceManager->Shutdown();
  SAFE_DELETE(m_ResourceManager);
  if(m_mtlCommandQueue)
    m_mtlCommandQueue->release();
  if(m_Real)
    ((MTL::Device *)m_Real)->release();
  m_Real = NULL;
}

IMP WrappedMTLDevice::g_real_CAMetalLayer_nextDrawable;
IMP WrappedMTLDevice::g_real_CAMetalDrawable_texture;
IMP WrappedMTLDevice::g_real_CAMetalDrawable_present;
uint64_t WrappedMTLDevice::g_nextDrawableTLSSlot;

MTL::Texture *hooked_CAMetalDrawable_texture(id self, SEL _cmd)
{
  WrappedMTLTexture *texture = WrappedMTLDevice::GetDrawableTexture((MTL::Drawable *)self);
  if(texture)
    return (MTL::Texture *)texture;

  return ((MTL::Texture * (*)(id, SEL))WrappedMTLDevice::g_real_CAMetalDrawable_texture)(self,
                                                                                       _cmd);
}

void hooked_CAMetalDrawable_present(id self, SEL _cmd)
{
  WrappedMTLTexture *texture = WrappedMTLDevice::GetDrawableTexture((MTL::Drawable *)self);
  ((void (*)(id, SEL))WrappedMTLDevice::g_real_CAMetalDrawable_present)(self, _cmd);
  // commandBuffer.presentDrawable already records its presentation and removes this
  // drawable from the lookup. Only handle applications that call drawable.present().
  if(texture)
    texture->m_Device->PresentDrawable((CA::MetalDrawable *)self);
}

CA::MetalDrawable *hooked_CAMetalLayer_nextDrawable(id self, SEL _cmd)
{
  CA::MetalLayer *mtlLayer = (CA::MetalLayer *)self;
  MTL::Device *mtlDevice = mtlLayer->device();
  WrappedMTLDevice *device = GetWrapped(mtlDevice);
  RDCASSERT(object_getClass(mtlDevice) == objc_getClass("ObjCBridgeMTLDevice"));
  device->RegisterMetalLayer(mtlLayer);
  mtlLayer->setFramebufferOnly(false);

  RDCASSERTEQUAL(Threading::GetTLSValue(WrappedMTLDevice::g_nextDrawableTLSSlot), 0);
  Threading::SetTLSValue(WrappedMTLDevice::g_nextDrawableTLSSlot, (void *)(uintptr_t) true);

  // CAMetalLayer creates its drawable texture internally. On newer Metal runtimes this also uses
  // residency sets, and passing our proxy device into that private allocation path can leak a
  // wrapped texture into Apple's real residency set. Use the real device only for the duration of
  // nextDrawable(), then restore the proxy which applications expect to retrieve from the layer.
  mtlLayer->setDevice(Unwrap(device));
  CA::MetalDrawable *caMtlDrawable =
      ((CA::MetalDrawable * (*)(id, SEL)) WrappedMTLDevice::g_real_CAMetalLayer_nextDrawable)(self,
                                                                                              _cmd);
  mtlLayer->setDevice((MTL::Device *)device);

  if(caMtlDrawable)
  {
    static bool s_hookDrawableTexture = false;
    if(!s_hookDrawableTexture)
    {
      Method textureMethod = class_getInstanceMethod(object_getClass(caMtlDrawable),
                                                     sel_registerName("texture"));
      WrappedMTLDevice::g_real_CAMetalDrawable_texture =
          method_setImplementation(textureMethod, (IMP)hooked_CAMetalDrawable_texture);
      Method presentMethod = class_getInstanceMethod(object_getClass(caMtlDrawable),
                                                    sel_registerName("present"));
      WrappedMTLDevice::g_real_CAMetalDrawable_present =
          method_setImplementation(presentMethod, (IMP)hooked_CAMetalDrawable_present);
      s_hookDrawableTexture = true;
    }

    MTL::Texture *realTexture =
        ((MTL::Texture * (*)(id, SEL))WrappedMTLDevice::g_real_CAMetalDrawable_texture)(
            (id)caMtlDrawable, sel_registerName("texture"));
    device->RegisterDrawableInfo(caMtlDrawable, realTexture);
  }
  Threading::SetTLSValue(WrappedMTLDevice::g_nextDrawableTLSSlot, (void *)(uintptr_t) false);
  return caMtlDrawable;
}

void WrappedMTLDevice::MTLHookObjcMethods()
{
  static bool s_hookObjcMethods = false;
  if(s_hookObjcMethods)
    return;

  g_nextDrawableTLSSlot = Threading::AllocateTLSSlot();
  Threading::SetTLSValue(WrappedMTLDevice::g_nextDrawableTLSSlot, (void *)(uintptr_t) false);

  Method m =
      class_getInstanceMethod(objc_lookUpClass("CAMetalLayer"), sel_registerName("nextDrawable"));
  g_real_CAMetalLayer_nextDrawable =
      method_setImplementation(m, (IMP)hooked_CAMetalLayer_nextDrawable);
  s_hookObjcMethods = true;
}

void WrappedMTLDevice::MTLFixupForMetalDriverAssert()
{
  static bool s_fixupMetalDriverAssert = false;
  if(s_fixupMetalDriverAssert)
    return;

  RDCLOG(
      "Fixup for Metal Driver debug assert. Adding protocol `MTLTextureImplementation` to "
      "`ObjCBridgeMTLTexture`");
  class_addProtocol(objc_lookUpClass("ObjCBridgeMTLTexture"),
                    objc_getProtocol("MTLTextureImplementation"));
  s_fixupMetalDriverAssert = true;
}

// Serialised MTLDevice APIs

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_MTLCreateSystemDefaultDevice(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(Device, GetResID(this)).TypedAs("MTLDevice"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    AddResource(Device, ResourceType::Device, "Device");
  }
  return true;
}

WrappedMTLDevice *WrappedMTLDevice::MTLCreateSystemDefaultDevice(MTL::Device *realMTLDevice)
{
  MTLFixupForMetalDriverAssert();
  MTLHookObjcMethods();
  ResourceId objId = ResourceIDGen::GetNewUniqueID();
  WrappedMTLDevice *wrappedMTLDevice = new WrappedMTLDevice(realMTLDevice, objId);

  return wrappedMTLDevice;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newCommandQueue(SerialiserType &ser, WrappedMTLCommandQueue *queue)
{
  SERIALISE_ELEMENT_LOCAL(Device, this);
  SERIALISE_ELEMENT_LOCAL(CommandQueue, GetResID(queue)).TypedAs("MTLCommandQueue"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    MTL::CommandQueue *realMTLCommandQueue = Unwrap(this)->newCommandQueue();
    WrappedMTLCommandQueue *wrappedMTLCommandQueue;
    GetResourceManager()->WrapResource(CommandQueue, realMTLCommandQueue, wrappedMTLCommandQueue,
                                       true);

    AddResource(CommandQueue, ResourceType::Queue, "Queue");
    DerivedResource(this, CommandQueue);
  }
  return true;
}

WrappedMTLCommandQueue *WrappedMTLDevice::newCommandQueue()
{
  MTL::CommandQueue *realMTLCommandQueue;
  SERIALISE_TIME_CALL(realMTLCommandQueue = Unwrap(this)->newCommandQueue());
  WrappedMTLCommandQueue *wrappedMTLCommandQueue;
  ResourceId id =
      GetResourceManager()->WrapResource(ResourceId(), realMTLCommandQueue, wrappedMTLCommandQueue);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newCommandQueue);
      Serialise_newCommandQueue(ser, wrappedMTLCommandQueue);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLCommandQueue);
    record->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, wrappedMTLCommandQueue);
  }
  return wrappedMTLCommandQueue;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newCommandQueue(SerialiserType &ser,
                                                 WrappedMTLCommandQueue *queue,
                                                 NS::UInteger maxCommandBufferCount)
{
  SERIALISE_ELEMENT_LOCAL(Device, this);
  SERIALISE_ELEMENT_LOCAL(CommandQueue, GetResID(queue)).TypedAs("MTLCommandQueue"_lit);
  SERIALISE_ELEMENT(maxCommandBufferCount).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(maxCommandBufferCount == 0)
    {
      RDCERR("Invalid Metal command queue maximum command-buffer count 0");
      return false;
    }
    MTL::CommandQueue *real = Unwrap(this)->newCommandQueue(maxCommandBufferCount);
    if(!real)
      return false;
    WrappedMTLCommandQueue *wrapped = NULL;
    GetResourceManager()->WrapResource(CommandQueue, real, wrapped, true);
    AddResource(CommandQueue, ResourceType::Queue, "Queue");
    DerivedResource(this, CommandQueue);
  }
  return true;
}

WrappedMTLCommandQueue *WrappedMTLDevice::newCommandQueue(NS::UInteger maxCommandBufferCount)
{
  MTL::CommandQueue *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newCommandQueue(maxCommandBufferCount));
  if(!real)
    return NULL;
  WrappedMTLCommandQueue *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newCommandQueueWithMaxCommandBufferCount);
    Serialise_newCommandQueue(ser, wrapped, maxCommandBufferCount);
    GetResourceManager()->AddResourceRecord(wrapped)->AddChunk(scope.Get());
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newDefaultLibrary(SerialiserType &ser, WrappedMTLLibrary *library)
{
  bytebuf data;
  if(ser.IsWriting())
  {
    NS::String *defaultType = NS::String::string("default", NS::UTF8StringEncoding);
    NS::String *metallibExt = NS::String::string("metallib", NS::UTF8StringEncoding);
    NS::Bundle *mainAppBundle = NS::Bundle::mainBundle();
    NS::String *defaultLibaryPath = mainAppBundle->pathForResource(defaultType, metallibExt);
    NS::Data *fileData = NS::Data::dataWithContentsOfFile(defaultLibaryPath);
    if(fileData && fileData->length())
      data.assign((const byte *)fileData->bytes(), fileData->length());
  }

  SERIALISE_ELEMENT_LOCAL(Device, this);
  SERIALISE_ELEMENT_LOCAL(Library, GetResID(library)).TypedAs("MTLLibrary"_lit);
  SERIALISE_ELEMENT(data);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!Device || Device != this || Library == ResourceId() ||
       GetResourceManager()->HasResource(Library) || data.size() < 4 ||
       memcmp(data.data(), "MTLB", 4) != 0)
    {
      RDCERR("Invalid captured default Metal library");
      return false;
    }
    dispatch_data_t dispatchData = dispatch_data_create(
        data.data(), data.size(), dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    NS::Error *error = NULL;
    MTL::Library *realMTLLibrary = Unwrap(this)->newLibrary(dispatchData, &error);
    dispatch_release(dispatchData);

    if(!realMTLLibrary)
    {
      RDCERR("Failed to recreate captured default Metal library: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }

    WrappedMTLLibrary *wrappedMTLLibrary;
    GetResourceManager()->WrapResource(Library, realMTLLibrary, wrappedMTLLibrary, true);
    AddResource(Library, ResourceType::Pool, "Library");
    GetReplay()->AddShaderBinary(Library, data);
    DerivedResource(this, Library);
  }
  return true;
}

WrappedMTLLibrary *WrappedMTLDevice::newDefaultLibrary()
{
  MTL::Library *realMTLLibrary;

  SERIALISE_TIME_CALL(realMTLLibrary = Unwrap(this)->newDefaultLibrary());
  if(!realMTLLibrary)
    return NULL;
  WrappedMTLLibrary *wrappedMTLLibrary;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), realMTLLibrary, wrappedMTLLibrary);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newDefaultLibrary);
      Serialise_newDefaultLibrary(ser, wrappedMTLLibrary);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLLibrary);
    record->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, wrappedMTLLibrary);
  }
  return wrappedMTLLibrary;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newLibraryWithSource(SerialiserType &ser,
                                                      WrappedMTLLibrary *library, NS::String *source,
                                                      MTL::CompileOptions *options, NS::Error **error)
{
  SERIALISE_ELEMENT_LOCAL(Device, this);
  SERIALISE_ELEMENT_LOCAL(Library, GetResID(library)).TypedAs("MTLLibrary"_lit);
  SERIALISE_ELEMENT(source);
  uint32_t libraryType = options ? (uint32_t)options->libraryType() : (uint32_t)MTL::LibraryTypeExecutable;
  rdcstr installName = options && options->installName() ? options->installName()->utf8String() : "";
  rdcarray<WrappedMTLDynamicLibrary *> dependencies;
  NS::Array *optionLibraries = options ? options->libraries() : NULL;
  for(NS::UInteger i = 0; optionLibraries && i < optionLibraries->count(); i++)
  {
    MTL::DynamicLibrary *dependency = optionLibraries->object<MTL::DynamicLibrary>(i);
    if(MetalDynamicLibraryIsWrapped(dependency))
      dependencies.push_back(GetWrapped(dependency));
  }
  bool supported = true;
  if(optionLibraries && dependencies.size() != optionLibraries->count()) supported = false;
  if(options)
  {
    MTL::CompileOptions *defaults = MTL::CompileOptions::alloc()->init();
    supported = supported &&
                (!options->preprocessorMacros() || !options->preprocessorMacros()->count()) &&
                options->fastMathEnabled() == defaults->fastMathEnabled() &&
                options->languageVersion() == defaults->languageVersion() &&
                options->preserveInvariance() == defaults->preserveInvariance() &&
                options->optimizationLevel() == defaults->optimizationLevel() &&
                options->compileSymbolVisibility() == defaults->compileSymbolVisibility() &&
                options->allowReferencingUndefinedSymbols() == defaults->allowReferencingUndefinedSymbols() &&
                options->maxTotalThreadsPerThreadgroup() == defaults->maxTotalThreadsPerThreadgroup();
    defaults->release();
  }
  if(ser.VersionAtLeast(0x6))
  {
    SERIALISE_ELEMENT(libraryType).Important();
    SERIALISE_ELEMENT(installName).Important();
    SERIALISE_ELEMENT(dependencies).Important();
    SERIALISE_ELEMENT(supported).Important();
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!Device || Device->m_Type != eResDevice || Device != this ||
       Library == ResourceId() || GetResourceManager()->HasResource(Library) || !source)
    {
      RDCERR("Invalid Metal source library device/identity/source");
      return false;
    }
    if(!supported || (libraryType != MTL::LibraryTypeExecutable &&
                      libraryType != MTL::LibraryTypeDynamic) ||
       installName.size() > 1024 || dependencies.size() > 8 ||
       (libraryType == MTL::LibraryTypeDynamic &&
        (installName.empty() || !dependencies.empty())) ||
       (libraryType == MTL::LibraryTypeExecutable && !installName.empty()))
    {
      RDCERR("Invalid or unsupported Metal source library compile options");
      return false;
    }
    for(WrappedMTLDynamicLibrary *dependency : dependencies)
      if(!dependency || dependency->m_Type != eResDynamicLibrary || !dependency->m_Real)
      {
        RDCERR("Invalid Metal source library dynamic dependency");
        return false;
      }
    rdcstr replayDirectory, replayInstallPath;
    MTL::CompileOptions *replayOptions = NULL;
    if(libraryType == MTL::LibraryTypeDynamic || !dependencies.empty())
    {
      replayOptions = MTL::CompileOptions::alloc()->init();
      replayOptions->setLibraryType((MTL::LibraryType)libraryType);
      if(libraryType == MTL::LibraryTypeDynamic)
      {
        rdcstr pattern = FileIO::GetTempFolderFilename() + "renderdoc-metal-dynamic.XXXXXX";
        rdcarray<char> chars(pattern.c_str(),pattern.size());
        chars.push_back(0);
        char *created = mkdtemp(chars.data());
        if(!created)
        {
          replayOptions->release();
          RDCERR("Could not create temporary Metal dynamic library directory");
          return false;
        }
        replayDirectory = created;
        replayInstallPath = replayDirectory + "/library.metallib";
        replayOptions->setInstallName(NS::String::string(replayInstallPath.c_str(),
                                                        NS::UTF8StringEncoding));
      }
      else
      {
        rdcarray<MTL::DynamicLibrary *> native;
        for(WrappedMTLDynamicLibrary *dependency : dependencies)
          native.push_back(Unwrap(dependency));
        replayOptions->setLibraries(NS::Array::array(
            (const NS::Object *const *)native.data(),native.size()));
      }
    }
    NS::Error *compileErrors = NULL;
    MTL::Library *realMTLLibrary = Unwrap(this)->newLibrary(source, replayOptions, &compileErrors);
    if(replayOptions) replayOptions->release();
    if(!realMTLLibrary)
    {
      if(!replayDirectory.empty()) rmdir(replayDirectory.c_str());
      RDCERR("Failed to recreate Metal library from captured MSL: %s",
             compileErrors ? compileErrors->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    WrappedMTLLibrary *wrappedMTLLibrary;
    GetResourceManager()->WrapResource(Library, realMTLLibrary, wrappedMTLLibrary, true);
    wrappedMTLLibrary->m_DynamicInstallPath = replayInstallPath;
    wrappedMTLLibrary->m_DynamicInstallDirectory = replayDirectory;
    AddResource(Library, ResourceType::Pool, "Library");
    GetReplay()->AddShaderLibrary(Library, source ? source->utf8String() : "");
    DerivedResource(this, Library);
  }
  return true;
}

WrappedMTLLibrary *WrappedMTLDevice::newLibraryWithSource(NS::String *source,
                                                          MTL::CompileOptions *options,
                                                          NS::Error **error)
{
  MTL::CompileOptions *realOptions = options ? options->copy() : NULL;
  NS::Array *optionLibraries = options ? options->libraries() : NULL;
  if(optionLibraries && optionLibraries->count())
  {
    rdcarray<MTL::DynamicLibrary *> native;
    for(NS::UInteger i = 0; i < optionLibraries->count(); i++)
    {
      MTL::DynamicLibrary *dependency = optionLibraries->object<MTL::DynamicLibrary>(i);
      native.push_back(MetalDynamicLibraryIsWrapped(dependency) ?
                       Unwrap(GetWrapped(dependency)) : dependency);
    }
    realOptions->setLibraries(NS::Array::array((const NS::Object *const *)native.data(),native.size()));
  }
  MTL::Library *realMTLLibrary;
  SERIALISE_TIME_CALL(realMTLLibrary = Unwrap(this)->newLibrary(source, realOptions, error));
  if(realOptions) realOptions->release();
  if(!realMTLLibrary)
    return NULL;
  WrappedMTLLibrary *wrappedMTLLibrary;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), realMTLLibrary, wrappedMTLLibrary);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newLibraryWithSource);
      Serialise_newLibraryWithSource(ser, wrappedMTLLibrary, source, options, error);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLLibrary);
    record->AddChunk(chunk);
    for(NS::UInteger i = 0; optionLibraries && i < optionLibraries->count(); i++)
    {
      MTL::DynamicLibrary *dependency = optionLibraries->object<MTL::DynamicLibrary>(i);
      if(MetalDynamicLibraryIsWrapped(dependency))
        record->AddParent(GetRecord(GetWrapped(dependency)));
    }
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, wrappedMTLLibrary);
  }
  return wrappedMTLLibrary;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newBufferWithBytes(SerialiserType &ser, WrappedMTLBuffer *buffer,
                                                    const void *pointer, NS::UInteger length,
                                                    MTL::ResourceOptions options)
{
  SERIALISE_ELEMENT_LOCAL(Buffer, GetResID(buffer)).TypedAs("MTLBuffer"_lit);
  bytebuf initialData;
  if(pointer)
  {
    initialData.assign((byte *)pointer, length);
  }
  SERIALISE_ELEMENT(initialData);
  SERIALISE_ELEMENT(length).Important();
  SERIALISE_ELEMENT(options);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    WrappedMTLObject *previous = GetResourceManager()->GetResource(Buffer, true);
    // Ordinary frame buffers also occur in legacy captures without descriptor annotations
    // (e.g. AS scratch/output). Recreate them on every seek through the same owned lifecycle.
    const bool frameBuffer = GetReplayEpoch() != 0 &&
        (!m_DescriptorCoverage || (m_DescriptorCoverage >= 8 && m_DescriptorFrameBuffers.count(Buffer)));
    if(Buffer == ResourceId() || !length ||
       (!initialData.empty() && (initialData.size() != length ||
        (uint64_t(options) & 0xf0ULL) >= uint64_t(MTL::ResourceStorageModePrivate))) ||
       (previous && !(frameBuffer && IsActiveReplaying(m_State) &&
                      previous->m_Type == eResBuffer && !previous->m_Real)))
      return false;
    MTL::Buffer *realMTLBuffer;
    if(initialData.isEmpty())
    {
      realMTLBuffer = Unwrap(this)->newBuffer(length, options);
    }
    else
    {
      RDCASSERT(initialData.size() == length);
      realMTLBuffer = Unwrap(this)->newBuffer(initialData.data(), initialData.size(), options);
    }
    if(!realMTLBuffer) return false;
    if(GetReplayEpoch() == 0 && initialData.size() == length)
      m_ReplayBuffersWithCreationContents.insert(Buffer);
    WrappedMTLBuffer *wrappedMTLBuffer = (WrappedMTLBuffer *)previous;
    if(previous)
      GetResourceManager()->ReplaceRealResource(wrappedMTLBuffer, realMTLBuffer, true);
    else
    {
      GetResourceManager()->WrapResource(Buffer, realMTLBuffer, wrappedMTLBuffer, true);
      AddResource(Buffer, ResourceType::Buffer, "Buffer");
      GetReplay()->AddBuffer(Buffer, length);
      DerivedResource(this, Buffer);
    }
    if(frameBuffer)
    {
      RegisterFramePlacementResource(Buffer, NULL);
      if(IsLoading(m_State))
        m_ReplayStandaloneBufferBirths[Buffer] = {length, uint64_t(options), m_CurChunkOffset};
    }
  }
  return true;
}

WrappedMTLBuffer *WrappedMTLDevice::newBufferWithBytes(const void *pointer, NS::UInteger length,
                                                       MTL::ResourceOptions options)
{
  return Common_NewBuffer(true, pointer, length, options);
}

WrappedMTLBuffer *WrappedMTLDevice::newBufferWithLength(NS::UInteger length,
                                                        MTL::ResourceOptions options)
{
  return Common_NewBuffer(false, NULL, length, options);
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newBufferWithBytesNoCopy(SerialiserType &ser,
    WrappedMTLBuffer *buffer, bytebuf initialData, uint64_t length, MTL::ResourceOptions options)
{
  SERIALISE_ELEMENT_LOCAL(Buffer, GetResID(buffer)).TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(initialData);
  SERIALISE_ELEMENT(length).Important();
  SERIALISE_ELEMENT(options);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    // Replay owns an independent copy: no application pointer or deallocator survives capture.
    if(Buffer == ResourceId() || GetResourceManager()->HasResource(Buffer) ||
       !length || length > 64 * 1024 * 1024 || initialData.size() != length ||
       options != MTL::ResourceStorageModeShared)
    {
      RDCERR("Invalid or unsupported Metal no-copy buffer identity, data or storage mode");
      return false;
    }
    MTL::Buffer *real = Unwrap(this)->newBuffer(initialData.data(), length, options);
    if(!real)
    {
      RDCERR("Metal failed to recreate no-copy buffer from captured bytes");
      return false;
    }
    WrappedMTLBuffer *wrapped = NULL;
    GetResourceManager()->WrapResource(Buffer, real, wrapped, true);
    AddResource(Buffer, ResourceType::Buffer, "Buffer");
    GetReplay()->AddBuffer(Buffer, length);
    DerivedResource(this, Buffer);
    if(GetReplayEpoch() == 0) m_ReplayBuffersWithCreationContents.insert(Buffer);
  }
  return true;
}

WrappedMTLBuffer *WrappedMTLDevice::WrapNewBufferNoCopy(MTL::Buffer *real, const void *pointer,
                                                       NS::UInteger length,
                                                       MTL::ResourceOptions options)
{
  if(!real) return NULL;
  WrappedMTLBuffer *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    bytebuf initialData;
    if(pointer && length) initialData.assign((const byte *)pointer, length);
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newBufferWithBytesNoCopy);
    Serialise_newBufferWithBytesNoCopy(ser, wrapped, initialData, length, options);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    record->bufInfo = new MetalBufferInfo(real->storageMode());
    if(real->storageMode() == MTL::StorageModeShared)
    {
      record->bufInfo->data = (byte *)real->contents();
      record->bufInfo->length = real->length();
    }
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newBufferWithBytesNoCopy(
    ReadSerialiser &ser, WrappedMTLBuffer *buffer, bytebuf initialData, uint64_t length,
    MTL::ResourceOptions options);
template bool WrappedMTLDevice::Serialise_newBufferWithBytesNoCopy(
    WriteSerialiser &ser, WrappedMTLBuffer *buffer, bytebuf initialData, uint64_t length,
    MTL::ResourceOptions options);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newDepthStencilStateWithDescriptor(
    SerialiserType &ser, WrappedMTLDepthStencilState *depthStencilState,
    RDMTL::DepthStencilDescriptor &descriptor)
{
  SERIALISE_ELEMENT_LOCAL(DepthStencilState, GetResID(depthStencilState))
      .TypedAs("MTLDepthStencilState"_lit);
  SERIALISE_ELEMENT(descriptor);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    MTL::DepthStencilDescriptor *mtlDescriptor(descriptor);
    MTL::DepthStencilState *realDepthStencilState =
        Unwrap(this)->newDepthStencilState(mtlDescriptor);
    mtlDescriptor->release();
    if(!realDepthStencilState)
    {
      RDCERR("Failed to recreate Metal depth-stencil state");
      return false;
    }

    WrappedMTLDepthStencilState *wrappedDepthStencilState;
    GetResourceManager()->WrapResource(DepthStencilState, realDepthStencilState,
                                       wrappedDepthStencilState, true);
    AddResource(DepthStencilState, ResourceType::StateObject, "Depth-Stencil State");
    GetReplay()->AddDepthStencilState(DepthStencilState, descriptor);
    DerivedResource(this, DepthStencilState);
  }
  return true;
}

WrappedMTLDepthStencilState *WrappedMTLDevice::newDepthStencilStateWithDescriptor(
    RDMTL::DepthStencilDescriptor &descriptor)
{
  MTL::DepthStencilDescriptor *realDescriptor(descriptor);
  MTL::DepthStencilState *realDepthStencilState;
  SERIALISE_TIME_CALL(realDepthStencilState =
                          Unwrap(this)->newDepthStencilState(realDescriptor));
  realDescriptor->release();

  if(!realDepthStencilState)
    return NULL;

  WrappedMTLDepthStencilState *wrappedDepthStencilState;
  GetResourceManager()->WrapResource(ResourceId(), realDepthStencilState,
                                     wrappedDepthStencilState);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newDepthStencilStateWithDescriptor);
      Serialise_newDepthStencilStateWithDescriptor(ser, wrappedDepthStencilState, descriptor);
      chunk = scope.Get();
    }

    MetalResourceRecord *record =
        GetResourceManager()->AddResourceRecord(wrappedDepthStencilState);
    record->AddChunk(chunk);
  }
  return wrappedDepthStencilState;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newSamplerStateWithDescriptor(
    SerialiserType &ser, WrappedMTLSamplerState *samplerState,
    RDMTL::SamplerDescriptor &descriptor)
{
  SERIALISE_ELEMENT_LOCAL(SamplerState, GetResID(samplerState)).TypedAs("MTLSamplerState"_lit);
  SERIALISE_ELEMENT(descriptor);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    MTL::SamplerDescriptor *mtlDescriptor(descriptor);
    MTL::SamplerState *realSamplerState = Unwrap(this)->newSamplerState(mtlDescriptor);
    mtlDescriptor->release();
    if(!realSamplerState)
    {
      RDCERR("Failed to recreate Metal sampler state");
      return false;
    }

    WrappedMTLSamplerState *wrappedSamplerState;
    GetResourceManager()->WrapResource(SamplerState, realSamplerState, wrappedSamplerState, true);
    AddResource(SamplerState, ResourceType::Sampler, "Sampler");
    GetReplay()->AddSamplerState(SamplerState, descriptor);
    DerivedResource(this, SamplerState);
  }
  return true;
}

WrappedMTLSamplerState *WrappedMTLDevice::newSamplerStateWithDescriptor(
    RDMTL::SamplerDescriptor &descriptor)
{
  MTL::SamplerDescriptor *realDescriptor(descriptor);
  MTL::SamplerState *realSamplerState;
  SERIALISE_TIME_CALL(realSamplerState = Unwrap(this)->newSamplerState(realDescriptor));
  realDescriptor->release();

  if(!realSamplerState)
    return NULL;

  WrappedMTLSamplerState *wrappedSamplerState;
  GetResourceManager()->WrapResource(ResourceId(), realSamplerState, wrappedSamplerState);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newSamplerStateWithDescriptor);
      Serialise_newSamplerStateWithDescriptor(ser, wrappedSamplerState, descriptor);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedSamplerState);
    record->AddChunk(chunk);
  }
  return wrappedSamplerState;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newIndirectCommandBufferWithDescriptor(
    SerialiserType &ser, WrappedMTLIndirectCommandBuffer *icb,
    MTL::IndirectCommandType commandTypes, bool inheritPipelineState, bool inheritBuffers,
    NS::UInteger maxVertexBufferBindCount, NS::UInteger maxFragmentBufferBindCount,
    NS::UInteger maxCount, MTL::ResourceOptions options)
{
  SERIALISE_ELEMENT_LOCAL(IndirectCommandBuffer, GetResID(icb))
      .TypedAs("MTLIndirectCommandBuffer"_lit);
  SERIALISE_ELEMENT(commandTypes).Important();
  SERIALISE_ELEMENT(inheritPipelineState).Important();
  SERIALISE_ELEMENT(inheritBuffers).Important();
  SERIALISE_ELEMENT(maxVertexBufferBindCount).Important();
  SERIALISE_ELEMENT(maxFragmentBufferBindCount).Important();
  SERIALISE_ELEMENT(maxCount).Important();
  SERIALISE_ELEMENT(options);
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    const bool supportedType =
        (commandTypes == MTL::IndirectCommandTypeDraw &&
         ((inheritBuffers && !inheritPipelineState && maxVertexBufferBindCount == 0) ||
          (!inheritBuffers && maxVertexBufferBindCount == 1))) ||
        (!inheritPipelineState && !inheritBuffers &&
         commandTypes == MTL::IndirectCommandTypeDrawIndexed && maxVertexBufferBindCount == 2) ||
        (!inheritPipelineState && !inheritBuffers &&
         (uint64_t)commandTypes == ((uint64_t)MTL::IndirectCommandTypeDraw |
                                    (uint64_t)MTL::IndirectCommandTypeDrawIndexed) &&
         maxVertexBufferBindCount == 2);
    // Replay restores CPU-encoded ICBs between event selections. Private/GPU-generated contents
    // need a different snapshot path and must fail before any CPU command encoding reaches Metal.
    if(!supportedType || (uint64_t(options) & 0xf0ULL) != uint64_t(MTL::ResourceStorageModeShared) ||
       maxFragmentBufferBindCount != 0 || maxCount == 0 ||
       maxCount > 1024)
    {
      RDCERR("Unsupported Metal ICB descriptor: storage mode, command type or inheritance/binding limits");
      return false;
    }
    MTL::IndirectCommandBufferDescriptor *descriptor =
        MTL::IndirectCommandBufferDescriptor::alloc()->init();
    descriptor->setCommandTypes(commandTypes);
    descriptor->setInheritPipelineState(inheritPipelineState);
    descriptor->setInheritBuffers(inheritBuffers);
    descriptor->setMaxVertexBufferBindCount(maxVertexBufferBindCount);
    descriptor->setMaxFragmentBufferBindCount(maxFragmentBufferBindCount);
    MTL::IndirectCommandBuffer *real = Unwrap(this)->newIndirectCommandBuffer(descriptor, maxCount,
                                                                               options);
    descriptor->release();
    if(!real)
    {
      RDCERR("Failed to recreate Metal indirect command buffer");
      return false;
    }
    WrappedMTLIndirectCommandBuffer *wrapped = NULL;
    GetResourceManager()->WrapResource(IndirectCommandBuffer, real, wrapped, true);
    wrapped->SetCount(maxCount);
    wrapped->SetMaxVertexBufferBindCount(maxVertexBufferBindCount);
    wrapped->SetCommandTypes(commandTypes);
    wrapped->SetInheritPipelineState(inheritPipelineState);
    wrapped->SetInheritBuffers(inheritBuffers);
    wrapped->SetSupportedDescriptor(true);
    AddResource(IndirectCommandBuffer, ResourceType::StateObject, "Indirect Command Buffer");
    DerivedResource(this, IndirectCommandBuffer);
  }
  return true;
}

WrappedMTLIndirectCommandBuffer *WrappedMTLDevice::newIndirectCommandBufferWithDescriptor(
    MTL::IndirectCommandType commandTypes, bool inheritPipelineState, bool inheritBuffers,
    NS::UInteger maxVertexBufferBindCount, NS::UInteger maxFragmentBufferBindCount,
    NS::UInteger maxCount, MTL::ResourceOptions options)
{
  MTL::IndirectCommandBufferDescriptor *descriptor =
      MTL::IndirectCommandBufferDescriptor::alloc()->init();
  descriptor->setCommandTypes(commandTypes);
  descriptor->setInheritPipelineState(inheritPipelineState);
  descriptor->setInheritBuffers(inheritBuffers);
  descriptor->setMaxVertexBufferBindCount(maxVertexBufferBindCount);
  descriptor->setMaxFragmentBufferBindCount(maxFragmentBufferBindCount);
  MTL::IndirectCommandBuffer *real = Unwrap(this)->newIndirectCommandBuffer(descriptor, maxCount,
                                                                             options);
  descriptor->release();
  if(!real)
    return NULL;
  WrappedMTLIndirectCommandBuffer *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->SetCount(maxCount);
  wrapped->SetMaxVertexBufferBindCount(maxVertexBufferBindCount);
  wrapped->SetCommandTypes(commandTypes);
  wrapped->SetInheritPipelineState(inheritPipelineState);
  wrapped->SetInheritBuffers(inheritBuffers);
  wrapped->SetSupportedDescriptor(((commandTypes == MTL::IndirectCommandTypeDraw &&
                                    ((inheritBuffers && !inheritPipelineState &&
                                      maxVertexBufferBindCount == 0) ||
                                     (!inheritBuffers && maxVertexBufferBindCount == 1))) ||
                                   (!inheritPipelineState && !inheritBuffers &&
                                    commandTypes == MTL::IndirectCommandTypeDrawIndexed &&
                                    maxVertexBufferBindCount == 2) ||
                                   (!inheritPipelineState && !inheritBuffers &&
                                    (uint64_t)commandTypes ==
                                        ((uint64_t)MTL::IndirectCommandTypeDraw |
                                         (uint64_t)MTL::IndirectCommandTypeDrawIndexed) &&
                                    maxVertexBufferBindCount == 2)) &&
                                  maxFragmentBufferBindCount == 0);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newIndirectCommandBufferWithDescriptor);
    Serialise_newIndirectCommandBufferWithDescriptor(
        ser, wrapped, commandTypes, inheritPipelineState, inheritBuffers,
        maxVertexBufferBindCount, maxFragmentBufferBindCount, maxCount, options);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newRenderPipelineStateWithDescriptor(
    SerialiserType &ser, WrappedMTLRenderPipelineState *pipelineState,
    RDMTL::RenderPipelineDescriptor &descriptor, NS::Error **error)
{
  SERIALISE_ELEMENT_LOCAL(RenderPipelineState, GetResID(pipelineState))
      .TypedAs("MTLRenderPipelineState"_lit);
  SERIALISE_ELEMENT(descriptor);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    auto validPreloads = [](const rdcarray<WrappedMTLDynamicLibrary *> &libraries) {
      if(libraries.size() > 8) return false;
      for(WrappedMTLDynamicLibrary *library : libraries)
        if(!library || library->m_Type != eResDynamicLibrary || !library->m_Real)
          return false;
      return true;
    };
    auto validVisibleLinks = [](const RDMTL::LinkedFunctions &links) {
      if(links.functions.size() > 8 || !links.binaryFunctions.empty() ||
         !links.groups.empty() || !links.privateFunctions.empty()) return false;
      for(WrappedMTLFunction *function : links.functions)
        if(!function || function->m_Type != eResFunction || !function->m_Real ||
           (Unwrap(function)->functionType() != MTL::FunctionTypeVisible &&
            Unwrap(function)->functionType() != MTL::FunctionTypeIntersection))
          return false;
      return true;
    };
    if(!ValidateMetalPipelineFunction(descriptor.vertexFunction, MTL::FunctionTypeVertex) ||
       (descriptor.fragmentFunction &&
        !ValidateMetalPipelineFunction(descriptor.fragmentFunction, MTL::FunctionTypeFragment)) ||
       !validPreloads(descriptor.vertexPreloadedLibraries) ||
       !validPreloads(descriptor.fragmentPreloadedLibraries) ||
       descriptor.binaryArchives.size() > 8 ||
       !validVisibleLinks(descriptor.vertexLinkedFunctions) ||
       !validVisibleLinks(descriptor.fragmentLinkedFunctions))
    {
      RDCERR("Invalid Metal render pipeline dynamic library or visible-function links");
      return false;
    }
    for(WrappedMTLBinaryArchive *archive : descriptor.binaryArchives)
      if(!archive || archive->m_Type != eResBinaryArchive || !archive->m_Real)
      {
        RDCERR("Invalid Metal render pipeline binary archive dependency");
        return false;
      }
    ResourceId liveID;

    MTL::RenderPipelineDescriptor *mtlDescriptor(descriptor);
    MTL::AutoreleasedRenderPipelineReflection pipelineReflection = NULL;
    MTL::RenderPipelineState *realMTLRenderPipelineState = Unwrap(this)->newRenderPipelineState(
        mtlDescriptor, MTL::PipelineOptionArgumentInfo, &pipelineReflection, error);
    if(realMTLRenderPipelineState)
      GetReplay()->CacheShaderPipeline(RenderPipelineState, mtlDescriptor, 0);
    mtlDescriptor->release();
    if(!realMTLRenderPipelineState)
    {
      RDCERR("Failed to recreate Metal render pipeline");
      return false;
    }
    WrappedMTLRenderPipelineState *wrappedMTLRenderPipelineState;
    liveID = GetResourceManager()->WrapResource(RenderPipelineState, realMTLRenderPipelineState,
                                                wrappedMTLRenderPipelineState, true);
    AddResource(RenderPipelineState, ResourceType::PipelineState, "Pipeline State");
    GetReplay()->AddRenderPipeline(RenderPipelineState, descriptor, pipelineReflection);
    DerivedResource(this, RenderPipelineState);
    for(WrappedMTLDynamicLibrary *library : descriptor.vertexPreloadedLibraries)
      DerivedResource(library, RenderPipelineState);
    for(WrappedMTLDynamicLibrary *library : descriptor.fragmentPreloadedLibraries)
      DerivedResource(library, RenderPipelineState);
    for(WrappedMTLBinaryArchive *archive : descriptor.binaryArchives)
      DerivedResource(archive, RenderPipelineState);
    for(WrappedMTLFunction *function : descriptor.vertexLinkedFunctions.functions)
      DerivedResource(function, RenderPipelineState);
    for(WrappedMTLFunction *function : descriptor.fragmentLinkedFunctions.functions)
      DerivedResource(function, RenderPipelineState);
  }
  return true;
}

WrappedMTLRenderPipelineState *WrappedMTLDevice::newRenderPipelineStateWithDescriptor(
    RDMTL::RenderPipelineDescriptor &descriptor, NS::Error **error)
{
  // A native, unwrapped function in a linked-functions array is represented as NULL by
  // RDMTL::LinkedFunctions. Never pass that placeholder into the Metal descriptor.
  auto validCaptureLinks = [](const RDMTL::LinkedFunctions &links) {
    for(WrappedMTLFunction *function : links.functions)
      if(!function) return false;
    for(WrappedMTLFunction *function : links.binaryFunctions)
      if(!function) return false;
    for(WrappedMTLFunction *function : links.privateFunctions)
      if(!function) return false;
    for(const RDMTL::FunctionGroup &group : links.groups)
      for(WrappedMTLFunction *function : group.functions)
        if(!function) return false;
    return true;
  };
  if(!validCaptureLinks(descriptor.vertexLinkedFunctions) ||
     !validCaptureLinks(descriptor.fragmentLinkedFunctions))
  {
    RDCERR("Cannot capture Metal render pipeline with unwrapped linked functions");
    return NULL;
  }
  for(WrappedMTLBinaryArchive *archive : descriptor.binaryArchives)
    if(!archive || archive->m_Type != eResBinaryArchive || !archive->m_Real)
    {
      RDCERR("Cannot capture Metal render pipeline with unwrapped binary archive");
      return NULL;
    }
  MTL::RenderPipelineDescriptor *realDescriptor(descriptor);
  MTL::RenderPipelineState *realMTLRenderPipelineState;
  SERIALISE_TIME_CALL(realMTLRenderPipelineState =
                          Unwrap(this)->newRenderPipelineState(realDescriptor, error));
  realDescriptor->release();

  if(!realMTLRenderPipelineState)
    return NULL;

  WrappedMTLRenderPipelineState *wrappedMTLRenderPipelineState;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), realMTLRenderPipelineState,
                                                     wrappedMTLRenderPipelineState);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newRenderPipelineStateWithDescriptor);
      Serialise_newRenderPipelineStateWithDescriptor(ser, wrappedMTLRenderPipelineState, descriptor,
                                                     error);
      chunk = scope.Get();
    }

    MetalResourceRecord *record =
        GetResourceManager()->AddResourceRecord(wrappedMTLRenderPipelineState);
    record->AddChunk(chunk);
    if(descriptor.vertexFunction)
    {
      record->AddParent(GetRecord(descriptor.vertexFunction));
    }
    if(descriptor.fragmentFunction)
    {
      record->AddParent(GetRecord(descriptor.fragmentFunction));
    }
    for(WrappedMTLDynamicLibrary *library : descriptor.vertexPreloadedLibraries)
      record->AddParent(GetRecord(library));
    for(WrappedMTLDynamicLibrary *library : descriptor.fragmentPreloadedLibraries)
      record->AddParent(GetRecord(library));
    for(WrappedMTLBinaryArchive *archive : descriptor.binaryArchives)
      record->AddParent(GetRecord(archive));
    for(WrappedMTLFunction *function : descriptor.vertexLinkedFunctions.functions)
      if(function) record->AddParent(GetRecord(function));
    for(WrappedMTLFunction *function : descriptor.fragmentLinkedFunctions.functions)
      if(function) record->AddParent(GetRecord(function));
  }
  else
  {
    // TODO: implement RD MTL replay
    //     GetResourceManager()->AddLiveResource(id, *wrappedMTLRenderPipelineState);
  }
  return wrappedMTLRenderPipelineState;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newTileRenderPipelineState(
    SerialiserType &ser, WrappedMTLRenderPipelineState *pipeline,
    WrappedMTLFunction *tileFunction, rdcarray<uint32_t> colorFormats,
    uint64_t sampleCount, uint64_t maxThreads, bool matchesTileSize,
    uint32_t options, bool supported, rdcarray<WrappedMTLFunction *> visibleFunctions,
    rdcarray<WrappedMTLBinaryArchive *> binaryArchives)
{
  SERIALISE_ELEMENT_LOCAL(PipelineState, GetResID(pipeline)).TypedAs("MTLRenderPipelineState"_lit).Important();
  SERIALISE_ELEMENT(tileFunction).Important();
  SERIALISE_ELEMENT(colorFormats).Important();
  SERIALISE_ELEMENT(sampleCount).Important();
  SERIALISE_ELEMENT(maxThreads).Important();
  SERIALISE_ELEMENT(matchesTileSize);
  SERIALISE_ELEMENT(options);
  SERIALISE_ELEMENT(supported).Important();
  if(ser.VersionAtLeast(0x9))
  {
    SERIALISE_ELEMENT(visibleFunctions).Important();
  }
  if(ser.VersionAtLeast(0xC))
  {
    SERIALISE_ELEMENT(binaryArchives).Important();
  }
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(PipelineState == ResourceId() || GetResourceManager()->HasResource(PipelineState) ||
       !tileFunction || tileFunction->m_Type != eResFunction || !tileFunction->m_Real ||
       tileFunction->m_Device != this || !supported || colorFormats.size() != 8 ||
       (colorFormats[0] != MTL::PixelFormatBGRA8Unorm &&
        colorFormats[0] != MTL::PixelFormatRGBA8Unorm) ||
       sampleCount != 1 || maxThreads > 1024 ||
       (options & ~((uint32_t)MTL::PipelineOptionArgumentInfo |
                    (uint32_t)MTL::PipelineOptionFailOnBinaryArchiveMiss)) != 0)
    {
      RDCERR("Invalid or unsupported Metal tile pipeline identity or descriptor");
      return false;
    }
    if(visibleFunctions.size() > 8)
    {
      RDCERR("Invalid Metal tile linked-function count");
      return false;
    }
    if(binaryArchives.size() > 4)
      return false;
    for(WrappedMTLBinaryArchive *archive : binaryArchives)
      if(!archive || archive->m_Type != eResBinaryArchive || !archive->m_Real ||
         archive->m_Device != this)
      {
        RDCERR("Invalid Metal tile pipeline archive dependency");
        return false;
      }
    for(WrappedMTLFunction *function : visibleFunctions)
      if(!function || function->m_Type != eResFunction || !function->m_Real ||
         function->m_Device != this ||
         (Unwrap(function)->functionType() != MTL::FunctionTypeVisible &&
          Unwrap(function)->functionType() != MTL::FunctionTypeIntersection))
      {
        RDCERR("Invalid Metal tile linked function");
        return false;
      }
    for(size_t i = 1; i < colorFormats.size(); i++)
      if(colorFormats[i] != MTL::PixelFormatInvalid)
      {
        RDCERR("Unsupported Metal tile pipeline color attachment %zu", i);
        return false;
      }
    MTL::TileRenderPipelineDescriptor *descriptor = MTL::TileRenderPipelineDescriptor::alloc()->init();
    descriptor->setTileFunction(Unwrap(tileFunction));
    if(!visibleFunctions.empty())
    {
      MTL::LinkedFunctions *links = MTL::LinkedFunctions::alloc()->init();
      rdcarray<const NS::Object *> native;
      for(WrappedMTLFunction *function : visibleFunctions) native.push_back(Unwrap(function));
      links->setFunctions(NS::Array::array(native.data(), native.size()));
      descriptor->setLinkedFunctions(links);
      links->release();
    }
    descriptor->setRasterSampleCount(sampleCount);
    descriptor->setMaxTotalThreadsPerThreadgroup(maxThreads);
    descriptor->setThreadgroupSizeMatchesTileSize(matchesTileSize);
    if(!binaryArchives.empty())
    {
      rdcarray<const NS::Object *> native;
      for(WrappedMTLBinaryArchive *archive : binaryArchives)
        native.push_back(Unwrap(archive));
      descriptor->setBinaryArchives(NS::Array::array(native.data(), native.size()));
    }
    for(size_t i = 0; i < colorFormats.size(); i++)
      descriptor->colorAttachments()->object(i)->setPixelFormat((MTL::PixelFormat)colorFormats[i]);
    MTL::AutoreleasedRenderPipelineReflection reflection = NULL;
    NS::Error *error = NULL;
    MTL::RenderPipelineState *real = Unwrap(this)->newRenderPipelineState(
        descriptor, (MTL::PipelineOption)(options |
            (uint32_t)MTL::PipelineOptionArgumentInfo), &reflection, &error);
    if(real) GetReplay()->CacheShaderPipeline(PipelineState, descriptor, 3);
    descriptor->release();
    if(!real)
    {
      RDCERR("Failed to recreate Metal tile pipeline: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    WrappedMTLRenderPipelineState *wrapped = NULL;
    GetResourceManager()->WrapResource(PipelineState, real, wrapped, true);
    AddResource(PipelineState, ResourceType::PipelineState, "Tile Pipeline State");
    GetReplay()->AddTilePipeline(PipelineState, GetResID(tileFunction), reflection);
    DerivedResource(tileFunction, PipelineState);
    for(WrappedMTLFunction *function : visibleFunctions)
      DerivedResource(function, PipelineState);
    for(WrappedMTLBinaryArchive *archive : binaryArchives)
      DerivedResource(archive, PipelineState);
  }
  return true;
}

WrappedMTLRenderPipelineState *WrappedMTLDevice::newTileRenderPipelineState(
    MTL::TileRenderPipelineDescriptor *descriptor, WrappedMTLFunction *tileFunction,
    MTL::PipelineOption options, MTL::AutoreleasedRenderPipelineReflection *reflection,
    NS::Error **error, bool supported, rdcarray<WrappedMTLFunction *> visibleFunctions,
    rdcarray<WrappedMTLBinaryArchive *> binaryArchives)
{
  MTL::RenderPipelineState *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newRenderPipelineState(
      descriptor, options, reflection, error));
  if(!real) return NULL;
  WrappedMTLRenderPipelineState *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    rdcarray<uint32_t> colorFormats;
    for(uint32_t i = 0; i < 8; i++)
      colorFormats.push_back((uint32_t)descriptor->colorAttachments()->object(i)->pixelFormat());
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newRenderPipelineStateWithTileDescriptor);
    Serialise_newTileRenderPipelineState(ser, wrapped, tileFunction, colorFormats,
        descriptor->rasterSampleCount(), descriptor->maxTotalThreadsPerThreadgroup(),
        descriptor->threadgroupSizeMatchesTileSize(), (uint32_t)options, supported,
        visibleFunctions, binaryArchives);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    if(tileFunction) record->AddParent(GetRecord(tileFunction));
    for(WrappedMTLFunction *function : visibleFunctions)
      if(function) record->AddParent(GetRecord(function));
    for(WrappedMTLBinaryArchive *archive : binaryArchives)
      if(archive) record->AddParent(GetRecord(archive));
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newTileRenderPipelineState(
    ReadSerialiser &, WrappedMTLRenderPipelineState *, WrappedMTLFunction *,
    rdcarray<uint32_t>, uint64_t, uint64_t, bool, uint32_t, bool,
    rdcarray<WrappedMTLFunction *>, rdcarray<WrappedMTLBinaryArchive *>);
template bool WrappedMTLDevice::Serialise_newTileRenderPipelineState(
    WriteSerialiser &, WrappedMTLRenderPipelineState *, WrappedMTLFunction *,
    rdcarray<uint32_t>, uint64_t, uint64_t, bool, uint32_t, bool,
    rdcarray<WrappedMTLFunction *>, rdcarray<WrappedMTLBinaryArchive *>);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newMeshRenderPipelineState(
    SerialiserType &ser, WrappedMTLRenderPipelineState *pipeline,
    WrappedMTLFunction *objectFunction, WrappedMTLFunction *meshFunction,
    WrappedMTLFunction *fragmentFunction, rdcarray<uint32_t> colorFormats,
    uint64_t sampleCount, uint64_t maxMeshThreads, uint32_t options, bool supported,
    uint64_t maxMeshGrid, rdcarray<WrappedMTLBinaryArchive *> binaryArchives)
{
  SERIALISE_ELEMENT_LOCAL(PipelineState, GetResID(pipeline)).TypedAs("MTLRenderPipelineState"_lit).Important();
  SERIALISE_ELEMENT(objectFunction);
  SERIALISE_ELEMENT(meshFunction).Important();
  SERIALISE_ELEMENT(fragmentFunction).Important();
  SERIALISE_ELEMENT(colorFormats).Important();
  SERIALISE_ELEMENT(sampleCount).Important();
  SERIALISE_ELEMENT(maxMeshThreads).Important();
  SERIALISE_ELEMENT(options);
  SERIALISE_ELEMENT(supported).Important();
  if(ser.VersionAtLeast(0x4))
  {
    SERIALISE_ELEMENT(maxMeshGrid).Important();
  }
  if(ser.VersionAtLeast(0xC))
  {
    SERIALISE_ELEMENT(binaryArchives).Important();
  }
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(PipelineState == ResourceId() || GetResourceManager()->HasResource(PipelineState) ||
       objectFunction || !meshFunction || meshFunction->m_Type != eResFunction ||
       !meshFunction->m_Real || meshFunction->m_Device != this ||
       !fragmentFunction || fragmentFunction->m_Type != eResFunction ||
       !fragmentFunction->m_Real || fragmentFunction->m_Device != this ||
       !supported || colorFormats.size() != 8 ||
       (colorFormats[0] != MTL::PixelFormatBGRA8Unorm &&
        colorFormats[0] != MTL::PixelFormatRGBA8Unorm) ||
       sampleCount != 1 || !maxMeshThreads || maxMeshThreads > 1024 ||
       maxMeshGrid > 1048575 ||
       (options & ~((uint32_t)MTL::PipelineOptionArgumentInfo | (uint32_t)MTL::PipelineOptionBufferTypeInfo |
                    (uint32_t)MTL::PipelineOptionFailOnBinaryArchiveMiss)) != 0)
    {
      RDCERR("Invalid or unsupported Metal mesh pipeline identity or descriptor");
      return false;
    }
    if(binaryArchives.size() > 4) return false;
    for(WrappedMTLBinaryArchive *archive : binaryArchives)
      if(!archive || archive->m_Type != eResBinaryArchive || !archive->m_Real ||
         archive->m_Device != this)
      {
        RDCERR("Invalid Metal mesh pipeline archive dependency");
        return false;
      }
    for(size_t i = 1; i < colorFormats.size(); i++)
      if(colorFormats[i] != MTL::PixelFormatInvalid)
      {
        RDCERR("Unsupported Metal mesh pipeline color attachment %zu", i);
        return false;
      }
    MTL::MeshRenderPipelineDescriptor *desc = MTL::MeshRenderPipelineDescriptor::alloc()->init();
    desc->setMeshFunction(Unwrap(meshFunction));
    desc->setFragmentFunction(Unwrap(fragmentFunction));
    desc->setRasterSampleCount(sampleCount);
    desc->setMaxTotalThreadsPerMeshThreadgroup(maxMeshThreads);
    if(maxMeshGrid) desc->setMaxTotalThreadgroupsPerMeshGrid(maxMeshGrid);
    if(!binaryArchives.empty())
    {
      rdcarray<const NS::Object *> native;
      for(WrappedMTLBinaryArchive *archive : binaryArchives)
        native.push_back(Unwrap(archive));
      NS::Array *archives = NS::Array::array(native.data(), native.size());
      SEL method = sel_registerName("setBinaryArchives:");
      if(!((BOOL (*)(id, SEL, SEL))objc_msgSend)(
             (id)desc, sel_registerName("respondsToSelector:"), method))
      {
        desc->release();
        return false;
      }
      ((void (*)(id, SEL, id))objc_msgSend)((id)desc, method, (id)archives);
    }
    for(size_t i = 0; i < colorFormats.size(); i++)
      desc->colorAttachments()->object(i)->setPixelFormat((MTL::PixelFormat)colorFormats[i]);
    MTL::AutoreleasedRenderPipelineReflection reflection = NULL;
    NS::Error *error = NULL;
    MTL::RenderPipelineState *real = Unwrap(this)->newRenderPipelineState(
        desc, (MTL::PipelineOption)(options |
            (uint32_t)MTL::PipelineOptionArgumentInfo | (uint32_t)MTL::PipelineOptionBufferTypeInfo), &reflection, &error);
    if(real) GetReplay()->CacheShaderPipeline(PipelineState, desc, 2);
    desc->release();
    if(!real)
    {
      RDCERR("Failed to recreate Metal mesh pipeline: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    WrappedMTLRenderPipelineState *wrapped = NULL;
    GetResourceManager()->WrapResource(PipelineState, real, wrapped, true);
    AddResource(PipelineState, ResourceType::PipelineState, "Mesh Pipeline State");
    GetReplay()->AddMeshPipeline(PipelineState, GetResID(meshFunction),
                                 GetResID(fragmentFunction), (uint32_t)sampleCount, reflection, colorFormats);
    DerivedResource(meshFunction, PipelineState);
    DerivedResource(fragmentFunction, PipelineState);
    for(WrappedMTLBinaryArchive *archive : binaryArchives)
      DerivedResource(archive, PipelineState);
  }
  return true;
}

WrappedMTLRenderPipelineState *WrappedMTLDevice::newMeshRenderPipelineState(
    MTL::MeshRenderPipelineDescriptor *descriptor, WrappedMTLFunction *objectFunction,
    WrappedMTLFunction *meshFunction, WrappedMTLFunction *fragmentFunction,
    MTL::PipelineOption options, MTL::AutoreleasedRenderPipelineReflection *reflection,
    NS::Error **error, bool supported,
    rdcarray<WrappedMTLBinaryArchive *> binaryArchives)
{
  MTL::RenderPipelineState *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newRenderPipelineState(
      descriptor, options, reflection, error));
  if(!real) return NULL;
  WrappedMTLRenderPipelineState *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    rdcarray<uint32_t> colorFormats;
    for(uint32_t i = 0; i < 8; i++)
      colorFormats.push_back((uint32_t)descriptor->colorAttachments()->object(i)->pixelFormat());
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(objectFunction ?
        MetalChunk::MTLDevice_newRenderPipelineStateWithObjectMeshDescriptor :
        MetalChunk::MTLDevice_newRenderPipelineStateWithMeshDescriptor);
    if(objectFunction)
      Serialise_newObjectMeshPipelineState(ser, wrapped, objectFunction, meshFunction,
          fragmentFunction, colorFormats, descriptor->rasterSampleCount(),
          descriptor->maxTotalThreadsPerObjectThreadgroup(),
          descriptor->maxTotalThreadsPerMeshThreadgroup(), descriptor->payloadMemoryLength(),
          descriptor->maxTotalThreadgroupsPerMeshGrid(), (uint32_t)options, supported);
    else
      Serialise_newMeshRenderPipelineState(ser, wrapped, objectFunction, meshFunction,
          fragmentFunction, colorFormats, descriptor->rasterSampleCount(),
          descriptor->maxTotalThreadsPerMeshThreadgroup(), (uint32_t)options, supported,
          descriptor->maxTotalThreadgroupsPerMeshGrid(), binaryArchives);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    if(objectFunction) record->AddParent(GetRecord(objectFunction));
    if(meshFunction) record->AddParent(GetRecord(meshFunction));
    if(fragmentFunction) record->AddParent(GetRecord(fragmentFunction));
    for(WrappedMTLBinaryArchive *archive : binaryArchives)
      if(archive) record->AddParent(GetRecord(archive));
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newMeshRenderPipelineState(
    ReadSerialiser &, WrappedMTLRenderPipelineState *, WrappedMTLFunction *,
    WrappedMTLFunction *, WrappedMTLFunction *, rdcarray<uint32_t>, uint64_t,
    uint64_t, uint32_t, bool, uint64_t, rdcarray<WrappedMTLBinaryArchive *>);
template bool WrappedMTLDevice::Serialise_newMeshRenderPipelineState(
    WriteSerialiser &, WrappedMTLRenderPipelineState *, WrappedMTLFunction *,
    WrappedMTLFunction *, WrappedMTLFunction *, rdcarray<uint32_t>, uint64_t,
    uint64_t, uint32_t, bool, uint64_t, rdcarray<WrappedMTLBinaryArchive *>);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newObjectMeshPipelineState(
    SerialiserType &ser, WrappedMTLRenderPipelineState *pipeline,
    WrappedMTLFunction *objectFunction, WrappedMTLFunction *meshFunction,
    WrappedMTLFunction *fragmentFunction, rdcarray<uint32_t> colorFormats,
    uint64_t sampleCount, uint64_t maxObjectThreads, uint64_t maxMeshThreads,
    uint64_t payloadLength, uint64_t maxMeshGrid, uint32_t options, bool supported)
{
  SERIALISE_ELEMENT_LOCAL(PipelineState, GetResID(pipeline)).TypedAs("MTLRenderPipelineState"_lit).Important();
  SERIALISE_ELEMENT(objectFunction).Important();
  SERIALISE_ELEMENT(meshFunction).Important();
  SERIALISE_ELEMENT(fragmentFunction).Important();
  SERIALISE_ELEMENT(colorFormats).Important();
  SERIALISE_ELEMENT(sampleCount).Important();
  SERIALISE_ELEMENT(maxObjectThreads).Important();
  SERIALISE_ELEMENT(maxMeshThreads).Important();
  SERIALISE_ELEMENT(payloadLength).Important();
  SERIALISE_ELEMENT(maxMeshGrid).Important();
  SERIALISE_ELEMENT(options);
  SERIALISE_ELEMENT(supported).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    auto validFunction = [&](WrappedMTLFunction *function) {
      return function && function->m_Type == eResFunction && function->m_Real &&
             function->m_Device == this;
    };
    if(PipelineState == ResourceId() || GetResourceManager()->HasResource(PipelineState) ||
       !validFunction(objectFunction) || !validFunction(meshFunction) ||
       !validFunction(fragmentFunction) || !supported || colorFormats.size() != 8 ||
       (colorFormats[0] != MTL::PixelFormatBGRA8Unorm &&
        colorFormats[0] != MTL::PixelFormatRGBA8Unorm) ||
       sampleCount != 1 || maxObjectThreads != 32 || !maxMeshThreads ||
       maxMeshThreads > 1024 || payloadLength != 16 || maxMeshGrid != 1 ||
       (options & ~((uint32_t)MTL::PipelineOptionArgumentInfo | (uint32_t)MTL::PipelineOptionBufferTypeInfo)) != 0)
    {
      RDCERR("Invalid or unsupported Metal object/mesh pipeline descriptor");
      return false;
    }
    for(size_t i = 1; i < colorFormats.size(); i++)
      if(colorFormats[i] != MTL::PixelFormatInvalid)
      {
        RDCERR("Unsupported Metal object/mesh color attachment %zu", i);
        return false;
      }
    MTL::MeshRenderPipelineDescriptor *desc = MTL::MeshRenderPipelineDescriptor::alloc()->init();
    desc->setObjectFunction(Unwrap(objectFunction));
    desc->setMeshFunction(Unwrap(meshFunction));
    desc->setFragmentFunction(Unwrap(fragmentFunction));
    desc->setRasterSampleCount(sampleCount);
    desc->setMaxTotalThreadsPerObjectThreadgroup(maxObjectThreads);
    desc->setMaxTotalThreadsPerMeshThreadgroup(maxMeshThreads);
    desc->setPayloadMemoryLength(payloadLength);
    desc->setMaxTotalThreadgroupsPerMeshGrid(maxMeshGrid);
    for(size_t i = 0; i < colorFormats.size(); i++)
      desc->colorAttachments()->object(i)->setPixelFormat((MTL::PixelFormat)colorFormats[i]);
    MTL::AutoreleasedRenderPipelineReflection reflection = NULL;
    NS::Error *error = NULL;
    MTL::RenderPipelineState *real = Unwrap(this)->newRenderPipelineState(
        desc, (MTL::PipelineOption)(MTL::PipelineOptionArgumentInfo | MTL::PipelineOptionBufferTypeInfo), &reflection, &error);
    if(real) GetReplay()->CacheShaderPipeline(PipelineState, desc, 2);
    desc->release();
    if(!real)
    {
      RDCERR("Failed to recreate Metal object/mesh pipeline: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    WrappedMTLRenderPipelineState *wrapped = NULL;
    GetResourceManager()->WrapResource(PipelineState, real, wrapped, true);
    AddResource(PipelineState, ResourceType::PipelineState, "Object/Mesh Pipeline State");
    GetReplay()->AddMeshPipeline(PipelineState, GetResID(meshFunction),
                                 GetResID(fragmentFunction), (uint32_t)sampleCount, reflection, colorFormats,
                                 GetResID(objectFunction));
    DerivedResource(objectFunction, PipelineState);
    DerivedResource(meshFunction, PipelineState);
    DerivedResource(fragmentFunction, PipelineState);
  }
  return true;
}

template bool WrappedMTLDevice::Serialise_newObjectMeshPipelineState(
    ReadSerialiser &, WrappedMTLRenderPipelineState *, WrappedMTLFunction *,
    WrappedMTLFunction *, WrappedMTLFunction *, rdcarray<uint32_t>, uint64_t,
    uint64_t, uint64_t, uint64_t, uint64_t, uint32_t, bool);
template bool WrappedMTLDevice::Serialise_newObjectMeshPipelineState(
    WriteSerialiser &, WrappedMTLRenderPipelineState *, WrappedMTLFunction *,
    WrappedMTLFunction *, WrappedMTLFunction *, rdcarray<uint32_t>, uint64_t,
    uint64_t, uint64_t, uint64_t, uint64_t, uint32_t, bool);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newComputePipelineStateWithFunction(
    SerialiserType &ser, WrappedMTLComputePipelineState *pipelineState,
    WrappedMTLFunction *computeFunction, NS::Error **error)
{
  SERIALISE_ELEMENT_LOCAL(ComputePipelineState, GetResID(pipelineState))
      .TypedAs("MTLComputePipelineState"_lit);
  SERIALISE_ELEMENT(computeFunction).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(ComputePipelineState == ResourceId() ||
       GetResourceManager()->HasResource(ComputePipelineState) || !computeFunction ||
       computeFunction->m_Type != eResFunction || !computeFunction->m_Real ||
       Unwrap(computeFunction)->functionType() != MTL::FunctionTypeKernel)
    {
      RDCERR("Invalid Metal compute pipeline identity or function");
      return false;
    }
    MTL::AutoreleasedComputePipelineReflection reflection = NULL;
    MTL::ComputePipelineState *realPipeline = Unwrap(this)->newComputePipelineState(
        Unwrap(computeFunction), MTL::PipelineOptionArgumentInfo, &reflection, error);
    if(!realPipeline)
    {
      RDCERR("Failed to recreate Metal compute pipeline");
      return false;
    }
    MTL::ComputePipelineDescriptor *editDescriptor = MTL::ComputePipelineDescriptor::alloc()->init();
    editDescriptor->setComputeFunction(Unwrap(computeFunction));
    GetReplay()->CacheShaderPipeline(ComputePipelineState, editDescriptor, 1);
    editDescriptor->release();
    WrappedMTLComputePipelineState *wrappedPipeline;
    GetResourceManager()->WrapResource(ComputePipelineState, realPipeline, wrappedPipeline, true);
    AddResource(ComputePipelineState, ResourceType::PipelineState, "Compute Pipeline State");
    DerivedResource(computeFunction, ComputePipelineState);
    GetReplay()->AddComputePipeline(ComputePipelineState, GetResID(computeFunction), reflection,
                                    realPipeline);
  }
  return true;
}

WrappedMTLComputePipelineState *WrappedMTLDevice::newComputePipelineStateWithFunction(
    WrappedMTLFunction *computeFunction, NS::Error **error)
{
  MTL::ComputePipelineState *realPipeline;
  SERIALISE_TIME_CALL(realPipeline =
                          Unwrap(this)->newComputePipelineState(Unwrap(computeFunction), error));
  if(!realPipeline)
    return NULL;

  WrappedMTLComputePipelineState *wrappedPipeline;
  GetResourceManager()->WrapResource(ResourceId(), realPipeline, wrappedPipeline);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newComputePipelineStateWithFunction);
    Serialise_newComputePipelineStateWithFunction(ser, wrappedPipeline, computeFunction, error);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedPipeline);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(computeFunction));
  }
  return wrappedPipeline;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newTextureWithDescriptor(SerialiserType &ser,
                                                          WrappedMTLTexture *texture,
                                                          RDMTL::TextureDescriptor &descriptor)
{
  SERIALISE_ELEMENT_LOCAL(Texture, GetResID(texture)).TypedAs("MTLTexture"_lit);
  SERIALISE_ELEMENT(descriptor);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    const bool cube = descriptor.textureType == MTL::TextureTypeCube ||
                      descriptor.textureType == MTL::TextureTypeCubeArray;
    if(!ValidTextureMipCount(descriptor.width, descriptor.height, descriptor.depth,
                             descriptor.mipmapLevelCount) ||
       (cube && (descriptor.width != descriptor.height || descriptor.depth != 1 ||
                 !descriptor.arrayLength || descriptor.sampleCount != 1 ||
                 (descriptor.textureType == MTL::TextureTypeCube && descriptor.arrayLength != 1))))
    {
      RDCERR("Invalid Metal texture dimensions or mip count");
      return false;
    }
    // Ensure the created textures can be read by a shader
    // Metal driver will treat TextureUsageUnknown as all options
    if(descriptor.usage != MTL::TextureUsageUnknown)
      descriptor.usage = (MTL::TextureUsage)(descriptor.usage | MTL::TextureUsageShaderRead);

    MTL::TextureDescriptor *mtlDescriptor(descriptor);
    // Debugger-created transfers/inspection need resource dependencies even
    // when the application opted out of automatic tracking. Preserve captured
    // fences; strengthen tracking only on this standalone replay allocation.
    // Heap allocations keep their separately validated heap tracking contract.
    if(descriptor.hazardTrackingMode == MTL::HazardTrackingModeUntracked)
      mtlDescriptor->setHazardTrackingMode(MTL::HazardTrackingModeTracked);
    // Like Vulkan's non-transient debug allocations, give transient targets
    // backing memory so partial replay and discard diagnostics can preserve them.
    // Captured load/store/resolve operations and ResourceIds remain unchanged.
    if(descriptor.storageMode == MTL::StorageModeMemoryless)
      mtlDescriptor->setStorageMode(MTL::StorageModePrivate);
    MTL::Texture *realMTLTexture = Unwrap(this)->newTexture(mtlDescriptor);
    mtlDescriptor->release();
    if(!realMTLTexture) return false;
    WrappedMTLTexture *wrappedMTLTexture;
    ResourceId liveID =
        GetResourceManager()->WrapResource(Texture, realMTLTexture, wrappedMTLTexture, true);

    AddResource(Texture, ResourceType::Texture, "Texture");
    GetReplay()->AddTexture(Texture, realMTLTexture, false, descriptor.storageMode);
    DerivedResource(this, Texture);
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newSharedTextureWithDescriptor(
    SerialiserType &ser, WrappedMTLTexture *texture, RDMTL::TextureDescriptor &descriptor)
{
  SERIALISE_ELEMENT_LOCAL(Texture, GetResID(texture)).TypedAs("MTLTexture"_lit).Important();
  SERIALISE_ELEMENT(descriptor).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(Texture == ResourceId() || GetResourceManager()->HasResource(Texture) ||
       descriptor.storageMode != MTL::StorageModePrivate ||
       descriptor.textureType != MTL::TextureType2D ||
       (descriptor.pixelFormat != MTL::PixelFormatRGBA8Unorm &&
        descriptor.pixelFormat != MTL::PixelFormatBGRA8Unorm) ||
       !descriptor.width || !descriptor.height || descriptor.width > 8192 ||
       descriptor.height > 8192 || descriptor.depth != 1 ||
       descriptor.mipmapLevelCount != 1 || descriptor.arrayLength != 1 ||
       descriptor.sampleCount != 1 ||
       descriptor.resourceOptions != MTL::ResourceStorageModePrivate ||
       descriptor.cpuCacheMode != MTL::CPUCacheModeDefaultCache ||
       descriptor.hazardTrackingMode != MTL::HazardTrackingModeDefault ||
       (uint64_t(descriptor.usage) & ~uint64_t(7)) != 0 ||
       descriptor.swizzle.red != MTL::TextureSwizzleRed ||
       descriptor.swizzle.green != MTL::TextureSwizzleGreen ||
       descriptor.swizzle.blue != MTL::TextureSwizzleBlue ||
       descriptor.swizzle.alpha != MTL::TextureSwizzleAlpha)
    {
      RDCERR("Invalid or unsupported Metal shared texture descriptor or identity");
      return false;
    }
    if(descriptor.usage != MTL::TextureUsageUnknown)
      descriptor.usage = (MTL::TextureUsage)(descriptor.usage | MTL::TextureUsageShaderRead);
    MTL::TextureDescriptor *nativeDescriptor(descriptor);
    MTL::Texture *real = Unwrap(this)->newSharedTexture(nativeDescriptor);
    nativeDescriptor->release();
    if(!real)
    {
      RDCERR("Metal failed to recreate descriptor-backed shared texture");
      return false;
    }
    WrappedMTLTexture *wrapped = NULL;
    GetResourceManager()->WrapResource(Texture, real, wrapped, true);
    AddResource(Texture, ResourceType::Texture, "Shared Texture");
    GetReplay()->AddTexture(Texture, real, false);
    DerivedResource(this, Texture);
  }
  return true;
}

WrappedMTLTexture *WrappedMTLDevice::newSharedTextureWithDescriptor(
    RDMTL::TextureDescriptor &descriptor)
{
  MTL::TextureDescriptor *nativeDescriptor(descriptor);
  MTL::Texture *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newSharedTexture(nativeDescriptor));
  nativeDescriptor->release();
  if(!real) return NULL;
  WrappedMTLTexture *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newSharedTextureWithDescriptor);
    Serialise_newSharedTextureWithDescriptor(ser, wrapped, descriptor);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newSharedTextureWithHandle(
    SerialiserType &ser, WrappedMTLTexture *texture, WrappedMTLTexture *source)
{
  SERIALISE_ELEMENT_LOCAL(Texture, GetResID(texture)).TypedAs("MTLTexture"_lit).Important();
  SERIALISE_ELEMENT_LOCAL(Source, GetResID(source)).TypedAs("MTLTexture"_lit).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(Texture == ResourceId() || GetResourceManager()->HasResource(Texture) ||
       Source == ResourceId() || !GetResourceManager()->HasResource(Source))
    {
      RDCERR("Invalid or unsupported Metal shared texture handle import identity");
      return false;
    }
    source = (WrappedMTLTexture *)GetResourceManager()->GetResource(Source);
    if(!source || source->m_Type != eResTexture || !source->m_Real ||
       source->GetDevice() != (MTL::Device *)this)
    {
      RDCERR("Invalid Metal shared texture handle source or device");
      return false;
    }
    MTL::SharedTextureHandle *handle = Unwrap(source)->newSharedTextureHandle();
    if(!handle)
    {
      RDCERR("Metal failed to export shared texture handle for replay import");
      return false;
    }
    MTL::Texture *real = Unwrap(this)->newSharedTexture(handle);
    handle->release();
    if(!real)
    {
      RDCERR("Metal failed to recreate shared texture handle import");
      return false;
    }
    WrappedMTLTexture *wrapped = NULL;
    GetResourceManager()->WrapResource(Texture, real, wrapped, true);
    AddResource(Texture, ResourceType::Texture, "Shared Texture Import");
    GetReplay()->AddTexture(Texture, real, false);
    DerivedResource(source, Texture);
  }
  return true;
}

WrappedMTLTexture *WrappedMTLDevice::WrapNewSharedTextureWithHandle(
    MTL::Texture *real, WrappedMTLTexture *source)
{
  if(!real) return NULL;
  WrappedMTLTexture *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newSharedTextureWithHandle);
    Serialise_newSharedTextureWithHandle(ser, wrapped, source);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    if(source && source->m_Type == eResTexture)
      record->AddParent(GetRecord(source));
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newSharedTextureWithHandle(
    ReadSerialiser &ser, WrappedMTLTexture *texture, WrappedMTLTexture *source);
template bool WrappedMTLDevice::Serialise_newSharedTextureWithHandle(
    WriteSerialiser &ser, WrappedMTLTexture *texture, WrappedMTLTexture *source);

WrappedMTLTexture *WrappedMTLDevice::newTextureWithDescriptor(RDMTL::TextureDescriptor &descriptor)
{
  return Common_NewTexture(descriptor, MetalChunk::MTLDevice_newTextureWithDescriptor, false, NULL,
                           0);
}

WrappedMTLTexture *WrappedMTLDevice::newTextureWithDescriptor(RDMTL::TextureDescriptor &descriptor,
                                                              IOSurfaceRef iosurface,
                                                              NS::UInteger plane)
{
  bool nextDrawable = (bool)(uintptr_t)Threading::GetTLSValue(g_nextDrawableTLSSlot);
  return Common_NewTexture(descriptor,
                           nextDrawable ? MetalChunk::MTLDevice_newTextureWithDescriptor_nextDrawable
                                        : MetalChunk::MTLDevice_newTextureWithDescriptor_iosurface,
                           true, iosurface, plane);
}

// Non-Serialised MTLDevice APIs

bool WrappedMTLDevice::isDepth24Stencil8PixelFormatSupported()
{
  return Unwrap(this)->depth24Stencil8PixelFormatSupported();
}

MTL::ReadWriteTextureTier WrappedMTLDevice::readWriteTextureSupport()
{
  return Unwrap(this)->readWriteTextureSupport();
}

MTL::ArgumentBuffersTier WrappedMTLDevice::argumentBuffersSupport()
{
  return Unwrap(this)->argumentBuffersSupport();
}

bool WrappedMTLDevice::areRasterOrderGroupsSupported()
{
  return Unwrap(this)->rasterOrderGroupsSupported();
}

bool WrappedMTLDevice::supports32BitFloatFiltering()
{
  return Unwrap(this)->supports32BitFloatFiltering();
}

bool WrappedMTLDevice::supports32BitMSAA()
{
  return Unwrap(this)->supports32BitMSAA();
}

bool WrappedMTLDevice::supportsQueryTextureLOD()
{
  return Unwrap(this)->supportsQueryTextureLOD();
}

bool WrappedMTLDevice::supportsBCTextureCompression()
{
  return Unwrap(this)->supportsBCTextureCompression();
}

bool WrappedMTLDevice::supportsPullModelInterpolation()
{
  return Unwrap(this)->supportsPullModelInterpolation();
}

bool WrappedMTLDevice::areBarycentricCoordsSupported()
{
  return Unwrap(this)->barycentricCoordsSupported();
}

bool WrappedMTLDevice::supportsShaderBarycentricCoordinates()
{
  return Unwrap(this)->supportsShaderBarycentricCoordinates();
}

bool WrappedMTLDevice::supportsFeatureSet(MTL::FeatureSet featureSet)
{
  return Unwrap(this)->supportsFeatureSet(featureSet);
}

bool WrappedMTLDevice::supportsFamily(MTL::GPUFamily gpuFamily)
{
  return Unwrap(this)->supportsFamily(gpuFamily);
}

bool WrappedMTLDevice::supportsTextureSampleCount(NS::UInteger sampleCount)
{
  return Unwrap(this)->supportsTextureSampleCount(sampleCount);
}

bool WrappedMTLDevice::areProgrammableSamplePositionsSupported()
{
  return Unwrap(this)->programmableSamplePositionsSupported();
}

bool WrappedMTLDevice::supportsRasterizationRateMapWithLayerCount(NS::UInteger layerCount)
{
  return Unwrap(this)->supportsRasterizationRateMap(layerCount);
}

bool WrappedMTLDevice::supportsCounterSampling(MTL::CounterSamplingPoint samplingPoint)
{
  return Unwrap(this)->supportsCounterSampling(samplingPoint);
}

bool WrappedMTLDevice::supportsVertexAmplificationCount(NS::UInteger count)
{
  return Unwrap(this)->supportsVertexAmplificationCount(count);
}

bool WrappedMTLDevice::supportsDynamicLibraries()
{
  return Unwrap(this)->supportsDynamicLibraries();
}

bool WrappedMTLDevice::supportsRenderDynamicLibraries()
{
  return Unwrap(this)->supportsRenderDynamicLibraries();
}

bool WrappedMTLDevice::supportsRaytracing()
{
  // RD device does not support ray tracing
  return false;
}

bool WrappedMTLDevice::supportsFunctionPointers()
{
  return Unwrap(this)->supportsFunctionPointers();
}

bool WrappedMTLDevice::supportsFunctionPointersFromRender()
{
  return Unwrap(this)->supportsFunctionPointersFromRender();
}

bool WrappedMTLDevice::supportsRaytracingFromRender()
{
  // RD device does not support ray tracing
  return false;
}

bool WrappedMTLDevice::supportsPrimitiveMotionBlur()
{
  return Unwrap(this)->supportsPrimitiveMotionBlur();
}

bool WrappedMTLDevice::shouldMaximizeConcurrentCompilation()
{
  return Unwrap(this)->shouldMaximizeConcurrentCompilation();
}

NS::UInteger WrappedMTLDevice::maximumConcurrentCompilationTaskCount()
{
  return Unwrap(this)->maximumConcurrentCompilationTaskCount();
}

// End of MTLDevice APIs

WrappedMTLTexture *WrappedMTLDevice::Common_NewTexture(RDMTL::TextureDescriptor &descriptor,
                                                       MetalChunk chunkType, bool ioSurfaceTexture,
                                                       IOSurfaceRef iosurface, NS::UInteger plane)
{
  MTL::Texture *realMTLTexture;
  MTL::TextureDescriptor *realDescriptor(descriptor);
  // Ensure the created textures can be read by a shader
  // Metal driver will treat TextureUsageUnknown as all options
  MTL::TextureUsage usage = realDescriptor->usage();
  if(usage != MTL::TextureUsageUnknown)
    realDescriptor->setUsage((MTL::TextureUsage)(usage | MTL::TextureUsageShaderRead));

  SERIALISE_TIME_CALL(realMTLTexture = !ioSurfaceTexture ? Unwrap(this)->newTexture(realDescriptor)
                                                         : Unwrap(this)->newTexture(
                                                               realDescriptor, iosurface, plane));
  realDescriptor->release();
  WrappedMTLTexture *wrappedMTLTexture;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), realMTLTexture, wrappedMTLTexture);
  if(IsCaptureMode(m_State))
  {
    RDMTL::TextureDescriptor rdDescriptor(descriptor);
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(chunkType);
      Serialise_newTextureWithDescriptor(ser, wrappedMTLTexture, rdDescriptor);
      chunk = scope.Get();
    }
    MetalResourceRecord *textureRecord = GetResourceManager()->AddResourceRecord(wrappedMTLTexture);
    textureRecord->AddChunk(chunk);
    if((realMTLTexture->storageMode() == MTL::StorageModePrivate ||
        realMTLTexture->storageMode() == MTL::StorageModeShared ||
        realMTLTexture->storageMode() == MTL::StorageModeManaged) && !ioSurfaceTexture)
      GetResourceManager()->MarkDirtyResource(id);
  }
  if(ioSurfaceTexture)
  {
    if(IsCaptureMode(m_State))
    {
      {
        SCOPED_LOCK(m_CapturePotentialBackBuffersLock);
        m_CapturePotentialBackBuffers.insert(wrappedMTLTexture);
      }
    }
  }
  return wrappedMTLTexture;
}

WrappedMTLTexture *WrappedMTLDevice::WrapDrawableTexture(MTL::Texture *realTexture)
{
  RDMTL::TextureDescriptor descriptor;
  descriptor.textureType = realTexture->textureType();
  descriptor.pixelFormat = realTexture->pixelFormat();
  descriptor.width = realTexture->width();
  descriptor.height = realTexture->height();
  descriptor.depth = realTexture->depth();
  descriptor.mipmapLevelCount = realTexture->mipmapLevelCount();
  descriptor.sampleCount = realTexture->sampleCount();
  descriptor.arrayLength = realTexture->arrayLength();
  descriptor.resourceOptions = realTexture->resourceOptions();
  descriptor.cpuCacheMode = realTexture->cpuCacheMode();
  descriptor.storageMode = realTexture->storageMode();
  descriptor.hazardTrackingMode = realTexture->hazardTrackingMode();
  descriptor.usage = realTexture->usage();
  descriptor.allowGPUOptimizedContents = realTexture->allowGPUOptimizedContents();
  descriptor.swizzle = realTexture->swizzle();

  WrappedMTLTexture *wrappedMTLTexture;
  GetResourceManager()->WrapResource(ResourceId(), realTexture, wrappedMTLTexture);

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newTextureWithDescriptor_nextDrawable);
      Serialise_newTextureWithDescriptor(ser, wrappedMTLTexture, descriptor);
      chunk = scope.Get();
    }

    MetalResourceRecord *textureRecord =
        GetResourceManager()->AddResourceRecord(wrappedMTLTexture);
    textureRecord->AddChunk(chunk);

    // A retained drawable may still be a bindless source in the next capture.
    // Snapshot its GPU contents at capture start like other background images;
    // its initial pixels cannot be inferred from the current frame's presentation.
    if(!realTexture->framebufferOnly() &&
       (realTexture->storageMode() == MTL::StorageModePrivate ||
        realTexture->storageMode() == MTL::StorageModeShared ||
        realTexture->storageMode() == MTL::StorageModeManaged))
    {
      GetResourceManager()->MarkDirtyResource(GetResID(wrappedMTLTexture));
      // A newly acquired drawable can Load pixels from its previous presentation.
      // It did not exist at StartFrameCapture, so preserve its acquired contents
      // before returning it to the application and before any frame encoding.
      if(IsActiveCapturing(m_State)) {
        const bool preserved=Prepare_InitialState(wrappedMTLTexture);
        if(getenv("RENDERDOC_METAL_TRACE_DRAWABLE_INITIAL"))
          fprintf(stderr,"Metal acquired drawable initial: id=%s width=%llu height=%llu format=%llu preserved=%d\n",
              ToStr(GetResID(wrappedMTLTexture)).c_str(),(unsigned long long)realTexture->width(),
              (unsigned long long)realTexture->height(),(unsigned long long)realTexture->pixelFormat(),preserved);
        if(!preserved) RDCERR("Failed to preserve acquired Metal drawable initial contents %s",
                             ToStr(GetResID(wrappedMTLTexture)).c_str());
      }
    }

    SCOPED_LOCK(m_CapturePotentialBackBuffersLock);
    m_CapturePotentialBackBuffers.insert(wrappedMTLTexture);
  }

  return wrappedMTLTexture;
}

WrappedMTLBuffer *WrappedMTLDevice::Common_NewBuffer(bool withBytes, const void *pointer,
                                                     NS::UInteger length,
                                                     MTL::ResourceOptions options)
{
  SCOPED_READLOCK(m_CapTransitionLock);
  MTL::Buffer *realMTLBuffer;
  SERIALISE_TIME_CALL(realMTLBuffer = withBytes ? Unwrap(this)->newBuffer(pointer, length, options)
                                                : Unwrap(this)->newBuffer(length, options));

  WrappedMTLBuffer *wrappedMTLBuffer;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), realMTLBuffer, wrappedMTLBuffer);
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(withBytes ? MetalChunk::MTLDevice_newBufferWithBytes
                                       : MetalChunk::MTLDevice_newBufferWithLength);
      Serialise_newBufferWithBytes(ser, wrappedMTLBuffer, pointer, length, options);
      chunk = scope.Get();
    }

    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLBuffer);
    record->AddChunk(chunk);
    if(IsActiveCapturing(m_State))
    {
      AddFrameCaptureRecordChunk(chunk->Duplicate());
      RegisterCapturedFrameResource(id);
    }

    MTL::StorageMode mode = realMTLBuffer->storageMode();
    record->bufInfo = new MetalBufferInfo(mode);

    // Create CPU side tracking info for CPU shared buffers
    if(mode == MTL::StorageModeShared)
    {
      record->bufInfo->data = (byte *)realMTLBuffer->contents();
      record->bufInfo->length = realMTLBuffer->length();
    }
    // Snapshot GPU only buffers
    else if(mode == MTL::StorageModePrivate)
    {
      GetResourceManager()->MarkDirtyResource(id);
    }
  }
  else
  {
    // TODO: implement RD MTL replay
  }
  return wrappedMTLBuffer;
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLDevice, bool, MTLCreateSystemDefaultDevice);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLCommandQueue *,
                                            newCommandQueue);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLCommandQueue *,
                                            newCommandQueue,
                                            NS::UInteger maxCommandBufferCount);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLLibrary *, newDefaultLibrary);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLLibrary *,
                                            newLibraryWithSource, NS::String *source,
                                            MTL::CompileOptions *options, NS::Error **error);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLDepthStencilState *,
                                            newDepthStencilStateWithDescriptor,
                                            RDMTL::DepthStencilDescriptor &descriptor);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLSamplerState *,
                                            newSamplerStateWithDescriptor,
                                            RDMTL::SamplerDescriptor &descriptor);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(
    WrappedMTLDevice, WrappedMTLIndirectCommandBuffer *,
    newIndirectCommandBufferWithDescriptor, MTL::IndirectCommandType, bool, bool,
    NS::UInteger, NS::UInteger, NS::UInteger, MTL::ResourceOptions);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice,
                                            WrappedMTLRenderPipelineState *renderPipelineState,
                                            newRenderPipelineStateWithDescriptor,
                                            RDMTL::RenderPipelineDescriptor &descriptor,
                                            NS::Error **error);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice,
                                            WrappedMTLComputePipelineState *computePipelineState,
                                            newComputePipelineStateWithFunction,
                                            WrappedMTLFunction *computeFunction, NS::Error **error);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLTexture *,
                                            newTextureWithDescriptor,
                                            RDMTL::TextureDescriptor &descriptor);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLTexture *,
                                            newSharedTextureWithDescriptor,
                                            RDMTL::TextureDescriptor &descriptor);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLBuffer *,
                                            newBufferWithBytes, const void *pointer,
                                            NS::UInteger length, MTL::ResourceOptions options);
