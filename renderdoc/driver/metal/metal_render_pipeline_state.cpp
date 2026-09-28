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

#include "metal_render_pipeline_state.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_manager.h"
#include "metal_visible_function_table.h"

WrappedMTLRenderPipelineState::WrappedMTLRenderPipelineState(
    MTL::RenderPipelineState *realMTLRenderPipelineState, ResourceId objId,
    WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLRenderPipelineState, objId, wrappedMTLDevice,
                       wrappedMTLDevice->GetStateRef())
{
  if(realMTLRenderPipelineState && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLRenderPipelineState::Serialise_functionHandle(SerialiserType &ser,
    WrappedMTLFunctionHandle *handle, WrappedMTLFunction *function, MTL::RenderStages stage)
{
  SERIALISE_ELEMENT_LOCAL(Pipeline, this).Important();
  SERIALISE_ELEMENT_LOCAL(Handle, GetResID(handle)).TypedAs("MTLFunctionHandle"_lit).Important();
  SERIALISE_ELEMENT(function).Important();
  uint32_t stageValue = (uint32_t)stage;
  SERIALISE_ELEMENT(stageValue).Important();
  stage = (MTL::RenderStages)stageValue;
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Pipeline || Pipeline->m_Type != eResRenderPipelineState || !Pipeline->m_Real ||
       !function || function->m_Type != eResFunction || !function->m_Real ||
       (Unwrap(function)->functionType() != MTL::FunctionTypeVisible &&
        Unwrap(function)->functionType() != MTL::FunctionTypeIntersection) ||
       (stage != MTL::RenderStageVertex && stage != MTL::RenderStageFragment &&
        stage != MTL::RenderStageTile) ||
       Handle == ResourceId() || GetResourceManager()->HasResource(Handle))
    {
      RDCERR("Invalid Metal render function-handle identity or stage");
      return false;
    }
    MTL::FunctionHandle *real = Unwrap(Pipeline)->functionHandle(Unwrap(function), stage);
    if(!real)
    {
      RDCERR("Failed to recreate Metal render function handle");
      return false;
    }
    WrappedMTLFunctionHandle *wrapped = NULL;
    GetResourceManager()->WrapResource(Handle, real, wrapped);
    wrapped->m_Pipeline = Pipeline;
    wrapped->m_Function = function;
    wrapped->m_Stage = stage;
    m_Device->AddResource(Handle, ResourceType::StateObject, "Function Handle");
    m_Device->DerivedResource(Pipeline, Handle);
    m_Device->DerivedResource(function, Handle);
  }
  return true;
}

WrappedMTLFunctionHandle *WrappedMTLRenderPipelineState::functionHandle(
    WrappedMTLFunction *function, MTL::RenderStages stage)
{
  if(!function) return NULL;
  MTL::FunctionHandle *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->functionHandle(Unwrap(function), stage));
  if(!real) return NULL;
  WrappedMTLFunctionHandle *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->m_Pipeline = this;
  wrapped->m_Function = function;
  wrapped->m_Stage = stage;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderPipelineState_functionHandleWithFunction);
    Serialise_functionHandle(ser, wrapped, function, stage);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddParent(GetRecord(function));
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLRenderPipelineState::Serialise_newVisibleFunctionTable(SerialiserType &ser,
    WrappedMTLVisibleFunctionTable *table, uint32_t count, MTL::RenderStages stage)
{
  SERIALISE_ELEMENT_LOCAL(Pipeline, this).Important();
  SERIALISE_ELEMENT_LOCAL(Table, GetResID(table)).TypedAs("MTLVisibleFunctionTable"_lit).Important();
  SERIALISE_ELEMENT(count).Important();
  uint32_t stageValue = (uint32_t)stage;
  SERIALISE_ELEMENT(stageValue).Important();
  stage = (MTL::RenderStages)stageValue;
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Pipeline || Pipeline->m_Type != eResRenderPipelineState || !Pipeline->m_Real ||
       Table == ResourceId() || GetResourceManager()->HasResource(Table) ||
       count == 0 || count > 32 ||
       (stage != MTL::RenderStageVertex && stage != MTL::RenderStageFragment &&
        stage != MTL::RenderStageTile))
    {
      RDCERR("Invalid Metal visible-function-table descriptor");
      return false;
    }
    MTL::VisibleFunctionTableDescriptor *desc =
        MTL::VisibleFunctionTableDescriptor::visibleFunctionTableDescriptor();
    desc->setFunctionCount(count);
    MTL::VisibleFunctionTable *real = Unwrap(Pipeline)->newVisibleFunctionTable(desc, stage);
    if(!real)
    {
      RDCERR("Failed to recreate Metal visible function table");
      return false;
    }
    WrappedMTLVisibleFunctionTable *wrapped = NULL;
    GetResourceManager()->WrapResource(Table, real, wrapped, true);
    wrapped->m_Pipeline = Pipeline;
    wrapped->m_Stage = stage;
    wrapped->m_FunctionCount = count;
    m_Device->AddResource(Table, ResourceType::StateObject, "Visible Function Table");
    m_Device->DerivedResource(Pipeline, Table);
  }
  return true;
}

