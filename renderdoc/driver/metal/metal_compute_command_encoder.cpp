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

#include "metal_compute_command_encoder.h"
#include "metal_fence.h"
#include <cmath>
#include "metal_command_buffer.h"
#include "metal_compute_pipeline_state.h"
#include "metal_visible_function_table.h"
#include "metal_replay.h"
#include "metal_resource_commands.h"
#include "metal_texture.h"
#include "metal_buffer.h"
#include "metal_heap.h"
#include "metal_sampler_state.h"
#include "metal_acceleration_structure.h"

WrappedMTLComputeCommandEncoder::WrappedMTLComputeCommandEncoder(MTL::ComputeCommandEncoder *real,
                                                                 ResourceId id,
                                                                 WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  m_CaptureIndirectArguments = IsCaptureMode(m_State) &&
      !Process::GetEnvVariable("RENDERDOC_METAL_CAPTURE_INDIRECT_ARGUMENTS").empty();
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_declareHeaps(
    SerialiserType &ser, rdcarray<WrappedMTLHeap *> heaps, bool arrayVariant)
{
  const bool expectedArrayVariant = arrayVariant;
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(heaps).Important();
  SERIALISE_ELEMENT(arrayVariant).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder) ||
       !Unwrap(ComputeCommandEncoder) || heaps.size() > 32 ||
       arrayVariant != expectedArrayVariant ||
       (!arrayVariant && heaps.size() != 1))
    {
      RDCERR("Invalid Metal compute heap declaration shape or encoder");
      return false;
    }
    rdcarray<const MTL::Heap *> real;
    for(WrappedMTLHeap *heap : heaps)
    {
      if(!heap || heap->m_Type != eResHeap || !heap->m_Real || heap->m_Device != m_Device)
      {
        RDCERR("Invalid Metal compute heap declaration resource identity");
        return false;
      }
      real.push_back(Unwrap(heap));
    }
    if(!arrayVariant)
      Unwrap(ComputeCommandEncoder)->useHeap(real[0]);
    else if(!real.empty())
      Unwrap(ComputeCommandEncoder)->useHeaps(real.data(), real.size());
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::declareHeaps(rdcarray<WrappedMTLHeap *> heaps,
                                                   bool arrayVariant)
{
  if(heaps.size() > 32 || (!arrayVariant && heaps.size() != 1))
  {
    RDCERR("Invalid Metal compute heap declaration count");
    return;
  }
  rdcarray<const MTL::Heap *> real;
  for(WrappedMTLHeap *heap : heaps)
  {
    if(!heap || heap->m_Type != eResHeap || !heap->m_Real || heap->m_Device != m_Device)
    {
      RDCERR("Invalid Metal compute heap declaration resource");
      return;
    }
    real.push_back(Unwrap(heap));
  }
  auto invoke = [&]() {
    if(!arrayVariant)
      Unwrap(this)->useHeap(real[0]);
    else if(!real.empty())
      Unwrap(this)->useHeaps(real.data(), real.size());
  };
  SERIALISE_TIME_CALL(invoke());
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(arrayVariant ? MetalChunk::MTLComputeCommandEncoder_useHeaps :
                                          MetalChunk::MTLComputeCommandEncoder_useHeap);
    Serialise_declareHeaps(ser, heaps, arrayVariant);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLHeap *heap : heaps)
    {
      record->AddParent(GetRecord(heap));
      record->MarkResourceFrameReferenced(GetResID(heap), eFrameRef_Read);
    }
  }
}

template bool WrappedMTLComputeCommandEncoder::Serialise_declareHeaps(
    ReadSerialiser &, rdcarray<WrappedMTLHeap *>, bool);
