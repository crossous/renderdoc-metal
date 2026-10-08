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

#include "metal_compute_pipeline_state.h"
#include "metal_function.h"
#include "metal_manager.h"
#include "metal_visible_function_table.h"

WrappedMTLComputePipelineState::WrappedMTLComputePipelineState(MTL::ComputePipelineState *real,
                                                               ResourceId id,
                                                               WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLComputePipelineState::Serialise_functionHandle(SerialiserType &ser,
    WrappedMTLFunctionHandle *handle, WrappedMTLFunction *function)
{
  SERIALISE_ELEMENT_LOCAL(Pipeline, this).Important();
  SERIALISE_ELEMENT_LOCAL(Handle, GetResID(handle)).TypedAs("MTLFunctionHandle"_lit).Important();
  SERIALISE_ELEMENT(function).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Pipeline || Pipeline->m_Type != eResComputePipelineState || !Pipeline->m_Real ||
       !function || function->m_Type != eResFunction || !function->m_Real ||
       (Unwrap(function)->functionType() != MTL::FunctionTypeVisible &&
        Unwrap(function)->functionType() != MTL::FunctionTypeIntersection) ||
       Handle == ResourceId() || GetResourceManager()->HasResource(Handle))
    {
      RDCERR("Invalid Metal compute function handle");
      return false;
    }
    MTL::FunctionHandle *real = Unwrap(Pipeline)->functionHandle(Unwrap(function));
    if(!real)
    {
      RDCERR("Failed to recreate Metal compute visible function handle");
      return false;
    }
    WrappedMTLFunctionHandle *wrapped = NULL;
    GetResourceManager()->WrapResource(Handle, real, wrapped);
    wrapped->m_Pipeline = Pipeline;
    wrapped->m_Function = function;
    wrapped->m_Stage = (MTL::RenderStages)0;
    m_Device->AddResource(Handle, ResourceType::StateObject, "Function Handle");
    m_Device->DerivedResource(Pipeline, Handle);
    m_Device->DerivedResource(function, Handle);
  }
  return true;
}

WrappedMTLFunctionHandle *WrappedMTLComputePipelineState::functionHandle(WrappedMTLFunction *function)
{
  if(!function) return NULL;
  MTL::FunctionHandle *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->functionHandle(Unwrap(function)));
  if(!real) return NULL;
  WrappedMTLFunctionHandle *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->m_Pipeline = this;
  wrapped->m_Function = function;
  wrapped->m_Stage = (MTL::RenderStages)0;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputePipelineState_functionHandleWithFunction);
    Serialise_functionHandle(ser, wrapped, function);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddParent(GetRecord(function));
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLComputePipelineState::Serialise_newVisibleFunctionTable(SerialiserType &ser,
    WrappedMTLVisibleFunctionTable *table, uint32_t count)
{
  SERIALISE_ELEMENT_LOCAL(Pipeline, this).Important();
  SERIALISE_ELEMENT_LOCAL(Table, GetResID(table)).TypedAs("MTLVisibleFunctionTable"_lit).Important();
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Pipeline || Pipeline->m_Type != eResComputePipelineState || !Pipeline->m_Real ||
       Table == ResourceId() || GetResourceManager()->HasResource(Table) ||
       count == 0 || count > MetalMaxComputeVisibleFunctionTableEntries)
    {
      RDCERR("Invalid Metal compute visible-function-table descriptor");
      return false;
    }
    MTL::VisibleFunctionTableDescriptor *desc =
        MTL::VisibleFunctionTableDescriptor::visibleFunctionTableDescriptor();
    desc->setFunctionCount(count);
    MTL::VisibleFunctionTable *real = Unwrap(Pipeline)->newVisibleFunctionTable(desc);
    if(!real)
    {
      RDCERR("Failed to recreate Metal compute visible function table");
      return false;
    }
    WrappedMTLVisibleFunctionTable *wrapped = NULL;
    GetResourceManager()->WrapResource(Table, real, wrapped, true);
    wrapped->m_Pipeline = Pipeline;
    wrapped->m_Stage = (MTL::RenderStages)0;
    wrapped->m_FunctionCount = count;
    m_Device->AddResource(Table, ResourceType::StateObject, "Visible Function Table");
    m_Device->DerivedResource(Pipeline, Table);
  }
  return true;
}

