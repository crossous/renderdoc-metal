#include "metal_visible_function_table.h"
#include "metal_buffer.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_manager.h"

WrappedMTLFunctionHandle::WrappedMTLFunctionHandle(MTL::FunctionHandle *real, ResourceId id,
                                                   WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State)) AllocateObjCBridge(this);
}

WrappedMTLVisibleFunctionTable::WrappedMTLVisibleFunctionTable(MTL::VisibleFunctionTable *real,
                                                               ResourceId id, WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State)) AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLVisibleFunctionTable::Serialise_setFunction(SerialiserType &ser,
    WrappedMTLFunctionHandle *function, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(Table, this).Important();
  // Keep the serialized ID so an unknown non-null handle cannot become a legal
  // empty slot when pointer deserialization fails to find its resource.
  ResourceId functionId = GetResID(function);
  ser.Serialise("function"_lit, functionId).TypedAs("MTLFunctionHandle"_lit).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(functionId != ResourceId() && !GetResourceManager()->HasResource(functionId))
      return false;
    function = functionId == ResourceId() ? NULL :
        (WrappedMTLFunctionHandle *)GetResourceManager()->GetResource(functionId);
    if(!Table || Table->m_Type != eResVisibleFunctionTable || !Table->m_Real ||
       index >= Table->m_FunctionCount ||
       (function && (function->m_Type != eResFunctionHandle || !function->m_Real ||
                     !function->m_Function || !function->m_Function->m_Real ||
                     Unwrap(function->m_Function)->functionType() != MTL::FunctionTypeVisible ||
                     function->m_Pipeline != Table->m_Pipeline ||
                     function->m_Stage != Table->m_Stage)))
    {
      RDCERR("Invalid Metal visible-function-table update");
      return false;
    }
    Unwrap(Table)->setFunction(Unwrap(function), index);
    Table->m_RayIRFunctions[index] = functionId;
  }
  return true;
}

void WrappedMTLVisibleFunctionTable::setFunction(WrappedMTLFunctionHandle *function,
                                                 uint32_t index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFunction(Unwrap(function), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLVisibleFunctionTable_setFunction);
    Serialise_setFunction(ser, function, index);
    MetalResourceRecord *record = GetRecord(this);
    if(function) record->AddParent(GetRecord(function));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLVisibleFunctionTable::Serialise_setFunction(
    ReadSerialiser &, WrappedMTLFunctionHandle *, uint32_t);
template bool WrappedMTLVisibleFunctionTable::Serialise_setFunction(
    WriteSerialiser &, WrappedMTLFunctionHandle *, uint32_t);

WrappedMTLIntersectionFunctionTable::WrappedMTLIntersectionFunctionTable(
    MTL::IntersectionFunctionTable *real, ResourceId id, WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State)) AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLIntersectionFunctionTable::Serialise_setFunction(
    SerialiserType &ser, WrappedMTLFunctionHandle *function, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(Table, this).Important();
  // Keep the serialized ID so an unknown non-null handle cannot become a legal
  // empty slot when pointer deserialization fails to find its resource.
  ResourceId functionId = GetResID(function);
  ser.Serialise("function"_lit, functionId).TypedAs("MTLFunctionHandle"_lit).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(functionId != ResourceId() && !GetResourceManager()->HasResource(functionId))
      return false;
    function = functionId == ResourceId() ? NULL :
        (WrappedMTLFunctionHandle *)GetResourceManager()->GetResource(functionId);
    if(!Table || Table->m_Type != eResIntersectionFunctionTable || !Table->m_Real ||
       index >= Table->m_FunctionCount ||
       (function && (function->m_Type != eResFunctionHandle || !function->m_Real ||
                     !function->m_Function || !function->m_Function->m_Real ||
                     Unwrap(function->m_Function)->functionType() != MTL::FunctionTypeIntersection ||
                     function->m_Pipeline != Table->m_Pipeline ||
                     function->m_Stage != Table->m_Stage)))
    {
      RDCERR("Invalid Metal intersection-function-table update");
      return false;
    }
    Unwrap(Table)->setFunction(Unwrap(function), index);
    Table->m_RayIRFunctions[index] = functionId;
  }
  return true;
}