WrappedMTLVisibleFunctionTable *WrappedMTLRenderPipelineState::newVisibleFunctionTable(
    uint32_t count, MTL::RenderStages stage)
{
  MTL::VisibleFunctionTableDescriptor *desc =
      MTL::VisibleFunctionTableDescriptor::visibleFunctionTableDescriptor();
  desc->setFunctionCount(count);
  MTL::VisibleFunctionTable *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newVisibleFunctionTable(desc, stage));
  if(!real) return NULL;
  WrappedMTLVisibleFunctionTable *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->m_Pipeline = this;
  wrapped->m_Stage = stage;
  wrapped->m_FunctionCount = count;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderPipelineState_newVisibleFunctionTableWithDescriptor);
    Serialise_newVisibleFunctionTable(ser, wrapped, count, stage);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLRenderPipelineState::Serialise_functionHandle(
    ReadSerialiser &, WrappedMTLFunctionHandle *, WrappedMTLFunction *, MTL::RenderStages);
template bool WrappedMTLRenderPipelineState::Serialise_functionHandle(
    WriteSerialiser &, WrappedMTLFunctionHandle *, WrappedMTLFunction *, MTL::RenderStages);
template bool WrappedMTLRenderPipelineState::Serialise_newVisibleFunctionTable(
    ReadSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t, MTL::RenderStages);
template bool WrappedMTLRenderPipelineState::Serialise_newVisibleFunctionTable(
    WriteSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t, MTL::RenderStages);

template <typename SerialiserType>
bool WrappedMTLRenderPipelineState::Serialise_newIntersectionFunctionTable(
    SerialiserType &ser, WrappedMTLIntersectionFunctionTable *table, uint32_t count,
    MTL::RenderStages stage)
{
  SERIALISE_ELEMENT_LOCAL(Pipeline, this).Important();
  SERIALISE_ELEMENT_LOCAL(Table, GetResID(table)).TypedAs("MTLIntersectionFunctionTable"_lit).Important();
  SERIALISE_ELEMENT(count).Important();
  uint32_t stageValue = (uint32_t)stage;
  SERIALISE_ELEMENT(stageValue).Important();
  stage = (MTL::RenderStages)stageValue;
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Pipeline || Pipeline->m_Type != eResRenderPipelineState || !Pipeline->m_Real ||
       Table == ResourceId() || GetResourceManager()->HasResource(Table) ||
       count == 0 || count > 32 ||
       (stage != MTL::RenderStageFragment && stage != MTL::RenderStageVertex &&
        stage != MTL::RenderStageTile))
    {
      RDCERR("Invalid Metal intersection-function-table descriptor");
      return false;
    }
    MTL::IntersectionFunctionTableDescriptor *desc =
        MTL::IntersectionFunctionTableDescriptor::intersectionFunctionTableDescriptor();
    desc->setFunctionCount(count);
    MTL::IntersectionFunctionTable *real = Unwrap(Pipeline)->newIntersectionFunctionTable(desc, stage);
    if(!real)
    {
      RDCERR("Failed to recreate Metal intersection function table");
      return false;
    }
    WrappedMTLIntersectionFunctionTable *wrapped = NULL;
    GetResourceManager()->WrapResource(Table, real, wrapped, true);
    wrapped->m_Pipeline = Pipeline;
    wrapped->m_Stage = stage;
    wrapped->m_FunctionCount = count;
    m_Device->AddResource(Table, ResourceType::StateObject, "Intersection Function Table");
    m_Device->DerivedResource(Pipeline, Table);
  }
  return true;
}

WrappedMTLIntersectionFunctionTable *WrappedMTLRenderPipelineState::newIntersectionFunctionTable(
    uint32_t count, MTL::RenderStages stage)
{
  MTL::IntersectionFunctionTableDescriptor *desc =
      MTL::IntersectionFunctionTableDescriptor::intersectionFunctionTableDescriptor();
  desc->setFunctionCount(count);
  MTL::IntersectionFunctionTable *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newIntersectionFunctionTable(desc, stage));
  if(!real) return NULL;
  WrappedMTLIntersectionFunctionTable *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->m_Pipeline = this;
  wrapped->m_Stage = stage;
  wrapped->m_FunctionCount = count;
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderPipelineState_newIntersectionFunctionTableWithDescriptor);
    Serialise_newIntersectionFunctionTable(ser, wrapped, count, stage);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLRenderPipelineState::Serialise_newIntersectionFunctionTable(
    ReadSerialiser &, WrappedMTLIntersectionFunctionTable *, uint32_t, MTL::RenderStages);
template bool WrappedMTLRenderPipelineState::Serialise_newIntersectionFunctionTable(
    WriteSerialiser &, WrappedMTLIntersectionFunctionTable *, uint32_t, MTL::RenderStages);