WrappedMTLVisibleFunctionTable *WrappedMTLComputePipelineState::newVisibleFunctionTable(
    uint32_t count)
{
  MTL::VisibleFunctionTableDescriptor *desc =
      MTL::VisibleFunctionTableDescriptor::visibleFunctionTableDescriptor();
  desc->setFunctionCount(count);
  MTL::VisibleFunctionTable *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newVisibleFunctionTable(desc));
  if(!real) return NULL;
  WrappedMTLVisibleFunctionTable *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->m_Pipeline = this;
  wrapped->m_Stage = (MTL::RenderStages)0;
  wrapped->m_FunctionCount = count;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputePipelineState_newVisibleFunctionTableWithDescriptor);
    Serialise_newVisibleFunctionTable(ser, wrapped, count);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLComputePipelineState::Serialise_functionHandle(
    ReadSerialiser &, WrappedMTLFunctionHandle *, WrappedMTLFunction *);
template bool WrappedMTLComputePipelineState::Serialise_functionHandle(
    WriteSerialiser &, WrappedMTLFunctionHandle *, WrappedMTLFunction *);
template bool WrappedMTLComputePipelineState::Serialise_newVisibleFunctionTable(
    ReadSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t);
template bool WrappedMTLComputePipelineState::Serialise_newVisibleFunctionTable(
    WriteSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t);

template <typename SerialiserType>
bool WrappedMTLComputePipelineState::Serialise_newIntersectionFunctionTable(
    SerialiserType &ser, WrappedMTLIntersectionFunctionTable *table, uint32_t count)
{
  SERIALISE_ELEMENT_LOCAL(Pipeline, this).Important();
  SERIALISE_ELEMENT_LOCAL(Table, GetResID(table)).TypedAs("MTLIntersectionFunctionTable"_lit).Important();
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Pipeline || Pipeline->m_Type != eResComputePipelineState || !Pipeline->m_Real ||
       Table == ResourceId() || GetResourceManager()->HasResource(Table) ||
       count == 0 || count > 32)
    {
      RDCERR("Invalid Metal compute intersection-function-table descriptor");
      return false;
    }
    MTL::IntersectionFunctionTableDescriptor *desc =
        MTL::IntersectionFunctionTableDescriptor::intersectionFunctionTableDescriptor();
    desc->setFunctionCount(count);
    MTL::IntersectionFunctionTable *real = Unwrap(Pipeline)->newIntersectionFunctionTable(desc);
    if(!real)
    {
      RDCERR("Failed to recreate Metal compute intersection function table");
      return false;
    }
    WrappedMTLIntersectionFunctionTable *wrapped = NULL;
    GetResourceManager()->WrapResource(Table, real, wrapped, true);
    wrapped->m_Pipeline = Pipeline;
    wrapped->m_Stage = (MTL::RenderStages)0;
    wrapped->m_FunctionCount = count;
    m_Device->AddResource(Table, ResourceType::StateObject, "Intersection Function Table");
    m_Device->DerivedResource(Pipeline, Table);
  }
  return true;
}

WrappedMTLIntersectionFunctionTable *WrappedMTLComputePipelineState::newIntersectionFunctionTable(
    uint32_t count)
{
  MTL::IntersectionFunctionTableDescriptor *desc =
      MTL::IntersectionFunctionTableDescriptor::intersectionFunctionTableDescriptor();
  desc->setFunctionCount(count);
  MTL::IntersectionFunctionTable *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newIntersectionFunctionTable(desc));
  if(!real) return NULL;
  WrappedMTLIntersectionFunctionTable *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->m_Pipeline = this;
  wrapped->m_Stage = (MTL::RenderStages)0;
  wrapped->m_FunctionCount = count;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputePipelineState_newIntersectionFunctionTableWithDescriptor);
    Serialise_newIntersectionFunctionTable(ser, wrapped, count);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLComputePipelineState::Serialise_newIntersectionFunctionTable(
    ReadSerialiser &, WrappedMTLIntersectionFunctionTable *, uint32_t);
template bool WrappedMTLComputePipelineState::Serialise_newIntersectionFunctionTable(
    WriteSerialiser &, WrappedMTLIntersectionFunctionTable *, uint32_t);
