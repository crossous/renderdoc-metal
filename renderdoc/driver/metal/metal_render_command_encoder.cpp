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

#include "metal_render_command_encoder.h"
#include "metal_acceleration_structure.h"
#include "metal_fence.h"
#include "metal_heap.h"
#include <cmath>
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_depth_stencil_state.h"
#include "metal_manager.h"
#include "metal_render_pipeline_state.h"
#include "metal_visible_function_table.h"
#include "metal_replay.h"
#include "metal_resource_commands.h"
#include "metal_sampler_state.h"
#include "metal_indirect_command_buffer.h"
#include "metal_parallel_render_command_encoder.h"
#include "metal_texture.h"

WrappedMTLRenderCommandEncoder::WrappedMTLRenderCommandEncoder(
    MTL::RenderCommandEncoder *realMTLRenderCommandEncoder, ResourceId objId,
    WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLRenderCommandEncoder, objId, wrappedMTLDevice,
                       wrappedMTLDevice->GetStateRef())
{
  if(realMTLRenderCommandEncoder && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

void WrappedMTLRenderCommandEncoder::ResolveDeferredStoreActions()
{
  if(!m_DeferredStoreActions || !m_Real)
    return;
  MTL::RenderCommandEncoder *real = Unwrap(this);
  for(uint32_t i = 0; i < 8; ++i)
    if(m_DeferredStoreActions & (1U << i))
    {
      real->setColorStoreAction(MTL::StoreActionStore, i);
      m_Device->GetReplay()->SetRenderPassStoreAction(i, MTL::StoreActionStore);
    }
  if(m_DeferredStoreActions & (1U << 8))
  {
    real->setDepthStoreAction(MTL::StoreActionStore);
    m_Device->GetReplay()->SetRenderPassStoreAction(8, MTL::StoreActionStore);
  }
  if(m_DeferredStoreActions & (1U << 9))
  {
    real->setStencilStoreAction(MTL::StoreActionStore);
    m_Device->GetReplay()->SetRenderPassStoreAction(9, MTL::StoreActionStore);
  }
  m_DeferredStoreActions = 0;
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentAccelerationStructure(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !Unwrap(RenderCommandEncoder) ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       index >= 31 ||
       (structure && (structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
                      !structure->m_LastBuildKind)))
    {
      RDCERR("Invalid Metal fragment acceleration structure binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setFragmentAccelerationStructure(Unwrap(structure), index);
    m_Device->GetReplay()->BindAccelerationStructure(ShaderStage::Fragment, uint32_t(index), GetResID(structure));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentAccelerationStructure(
    WrappedMTLAccelerationStructure *structure, NS::UInteger index)
{
  if(index >= 31 ||
     (structure && (structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
                    !structure->m_LastBuildKind)))
  {
    RDCERR("Invalid Metal fragment acceleration structure binding");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentAccelerationStructure(Unwrap(structure), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentAccelerationStructure);
    Serialise_setFragmentAccelerationStructure(ser, structure, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    if(structure) record->AddParent(GetRecord(structure));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentAccelerationStructure(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, NS::UInteger);
template bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentAccelerationStructure(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexAccelerationStructure(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !Unwrap(RenderCommandEncoder) ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       index >= 31 ||
       (structure && (structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
                      !structure->m_LastBuildKind)))
    {
      RDCERR("Invalid Metal vertex acceleration structure binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setVertexAccelerationStructure(Unwrap(structure), index);
    m_Device->GetReplay()->BindAccelerationStructure(ShaderStage::Vertex, uint32_t(index), GetResID(structure));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexAccelerationStructure(
    WrappedMTLAccelerationStructure *structure, NS::UInteger index)
{
  if(index >= 31 ||
     (structure && (structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
                    !structure->m_LastBuildKind)))
  {
    RDCERR("Invalid Metal vertex acceleration structure binding");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexAccelerationStructure(Unwrap(structure), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexAccelerationStructure);
    Serialise_setVertexAccelerationStructure(ser, structure, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    if(structure) record->AddParent(GetRecord(structure));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setVertexAccelerationStructure(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, NS::UInteger);
template bool WrappedMTLRenderCommandEncoder::Serialise_setVertexAccelerationStructure(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setTileAccelerationStructure(
    SerialiserType &ser, WrappedMTLAccelerationStructure *structure, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(structure).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !Unwrap(RenderCommandEncoder) ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       index >= 31 ||
       (structure && (structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
                      !structure->m_LastBuildKind)))
    {
      RDCERR("Invalid Metal tile acceleration structure binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setTileAccelerationStructure(Unwrap(structure), index);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setTileAccelerationStructure(
    WrappedMTLAccelerationStructure *structure, NS::UInteger index)
{
  if(index >= 31 ||
     (structure && (structure->m_Type != eResAccelerationStructure || !Unwrap(structure) ||
                    !structure->m_LastBuildKind)))
  {
    RDCERR("Invalid Metal tile acceleration structure binding");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->setTileAccelerationStructure(Unwrap(structure), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setTileAccelerationStructure);
    Serialise_setTileAccelerationStructure(ser, structure, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    if(structure) record->AddParent(GetRecord(structure));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setTileAccelerationStructure(
    ReadSerialiser &, WrappedMTLAccelerationStructure *, NS::UInteger);
template bool WrappedMTLRenderCommandEncoder::Serialise_setTileAccelerationStructure(
    WriteSerialiser &, WrappedMTLAccelerationStructure *, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentVisibleFunctionTable(
    SerialiserType &ser, WrappedMTLVisibleFunctionTable *table, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(table).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) || index >= 31 ||
       (table && (table->m_Type != eResVisibleFunctionTable || !table->m_Real ||
                  table->m_Stage != MTL::RenderStageFragment ||
                  table->m_Pipeline != m_EncoderPipeline)))
    {
      RDCERR("Invalid Metal fragment visible-function-table binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setFragmentVisibleFunctionTable(Unwrap(table), index);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentVisibleFunctionTable(
    WrappedMTLVisibleFunctionTable *table, uint32_t index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentVisibleFunctionTable(Unwrap(table), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentVisibleFunctionTable);
    Serialise_setFragmentVisibleFunctionTable(ser, table, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentVisibleFunctionTable(
    ReadSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentVisibleFunctionTable(
    WriteSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentIntersectionFunctionTable(
    SerialiserType &ser, WrappedMTLIntersectionFunctionTable *table, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(table).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) || index >= 31 ||
       !table || table->m_Type != eResIntersectionFunctionTable || !table->m_Real ||
       table->m_Stage != MTL::RenderStageFragment ||
       table->m_Pipeline != m_EncoderPipeline)
    {
      RDCERR("Invalid Metal fragment intersection-function-table binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setFragmentIntersectionFunctionTable(Unwrap(table), index);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentIntersectionFunctionTable(
    WrappedMTLIntersectionFunctionTable *table, uint32_t index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentIntersectionFunctionTable(Unwrap(table), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentIntersectionFunctionTable);
    Serialise_setFragmentIntersectionFunctionTable(ser, table, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentIntersectionFunctionTable(
    ReadSerialiser &, WrappedMTLIntersectionFunctionTable *, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentIntersectionFunctionTable(
    WriteSerialiser &, WrappedMTLIntersectionFunctionTable *, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexIntersectionFunctionTable(
    SerialiserType &ser, WrappedMTLIntersectionFunctionTable *table, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(table).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) || index >= 31 ||
       !table || table->m_Type != eResIntersectionFunctionTable || !table->m_Real ||
       table->m_Stage != MTL::RenderStageVertex || table->m_Pipeline != m_EncoderPipeline)
    {
      RDCERR("Invalid Metal vertex intersection-function-table binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setVertexIntersectionFunctionTable(Unwrap(table), index);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexIntersectionFunctionTable(
    WrappedMTLIntersectionFunctionTable *table, uint32_t index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexIntersectionFunctionTable(Unwrap(table), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexIntersectionFunctionTable);
    Serialise_setVertexIntersectionFunctionTable(ser, table, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setVertexIntersectionFunctionTable(
    ReadSerialiser &, WrappedMTLIntersectionFunctionTable *, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setVertexIntersectionFunctionTable(
    WriteSerialiser &, WrappedMTLIntersectionFunctionTable *, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setTileIntersectionFunctionTable(
    SerialiserType &ser, WrappedMTLIntersectionFunctionTable *table, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(table).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) || index >= 31 ||
       !table || table->m_Type != eResIntersectionFunctionTable || !table->m_Real ||
       table->m_Stage != MTL::RenderStageTile || table->m_Pipeline != m_EncoderPipeline)
    {
      RDCERR("Invalid Metal tile intersection-function-table binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setTileIntersectionFunctionTable(Unwrap(table), index);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setTileIntersectionFunctionTable(
    WrappedMTLIntersectionFunctionTable *table, uint32_t index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTileIntersectionFunctionTable(Unwrap(table), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setTileIntersectionFunctionTable);
    Serialise_setTileIntersectionFunctionTable(ser, table, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setTileIntersectionFunctionTable(
    ReadSerialiser &, WrappedMTLIntersectionFunctionTable *, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setTileIntersectionFunctionTable(
    WriteSerialiser &, WrappedMTLIntersectionFunctionTable *, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setIntersectionFunctionTables(
    SerialiserType &ser, rdcarray<WrappedMTLIntersectionFunctionTable *> tables,
    NS::Range range, MTL::RenderStages stage)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(tables).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       range.length == 0 || range.length > 31 || range.location > 31 - range.length ||
       tables.size() != range.length)
    {
      RDCERR("Invalid Metal intersection-function-table range");
      return false;
    }
    rdcarray<const MTL::IntersectionFunctionTable *> native;
    native.reserve(tables.size());
    for(WrappedMTLIntersectionFunctionTable *table : tables)
    {
      if(!table || table->m_Type != eResIntersectionFunctionTable || !table->m_Real ||
         table->m_Stage != stage || table->m_Pipeline != m_EncoderPipeline)
      {
        RDCERR("Invalid Metal intersection-function-table range member");
        return false;
      }
      native.push_back(Unwrap(table));
    }
    if(stage == MTL::RenderStageVertex)
      Unwrap(RenderCommandEncoder)->setVertexIntersectionFunctionTables(native.data(), range);
    else if(stage == MTL::RenderStageFragment)
      Unwrap(RenderCommandEncoder)->setFragmentIntersectionFunctionTables(native.data(), range);
    else if(stage == MTL::RenderStageTile)
      Unwrap(RenderCommandEncoder)->setTileIntersectionFunctionTables(native.data(), range);
    else
      return false;
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setIntersectionFunctionTables(
    rdcarray<WrappedMTLIntersectionFunctionTable *> tables, NS::Range range,
    MTL::RenderStages stage)
{
  if(range.length == 0 || range.length > 31 || range.location > 31 - range.length ||
     tables.size() != range.length ||
     (stage != MTL::RenderStageVertex && stage != MTL::RenderStageFragment &&
      stage != MTL::RenderStageTile))
  {
    RDCERR("Unsupported Metal intersection-function-table range");
    return;
  }
  rdcarray<const MTL::IntersectionFunctionTable *> native;
  native.reserve(tables.size());
  for(WrappedMTLIntersectionFunctionTable *table : tables) native.push_back(Unwrap(table));
  if(stage == MTL::RenderStageVertex)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setVertexIntersectionFunctionTables(native.data(), range));
  }
  else if(stage == MTL::RenderStageFragment)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setFragmentIntersectionFunctionTables(native.data(), range));
  }
  else
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setTileIntersectionFunctionTables(native.data(), range));
  }
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    const MetalChunk chunk = stage == MTL::RenderStageVertex ?
        MetalChunk::MTLRenderCommandEncoder_setVertexIntersectionFunctionTables :
        stage == MTL::RenderStageFragment ?
        MetalChunk::MTLRenderCommandEncoder_setFragmentIntersectionFunctionTables :
        MetalChunk::MTLRenderCommandEncoder_setTileIntersectionFunctionTables;
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_setIntersectionFunctionTables(ser, tables, range, stage);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLIntersectionFunctionTable *table : tables)
      if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setIntersectionFunctionTables(
    ReadSerialiser &, rdcarray<WrappedMTLIntersectionFunctionTable *>, NS::Range,
    MTL::RenderStages);
template bool WrappedMTLRenderCommandEncoder::Serialise_setIntersectionFunctionTables(
    WriteSerialiser &, rdcarray<WrappedMTLIntersectionFunctionTable *>, NS::Range,
    MTL::RenderStages);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setTileVisibleFunctionTable(
    SerialiserType &ser, WrappedMTLVisibleFunctionTable *table, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(table).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) || index >= 31 ||
       (table && (table->m_Type != eResVisibleFunctionTable || !table->m_Real ||
                  table->m_Stage != MTL::RenderStageTile ||
                  table->m_Pipeline != m_EncoderPipeline)))
    {
      RDCERR("Invalid Metal tile visible-function-table binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setTileVisibleFunctionTable(Unwrap(table), index);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setTileVisibleFunctionTable(
    WrappedMTLVisibleFunctionTable *table, uint32_t index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTileVisibleFunctionTable(Unwrap(table), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setTileVisibleFunctionTable);
    Serialise_setTileVisibleFunctionTable(ser, table, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setTileVisibleFunctionTable(
    ReadSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setTileVisibleFunctionTable(
    WriteSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setTileVisibleFunctionTables(
    SerialiserType &ser, rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(tables).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       range.length == 0 || range.length > 31 || range.location > 31 - range.length ||
       tables.size() != range.length)
    {
      RDCERR("Invalid Metal tile visible-function-table range");
      return false;
    }
    rdcarray<const MTL::VisibleFunctionTable *> native;
    native.reserve(tables.size());
    for(WrappedMTLVisibleFunctionTable *table : tables)
    {
      if(table && (table->m_Type != eResVisibleFunctionTable || !table->m_Real ||
                   table->m_Stage != MTL::RenderStageTile ||
                   table->m_Pipeline != m_EncoderPipeline))
      {
        RDCERR("Invalid Metal tile visible-function-table member");
        return false;
      }
      native.push_back(Unwrap(table));
    }
    Unwrap(RenderCommandEncoder)->setTileVisibleFunctionTables(native.data(), range);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setTileVisibleFunctionTables(
    rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range)
{
  if(range.length == 0 || range.length > 31 || tables.size() != range.length)
  {
    RDCERR("Unsupported Metal tile visible-function-table range");
    return;
  }
  rdcarray<const MTL::VisibleFunctionTable *> native;
  native.reserve(tables.size());
  for(WrappedMTLVisibleFunctionTable *table : tables) native.push_back(Unwrap(table));
  SERIALISE_TIME_CALL(Unwrap(this)->setTileVisibleFunctionTables(native.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setTileVisibleFunctionTables);
    Serialise_setTileVisibleFunctionTables(ser, tables, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLVisibleFunctionTable *table : tables)
      if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setTileVisibleFunctionTables(
    ReadSerialiser &, rdcarray<WrappedMTLVisibleFunctionTable *>, NS::Range);
template bool WrappedMTLRenderCommandEncoder::Serialise_setTileVisibleFunctionTables(
    WriteSerialiser &, rdcarray<WrappedMTLVisibleFunctionTable *>, NS::Range);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexVisibleFunctionTable(
    SerialiserType &ser, WrappedMTLVisibleFunctionTable *table, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(table).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) || index >= 31 ||
       (table && (table->m_Type != eResVisibleFunctionTable || !table->m_Real ||
                  table->m_Stage != MTL::RenderStageVertex ||
                  table->m_Pipeline != m_EncoderPipeline)))
    {
      RDCERR("Invalid Metal vertex visible-function-table binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setVertexVisibleFunctionTable(Unwrap(table), index);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexVisibleFunctionTable(
    WrappedMTLVisibleFunctionTable *table, uint32_t index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexVisibleFunctionTable(Unwrap(table), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexVisibleFunctionTable);
    Serialise_setVertexVisibleFunctionTable(ser, table, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setVertexVisibleFunctionTable(
    ReadSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setVertexVisibleFunctionTable(
    WriteSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexVisibleFunctionTables(
    SerialiserType &ser, rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(tables).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       range.length == 0 || range.length > 31 || range.location > 31 - range.length ||
       tables.size() != range.length)
    {
      RDCERR("Invalid Metal vertex visible-function-table range");
      return false;
    }
    rdcarray<const MTL::VisibleFunctionTable *> native;
    native.reserve(tables.size());
    for(WrappedMTLVisibleFunctionTable *table : tables)
    {
      if(table && (table->m_Type != eResVisibleFunctionTable || !table->m_Real ||
                   table->m_Stage != MTL::RenderStageVertex ||
                   table->m_Pipeline != m_EncoderPipeline))
      {
        RDCERR("Invalid Metal vertex visible-function-table member");
        return false;
      }
      native.push_back(Unwrap(table));
    }
    Unwrap(RenderCommandEncoder)->setVertexVisibleFunctionTables(native.data(), range);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexVisibleFunctionTables(
    rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range)
{
  if(range.length == 0 || range.length > 31 || tables.size() != range.length)
  {
    RDCERR("Unsupported Metal vertex visible-function-table range");
    return;
  }
  rdcarray<const MTL::VisibleFunctionTable *> native;
  native.reserve(tables.size());
  for(WrappedMTLVisibleFunctionTable *table : tables) native.push_back(Unwrap(table));
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexVisibleFunctionTables(native.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexVisibleFunctionTables);
    Serialise_setVertexVisibleFunctionTables(ser, tables, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLVisibleFunctionTable *table : tables)
      if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setVertexVisibleFunctionTables(
    ReadSerialiser &, rdcarray<WrappedMTLVisibleFunctionTable *>, NS::Range);
template bool WrappedMTLRenderCommandEncoder::Serialise_setVertexVisibleFunctionTables(
    WriteSerialiser &, rdcarray<WrappedMTLVisibleFunctionTable *>, NS::Range);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentVisibleFunctionTables(
    SerialiserType &ser, rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this).Important();
  SERIALISE_ELEMENT(tables).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       range.length == 0 || range.length > 31 || range.location > 31 - range.length ||
       tables.size() != range.length)
    {
      RDCERR("Invalid Metal fragment visible-function-table range");
      return false;
    }
    rdcarray<const MTL::VisibleFunctionTable *> native;
    native.reserve(tables.size());
    for(WrappedMTLVisibleFunctionTable *table : tables)
    {
      if(table && (table->m_Type != eResVisibleFunctionTable || !table->m_Real ||
                   table->m_Stage != MTL::RenderStageFragment ||
                   table->m_Pipeline != m_EncoderPipeline))
      {
        RDCERR("Invalid Metal fragment visible-function-table member");
        return false;
      }
      native.push_back(Unwrap(table));
    }
    Unwrap(RenderCommandEncoder)->setFragmentVisibleFunctionTables(native.data(), range);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentVisibleFunctionTables(
    rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range)
{
  if(range.length == 0 || range.length > 31 || tables.size() != range.length)
  {
    RDCERR("Unsupported Metal fragment visible-function-table range");
    return;
  }
  rdcarray<const MTL::VisibleFunctionTable *> native;
  native.reserve(tables.size());
  for(WrappedMTLVisibleFunctionTable *table : tables) native.push_back(Unwrap(table));
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentVisibleFunctionTables(native.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentVisibleFunctionTables);
    Serialise_setFragmentVisibleFunctionTables(ser, tables, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLVisibleFunctionTable *table : tables)
      if(table) record->MarkResourceFrameReferenced(GetResID(table), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentVisibleFunctionTables(
    ReadSerialiser &, rdcarray<WrappedMTLVisibleFunctionTable *>, NS::Range);
template bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentVisibleFunctionTables(
    WriteSerialiser &, rdcarray<WrappedMTLVisibleFunctionTable *>, NS::Range);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_insertDebugSignpost(SerialiserType &ser,
                                                                   NS::String *string)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(string).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
      !Unwrap(RenderCommandEncoder) ||
      RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder)))
    return false;
  if(IsLoading(m_State))
  {
    AddEvent();
    m_Device->GetReplay()->AddDebugGroup(string, ActionFlags::SetMarker);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::insertDebugSignpost(NS::String *string)
{
  SERIALISE_TIME_CALL(Unwrap(this)->insertDebugSignpost(string));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_insertDebugSignpost);
    Serialise_insertDebugSignpost(ser, string);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_pushDebugGroup(SerialiserType &ser,
                                                              NS::String *string)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(string).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
      !Unwrap(RenderCommandEncoder) ||
      RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder)))
    return false;
  if(IsLoading(m_State))
  {
    AddEvent();
    m_Device->GetReplay()->AddDebugGroup(string, ActionFlags::PushMarker);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::pushDebugGroup(NS::String *string)
{
  SERIALISE_TIME_CALL(Unwrap(this)->pushDebugGroup(string));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_pushDebugGroup);
    Serialise_pushDebugGroup(ser, string);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_popDebugGroup(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
      !Unwrap(RenderCommandEncoder) ||
      RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder)))
    return false;
  if(IsLoading(m_State))
  {
    AddEvent();
    m_Device->GetReplay()->AddDebugGroup(NULL, ActionFlags::PopMarker);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::popDebugGroup()
{
  SERIALISE_TIME_CALL(Unwrap(this)->popDebugGroup());
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_popDebugGroup);
    Serialise_popDebugGroup(ser);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setRenderPipelineState(
    SerialiserType &ser, WrappedMTLRenderPipelineState *pipelineState)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(pipelineState).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || !pipelineState || pipelineState->m_Type != eResRenderPipelineState ||
       !Unwrap(pipelineState))
    {
      RDCERR("Invalid Metal render pipeline or encoder");
      return false;
    }
    m_EncoderPipeline = pipelineState;
    // Overlay framebuffer formats differ from the captured targets. Keep the
    // original metadata for binding validation, while its owned encoder uses
    // the patched Native pipeline supplied by the overlay renderer.
    if(!m_Device->IsOverlayPipelineOverride(RenderCommandEncoder))
      Unwrap(RenderCommandEncoder)->setRenderPipelineState(Unwrap(pipelineState));
    m_Device->GetReplay()->BindRenderPipeline(GetResID(pipelineState));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setRenderPipelineState(WrappedMTLRenderPipelineState *pipelineState)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setRenderPipelineState(Unwrap(pipelineState)));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setRenderPipelineState);
      Serialise_setRenderPipelineState(ser, pipelineState);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(pipelineState), eFrameRef_Read);
  }
  else
  {
// TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setTileBuffer(
    SerialiserType &ser, WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) ||
       (buffer && (buffer->m_Type != eResBuffer || !buffer->m_Real ||
                   buffer->m_Device != m_Device || offset > Unwrap(buffer)->length() ||
                   Unwrap(buffer)->length() - offset < 4)) ||
       (!buffer && offset != 0) || index >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS)
    {
      RDCERR("Invalid Metal tile buffer binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setTileBuffer(Unwrap(buffer), offset, index);
    m_EncoderTileBuffers[index] = buffer;
    m_EncoderTileOffsets[index] = offset;
    m_Device->GetReplay()->BindComputeBuffer((uint32_t)index, GetResID(buffer), offset);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setTileBuffer(WrappedMTLBuffer *buffer,
                                                    NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTileBuffer(Unwrap(buffer), offset, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setTileBuffer);
    Serialise_setTileBuffer(ser, buffer, offset, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(buffer) record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_ReadBeforeWrite);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setObjectBuffer(
    SerialiserType &ser, WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) ||
       (buffer && (buffer->m_Type != eResBuffer || !buffer->m_Real ||
                   buffer->m_Device != m_Device || offset > Unwrap(buffer)->length() ||
                   Unwrap(buffer)->length() - offset < 4)) ||
       (!buffer && offset != 0) || index >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS)
    {
      RDCERR("Invalid Metal object buffer binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setObjectBuffer(Unwrap(buffer), offset, index);
    m_EncoderObjectBuffers[index] = buffer;
    m_EncoderObjectOffsets[index] = offset;
    m_Device->GetReplay()->BindExtendedBuffer(ShaderStage::Task, uint32_t(index), GetResID(buffer), offset);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setObjectBuffer(WrappedMTLBuffer *buffer,
                                                      NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setObjectBuffer(Unwrap(buffer), offset, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setObjectBuffer);
    Serialise_setObjectBuffer(ser, buffer, offset, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(buffer) record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_ReadBeforeWrite);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void,
                                setObjectBuffer, WrappedMTLBuffer *buffer,
                                NS::UInteger offset, NS::UInteger index);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setObjectBinding(
    SerialiserType &ser, rdcarray<WrappedMTLBuffer *> buffers,
    rdcarray<NS::UInteger> offsets, rdcarray<byte> data, NS::Range range, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(buffers).Important();
  SERIALISE_ELEMENT(offsets).Important();
  SERIALISE_ELEMENT(data).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || variant > 2 ||
       range.location >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS || !range.length ||
       range.length > MAX_RENDER_PASS_BUFFER_ATTACHMENTS - range.location ||
       (variant < 2 && range.length != 1) ||
       (variant == 0 && (data.empty() || data.size() > 4096 || !buffers.empty() ||
                         !offsets.empty())) ||
       (variant == 1 && (offsets.size() != 1 || !buffers.empty() || !data.empty())) ||
       (variant == 2 && (buffers.size() != range.length || offsets.size() != range.length ||
                         !data.empty())))
    {
      RDCERR("Invalid Metal object binding shape or encoder");
      return false;
    }
    if(variant == 0)
    {
      m_Device->GetReplay()->BindExtendedBytes(ShaderStage::Task, uint32_t(range.location), data, GetResID(RenderCommandEncoder));
      if(!m_Device->RelocateGraphicsDescriptorBytes(GetResID(RenderCommandEncoder), 3, range.location, data))
        return false;
      Unwrap(RenderCommandEncoder)->setObjectBytes(data.data(), data.size(), range.location);
      m_EncoderObjectBuffers[range.location] = NULL;
      m_EncoderObjectOffsets[range.location] = 0;
    }
    else if(variant == 1)
    {
      WrappedMTLBuffer *buffer = m_EncoderObjectBuffers[range.location];
      if(!buffer || !buffer->m_Real || offsets[0] > Unwrap(buffer)->length() ||
         Unwrap(buffer)->length() - offsets[0] < 4)
      {
        RDCERR("Invalid Metal object buffer offset or missing prior binding");
        return false;
      }
      Unwrap(RenderCommandEncoder)->setObjectBufferOffset(offsets[0], range.location);
      m_EncoderObjectOffsets[range.location] = offsets[0];
      m_Device->GetReplay()->BindExtendedBuffer(ShaderStage::Task, uint32_t(range.location), GetResID(buffer), offsets[0]);
    }
    else
    {
      rdcarray<const MTL::Buffer *> real;
      for(size_t i = 0; i < buffers.size(); i++)
      {
        WrappedMTLBuffer *buffer = buffers[i];
        if((buffer && (buffer->m_Type != eResBuffer || !buffer->m_Real ||
                       buffer->m_Device != m_Device || offsets[i] > Unwrap(buffer)->length() ||
                       Unwrap(buffer)->length() - offsets[i] < 4)) ||
           (!buffer && offsets[i] != 0))
        {
          RDCERR("Invalid Metal object buffer batch resource or range");
          return false;
        }
        real.push_back(Unwrap(buffer));
      }
      Unwrap(RenderCommandEncoder)->setObjectBuffers(real.data(), offsets.data(), range);
      for(size_t i = 0; i < buffers.size(); i++)
      {
        m_EncoderObjectBuffers[range.location + i] = buffers[i];
        m_EncoderObjectOffsets[range.location + i] = offsets[i];
        m_Device->GetReplay()->BindExtendedBuffer(ShaderStage::Task, uint32_t(range.location+i), GetResID(buffers[i]), offsets[i]);
      }
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setObjectBytes(rdcarray<byte> data, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setObjectBytes(data.data(), data.size(), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setObjectBytes);
    Serialise_setObjectBinding(ser, {}, {}, data, NS::Range::Make(index, 1), 0);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

void WrappedMTLRenderCommandEncoder::setObjectBufferOffset(NS::UInteger offset,
                                                            NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setObjectBufferOffset(offset, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setObjectBufferOffset);
    Serialise_setObjectBinding(ser, {}, {offset}, {}, NS::Range::Make(index, 1), 1);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

void WrappedMTLRenderCommandEncoder::setObjectBuffers(
    rdcarray<WrappedMTLBuffer *> buffers, rdcarray<NS::UInteger> offsets, NS::Range range)
{
  rdcarray<const MTL::Buffer *> real;
  for(WrappedMTLBuffer *buffer : buffers) real.push_back(Unwrap(buffer));
  SERIALISE_TIME_CALL(Unwrap(this)->setObjectBuffers(real.data(), offsets.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setObjectBuffers);
    Serialise_setObjectBinding(ser, buffers, offsets, {}, range, 2);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLBuffer *buffer : buffers)
      if(buffer) record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_ReadBeforeWrite);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setObjectBinding(
    ReadSerialiser &, rdcarray<WrappedMTLBuffer *>, rdcarray<NS::UInteger>,
    rdcarray<byte>, NS::Range, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setObjectBinding(
    WriteSerialiser &, rdcarray<WrappedMTLBuffer *>, rdcarray<NS::UInteger>,
    rdcarray<byte>, NS::Range, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setObjectTextures(
    SerialiserType &ser, rdcarray<WrappedMTLTexture *> textures,
    NS::Range range, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_ELEMENT(textures).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || variant > 1 ||
       range.location >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS || !range.length ||
       range.length > MAX_RENDER_PASS_BUFFER_ATTACHMENTS - range.location ||
       (variant == 0 && range.length != 1) || textures.size() != range.length)
    {
      RDCERR("Invalid Metal object texture binding shape or encoder");
      return false;
    }
    rdcarray<const MTL::Texture *> real;
    for(WrappedMTLTexture *texture : textures)
    {
      if(texture && (texture->m_Type != eResTexture || !texture->m_Real ||
                     texture->m_Device != m_Device))
      {
        RDCERR("Invalid Metal object texture resource");
        return false;
      }
      real.push_back(Unwrap(texture));
    }
    if(variant == 0)
      Unwrap(RenderCommandEncoder)->setObjectTexture(real[0], range.location);
    else
      Unwrap(RenderCommandEncoder)->setObjectTextures(real.data(), range);
    for(size_t i = 0; i < textures.size(); i++)
      m_Device->GetReplay()->BindExtendedTexture(ShaderStage::Task, uint32_t(range.location+i), GetResID(textures[i]));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setObjectTextures(
    rdcarray<WrappedMTLTexture *> textures, NS::Range range, uint32_t variant)
{
  rdcarray<const MTL::Texture *> real;
  for(WrappedMTLTexture *texture : textures) real.push_back(Unwrap(texture));
  if(variant == 0)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setObjectTexture(real[0], range.location));
  }
  else
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setObjectTextures(real.data(), range));
  }
  if(IsCaptureMode(m_State))
  {
    const MetalChunk chunk = variant == 0 ? MetalChunk::MTLRenderCommandEncoder_setObjectTexture :
                                           MetalChunk::MTLRenderCommandEncoder_setObjectTextures;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_setObjectTextures(ser, textures, range, variant);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLTexture *texture : textures)
      if(texture) record->MarkResourceFrameReferenced(GetResID(texture), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setObjectTextures(
    ReadSerialiser &, rdcarray<WrappedMTLTexture *>, NS::Range, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setObjectTextures(
    WriteSerialiser &, rdcarray<WrappedMTLTexture *>, NS::Range, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setObjectSamplers(
    SerialiserType &ser, rdcarray<WrappedMTLSamplerState *> samplers,
    rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps,
    NS::Range range, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_ELEMENT(samplers).Important();
  SERIALISE_ELEMENT(lodMinClamps).Important();
  SERIALISE_ELEMENT(lodMaxClamps).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    const bool clamped = variant >= 2;
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || variant > 3 || range.location >= 16 ||
       !range.length || range.length > 16 - range.location ||
       (variant % 2 == 0 && range.length != 1) || samplers.size() != range.length ||
       (clamped && (lodMinClamps.size() != range.length ||
                    lodMaxClamps.size() != range.length)) ||
       (!clamped && (!lodMinClamps.empty() || !lodMaxClamps.empty())))
    {
      RDCERR("Invalid Metal object sampler binding shape or encoder");
      return false;
    }
    rdcarray<const MTL::SamplerState *> real;
    for(size_t i = 0; i < samplers.size(); i++)
    {
      WrappedMTLSamplerState *sampler = samplers[i];
      if((sampler && (sampler->m_Type != eResSamplerState || !sampler->m_Real ||
                      sampler->m_Device != m_Device)) ||
         (clamped && (!std::isfinite(lodMinClamps[i]) ||
                      !std::isfinite(lodMaxClamps[i]) || lodMinClamps[i] < 0.0f ||
                      lodMaxClamps[i] < lodMinClamps[i])))
      {
        RDCERR("Invalid Metal object sampler resource or LOD clamp");
        return false;
      }
      real.push_back(Unwrap(sampler));
    }
    if(variant == 0)
      Unwrap(RenderCommandEncoder)->setObjectSamplerState(real[0], range.location);
    else if(variant == 1)
      Unwrap(RenderCommandEncoder)->setObjectSamplerStates(real.data(), range);
    else if(variant == 2)
      Unwrap(RenderCommandEncoder)->setObjectSamplerState(
          real[0], lodMinClamps[0], lodMaxClamps[0], range.location);
    else
      Unwrap(RenderCommandEncoder)->setObjectSamplerStates(
          real.data(), lodMinClamps.data(), lodMaxClamps.data(), range);
    for(size_t i = 0; i < samplers.size(); i++)
    {
      m_Device->GetReplay()->BindExtendedSampler(ShaderStage::Task, uint32_t(range.location+i), GetResID(samplers[i]));
      if(clamped) m_Device->GetReplay()->SetSamplerLOD(ShaderStage::Task, uint32_t(range.location+i), lodMinClamps[i], lodMaxClamps[i]);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setObjectSamplers(
    rdcarray<WrappedMTLSamplerState *> samplers, rdcarray<float> lodMinClamps,
    rdcarray<float> lodMaxClamps, NS::Range range, uint32_t variant)
{
  rdcarray<const MTL::SamplerState *> real;
  for(WrappedMTLSamplerState *sampler : samplers) real.push_back(Unwrap(sampler));
  if(variant == 0)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setObjectSamplerState(real[0], range.location));
  }
  else if(variant == 1)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setObjectSamplerStates(real.data(), range));
  }
  else if(variant == 2)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setObjectSamplerState(
        real[0], lodMinClamps[0], lodMaxClamps[0], range.location));
  }
  else
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setObjectSamplerStates(
        real.data(), lodMinClamps.data(), lodMaxClamps.data(), range));
  }
  if(IsCaptureMode(m_State))
  {
    const MetalChunk chunk = variant == 0 ? MetalChunk::MTLRenderCommandEncoder_setObjectSamplerState :
                             variant == 1 ? MetalChunk::MTLRenderCommandEncoder_setObjectSamplerStates :
                             variant == 2 ? MetalChunk::MTLRenderCommandEncoder_setObjectSamplerState_lodclamp :
                                            MetalChunk::MTLRenderCommandEncoder_setObjectSamplerStates_lodclamp;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_setObjectSamplers(ser, samplers, lodMinClamps, lodMaxClamps, range, variant);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLSamplerState *sampler : samplers)
      if(sampler) record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setObjectSamplers(
    ReadSerialiser &, rdcarray<WrappedMTLSamplerState *>, rdcarray<float>,
    rdcarray<float>, NS::Range, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setObjectSamplers(
    WriteSerialiser &, rdcarray<WrappedMTLSamplerState *>, rdcarray<float>,
    rdcarray<float>, NS::Range, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setObjectThreadgroupMemoryLength(
    SerialiserType &ser, NS::UInteger length, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(length).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || index >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS ||
       (length & 15) != 0 || length > Unwrap(m_Device)->maxThreadgroupMemoryLength())
    {
      RDCERR("Invalid Metal object threadgroup-memory binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setObjectThreadgroupMemoryLength(length, index);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setObjectThreadgroupMemoryLength(
    NS::UInteger length, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setObjectThreadgroupMemoryLength(length, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setObjectThreadgroupMemoryLength);
    Serialise_setObjectThreadgroupMemoryLength(ser, length, index);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void,
                                setObjectThreadgroupMemoryLength,
                                NS::UInteger length, NS::UInteger index);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setTileBinding(
    SerialiserType &ser, rdcarray<WrappedMTLBuffer *> buffers,
    rdcarray<NS::UInteger> offsets, rdcarray<byte> data, NS::Range range, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(buffers).Important();
  SERIALISE_ELEMENT(offsets).Important();
  SERIALISE_ELEMENT(data).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || variant > 2 ||
       range.location >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS ||
       !range.length || range.length > MAX_RENDER_PASS_BUFFER_ATTACHMENTS - range.location ||
       (variant < 2 && range.length != 1) ||
       (variant == 0 && (data.empty() || data.size() > 4096 || !buffers.empty() ||
                         !offsets.empty())) ||
       (variant == 1 && (offsets.size() != 1 || !buffers.empty() || !data.empty())) ||
       (variant == 2 && (buffers.size() != range.length || offsets.size() != range.length ||
                         !data.empty())))
    {
      RDCERR("Invalid Metal tile binding shape or encoder");
      return false;
    }
    if(variant == 0)
    {
      Unwrap(RenderCommandEncoder)->setTileBytes(data.data(), data.size(), range.location);
      m_Device->GetReplay()->BindComputeBytes((uint32_t)range.location, data);
      m_Device->GetReplay()->SaveShaderInlineData(0, (uint32_t)range.location, data, GetResID(RenderCommandEncoder));
    }
    else if(variant == 1)
    {
      WrappedMTLBuffer *buffer = m_EncoderTileBuffers[range.location];
      if(!buffer || !buffer->m_Real || offsets[0] > Unwrap(buffer)->length() ||
         Unwrap(buffer)->length() - offsets[0] < 4)
      {
        RDCERR("Invalid Metal tile buffer offset or missing prior binding");
        return false;
      }
      Unwrap(RenderCommandEncoder)->setTileBufferOffset(offsets[0], range.location);
      m_EncoderTileOffsets[range.location] = offsets[0];
      m_Device->GetReplay()->BindComputeBuffer((uint32_t)range.location, GetResID(buffer), offsets[0]);
    }
    else
    {
      rdcarray<const MTL::Buffer *> real;
      for(size_t i = 0; i < buffers.size(); i++)
      {
        WrappedMTLBuffer *buffer = buffers[i];
        if((buffer && (buffer->m_Type != eResBuffer || !buffer->m_Real ||
                       buffer->m_Device != m_Device || offsets[i] > Unwrap(buffer)->length() ||
                       Unwrap(buffer)->length() - offsets[i] < 4)) ||
           (!buffer && offsets[i] != 0))
        {
          RDCERR("Invalid Metal tile buffer batch resource or range");
          return false;
        }
        real.push_back(Unwrap(buffer));
      }
      Unwrap(RenderCommandEncoder)->setTileBuffers(real.data(), offsets.data(), range);
      for(size_t i = 0; i < buffers.size(); i++)
      {
        m_EncoderTileBuffers[range.location + i] = buffers[i];
        m_EncoderTileOffsets[range.location + i] = offsets[i];
        m_Device->GetReplay()->BindComputeBuffer((uint32_t)(range.location + i), GetResID(buffers[i]), offsets[i]);
      }
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setTileBytes(rdcarray<byte> data, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTileBytes(data.data(), data.size(), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setTileBytes);
    Serialise_setTileBinding(ser, {}, {}, data, NS::Range::Make(index, 1), 0);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

void WrappedMTLRenderCommandEncoder::setTileBufferOffset(NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTileBufferOffset(offset, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setTileBufferOffset);
    Serialise_setTileBinding(ser, {}, {offset}, {}, NS::Range::Make(index, 1), 1);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

void WrappedMTLRenderCommandEncoder::setTileBuffers(
    rdcarray<WrappedMTLBuffer *> buffers, rdcarray<NS::UInteger> offsets, NS::Range range)
{
  rdcarray<const MTL::Buffer *> real;
  for(WrappedMTLBuffer *buffer : buffers) real.push_back(Unwrap(buffer));
  SERIALISE_TIME_CALL(Unwrap(this)->setTileBuffers(real.data(), offsets.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setTileBuffers);
    Serialise_setTileBinding(ser, buffers, offsets, {}, range, 2);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLBuffer *buffer : buffers)
      if(buffer) record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_ReadBeforeWrite);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setTileBinding(
    ReadSerialiser &, rdcarray<WrappedMTLBuffer *>, rdcarray<NS::UInteger>,
    rdcarray<byte>, NS::Range, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setTileBinding(
    WriteSerialiser &, rdcarray<WrappedMTLBuffer *>, rdcarray<NS::UInteger>,
    rdcarray<byte>, NS::Range, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setMeshBinding(
    SerialiserType &ser, rdcarray<WrappedMTLBuffer *> buffers,
    rdcarray<NS::UInteger> offsets, rdcarray<byte> data, NS::Range range, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(buffers).Important();
  SERIALISE_ELEMENT(offsets).Important();
  SERIALISE_ELEMENT(data).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || variant > 3 ||
       range.location >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS || !range.length ||
       range.length > MAX_RENDER_PASS_BUFFER_ATTACHMENTS - range.location ||
       (variant != 3 && range.length != 1) ||
       (variant == 0 && (buffers.size() != 1 || offsets.size() != 1 || !data.empty())) ||
       (variant == 1 && (data.empty() || data.size() > 4096 || !buffers.empty() ||
                         !offsets.empty())) ||
       (variant == 2 && (offsets.size() != 1 || !buffers.empty() || !data.empty())) ||
       (variant == 3 && (buffers.size() != range.length || offsets.size() != range.length ||
                         !data.empty())))
    {
      RDCERR("Invalid Metal mesh binding shape or encoder");
      return false;
    }

    auto validBuffer = [&](WrappedMTLBuffer *buffer, NS::UInteger offset) {
      return buffer ? buffer->m_Type == eResBuffer && buffer->m_Real &&
                          buffer->m_Device == m_Device && offset <= Unwrap(buffer)->length() &&
                          Unwrap(buffer)->length() - offset >= 4
                    : offset == 0;
    };
    if(variant == 0)
    {
      if(!validBuffer(buffers[0], offsets[0])) return false;
      Unwrap(RenderCommandEncoder)->setMeshBuffer(Unwrap(buffers[0]), offsets[0], range.location);
      m_EncoderMeshBuffers[range.location] = buffers[0];
      m_EncoderMeshOffsets[range.location] = offsets[0];
      m_Device->GetReplay()->BindExtendedBuffer(ShaderStage::Mesh, uint32_t(range.location), GetResID(m_EncoderMeshBuffers[range.location]), offsets[0]);
    }
    else if(variant == 1)
    {
      m_Device->GetReplay()->BindExtendedBytes(ShaderStage::Mesh, uint32_t(range.location), data, GetResID(RenderCommandEncoder));
      if(!m_Device->RelocateGraphicsDescriptorBytes(GetResID(RenderCommandEncoder), 4, range.location, data))
        return false;
      Unwrap(RenderCommandEncoder)->setMeshBytes(data.data(), data.size(), range.location);
      m_EncoderMeshBuffers[range.location] = NULL;
      m_EncoderMeshOffsets[range.location] = 0;
    }
    else if(variant == 2)
    {
      WrappedMTLBuffer *buffer = m_EncoderMeshBuffers[range.location];
      if(!buffer || !validBuffer(buffer, offsets[0]))
      {
        RDCERR("Invalid Metal mesh buffer offset or missing prior binding");
        return false;
      }
      Unwrap(RenderCommandEncoder)->setMeshBufferOffset(offsets[0], range.location);
      m_EncoderMeshOffsets[range.location] = offsets[0];
      m_Device->GetReplay()->BindExtendedBuffer(ShaderStage::Mesh, uint32_t(range.location), GetResID(m_EncoderMeshBuffers[range.location]), offsets[0]);
    }
    else
    {
      rdcarray<const MTL::Buffer *> real;
      for(size_t i = 0; i < buffers.size(); i++)
      {
        if(!validBuffer(buffers[i], offsets[i]))
        {
          RDCERR("Invalid Metal mesh buffer batch resource or range");
          return false;
        }
        real.push_back(Unwrap(buffers[i]));
      }
      Unwrap(RenderCommandEncoder)->setMeshBuffers(real.data(), offsets.data(), range);
      for(size_t i = 0; i < buffers.size(); i++)
      {
        m_EncoderMeshBuffers[range.location + i] = buffers[i];
        m_EncoderMeshOffsets[range.location + i] = offsets[i];
        m_Device->GetReplay()->BindExtendedBuffer(ShaderStage::Mesh, uint32_t(range.location+i), GetResID(buffers[i]), offsets[i]);
      }
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setMeshBuffer(WrappedMTLBuffer *buffer,
                                                    NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setMeshBuffer(Unwrap(buffer), offset, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setMeshBuffer);
    Serialise_setMeshBinding(ser, {buffer}, {offset}, {}, NS::Range::Make(index, 1), 0);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(buffer) record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_ReadBeforeWrite);
  }
}

void WrappedMTLRenderCommandEncoder::setMeshBytes(rdcarray<byte> data, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setMeshBytes(data.data(), data.size(), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setMeshBytes);
    Serialise_setMeshBinding(ser, {}, {}, data, NS::Range::Make(index, 1), 1);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

void WrappedMTLRenderCommandEncoder::setMeshBufferOffset(NS::UInteger offset,
                                                          NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setMeshBufferOffset(offset, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setMeshBufferOffset);
    Serialise_setMeshBinding(ser, {}, {offset}, {}, NS::Range::Make(index, 1), 2);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

void WrappedMTLRenderCommandEncoder::setMeshBuffers(
    rdcarray<WrappedMTLBuffer *> buffers, rdcarray<NS::UInteger> offsets, NS::Range range)
{
  rdcarray<const MTL::Buffer *> real;
  for(WrappedMTLBuffer *buffer : buffers) real.push_back(Unwrap(buffer));
  SERIALISE_TIME_CALL(Unwrap(this)->setMeshBuffers(real.data(), offsets.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setMeshBuffers);
    Serialise_setMeshBinding(ser, buffers, offsets, {}, range, 3);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLBuffer *buffer : buffers)
      if(buffer) record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_ReadBeforeWrite);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setMeshBinding(
    ReadSerialiser &, rdcarray<WrappedMTLBuffer *>, rdcarray<NS::UInteger>,
    rdcarray<byte>, NS::Range, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setMeshBinding(
    WriteSerialiser &, rdcarray<WrappedMTLBuffer *>, rdcarray<NS::UInteger>,
    rdcarray<byte>, NS::Range, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setMeshTextures(
    SerialiserType &ser, rdcarray<WrappedMTLTexture *> textures,
    NS::Range range, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_ELEMENT(textures).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || variant > 1 ||
       range.location >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS || !range.length ||
       range.length > MAX_RENDER_PASS_BUFFER_ATTACHMENTS - range.location ||
       (variant == 0 && range.length != 1) || textures.size() != range.length)
    {
      RDCERR("Invalid Metal mesh texture binding shape or encoder");
      return false;
    }
    rdcarray<const MTL::Texture *> real;
    for(WrappedMTLTexture *texture : textures)
    {
      if(texture && (texture->m_Type != eResTexture || !texture->m_Real ||
                     texture->m_Device != m_Device))
      {
        RDCERR("Invalid Metal mesh texture resource");
        return false;
      }
      real.push_back(Unwrap(texture));
    }
    if(variant == 0)
      Unwrap(RenderCommandEncoder)->setMeshTexture(real[0], range.location);
    else
      Unwrap(RenderCommandEncoder)->setMeshTextures(real.data(), range);
    for(size_t i = 0; i < textures.size(); i++)
      m_Device->GetReplay()->BindExtendedTexture(ShaderStage::Mesh, uint32_t(range.location+i), GetResID(textures[i]));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setMeshTextures(
    rdcarray<WrappedMTLTexture *> textures, NS::Range range, uint32_t variant)
{
  rdcarray<const MTL::Texture *> real;
  for(WrappedMTLTexture *texture : textures) real.push_back(Unwrap(texture));
  if(variant == 0)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setMeshTexture(real[0], range.location));
  }
  else
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setMeshTextures(real.data(), range));
  }
  if(IsCaptureMode(m_State))
  {
    const MetalChunk chunk = variant == 0 ? MetalChunk::MTLRenderCommandEncoder_setMeshTexture :
                                           MetalChunk::MTLRenderCommandEncoder_setMeshTextures;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_setMeshTextures(ser, textures, range, variant);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLTexture *texture : textures)
      if(texture) record->MarkResourceFrameReferenced(GetResID(texture), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setMeshTextures(
    ReadSerialiser &, rdcarray<WrappedMTLTexture *>, NS::Range, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setMeshTextures(
    WriteSerialiser &, rdcarray<WrappedMTLTexture *>, NS::Range, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setMeshSamplers(
    SerialiserType &ser, rdcarray<WrappedMTLSamplerState *> samplers,
    rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps,
    NS::Range range, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_ELEMENT(samplers).Important();
  SERIALISE_ELEMENT(lodMinClamps).Important();
  SERIALISE_ELEMENT(lodMaxClamps).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    const bool clamped = variant >= 2;
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || variant > 3 || range.location >= 16 ||
       !range.length || range.length > 16 - range.location ||
       (variant % 2 == 0 && range.length != 1) || samplers.size() != range.length ||
       (clamped && (lodMinClamps.size() != range.length ||
                    lodMaxClamps.size() != range.length)) ||
       (!clamped && (!lodMinClamps.empty() || !lodMaxClamps.empty())))
    {
      RDCERR("Invalid Metal mesh sampler binding shape or encoder");
      return false;
    }
    rdcarray<const MTL::SamplerState *> real;
    for(size_t i = 0; i < samplers.size(); i++)
    {
      WrappedMTLSamplerState *sampler = samplers[i];
      if((sampler && (sampler->m_Type != eResSamplerState || !sampler->m_Real ||
                      sampler->m_Device != m_Device)) ||
         (clamped && (!std::isfinite(lodMinClamps[i]) ||
                      !std::isfinite(lodMaxClamps[i]) || lodMinClamps[i] < 0.0f ||
                      lodMaxClamps[i] < lodMinClamps[i])))
      {
        RDCERR("Invalid Metal mesh sampler resource or LOD clamp");
        return false;
      }
      real.push_back(Unwrap(sampler));
    }
    if(variant == 0)
      Unwrap(RenderCommandEncoder)->setMeshSamplerState(real[0], range.location);
    else if(variant == 1)
      Unwrap(RenderCommandEncoder)->setMeshSamplerStates(real.data(), range);
    else if(variant == 2)
      Unwrap(RenderCommandEncoder)->setMeshSamplerState(
          real[0], lodMinClamps[0], lodMaxClamps[0], range.location);
    else
      Unwrap(RenderCommandEncoder)->setMeshSamplerStates(
          real.data(), lodMinClamps.data(), lodMaxClamps.data(), range);
    for(size_t i = 0; i < samplers.size(); i++)
    {
      m_Device->GetReplay()->BindExtendedSampler(ShaderStage::Mesh, uint32_t(range.location+i), GetResID(samplers[i]));
      if(clamped) m_Device->GetReplay()->SetSamplerLOD(ShaderStage::Mesh, uint32_t(range.location+i), lodMinClamps[i], lodMaxClamps[i]);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setMeshSamplers(
    rdcarray<WrappedMTLSamplerState *> samplers, rdcarray<float> lodMinClamps,
    rdcarray<float> lodMaxClamps, NS::Range range, uint32_t variant)
{
  rdcarray<const MTL::SamplerState *> real;
  for(WrappedMTLSamplerState *sampler : samplers) real.push_back(Unwrap(sampler));
  if(variant == 0)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setMeshSamplerState(real[0], range.location));
  }
  else if(variant == 1)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setMeshSamplerStates(real.data(), range));
  }
  else if(variant == 2)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setMeshSamplerState(
        real[0], lodMinClamps[0], lodMaxClamps[0], range.location));
  }
  else
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setMeshSamplerStates(
        real.data(), lodMinClamps.data(), lodMaxClamps.data(), range));
  }
  if(IsCaptureMode(m_State))
  {
    const MetalChunk chunk = variant == 0 ? MetalChunk::MTLRenderCommandEncoder_setMeshSamplerState :
                             variant == 1 ? MetalChunk::MTLRenderCommandEncoder_setMeshSamplerStates :
                             variant == 2 ? MetalChunk::MTLRenderCommandEncoder_setMeshSamplerState_lodclamp :
                                            MetalChunk::MTLRenderCommandEncoder_setMeshSamplerStates_lodclamp;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_setMeshSamplers(ser, samplers, lodMinClamps, lodMaxClamps, range, variant);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLSamplerState *sampler : samplers)
      if(sampler) record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setMeshSamplers(
    ReadSerialiser &, rdcarray<WrappedMTLSamplerState *>, rdcarray<float>,
    rdcarray<float>, NS::Range, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setMeshSamplers(
    WriteSerialiser &, rdcarray<WrappedMTLSamplerState *>, rdcarray<float>,
    rdcarray<float>, NS::Range, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setTileTextures(
    SerialiserType &ser, rdcarray<WrappedMTLTexture *> textures,
    NS::Range range, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_ELEMENT(textures).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || variant > 1 ||
       range.location >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS || !range.length ||
       range.length > MAX_RENDER_PASS_BUFFER_ATTACHMENTS - range.location ||
       (variant == 0 && range.length != 1) || textures.size() != range.length)
    {
      RDCERR("Invalid Metal tile texture binding shape or encoder");
      return false;
    }
    rdcarray<const MTL::Texture *> real;
    for(WrappedMTLTexture *texture : textures)
    {
      if(texture && (texture->m_Type != eResTexture || !texture->m_Real ||
                     texture->m_Device != m_Device))
      {
        RDCERR("Invalid Metal tile texture resource");
        return false;
      }
      real.push_back(Unwrap(texture));
    }
    if(variant == 0)
      Unwrap(RenderCommandEncoder)->setTileTexture(real[0], range.location);
    else
      Unwrap(RenderCommandEncoder)->setTileTextures(real.data(), range);
    for(size_t i = 0; i < textures.size(); i++)
      m_Device->GetReplay()->SetComputeTexture((uint32_t)(range.location + i), GetResID(textures[i]));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setTileTextures(
    rdcarray<WrappedMTLTexture *> textures, NS::Range range, uint32_t variant)
{
  rdcarray<const MTL::Texture *> real;
  for(WrappedMTLTexture *texture : textures) real.push_back(Unwrap(texture));
  if(variant == 0)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setTileTexture(real[0], range.location));
  }
  else
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setTileTextures(real.data(), range));
  }
  if(IsCaptureMode(m_State))
  {
    const MetalChunk chunk = variant == 0 ? MetalChunk::MTLRenderCommandEncoder_setTileTexture :
                                           MetalChunk::MTLRenderCommandEncoder_setTileTextures;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_setTileTextures(ser, textures, range, variant);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLTexture *texture : textures)
      if(texture) record->MarkResourceFrameReferenced(GetResID(texture), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setTileTextures(
    ReadSerialiser &, rdcarray<WrappedMTLTexture *>, NS::Range, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setTileTextures(
    WriteSerialiser &, rdcarray<WrappedMTLTexture *>, NS::Range, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setTileSamplers(
    SerialiserType &ser, rdcarray<WrappedMTLSamplerState *> samplers,
    rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps,
    NS::Range range, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_ELEMENT(samplers).Important();
  SERIALISE_ELEMENT(lodMinClamps).Important();
  SERIALISE_ELEMENT(lodMaxClamps).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    const bool clamped = variant >= 2;
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || variant > 3 || range.location >= 16 ||
       !range.length || range.length > 16 - range.location ||
       (variant % 2 == 0 && range.length != 1) || samplers.size() != range.length ||
       (clamped && (lodMinClamps.size() != range.length ||
                    lodMaxClamps.size() != range.length)) ||
       (!clamped && (!lodMinClamps.empty() || !lodMaxClamps.empty())))
    {
      RDCERR("Invalid Metal tile sampler binding shape or encoder");
      return false;
    }
    rdcarray<const MTL::SamplerState *> real;
    for(size_t i = 0; i < samplers.size(); i++)
    {
      WrappedMTLSamplerState *sampler = samplers[i];
      if((sampler && (sampler->m_Type != eResSamplerState || !sampler->m_Real ||
                      sampler->m_Device != m_Device)) ||
         (clamped && (!std::isfinite(lodMinClamps[i]) ||
                      !std::isfinite(lodMaxClamps[i]) || lodMinClamps[i] < 0.0f ||
                      lodMaxClamps[i] < lodMinClamps[i])))
      {
        RDCERR("Invalid Metal tile sampler resource or LOD clamp");
        return false;
      }
      real.push_back(Unwrap(sampler));
    }
    if(variant == 0)
      Unwrap(RenderCommandEncoder)->setTileSamplerState(real[0], range.location);
    else if(variant == 1)
      Unwrap(RenderCommandEncoder)->setTileSamplerStates(real.data(), range);
    else if(variant == 2)
      Unwrap(RenderCommandEncoder)->setTileSamplerState(
          real[0], lodMinClamps[0], lodMaxClamps[0], range.location);
    else
      Unwrap(RenderCommandEncoder)->setTileSamplerStates(
          real.data(), lodMinClamps.data(), lodMaxClamps.data(), range);
    for(size_t i = 0; i < samplers.size(); i++)
    {
      m_Device->GetReplay()->BindComputeSampler((uint32_t)(range.location + i), GetResID(samplers[i]));
      if(clamped) m_Device->GetReplay()->SetSamplerLOD(ShaderStage::Compute, (uint32_t)(range.location + i),
                                                     lodMinClamps[i], lodMaxClamps[i]);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setTileSamplers(
    rdcarray<WrappedMTLSamplerState *> samplers, rdcarray<float> lodMinClamps,
    rdcarray<float> lodMaxClamps, NS::Range range, uint32_t variant)
{
  rdcarray<const MTL::SamplerState *> real;
  for(WrappedMTLSamplerState *sampler : samplers) real.push_back(Unwrap(sampler));
  if(variant == 0)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setTileSamplerState(real[0], range.location));
  }
  else if(variant == 1)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setTileSamplerStates(real.data(), range));
  }
  else if(variant == 2)
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setTileSamplerState(
        real[0], lodMinClamps[0], lodMaxClamps[0], range.location));
  }
  else
  {
    SERIALISE_TIME_CALL(Unwrap(this)->setTileSamplerStates(
        real.data(), lodMinClamps.data(), lodMaxClamps.data(), range));
  }
  if(IsCaptureMode(m_State))
  {
    const MetalChunk chunk = variant == 0 ? MetalChunk::MTLRenderCommandEncoder_setTileSamplerState :
                             variant == 1 ? MetalChunk::MTLRenderCommandEncoder_setTileSamplerStates :
                             variant == 2 ? MetalChunk::MTLRenderCommandEncoder_setTileSamplerState_lodclamp :
                                            MetalChunk::MTLRenderCommandEncoder_setTileSamplerStates_lodclamp;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_setTileSamplers(ser, samplers, lodMinClamps, lodMaxClamps, range, variant);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLSamplerState *sampler : samplers)
      if(sampler) record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setTileSamplers(
    ReadSerialiser &, rdcarray<WrappedMTLSamplerState *>, rdcarray<float>,
    rdcarray<float>, NS::Range, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setTileSamplers(
    WriteSerialiser &, rdcarray<WrappedMTLSamplerState *>, rdcarray<float>,
    rdcarray<float>, NS::Range, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setThreadgroupMemoryLength(
    SerialiserType &ser, NS::UInteger length, NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(length).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    const RDMTL::RenderPassDescriptor &pass = m_Device->GetReplay()->GetRenderPassDescriptor();
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || index >= 31 || (!length && offset != 0) ||
       (length & 15) != 0 || (offset & 15) != 0 ||
       offset > pass.threadgroupMemoryLength ||
       length > pass.threadgroupMemoryLength - offset ||
       length > Unwrap(m_Device)->maxThreadgroupMemoryLength())
    {
      RDCERR("Invalid Metal tile threadgroup memory binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setThreadgroupMemoryLength(length, offset, index);
    m_Device->GetReplay()->SetTileMemory((uint32_t)index, length, offset);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setThreadgroupMemoryLength(
    NS::UInteger length, NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setThreadgroupMemoryLength(length, offset, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setThreadgroupMemoryLength);
    Serialise_setThreadgroupMemoryLength(ser, length, offset, index);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void,
                                setThreadgroupMemoryLength, NS::UInteger length,
                                NS::UInteger offset, NS::UInteger index);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawMeshThreadgroups(
    SerialiserType &ser, MTL::Size threadgroupsPerGrid,
    MTL::Size threadsPerObjectThreadgroup, MTL::Size threadsPerMeshThreadgroup)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(threadgroupsPerGrid).Important();
  SERIALISE_ELEMENT(threadsPerObjectThreadgroup).Important();
  SERIALISE_ELEMENT(threadsPerMeshThreadgroup).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    MTL::RenderPipelineState *pipeline = m_EncoderPipeline ? Unwrap(m_EncoderPipeline) : NULL;
    const uint64_t meshLimit = pipeline ? pipeline->maxTotalThreadsPerMeshThreadgroup() : 0;
    const uint64_t objectLimit = pipeline ? pipeline->maxTotalThreadsPerObjectThreadgroup() : 0;
    const uint64_t gridLimit = pipeline ? pipeline->maxTotalThreadgroupsPerMeshGrid() : 0;
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || !pipeline ||
       !m_Device->GetReplay()->IsMeshPipeline(GetResID(m_EncoderPipeline)) ||
       !threadgroupsPerGrid.width || !threadgroupsPerGrid.height ||
       !threadgroupsPerGrid.depth || threadgroupsPerGrid.width > UINT32_MAX ||
       threadgroupsPerGrid.height > UINT32_MAX || threadgroupsPerGrid.depth > UINT32_MAX ||
       !gridLimit ||
       (!objectLimit &&
        (threadgroupsPerGrid.width > gridLimit / threadgroupsPerGrid.height ||
         threadgroupsPerGrid.width * threadgroupsPerGrid.height >
             gridLimit / threadgroupsPerGrid.depth)) ||
       !threadsPerObjectThreadgroup.width || !threadsPerObjectThreadgroup.height ||
       !threadsPerObjectThreadgroup.depth ||
       (objectLimit ?
            (threadsPerObjectThreadgroup.width >
                 objectLimit / threadsPerObjectThreadgroup.height ||
             threadsPerObjectThreadgroup.width * threadsPerObjectThreadgroup.height >
                 objectLimit / threadsPerObjectThreadgroup.depth) :
            (threadsPerObjectThreadgroup.width != 1 ||
             threadsPerObjectThreadgroup.height != 1 ||
             threadsPerObjectThreadgroup.depth != 1)) ||
       !threadsPerMeshThreadgroup.width || !threadsPerMeshThreadgroup.height ||
       !threadsPerMeshThreadgroup.depth || !meshLimit ||
       threadsPerMeshThreadgroup.width > meshLimit / threadsPerMeshThreadgroup.height ||
       threadsPerMeshThreadgroup.width * threadsPerMeshThreadgroup.height >
           meshLimit / threadsPerMeshThreadgroup.depth)
    {
      RDCERR("Invalid Metal mesh pipeline or direct draw dimensions");
      return false;
    }
    Unwrap(RenderCommandEncoder)->drawMeshThreadgroups(
        threadgroupsPerGrid, threadsPerObjectThreadgroup, threadsPerMeshThreadgroup);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawMeshThreadgroups(%llux%llux%llu)",
                                            (uint64_t)threadgroupsPerGrid.width,
                                            (uint64_t)threadgroupsPerGrid.height,
                                            (uint64_t)threadgroupsPerGrid.depth);
      action.flags = ActionFlags::MeshDispatch;
      action.dispatchDimension[0] = (uint32_t)threadgroupsPerGrid.width;
      action.dispatchDimension[1] = (uint32_t)threadgroupsPerGrid.height;
      action.dispatchDimension[2] = (uint32_t)threadgroupsPerGrid.depth;
      action.dispatchThreadsDimension[0] = (uint32_t)threadsPerMeshThreadgroup.width;
      action.dispatchThreadsDimension[1] = (uint32_t)threadsPerMeshThreadgroup.height;
      action.dispatchThreadsDimension[2] = (uint32_t)threadsPerMeshThreadgroup.depth;
      m_Device->GetReplay()->SetActionOutputs(action);
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawMeshThreadgroups(
    MTL::Size threadgroupsPerGrid, MTL::Size threadsPerObjectThreadgroup,
    MTL::Size threadsPerMeshThreadgroup)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawMeshThreadgroups(
      threadgroupsPerGrid, threadsPerObjectThreadgroup, threadsPerMeshThreadgroup));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawMeshThreadgroups);
    Serialise_drawMeshThreadgroups(ser, threadgroupsPerGrid,
                                   threadsPerObjectThreadgroup, threadsPerMeshThreadgroup);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void,
                                drawMeshThreadgroups, MTL::Size threadgroupsPerGrid,
                                MTL::Size threadsPerObjectThreadgroup,
                                MTL::Size threadsPerMeshThreadgroup);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawMeshThreadgroups(
    SerialiserType &ser, WrappedMTLBuffer *indirectBuffer, NS::UInteger indirectBufferOffset,
    MTL::Size threadsPerObjectThreadgroup, MTL::Size threadsPerMeshThreadgroup)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(indirectBuffer).Important();
  SERIALISE_ELEMENT(indirectBufferOffset).Important();
  SERIALISE_ELEMENT(threadsPerObjectThreadgroup).Important();
  SERIALISE_ELEMENT(threadsPerMeshThreadgroup).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    MTL::RenderPipelineState *pipeline = m_EncoderPipeline ? Unwrap(m_EncoderPipeline) : NULL;
    MTL::Buffer *realBuffer = Unwrap(indirectBuffer);
    const uint64_t meshLimit = pipeline ? pipeline->maxTotalThreadsPerMeshThreadgroup() : 0;
    const uint64_t objectLimit = pipeline ? pipeline->maxTotalThreadsPerObjectThreadgroup() : 0;
    const uint64_t argumentSize = sizeof(MTL::DispatchThreadgroupsIndirectArguments);
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || !pipeline ||
       !m_Device->GetReplay()->IsMeshPipeline(GetResID(m_EncoderPipeline)) || !realBuffer ||
       (indirectBufferOffset & 3) != 0 || indirectBufferOffset > realBuffer->length() ||
       argumentSize > realBuffer->length() - indirectBufferOffset ||
       !threadsPerObjectThreadgroup.width || !threadsPerObjectThreadgroup.height ||
       !threadsPerObjectThreadgroup.depth ||
       (objectLimit ?
            (threadsPerObjectThreadgroup.width >
                 objectLimit / threadsPerObjectThreadgroup.height ||
             threadsPerObjectThreadgroup.width * threadsPerObjectThreadgroup.height >
                 objectLimit / threadsPerObjectThreadgroup.depth) :
            (threadsPerObjectThreadgroup.width != 1 ||
             threadsPerObjectThreadgroup.height != 1 ||
             threadsPerObjectThreadgroup.depth != 1)) ||
       !threadsPerMeshThreadgroup.width || !threadsPerMeshThreadgroup.height ||
       !threadsPerMeshThreadgroup.depth || !meshLimit ||
       threadsPerMeshThreadgroup.width > meshLimit / threadsPerMeshThreadgroup.height ||
       threadsPerMeshThreadgroup.width * threadsPerMeshThreadgroup.height >
           meshLimit / threadsPerMeshThreadgroup.depth)
    {
      RDCERR("Invalid Metal mesh indirect draw pipeline, buffer, offset, or threadgroup size");
      return false;
    }
    MetalReplay *replay = m_Device->GetReplay();
    replay->SetIndirectBuffer(GetResID(indirectBuffer), indirectBufferOffset, argumentSize);
    Unwrap(RenderCommandEncoder)
        ->drawMeshThreadgroups(realBuffer, indirectBufferOffset,
                               threadsPerObjectThreadgroup, threadsPerMeshThreadgroup);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "drawMeshThreadgroups(indirect, <?, ?, ?>)";
      action.flags = ActionFlags::MeshDispatch | ActionFlags::Indirect;
      action.dispatchThreadsDimension[0] = (uint32_t)threadsPerMeshThreadgroup.width;
      action.dispatchThreadsDimension[1] = (uint32_t)threadsPerMeshThreadgroup.height;
      action.dispatchThreadsDimension[2] = (uint32_t)threadsPerMeshThreadgroup.depth;
      replay->SetActionOutputs(action);
      AddAction(action);
      replay->AddUsage(GetResID(indirectBuffer), ResourceUsage::Indirect);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawMeshThreadgroups(
    WrappedMTLBuffer *indirectBuffer, NS::UInteger indirectBufferOffset,
    MTL::Size threadsPerObjectThreadgroup, MTL::Size threadsPerMeshThreadgroup)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawMeshThreadgroups(
      Unwrap(indirectBuffer), indirectBufferOffset, threadsPerObjectThreadgroup,
      threadsPerMeshThreadgroup));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawMeshThreadgroups_indirect);
    Serialise_drawMeshThreadgroups(ser, indirectBuffer, indirectBufferOffset,
                                   threadsPerObjectThreadgroup, threadsPerMeshThreadgroup);
    MetalResourceRecord *commandBufferRecord = GetRecord(m_CommandBuffer);
    commandBufferRecord->AddChunk(scope.Get());
    commandBufferRecord->MarkResourceFrameReferenced(GetResID(indirectBuffer), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void,
                                drawMeshThreadgroups, WrappedMTLBuffer *indirectBuffer,
                                NS::UInteger indirectBufferOffset,
                                MTL::Size threadsPerObjectThreadgroup,
                                MTL::Size threadsPerMeshThreadgroup);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawMeshThreads(
    SerialiserType &ser, MTL::Size threadsPerGrid,
    MTL::Size threadsPerObjectThreadgroup, MTL::Size threadsPerMeshThreadgroup)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(threadsPerGrid).Important();
  SERIALISE_ELEMENT(threadsPerObjectThreadgroup).Important();
  SERIALISE_ELEMENT(threadsPerMeshThreadgroup).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    MTL::RenderPipelineState *pipeline = m_EncoderPipeline ? Unwrap(m_EncoderPipeline) : NULL;
    const uint64_t meshLimit = pipeline ? pipeline->maxTotalThreadsPerMeshThreadgroup() : 0;
    const uint64_t objectLimit = pipeline ? pipeline->maxTotalThreadsPerObjectThreadgroup() : 0;
    const uint64_t gridLimit = pipeline ? pipeline->maxTotalThreadgroupsPerMeshGrid() : 0;
    const uint64_t gx = threadsPerMeshThreadgroup.width ?
        1 + (threadsPerGrid.width - 1) / threadsPerMeshThreadgroup.width : 0;
    const uint64_t gy = threadsPerMeshThreadgroup.height ?
        1 + (threadsPerGrid.height - 1) / threadsPerMeshThreadgroup.height : 0;
    const uint64_t gz = threadsPerMeshThreadgroup.depth ?
        1 + (threadsPerGrid.depth - 1) / threadsPerMeshThreadgroup.depth : 0;
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || !pipeline ||
       !m_Device->GetReplay()->IsMeshPipeline(GetResID(m_EncoderPipeline)) ||
       !threadsPerGrid.width || !threadsPerGrid.height || !threadsPerGrid.depth ||
       threadsPerGrid.width > UINT32_MAX || threadsPerGrid.height > UINT32_MAX ||
       threadsPerGrid.depth > UINT32_MAX ||
       !threadsPerObjectThreadgroup.width || !threadsPerObjectThreadgroup.height ||
       !threadsPerObjectThreadgroup.depth ||
       (objectLimit ?
            (threadsPerObjectThreadgroup.width >
                 objectLimit / threadsPerObjectThreadgroup.height ||
             threadsPerObjectThreadgroup.width * threadsPerObjectThreadgroup.height >
                 objectLimit / threadsPerObjectThreadgroup.depth) :
            (threadsPerObjectThreadgroup.width != 1 ||
             threadsPerObjectThreadgroup.height != 1 ||
             threadsPerObjectThreadgroup.depth != 1)) ||
       !threadsPerMeshThreadgroup.width || !threadsPerMeshThreadgroup.height ||
       !threadsPerMeshThreadgroup.depth || !meshLimit ||
       threadsPerMeshThreadgroup.width > meshLimit / threadsPerMeshThreadgroup.height ||
       threadsPerMeshThreadgroup.width * threadsPerMeshThreadgroup.height >
           meshLimit / threadsPerMeshThreadgroup.depth ||
       !gridLimit || (!objectLimit && (gx > gridLimit / gy || gx * gy > gridLimit / gz)))
    {
      RDCERR("Invalid Metal mesh pipeline or thread-grid dimensions");
      return false;
    }
    Unwrap(RenderCommandEncoder)->drawMeshThreads(
        threadsPerGrid, threadsPerObjectThreadgroup, threadsPerMeshThreadgroup);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawMeshThreads(%llux%llux%llu)",
                                            (uint64_t)threadsPerGrid.width,
                                            (uint64_t)threadsPerGrid.height,
                                            (uint64_t)threadsPerGrid.depth);
      action.flags = ActionFlags::MeshDispatch;
      action.dispatchDimension[0] = (uint32_t)threadsPerGrid.width;
      action.dispatchDimension[1] = (uint32_t)threadsPerGrid.height;
      action.dispatchDimension[2] = (uint32_t)threadsPerGrid.depth;
      action.dispatchThreadsDimension[0] = (uint32_t)threadsPerMeshThreadgroup.width;
      action.dispatchThreadsDimension[1] = (uint32_t)threadsPerMeshThreadgroup.height;
      action.dispatchThreadsDimension[2] = (uint32_t)threadsPerMeshThreadgroup.depth;
      m_Device->GetReplay()->SetActionOutputs(action);
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawMeshThreads(
    MTL::Size threadsPerGrid, MTL::Size threadsPerObjectThreadgroup,
    MTL::Size threadsPerMeshThreadgroup)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawMeshThreads(
      threadsPerGrid, threadsPerObjectThreadgroup, threadsPerMeshThreadgroup));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawMeshThreads);
    Serialise_drawMeshThreads(ser, threadsPerGrid, threadsPerObjectThreadgroup,
                              threadsPerMeshThreadgroup);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void,
                                drawMeshThreads, MTL::Size threadsPerGrid,
                                MTL::Size threadsPerObjectThreadgroup,
                                MTL::Size threadsPerMeshThreadgroup);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_dispatchThreadsPerTile(
    SerialiserType &ser, MTL::Size threadsPerTile)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(threadsPerTile).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    const uint64_t tileWidth = RenderCommandEncoder ? Unwrap(RenderCommandEncoder)->tileWidth() : 0;
    const uint64_t tileHeight = RenderCommandEncoder ? Unwrap(RenderCommandEncoder)->tileHeight() : 0;
    const RDMTL::RenderPassDescriptor &pass = m_Device->GetReplay()->GetRenderPassDescriptor();
    uint64_t targetWidth = pass.renderTargetWidth;
    uint64_t targetHeight = pass.renderTargetHeight;
    if((!targetWidth || !targetHeight) && !pass.colorAttachments.empty() &&
       pass.colorAttachments[0].texture && pass.colorAttachments[0].texture->m_Real)
    {
      MTL::Texture *target = Unwrap(pass.colorAttachments[0].texture);
      if(!targetWidth) targetWidth = target->width();
      if(!targetHeight) targetHeight = target->height();
    }
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || !m_EncoderPipeline ||
       !m_Device->GetReplay()->IsTilePipeline(GetResID(m_EncoderPipeline)) ||
       !tileWidth || !tileHeight || !targetWidth || !targetHeight ||
       1 + (targetWidth - 1) / tileWidth > UINT32_MAX ||
       1 + (targetHeight - 1) / tileHeight > UINT32_MAX ||
       !threadsPerTile.width || !threadsPerTile.height ||
       threadsPerTile.depth != 1 || threadsPerTile.width > tileWidth ||
       threadsPerTile.height > tileHeight || threadsPerTile.width > 1024 ||
       threadsPerTile.height > 1024 ||
       threadsPerTile.width > 1024 / threadsPerTile.height)
    {
      RDCERR("Invalid Metal tile dispatch pipeline or thread dimensions");
      return false;
    }
    m_Device->GetReplay()->SetTileDispatch(threadsPerTile, (uint32_t)tileWidth, (uint32_t)tileHeight);
    Unwrap(RenderCommandEncoder)->dispatchThreadsPerTile(threadsPerTile);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("dispatchThreadsPerTile(%llux%llux1)",
                                            (uint64_t)threadsPerTile.width,
                                            (uint64_t)threadsPerTile.height);
      action.flags = ActionFlags::Dispatch;
      action.dispatchDimension[0] = (uint32_t)(1 + (targetWidth - 1) / tileWidth);
      action.dispatchDimension[1] = (uint32_t)(1 + (targetHeight - 1) / tileHeight);
      action.dispatchDimension[2] = 1;
      action.dispatchThreadsDimension[0] = (uint32_t)threadsPerTile.width;
      action.dispatchThreadsDimension[1] = (uint32_t)threadsPerTile.height;
      action.dispatchThreadsDimension[2] = 1;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::dispatchThreadsPerTile(MTL::Size threadsPerTile)
{
  SERIALISE_TIME_CALL(Unwrap(this)->dispatchThreadsPerTile(threadsPerTile));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_dispatchThreadsPerTile);
    Serialise_dispatchThreadsPerTile(ser, threadsPerTile);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setTileBuffer,
                                WrappedMTLBuffer *, NS::UInteger, NS::UInteger);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, dispatchThreadsPerTile,
                                MTL::Size);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexAmplificationCount(
    SerialiserType &ser, NS::UInteger count, rdcarray<uint32_t> viewportOffsets,
    rdcarray<uint32_t> targetOffsets, bool hasMappings)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_ELEMENT(hasMappings).Important();
  SERIALISE_ELEMENT(viewportOffsets).Important();
  SERIALISE_ELEMENT(targetOffsets).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    // Multi-view amplification needs render-target array state and separate output QA.
    if(!RenderCommandEncoder || count != 1 ||
       (hasMappings && (viewportOffsets.size() != 1 || targetOffsets.size() != 1 ||
                        viewportOffsets[0] != 0 || targetOffsets[0] != 0)) ||
       (!hasMappings && (!viewportOffsets.empty() || !targetOffsets.empty())))
    {
      RDCERR("Invalid or unsupported Metal vertex amplification mapping");
      return false;
    }
    MTL::VertexAmplificationViewMapping mapping = {0,0};
    Unwrap(RenderCommandEncoder)->setVertexAmplificationCount(1,hasMappings ? &mapping : NULL);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexAmplificationCount(
    NS::UInteger count, rdcarray<uint32_t> viewportOffsets,
    rdcarray<uint32_t> targetOffsets, bool hasMappings)
{
  rdcarray<MTL::VertexAmplificationViewMapping> mappings;
  if(hasMappings)
    for(size_t i = 0; i < viewportOffsets.size(); i++)
      mappings.push_back({viewportOffsets[i],targetOffsets[i]});
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexAmplificationCount(
      count,hasMappings ? mappings.data() : NULL));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexAmplificationCount);
    Serialise_setVertexAmplificationCount(ser,count,viewportOffsets,targetOffsets,hasMappings);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexBuffer(SerialiserType &ser,
                                                               WrappedMTLBuffer *buffer,
                                                               NS::UInteger offset,
                                                               NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(index >= 31 || (!buffer && offset != 0) || (buffer &&
        (buffer->m_Type != eResBuffer || !buffer->m_Real || offset % 4)))
    {
      RDCERR("Invalid Metal vertex buffer slot %llu", (uint64_t)index);
      return false;
    }
    if(buffer && offset >= Unwrap(buffer)->length())
    {
      RDCERR("Invalid Metal vertex buffer offset %llu for %llu-byte buffer at slot %llu",
             (uint64_t)offset, (uint64_t)Unwrap(buffer)->length(), (uint64_t)index);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setVertexBuffer(Unwrap(buffer), offset, index);
    if(index < MAX_RENDER_PASS_BUFFER_ATTACHMENTS)
    {
      m_EncoderVertexBuffers[index] = buffer;
      m_EncoderVertexOffsets[index] = offset;
    }
    m_Device->GetReplay()->BindVertexBuffer((uint32_t)index, GetResID(buffer), (uint64_t)offset);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexBuffer(WrappedMTLBuffer *buffer, NS::UInteger offset,
                                                     NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexBuffer(Unwrap(buffer), offset, index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexBuffer);
      Serialise_setVertexBuffer(ser, buffer, offset, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexBytes(SerialiserType &ser,
                                                              rdcarray<byte> data,
                                                              NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(data).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || index >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS ||
       data.size() > 4096)
    {
      RDCERR("Invalid Metal inline vertex bytes binding at slot %llu with %llu bytes",
             (uint64_t)index, (uint64_t)data.size());
      return false;
    }
    m_Device->GetReplay()->BindGraphicsBytes(1, (uint32_t)index, data, GetResID(RenderCommandEncoder));
    if(!m_Device->RelocateGraphicsDescriptorBytes(GetResID(RenderCommandEncoder), 1, index, data))
      return false;
    Unwrap(RenderCommandEncoder)->setVertexBytes(data.data(), data.size(), index);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexBytes(rdcarray<byte> data, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexBytes(data.data(), data.size(), index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexBytes);
      Serialise_setVertexBytes(ser, data, index);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexBufferOffset(SerialiserType &ser,
                                                                     NS::UInteger offset,
                                                                     NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || index >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS ||
       (m_EncoderVertexBuffers[index] != NULL &&
        offset >= Unwrap(m_EncoderVertexBuffers[index])->length()))
    {
      RDCERR("Invalid Metal vertex buffer offset slot %llu", (uint64_t)index);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setVertexBufferOffset(offset, index);
    if(index < MAX_RENDER_PASS_BUFFER_ATTACHMENTS)
      m_EncoderVertexOffsets[index] = offset;
    m_Device->GetReplay()->SetVertexBufferOffset((uint32_t)index, (uint64_t)offset);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexBufferOffset(NS::UInteger offset,
                                                           NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexBufferOffset(offset, index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexBufferOffset);
      Serialise_setVertexBufferOffset(ser, offset, index);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexBuffers(
    SerialiserType &ser, rdcarray<WrappedMTLBuffer *> buffers,
    rdcarray<NS::UInteger> offsets, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (range.location >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS ||
      range.length > MAX_RENDER_PASS_BUFFER_ATTACHMENTS - range.location))
  {
    RDCERR("Invalid Metal vertex buffer batch range %llu+%llu", (uint64_t)range.location,
           (uint64_t)range.length);
    return false;
  }

  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLBuffer *buffer : buffers)
      bound.push_back(buffer != NULL ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(buffers).Important();
  SERIALISE_ELEMENT(offsets).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || buffers.size() != range.length ||
       offsets.size() != range.length || bound.size() != range.length)
    {
      RDCERR("Invalid Metal vertex buffer batch resource count");
      return false;
    }

    rdcarray<const MTL::Buffer *> real;
    for(size_t i = 0; i < buffers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0 && buffers[i] == NULL) ||
         (bound[i] == 0 && buffers[i] != NULL) ||
         (buffers[i] == NULL && offsets[i] != 0) ||
         (buffers[i] != NULL && offsets[i] >= Unwrap(buffers[i])->length()))
      {
        RDCERR("Missing or invalid Metal vertex buffer resource at slot %llu",
               (uint64_t)(range.location + i));
        return false;
      }
      real.push_back(Unwrap(buffers[i]));
    }

    Unwrap(RenderCommandEncoder)->setVertexBuffers(real.data(), offsets.data(), range);
    for(size_t i = 0; i < buffers.size(); i++)
    {
      const NS::UInteger slot = range.location + i;
      if(slot < MAX_RENDER_PASS_BUFFER_ATTACHMENTS)
      {
        m_EncoderVertexBuffers[slot] = buffers[i];
        m_EncoderVertexOffsets[slot] = offsets[i];
      }
      m_Device->GetReplay()->BindVertexBuffer((uint32_t)slot, GetResID(buffers[i]), offsets[i]);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexBuffers(
    rdcarray<WrappedMTLBuffer *> buffers, rdcarray<NS::UInteger> offsets, NS::Range range)
{
  rdcarray<const MTL::Buffer *> real;
  for(WrappedMTLBuffer *buffer : buffers)
    real.push_back(Unwrap(buffer));
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexBuffers(real.data(), offsets.data(), range));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexBuffers);
      Serialise_setVertexBuffers(ser, buffers, offsets, range);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    for(WrappedMTLBuffer *buffer : buffers)
      if(buffer != NULL)
        record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexBindingWithStride(
    SerialiserType &ser, rdcarray<WrappedMTLBuffer *> buffers,
    rdcarray<NS::UInteger> offsets, rdcarray<NS::UInteger> strides,
    rdcarray<byte> data, NS::Range range, uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_ELEMENT(buffers).Important();
  SERIALISE_ELEMENT(offsets).Important();
  SERIALISE_ELEMENT(strides).Important();
  SERIALISE_ELEMENT(data).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(!RenderCommandEncoder || variant > 3 || range.location >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS ||
       !range.length || range.length > MAX_RENDER_PASS_BUFFER_ATTACHMENTS - range.location ||
       (variant != 1 && range.length != 1) || strides.size() != range.length ||
       ((variant < 2) && (buffers.size() != range.length || offsets.size() != range.length ||
                          !data.empty())) ||
       (variant == 2 && (!buffers.empty() || offsets.size() != 1 || !data.empty())) ||
       (variant == 3 && (!buffers.empty() || !offsets.empty() || data.empty() ||
                          data.size() > 4096)))
    {
      RDCERR("Invalid Metal vertex dynamic-stride binding shape");
      return false;
    }
    for(size_t i = 0; i < strides.size(); i++)
      if((strides[i] == 0 || strides[i] > 2048) &&
         !(variant == 1 && strides[i] == MTL::AttributeStrideStatic))
      {
        RDCERR("Invalid Metal vertex dynamic attribute stride");
        return false;
      }
    if(m_EncoderPipeline)
    {
      for(size_t i = 0; i < strides.size(); i++)
      {
        const uint64_t layout = m_Device->GetReplay()->GetVertexLayoutStride(
            GetResID(m_EncoderPipeline), (uint32_t)(range.location + i));
        const bool dynamic = layout == MTL::BufferLayoutStrideDynamic;
        if((strides[i] == MTL::AttributeStrideStatic && dynamic) ||
           (strides[i] != MTL::AttributeStrideStatic && !dynamic))
        {
          RDCERR("Metal vertex attribute stride does not match pipeline layout");
          return false;
        }
      }
    }
    for(size_t i = 0; i < buffers.size(); i++)
      if((buffers[i] && (buffers[i]->m_Type != eResBuffer || !buffers[i]->m_Real ||
                         offsets[i] >= Unwrap(buffers[i])->length())) ||
         (!buffers[i] && offsets[i] != 0))
      {
        RDCERR("Invalid Metal vertex dynamic-stride buffer or offset");
        return false;
      }
    if(variant == 2 && (!m_EncoderVertexBuffers[range.location] ||
                        offsets[0] >= Unwrap(m_EncoderVertexBuffers[range.location])->length()))
    {
      RDCERR("Invalid Metal vertex dynamic-stride offset without bound buffer");
      return false;
    }

    MTL::RenderCommandEncoder *real = Unwrap(RenderCommandEncoder);
    if(variant == 0)
      real->setVertexBuffer(Unwrap(buffers[0]), offsets[0], strides[0], range.location);
    else if(variant == 1)
    {
      rdcarray<const MTL::Buffer *> realBuffers;
      for(WrappedMTLBuffer *buffer : buffers) realBuffers.push_back(Unwrap(buffer));
      real->setVertexBuffers(realBuffers.data(), offsets.data(), strides.data(), range);
    }
    else if(variant == 2)
      real->setVertexBufferOffset(offsets[0], strides[0], range.location);
    else
      real->setVertexBytes(data.data(), data.size(), strides[0], range.location);

    for(size_t i = 0; i < range.length; i++)
    {
      const uint32_t slot = (uint32_t)(range.location + i);
      if(variant < 2)
      {
        m_EncoderVertexBuffers[slot] = buffers[i];
        m_EncoderVertexOffsets[slot] = offsets[i];
        m_Device->GetReplay()->BindVertexBuffer(slot, GetResID(buffers[i]), offsets[i]);
      }
      else if(variant == 2)
      {
        m_EncoderVertexOffsets[slot] = offsets[0];
        m_Device->GetReplay()->SetVertexBufferOffset(slot, offsets[0]);
      }
      else
      {
        m_EncoderVertexBuffers[slot] = NULL;
        m_EncoderVertexOffsets[slot] = 0;
        m_Device->GetReplay()->BindVertexBuffer(slot, ResourceId(), 0);
      }
      if(strides[i] != MTL::AttributeStrideStatic)
        m_Device->GetReplay()->SetVertexBufferStride(slot, (uint32_t)strides[i]);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexBufferWithStride(
    WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger stride, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexBuffer(Unwrap(buffer), offset, stride, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexBuffer_stride);
    Serialise_setVertexBindingWithStride(ser, {buffer}, {offset}, {stride}, {},
                                         NS::Range::Make(index, 1), 0);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(buffer) record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
  }
}

void WrappedMTLRenderCommandEncoder::setVertexBuffersWithStrides(
    rdcarray<WrappedMTLBuffer *> buffers, rdcarray<NS::UInteger> offsets,
    rdcarray<NS::UInteger> strides, NS::Range range)
{
  rdcarray<const MTL::Buffer *> realBuffers;
  for(WrappedMTLBuffer *buffer : buffers) realBuffers.push_back(Unwrap(buffer));
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexBuffers(realBuffers.data(), offsets.data(),
                                                     strides.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexBuffers_strides);
    Serialise_setVertexBindingWithStride(ser, buffers, offsets, strides, {}, range, 1);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLBuffer *buffer : buffers)
      if(buffer) record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
  }
}

void WrappedMTLRenderCommandEncoder::setVertexBufferOffsetWithStride(
    NS::UInteger offset, NS::UInteger stride, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexBufferOffset(offset, stride, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexBufferOffset_stride);
    Serialise_setVertexBindingWithStride(ser, {}, {offset}, {stride}, {},
                                         NS::Range::Make(index, 1), 2);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

void WrappedMTLRenderCommandEncoder::setVertexBytesWithStride(
    rdcarray<byte> data, NS::UInteger stride, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexBytes(data.data(), data.size(), stride, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexBytes_stride);
    Serialise_setVertexBindingWithStride(ser, {}, {}, {stride}, data,
                                         NS::Range::Make(index, 1), 3);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_setVertexBindingWithStride(
    ReadSerialiser &ser, rdcarray<WrappedMTLBuffer *>, rdcarray<NS::UInteger>,
    rdcarray<NS::UInteger>, rdcarray<byte>, NS::Range, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_setVertexBindingWithStride(
    WriteSerialiser &ser, rdcarray<WrappedMTLBuffer *>, rdcarray<NS::UInteger>,
    rdcarray<NS::UInteger>, rdcarray<byte>, NS::Range, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexTexture(SerialiserType &ser,
                                                                WrappedMTLTexture *texture,
                                                                NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(index >= 128 || texture == NULL || RenderCommandEncoder == NULL)
    {
      RDCERR("Cannot replay Metal vertex texture slot %llu with a null resource or encoder",
             (uint64_t)index);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setVertexTexture(Unwrap(texture), index);
    m_Device->GetReplay()->BindVertexTexture((uint32_t)index, GetResID(texture));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexTexture(WrappedMTLTexture *texture,
                                                      NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexTexture(Unwrap(texture), index));
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexTexture);
      Serialise_setVertexTexture(ser, texture, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    record->MarkResourceFrameReferenced(GetResID(texture), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexSamplerState(
    SerialiserType &ser, WrappedMTLSamplerState *sampler, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(sampler).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(index >= 128 || sampler == NULL || RenderCommandEncoder == NULL)
    {
      RDCERR("Cannot replay Metal vertex sampler slot %llu with a null resource or encoder",
             (uint64_t)index);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setVertexSamplerState(Unwrap(sampler), index);
    m_Device->GetReplay()->BindVertexSampler((uint32_t)index, GetResID(sampler));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexSamplerState(WrappedMTLSamplerState *sampler,
                                                           NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexSamplerState(Unwrap(sampler), index));
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexSamplerState);
      Serialise_setVertexSamplerState(ser, sampler, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexTextures(
    SerialiserType &ser, rdcarray<WrappedMTLTexture *> textures, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (range.location >= 128 || range.length > 128 - range.location))
  {
    RDCERR("Invalid Metal vertex texture batch range %llu+%llu",
           (uint64_t)range.location, (uint64_t)range.length);
    return false;
  }

  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLTexture *resource : textures)
      bound.push_back(resource != NULL ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(textures).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || textures.size() != range.length ||
       bound.size() != range.length)
    {
      RDCERR("Invalid Metal vertex texture batch resource count");
      return false;
    }
    rdcarray<const MTL::Texture *> real;
    for(size_t i = 0; i < textures.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0 && textures[i] == NULL) ||
         (bound[i] == 0 && textures[i] != NULL))
      {
        RDCERR("Missing or invalid Metal vertex texture resource at slot %llu",
               (uint64_t)(range.location + i));
        return false;
      }
      real.push_back(Unwrap(textures[i]));
    }
    Unwrap(RenderCommandEncoder)->setVertexTextures(real.data(), range);
    for(size_t i = 0; i < textures.size(); i++)
      m_Device->GetReplay()->BindVertexTexture((uint32_t)(range.location + i),
                                    GetResID(textures[i]));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexTextures(rdcarray<WrappedMTLTexture *> textures,
                                               NS::Range range)
{
  rdcarray<const MTL::Texture *> real;
  for(WrappedMTLTexture *resource : textures)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexTextures(real.data(), range));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexTextures);
      Serialise_setVertexTextures(ser, textures, range);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    for(WrappedMTLTexture *resource : textures)
      if(resource != NULL)
        record->MarkResourceFrameReferenced(GetResID(resource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexSamplerStates(
    SerialiserType &ser, rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (range.location >= 128 || range.length > 128 - range.location))
  {
    RDCERR("Invalid Metal vertex sampler batch range %llu+%llu",
           (uint64_t)range.location, (uint64_t)range.length);
    return false;
  }

  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLSamplerState *resource : samplers)
      bound.push_back(resource != NULL ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(samplers).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || samplers.size() != range.length ||
       bound.size() != range.length)
    {
      RDCERR("Invalid Metal vertex sampler batch resource count");
      return false;
    }
    rdcarray<const MTL::SamplerState *> real;
    for(size_t i = 0; i < samplers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0 && samplers[i] == NULL) ||
         (bound[i] == 0 && samplers[i] != NULL))
      {
        RDCERR("Missing or invalid Metal vertex sampler resource at slot %llu",
               (uint64_t)(range.location + i));
        return false;
      }
      real.push_back(Unwrap(samplers[i]));
    }
    Unwrap(RenderCommandEncoder)->setVertexSamplerStates(real.data(), range);
    for(size_t i = 0; i < samplers.size(); i++)
      m_Device->GetReplay()->BindVertexSampler((uint32_t)(range.location + i),
                                    GetResID(samplers[i]));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexSamplerStates(rdcarray<WrappedMTLSamplerState *> samplers,
                                               NS::Range range)
{
  rdcarray<const MTL::SamplerState *> real;
  for(WrappedMTLSamplerState *resource : samplers)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexSamplerStates(real.data(), range));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexSamplerStates);
      Serialise_setVertexSamplerStates(ser, samplers, range);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    for(WrappedMTLSamplerState *resource : samplers)
      if(resource != NULL)
        record->MarkResourceFrameReferenced(GetResID(resource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentTextures(
    SerialiserType &ser, rdcarray<WrappedMTLTexture *> textures, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (range.location >= 128 || range.length > 128 - range.location))
  {
    RDCERR("Invalid Metal fragment texture batch range %llu+%llu",
           (uint64_t)range.location, (uint64_t)range.length);
    return false;
  }

  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLTexture *resource : textures)
      bound.push_back(resource != NULL ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(textures).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || textures.size() != range.length ||
       bound.size() != range.length)
    {
      RDCERR("Invalid Metal fragment texture batch resource count");
      return false;
    }
    rdcarray<const MTL::Texture *> real;
    for(size_t i = 0; i < textures.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0 && textures[i] == NULL) ||
         (bound[i] == 0 && textures[i] != NULL))
      {
        RDCERR("Missing or invalid Metal fragment texture resource at slot %llu",
               (uint64_t)(range.location + i));
        return false;
      }
      real.push_back(Unwrap(textures[i]));
    }
    Unwrap(RenderCommandEncoder)->setFragmentTextures(real.data(), range);
    for(size_t i = 0; i < textures.size(); i++)
      m_Device->GetReplay()->BindFragmentTexture((uint32_t)(range.location + i),
                                    GetResID(textures[i]));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentTextures(rdcarray<WrappedMTLTexture *> textures,
                                               NS::Range range)
{
  rdcarray<const MTL::Texture *> real;
  for(WrappedMTLTexture *resource : textures)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentTextures(real.data(), range));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentTextures);
      Serialise_setFragmentTextures(ser, textures, range);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    for(WrappedMTLTexture *resource : textures)
      if(resource != NULL)
        record->MarkResourceFrameReferenced(GetResID(resource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentSamplerStates(
    SerialiserType &ser, rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (range.location >= 128 || range.length > 128 - range.location))
  {
    RDCERR("Invalid Metal fragment sampler batch range %llu+%llu",
           (uint64_t)range.location, (uint64_t)range.length);
    return false;
  }

  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLSamplerState *resource : samplers)
      bound.push_back(resource != NULL ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(samplers).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || samplers.size() != range.length ||
       bound.size() != range.length)
    {
      RDCERR("Invalid Metal fragment sampler batch resource count");
      return false;
    }
    rdcarray<const MTL::SamplerState *> real;
    for(size_t i = 0; i < samplers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0 && samplers[i] == NULL) ||
         (bound[i] == 0 && samplers[i] != NULL))
      {
        RDCERR("Missing or invalid Metal fragment sampler resource at slot %llu",
               (uint64_t)(range.location + i));
        return false;
      }
      real.push_back(Unwrap(samplers[i]));
    }
    Unwrap(RenderCommandEncoder)->setFragmentSamplerStates(real.data(), range);
    for(size_t i = 0; i < samplers.size(); i++)
      m_Device->GetReplay()->BindFragmentSampler((uint32_t)(range.location + i),
                                    GetResID(samplers[i]));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentSamplerStates(rdcarray<WrappedMTLSamplerState *> samplers,
                                               NS::Range range)
{
  rdcarray<const MTL::SamplerState *> real;
  for(WrappedMTLSamplerState *resource : samplers)
    real.push_back(Unwrap(resource));
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentSamplerStates(real.data(), range));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerStates);
      Serialise_setFragmentSamplerStates(ser, samplers, range);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    for(WrappedMTLSamplerState *resource : samplers)
      if(resource != NULL)
        record->MarkResourceFrameReferenced(GetResID(resource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentBytes(SerialiserType &ser,
                                                                rdcarray<byte> data,
                                                                NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(data).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || index >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS ||
       data.size() > 4096)
    {
      RDCERR("Invalid Metal inline fragment bytes binding at slot %llu with %llu bytes",
             (uint64_t)index, (uint64_t)data.size());
      return false;
    }
    m_Device->GetReplay()->BindGraphicsBytes(2, (uint32_t)index, data, GetResID(RenderCommandEncoder));
    if(!m_Device->RelocateGraphicsDescriptorBytes(GetResID(RenderCommandEncoder), 2, index, data))
      return false;
    Unwrap(RenderCommandEncoder)->setFragmentBytes(data.data(), data.size(), index);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentBytes(rdcarray<byte> data, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentBytes(data.data(), data.size(), index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentBytes);
      Serialise_setFragmentBytes(ser, data, index);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentBuffer(SerialiserType &ser,
                                                                 WrappedMTLBuffer *buffer,
                                                                 NS::UInteger offset,
                                                                 NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) || index >= 31 ||
       (!buffer && offset != 0) || (buffer &&
        (buffer->m_Type != eResBuffer || !buffer->m_Real || offset >= Unwrap(buffer)->length() || offset % 4)))
    {
      RDCERR("Invalid Metal fragment buffer, encoder, index or range");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setFragmentBuffer(Unwrap(buffer), offset, index);
    m_Device->GetReplay()->BindFragmentBuffer((uint32_t)index, GetResID(buffer), (uint64_t)offset);
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentBufferOffset(SerialiserType &ser,
                                                                       NS::UInteger offset,
                                                                       NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !m_Device->GetReplay()->IsFragmentBufferOffsetValid((uint32_t)index, offset) || index >= 31)
    {
      RDCERR("Invalid Metal fragment buffer offset or encoder");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setFragmentBufferOffset(offset, index);
    m_Device->GetReplay()->SetFragmentBufferOffset((uint32_t)index, (uint64_t)offset);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentBufferOffset(NS::UInteger offset,
                                                              NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentBufferOffset(offset, index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentBufferOffset);
      Serialise_setFragmentBufferOffset(ser, offset, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
  }
}

void WrappedMTLRenderCommandEncoder::setFragmentBuffer(WrappedMTLBuffer *buffer,
                                                       NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentBuffer(Unwrap(buffer), offset, index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentBuffer);
      Serialise_setFragmentBuffer(ser, buffer, offset, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentBuffers(
    SerialiserType &ser, rdcarray<WrappedMTLBuffer *> buffers,
    rdcarray<NS::UInteger> offsets, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     (range.location >= MAX_RENDER_PASS_BUFFER_ATTACHMENTS ||
      range.length > MAX_RENDER_PASS_BUFFER_ATTACHMENTS - range.location))
  {
    RDCERR("Invalid Metal fragment buffer batch range %llu+%llu", (uint64_t)range.location,
           (uint64_t)range.length);
    return false;
  }

  rdcarray<uint8_t> bound;
  if(ser.IsWriting())
    for(WrappedMTLBuffer *buffer : buffers)
      bound.push_back(buffer != NULL ? 1 : 0);
  SERIALISE_ELEMENT(bound).Important();
  SERIALISE_ELEMENT(buffers).Important();
  SERIALISE_ELEMENT(offsets).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || buffers.size() != range.length ||
       offsets.size() != range.length || bound.size() != range.length)
    {
      RDCERR("Invalid Metal fragment buffer batch resource count");
      return false;
    }

    rdcarray<const MTL::Buffer *> real;
    for(size_t i = 0; i < buffers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0 && buffers[i] == NULL) ||
         (bound[i] == 0 && buffers[i] != NULL) ||
         (buffers[i] == NULL && offsets[i] != 0) ||
         (buffers[i] != NULL && offsets[i] >= Unwrap(buffers[i])->length()))
      {
        RDCERR("Missing or invalid Metal fragment buffer resource at slot %llu",
               (uint64_t)(range.location + i));
        return false;
      }
      real.push_back(Unwrap(buffers[i]));
    }

    Unwrap(RenderCommandEncoder)->setFragmentBuffers(real.data(), offsets.data(), range);
    for(size_t i = 0; i < buffers.size(); i++)
      m_Device->GetReplay()->BindFragmentBuffer((uint32_t)(range.location + i),
                                                GetResID(buffers[i]), offsets[i]);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentBuffers(
    rdcarray<WrappedMTLBuffer *> buffers, rdcarray<NS::UInteger> offsets, NS::Range range)
{
  rdcarray<const MTL::Buffer *> real;
  for(WrappedMTLBuffer *buffer : buffers)
    real.push_back(Unwrap(buffer));
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentBuffers(real.data(), offsets.data(), range));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentBuffers);
      Serialise_setFragmentBuffers(ser, buffers, offsets, range);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(chunk);
    for(WrappedMTLBuffer *buffer : buffers)
      if(buffer != NULL)
        record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentTexture(SerialiserType &ser,
                                                                  WrappedMTLTexture *texture,
                                                                  NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    Unwrap(RenderCommandEncoder)->setFragmentTexture(Unwrap(texture), index);
    m_Device->GetReplay()->BindFragmentTexture((uint32_t)index, GetResID(texture));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentTexture(WrappedMTLTexture *texture, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentTexture(Unwrap(texture), index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentTexture);
      Serialise_setFragmentTexture(ser, texture, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(texture), eFrameRef_Read);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentSamplerState(
    SerialiserType &ser, WrappedMTLSamplerState *sampler, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(sampler).Important();
  SERIALISE_ELEMENT(index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    Unwrap(RenderCommandEncoder)->setFragmentSamplerState(Unwrap(sampler), index);
    m_Device->GetReplay()->BindFragmentSampler((uint32_t)index, GetResID(sampler));
  }

  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentSamplerState(WrappedMTLSamplerState *sampler,
                                                              NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentSamplerState(Unwrap(sampler), index));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerState);
      Serialise_setFragmentSamplerState(ser, sampler, index);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_useResource(SerialiserType &ser,
                                                           WrappedMTLResource *resource,
                                                           MTL::ResourceUsage usage)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(resource).Important();
  uint64_t usageValue = (uint64_t)usage;
  SERIALISE_ELEMENT(usageValue).Important();
  SERIALISE_CHECK_READ_ERRORS();

  usage = (MTL::ResourceUsage)usageValue;

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) ||
       !ValidMetalResidencyResource(m_Device, resource) ||
       !ValidMetalResourceUsage(usageValue))
    {
      RDCERR("Invalid Metal render resource declaration");
      return false;
    }
    Unwrap(RenderCommandEncoder)->useResource(Unwrap(resource), usage);
    if(IsLoading(m_State) && (usageValue & MTL::ResourceUsageWrite))
      m_Device->GetReplay()->NoteRenderIndirectWrite(
          RenderCommandEncoder->GetParallelParent()?GetResID(RenderCommandEncoder->GetParallelParent()):GetResID(RenderCommandEncoder),resource);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::useResource(WrappedMTLResource *resource,
                                                 MTL::ResourceUsage usage)
{
  if(usage & MTL::ResourceUsageWrite) CaptureIndirectWrite(resource);

  SERIALISE_TIME_CALL(Unwrap(this)->useResource(Unwrap(resource), usage));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_useResource);
    Serialise_useResource(ser, resource, usage);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(resource),
                                        (usage & MTL::ResourceUsageWrite) ? eFrameRef_ReadBeforeWrite
                                                                         : eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setViewport(SerialiserType &ser,
                                                           MTL::Viewport &viewport)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(viewport).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    Unwrap(RenderCommandEncoder)->setViewport(viewport);
    m_Device->GetReplay()->SetViewport(viewport);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setViewport(MTL::Viewport &viewport)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setViewport(viewport));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setViewport);
      Serialise_setViewport(ser, viewport);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setViewports(
    SerialiserType &ser, rdcarray<MTL::Viewport> viewports)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(viewports).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || viewports.empty() || viewports.size() > 16)
    {
      RDCERR("Invalid Metal viewport array count %llu", (uint64_t)viewports.size());
      return false;
    }
    Unwrap(RenderCommandEncoder)->setViewports(viewports.data(), viewports.size());
    m_Device->GetReplay()->SetViewport(viewports[0], (uint32_t)viewports.size());
    m_Device->GetReplay()->SetInspectionViewports(viewports);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setViewports(rdcarray<MTL::Viewport> viewports)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setViewports(viewports.data(), viewports.size()));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setViewports);
    Serialise_setViewports(ser, viewports);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setScissorRect(SerialiserType &ser,
                                                              MTL::ScissorRect &rect)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(rect).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    Unwrap(RenderCommandEncoder)->setScissorRect(rect);
    m_Device->GetReplay()->SetScissor(rect);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setScissorRect(MTL::ScissorRect &rect)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setScissorRect(rect));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setScissorRect);
      Serialise_setScissorRect(ser, rect);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setScissorRects(
    SerialiserType &ser, rdcarray<MTL::ScissorRect> scissors)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(scissors).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || scissors.empty() || scissors.size() > 16)
    {
      RDCERR("Invalid Metal scissor array count %llu", (uint64_t)scissors.size());
      return false;
    }
    Unwrap(RenderCommandEncoder)->setScissorRects(scissors.data(), scissors.size());
    m_Device->GetReplay()->SetScissor(scissors[0], (uint32_t)scissors.size());
    m_Device->GetReplay()->SetInspectionScissors(scissors);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setScissorRects(rdcarray<MTL::ScissorRect> scissors)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setScissorRects(scissors.data(), scissors.size()));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setScissorRects);
    Serialise_setScissorRects(ser, scissors);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFrontFacingWinding(SerialiserType &ser,
                                                                    MTL::Winding winding)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(winding).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    Unwrap(RenderCommandEncoder)->setFrontFacingWinding(winding);
    m_Device->GetReplay()->SetFrontFacingWinding(winding);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFrontFacingWinding(MTL::Winding winding)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFrontFacingWinding(winding));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFrontFacingWinding);
      Serialise_setFrontFacingWinding(ser, winding);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setCullMode(SerialiserType &ser,
                                                           MTL::CullMode cullMode)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(cullMode).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    Unwrap(RenderCommandEncoder)->setCullMode(cullMode);
    m_Device->GetReplay()->SetCullMode(cullMode);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setCullMode(MTL::CullMode cullMode)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setCullMode(cullMode));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setCullMode);
      Serialise_setCullMode(ser, cullMode);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setDepthClipMode(
    SerialiserType &ser, MTL::DepthClipMode depthClipMode)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(depthClipMode).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL ||
       (depthClipMode != MTL::DepthClipModeClip && depthClipMode != MTL::DepthClipModeClamp))
    {
      RDCERR("Invalid Metal depth clip mode %u", (uint32_t)depthClipMode);
      return false;
    }
    m_Device->GetReplay()->SetInspectionDepthClip(depthClipMode == MTL::DepthClipModeClip);
    Unwrap(RenderCommandEncoder)->setDepthClipMode(depthClipMode);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setDepthClipMode(MTL::DepthClipMode depthClipMode)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setDepthClipMode(depthClipMode));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setDepthClipMode);
    Serialise_setDepthClipMode(ser, depthClipMode);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setDepthBias(
    SerialiserType &ser, float depthBias, float slopeScale, float clamp)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(depthBias).Important();
  SERIALISE_ELEMENT(slopeScale).Important();
  SERIALISE_ELEMENT(clamp).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL)
      return false;
    m_Device->GetReplay()->SetInspectionDepthBias(depthBias, slopeScale, clamp);
    Unwrap(RenderCommandEncoder)->setDepthBias(depthBias, slopeScale, clamp);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setDepthBias(float depthBias, float slopeScale, float clamp)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setDepthBias(depthBias, slopeScale, clamp));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setDepthBias);
    Serialise_setDepthBias(ser, depthBias, slopeScale, clamp);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setTriangleFillMode(
    SerialiserType &ser, MTL::TriangleFillMode fillMode)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(fillMode).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL ||
       (fillMode != MTL::TriangleFillModeFill && fillMode != MTL::TriangleFillModeLines))
    {
      RDCERR("Invalid Metal triangle fill mode %u", (uint32_t)fillMode);
      return false;
    }
    m_Device->GetReplay()->SetInspectionFillMode(fillMode == MTL::TriangleFillModeFill ? FillMode::Solid : FillMode::Wireframe);
    Unwrap(RenderCommandEncoder)->setTriangleFillMode(fillMode);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setTriangleFillMode(MTL::TriangleFillMode fillMode)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTriangleFillMode(fillMode));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setTriangleFillMode);
    Serialise_setTriangleFillMode(ser, fillMode);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setBlendColor(
    SerialiserType &ser, float red, float green, float blue, float alpha)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(red).Important();
  SERIALISE_ELEMENT(green).Important();
  SERIALISE_ELEMENT(blue).Important();
  SERIALISE_ELEMENT(alpha).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL)
      return false;
    m_Device->GetReplay()->SetInspectionBlendFactor(red, green, blue, alpha);
    Unwrap(RenderCommandEncoder)->setBlendColor(red, green, blue, alpha);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setBlendColor(float red, float green, float blue, float alpha)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setBlendColor(red, green, blue, alpha));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setBlendColor);
    Serialise_setBlendColor(ser, red, green, blue, alpha);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setDepthStencilState(
    SerialiserType &ser, WrappedMTLDepthStencilState *depthStencilState)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(depthStencilState).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    Unwrap(RenderCommandEncoder)->setDepthStencilState(Unwrap(depthStencilState));
    m_Device->GetReplay()->BindDepthStencilState(GetResID(depthStencilState));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setDepthStencilState(
    WrappedMTLDepthStencilState *depthStencilState)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setDepthStencilState(Unwrap(depthStencilState)));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setDepthStencilState);
      Serialise_setDepthStencilState(ser, depthStencilState);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(depthStencilState), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setStencilReferenceValue(
    SerialiserType &ser, uint32_t referenceValue)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(referenceValue).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    Unwrap(RenderCommandEncoder)->setStencilReferenceValue(referenceValue);
    m_Device->GetReplay()->SetStencilReferenceValue(referenceValue);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setStencilReferenceValue(uint32_t referenceValue)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setStencilReferenceValue(referenceValue));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setStencilReferenceValue);
      Serialise_setStencilReferenceValue(ser, referenceValue);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setStencilReferenceValues(
    SerialiserType &ser, uint32_t frontReferenceValue, uint32_t backReferenceValue)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(frontReferenceValue).Important();
  SERIALISE_ELEMENT(backReferenceValue).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    Unwrap(RenderCommandEncoder)
        ->setStencilReferenceValues(frontReferenceValue, backReferenceValue);
    m_Device->GetReplay()->SetStencilReferenceValues(frontReferenceValue, backReferenceValue);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setStencilReferenceValues(uint32_t frontReferenceValue,
                                                                uint32_t backReferenceValue)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setStencilReferenceValues(frontReferenceValue,
                                                               backReferenceValue));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setStencilFrontReferenceValue);
      Serialise_setStencilReferenceValues(ser, frontReferenceValue, backReferenceValue);
      chunk = scope.Get();
    }
    GetRecord(m_CommandBuffer)->AddChunk(chunk);
  }
}