template bool WrappedMTLComputeCommandEncoder::Serialise_declareHeaps(
    WriteSerialiser &, rdcarray<WrappedMTLHeap *>, bool);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_useResource(SerialiserType &ser, WrappedMTLResource *resource, MTL::ResourceUsage usage)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(resource).Important();
  SERIALISE_ELEMENT_LOCAL(usageValue, (uint64_t)usage).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder) ||
       !Unwrap(ComputeCommandEncoder) || !ValidMetalResourceUsage(usageValue) ||
       !ValidMetalResidencyResource(m_Device, resource))
    {
      RDCERR("Invalid Metal compute resource declaration");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->useResource(Unwrap(resource), (MTL::ResourceUsage)usageValue);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::useResource(WrappedMTLResource *resource, MTL::ResourceUsage usage)
{
  SERIALISE_TIME_CALL(Unwrap(this)->useResource(Unwrap(resource), usage));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_useResource);
    Serialise_useResource(ser, resource, usage);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    ReferenceMetalCommandResources(record, {resource}, (usage & MTL::ResourceUsageWrite) != 0);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, useResource, WrappedMTLResource *resource, MTL::ResourceUsage usage);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_useResources(SerialiserType &ser, rdcarray<WrappedMTLResource *> resources, MTL::ResourceUsage usage)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(resources).Important();
  SERIALISE_ELEMENT_LOCAL(usageValue, (uint64_t)usage).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder) ||
       !Unwrap(ComputeCommandEncoder) || !ValidMetalResourceUsage(usageValue) ||
       !ValidMetalResidencyResources(m_Device, resources))
    {
      RDCERR("Invalid Metal compute resource declaration");
      return false;
    }
    const auto real = UnwrapMetalResources(resources);
    if(!real.empty())
      Unwrap(ComputeCommandEncoder)->useResources(real.data(), real.size(), (MTL::ResourceUsage)usageValue);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::useResources(rdcarray<WrappedMTLResource *> resources, MTL::ResourceUsage usage)
{
  const auto real = UnwrapMetalResources(resources);
  if(!real.empty())
  {
    SERIALISE_TIME_CALL(Unwrap(this)->useResources(real.data(), real.size(), usage));
  }
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_useResources);
    Serialise_useResources(ser, resources, usage);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    ReferenceMetalCommandResources(record, resources, (usage & MTL::ResourceUsageWrite) != 0);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, useResources, rdcarray<WrappedMTLResource *> resources, MTL::ResourceUsage usage);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_memoryBarrierWithScope(SerialiserType &ser, MTL::BarrierScope barrierScope)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT_LOCAL(scopeValue, (uint64_t)barrierScope).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!ComputeCommandEncoder || scopeValue == 0 || (scopeValue & ~uint64_t(MTL::BarrierScopeBuffers | MTL::BarrierScopeTextures)) != 0)
    {
      RDCERR("Invalid Metal compute memory barrier");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->memoryBarrier((MTL::BarrierScope)scopeValue);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::memoryBarrierWithScope(MTL::BarrierScope barrierScope)
{
  SERIALISE_TIME_CALL(Unwrap(this)->memoryBarrier(barrierScope));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_memoryBarrierWithScope);
    Serialise_memoryBarrierWithScope(ser, barrierScope);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, memoryBarrierWithScope, MTL::BarrierScope scope);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_memoryBarrierWithResources(SerialiserType &ser, rdcarray<WrappedMTLResource *> resources)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(resources).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!ComputeCommandEncoder || !ValidMetalCommandResources(m_Device, resources))
    {
      RDCERR("Invalid Metal compute memory barrier");
      return false;
    }
    const auto real = UnwrapMetalResources(resources);
    if(!real.empty())
      Unwrap(ComputeCommandEncoder)->memoryBarrier(real.data(), real.size());
    if(IsLoading(m_State))
      for(auto resource : resources)
        m_Device->GetReplay()->AddUsage(GetResID(resource), ResourceUsage::Barrier,
                                       m_Device->GetReplay()->GetNextEventID());
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::memoryBarrierWithResources(rdcarray<WrappedMTLResource *> resources)
{
  const auto real = UnwrapMetalResources(resources);
  if(!real.empty())
  {
    SERIALISE_TIME_CALL(Unwrap(this)->memoryBarrier(real.data(), real.size()));
  }
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_memoryBarrierWithResources);
    Serialise_memoryBarrierWithResources(ser, resources);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    ReferenceMetalCommandResources(record, resources, false);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, memoryBarrierWithResources, rdcarray<WrappedMTLResource *> resources);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_pushDebugGroup(SerialiserType &ser, NS::String *string)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(string).Important();
  SERIALISE_CHECK_READ_ERRORS();
  // Select the owning command context before recording marker state and hierarchy.
  if(IsReplayingAndReading() &&
     (!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
      !ComputeCommandEncoder->m_Real ||
      ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder)))
    return false;
  if(IsLoading(m_State))
  {
    AddEvent();
    m_Device->GetReplay()->AddDebugGroup(string, ActionFlags::PushMarker);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::pushDebugGroup(NS::String *string)
{
  SERIALISE_TIME_CALL(Unwrap(this)->pushDebugGroup(string));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_pushDebugGroup);
    Serialise_pushDebugGroup(ser, string);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, pushDebugGroup, NS::String *string);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_insertDebugSignpost(SerialiserType &ser, NS::String *string)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(string).Important();
  SERIALISE_CHECK_READ_ERRORS();
  // Select the owning command context before recording marker state and hierarchy.
  if(IsReplayingAndReading() &&
     (!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
      !ComputeCommandEncoder->m_Real ||
      ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder)))
    return false;
  if(IsLoading(m_State))
  {
    AddEvent();
    m_Device->GetReplay()->AddDebugGroup(string, ActionFlags::SetMarker);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::insertDebugSignpost(NS::String *string)
{
  SERIALISE_TIME_CALL(Unwrap(this)->insertDebugSignpost(string));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_insertDebugSignpost);
    Serialise_insertDebugSignpost(ser, string);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, insertDebugSignpost, NS::String *string);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_popDebugGroup(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_CHECK_READ_ERRORS();
  // Select the owning command context before recording marker state and hierarchy.
  if(IsReplayingAndReading() &&
     (!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
      !ComputeCommandEncoder->m_Real ||
      ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder)))
    return false;
  if(IsLoading(m_State))
  {
    AddEvent();
    m_Device->GetReplay()->AddDebugGroup(NULL, ActionFlags::PopMarker);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::popDebugGroup()
{
  SERIALISE_TIME_CALL(Unwrap(this)->popDebugGroup());
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_popDebugGroup);
    Serialise_popDebugGroup(ser);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, popDebugGroup);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_updateFence(
    SerialiserType &ser, WrappedMTLFence *fence)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(fence).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder) ||
       ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
       !ComputeCommandEncoder->m_Real || !ValidMetalFence(fence))
    {
      RDCERR("Invalid Metal compute updateFence resource, stage or dependency");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->updateFence(Unwrap(fence));
    fence->Updated(m_Device->GetReplayEpoch(), GetResID(ComputeCommandEncoder));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::updateFence(WrappedMTLFence *fence)
{
  SERIALISE_TIME_CALL(Unwrap(this)->updateFence(Unwrap(fence)));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_updateFence);
    Serialise_updateFence(ser, fence);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(fence), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, updateFence,
                                WrappedMTLFence *fence);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_waitForFence(
    SerialiserType &ser, WrappedMTLFence *fence)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(fence).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder) ||
       ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
       !ComputeCommandEncoder->m_Real || !ValidMetalFence(fence) ||
       !fence->CanWait(m_Device->GetReplayEpoch(), GetResID(ComputeCommandEncoder)))
    {
      RDCERR("Invalid Metal compute waitForFence resource, stage or dependency");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->waitForFence(Unwrap(fence));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::waitForFence(WrappedMTLFence *fence)
{
  SERIALISE_TIME_CALL(Unwrap(this)->waitForFence(Unwrap(fence)));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_waitForFence);
    Serialise_waitForFence(ser, fence);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(fence), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, waitForFence,
                                WrappedMTLFence *fence);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_endEncoding(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    Unwrap(ComputeCommandEncoder)->endEncoding();
    m_Device->SetReplayComputeCommandEncoder(NULL);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "End Metal Compute Pass";
      action.flags = ActionFlags::PassBoundary | ActionFlags::EndPass;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::endEncoding()
{
  SERIALISE_TIME_CALL(Unwrap(this)->endEncoding());
  m_IndirectCapture.Clear();
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_endEncoding);
    Serialise_endEncoding(ser);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setComputePipelineState(
    SerialiserType &ser, WrappedMTLComputePipelineState *pipeline)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(pipeline).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!pipeline)
      return false;
    Unwrap(ComputeCommandEncoder)->setComputePipelineState(Unwrap(pipeline));
    m_Pipeline = pipeline;
    m_Device->GetReplay()->SetComputePipeline(GetResID(pipeline));
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setVisibleFunctionTable(
    SerialiserType &ser, WrappedMTLVisibleFunctionTable *table, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this).Important();
  SERIALISE_ELEMENT(table).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
       !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder) || index >= 31 ||
       (table && (table->m_Type != eResVisibleFunctionTable || !table->m_Real ||
                  table->m_Pipeline != m_Pipeline || table->m_Stage != (MTL::RenderStages)0)))
    {
      RDCERR("Invalid Metal compute visible-function-table binding");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->setVisibleFunctionTable(Unwrap(table), index);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setVisibleFunctionTable(
    WrappedMTLVisibleFunctionTable *table, uint32_t index)
{
  if(m_CaptureIndirectArguments) m_IndirectCapture.InvalidateSlot((uint32_t)index);
  SERIALISE_TIME_CALL(Unwrap(this)->setVisibleFunctionTable(Unwrap(table), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setVisibleFunctionTable);
    Serialise_setVisibleFunctionTable(ser, table, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLComputeCommandEncoder::Serialise_setVisibleFunctionTable(
    ReadSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t);
template bool WrappedMTLComputeCommandEncoder::Serialise_setVisibleFunctionTable(
    WriteSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setVisibleFunctionTables(
    SerialiserType &ser, rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this).Important();
  SERIALISE_ELEMENT(tables).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
       !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder) ||
       range.length == 0 || range.length > 31 || range.location > 31 - range.length ||
       tables.size() != range.length)
    {
      RDCERR("Invalid Metal compute visible-function-table range");
      return false;
    }
    rdcarray<const MTL::VisibleFunctionTable *> native;
    native.reserve(tables.size());
    for(WrappedMTLVisibleFunctionTable *table : tables)
    {
      if(table && (table->m_Type != eResVisibleFunctionTable || !table->m_Real ||
                   table->m_Stage != (MTL::RenderStages)0 ||
                   table->m_Pipeline != m_Pipeline))
      {
        RDCERR("Invalid Metal compute visible-function-table member");
        return false;
      }
      native.push_back(Unwrap(table));
    }
    Unwrap(ComputeCommandEncoder)->setVisibleFunctionTables(native.data(), range);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setVisibleFunctionTables(
    rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range)
{
  if(m_CaptureIndirectArguments)
    for(NS::UInteger i=0; i<range.length && i<2; i++)
      m_IndirectCapture.InvalidateSlot((uint32_t)(range.location+i));
  if(range.length == 0 || range.length > 31 || tables.size() != range.length)
  {
    RDCERR("Unsupported Metal compute visible-function-table range");
    return;
  }
  rdcarray<const MTL::VisibleFunctionTable *> native;
  native.reserve(tables.size());
  for(WrappedMTLVisibleFunctionTable *table : tables) native.push_back(Unwrap(table));
  SERIALISE_TIME_CALL(Unwrap(this)->setVisibleFunctionTables(native.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setVisibleFunctionTables);
    Serialise_setVisibleFunctionTables(ser, tables, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLVisibleFunctionTable *table : tables)
      if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLComputeCommandEncoder::Serialise_setVisibleFunctionTables(
    ReadSerialiser &, rdcarray<WrappedMTLVisibleFunctionTable *>, NS::Range);
template bool WrappedMTLComputeCommandEncoder::Serialise_setVisibleFunctionTables(
    WriteSerialiser &, rdcarray<WrappedMTLVisibleFunctionTable *>, NS::Range);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setIntersectionFunctionTable(
    SerialiserType &ser, WrappedMTLIntersectionFunctionTable *table, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this).Important();
  SERIALISE_ELEMENT(table).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
       !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder) || index >= 31 ||
       (table && (table->m_Type != eResIntersectionFunctionTable || !table->m_Real ||
                  table->m_Pipeline != m_Pipeline || table->m_Stage != (MTL::RenderStages)0)))
    {
      RDCERR("Invalid Metal compute intersection-function-table binding");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->setIntersectionFunctionTable(Unwrap(table), index);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setIntersectionFunctionTable(
    WrappedMTLIntersectionFunctionTable *table, uint32_t index)
{
  if(m_CaptureIndirectArguments) m_IndirectCapture.InvalidateSlot((uint32_t)index);
  SERIALISE_TIME_CALL(Unwrap(this)->setIntersectionFunctionTable(Unwrap(table), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setIntersectionFunctionTable);
    Serialise_setIntersectionFunctionTable(ser, table, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLComputeCommandEncoder::Serialise_setIntersectionFunctionTable(
    ReadSerialiser &, WrappedMTLIntersectionFunctionTable *, uint32_t);
template bool WrappedMTLComputeCommandEncoder::Serialise_setIntersectionFunctionTable(
    WriteSerialiser &, WrappedMTLIntersectionFunctionTable *, uint32_t);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setIntersectionFunctionTables(
    SerialiserType &ser, rdcarray<WrappedMTLIntersectionFunctionTable *> tables, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this).Important();
  SERIALISE_ELEMENT(tables).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
       !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder) ||
       range.length == 0 || range.length > 31 || range.location > 31 - range.length ||
       tables.size() != range.length)
    {
      RDCERR("Invalid Metal compute intersection-function-table range");
      return false;
    }
    rdcarray<const MTL::IntersectionFunctionTable *> native;
    native.reserve(tables.size());
    for(WrappedMTLIntersectionFunctionTable *table : tables)
    {
      if(table && (table->m_Type != eResIntersectionFunctionTable || !table->m_Real ||
                   table->m_Stage != (MTL::RenderStages)0 ||
                   table->m_Pipeline != m_Pipeline))
      {
        RDCERR("Invalid Metal compute intersection-function-table member");
        return false;
      }
      native.push_back(Unwrap(table));
    }
    Unwrap(ComputeCommandEncoder)->setIntersectionFunctionTables(native.data(), range);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setIntersectionFunctionTables(
    rdcarray<WrappedMTLIntersectionFunctionTable *> tables, NS::Range range)
{
  if(m_CaptureIndirectArguments)
    for(NS::UInteger i=0; i<range.length && i<2; i++)
      m_IndirectCapture.InvalidateSlot((uint32_t)(range.location+i));
  if(range.length == 0 || range.length > 31 || tables.size() != range.length)
  {
    RDCERR("Unsupported Metal compute intersection-function-table range");
    return;
  }
  rdcarray<const MTL::IntersectionFunctionTable *> native;
  native.reserve(tables.size());
  for(WrappedMTLIntersectionFunctionTable *table : tables) native.push_back(Unwrap(table));
  SERIALISE_TIME_CALL(Unwrap(this)->setIntersectionFunctionTables(native.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setIntersectionFunctionTables);
    Serialise_setIntersectionFunctionTables(ser, tables, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLIntersectionFunctionTable *table : tables)
      if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLComputeCommandEncoder::Serialise_setIntersectionFunctionTables(
    ReadSerialiser &, rdcarray<WrappedMTLIntersectionFunctionTable *>, NS::Range);
template bool WrappedMTLComputeCommandEncoder::Serialise_setIntersectionFunctionTables(
    WriteSerialiser &, rdcarray<WrappedMTLIntersectionFunctionTable *>, NS::Range);

void WrappedMTLComputeCommandEncoder::setComputePipelineState(
    WrappedMTLComputePipelineState *pipeline)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setComputePipelineState(Unwrap(pipeline)));
  if(m_CaptureIndirectArguments) m_IndirectCapture.BindPipeline(Unwrap(pipeline));
  m_Pipeline = pipeline;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setComputePipelineState);
    Serialise_setComputePipelineState(ser, pipeline);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(pipeline), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setTexture(SerialiserType &ser,
                                                             WrappedMTLTexture *texture,
                                                             NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(index);
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(index >= 128)
      return false;
    Unwrap(ComputeCommandEncoder)->setTexture(Unwrap(texture), index);
    m_Device->GetReplay()->SetComputeTexture((uint32_t)index, GetResID(texture));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setTexture(WrappedMTLTexture *texture, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTexture(Unwrap(texture), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setTexture);
    Serialise_setTexture(ser, texture, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(texture)
      record->MarkResourceFrameReferenced(GetResID(texture), eFrameRef_ReadBeforeWrite);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setTextures(
    SerialiserType &ser, rdcarray<WrappedMTLTexture *> textures, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && (range.location >= 128 || range.length > 128 - range.location))
  {
    RDCERR("Invalid Metal compute texture batch range");
    return false;
  }
  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLTexture *resource : textures)
      bound.push_back(resource ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(textures).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!ComputeCommandEncoder || textures.size() != range.length || bound.size() != range.length)
      return false;
    rdcarray<const MTL::Texture *> real;
    for(size_t i = 0; i < textures.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0) != (textures[i] != NULL))
      {
        RDCERR("Invalid Metal compute texture batch resource");
        return false;
      }
      real.push_back(Unwrap(textures[i]));
    }
    Unwrap(ComputeCommandEncoder)->setTextures(real.data(), range);
    for(size_t i = 0; i < textures.size(); i++)
      m_Device->GetReplay()->SetComputeTexture((uint32_t)(range.location + i),
                                                GetResID(textures[i]));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setTextures(rdcarray<WrappedMTLTexture *> textures,
                                                    NS::Range range)
{
  rdcarray<const MTL::Texture *> real;
  for(WrappedMTLTexture *resource : textures)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setTextures(real.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setTextures);
    Serialise_setTextures(ser, textures, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(size_t i = 0; i < textures.size(); i++)
      if(textures[i])
        record->MarkResourceFrameReferenced(GetResID(textures[i]), eFrameRef_ReadBeforeWrite);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setSamplerState(SerialiserType &ser,
                                                                  WrappedMTLSamplerState *sampler,
                                                                  NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(sampler).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(index >= 16 || !sampler)
    {
      RDCERR("Invalid Metal compute sampler binding");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->setSamplerState(Unwrap(sampler), index);
    m_Device->GetReplay()->BindComputeSampler((uint32_t)index, GetResID(sampler));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setSamplerState(WrappedMTLSamplerState *sampler,
                                                        NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setSamplerState(Unwrap(sampler), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setSamplerState);
    Serialise_setSamplerState(ser, sampler, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setSamplerStates(
    SerialiserType &ser, rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && (range.location >= 16 || range.length > 16 - range.location))
  {
    RDCERR("Invalid Metal compute sampler batch range");
    return false;
  }
  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLSamplerState *resource : samplers)
      bound.push_back(resource ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(samplers).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!ComputeCommandEncoder || samplers.size() != range.length || bound.size() != range.length)
      return false;
    rdcarray<const MTL::SamplerState *> real;
    for(size_t i = 0; i < samplers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0) != (samplers[i] != NULL))
      {
        RDCERR("Invalid Metal compute sampler batch resource");
        return false;
      }
      real.push_back(Unwrap(samplers[i]));
    }
    Unwrap(ComputeCommandEncoder)->setSamplerStates(real.data(), range);
    for(size_t i = 0; i < samplers.size(); i++)
      m_Device->GetReplay()->BindComputeSampler((uint32_t)(range.location + i),
                                                 GetResID(samplers[i]));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setSamplerStates(
    rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range)
{
  rdcarray<const MTL::SamplerState *> real;
  for(WrappedMTLSamplerState *resource : samplers)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setSamplerStates(real.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setSamplerStates);
    Serialise_setSamplerStates(ser, samplers, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLSamplerState *resource : samplers)
      if(resource)
        record->MarkResourceFrameReferenced(GetResID(resource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setBuffer(SerialiserType &ser,
                                                            WrappedMTLBuffer *buffer,
                                                            NS::UInteger offset,
                                                            NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!ComputeCommandEncoder || index >= 31 || (buffer && offset >= Unwrap(buffer)->length()) ||
       (!buffer && offset != 0))
    {
      RDCERR("Invalid Metal compute buffer binding or offset");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->setBuffer(Unwrap(buffer), offset, index);
    m_Device->GetReplay()->BindComputeBuffer((uint32_t)index, GetResID(buffer), (uint64_t)offset);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setBuffer(WrappedMTLBuffer *buffer, NS::UInteger offset,
                                                  NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setBuffer(Unwrap(buffer), offset, index));
  if(m_CaptureIndirectArguments) m_IndirectCapture.BindBuffer(Unwrap(buffer), offset, (uint32_t)index);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setBuffer);
    Serialise_setBuffer(ser, buffer, offset, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(buffer)
      record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_ReadBeforeWrite);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setAccelerationStructure(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder ||
       !Unwrap(ComputeCommandEncoder) || index >= 31 ||
       (structure && (structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
                      !structure->m_LastBuildKind)))
    {
      RDCERR("Invalid Metal compute acceleration structure binding");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->setAccelerationStructure(Unwrap(structure), index);
    m_Device->GetReplay()->BindAccelerationStructure(ShaderStage::Compute, uint32_t(index), GetResID(structure));
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setAccelerationStructure(
    WrappedMTLAccelerationStructure *structure, NS::UInteger index)
{
  if(m_CaptureIndirectArguments) m_IndirectCapture.InvalidateSlot((uint32_t)index);
  if(index >= 31 ||
     (structure && (structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
                    !structure->m_LastBuildKind)))
  {
    RDCERR("Invalid Metal compute acceleration structure binding");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->setAccelerationStructure(Unwrap(structure), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setAccelerationStructure);
    Serialise_setAccelerationStructure(ser, structure, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    if(structure) record->AddParent(GetRecord(structure));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLComputeCommandEncoder::Serialise_setAccelerationStructure(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, NS::UInteger);
template bool WrappedMTLComputeCommandEncoder::Serialise_setAccelerationStructure(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setBuffers(
    SerialiserType &ser, rdcarray<WrappedMTLBuffer *> buffers,
    rdcarray<NS::UInteger> offsets, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && (range.location >= 31 || range.length > 31 - range.location))
  {
    RDCERR("Invalid Metal compute buffer batch range");
    return false;
  }
  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLBuffer *resource : buffers)
      bound.push_back(resource ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(buffers).Important();
  SERIALISE_ELEMENT(offsets).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!ComputeCommandEncoder || buffers.size() != range.length ||
       offsets.size() != range.length || bound.size() != range.length)
      return false;
    rdcarray<const MTL::Buffer *> real;
    for(size_t i = 0; i < buffers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0) != (buffers[i] != NULL) ||
         (buffers[i] && offsets[i] >= Unwrap(buffers[i])->length()) ||
         (!buffers[i] && offsets[i] != 0))
      {
        RDCERR("Invalid Metal compute buffer batch resource or offset");
        return false;
      }
      real.push_back(Unwrap(buffers[i]));
    }
    Unwrap(ComputeCommandEncoder)->setBuffers(real.data(), offsets.data(), range);
    for(size_t i = 0; i < buffers.size(); i++)
      m_Device->GetReplay()->BindComputeBuffer((uint32_t)(range.location + i),
                                                GetResID(buffers[i]), offsets[i]);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setBuffers(rdcarray<WrappedMTLBuffer *> buffers,
                                                   rdcarray<NS::UInteger> offsets, NS::Range range)
{
  rdcarray<const MTL::Buffer *> real;
  for(WrappedMTLBuffer *resource : buffers)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setBuffers(real.data(), offsets.data(), range));
  if(m_CaptureIndirectArguments)
    for(size_t i=0; i<buffers.size() && i<offsets.size(); i++)
      m_IndirectCapture.BindBuffer(Unwrap(buffers[i]), offsets[i], (uint32_t)(range.location+i));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setBuffers);
    Serialise_setBuffers(ser, buffers, offsets, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLBuffer *resource : buffers)
      if(resource)
        record->MarkResourceFrameReferenced(GetResID(resource), eFrameRef_ReadBeforeWrite);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setBytes(SerialiserType &ser, rdcarray<byte> data,
                                                         NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(data).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!ComputeCommandEncoder || index >= 31 || data.size() > 4096)
    {
      RDCERR("Invalid Metal compute inline bytes binding");
      return false;
    }
    m_Device->GetReplay()->SaveShaderInlineData(0, (uint32_t)index, data,
                                               GetResID(ComputeCommandEncoder));
    if(!m_Device->RelocateDescriptorBytes(GetResID(ComputeCommandEncoder), index, data))
      return false;
    Unwrap(ComputeCommandEncoder)->setBytes(data.data(), data.size(), index);
    m_Device->GetReplay()->BindComputeBytes((uint32_t)index, data);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setBytes(rdcarray<byte> data, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setBytes(data.data(), data.size(), index));
  if(m_CaptureIndirectArguments) m_IndirectCapture.BindBytes(data, (uint32_t)index);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setBytes);
    Serialise_setBytes(ser, data, index);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setBufferOffset(SerialiserType &ser,
                                                                NS::UInteger offset,
                                                                NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!ComputeCommandEncoder || index >= 31 ||
       !m_Device->GetReplay()->SetComputeBufferOffset((uint32_t)index, (uint64_t)offset))
    {
      RDCERR("Invalid Metal compute buffer offset or missing binding");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->setBufferOffset(offset, index);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setBufferOffset(NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setBufferOffset(offset, index));
  if(m_CaptureIndirectArguments) m_IndirectCapture.SetBufferOffset(offset, (uint32_t)index);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setBufferOffset);
    Serialise_setBufferOffset(ser, offset, index);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setThreadgroupMemoryLength(
    SerialiserType &ser, NS::UInteger length, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(length).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!ComputeCommandEncoder || index >= 31 || (length & 15) != 0 ||
       length > Unwrap(m_Device)->maxThreadgroupMemoryLength())
    {
      RDCERR("Invalid Metal compute threadgroup memory binding");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->setThreadgroupMemoryLength(length, index);
    m_Device->GetReplay()->SetComputeThreadgroupMemory((uint32_t)index, (uint64_t)length);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setThreadgroupMemoryLength(NS::UInteger length,
                                                                 NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setThreadgroupMemoryLength(length, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setThreadgroupMemoryLength);
    Serialise_setThreadgroupMemoryLength(ser, length, index);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setBytes,
                                rdcarray<byte> data, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setBufferOffset,
                                NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setThreadgroupMemoryLength,
                                NS::UInteger length, NS::UInteger index);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_dispatchThreadgroups(SerialiserType &ser,
                                                                        MTL::Size &groups,
                                                                        MTL::Size &threadsPerGroup)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(groups).Important();
  SERIALISE_ELEMENT(threadsPerGroup).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    MTL::ComputeCommandEncoder *realEncoder = Unwrap(ComputeCommandEncoder);
    MetalReplay *replay = m_Device->GetReplay();
    const TextureDescription source = replay->GetTexture(replay->GetComputeTextureForAccess(false));
    const TextureDescription destination = replay->GetTexture(replay->GetComputeTextureForAccess(true));
    const MetalPipe::BufferBinding input = replay->GetComputeBufferForAccess(false);
    const MetalPipe::BufferBinding output = replay->GetComputeBufferForAccess(true);
    const bool bufferOnly = destination.resourceId == ResourceId() &&
                            output.resourceId != ResourceId();
    // UE's precompiled compute shader may have no resource reflection. It can still use a
    // valid slot-0 buffer with no texture bindings (e.g. a GPU indirect-argument writer).
    const MetalPipe::BufferBinding slot0 = replay->GetComputeBuffer(0);
    const bool slot0BufferOnly = source.resourceId == ResourceId() &&
                                 destination.resourceId == ResourceId() &&
                                 slot0.resourceId != ResourceId() && slot0.byteSize > 0 &&
                                 replay->ValidateComputeBufferBindings(true);
    // D3D12/Vulkan dispatches do not require a writable buffer or a 2D copy pair.
    // A sourced capture already proves the inline layouts, resource lifetimes,
    // pipeline buffer snapshot and tiny grid before any GPU submission.
    const bool sourcedInlineOnly = source.resourceId == ResourceId() &&
        destination.resourceId == ResourceId() && slot0.resourceId == ResourceId() &&
        slot0.byteSize > 0 && m_Device->HasValidatedSourcedComputeDispatch(GetResID(ComputeCommandEncoder)) &&
        replay->ValidateComputeBufferBindings();
    if(!ComputeCommandEncoder || !replay->ValidateComputeThreadgroup(threadsPerGroup) ||
       (!bufferOnly && !slot0BufferOnly && !sourcedInlineOnly &&
        (source.resourceId == ResourceId() || destination.resourceId == ResourceId() ||
         source.type != TextureType::Texture2D || destination.type != TextureType::Texture2D ||
         source.width != destination.width || source.height != destination.height ||
         source.format != destination.format)) ||
       groups.width == 0 || groups.height == 0 || groups.depth == 0 ||
       threadsPerGroup.width == 0 || threadsPerGroup.height == 0 ||
       threadsPerGroup.depth == 0 || groups.width > UINT32_MAX || groups.height > UINT32_MAX ||
       groups.depth > UINT32_MAX ||
       threadsPerGroup.width > UINT32_MAX || threadsPerGroup.height > UINT32_MAX ||
       threadsPerGroup.depth > UINT32_MAX ||
       threadsPerGroup.width > 1024 / threadsPerGroup.height ||
       threadsPerGroup.width * threadsPerGroup.height > 1024 / threadsPerGroup.depth ||
       (!bufferOnly && !slot0BufferOnly && !sourcedInlineOnly &&
        (groups.width < (destination.width + threadsPerGroup.width - 1) / threadsPerGroup.width ||
         groups.height < (destination.height + threadsPerGroup.height - 1) / threadsPerGroup.height)))
    {
      RDCERR("Invalid Metal compute texture binding or dispatch grid");
      fprintf(stderr, "Metal compute dispatch rejected: groups=%llux%llux%llu threads=%llux%llux%llu src=%d srcType=%u dst=%d dstType=%u in=%d out=%d bufferOnly=%d\n",
              (uint64_t)groups.width, (uint64_t)groups.height, (uint64_t)groups.depth,
              (uint64_t)threadsPerGroup.width, (uint64_t)threadsPerGroup.height,
              (uint64_t)threadsPerGroup.depth, source.resourceId != ResourceId(),
              (uint32_t)source.type, destination.resourceId != ResourceId(),
              (uint32_t)destination.type, input.resourceId != ResourceId(),
              output.resourceId != ResourceId(), bufferOnly ? 1 : 0);
      fprintf(stderr, "Metal compute dispatch bindings: slot0=%d slot0Bytes=%llu validated=%d threadgroupValid=%d\n",
              replay->GetComputeBuffer(0).resourceId != ResourceId() ? 1 : 0,
              replay->GetComputeBuffer(0).byteSize,
              replay->ValidateComputeBufferBindings() ? 1 : 0,
              replay->ValidateComputeThreadgroup(threadsPerGroup) ? 1 : 0);
      return false;
    }
    if(bufferOnly && !replay->ValidateComputeBufferBindings())
    {
      RDCERR("Invalid Metal compute output buffer range");
      return false;
    }
    if(!bufferOnly && !slot0BufferOnly && !sourcedInlineOnly &&
       (input.resourceId != ResourceId() || output.resourceId != ResourceId()) &&
       (input.resourceId == ResourceId() || output.resourceId == ResourceId() ||
        input.byteSize < (uint64_t)destination.width * destination.height * 4 ||
        output.byteSize < (uint64_t)destination.width * destination.height * 4))
    {
      RDCERR("Invalid Metal compute buffer range");
      return false;
    }
    realEncoder->dispatchThreadgroups(groups, threadsPerGroup);
    m_Device->NoteDescriptorDispatch(GetResID(ComputeCommandEncoder));
    if(IsLoading(m_State) &&
       !replay->TraceComputeArgumentProducers(replay->GetNextEventID(),
                                             GetResID(ComputeCommandEncoder->m_CommandBuffer), realEncoder))
      return false;
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("dispatchThreadgroups(%llux%llux%llu, %llux%llux%llu)",
                                             (uint64_t)groups.width, (uint64_t)groups.height,
                                             (uint64_t)groups.depth,
                                             (uint64_t)threadsPerGroup.width,
                                             (uint64_t)threadsPerGroup.height,
                                             (uint64_t)threadsPerGroup.depth);
      action.flags = ActionFlags::Dispatch;
      action.dispatchDimension[0] = (uint32_t)groups.width;
      action.dispatchDimension[1] = (uint32_t)groups.height;
      action.dispatchDimension[2] = (uint32_t)groups.depth;
      action.dispatchThreadsDimension[0] = (uint32_t)threadsPerGroup.width;
      action.dispatchThreadsDimension[1] = (uint32_t)threadsPerGroup.height;
      action.dispatchThreadsDimension[2] = (uint32_t)threadsPerGroup.depth;
      AddAction(action);
      // Sourced closure validates residency/lifetime, not shader access. AddAction
      // records reflection and proven bindless usage; never turn the closure into UAVs.
      if(!sourcedInlineOnly && slot0BufferOnly)
      {
        // With no shader resource reflection, report all bound buffers conservatively.
        for(uint32_t slot = 0; slot < 31; slot++)
          replay->AddUsage(replay->GetComputeBuffer(slot).resourceId,
                           ResourceUsage::CS_RWResource);
      }
      else if(!sourcedInlineOnly && !bufferOnly)
      {
        replay->AddUsage(source.resourceId, ResourceUsage::CS_Resource);
        replay->AddUsage(destination.resourceId, ResourceUsage::CS_RWResource);
      }
      else if(!sourcedInlineOnly)
      {
        if(source.resourceId != ResourceId())
          replay->AddUsage(source.resourceId, ResourceUsage::CS_Resource);
        replay->AddUsage(output.resourceId, ResourceUsage::CS_RWResource);
      }
      if(input.resourceId != ResourceId())
      {
        replay->AddUsage(input.resourceId, ResourceUsage::CS_Resource);
        replay->AddUsage(output.resourceId, ResourceUsage::CS_RWResource);
      }
    }
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::dispatchThreadgroups(MTL::Size &groups,
                                                              MTL::Size &threadsPerGroup)
{
  SERIALISE_TIME_CALL(Unwrap(this)->dispatchThreadgroups(groups, threadsPerGroup));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups);
    Serialise_dispatchThreadgroups(ser, groups, threadsPerGroup);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_dispatchThreadgroups(
    SerialiserType &ser, WrappedMTLBuffer *indirectBuffer,
    NS::UInteger indirectBufferOffset, MTL::Size &threadsPerGroup)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(indirectBuffer).Important();
  SERIALISE_ELEMENT(indirectBufferOffset).Important();
  SERIALISE_ELEMENT(threadsPerGroup).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    MTL::Buffer *realBuffer = Unwrap(indirectBuffer);
    const uint64_t argumentSize = sizeof(MTL::DispatchThreadgroupsIndirectArguments);
    if(!ComputeCommandEncoder || !m_Device->GetReplay()->ValidateComputeThreadgroup(threadsPerGroup) ||
       realBuffer == NULL || (indirectBufferOffset & 3) != 0 ||
       indirectBufferOffset > realBuffer->length() ||
       argumentSize > realBuffer->length() - indirectBufferOffset ||
       threadsPerGroup.width == 0 || threadsPerGroup.height == 0 ||
       threadsPerGroup.depth != 1 || threadsPerGroup.width > UINT32_MAX ||
       threadsPerGroup.height > UINT32_MAX ||
       threadsPerGroup.width * threadsPerGroup.height > 1024)
    {
      RDCERR("Invalid Metal compute indirect argument buffer, offset, or threadgroup size");
      return false;
    }

    MetalReplay *replay = m_Device->GetReplay();
    const TextureDescription source = replay->GetTexture(replay->GetComputeTextureForAccess(false));
    const TextureDescription destination = replay->GetTexture(replay->GetComputeTextureForAccess(true));
    const MetalPipe::BufferBinding slot0 = replay->GetComputeBuffer(0);
    const bool slot0BufferOnly = source.resourceId == ResourceId() &&
                                 destination.resourceId == ResourceId() &&
                                 slot0.resourceId != ResourceId() && slot0.byteSize > 0 &&
                                 replay->ValidateComputeBufferBindings(true);
    const bool sourcedInlineOnly = source.resourceId == ResourceId() &&
        destination.resourceId == ResourceId() && slot0.resourceId == ResourceId() &&
        slot0.byteSize > 0 && m_Device->HasValidatedSourcedComputeDispatch(GetResID(ComputeCommandEncoder)) &&
        replay->ValidateComputeBufferBindings();
    if(!slot0BufferOnly && !sourcedInlineOnly &&
       (source.resourceId == ResourceId() || destination.resourceId == ResourceId() ||
        source.type != TextureType::Texture2D || destination.type != TextureType::Texture2D ||
        source.width != destination.width || source.height != destination.height ||
        source.format != destination.format))
    {
      RDCERR("Invalid Metal compute texture binding for indirect dispatch");
      return false;
    }

    const MetalPipe::BufferBinding input = replay->GetComputeBufferForAccess(false);
    const MetalPipe::BufferBinding output = replay->GetComputeBufferForAccess(true);
    if(!slot0BufferOnly && !sourcedInlineOnly &&
       (input.resourceId != ResourceId() || output.resourceId != ResourceId()) &&
       (input.resourceId == ResourceId() || output.resourceId == ResourceId() ||
        input.byteSize < (uint64_t)destination.width * destination.height * 4 ||
        output.byteSize < (uint64_t)destination.width * destination.height * 4))
    {
      RDCERR("Invalid Metal compute buffer range for indirect dispatch");
      return false;
    }

    if(IsLoading(m_State) && !replay->RegisterComputeIndirectAction(replay->GetNextEventID(),
        GetResID(indirectBuffer), indirectBufferOffset, Unwrap(ComputeCommandEncoder),
        GetResID(ComputeCommandEncoder)))
    {
      RDCERR("Could not preserve Metal per-use compute indirect arguments");
      return false;
    }
    replay->SetIndirectBuffer(GetResID(indirectBuffer), indirectBufferOffset, argumentSize);
    Unwrap(ComputeCommandEncoder)
        ->dispatchThreadgroups(realBuffer, indirectBufferOffset, threadsPerGroup);
    m_Device->NoteDescriptorDispatch(GetResID(ComputeCommandEncoder));

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "dispatchThreadgroups(indirect, <?, ?, ?>)";
      action.flags = ActionFlags::Dispatch | ActionFlags::Indirect;
      action.dispatchThreadsDimension[0] = (uint32_t)threadsPerGroup.width;
      action.dispatchThreadsDimension[1] = (uint32_t)threadsPerGroup.height;
      action.dispatchThreadsDimension[2] = (uint32_t)threadsPerGroup.depth;
      AddAction(action);
      replay->AddUsage(GetResID(indirectBuffer), ResourceUsage::Indirect);
      // The safety closure is not evidence of shader reads or writes.
      if(!sourcedInlineOnly && slot0BufferOnly)
      {
        for(uint32_t slot = 0; slot < 31; slot++)
          replay->AddUsage(replay->GetComputeBuffer(slot).resourceId,
                           ResourceUsage::CS_RWResource);
      }
      else if(!sourcedInlineOnly)
      {
        replay->AddUsage(source.resourceId, ResourceUsage::CS_Resource);
        replay->AddUsage(destination.resourceId, ResourceUsage::CS_RWResource);
      }
      if(input.resourceId != ResourceId())
      {
        replay->AddUsage(input.resourceId, ResourceUsage::CS_Resource);
        replay->AddUsage(output.resourceId, ResourceUsage::CS_RWResource);
      }
    }
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::dispatchThreadgroups(
    WrappedMTLBuffer *indirectBuffer, NS::UInteger indirectBufferOffset,
    MTL::Size &threadsPerGroup)
{
  SCOPED_READLOCK(m_Device->GetCaptureTransitionLock());
  if(m_CaptureIndirectArguments && IsActiveCapturing(m_State))
  {
    MetalCapturedComputeIndirectArguments evidence;
    evidence.command=GetResID(m_CommandBuffer); evidence.encoder=m_ID;
    evidence.buffer=GetResID(indirectBuffer); evidence.offset=indirectBufferOffset;
    evidence.epoch=m_Device->GetCaptureEpoch(); evidence.ordinal=m_CaptureIndirectOrdinal++;
    if(m_IndirectCapture.Snapshot(Unwrap(m_Device),Unwrap(this),Unwrap(indirectBuffer),
                                 indirectBufferOffset,evidence.readback))
    {
      // RAII owners are copied into the native completion block, including for
      // unretained command buffers and records discarded before completion.
      const MetalIndirectReadback owners=evidence.readback;
      Unwrap(m_CommandBuffer)->addCompletedHandler([owners](MTL::CommandBuffer *) {});
    }
    else
    {
      if(!Process::GetEnvVariable("RENDERDOC_METAL_TRACE_INDIRECT_CAPTURE").empty())
        fprintf(stderr, "Metal indirect snapshot rejected: device=%p encoder=%p source=%p sourceDevice=%p offset=%llu length=%llu\n",
                Unwrap(m_Device), Unwrap(this), Unwrap(indirectBuffer),
                indirectBuffer ? Unwrap(indirectBuffer)->device() : NULL,
                (unsigned long long)indirectBufferOffset,
                indirectBuffer ? (unsigned long long)Unwrap(indirectBuffer)->length() : 0);
      RDCERR("Could not capture Metal per-use indirect arguments for encoder %s", ToStr(m_ID).c_str());
    }
    GetRecord(m_CommandBuffer)->cmdInfo->indirectArguments.push_back(evidence);
  }

  SERIALISE_TIME_CALL(Unwrap(this)->dispatchThreadgroups(
      Unwrap(indirectBuffer), indirectBufferOffset, threadsPerGroup));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups_indirect);
    Serialise_dispatchThreadgroups(ser, indirectBuffer, indirectBufferOffset, threadsPerGroup);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(indirectBuffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_dispatchThreads(SerialiserType &ser,
                                                                  MTL::Size &grid,
                                                                  MTL::Size &threadsPerGroup)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(grid).Important();
  SERIALISE_ELEMENT(threadsPerGroup).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    MetalReplay *replay = m_Device->GetReplay();
    const TextureDescription source = replay->GetTexture(replay->GetComputeTextureForAccess(false));
    const TextureDescription destination = replay->GetTexture(replay->GetComputeTextureForAccess(true));
    const MetalPipe::BufferBinding output = replay->GetComputeBufferForAccess(true);
    const bool bufferOutput = destination.resourceId == ResourceId() && output.resourceId != ResourceId();
    if(!ComputeCommandEncoder || !replay->ValidateComputeThreadgroup(threadsPerGroup, &grid) ||
       (bufferOutput && !replay->ValidateComputeBufferBindings()) ||
       (!bufferOutput && (source.resourceId == ResourceId() || destination.resourceId == ResourceId() ||
       source.type != TextureType::Texture2D || destination.type != TextureType::Texture2D ||
       source.width != destination.width || source.height != destination.height ||
       source.format != destination.format || grid.width > destination.width || grid.height > destination.height)) ||
       grid.width == 0 || grid.height == 0 || grid.depth != 1 ||
       grid.width > UINT32_MAX || grid.height > UINT32_MAX ||
       threadsPerGroup.width == 0 || threadsPerGroup.height == 0 ||
       threadsPerGroup.depth != 1 || threadsPerGroup.width > 1024 ||
       threadsPerGroup.height > 1024 ||
       threadsPerGroup.width * threadsPerGroup.height > 1024)
    {
      RDCERR("Invalid Metal compute texture binding or thread grid");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->dispatchThreads(grid, threadsPerGroup);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("dispatchThreads(%llux%llux1, %llux%llux1)",
                                             (uint64_t)grid.width, (uint64_t)grid.height,
                                             (uint64_t)threadsPerGroup.width,
                                             (uint64_t)threadsPerGroup.height);
      action.flags = ActionFlags::Dispatch;
      action.dispatchDimension[0] = (uint32_t)grid.width;
      action.dispatchDimension[1] = (uint32_t)grid.height;
      action.dispatchDimension[2] = 1;
      action.dispatchThreadsDimension[0] = (uint32_t)threadsPerGroup.width;
      action.dispatchThreadsDimension[1] = (uint32_t)threadsPerGroup.height;
      action.dispatchThreadsDimension[2] = 1;
      AddAction(action);
      replay->AddUsage(source.resourceId, ResourceUsage::CS_Resource);
      replay->AddUsage(destination.resourceId, ResourceUsage::CS_RWResource);
      if(bufferOutput)
        replay->AddUsage(output.resourceId, ResourceUsage::CS_RWResource);
    }
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::dispatchThreads(MTL::Size &grid,
                                                        MTL::Size &threadsPerGroup)
{
  SERIALISE_TIME_CALL(Unwrap(this)->dispatchThreads(grid, threadsPerGroup));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_dispatchThreads);
    Serialise_dispatchThreads(ser, grid, threadsPerGroup);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, endEncoding);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setComputePipelineState,
                                WrappedMTLComputePipelineState *pipeline);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setTexture,
                                WrappedMTLTexture *texture, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setTextures,
                                rdcarray<WrappedMTLTexture *> textures, NS::Range range);

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setSamplerStateWithLOD(
    SerialiserType &ser, WrappedMTLSamplerState *sampler, float lodMinClamp,
    float lodMaxClamp, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT_LOCAL(bound, sampler != NULL);
  SERIALISE_ELEMENT(sampler).Important();
  SERIALISE_ELEMENT(lodMinClamp).Important();
  SERIALISE_ELEMENT(lodMaxClamp).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!ComputeCommandEncoder || index >= 16 || bound != (sampler != NULL) ||
       !std::isfinite(lodMinClamp) || !std::isfinite(lodMaxClamp) ||
       lodMinClamp < 0.0f || lodMaxClamp < lodMinClamp)
    {
      RDCERR("Invalid Metal compute sampler LOD binding");
      return false;
    }
    Unwrap(ComputeCommandEncoder)->setSamplerState(Unwrap(sampler), lodMinClamp, lodMaxClamp, index);
    m_Device->GetReplay()->BindComputeSampler((uint32_t)index, GetResID(sampler));
    if(sampler)
      m_Device->GetReplay()->SetSamplerLOD(ShaderStage::Compute, (uint32_t)index,
                                           lodMinClamp, lodMaxClamp);
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setSamplerStateWithLOD(
    WrappedMTLSamplerState *sampler, float lodMinClamp, float lodMaxClamp, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setSamplerState(Unwrap(sampler), lodMinClamp, lodMaxClamp, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setSamplerState_lodclamp);
    Serialise_setSamplerStateWithLOD(ser, sampler, lodMinClamp, lodMaxClamp, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(sampler)
      record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLComputeCommandEncoder::Serialise_setSamplerStatesWithLOD(
    SerialiserType &ser, rdcarray<WrappedMTLSamplerState *> samplers,
    rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(ComputeCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && (range.location >= 16 || range.length > 16 - range.location))
  {
    RDCERR("Invalid Metal compute sampler LOD range");
    return false;
  }
  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLSamplerState *sampler : samplers)
      bound.push_back(sampler ? 1 : 0);
  SERIALISE_ELEMENT(bound);
  SERIALISE_ELEMENT(samplers).Important();
  SERIALISE_ELEMENT(lodMinClamps).Important();
  SERIALISE_ELEMENT(lodMaxClamps).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ComputeCommandEncoder || ComputeCommandEncoder->m_Type != eResComputeCommandEncoder || !ComputeCommandEncoder->m_Real ||
       ComputeCommandEncoder != m_Device->GetReplayComputeCommandEncoder(ComputeCommandEncoder))
      return false;

    if(!ComputeCommandEncoder || samplers.size() != range.length || bound.size() != range.length ||
       lodMinClamps.size() != range.length || lodMaxClamps.size() != range.length)
    {
      RDCERR("Invalid Metal compute sampler LOD array lengths");
      return false;
    }
    rdcarray<const MTL::SamplerState *> real;
    for(size_t i = 0; i < samplers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0) != (samplers[i] != NULL) ||
         !std::isfinite(lodMinClamps[i]) || !std::isfinite(lodMaxClamps[i]) ||
         lodMinClamps[i] < 0.0f || lodMaxClamps[i] < lodMinClamps[i])
      {
        RDCERR("Invalid Metal compute sampler LOD array element");
        return false;
      }
      real.push_back(Unwrap(samplers[i]));
    }
    Unwrap(ComputeCommandEncoder)->setSamplerStates(real.data(), lodMinClamps.data(), lodMaxClamps.data(), range);
    for(size_t i = 0; i < samplers.size(); i++)
    {
      const uint32_t slot = (uint32_t)(range.location + i);
      m_Device->GetReplay()->BindComputeSampler(slot, GetResID(samplers[i]));
      if(samplers[i])
        m_Device->GetReplay()->SetSamplerLOD(ShaderStage::Compute, slot,
                                             lodMinClamps[i], lodMaxClamps[i]);
    }
  }
  return true;
}

void WrappedMTLComputeCommandEncoder::setSamplerStatesWithLOD(
    rdcarray<WrappedMTLSamplerState *> samplers, rdcarray<float> lodMinClamps,
    rdcarray<float> lodMaxClamps, NS::Range range)
{
  rdcarray<const MTL::SamplerState *> real;
  for(WrappedMTLSamplerState *sampler : samplers)
    real.push_back(Unwrap(sampler));
  SERIALISE_TIME_CALL(Unwrap(this)->setSamplerStates(real.data(), lodMinClamps.data(),
                                                     lodMaxClamps.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_setSamplerStates_lodclamp);
    Serialise_setSamplerStatesWithLOD(ser, samplers, lodMinClamps, lodMaxClamps, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLSamplerState *sampler : samplers)
      if(sampler)
        record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setSamplerStateWithLOD,
                                WrappedMTLSamplerState *sampler, float lodMinClamp,
                                float lodMaxClamp, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setSamplerStatesWithLOD,
                                rdcarray<WrappedMTLSamplerState *> samplers,
                                rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps,
                                NS::Range range);

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setSamplerState,
                                WrappedMTLSamplerState *sampler, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setSamplerStates,
                                rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setBuffer,
                                WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, setBuffers,
                                rdcarray<WrappedMTLBuffer *> buffers,
                                rdcarray<NS::UInteger> offsets, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, dispatchThreadgroups,
                                MTL::Size &groups, MTL::Size &threadsPerGroup);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, dispatchThreadgroups,
                                WrappedMTLBuffer *indirectBuffer,
                                NS::UInteger indirectBufferOffset, MTL::Size &threadsPerGroup);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLComputeCommandEncoder, void, dispatchThreads,
                                MTL::Size &grid, MTL::Size &threadsPerGroup);

// Capture evidence contains ResourceIds, never live wrappers, and is written only
// after the submitted Native command buffers have completed.
template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_CaptureComputeIndirectArgumentsCount(SerialiserType &ser, uint32_t count)
{
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_CHECK_READ_ERRORS();
  return !ser.IsReading() || IsStructuredExporting(m_State) ||
      (m_HasCapturedComputeIndirectArguments && count == m_CapturedComputeIndirectArgumentsCount);
}
template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_CaptureComputeIndirectArguments(SerialiserType &ser,
    ResourceId command, ResourceId encoder, uint32_t ordinal, ResourceId buffer,
    uint64_t offset, rdcarray<uint32_t> groups)
{
  SERIALISE_ELEMENT(command).Important();
  SERIALISE_ELEMENT(encoder).Important();
  SERIALISE_ELEMENT(ordinal).Important();
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(groups).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(ser.IsReading() && !IsStructuredExporting(m_State))
  {
    const auto found=m_CapturedComputeIndirectArguments.find(make_rdcpair(encoder,ordinal));
    return found != m_CapturedComputeIndirectArguments.end() &&
        found->second.command == command && found->second.buffer == buffer &&
        found->second.offset == offset && found->second.groups == groups;
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_CaptureComputeIndirectArgumentsCount(ReadSerialiser &, uint32_t);
template bool WrappedMTLDevice::Serialise_CaptureComputeIndirectArgumentsCount(WriteSerialiser &, uint32_t);
template bool WrappedMTLDevice::Serialise_CaptureComputeIndirectArguments(ReadSerialiser &, ResourceId, ResourceId, uint32_t, ResourceId, uint64_t, rdcarray<uint32_t>);
template bool WrappedMTLDevice::Serialise_CaptureComputeIndirectArguments(WriteSerialiser &, ResourceId, ResourceId, uint32_t, ResourceId, uint64_t, rdcarray<uint32_t>);