void WrappedMTLIntersectionFunctionTable::setFunction(WrappedMTLFunctionHandle *function,
                                                       uint32_t index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setFunction(Unwrap(function), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIntersectionFunctionTable_setFunction);
    Serialise_setFunction(ser, function, index);
    MetalResourceRecord *record = GetRecord(this);
    if(function) record->AddParent(GetRecord(function));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLIntersectionFunctionTable::Serialise_setFunction(
    ReadSerialiser &, WrappedMTLFunctionHandle *, uint32_t);
template bool WrappedMTLIntersectionFunctionTable::Serialise_setFunction(
    WriteSerialiser &, WrappedMTLFunctionHandle *, uint32_t);

static bool ValidIntersectionBuffer(WrappedMTLIntersectionFunctionTable *table,
                                    WrappedMTLBuffer *buffer, NS::UInteger offset)
{
  if(!table || table->m_Type != eResIntersectionFunctionTable || !table->m_Real ||
     !table->m_Pipeline)
    return false;
  if(!buffer) return offset == 0;
  return buffer->m_Type == eResBuffer && buffer->m_Real &&
         buffer->m_Device == table->m_Device && offset <= Unwrap(buffer)->length() &&
         Unwrap(buffer)->length() - offset >= 4;
}

template <typename SerialiserType>
bool WrappedMTLIntersectionFunctionTable::Serialise_setBuffer(
    SerialiserType &ser, WrappedMTLBuffer *buffer, NS::UInteger offset, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(Table, this).Important();
  SERIALISE_ELEMENT(buffer).Important();
  ResourceId bufferId = GetResID(buffer);
  SERIALISE_ELEMENT(bufferId).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(index >= 31 || GetResID(buffer) != bufferId ||
       !ValidIntersectionBuffer(Table, buffer, offset))
    {
      RDCERR("Invalid Metal intersection function buffer update");
      return false;
    }
    Unwrap(Table)->setBuffer(Unwrap(buffer), offset, index);
  }
  return true;
}