static bool ValidStoreAction(MTL::StoreAction action)
{
  return action >= MTL::StoreActionDontCare &&
         action <= MTL::StoreActionCustomSampleDepthStore && action != MTL::StoreActionUnknown;
}

static bool ValidStoreActionOptions(MTL::StoreActionOptions options)
{
  return ((uint64_t)options & ~(uint64_t)MTL::StoreActionOptionValidMask) == 0;
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVisibilityResultMode(
    SerialiserType &ser, MTL::VisibilityResultMode mode, NS::UInteger offset)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  uint64_t modeValue = (uint64_t)mode;
  SERIALISE_ELEMENT(modeValue).Named("mode"_lit).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  mode = (MTL::VisibilityResultMode)modeValue;
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    MTL::Buffer *buffer =
        Unwrap(m_Device->GetReplay()->GetRenderPassDescriptor().visibilityResultBuffer);
    if(RenderCommandEncoder == NULL || modeValue > MTL::VisibilityResultModeCounting ||
       (offset & 7) || (mode != MTL::VisibilityResultModeDisabled &&
       (!buffer || offset > buffer->length() || sizeof(uint64_t) > buffer->length() - offset)))
    {
      RDCERR("Invalid Metal visibility result mode %llu or offset %llu", (uint64_t)mode,
             (uint64_t)offset);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setVisibilityResultMode(mode, offset);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVisibilityResultMode(MTL::VisibilityResultMode mode,
                                                              NS::UInteger offset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVisibilityResultMode(mode, offset));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVisibilityResultMode);
    Serialise_setVisibilityResultMode(ser, mode, offset);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setColorStoreAction(
    SerialiserType &ser, MTL::StoreAction storeAction, NS::UInteger colorAttachmentIndex)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  uint64_t storeActionValue = (uint64_t)storeAction;
  SERIALISE_ELEMENT(storeActionValue).Named("storeAction"_lit).Important();
  SERIALISE_ELEMENT(colorAttachmentIndex).Important();
  SERIALISE_CHECK_READ_ERRORS();
  storeAction = (MTL::StoreAction)storeActionValue;
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || !ValidStoreAction(storeAction) || colorAttachmentIndex >= 8)
    {
      RDCERR("Invalid Metal color store action %llu or attachment %llu", (uint64_t)storeAction,
             (uint64_t)colorAttachmentIndex);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setColorStoreAction(storeAction, colorAttachmentIndex);
    RenderCommandEncoder->ClearDeferredStoreAction((uint32_t)colorAttachmentIndex);
    m_Device->GetReplay()->SetRenderPassStoreAction((uint32_t)colorAttachmentIndex, storeAction);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setColorStoreAction(MTL::StoreAction storeAction,
                                                          NS::UInteger colorAttachmentIndex)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setColorStoreAction(storeAction, colorAttachmentIndex));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setColorStoreAction);
    Serialise_setColorStoreAction(ser, storeAction, colorAttachmentIndex);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setDepthStoreAction(SerialiserType &ser,
                                                                   MTL::StoreAction storeAction)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  uint64_t storeActionValue = (uint64_t)storeAction;
  SERIALISE_ELEMENT(storeActionValue).Named("storeAction"_lit).Important();
  SERIALISE_CHECK_READ_ERRORS();
  storeAction = (MTL::StoreAction)storeActionValue;
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || !ValidStoreAction(storeAction))
    {
      RDCERR("Invalid Metal depth store action %llu", (uint64_t)storeAction);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setDepthStoreAction(storeAction);
    RenderCommandEncoder->ClearDeferredStoreAction(8);
    m_Device->GetReplay()->SetRenderPassStoreAction(8, storeAction);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setDepthStoreAction(MTL::StoreAction storeAction)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setDepthStoreAction(storeAction));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setDepthStoreAction);
    Serialise_setDepthStoreAction(ser, storeAction);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setStencilStoreAction(SerialiserType &ser,
                                                                     MTL::StoreAction storeAction)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  uint64_t storeActionValue = (uint64_t)storeAction;
  SERIALISE_ELEMENT(storeActionValue).Named("storeAction"_lit).Important();
  SERIALISE_CHECK_READ_ERRORS();
  storeAction = (MTL::StoreAction)storeActionValue;
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || !ValidStoreAction(storeAction))
    {
      RDCERR("Invalid Metal stencil store action %llu", (uint64_t)storeAction);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setStencilStoreAction(storeAction);
    RenderCommandEncoder->ClearDeferredStoreAction(9);
    m_Device->GetReplay()->SetRenderPassStoreAction(9, storeAction);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setStencilStoreAction(MTL::StoreAction storeAction)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setStencilStoreAction(storeAction));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setStencilStoreAction);
    Serialise_setStencilStoreAction(ser, storeAction);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setColorStoreActionOptions(
    SerialiserType &ser, MTL::StoreActionOptions storeActionOptions,
    NS::UInteger colorAttachmentIndex)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  uint64_t storeActionOptionsValue = (uint64_t)storeActionOptions;
  SERIALISE_ELEMENT(storeActionOptionsValue).Named("storeActionOptions"_lit).Important();
  SERIALISE_ELEMENT(colorAttachmentIndex).Important();
  SERIALISE_CHECK_READ_ERRORS();
  storeActionOptions = (MTL::StoreActionOptions)storeActionOptionsValue;
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || !ValidStoreActionOptions(storeActionOptions) ||
       colorAttachmentIndex >= 8)
    {
      RDCERR("Invalid Metal color store options %llu or attachment %llu",
             (uint64_t)storeActionOptions, (uint64_t)colorAttachmentIndex);
      return false;
    }
    Unwrap(RenderCommandEncoder)
        ->setColorStoreActionOptions(storeActionOptions, colorAttachmentIndex);
    m_Device->GetReplay()->SetRenderPassStoreOptions((uint32_t)colorAttachmentIndex,
                                                    storeActionOptions);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setColorStoreActionOptions(
    MTL::StoreActionOptions storeActionOptions, NS::UInteger colorAttachmentIndex)
{
  SERIALISE_TIME_CALL(
      Unwrap(this)->setColorStoreActionOptions(storeActionOptions, colorAttachmentIndex));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setColorStoreActionOptions);
    Serialise_setColorStoreActionOptions(ser, storeActionOptions, colorAttachmentIndex);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setDepthStoreActionOptions(
    SerialiserType &ser, MTL::StoreActionOptions storeActionOptions)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  uint64_t storeActionOptionsValue = (uint64_t)storeActionOptions;
  SERIALISE_ELEMENT(storeActionOptionsValue).Named("storeActionOptions"_lit).Important();
  SERIALISE_CHECK_READ_ERRORS();
  storeActionOptions = (MTL::StoreActionOptions)storeActionOptionsValue;
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || !ValidStoreActionOptions(storeActionOptions))
    {
      RDCERR("Invalid Metal depth store options %llu", (uint64_t)storeActionOptions);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setDepthStoreActionOptions(storeActionOptions);
    m_Device->GetReplay()->SetRenderPassStoreOptions(8, storeActionOptions);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setDepthStoreActionOptions(
    MTL::StoreActionOptions storeActionOptions)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setDepthStoreActionOptions(storeActionOptions));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setDepthStoreActionOptions);
    Serialise_setDepthStoreActionOptions(ser, storeActionOptions);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setStencilStoreActionOptions(
    SerialiserType &ser, MTL::StoreActionOptions storeActionOptions)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  uint64_t storeActionOptionsValue = (uint64_t)storeActionOptions;
  SERIALISE_ELEMENT(storeActionOptionsValue).Named("storeActionOptions"_lit).Important();
  SERIALISE_CHECK_READ_ERRORS();
  storeActionOptions = (MTL::StoreActionOptions)storeActionOptionsValue;
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(RenderCommandEncoder == NULL || !ValidStoreActionOptions(storeActionOptions))
    {
      RDCERR("Invalid Metal stencil store options %llu", (uint64_t)storeActionOptions);
      return false;
    }
    Unwrap(RenderCommandEncoder)->setStencilStoreActionOptions(storeActionOptions);
    m_Device->GetReplay()->SetRenderPassStoreOptions(9, storeActionOptions);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setStencilStoreActionOptions(
    MTL::StoreActionOptions storeActionOptions)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setStencilStoreActionOptions(storeActionOptions));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setStencilStoreActionOptions);
    Serialise_setStencilStoreActionOptions(ser, storeActionOptions);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_textureBarrier(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       RenderCommandEncoder->HasGPUWork())
    {
      RDCERR("Unsupported Metal texture barrier after render GPU work");
      return false;
    }
    // Before the first draw/dispatch there are no same-pass texture writes to order. Metal
    // Validation rejects this deprecated API on the current device, so no native call is needed.
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::textureBarrier()
{
  SERIALISE_TIME_CALL(Unwrap(this)->textureBarrier());
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_textureBarrier);
    Serialise_textureBarrier(ser);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitiveType, NS::UInteger vertexStart,
    NS::UInteger vertexCount, NS::UInteger instanceCount, NS::UInteger baseInstance)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(primitiveType);
  SERIALISE_ELEMENT(vertexStart);
  SERIALISE_ELEMENT(vertexCount).Important();
  SERIALISE_ELEMENT(instanceCount);
  SERIALISE_ELEMENT(baseInstance);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    const char *primitiveName = NULL;
    switch(primitiveType)
    {
      case MTL::PrimitiveTypePoint: primitiveName = "Point"; break;
      case MTL::PrimitiveTypeLine: primitiveName = "Line"; break;
      case MTL::PrimitiveTypeLineStrip: primitiveName = "Line Strip"; break;
      case MTL::PrimitiveTypeTriangle:
      case MTL::PrimitiveTypeTriangleStrip: break;
      default:
        RDCERR("Invalid Metal drawPrimitives primitive type %llu", (uint64_t)primitiveType);
        return false;
    }
    if(vertexStart > UINT32_MAX || vertexCount == 0 || vertexCount > UINT32_MAX ||
       instanceCount == 0 || instanceCount > UINT32_MAX || baseInstance > UINT32_MAX)
    {
      RDCERR("Invalid Metal drawPrimitives vertex/instance count or 32-bit action range");
      return false;
    }
    if(!m_Device->GetReplay()->ValidateArgumentBufferBindings())
      return false;
    m_Device->GetReplay()->SetPrimitiveTopology(primitiveType);
    m_Device->GetReplay()->SetIndirectBuffer(ResourceId(), 0, 0);
    Unwrap(RenderCommandEncoder)
        ->drawPrimitives(primitiveType, vertexStart, vertexCount, instanceCount, baseInstance);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = primitiveName
                              ? StringFormat::Fmt("drawPrimitives(%s, %llu)", primitiveName,
                                                  (uint64_t)vertexCount)
                              : StringFormat::Fmt("drawPrimitives(%llu)", (uint64_t)vertexCount);
      action.flags = ActionFlags::Drawcall;
      if(instanceCount > 1 || baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = (uint32_t)vertexCount;
      action.numInstances = (uint32_t)instanceCount;
      action.vertexOffset = (uint32_t)vertexStart;
      action.instanceOffset = (uint32_t)baseInstance;
      m_Device->GetReplay()->SetActionOutputs(action);
      AddAction(action);
    }
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitiveType, WrappedMTLBuffer *indirectBuffer,
    NS::UInteger indirectBufferOffset)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(primitiveType);
  SERIALISE_ELEMENT(indirectBuffer).Important();
  SERIALISE_ELEMENT(indirectBufferOffset).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    static const uint64_t IndirectArgumentSize = sizeof(uint32_t) * 4;
    MTL::Buffer *realBuffer = indirectBuffer && indirectBuffer->m_Type == eResBuffer
                                  ? Unwrap(indirectBuffer) : NULL;
    if((primitiveType != MTL::PrimitiveTypePoint &&
        primitiveType != MTL::PrimitiveTypeLine &&
        primitiveType != MTL::PrimitiveTypeLineStrip &&
        primitiveType != MTL::PrimitiveTypeTriangle &&
        primitiveType != MTL::PrimitiveTypeTriangleStrip) ||
       realBuffer == NULL || (indirectBufferOffset & 3) != 0 ||
       indirectBufferOffset > realBuffer->length() ||
       IndirectArgumentSize > realBuffer->length() - indirectBufferOffset)
    {
      RDCERR("Invalid Metal drawPrimitives indirect argument buffer or offset");
      return false;
    }

    // Metal reads indirect arguments at GPU execution time. Private buffers cannot be read
    // with contents(); preserve the GPU draw and report unknown counts in the action tree.
    const bool privateArguments = realBuffer->storageMode() == MTL::StorageModePrivate;
    if(!privateArguments && realBuffer->contents() == NULL)
    {
      RDCERR("Metal drawPrimitives indirect arguments are not CPU accessible");
      return false;
    }

    uint32_t vertexCount = 0, instanceCount = 0, vertexStart = 0, baseInstance = 0;
    if(!privateArguments)
    {
      const uint32_t *arguments = (const uint32_t *)((const byte *)realBuffer->contents() +
                                                     indirectBufferOffset);
      vertexCount = arguments[0];
      instanceCount = arguments[1];
      vertexStart = arguments[2];
      baseInstance = arguments[3];
    }

    MetalReplay *replay = m_Device->GetReplay();
    if(!m_Device->GetReplay()->ValidateArgumentBufferBindings())
      return false;
    replay->SetPrimitiveTopology(primitiveType);
    replay->SetIndirectBuffer(GetResID(indirectBuffer), indirectBufferOffset,
                              IndirectArgumentSize);
    if(IsLoading(m_State) && !replay->RegisterRenderIndirectAction(replay->GetNextEventID(),
        RenderCommandEncoder,GetResID(indirectBuffer),indirectBufferOffset,4))return false;
    Unwrap(RenderCommandEncoder)
        ->drawPrimitives(primitiveType, realBuffer, indirectBufferOffset);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = privateArguments
                              ? "drawPrimitives(indirect, GPU-defined arguments)"
                              : StringFormat::Fmt("drawPrimitives(indirect, %u vertices, %u instances)",
                                                  vertexCount, instanceCount);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indirect;
      if(instanceCount > 1 || baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = vertexCount;
      action.numInstances = instanceCount;
      action.vertexOffset = vertexStart;
      action.instanceOffset = baseInstance;
      replay->SetActionOutputs(action);
      AddAction(action);
      replay->AddUsage(GetResID(indirectBuffer), ResourceUsage::Indirect);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType,
                                                    WrappedMTLBuffer *indirectBuffer,
                                                    NS::UInteger indirectBufferOffset)
{
  SCOPED_READLOCK(m_Device->GetCaptureTransitionLock());
  CaptureIndirectArguments(indirectBuffer,indirectBufferOffset,4);

  SERIALISE_TIME_CALL(Unwrap(this)->drawPrimitives(primitiveType, Unwrap(indirectBuffer),
                                                   indirectBufferOffset));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawPrimitives_indirect);
    Serialise_drawPrimitives(ser, primitiveType, indirectBuffer, indirectBufferOffset);
    MetalResourceRecord *commandBufferRecord = GetRecord(m_CommandBuffer);
    commandBufferRecord->AddChunk(scope.Get());
    commandBufferRecord->MarkResourceFrameReferenced(GetResID(indirectBuffer), eFrameRef_Read);
  }
}

void WrappedMTLRenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType,
                                                    NS::UInteger vertexStart,
                                                    NS::UInteger vertexCount,
                                                    NS::UInteger instanceCount,
                                                    NS::UInteger baseInstance)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawPrimitives(primitiveType, vertexStart, vertexCount,
                                                   instanceCount, baseInstance));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawPrimitives_instanced);
      Serialise_drawPrimitives(ser, primitiveType, vertexStart, vertexCount, instanceCount,
                               baseInstance);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawIndexedPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitiveType, NS::UInteger indexCount,
    MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(primitiveType);
  SERIALISE_ELEMENT(indexCount).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexBuffer).Important();
  SERIALISE_ELEMENT(indexBufferOffset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(!m_Device->GetReplay()->ValidateArgumentBufferBindings())
      return false;
    m_Device->GetReplay()->SetPrimitiveTopology(primitiveType);
    m_Device->GetReplay()->SetIndirectBuffer(ResourceId(), 0, 0);
    m_Device->GetReplay()->BindIndexBuffer(GetResID(indexBuffer), indexBufferOffset, indexType);
    Unwrap(RenderCommandEncoder)
        ->drawIndexedPrimitives(primitiveType, indexCount, indexType, Unwrap(indexBuffer),
                                indexBufferOffset);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawIndexedPrimitives(%llu)", (uint64_t)indexCount);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indexed;
      action.numIndices = (uint32_t)indexCount;
      action.numInstances = 1;
      action.indexOffset = (uint32_t)(indexBufferOffset /
                                      (indexType == MTL::IndexTypeUInt16 ? 2 : 4));
      m_Device->GetReplay()->SetActionOutputs(action);
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawIndexedPrimitives(
    MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType,
    WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawIndexedPrimitives(
      primitiveType, indexCount, indexType, Unwrap(indexBuffer), indexBufferOffset));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives);
      Serialise_drawIndexedPrimitives(ser, primitiveType, indexCount, indexType, indexBuffer,
                                      indexBufferOffset);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(indexBuffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawIndexedPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitiveType, NS::UInteger indexCount,
    MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset,
    NS::UInteger instanceCount, NS::Integer baseVertex, NS::UInteger baseInstance)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(primitiveType);
  SERIALISE_ELEMENT(indexCount).Important();
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexBuffer).Important();
  SERIALISE_ELEMENT(indexBufferOffset).Important();
  SERIALISE_ELEMENT(instanceCount).Important();
  if(ser.ChunkMetadata().chunkID !=
     (uint32_t)MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced)
  {
    SERIALISE_ELEMENT(baseVertex).Important();
    SERIALISE_ELEMENT(baseInstance).Important();
  }
  else
  {
    baseVertex = 0;
    baseInstance = 0;
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    MTL::Buffer *realBuffer = indexBuffer && indexBuffer->m_Type == eResBuffer ? Unwrap(indexBuffer) : NULL;
    const uint64_t indexStride = indexType == MTL::IndexTypeUInt16 ? 2 : 4;
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       primitiveType > MTL::PrimitiveTypeTriangleStrip || realBuffer == NULL ||
       (indexType != MTL::IndexTypeUInt16 && indexType != MTL::IndexTypeUInt32) ||
       indexBufferOffset % indexStride != 0 || indexBufferOffset > realBuffer->length() ||
       indexCount > (realBuffer->length() - indexBufferOffset) / indexStride ||
       indexCount > UINT32_MAX || instanceCount > UINT32_MAX || baseInstance > UINT32_MAX ||
       baseVertex < INT32_MIN || baseVertex > INT32_MAX)
    {
      RDCERR("Invalid Metal indexed instancing buffer range or draw arguments");
      return false;
    }

    MetalReplay *replay = m_Device->GetReplay();
    if(!m_Device->GetReplay()->ValidateArgumentBufferBindings())
      return false;
    replay->SetPrimitiveTopology(primitiveType);
    replay->SetIndirectBuffer(ResourceId(), 0, 0);
    replay->BindIndexBuffer(GetResID(indexBuffer), indexBufferOffset, indexType, indexCount);
    Unwrap(RenderCommandEncoder)
        ->drawIndexedPrimitives(primitiveType, indexCount, indexType, realBuffer,
                                indexBufferOffset, instanceCount, baseVertex, baseInstance);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawIndexedPrimitives(%llu, %llu instances, baseVertex %lld, baseInstance %llu)",
                                            (uint64_t)indexCount, (uint64_t)instanceCount,
                                            (int64_t)baseVertex, (uint64_t)baseInstance);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indexed;
      if(instanceCount > 1 || baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = (uint32_t)indexCount;
      action.numInstances = (uint32_t)instanceCount;
      action.baseVertex = (int32_t)baseVertex;
      action.instanceOffset = (uint32_t)baseInstance;
      // The index binding starts at indexBufferOffset, so indexOffset is relative to that binding.
      action.indexOffset = 0;
      replay->SetActionOutputs(action);
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawIndexedPrimitives(
    MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType,
    WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset, NS::UInteger instanceCount)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawIndexedPrimitives(
      primitiveType, indexCount, indexType, Unwrap(indexBuffer), indexBufferOffset, instanceCount));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced);
    Serialise_drawIndexedPrimitives(ser, primitiveType, indexCount, indexType, indexBuffer,
                                    indexBufferOffset, instanceCount, 0, 0);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
    GetRecord(m_CommandBuffer)->MarkResourceFrameReferenced(GetResID(indexBuffer), eFrameRef_Read);
  }
}