void WrappedMTLIntersectionFunctionTable::setBuffer(WrappedMTLBuffer *buffer,
                                                    NS::UInteger offset, uint32_t index)
{
  if(index >= 31 || !ValidIntersectionBuffer(this, buffer, offset))
  {
    RDCERR("Unsupported Metal intersection function buffer update");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->setBuffer(Unwrap(buffer), offset, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIntersectionFunctionTable_setBuffer);
    Serialise_setBuffer(ser, buffer, offset, index);
    MetalResourceRecord *record = GetRecord(this);
    if(buffer) record->AddParent(GetRecord(buffer));
    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLIntersectionFunctionTable::Serialise_setBuffers(
    SerialiserType &ser, rdcarray<WrappedMTLBuffer *> buffers,
    rdcarray<NS::UInteger> offsets, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(Table, this).Important();
  SERIALISE_ELEMENT(buffers).Important();
  rdcarray<ResourceId> bufferIds;
  for(WrappedMTLBuffer *buffer : buffers) bufferIds.push_back(GetResID(buffer));
  SERIALISE_ELEMENT(bufferIds).Important();
  SERIALISE_ELEMENT(offsets).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(range.location >= 31 || !range.length || range.length > 31 - range.location ||
       buffers.size() != range.length || offsets.size() != range.length ||
       bufferIds.size() != range.length)
    {
      RDCERR("Invalid Metal intersection function buffer range");
      return false;
    }
    rdcarray<const MTL::Buffer *> real;
    for(size_t i = 0; i < buffers.size(); i++)
    {
      if(GetResID(buffers[i]) != bufferIds[i] ||
         !ValidIntersectionBuffer(Table, buffers[i], offsets[i]))
      {
        RDCERR("Invalid Metal intersection function buffer range resource");
        return false;
      }
      real.push_back(Unwrap(buffers[i]));
    }
    Unwrap(Table)->setBuffers(real.data(), offsets.data(), range);
  }
  return true;
}

void WrappedMTLIntersectionFunctionTable::setBuffers(
    rdcarray<WrappedMTLBuffer *> buffers, rdcarray<NS::UInteger> offsets, NS::Range range)
{
  if(range.location >= 31 || !range.length || range.length > 31 - range.location ||
     buffers.size() != range.length || offsets.size() != range.length)
  {
    RDCERR("Unsupported Metal intersection function buffer range");
    return;
  }
  rdcarray<const MTL::Buffer *> real;
  for(size_t i = 0; i < buffers.size(); i++)
  {
    if(!ValidIntersectionBuffer(this, buffers[i], offsets[i]))
    {
      RDCERR("Unsupported Metal intersection function buffer range resource");
      return;
    }
    real.push_back(Unwrap(buffers[i]));
  }
  SERIALISE_TIME_CALL(Unwrap(this)->setBuffers(real.data(), offsets.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIntersectionFunctionTable_setBuffers);
    Serialise_setBuffers(ser, buffers, offsets, range);
    MetalResourceRecord *record = GetRecord(this);
    for(WrappedMTLBuffer *buffer : buffers)
      if(buffer) record->AddParent(GetRecord(buffer));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLIntersectionFunctionTable::Serialise_setBuffer(
    ReadSerialiser &, WrappedMTLBuffer *, NS::UInteger, uint32_t);
template bool WrappedMTLIntersectionFunctionTable::Serialise_setBuffer(
    WriteSerialiser &, WrappedMTLBuffer *, NS::UInteger, uint32_t);
template bool WrappedMTLIntersectionFunctionTable::Serialise_setBuffers(
    ReadSerialiser &, rdcarray<WrappedMTLBuffer *>, rdcarray<NS::UInteger>, NS::Range);
template bool WrappedMTLIntersectionFunctionTable::Serialise_setBuffers(
    WriteSerialiser &, rdcarray<WrappedMTLBuffer *>, rdcarray<NS::UInteger>, NS::Range);

static bool ValidNestedVisibleTable(WrappedMTLIntersectionFunctionTable *outer,
                                    WrappedMTLVisibleFunctionTable *nested)
{
  if(!outer || outer->m_Type != eResIntersectionFunctionTable || !outer->m_Real ||
     !outer->m_Pipeline)
    return false;
  if(!nested) return true;
  return nested->m_Type == eResVisibleFunctionTable && nested->m_Real &&
         nested->m_Device == outer->m_Device && nested->m_Pipeline == outer->m_Pipeline &&
         nested->m_Stage == outer->m_Stage;
}

template <typename SerialiserType>
bool WrappedMTLIntersectionFunctionTable::Serialise_setVisibleFunctionTable(
    SerialiserType &ser, WrappedMTLVisibleFunctionTable *visibleTable, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(Table, this).Important();
  SERIALISE_ELEMENT(visibleTable).Important();
  ResourceId visibleTableId = GetResID(visibleTable);
  SERIALISE_ELEMENT(visibleTableId).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(index >= 31 || GetResID(visibleTable) != visibleTableId ||
       !ValidNestedVisibleTable(Table, visibleTable))
    {
      RDCERR("Invalid nested Metal visible function table binding");
      return false;
    }
    Unwrap(Table)->setVisibleFunctionTable(Unwrap(visibleTable), index);
  }
  return true;
}

void WrappedMTLIntersectionFunctionTable::setVisibleFunctionTable(
    WrappedMTLVisibleFunctionTable *visibleTable, uint32_t index)
{
  if(index >= 31 || !ValidNestedVisibleTable(this, visibleTable))
  {
    RDCERR("Unsupported nested Metal visible function table binding");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->setVisibleFunctionTable(Unwrap(visibleTable), index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIntersectionFunctionTable_setVisibleFunctionTable);
    Serialise_setVisibleFunctionTable(ser, visibleTable, index);
    MetalResourceRecord *record = GetRecord(this);
    if(visibleTable) record->AddParent(GetRecord(visibleTable));
    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLIntersectionFunctionTable::Serialise_setVisibleFunctionTables(
    SerialiserType &ser, rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(Table, this).Important();
  SERIALISE_ELEMENT(tables).Important();
  rdcarray<ResourceId> tableIds;
  for(WrappedMTLVisibleFunctionTable *table : tables) tableIds.push_back(GetResID(table));
  SERIALISE_ELEMENT(tableIds).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(range.location >= 31 || !range.length || range.length > 31 - range.location ||
       tables.size() != range.length || tableIds.size() != range.length)
    {
      RDCERR("Invalid nested Metal visible function table range");
      return false;
    }
    rdcarray<const MTL::VisibleFunctionTable *> real;
    for(size_t i = 0; i < tables.size(); i++)
    {
      if(GetResID(tables[i]) != tableIds[i] || !ValidNestedVisibleTable(Table, tables[i]))
      {
        RDCERR("Invalid nested Metal visible function table range member");
        return false;
      }
      real.push_back(Unwrap(tables[i]));
    }
    Unwrap(Table)->setVisibleFunctionTables(real.data(), range);
  }
  return true;
}

void WrappedMTLIntersectionFunctionTable::setVisibleFunctionTables(
    rdcarray<WrappedMTLVisibleFunctionTable *> tables, NS::Range range)
{
  if(range.location >= 31 || !range.length || range.length > 31 - range.location ||
     tables.size() != range.length)
  {
    RDCERR("Unsupported nested Metal visible function table range");
    return;
  }
  rdcarray<const MTL::VisibleFunctionTable *> real;
  for(WrappedMTLVisibleFunctionTable *table : tables)
  {
    if(!ValidNestedVisibleTable(this, table))
    {
      RDCERR("Unsupported nested Metal visible function table range member");
      return;
    }
    real.push_back(Unwrap(table));
  }
  SERIALISE_TIME_CALL(Unwrap(this)->setVisibleFunctionTables(real.data(), range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIntersectionFunctionTable_setVisibleFunctionTables);
    Serialise_setVisibleFunctionTables(ser, tables, range);
    MetalResourceRecord *record = GetRecord(this);
    for(WrappedMTLVisibleFunctionTable *table : tables)
      if(table) record->AddParent(GetRecord(table));
    record->AddChunk(scope.Get());
  }
}

template bool WrappedMTLIntersectionFunctionTable::Serialise_setVisibleFunctionTable(
    ReadSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t);
template bool WrappedMTLIntersectionFunctionTable::Serialise_setVisibleFunctionTable(
    WriteSerialiser &, WrappedMTLVisibleFunctionTable *, uint32_t);
template bool WrappedMTLIntersectionFunctionTable::Serialise_setVisibleFunctionTables(
    ReadSerialiser &, rdcarray<WrappedMTLVisibleFunctionTable *>, NS::Range);
template bool WrappedMTLIntersectionFunctionTable::Serialise_setVisibleFunctionTables(
    WriteSerialiser &, rdcarray<WrappedMTLVisibleFunctionTable *>, NS::Range);

static bool SupportedOpaqueTriangleTable(WrappedMTLIntersectionFunctionTable *table,
                                         MTL::IntersectionFunctionSignature signature)
{
  const uint64_t expected = MTL::IntersectionFunctionSignatureInstancing |
                            MTL::IntersectionFunctionSignatureTriangleData;
  return table && table->m_Type == eResIntersectionFunctionTable && table->m_Real &&
         table->m_Pipeline && table->m_Stage == MTL::RenderStageFragment &&
         table->m_FunctionCount == 1 && (uint64_t)signature == expected;
}

template <typename SerialiserType>
bool WrappedMTLIntersectionFunctionTable::Serialise_setOpaqueTriangleFunction(
    SerialiserType &ser, MTL::IntersectionFunctionSignature signature, uint32_t index)
{
  SERIALISE_ELEMENT_LOCAL(Table, this).Important();
  uint64_t signatureValue = (uint64_t)signature;
  SERIALISE_ELEMENT(signatureValue).Important();
  SERIALISE_ELEMENT(index).Important();
  signature = (MTL::IntersectionFunctionSignature)signatureValue;
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!SupportedOpaqueTriangleTable(Table, signature) || index != 0)
    {
      RDCERR("Invalid Metal opaque-triangle intersection table update");
      return false;
    }
    Unwrap(Table)->setOpaqueTriangleIntersectionFunction(signature, index);
  }
  return true;
}

void WrappedMTLIntersectionFunctionTable::setOpaqueTriangleFunction(
    MTL::IntersectionFunctionSignature signature, uint32_t index)
{
  if(!SupportedOpaqueTriangleTable(this, signature) || index != 0)
  {
    RDCERR("Unsupported Metal opaque-triangle intersection table update");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->setOpaqueTriangleIntersectionFunction(signature, index));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIntersectionFunctionTable_setOpaqueTriangleFunction);
    Serialise_setOpaqueTriangleFunction(ser, signature, index);
    GetRecord(this)->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLIntersectionFunctionTable::Serialise_setOpaqueTriangleFunctions(
    SerialiserType &ser, MTL::IntersectionFunctionSignature signature, NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(Table, this).Important();
  uint64_t signatureValue = (uint64_t)signature;
  SERIALISE_ELEMENT(signatureValue).Important();
  SERIALISE_ELEMENT(range).Important();
  signature = (MTL::IntersectionFunctionSignature)signatureValue;
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!SupportedOpaqueTriangleTable(Table, signature) ||
       range.location != 0 || range.length != 1)
    {
      RDCERR("Invalid Metal opaque-triangle intersection table range update");
      return false;
    }
    Unwrap(Table)->setOpaqueTriangleIntersectionFunction(signature, range);
  }
  return true;
}