void WrappedMTLRenderCommandEncoder::drawIndexedPrimitives(
    MTL::PrimitiveType primitiveType, NS::UInteger indexCount, MTL::IndexType indexType,
    WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset, NS::UInteger instanceCount,
    NS::Integer baseVertex, NS::UInteger baseInstance)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawIndexedPrimitives(
      primitiveType, indexCount, indexType, Unwrap(indexBuffer), indexBufferOffset,
      instanceCount, baseVertex, baseInstance));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced_base);
    Serialise_drawIndexedPrimitives(ser, primitiveType, indexCount, indexType, indexBuffer,
                                    indexBufferOffset, instanceCount, baseVertex, baseInstance);
    MetalResourceRecord *commandBufferRecord = GetRecord(m_CommandBuffer);
    commandBufferRecord->AddChunk(scope.Get());
    commandBufferRecord->MarkResourceFrameReferenced(GetResID(indexBuffer), eFrameRef_Read);
  }
}

void WrappedMTLRenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType,
                                                    NS::UInteger vertexStart,
                                                    NS::UInteger vertexCount)
{
  drawPrimitives(primitiveType, vertexStart, vertexCount, 1, 0);
}

void WrappedMTLRenderCommandEncoder::drawPrimitives(MTL::PrimitiveType primitiveType,
                                                    NS::UInteger vertexStart,
                                                    NS::UInteger vertexCount,
                                                    NS::UInteger instanceCount)
{
  drawPrimitives(primitiveType, vertexStart, vertexCount, instanceCount, 0);
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_useResourceWithStages(SerialiserType &ser, WrappedMTLResource *resource, MTL::ResourceUsage usage, MTL::RenderStages stages)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(resource).Important();
  SERIALISE_ELEMENT_LOCAL(usageValue, (uint64_t)usage).Important();
  SERIALISE_ELEMENT_LOCAL(stagesValue, (uint64_t)stages).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || !ValidMetalResourceUsage(usageValue) ||
       !ValidMetalResidencyResource(m_Device, resource) ||
       !ValidMetalGraphicsStages(stagesValue))
    {
      RDCERR("Invalid Metal render resource declaration");
      return false;
    }
    Unwrap(RenderCommandEncoder)->useResource(Unwrap(resource), (MTL::ResourceUsage)usageValue, (MTL::RenderStages)stagesValue);
    if(IsLoading(m_State) && (usageValue & MTL::ResourceUsageWrite))
      m_Device->GetReplay()->NoteRenderIndirectWrite(
          RenderCommandEncoder->GetParallelParent()?GetResID(RenderCommandEncoder->GetParallelParent()):GetResID(RenderCommandEncoder),resource);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::useResourceWithStages(WrappedMTLResource *resource, MTL::ResourceUsage usage, MTL::RenderStages stages)
{
  if(usage & MTL::ResourceUsageWrite) CaptureIndirectWrite(resource);

  SERIALISE_TIME_CALL(Unwrap(this)->useResource(Unwrap(resource), usage, stages));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_useResource_stages);
    Serialise_useResourceWithStages(ser, resource, usage, stages);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    ReferenceMetalCommandResources(record, {resource}, (usage & MTL::ResourceUsageWrite) != 0);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, useResourceWithStages, WrappedMTLResource *resource, MTL::ResourceUsage usage, MTL::RenderStages stages);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_useResources(SerialiserType &ser, rdcarray<WrappedMTLResource *> resources, MTL::ResourceUsage usage)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(resources).Important();
  SERIALISE_ELEMENT_LOCAL(usageValue, (uint64_t)usage).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || !ValidMetalResourceUsage(usageValue) ||
       !ValidMetalResidencyResources(m_Device, resources))
    {
      RDCERR("Invalid Metal render resource declaration");
      return false;
    }
    if(IsLoading(m_State) && (usageValue & MTL::ResourceUsageWrite))
      for(auto resource:resources)m_Device->GetReplay()->NoteRenderIndirectWrite(
          RenderCommandEncoder->GetParallelParent()?GetResID(RenderCommandEncoder->GetParallelParent()):GetResID(RenderCommandEncoder),resource);
    const auto real = UnwrapMetalResources(resources);
    if(!real.empty())
      Unwrap(RenderCommandEncoder)->useResources(real.data(), real.size(), (MTL::ResourceUsage)usageValue);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::useResources(rdcarray<WrappedMTLResource *> resources, MTL::ResourceUsage usage)
{
  if(usage & MTL::ResourceUsageWrite) for(auto resource:resources)CaptureIndirectWrite(resource);

  const auto real = UnwrapMetalResources(resources);
  if(!real.empty())
  {
    SERIALISE_TIME_CALL(Unwrap(this)->useResources(real.data(), real.size(), usage));
  }
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_useResources);
    Serialise_useResources(ser, resources, usage);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    ReferenceMetalCommandResources(record, resources, (usage & MTL::ResourceUsageWrite) != 0);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, useResources, rdcarray<WrappedMTLResource *> resources, MTL::ResourceUsage usage);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_useResourcesWithStages(SerialiserType &ser, rdcarray<WrappedMTLResource *> resources, MTL::ResourceUsage usage, MTL::RenderStages stages)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(resources).Important();
  SERIALISE_ELEMENT_LOCAL(usageValue, (uint64_t)usage).Important();
  SERIALISE_ELEMENT_LOCAL(stagesValue, (uint64_t)stages).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || !ValidMetalResourceUsage(usageValue) ||
       !ValidMetalResidencyResources(m_Device, resources) ||
       !ValidMetalGraphicsStages(stagesValue))
    {
      RDCERR("Invalid Metal render resource declaration");
      return false;
    }
    if(IsLoading(m_State) && (usageValue & MTL::ResourceUsageWrite))
      for(auto resource:resources)m_Device->GetReplay()->NoteRenderIndirectWrite(
          RenderCommandEncoder->GetParallelParent()?GetResID(RenderCommandEncoder->GetParallelParent()):GetResID(RenderCommandEncoder),resource);
    const auto real = UnwrapMetalResources(resources);
    if(!real.empty())
      Unwrap(RenderCommandEncoder)->useResources(real.data(), real.size(), (MTL::ResourceUsage)usageValue, (MTL::RenderStages)stagesValue);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::useResourcesWithStages(rdcarray<WrappedMTLResource *> resources, MTL::ResourceUsage usage, MTL::RenderStages stages)
{
  if(usage & MTL::ResourceUsageWrite) for(auto resource:resources)CaptureIndirectWrite(resource);

  const auto real = UnwrapMetalResources(resources);
  if(!real.empty())
  {
    SERIALISE_TIME_CALL(Unwrap(this)->useResources(real.data(), real.size(), usage, stages));
  }
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_useResources_stages);
    Serialise_useResourcesWithStages(ser, resources, usage, stages);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    ReferenceMetalCommandResources(record, resources, (usage & MTL::ResourceUsageWrite) != 0);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, useResourcesWithStages, rdcarray<WrappedMTLResource *> resources, MTL::ResourceUsage usage, MTL::RenderStages stages);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_declareHeaps(
    SerialiserType &ser, rdcarray<WrappedMTLHeap *> heaps, MTL::RenderStages stages,
    uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(heaps).Important();
  SERIALISE_ELEMENT_LOCAL(stagesValue, (uint64_t)stages).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder) || variant > 3 || heaps.size() > 32 ||
       ((variant == 0 || variant == 1) && heaps.size() != 1) ||
       ((variant == 0 || variant == 2) && stagesValue != MTL::RenderStageVertex) ||
       ((variant == 1 || variant == 3) && !ValidMetalGraphicsStages(stagesValue)))
    {
      RDCERR("Invalid Metal render heap declaration shape, stage or encoder");
      return false;
    }
    rdcarray<const MTL::Heap *> real;
    for(WrappedMTLHeap *heap : heaps)
    {
      if(!heap || heap->m_Type != eResHeap || !heap->m_Real || heap->m_Device != m_Device)
      {
        RDCERR("Invalid Metal render heap declaration resource identity");
        return false;
      }
      real.push_back(Unwrap(heap));
    }
    if(variant == 0)
      Unwrap(RenderCommandEncoder)->useHeap(real[0]);
    else if(variant == 1)
      Unwrap(RenderCommandEncoder)->useHeap(real[0], (MTL::RenderStages)stagesValue);
    else if(variant == 2 && !real.empty())
      Unwrap(RenderCommandEncoder)->useHeaps(real.data(), real.size());
    else if(variant == 3 && !real.empty())
      Unwrap(RenderCommandEncoder)->useHeaps(real.data(), real.size(),
                                               (MTL::RenderStages)stagesValue);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::declareHeaps(rdcarray<WrappedMTLHeap *> heaps,
                                                   MTL::RenderStages stages, uint32_t variant)
{
  if(variant > 3 || heaps.size() > 32 ||
     ((variant == 0 || variant == 1) && heaps.size() != 1))
  {
    RDCERR("Invalid Metal render heap declaration count or variant");
    return;
  }
  rdcarray<const MTL::Heap *> real;
  for(WrappedMTLHeap *heap : heaps)
  {
    if(!heap || heap->m_Type != eResHeap || !heap->m_Real)
    {
      RDCERR("Invalid Metal render heap declaration resource");
      return;
    }
    real.push_back(Unwrap(heap));
  }
  auto invoke = [&]() {
    if(variant == 0)
      Unwrap(this)->useHeap(real[0]);
    else if(variant == 1)
      Unwrap(this)->useHeap(real[0], stages);
    else if(variant == 2 && !real.empty())
      Unwrap(this)->useHeaps(real.data(), real.size());
    else if(variant == 3 && !real.empty())
      Unwrap(this)->useHeaps(real.data(), real.size(), stages);
  };
  SERIALISE_TIME_CALL(invoke());
  if(IsCaptureMode(m_State))
  {
    const MetalChunk chunk = variant == 0 ? MetalChunk::MTLRenderCommandEncoder_useHeap :
                             variant == 1 ? MetalChunk::MTLRenderCommandEncoder_useHeap_stages :
                             variant == 2 ? MetalChunk::MTLRenderCommandEncoder_useHeaps :
                                            MetalChunk::MTLRenderCommandEncoder_useHeaps_stages;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_declareHeaps(ser, heaps, stages, variant);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLHeap *heap : heaps)
    {
      record->AddParent(GetRecord(heap));
      record->MarkResourceFrameReferenced(GetResID(heap), eFrameRef_Read);
    }
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_declareHeaps(
    ReadSerialiser &, rdcarray<WrappedMTLHeap *>, MTL::RenderStages, uint32_t);
template bool WrappedMTLRenderCommandEncoder::Serialise_declareHeaps(
    WriteSerialiser &, rdcarray<WrappedMTLHeap *>, MTL::RenderStages, uint32_t);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_memoryBarrierWithScope(SerialiserType &ser, MTL::BarrierScope barrierScope, MTL::RenderStages after, MTL::RenderStages before)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT_LOCAL(scopeValue, (uint64_t)barrierScope).Important();
  SERIALISE_ELEMENT_LOCAL(afterValue, (uint64_t)after).Important();
  SERIALISE_ELEMENT_LOCAL(beforeValue, (uint64_t)before).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(!RenderCommandEncoder || scopeValue == 0 || (scopeValue & ~uint64_t(MTL::BarrierScopeBuffers | MTL::BarrierScopeTextures | MTL::BarrierScopeRenderTargets)) != 0 ||
       !ValidMetalGraphicsStages(afterValue) || !ValidMetalGraphicsStages(beforeValue))
    {
      RDCERR("Invalid Metal render memory barrier");
      return false;
    }
    Unwrap(RenderCommandEncoder)->memoryBarrier((MTL::BarrierScope)scopeValue, (MTL::RenderStages)afterValue, (MTL::RenderStages)beforeValue);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::memoryBarrierWithScope(MTL::BarrierScope barrierScope, MTL::RenderStages after, MTL::RenderStages before)
{
  SERIALISE_TIME_CALL(Unwrap(this)->memoryBarrier(barrierScope, after, before));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_memoryBarrierWithScope);
    Serialise_memoryBarrierWithScope(ser, barrierScope, after, before);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, memoryBarrierWithScope, MTL::BarrierScope scope, MTL::RenderStages after, MTL::RenderStages before);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_memoryBarrierWithResources(SerialiserType &ser, rdcarray<WrappedMTLResource *> resources, MTL::RenderStages after, MTL::RenderStages before)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(resources).Important();
  SERIALISE_ELEMENT_LOCAL(afterValue, (uint64_t)after).Important();
  SERIALISE_ELEMENT_LOCAL(beforeValue, (uint64_t)before).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(!RenderCommandEncoder || !ValidMetalCommandResources(m_Device, resources) ||
       !ValidMetalGraphicsStages(afterValue) || !ValidMetalGraphicsStages(beforeValue))
    {
      RDCERR("Invalid Metal render memory barrier");
      return false;
    }
    const auto real = UnwrapMetalResources(resources);
    if(!real.empty())
      Unwrap(RenderCommandEncoder)->memoryBarrier(real.data(), real.size(), (MTL::RenderStages)afterValue, (MTL::RenderStages)beforeValue);
    if(IsLoading(m_State))
      for(auto resource : resources)
        m_Device->GetReplay()->AddUsage(GetResID(resource), ResourceUsage::Barrier,
                                       m_Device->GetReplay()->GetNextEventID());
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::memoryBarrierWithResources(rdcarray<WrappedMTLResource *> resources, MTL::RenderStages after, MTL::RenderStages before)
{
  const auto real = UnwrapMetalResources(resources);
  if(!real.empty())
  {
    SERIALISE_TIME_CALL(Unwrap(this)->memoryBarrier(real.data(), real.size(), after, before));
  }
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_memoryBarrierWithResources);
    Serialise_memoryBarrierWithResources(ser, resources, after, before);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    ReferenceMetalCommandResources(record, resources, false);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, memoryBarrierWithResources, rdcarray<WrappedMTLResource *> resources, MTL::RenderStages after, MTL::RenderStages before);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_updateFence(
    SerialiserType &ser, WrappedMTLFence *fence, MTL::RenderStages stages)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(fence).Important();
  SERIALISE_ELEMENT_LOCAL(stagesValue, (uint64_t)stages).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real || !ValidMetalFence(fence) ||
       !ValidMetalGraphicsStages(stagesValue))
    {
      RDCERR("Invalid Metal render updateFence resource, stage or dependency");
      return false;
    }
    Unwrap(RenderCommandEncoder)->updateFence(Unwrap(fence), (MTL::RenderStages)stagesValue);
    fence->Updated(m_Device->GetReplayEpoch(), GetResID(RenderCommandEncoder));
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::updateFence(WrappedMTLFence *fence, MTL::RenderStages stages)
{
  SERIALISE_TIME_CALL(Unwrap(this)->updateFence(Unwrap(fence), stages));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_updateFence);
    Serialise_updateFence(ser, fence, stages);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(fence), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, updateFence,
                                WrappedMTLFence *fence, MTL::RenderStages stages);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_waitForFence(
    SerialiserType &ser, WrappedMTLFence *fence, MTL::RenderStages stages)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(fence).Important();
  SERIALISE_ELEMENT_LOCAL(stagesValue, (uint64_t)stages).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real || !ValidMetalFence(fence) ||
       !ValidMetalGraphicsStages(stagesValue) ||
       !fence->CanWait(m_Device->GetReplayEpoch(), GetResID(RenderCommandEncoder)))
    {
      RDCERR("Invalid Metal render waitForFence resource, stage or dependency");
      return false;
    }
    Unwrap(RenderCommandEncoder)->waitForFence(Unwrap(fence), (MTL::RenderStages)stagesValue);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::waitForFence(WrappedMTLFence *fence, MTL::RenderStages stages)
{
  SERIALISE_TIME_CALL(Unwrap(this)->waitForFence(Unwrap(fence), stages));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_waitForFence);
    Serialise_waitForFence(ser, fence, stages);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(fence), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, waitForFence,
                                WrappedMTLFence *fence, MTL::RenderStages stages);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_endEncoding(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !Unwrap(RenderCommandEncoder))
      return false;
    if(!RenderCommandEncoder->GetParallelParent())
      RenderCommandEncoder->ResolveDeferredStoreActions();
    if(!RenderCommandEncoder->GetParallelParent())
      m_Device->FinaliseReplayStores(Unwrap(RenderCommandEncoder), false);
    Unwrap(RenderCommandEncoder)->endEncoding();
    if(!RenderCommandEncoder->GetParallelParent())
      m_Device->ApplyReplayStoreDiscards(Unwrap(RenderCommandEncoder->GetCommandBuffer()),
          m_Device->GetReplay()->GetRenderPassDescriptor());
    if(IsLoading(m_State) && !RenderCommandEncoder->GetParallelParent() &&
       !m_Device->GetReplay()->FlushRenderIndirectActions(GetResID(RenderCommandEncoder),
           Unwrap(RenderCommandEncoder->GetCommandBuffer())))return false;
    m_Device->SetReplayRenderCommandEncoder(NULL);

    if(RenderCommandEncoder->GetParallelParent())
    {
      if(IsLoading(m_State))
      {
        AddEvent();
        ActionDescription childEnd;
        childEnd.customName = "End Metal Parallel Render Child";
        childEnd.flags = ActionFlags::PassBoundary | ActionFlags::EndPass;
        AddAction(childEnd);
      }
      return true;
    }

    ActionDescription action;
    if(IsLoading(m_State))
    {
      action.customName = StringFormat::Fmt(
          "End Metal Render Pass (%s)",
          RDMTL::RenderPassOpString(m_Device->GetReplay()->GetRenderPassDescriptor(), true).c_str());
      action.flags = ActionFlags::PassBoundary | ActionFlags::EndPass;
      m_Device->GetReplay()->SetActionOutputs(action);
    }

    m_Device->GetReplay()->EndRenderPass();

    if(IsLoading(m_State))
    {
      AddEvent();
      AddAction(action);
      m_Device->GetReplay()->AddRenderPassStoreUsage(
          m_Device->GetReplay()->GetRenderPassDescriptor());
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::endEncoding()
{
  SERIALISE_TIME_CALL(Unwrap(this)->endEncoding());
  if(!m_ParallelParent)m_Device->FlushRenderIndirectCaptures(m_CommandBuffer,m_ID);

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_endEncoding);
      Serialise_endEncoding(ser);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, endEncoding);
template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setTessellationFactorBuffer(
    SerialiserType &ser, WrappedMTLBuffer *buffer, NS::UInteger offset,
    NS::UInteger instanceStride)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT_LOCAL(bufferId, GetResID(buffer)).Named("buffer"_lit)
      .TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(instanceStride).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(bufferId != ResourceId() && !GetResourceManager()->HasResource(bufferId))
    {
      RDCERR("Invalid Metal tessellation factor buffer identity");
      return false;
    }
    buffer = bufferId == ResourceId() ? NULL :
        (WrappedMTLBuffer *)GetResourceManager()->GetResource(bufferId);
    MTL::Buffer *real = buffer && buffer->m_Type == eResBuffer ? Unwrap(buffer) : NULL;
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real || RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       (buffer && (!real || offset >= real->length())) ||
       instanceStride > UINT32_MAX)
    {
      RDCERR("Invalid Metal tessellation factor buffer or range");
      return false;
    }
    m_Device->GetReplay()->SetInspectionTessellationBuffer(bufferId, offset, instanceStride);
    Unwrap(RenderCommandEncoder)->setTessellationFactorBuffer(real, offset, instanceStride);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setTessellationFactorBuffer(
    WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger instanceStride)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTessellationFactorBuffer(Unwrap(buffer), offset,
                                                                 instanceStride));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setTessellationFactorBuffer);
    Serialise_setTessellationFactorBuffer(ser, buffer, offset, instanceStride);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(buffer)
      record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setTessellationFactorScale(SerialiserType &ser,
                                                                          float scale)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(scale).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real || RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !std::isfinite(scale) || scale < 0.0f)
    {
      RDCERR("Invalid Metal tessellation factor scale");
      return false;
    }
    m_Device->GetReplay()->SetInspectionTessellationScale(scale);
    Unwrap(RenderCommandEncoder)->setTessellationFactorScale(scale);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setTessellationFactorScale(float scale)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTessellationFactorScale(scale));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setTessellationFactorScale);
    Serialise_setTessellationFactorScale(ser, scale);
    GetRecord(m_CommandBuffer)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawPatches(
    SerialiserType &ser, NS::UInteger controlPoints, NS::UInteger patchStart,
    NS::UInteger patchCount, WrappedMTLBuffer *patchIndexBuffer,
    NS::UInteger patchIndexBufferOffset, NS::UInteger instanceCount,
    NS::UInteger baseInstance)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(controlPoints).Important();
  SERIALISE_ELEMENT(patchStart).Important();
  SERIALISE_ELEMENT(patchCount).Important();
  SERIALISE_ELEMENT_LOCAL(patchIndexBufferId, GetResID(patchIndexBuffer))
      .Named("patchIndexBuffer"_lit).TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(patchIndexBufferOffset).Important();
  SERIALISE_ELEMENT(instanceCount).Important();
  SERIALISE_ELEMENT(baseInstance).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(patchIndexBufferId != ResourceId() &&
       !GetResourceManager()->HasResource(patchIndexBufferId))
    {
      RDCERR("Invalid Metal patch index buffer identity");
      return false;
    }
    patchIndexBuffer = patchIndexBufferId == ResourceId() ? NULL :
        (WrappedMTLBuffer *)GetResourceManager()->GetResource(patchIndexBufferId);
    MTL::Buffer *indices = patchIndexBuffer && patchIndexBuffer->m_Type == eResBuffer
                               ? Unwrap(patchIndexBuffer) : NULL;
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real || RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       controlPoints == 0 || controlPoints > 32 || patchCount == 0 ||
       patchCount > UINT32_MAX / controlPoints || patchStart > UINT32_MAX ||
       instanceCount == 0 || instanceCount > UINT32_MAX || baseInstance > UINT32_MAX ||
       (patchIndexBuffer && (!indices || patchIndexBufferOffset >= indices->length())) ||
       (!patchIndexBuffer && patchIndexBufferOffset != 0))
    {
      RDCERR("Invalid Metal patch draw arguments or index buffer");
      return false;
    }
    if(!m_Device->GetReplay()->ValidateArgumentBufferBindings())
      return false;
    m_Device->GetReplay()->SetIndirectBuffer(ResourceId(), 0, 0);
    m_Device->GetReplay()->SetInspectionPatchControlPoints((uint32_t)controlPoints);
    Unwrap(RenderCommandEncoder)->drawPatches(controlPoints, patchStart, patchCount, indices,
                                               patchIndexBufferOffset, instanceCount, baseInstance);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawPatches(%llu patches, %llu control points)",
                                          (uint64_t)patchCount, (uint64_t)controlPoints);
      action.flags = ActionFlags::Drawcall;
      if(instanceCount > 1 || baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = (uint32_t)(patchCount * controlPoints);
      action.numInstances = (uint32_t)instanceCount;
      action.vertexOffset = (uint32_t)patchStart;
      action.instanceOffset = (uint32_t)baseInstance;
      m_Device->GetReplay()->SetActionOutputs(action);
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawPatches(
    NS::UInteger controlPoints, NS::UInteger patchStart, NS::UInteger patchCount,
    WrappedMTLBuffer *patchIndexBuffer, NS::UInteger patchIndexBufferOffset,
    NS::UInteger instanceCount, NS::UInteger baseInstance)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawPatches(controlPoints, patchStart, patchCount,
                                                Unwrap(patchIndexBuffer), patchIndexBufferOffset,
                                                instanceCount, baseInstance));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawPatches);
    Serialise_drawPatches(ser, controlPoints, patchStart, patchCount, patchIndexBuffer,
                          patchIndexBufferOffset, instanceCount, baseInstance);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(patchIndexBuffer)
      record->MarkResourceFrameReferenced(GetResID(patchIndexBuffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawPatchesIndirect(
    SerialiserType &ser, NS::UInteger controlPoints, WrappedMTLBuffer *patchIndexBuffer,
    NS::UInteger patchIndexBufferOffset, WrappedMTLBuffer *indirectBuffer,
    NS::UInteger indirectBufferOffset)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(controlPoints).Important();
  SERIALISE_ELEMENT_LOCAL(patchIndexId, GetResID(patchIndexBuffer))
      .Named("patchIndexBuffer"_lit).TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(patchIndexBufferOffset).Important();
  SERIALISE_ELEMENT_LOCAL(indirectId, GetResID(indirectBuffer))
      .Named("indirectBuffer"_lit).TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(indirectBufferOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if((patchIndexId != ResourceId() && !GetResourceManager()->HasResource(patchIndexId)) ||
       indirectId == ResourceId() || !GetResourceManager()->HasResource(indirectId))
    {
      RDCERR("Invalid Metal indirect patch draw resource identity");
      return false;
    }
    patchIndexBuffer = patchIndexId == ResourceId() ? NULL :
        (WrappedMTLBuffer *)GetResourceManager()->GetResource(patchIndexId);
    indirectBuffer = (WrappedMTLBuffer *)GetResourceManager()->GetResource(indirectId);
    MTL::Buffer *indices = patchIndexBuffer && patchIndexBuffer->m_Type == eResBuffer
                               ? Unwrap(patchIndexBuffer) : NULL;
    MTL::Buffer *args = indirectBuffer && indirectBuffer->m_Type == eResBuffer
                            ? Unwrap(indirectBuffer) : NULL;
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real || RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       controlPoints == 0 || controlPoints > 32 ||
       (patchIndexBuffer && (!indices || patchIndexBufferOffset >= indices->length())) ||
       (!patchIndexBuffer && patchIndexBufferOffset != 0) || !args ||
       (indirectBufferOffset & 3) || indirectBufferOffset > args->length() ||
       sizeof(MTL::DrawPatchIndirectArguments) > args->length() - indirectBufferOffset)
    {
      RDCERR("Invalid Metal indirect patch draw arguments or buffer range");
      return false;
    }
    if(!m_Device->GetReplay()->ValidateArgumentBufferBindings())
      return false;
    MetalReplay *replay = m_Device->GetReplay();
    replay->SetIndirectBuffer(indirectId, indirectBufferOffset,
                              sizeof(MTL::DrawPatchIndirectArguments));
    m_Device->GetReplay()->SetInspectionPatchControlPoints((uint32_t)controlPoints);
    Unwrap(RenderCommandEncoder)->drawPatches(controlPoints, indices, patchIndexBufferOffset,
                                               args, indirectBufferOffset);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawPatches(indirect, %llu control points)",
                                          (uint64_t)controlPoints);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indirect;
      // The GPU can write indirect arguments after CPU encoding. The count is unknown here.
      action.numIndices = 0;
      action.numInstances = 0;
      replay->SetActionOutputs(action);
      AddAction(action);
      replay->AddUsage(indirectId, ResourceUsage::Indirect);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawPatchesIndirect(
    NS::UInteger controlPoints, WrappedMTLBuffer *patchIndexBuffer,
    NS::UInteger patchIndexBufferOffset, WrappedMTLBuffer *indirectBuffer,
    NS::UInteger indirectBufferOffset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawPatches(controlPoints, Unwrap(patchIndexBuffer),
                                                patchIndexBufferOffset, Unwrap(indirectBuffer),
                                                indirectBufferOffset));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawPatches_indirect);
    Serialise_drawPatchesIndirect(ser, controlPoints, patchIndexBuffer,
                                 patchIndexBufferOffset, indirectBuffer, indirectBufferOffset);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(patchIndexBuffer)
      record->MarkResourceFrameReferenced(GetResID(patchIndexBuffer), eFrameRef_Read);
    if(indirectBuffer)
      record->MarkResourceFrameReferenced(GetResID(indirectBuffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawIndexedPatches(
    SerialiserType &ser, NS::UInteger controlPoints, NS::UInteger patchStart,
    NS::UInteger patchCount, WrappedMTLBuffer *patchIndexBuffer,
    NS::UInteger patchIndexBufferOffset, WrappedMTLBuffer *controlPointIndexBuffer,
    NS::UInteger controlPointIndexBufferOffset, NS::UInteger instanceCount,
    NS::UInteger baseInstance)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(controlPoints).Important();
  SERIALISE_ELEMENT(patchStart).Important();
  SERIALISE_ELEMENT(patchCount).Important();
  SERIALISE_ELEMENT_LOCAL(patchIndexId, GetResID(patchIndexBuffer))
      .Named("patchIndexBuffer"_lit).TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(patchIndexBufferOffset).Important();
  SERIALISE_ELEMENT_LOCAL(controlPointIndexId, GetResID(controlPointIndexBuffer))
      .Named("controlPointIndexBuffer"_lit).TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(controlPointIndexBufferOffset).Important();
  SERIALISE_ELEMENT(instanceCount).Important();
  SERIALISE_ELEMENT(baseInstance).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if((patchIndexId != ResourceId() && !GetResourceManager()->HasResource(patchIndexId)) ||
       controlPointIndexId == ResourceId() ||
       !GetResourceManager()->HasResource(controlPointIndexId))
    {
      RDCERR("Invalid Metal indexed patch draw resource identity");
      return false;
    }
    patchIndexBuffer = patchIndexId == ResourceId() ? NULL :
        (WrappedMTLBuffer *)GetResourceManager()->GetResource(patchIndexId);
    controlPointIndexBuffer =
        (WrappedMTLBuffer *)GetResourceManager()->GetResource(controlPointIndexId);
    MTL::Buffer *patchIndices = patchIndexBuffer && patchIndexBuffer->m_Type == eResBuffer
                                    ? Unwrap(patchIndexBuffer) : NULL;
    MTL::Buffer *controlIndices = controlPointIndexBuffer &&
                                   controlPointIndexBuffer->m_Type == eResBuffer
                                       ? Unwrap(controlPointIndexBuffer) : NULL;
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real || RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       controlPoints == 0 || controlPoints > 32 || patchCount == 0 ||
       patchCount > UINT32_MAX / controlPoints || patchStart > UINT32_MAX ||
       instanceCount == 0 || instanceCount > UINT32_MAX || baseInstance > UINT32_MAX ||
       (patchIndexBuffer && (!patchIndices || patchIndexBufferOffset >= patchIndices->length())) ||
       (!patchIndexBuffer && patchIndexBufferOffset != 0) || !controlIndices ||
       controlPointIndexBufferOffset >= controlIndices->length())
    {
      RDCERR("Invalid Metal indexed patch draw arguments or buffer range");
      return false;
    }
    if(!m_Device->GetReplay()->ValidateArgumentBufferBindings())
      return false;
    MetalReplay *replay = m_Device->GetReplay();
    replay->SetIndirectBuffer(ResourceId(), 0, 0);
    m_Device->GetReplay()->SetInspectionPatchControlPoints((uint32_t)controlPoints);
    Unwrap(RenderCommandEncoder)->drawIndexedPatches(
        controlPoints, patchStart, patchCount, patchIndices, patchIndexBufferOffset,
        controlIndices, controlPointIndexBufferOffset, instanceCount, baseInstance);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawIndexedPatches(%llu patches, %llu control points)",
                                          (uint64_t)patchCount, (uint64_t)controlPoints);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indexed;
      if(instanceCount > 1 || baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = (uint32_t)(patchCount * controlPoints);
      action.numInstances = (uint32_t)instanceCount;
      action.vertexOffset = (uint32_t)patchStart;
      action.instanceOffset = (uint32_t)baseInstance;
      replay->SetActionOutputs(action);
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawIndexedPatches(
    NS::UInteger controlPoints, NS::UInteger patchStart, NS::UInteger patchCount,
    WrappedMTLBuffer *patchIndexBuffer, NS::UInteger patchIndexBufferOffset,
    WrappedMTLBuffer *controlPointIndexBuffer, NS::UInteger controlPointIndexBufferOffset,
    NS::UInteger instanceCount, NS::UInteger baseInstance)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawIndexedPatches(
      controlPoints, patchStart, patchCount, Unwrap(patchIndexBuffer), patchIndexBufferOffset,
      Unwrap(controlPointIndexBuffer), controlPointIndexBufferOffset, instanceCount, baseInstance));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawIndexedPatches);
    Serialise_drawIndexedPatches(ser, controlPoints, patchStart, patchCount, patchIndexBuffer,
                                 patchIndexBufferOffset, controlPointIndexBuffer,
                                 controlPointIndexBufferOffset, instanceCount, baseInstance);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(patchIndexBuffer)
      record->MarkResourceFrameReferenced(GetResID(patchIndexBuffer), eFrameRef_Read);
    if(controlPointIndexBuffer)
      record->MarkResourceFrameReferenced(GetResID(controlPointIndexBuffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawIndexedPatchesIndirect(
    SerialiserType &ser, NS::UInteger controlPoints, WrappedMTLBuffer *patchIndexBuffer,
    NS::UInteger patchIndexBufferOffset, WrappedMTLBuffer *controlPointIndexBuffer,
    NS::UInteger controlPointIndexBufferOffset, WrappedMTLBuffer *indirectBuffer,
    NS::UInteger indirectBufferOffset)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(controlPoints).Important();
  SERIALISE_ELEMENT_LOCAL(patchIndexId, GetResID(patchIndexBuffer))
      .Named("patchIndexBuffer"_lit).TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(patchIndexBufferOffset).Important();
  SERIALISE_ELEMENT_LOCAL(controlPointIndexId, GetResID(controlPointIndexBuffer))
      .Named("controlPointIndexBuffer"_lit).TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(controlPointIndexBufferOffset).Important();
  SERIALISE_ELEMENT_LOCAL(indirectId, GetResID(indirectBuffer))
      .Named("indirectBuffer"_lit).TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(indirectBufferOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if((patchIndexId != ResourceId() && !GetResourceManager()->HasResource(patchIndexId)) ||
       controlPointIndexId == ResourceId() ||
       !GetResourceManager()->HasResource(controlPointIndexId) ||
       indirectId == ResourceId() || !GetResourceManager()->HasResource(indirectId))
    {
      RDCERR("Invalid Metal indirect indexed patch draw resource identity");
      return false;
    }
    patchIndexBuffer = patchIndexId == ResourceId() ? NULL :
        (WrappedMTLBuffer *)GetResourceManager()->GetResource(patchIndexId);
    controlPointIndexBuffer =
        (WrappedMTLBuffer *)GetResourceManager()->GetResource(controlPointIndexId);
    indirectBuffer = (WrappedMTLBuffer *)GetResourceManager()->GetResource(indirectId);
    MTL::Buffer *patchIndices = patchIndexBuffer && patchIndexBuffer->m_Type == eResBuffer
                                    ? Unwrap(patchIndexBuffer) : NULL;
    MTL::Buffer *controlIndices = controlPointIndexBuffer &&
                                   controlPointIndexBuffer->m_Type == eResBuffer
                                       ? Unwrap(controlPointIndexBuffer) : NULL;
    MTL::Buffer *args = indirectBuffer && indirectBuffer->m_Type == eResBuffer
                            ? Unwrap(indirectBuffer) : NULL;
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !RenderCommandEncoder->m_Real || RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       controlPoints == 0 || controlPoints > 32 ||
       (patchIndexBuffer && (!patchIndices || patchIndexBufferOffset >= patchIndices->length())) ||
       (!patchIndexBuffer && patchIndexBufferOffset != 0) || !controlIndices ||
       controlPointIndexBufferOffset >= controlIndices->length() || !args ||
       (indirectBufferOffset & 3) || indirectBufferOffset > args->length() ||
       sizeof(MTL::DrawPatchIndirectArguments) > args->length() - indirectBufferOffset)
    {
      RDCERR("Invalid Metal indirect indexed patch draw arguments or buffer range");
      return false;
    }
    if(!m_Device->GetReplay()->ValidateArgumentBufferBindings())
      return false;
    MetalReplay *replay = m_Device->GetReplay();
    replay->SetIndirectBuffer(indirectId, indirectBufferOffset,
                              sizeof(MTL::DrawPatchIndirectArguments));
    m_Device->GetReplay()->SetInspectionPatchControlPoints((uint32_t)controlPoints);
    Unwrap(RenderCommandEncoder)->drawIndexedPatches(
        controlPoints, patchIndices, patchIndexBufferOffset, controlIndices,
        controlPointIndexBufferOffset, args, indirectBufferOffset);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("drawIndexedPatches(indirect, %llu control points)",
                                          (uint64_t)controlPoints);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indexed | ActionFlags::Indirect;
      action.numIndices = 0;
      action.numInstances = 0;
      replay->SetActionOutputs(action);
      AddAction(action);
      replay->AddUsage(indirectId, ResourceUsage::Indirect);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawIndexedPatchesIndirect(
    NS::UInteger controlPoints, WrappedMTLBuffer *patchIndexBuffer,
    NS::UInteger patchIndexBufferOffset, WrappedMTLBuffer *controlPointIndexBuffer,
    NS::UInteger controlPointIndexBufferOffset, WrappedMTLBuffer *indirectBuffer,
    NS::UInteger indirectBufferOffset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->drawIndexedPatches(
      controlPoints, Unwrap(patchIndexBuffer), patchIndexBufferOffset,
      Unwrap(controlPointIndexBuffer), controlPointIndexBufferOffset,
      Unwrap(indirectBuffer), indirectBufferOffset));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawIndexedPatches_indirect);
    Serialise_drawIndexedPatchesIndirect(ser, controlPoints, patchIndexBuffer,
                                         patchIndexBufferOffset, controlPointIndexBuffer,
                                         controlPointIndexBufferOffset, indirectBuffer,
                                         indirectBufferOffset);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(patchIndexBuffer)
      record->MarkResourceFrameReferenced(GetResID(patchIndexBuffer), eFrameRef_Read);
    if(controlPointIndexBuffer)
      record->MarkResourceFrameReferenced(GetResID(controlPointIndexBuffer), eFrameRef_Read);
    if(indirectBuffer)
      record->MarkResourceFrameReferenced(GetResID(indirectBuffer), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setTessellationFactorBuffer,
                                WrappedMTLBuffer *buffer, NS::UInteger offset,
                                NS::UInteger instanceStride);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setTessellationFactorScale,
                                float scale);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawPatches,
                                NS::UInteger controlPoints, NS::UInteger patchStart,
                                NS::UInteger patchCount, WrappedMTLBuffer *patchIndexBuffer,
                                NS::UInteger patchIndexBufferOffset, NS::UInteger instanceCount,
                                NS::UInteger baseInstance);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawPatchesIndirect,
                                NS::UInteger controlPoints, WrappedMTLBuffer *patchIndexBuffer,
                                NS::UInteger patchIndexBufferOffset, WrappedMTLBuffer *indirectBuffer,
                                NS::UInteger indirectBufferOffset);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawIndexedPatches,
                                NS::UInteger controlPoints, NS::UInteger patchStart,
                                NS::UInteger patchCount, WrappedMTLBuffer *patchIndexBuffer,
                                NS::UInteger patchIndexBufferOffset,
                                WrappedMTLBuffer *controlPointIndexBuffer,
                                NS::UInteger controlPointIndexBufferOffset,
                                NS::UInteger instanceCount, NS::UInteger baseInstance);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawIndexedPatchesIndirect,
                                NS::UInteger controlPoints, WrappedMTLBuffer *patchIndexBuffer,
                                NS::UInteger patchIndexBufferOffset,
                                WrappedMTLBuffer *controlPointIndexBuffer,
                                NS::UInteger controlPointIndexBufferOffset,
                                WrappedMTLBuffer *indirectBuffer,
                                NS::UInteger indirectBufferOffset);

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, insertDebugSignpost,
                                NS::String *string);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, pushDebugGroup,
                                NS::String *string);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, popDebugGroup);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setRenderPipelineState,
                                WrappedMTLRenderPipelineState *pipelineState);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexAmplificationCount,
                                NS::UInteger, rdcarray<uint32_t>, rdcarray<uint32_t>, bool);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexBuffer,
                                WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexBytes,
                                rdcarray<byte> data, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexBufferOffset,
                                NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexBuffers,
                                rdcarray<WrappedMTLBuffer *> buffers,
                                rdcarray<NS::UInteger> offsets, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexTexture,
                                WrappedMTLTexture *texture, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexTextures,
                                rdcarray<WrappedMTLTexture *> textures, NS::Range range);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexSamplerStateWithLOD(
    SerialiserType &ser, WrappedMTLSamplerState *sampler, float lodMinClamp,
    float lodMaxClamp, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT_LOCAL(bound, sampler != NULL);
  SERIALISE_ELEMENT(sampler).Important();
  SERIALISE_ELEMENT(lodMinClamp).Important();
  SERIALISE_ELEMENT(lodMaxClamp).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(!RenderCommandEncoder || index >= 16 || bound != (sampler != NULL) ||
       !std::isfinite(lodMinClamp) || !std::isfinite(lodMaxClamp) ||
       lodMinClamp < 0.0f || lodMaxClamp < lodMinClamp)
    {
      RDCERR("Invalid Metal vertex sampler LOD binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setVertexSamplerState(Unwrap(sampler), lodMinClamp, lodMaxClamp, index);
    m_Device->GetReplay()->BindVertexSampler((uint32_t)index, GetResID(sampler));
    if(sampler)
      m_Device->GetReplay()->SetSamplerLOD(ShaderStage::Vertex, (uint32_t)index,
                                           lodMinClamp, lodMaxClamp);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexSamplerStateWithLOD(
    WrappedMTLSamplerState *sampler, float lodMinClamp, float lodMaxClamp, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexSamplerState(Unwrap(sampler), lodMinClamp, lodMaxClamp, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexSamplerState_lodclamp);
    Serialise_setVertexSamplerStateWithLOD(ser, sampler, lodMinClamp, lodMaxClamp, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(sampler)
      record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setVertexSamplerStatesWithLOD(
    SerialiserType &ser, rdcarray<WrappedMTLSamplerState *> samplers,
    rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && (range.location >= 16 || range.length > 16 - range.location))
  {
    RDCERR("Invalid Metal vertex sampler LOD range");
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
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(!RenderCommandEncoder || samplers.size() != range.length || bound.size() != range.length ||
       lodMinClamps.size() != range.length || lodMaxClamps.size() != range.length)
    {
      RDCERR("Invalid Metal vertex sampler LOD array lengths");
      return false;
    }
    rdcarray<const MTL::SamplerState *> real;
    for(size_t i = 0; i < samplers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0) != (samplers[i] != NULL) ||
         !std::isfinite(lodMinClamps[i]) || !std::isfinite(lodMaxClamps[i]) ||
         lodMinClamps[i] < 0.0f || lodMaxClamps[i] < lodMinClamps[i])
      {
        RDCERR("Invalid Metal vertex sampler LOD array element");
        return false;
      }
      real.push_back(Unwrap(samplers[i]));
    }
    Unwrap(RenderCommandEncoder)->setVertexSamplerStates(real.data(), lodMinClamps.data(), lodMaxClamps.data(), range);
    for(size_t i = 0; i < samplers.size(); i++)
    {
      const uint32_t slot = (uint32_t)(range.location + i);
      m_Device->GetReplay()->BindVertexSampler(slot, GetResID(samplers[i]));
      if(samplers[i])
        m_Device->GetReplay()->SetSamplerLOD(ShaderStage::Vertex, slot,
                                             lodMinClamps[i], lodMaxClamps[i]);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setVertexSamplerStatesWithLOD(
    rdcarray<WrappedMTLSamplerState *> samplers, rdcarray<float> lodMinClamps,
    rdcarray<float> lodMaxClamps, NS::Range range)
{
  rdcarray<const MTL::SamplerState *> real;
  for(WrappedMTLSamplerState *sampler : samplers)
    real.push_back(Unwrap(sampler));
  SERIALISE_TIME_CALL(Unwrap(this)->setVertexSamplerStates(real.data(), lodMinClamps.data(),
                                                     lodMaxClamps.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setVertexSamplerStates_lodclamp);
    Serialise_setVertexSamplerStatesWithLOD(ser, samplers, lodMinClamps, lodMaxClamps, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLSamplerState *sampler : samplers)
      if(sampler)
        record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexSamplerStateWithLOD,
                                WrappedMTLSamplerState *sampler, float lodMinClamp,
                                float lodMaxClamp, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexSamplerStatesWithLOD,
                                rdcarray<WrappedMTLSamplerState *> samplers,
                                rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps,
                                NS::Range range);

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexSamplerState,
                                WrappedMTLSamplerState *sampler, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVertexSamplerStates,
                                rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentBuffer,
                                WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentBytes,
                                rdcarray<byte> data, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentBufferOffset,
                                NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentBuffers,
                                rdcarray<WrappedMTLBuffer *> buffers,
                                rdcarray<NS::UInteger> offsets, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentTexture,
                                WrappedMTLTexture *texture, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentTextures,
                                rdcarray<WrappedMTLTexture *> textures, NS::Range range);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentSamplerStateWithLOD(
    SerialiserType &ser, WrappedMTLSamplerState *sampler, float lodMinClamp,
    float lodMaxClamp, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT_LOCAL(bound, sampler != NULL);
  SERIALISE_ELEMENT(sampler).Important();
  SERIALISE_ELEMENT(lodMinClamp).Important();
  SERIALISE_ELEMENT(lodMaxClamp).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(!RenderCommandEncoder || index >= 16 || bound != (sampler != NULL) ||
       !std::isfinite(lodMinClamp) || !std::isfinite(lodMaxClamp) ||
       lodMinClamp < 0.0f || lodMaxClamp < lodMinClamp)
    {
      RDCERR("Invalid Metal fragment sampler LOD binding");
      return false;
    }
    Unwrap(RenderCommandEncoder)->setFragmentSamplerState(Unwrap(sampler), lodMinClamp, lodMaxClamp, index);
    m_Device->GetReplay()->BindFragmentSampler((uint32_t)index, GetResID(sampler));
    if(sampler)
      m_Device->GetReplay()->SetSamplerLOD(ShaderStage::Fragment, (uint32_t)index,
                                           lodMinClamp, lodMaxClamp);
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentSamplerStateWithLOD(
    WrappedMTLSamplerState *sampler, float lodMinClamp, float lodMaxClamp, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentSamplerState(Unwrap(sampler), lodMinClamp, lodMaxClamp, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerState_lodclamp);
    Serialise_setFragmentSamplerStateWithLOD(ser, sampler, lodMinClamp, lodMaxClamp, index);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    if(sampler)
      record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_setFragmentSamplerStatesWithLOD(
    SerialiserType &ser, rdcarray<WrappedMTLSamplerState *> samplers,
    rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && (range.location >= 16 || range.length > 16 - range.location))
  {
    RDCERR("Invalid Metal fragment sampler LOD range");
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
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(!RenderCommandEncoder || samplers.size() != range.length || bound.size() != range.length ||
       lodMinClamps.size() != range.length || lodMaxClamps.size() != range.length)
    {
      RDCERR("Invalid Metal fragment sampler LOD array lengths");
      return false;
    }
    rdcarray<const MTL::SamplerState *> real;
    for(size_t i = 0; i < samplers.size(); i++)
    {
      if(bound[i] > 1 || (bound[i] != 0) != (samplers[i] != NULL) ||
         !std::isfinite(lodMinClamps[i]) || !std::isfinite(lodMaxClamps[i]) ||
         lodMinClamps[i] < 0.0f || lodMaxClamps[i] < lodMinClamps[i])
      {
        RDCERR("Invalid Metal fragment sampler LOD array element");
        return false;
      }
      real.push_back(Unwrap(samplers[i]));
    }
    Unwrap(RenderCommandEncoder)->setFragmentSamplerStates(real.data(), lodMinClamps.data(), lodMaxClamps.data(), range);
    for(size_t i = 0; i < samplers.size(); i++)
    {
      const uint32_t slot = (uint32_t)(range.location + i);
      m_Device->GetReplay()->BindFragmentSampler(slot, GetResID(samplers[i]));
      if(samplers[i])
        m_Device->GetReplay()->SetSamplerLOD(ShaderStage::Fragment, slot,
                                             lodMinClamps[i], lodMaxClamps[i]);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::setFragmentSamplerStatesWithLOD(
    rdcarray<WrappedMTLSamplerState *> samplers, rdcarray<float> lodMinClamps,
    rdcarray<float> lodMaxClamps, NS::Range range)
{
  rdcarray<const MTL::SamplerState *> real;
  for(WrappedMTLSamplerState *sampler : samplers)
    real.push_back(Unwrap(sampler));
  SERIALISE_TIME_CALL(Unwrap(this)->setFragmentSamplerStates(real.data(), lodMinClamps.data(),
                                                     lodMaxClamps.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_setFragmentSamplerStates_lodclamp);
    Serialise_setFragmentSamplerStatesWithLOD(ser, samplers, lodMinClamps, lodMaxClamps, range);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    for(WrappedMTLSamplerState *sampler : samplers)
      if(sampler)
        record->MarkResourceFrameReferenced(GetResID(sampler), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentSamplerStateWithLOD,
                                WrappedMTLSamplerState *sampler, float lodMinClamp,
                                float lodMaxClamp, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentSamplerStatesWithLOD,
                                rdcarray<WrappedMTLSamplerState *> samplers,
                                rdcarray<float> lodMinClamps, rdcarray<float> lodMaxClamps,
                                NS::Range range);

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentSamplerState,
                                WrappedMTLSamplerState *sampler, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFragmentSamplerStates,
                                rdcarray<WrappedMTLSamplerState *> samplers, NS::Range range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, useResource,
                                WrappedMTLResource *resource, MTL::ResourceUsage usage);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setViewport,
                                MTL::Viewport &viewport);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setViewports,
                                rdcarray<MTL::Viewport> viewports);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setScissorRect,
                                MTL::ScissorRect &rect);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setScissorRects,
                                rdcarray<MTL::ScissorRect> scissors);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setFrontFacingWinding,
                                MTL::Winding winding);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setCullMode,
                                MTL::CullMode cullMode);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setDepthClipMode,
                                MTL::DepthClipMode depthClipMode);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setDepthBias,
                                float depthBias, float slopeScale, float clamp);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setTriangleFillMode,
                                MTL::TriangleFillMode fillMode);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setBlendColor,
                                float red, float green, float blue, float alpha);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setDepthStencilState,
                                WrappedMTLDepthStencilState *depthStencilState);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setStencilReferenceValue,
                                uint32_t referenceValue);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setStencilReferenceValues,
                                uint32_t frontReferenceValue, uint32_t backReferenceValue);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setVisibilityResultMode,
                                MTL::VisibilityResultMode mode, NS::UInteger offset);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setColorStoreAction,
                                MTL::StoreAction storeAction,
                                NS::UInteger colorAttachmentIndex);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setDepthStoreAction,
                                MTL::StoreAction storeAction);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setStencilStoreAction,
                                MTL::StoreAction storeAction);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setColorStoreActionOptions,
                                MTL::StoreActionOptions storeActionOptions,
                                NS::UInteger colorAttachmentIndex);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, setDepthStoreActionOptions,
                                MTL::StoreActionOptions storeActionOptions);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void,
                                setStencilStoreActionOptions,
                                MTL::StoreActionOptions storeActionOptions);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, textureBarrier);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawPrimitives,
                                MTL::PrimitiveType primitiveType, NS::UInteger vertexStart,
                                NS::UInteger vertexCount, NS::UInteger instanceCount,
                                NS::UInteger baseInstance);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawPrimitives,
                                MTL::PrimitiveType primitiveType,
                                WrappedMTLBuffer *indirectBuffer,
                                NS::UInteger indirectBufferOffset);