void WrappedMTLIntersectionFunctionTable::setOpaqueTriangleFunctions(
    MTL::IntersectionFunctionSignature signature, NS::Range range)
{
  if(!SupportedOpaqueTriangleTable(this, signature) ||
     range.location != 0 || range.length != 1)
  {
    RDCERR("Unsupported Metal opaque-triangle intersection table range update");
    return;
  }
  SERIALISE_TIME_CALL(Unwrap(this)->setOpaqueTriangleIntersectionFunction(signature, range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLIntersectionFunctionTable_setOpaqueTriangleFunctions);
    Serialise_setOpaqueTriangleFunctions(ser, signature, range);
    GetRecord(this)->AddChunk(scope.Get());
  }
}

template bool WrappedMTLIntersectionFunctionTable::Serialise_setOpaqueTriangleFunction(
    ReadSerialiser &, MTL::IntersectionFunctionSignature, uint32_t);
template bool WrappedMTLIntersectionFunctionTable::Serialise_setOpaqueTriangleFunction(
    WriteSerialiser &, MTL::IntersectionFunctionSignature, uint32_t);
template bool WrappedMTLIntersectionFunctionTable::Serialise_setOpaqueTriangleFunctions(
    ReadSerialiser &, MTL::IntersectionFunctionSignature, NS::Range);
template bool WrappedMTLIntersectionFunctionTable::Serialise_setOpaqueTriangleFunctions(
    WriteSerialiser &, MTL::IntersectionFunctionSignature, NS::Range);