template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_drawIndexedPrimitives(
    SerialiserType &ser, MTL::PrimitiveType primitiveType, MTL::IndexType indexType,
    WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset,
    WrappedMTLBuffer *indirectBuffer, NS::UInteger indirectBufferOffset)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(primitiveType);
  SERIALISE_ELEMENT(indexType).Important();
  SERIALISE_ELEMENT(indexBuffer).Important();
  SERIALISE_ELEMENT(indexBufferOffset).Important();
  SERIALISE_ELEMENT(indirectBuffer).Important();
  SERIALISE_ELEMENT(indirectBufferOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    MTL::Buffer *realIndex = indexBuffer && indexBuffer->m_Type == eResBuffer
                                 ? Unwrap(indexBuffer) : NULL;
    MTL::Buffer *realIndirect = indirectBuffer && indirectBuffer->m_Type == eResBuffer
                                    ? Unwrap(indirectBuffer) : NULL;
    const uint64_t indexStride = indexType == MTL::IndexTypeUInt16 ? 2 : 4;
    const uint64_t argumentSize = sizeof(MTL::DrawIndexedPrimitivesIndirectArguments);
    if(!RenderCommandEncoder || !realIndex || !realIndirect ||
       (indexType != MTL::IndexTypeUInt16 && indexType != MTL::IndexTypeUInt32) ||
       indexBufferOffset % indexStride != 0 || indexBufferOffset > realIndex->length() ||
       (indirectBufferOffset & 3) != 0 || indirectBufferOffset > realIndirect->length() ||
       argumentSize > realIndirect->length() - indirectBufferOffset)
    {
      RDCERR("Invalid Metal indexed indirect buffers, alignment or argument offset");
      return false;
    }

    const bool privateArguments = realIndirect->storageMode() == MTL::StorageModePrivate;
    if(!privateArguments && realIndirect->contents() == NULL)
    {
      RDCERR("Metal indexed indirect arguments are not CPU accessible");
      return false;
    }

    MTL::DrawIndexedPrimitivesIndirectArguments args = {};
    const bool capturedArguments=IsLoading(m_State) && m_Device->m_HasCapturedRenderIndirectArguments;
    if(IsLoading(m_State) && !m_Device->GetReplay()->RegisterRenderIndirectAction(
        m_Device->GetReplay()->GetNextEventID(),RenderCommandEncoder,GetResID(indirectBuffer),
        indirectBufferOffset,5,(uint32_t *)&args))return false;
    if(!privateArguments && !capturedArguments)
    {
      memcpy(&args, (const byte *)realIndirect->contents() + indirectBufferOffset, sizeof(args));
    }
    if(!privateArguments || capturedArguments)
    {
      if((!capturedArguments && (!args.indexCount || !args.instanceCount)) ||
         args.indexStart > (realIndex->length() - indexBufferOffset) / indexStride ||
         args.indexCount > (realIndex->length() - indexBufferOffset) / indexStride - args.indexStart)
      {
        RDCERR("Invalid Metal indexed indirect indexStart/indexCount range");
        return false;
      }
    }

    const uint64_t selectedOffset = indexBufferOffset + uint64_t(args.indexStart) * indexStride;

    MetalReplay *replay = m_Device->GetReplay();
    if(!m_Device->GetReplay()->ValidateArgumentBufferBindings())
      return false;
    replay->SetPrimitiveTopology(primitiveType);
    replay->BindIndexBuffer(GetResID(indexBuffer), selectedOffset, indexType, args.indexCount);
    replay->SetIndirectBuffer(GetResID(indirectBuffer), indirectBufferOffset, argumentSize);
    Unwrap(RenderCommandEncoder)
        ->drawIndexedPrimitives(primitiveType, indexType, realIndex, indexBufferOffset,
                                realIndirect, indirectBufferOffset);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = privateArguments
                              ? "drawIndexedPrimitives(indirect, GPU-defined arguments)"
                              : StringFormat::Fmt(
                                    "drawIndexedPrimitives(indirect, %u indices, %u instances, indexStart %u, baseVertex %d, baseInstance %u)",
                                    args.indexCount, args.instanceCount, args.indexStart,
                                    args.baseVertex, args.baseInstance);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indexed | ActionFlags::Indirect;
      if(args.instanceCount > 1 || args.baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = args.indexCount;
      action.numInstances = args.instanceCount;
      // The Metal pipeline snapshot binds the selected index byte range above.
      // Action indices are relative to that binding, as for existing indexed draws.
      action.indexOffset = 0;
      action.baseVertex = args.baseVertex;
      action.instanceOffset = args.baseInstance;
      replay->SetActionOutputs(action);
      AddAction(action);
      replay->AddUsage(GetResID(indirectBuffer), ResourceUsage::Indirect);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::drawIndexedPrimitives(
    MTL::PrimitiveType primitiveType, MTL::IndexType indexType,
    WrappedMTLBuffer *indexBuffer, NS::UInteger indexBufferOffset,
    WrappedMTLBuffer *indirectBuffer, NS::UInteger indirectBufferOffset)
{
  SCOPED_READLOCK(m_Device->GetCaptureTransitionLock());
  CaptureIndirectArguments(indirectBuffer,indirectBufferOffset,5);

  SERIALISE_TIME_CALL(Unwrap(this)->drawIndexedPrimitives(
      primitiveType, indexType, Unwrap(indexBuffer), indexBufferOffset,
      Unwrap(indirectBuffer), indirectBufferOffset));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_indirect);
    Serialise_drawIndexedPrimitives(ser, primitiveType, indexType, indexBuffer,
                                    indexBufferOffset, indirectBuffer, indirectBufferOffset);
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(indexBuffer), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetResID(indirectBuffer), eFrameRef_Read);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawIndexedPrimitives,
                                MTL::PrimitiveType, MTL::IndexType, WrappedMTLBuffer *,
                                NS::UInteger, WrappedMTLBuffer *, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_executeCommandsMarker(
    SerialiserType &ser, WrappedMTLIndirectCommandBuffer *icb, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(icb).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       !icb || icb->m_Type != eResIndirectCommandBuffer || !range.length || range.location >= icb->Count() ||
       range.length > icb->Count() - range.location)
    {
      RDCERR("Invalid Metal ICB execute marker range or resource");
      return false;
    }
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription execute;
      execute.customName = StringFormat::Fmt("executeCommandsInBuffer(location=%llu, length=%llu)",
                                             (uint64_t)range.location, (uint64_t)range.length);
      execute.flags = ActionFlags::MultiAction | ActionFlags::PushMarker;
      AddAction(execute);
      m_Device->GetReplay()->BeginMultiAction((uint32_t)range.length);
    }
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_executeCommandsInBuffer(
    SerialiserType &ser, WrappedMTLIndirectCommandBuffer *icb, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(icb).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder) ||
       !icb || icb->m_Type != eResIndirectCommandBuffer || !icb->SupportedDescriptor() ||
       range.length != 1 || range.location >= icb->Count() || !icb->PrepareReplay())
    {
      RDCERR("Invalid Metal ICB execute range, object or descriptor");
      return false;
    }
    const MetalIndirectDraw &draw = icb->Draw(range.location);
    if(!draw.encoded)
    {
      // A reset/unencoded command is a real no-op, not a malformed draw. Keep one child per
      // execute slot for event selection without inventing a draw call or shader resource usage.
      Unwrap(RenderCommandEncoder)->executeCommandsInBuffer(Unwrap(icb), range);
      if(IsLoading(m_State))
      {
        AddEvent();
        ActionDescription action;
        action.customName = StringFormat::Fmt("ICB[%llu] empty command", (uint64_t)range.location);
        action.flags = ActionFlags::SetMarker;
        m_Device->GetReplay()->SetActionOutputs(action);
        AddAction(action);
        m_Device->GetReplay()->AddUsage(GetResID(icb), ResourceUsage::Indirect);
      }
      return true;
    }
    WrappedMTLRenderPipelineState *pipeline =
        icb->InheritPipelineState() ? m_EncoderPipeline : draw.pipeline;
    WrappedMTLBuffer *vertexBuffers[2] = {};
    NS::UInteger vertexOffsets[2] = {};
    for(unsigned slot = 0; slot < 2; ++slot)
    {
      vertexBuffers[slot] = icb->InheritBuffers() ? m_EncoderVertexBuffers[slot]
                                                   : draw.vertexBuffers[slot];
      vertexOffsets[slot] = icb->InheritBuffers() ? m_EncoderVertexOffsets[slot]
                                                  : draw.vertexBufferOffsets[slot];
    }
    if(!draw.encoded || !pipeline ||
       (icb->InheritPipelineState() && draw.pipeline) ||
       (icb->InheritBuffers() && (draw.vertexBuffers[0] || draw.vertexBuffers[1])) ||
       !vertexBuffers[0] || vertexOffsets[0] >= Unwrap(vertexBuffers[0])->length() ||
       !draw.instanceCount ||
       (draw.indexed &&
        (!vertexBuffers[1] ||
         vertexOffsets[1] >= Unwrap(vertexBuffers[1])->length() ||
         !draw.indexBuffer || !draw.indexCount)) ||
       (!draw.indexed && !draw.vertexCount))
    {
      RDCERR("Metal ICB command lacks pipeline, vertex buffer or draw arguments");
      return false;
    }
    MetalReplay *replay = m_Device->GetReplay();
    replay->BindRenderPipeline(GetResID(pipeline));
    replay->BindVertexBuffer(0, GetResID(vertexBuffers[0]), vertexOffsets[0]);
    if(vertexBuffers[1])
      replay->BindVertexBuffer(1, GetResID(vertexBuffers[1]), vertexOffsets[1]);
    if(draw.indexed)
    {
      replay->BindIndexBuffer(GetResID(draw.indexBuffer), draw.indexBufferOffset,
                              draw.indexType, draw.indexCount);
    }
    if(!m_Device->GetReplay()->ValidateArgumentBufferBindings())
      return false;
    replay->SetPrimitiveTopology(draw.primitive);
    replay->SetIndirectBuffer(ResourceId(), 0, 0);
    Unwrap(RenderCommandEncoder)->executeCommandsInBuffer(Unwrap(icb), range);

    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = draw.indexed
                              ? StringFormat::Fmt("ICB[%llu] drawIndexedPrimitives(%llu) instances=%llu",
                                                  (uint64_t)range.location,
                                                  (uint64_t)draw.indexCount,
                                                  (uint64_t)draw.instanceCount)
                              : StringFormat::Fmt("ICB[%llu] drawPrimitives(%llu) instances=%llu",
                                                  (uint64_t)range.location,
                                                  (uint64_t)draw.vertexCount,
                                                  (uint64_t)draw.instanceCount);
      action.flags = ActionFlags::Drawcall | ActionFlags::Indirect;
      if(draw.indexed)
        action.flags |= ActionFlags::Indexed;
      if(draw.instanceCount > 1 || draw.baseInstance > 0)
        action.flags |= ActionFlags::Instanced;
      action.numIndices = (uint32_t)(draw.indexed ? draw.indexCount : draw.vertexCount);
      action.numInstances = (uint32_t)draw.instanceCount;
      action.vertexOffset = (uint32_t)draw.vertexStart;
      action.baseVertex = (int32_t)draw.baseVertex;
      action.instanceOffset = (uint32_t)draw.baseInstance;
      replay->SetActionOutputs(action);
      AddAction(action);
      replay->AddUsage(GetResID(icb), ResourceUsage::Indirect);
    }
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::executeCommandsInBuffer(WrappedMTLIndirectCommandBuffer *icb,
                                                               NS::Range range)
{
  SERIALISE_TIME_CALL(Unwrap(this)->executeCommandsInBuffer(Unwrap(icb), range));
  if(IsCaptureMode(m_State))
  {
    if(!icb || !range.length || range.location >= icb->Count() ||
       range.length > icb->Count() - range.location)
    {
      RDCERR("Invalid Metal ICB capture execute range or object");
      return;
    }
    CACHE_THREAD_SERIALISER();
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    icb->CaptureReplayDependency(record);
    {
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_executeCommandsMarker);
      Serialise_executeCommandsMarker(ser, icb, range);
      record->AddChunk(scope.Get());
    }
    for(NS::UInteger index = range.location; index < range.location + range.length; ++index)
    {
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_executeCommandsInBuffer);
      Serialise_executeCommandsInBuffer(ser, icb, NS::Range::Make(index, 1));
      record->AddChunk(scope.Get());
    }
    record->MarkResourceFrameReferenced(GetResID(icb), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_executeCommandsMarker(
    ReadSerialiser &, WrappedMTLIndirectCommandBuffer *, NS::Range);
template bool WrappedMTLRenderCommandEncoder::Serialise_executeCommandsMarker(
    WriteSerialiser &, WrappedMTLIndirectCommandBuffer *, NS::Range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, executeCommandsInBuffer,
                                WrappedMTLIndirectCommandBuffer *, NS::Range);

template <typename SerialiserType>
bool WrappedMTLRenderCommandEncoder::Serialise_executeCommandsInBufferIndirect(
    SerialiserType &ser, WrappedMTLIndirectCommandBuffer *icb, WrappedMTLBuffer *rangeBuffer,
    NS::UInteger offset)
{
  SERIALISE_ELEMENT_LOCAL(RenderCommandEncoder, this);
  SERIALISE_ELEMENT(icb).Important();
  SERIALISE_ELEMENT(rangeBuffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!RenderCommandEncoder || RenderCommandEncoder->m_Type != eResRenderCommandEncoder || !RenderCommandEncoder->m_Real ||
       RenderCommandEncoder != m_Device->GetReplayRenderCommandEncoder(RenderCommandEncoder))
      return false;

    // The buffer may be written by the GPU after CPU encoding. No captured CPU value can prove
    // the executed range or the number and identity of child actions at this execution point.
    RDCERR("Unsupported Metal ICB GPU indirect execution range: execution-point range is unavailable");
    return false;
  }
  return true;
}

void WrappedMTLRenderCommandEncoder::executeCommandsInBufferIndirect(
    WrappedMTLIndirectCommandBuffer *icb, WrappedMTLBuffer *rangeBuffer, NS::UInteger offset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->executeCommandsInBuffer(Unwrap(icb), Unwrap(rangeBuffer), offset));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    MetalResourceRecord *record = GetRecord(m_CommandBuffer);
    if(icb) icb->CaptureReplayDependency(record);
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLRenderCommandEncoder_executeCommandsInBuffer_indirect);
    Serialise_executeCommandsInBufferIndirect(ser, icb, rangeBuffer, offset);
    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(icb), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetResID(rangeBuffer), eFrameRef_Read);
  }
}

template bool WrappedMTLRenderCommandEncoder::Serialise_executeCommandsInBufferIndirect(
    ReadSerialiser &, WrappedMTLIndirectCommandBuffer *, WrappedMTLBuffer *, NS::UInteger);
template bool WrappedMTLRenderCommandEncoder::Serialise_executeCommandsInBufferIndirect(
    WriteSerialiser &, WrappedMTLIndirectCommandBuffer *, WrappedMTLBuffer *, NS::UInteger);

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawIndexedPrimitives,
                                MTL::PrimitiveType primitiveType, NS::UInteger indexCount,
                                MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer,
                                NS::UInteger indexBufferOffset);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLRenderCommandEncoder, void, drawIndexedPrimitives,
                                MTL::PrimitiveType primitiveType, NS::UInteger indexCount,
                                MTL::IndexType indexType, WrappedMTLBuffer *indexBuffer,
                                NS::UInteger indexBufferOffset, NS::UInteger instanceCount,
                                NS::Integer baseVertex, NS::UInteger baseInstance);
