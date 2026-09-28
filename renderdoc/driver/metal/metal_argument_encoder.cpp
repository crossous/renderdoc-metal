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

#include "metal_argument_encoder.h"
#include "metal_buffer.h"
#include "metal_device.h"
#include "metal_manager.h"
#include "metal_replay.h"
#include "metal_sampler_state.h"
#include "metal_texture.h"
#include "metal_visible_function_table.h"

WrappedMTLArgumentEncoder::WrappedMTLArgumentEncoder(MTL::ArgumentEncoder *realArgumentEncoder,
                                                     ResourceId objId,
                                                     WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realArgumentEncoder, objId, wrappedMTLDevice,
                       wrappedMTLDevice->GetStateRef())
{
  if(realArgumentEncoder && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

MTL::ArrayType *MetalArgumentArray(MTL::StructMember *member)
{
  if(member->dataType() == MTL::DataTypeArray)
    return member->arrayType();
  // metal::array<device T *, N> is reflected as a transparent struct containing one array.
  // Inspect public reflection shape, not compiler-generated member names or object storage.
  if(member->dataType() == MTL::DataTypeStruct)
  {
    auto structure = member->structType();
    auto members = structure ? structure->members() : NULL;
    if(members && members->count() == 1)
    {
      auto child = members->object<MTL::StructMember>(0);
      if(child->dataType() == MTL::DataTypeArray && child->offset() == 0 && child->argumentIndex() == 0)
        return child->arrayType();
    }
  }
  return NULL;
}

void WrappedMTLArgumentEncoder::ConfigureLayout(MTL::Argument *reflection)
{
  MTL::StructType *structure = reflection ? reflection->bufferStructType() : NULL;
  NS::Array *members = structure ? structure->members() : NULL;
  for(NS::UInteger i = 0; members && i < members->count(); i++)
  {
    MTL::StructMember *member = members->object<MTL::StructMember>(i);
    const NS::UInteger index = member->argumentIndex();
    if(index >= ARRAY_COUNT(m_MemberTypes))
      continue;
    MTL::ArrayType *array = MetalArgumentArray(member);
    MTL::PointerType *pointer = array && array->elementType() == MTL::DataTypePointer
                                  ? array->elementPointerType()
                                  : member->dataType() == MTL::DataTypePointer ? member->pointerType() : NULL;
    const bool bufferPointer = pointer && pointer->access() == MTL::BindingAccessReadOnly;
    const auto setMember = [&](NS::UInteger slot, MTL::DataType type) {
      m_MemberTypes[slot] = type;
      if(bufferPointer)
      {
        m_BufferSizes[slot] = RDCMAX((uint64_t)1, (uint64_t)pointer->dataSize());
        m_BufferAlignments[slot] = RDCMAX((uint64_t)1, (uint64_t)pointer->alignment());
      }
    };
    if(array)
    {
      const NS::UInteger stride = array->argumentIndexStride();
      if(stride == 0 || array->arrayLength() > ARRAY_COUNT(m_MemberTypes) ||
         (array->arrayLength() > 1 &&
          stride > (ARRAY_COUNT(m_MemberTypes) - 1 - index) / (array->arrayLength() - 1)))
        continue;
      for(NS::UInteger j = 0; j < array->arrayLength(); j++)
        setMember(index + j * stride, array->elementType());
    }
    else
      setMember(index, member->dataType());
    if(!array && pointer && pointer->elementIsArgumentBuffer())
    {
      NestedLayout layout;
      layout.index = (uint32_t)index;
      layout.size = pointer->dataSize();
      layout.alignment = pointer->alignment();
      MTL::StructType *child = pointer->elementStructType();
      NS::Array *childMembers = child ? child->members() : NULL;
      layout.supported = bufferPointer && layout.size > 0 && layout.alignment > 0 &&
                         childMembers && childMembers->count() > 0 && childMembers->count() <= 8;
      bool seen[32] = {};
      for(NS::UInteger j = 0; layout.supported && j < childMembers->count(); j++)
      {
        MTL::StructMember *childMember = childMembers->object<MTL::StructMember>(j);
        const NS::UInteger childIndex = childMember->argumentIndex();
        const MTL::DataType childType = childMember->dataType();
        if(childIndex >= 32 || seen[childIndex] ||
           (childType != MTL::DataTypeTexture && childType != MTL::DataTypeSampler))
          layout.supported = false;
        else
        {
          layout.memberTypes[childIndex] = childType;
          seen[childIndex] = true;
        }
      }
      m_NestedLayouts.push_back(layout);
    }
  }
}

const WrappedMTLArgumentEncoder::NestedLayout *WrappedMTLArgumentEncoder::FindNestedLayout(
    NS::UInteger index) const
{
  for(const NestedLayout &layout : m_NestedLayouts)
    if(layout.index == index)
      return &layout;
  return NULL;
}

void WrappedMTLArgumentEncoder::ConfigureNestedLayout(const NestedLayout &layout)
{
  for(size_t i = 0; i < ARRAY_COUNT(m_MemberTypes); i++)
    m_MemberTypes[i] = layout.memberTypes[i];
}

void WrappedMTLArgumentEncoder::ConfigureDescriptorLayout(const rdcarray<uint64_t> &descriptors)
{
  for(size_t i = 0; i + 5 < descriptors.size(); i += 6)
  {
    const uint64_t base = descriptors[i], length = descriptors[i + 2];
    if(base >= ARRAY_COUNT(m_MemberTypes) || length > ARRAY_COUNT(m_MemberTypes) - base)
      continue;
    for(uint64_t member = 0; member < length; ++member)
      m_MemberTypes[base + member] = MTL::DataType(descriptors[i + 1]);
  }
}

template <typename SerialiserType>
bool WrappedMTLArgumentEncoder::Serialise_newArgumentEncoder(
    SerialiserType &ser, WrappedMTLArgumentEncoder *encoder, NS::UInteger index,
    uint64_t encodedLength, uint64_t alignment, bool supported)
{
  SERIALISE_ELEMENT_LOCAL(ParentEncoder, this).Important();
  SERIALISE_ELEMENT_LOCAL(Encoder, GetResID(encoder)).TypedAs("MTLArgumentEncoder"_lit).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_ELEMENT(encodedLength).Important();
  SERIALISE_ELEMENT(alignment).Important();
  SERIALISE_ELEMENT(supported).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ParentEncoder || ParentEncoder->m_Type != eResArgumentEncoder ||
       !ParentEncoder->m_Real || index >= 32 || Encoder == ResourceId() ||
       GetResourceManager()->HasResource(Encoder) || !supported)
    {
      RDCERR("Invalid or unsupported nested Metal argument encoder identity or index");
      return false;
    }
    const NestedLayout *layout = ParentEncoder->FindNestedLayout(index);
    if(!layout || !layout->supported || encodedLength != layout->size ||
       alignment != layout->alignment)
    {
      RDCERR("Unsupported nested Metal argument encoder reflection layout");
      return false;
    }
    MTL::ArgumentEncoder *real = Unwrap(ParentEncoder)->newArgumentEncoder(index);
    if(!real || real->encodedLength() != encodedLength || real->alignment() != alignment)
    {
      if(real) real->release();
      RDCERR("Metal failed to recreate nested argument encoder layout");
      return false;
    }
    WrappedMTLArgumentEncoder *wrapped = NULL;
    GetResourceManager()->WrapResource(Encoder, real, wrapped, true);
    wrapped->ConfigureNestedLayout(*layout);
    m_Device->AddResource(Encoder, ResourceType::StateObject, "Nested Argument Encoder");
    m_Device->DerivedResource(ParentEncoder, Encoder);
  }
  return true;
}

WrappedMTLArgumentEncoder *WrappedMTLArgumentEncoder::newArgumentEncoder(NS::UInteger index)
{
  MTL::ArgumentEncoder *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newArgumentEncoder(index));
  if(!real) return NULL;
  WrappedMTLArgumentEncoder *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  const NestedLayout *layout = FindNestedLayout(index);
  const bool supported = layout && layout->supported &&
                         real->encodedLength() == layout->size &&
                         real->alignment() == layout->alignment;
  if(supported)
    wrapped->ConfigureNestedLayout(*layout);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLArgumentEncoder_newArgumentEncoderForBufferAtIndex);
    Serialise_newArgumentEncoder(ser, wrapped, index, real->encodedLength(),
                                real->alignment(), supported);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLArgumentEncoder::Serialise_newArgumentEncoder(ReadSerialiser &,
    WrappedMTLArgumentEncoder *, NS::UInteger, uint64_t, uint64_t, bool);
template bool WrappedMTLArgumentEncoder::Serialise_newArgumentEncoder(WriteSerialiser &,
    WrappedMTLArgumentEncoder *, NS::UInteger, uint64_t, uint64_t, bool);

bool WrappedMTLArgumentEncoder::SelectReplayBuffer(WrappedMTLBuffer *buffer, uint64_t offset)
{
  if(!buffer || buffer->m_Type != eResBuffer || !buffer->m_Real ||
     Unwrap(buffer)->storageMode() != MTL::StorageModeShared ||
     offset > Unwrap(buffer)->length() ||
     Unwrap(this)->encodedLength() > Unwrap(buffer)->length() - offset ||
     (Unwrap(this)->alignment() && offset % Unwrap(this)->alignment()))
  {
    RDCERR("Invalid Metal argument-buffer selection identity, storage or range");
    return false;
  }
  if(!m_Device->GetReplay()->RegisterArgumentBuffer(this, GetResID(buffer), offset))
    return false;
  Unwrap(this)->setArgumentBuffer(Unwrap(buffer), offset);
  m_ArgumentBuffer = buffer;
  m_ArgumentOffset = offset;
  return true;
}

template <typename SerialiserType>
bool WrappedMTLArgumentEncoder::Serialise_setArgumentBuffer(SerialiserType &ser,
                                                            WrappedMTLBuffer *argumentBuffer,
                                                            NS::UInteger offset)
{
  SERIALISE_ELEMENT_LOCAL(ArgumentEncoder, this);
  SERIALISE_ELEMENT(argumentBuffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ArgumentEncoder || ArgumentEncoder->m_Type != eResArgumentEncoder ||
       !ArgumentEncoder->m_Real)
    {
      RDCERR("Invalid Metal argument-buffer selection identity, storage or range");
      return false;
    }
    return ArgumentEncoder->SelectReplayBuffer(argumentBuffer, offset);
  }
  return true;
}

void WrappedMTLArgumentEncoder::setArgumentBuffer(WrappedMTLBuffer *argumentBuffer,
                                                  NS::UInteger offset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setArgumentBuffer(Unwrap(argumentBuffer), offset));
  m_ArgumentBuffer = argumentBuffer;
  m_ArgumentOffset = offset;
  if(IsCaptureMode(m_State) && argumentBuffer)
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLArgumentEncoder_setArgumentBuffer);
    Serialise_setArgumentBuffer(ser, argumentBuffer, offset);
    MetalResourceRecord *record = GetRecord(argumentBuffer);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLArgumentEncoder::Serialise_setTexture(SerialiserType &ser,
                                                     WrappedMTLTexture *texture,
                                                     NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ArgumentEncoder, this);
  SERIALISE_ELEMENT_LOCAL(textureId, GetResID(texture)).Named("texture"_lit)
      .TypedAs("MTLTexture"_lit).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ArgumentEncoder || ArgumentEncoder->m_Type != eResArgumentEncoder ||
       !ArgumentEncoder->m_Real || !ArgumentEncoder->m_ArgumentBuffer || index >= 32 ||
       ArgumentEncoder->m_MemberTypes[index] != MTL::DataTypeTexture ||
       (textureId != ResourceId() && !GetResourceManager()->HasResource(textureId)))
    {
      RDCERR("Invalid Metal argument texture identity, selection or member index");
      return false;
    }
    texture = textureId == ResourceId() ? NULL :
        (WrappedMTLTexture *)GetResourceManager()->GetResource(textureId);
    if(texture && (texture->m_Type != eResTexture || !texture->m_Real))
    {
      RDCERR("Invalid Metal argument texture resource type");
      return false;
    }
    Unwrap(ArgumentEncoder)->setTexture(Unwrap(texture), index);
    m_Device->GetReplay()->SetArgumentBufferTexture(
        GetResID(ArgumentEncoder->m_ArgumentBuffer), ArgumentEncoder->m_ArgumentOffset,
        (uint32_t)index, GetResID(texture));
  }
  return true;
}

void WrappedMTLArgumentEncoder::setTexture(WrappedMTLTexture *texture, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTexture(Unwrap(texture), index));
  if(IsCaptureMode(m_State))
  {
    if(!CheckCaptureMutation())
      return;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLArgumentEncoder_setTexture);
    Serialise_setTexture(ser, texture, index);
    MetalResourceRecord *record = GetRecord(m_ArgumentBuffer);
    record->AddChunk(scope.Get());
    if(texture)
      record->AddParent(GetRecord(texture));
  }
}

template <typename SerialiserType>
bool WrappedMTLArgumentEncoder::Serialise_setSamplerState(SerialiserType &ser,
                                                          WrappedMTLSamplerState *sampler,
                                                          NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ArgumentEncoder, this);
  SERIALISE_ELEMENT_LOCAL(samplerId, GetResID(sampler)).Named("sampler"_lit)
      .TypedAs("MTLSamplerState"_lit).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ArgumentEncoder || ArgumentEncoder->m_Type != eResArgumentEncoder ||
       !ArgumentEncoder->m_Real || !ArgumentEncoder->m_ArgumentBuffer || index >= 32 ||
       ArgumentEncoder->m_MemberTypes[index] != MTL::DataTypeSampler ||
       (samplerId != ResourceId() && !GetResourceManager()->HasResource(samplerId)))
    {
      RDCERR("Invalid Metal argument sampler identity, selection or member index");
      return false;
    }
    sampler = samplerId == ResourceId() ? NULL :
        (WrappedMTLSamplerState *)GetResourceManager()->GetResource(samplerId);
    if(sampler && (sampler->m_Type != eResSamplerState || !sampler->m_Real))
    {
      RDCERR("Invalid Metal argument sampler resource type");
      return false;
    }
    Unwrap(ArgumentEncoder)->setSamplerState(Unwrap(sampler), index);
    m_Device->GetReplay()->SetArgumentBufferSampler(
        GetResID(ArgumentEncoder->m_ArgumentBuffer), ArgumentEncoder->m_ArgumentOffset,
        (uint32_t)index, GetResID(sampler));
  }
  return true;
}

void WrappedMTLArgumentEncoder::setSamplerState(WrappedMTLSamplerState *sampler,
                                                NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setSamplerState(Unwrap(sampler), index));
  if(IsCaptureMode(m_State))
  {
    if(!CheckCaptureMutation())
      return;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLArgumentEncoder_setSamplerState);
    Serialise_setSamplerState(ser, sampler, index);
    MetalResourceRecord *record = GetRecord(m_ArgumentBuffer);
    record->AddChunk(scope.Get());
    if(sampler)
      record->AddParent(GetRecord(sampler));
  }
}

template <typename SerialiserType>
bool WrappedMTLArgumentEncoder::Serialise_setVisibleFunctionTable(
    SerialiserType &ser, WrappedMTLVisibleFunctionTable *table, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ArgumentEncoder, this).Important();
  SERIALISE_ELEMENT_LOCAL(tableId, GetResID(table)).Named("table"_lit)
      .TypedAs("MTLVisibleFunctionTable"_lit).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ArgumentEncoder || ArgumentEncoder->m_Type != eResArgumentEncoder ||
       !ArgumentEncoder->m_Real || !ArgumentEncoder->m_ArgumentBuffer || index >= 32 ||
       ArgumentEncoder->m_MemberTypes[index] != MTL::DataTypeVisibleFunctionTable ||
       (tableId != ResourceId() && !GetResourceManager()->HasResource(tableId)))
    {
      RDCERR("Invalid Metal argument visible-function-table identity or member");
      return false;
    }
    table = tableId == ResourceId() ? NULL :
        (WrappedMTLVisibleFunctionTable *)GetResourceManager()->GetResource(tableId);
    if(table && (table->m_Type != eResVisibleFunctionTable || !table->m_Real))
    {
      RDCERR("Invalid Metal argument visible-function-table resource type");
      return false;
    }
    Unwrap(ArgumentEncoder)->setVisibleFunctionTable(Unwrap(table), index);
  }
  return true;
}

void WrappedMTLArgumentEncoder::setVisibleFunctionTable(
    WrappedMTLVisibleFunctionTable *table, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setVisibleFunctionTable(Unwrap(table), index));
  if(IsCaptureMode(m_State))
  {
    if(!CheckCaptureMutation()) return;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLArgumentEncoder_setVisibleFunctionTable);
    Serialise_setVisibleFunctionTable(ser, table, index);
    MetalResourceRecord *record = GetRecord(m_ArgumentBuffer);
    record->AddChunk(scope.Get());
    if(table) record->AddParent(GetRecord(table));
  }
}

template bool WrappedMTLArgumentEncoder::Serialise_setVisibleFunctionTable(
    ReadSerialiser &, WrappedMTLVisibleFunctionTable *, NS::UInteger);
template bool WrappedMTLArgumentEncoder::Serialise_setVisibleFunctionTable(
    WriteSerialiser &, WrappedMTLVisibleFunctionTable *, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLArgumentEncoder::Serialise_setArgumentBufferArray(
    SerialiserType &ser, WrappedMTLBuffer *argumentBuffer, NS::UInteger startOffset,
    NS::UInteger arrayElement)
{
  SERIALISE_ELEMENT_LOCAL(ArgumentEncoder, this);
  SERIALISE_ELEMENT(argumentBuffer).Important();
  SERIALISE_ELEMENT(startOffset).Important();
  SERIALISE_ELEMENT(arrayElement).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ArgumentEncoder || ArgumentEncoder->m_Type != eResArgumentEncoder ||
       !ArgumentEncoder->m_Real)
      return false;
    const uint64_t stride = Unwrap(ArgumentEncoder)->encodedLength();
    if(stride == 0 || arrayElement > (UINT64_MAX - startOffset) / stride)
    {
      RDCERR("Invalid Metal argument-buffer array offset overflow");
      return false;
    }
    return ArgumentEncoder->SelectReplayBuffer(argumentBuffer, startOffset + stride * arrayElement);
  }
  return true;
}

void WrappedMTLArgumentEncoder::setArgumentBufferArray(WrappedMTLBuffer *argumentBuffer,
                                                        NS::UInteger startOffset,
                                                        NS::UInteger arrayElement)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setArgumentBuffer(Unwrap(argumentBuffer), startOffset, arrayElement));
  m_ArgumentBuffer = argumentBuffer;
  m_ArgumentOffset = startOffset + Unwrap(this)->encodedLength() * arrayElement;
  if(IsCaptureMode(m_State) && argumentBuffer)
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLArgumentEncoder_setArgumentBuffer_arrayElement);
    Serialise_setArgumentBufferArray(ser, argumentBuffer, startOffset, arrayElement);
    MetalResourceRecord *record = GetRecord(argumentBuffer);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedMTLArgumentEncoder::Serialise_setBuffer(SerialiserType &ser, WrappedMTLBuffer *buffer,
                                                    NS::UInteger offset, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ArgumentEncoder, this);
  SERIALISE_ELEMENT_LOCAL(bufferId, GetResID(buffer)).Named("buffer"_lit).TypedAs("MTLBuffer"_lit).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ArgumentEncoder || ArgumentEncoder->m_Type != eResArgumentEncoder ||
       !ArgumentEncoder->m_Real || !ArgumentEncoder->m_ArgumentBuffer || index >= 32 ||
       ArgumentEncoder->m_MemberTypes[index] != MTL::DataTypePointer ||
       ArgumentEncoder->m_BufferSizes[index] == 0 ||
       (bufferId != ResourceId() && !GetResourceManager()->HasResource(bufferId)))
    {
      RDCERR("Invalid or unsupported Metal argument buffer member, identity or selection");
      return false;
    }
    buffer = bufferId == ResourceId() ? NULL :
                 (WrappedMTLBuffer *)GetResourceManager()->GetResource(bufferId);
    if((!buffer && offset != 0) || (buffer &&
       (buffer->m_Type != eResBuffer || !buffer->m_Real ||
        offset > Unwrap(buffer)->length() ||
        ArgumentEncoder->m_BufferSizes[index] > Unwrap(buffer)->length() - offset ||
        offset % ArgumentEncoder->m_BufferAlignments[index])))
    {
      RDCERR("Invalid Metal argument buffer member resource type, alignment or range");
      return false;
    }
    Unwrap(ArgumentEncoder)->setBuffer(Unwrap(buffer), offset, index);
    m_Device->GetReplay()->SetArgumentBufferMember(
        GetResID(ArgumentEncoder->m_ArgumentBuffer), ArgumentEncoder->m_ArgumentOffset,
        (uint32_t)index, bufferId, offset);
  }
  return true;
}

void WrappedMTLArgumentEncoder::setBuffer(WrappedMTLBuffer *buffer, NS::UInteger offset,
                                          NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setBuffer(Unwrap(buffer), offset, index));
  if(IsCaptureMode(m_State) && CheckCaptureMutation())
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLArgumentEncoder_setBuffer);
    Serialise_setBuffer(ser, buffer, offset, index);
    MetalResourceRecord *record = GetRecord(m_ArgumentBuffer);
    record->AddChunk(scope.Get());
    if(buffer)
      record->AddParent(GetRecord(buffer));
  }
}

static uint64_t ConstantSize(MTL::DataType type)
{
  // Three-component MSL vectors have the storage size/alignment of four components.
  const MTL::DataType bases[] = {MTL::DataTypeFloat, MTL::DataTypeHalf, MTL::DataTypeInt,
      MTL::DataTypeUInt, MTL::DataTypeShort, MTL::DataTypeUShort, MTL::DataTypeChar,
      MTL::DataTypeUChar, MTL::DataTypeBool, MTL::DataTypeLong, MTL::DataTypeULong};
  const uint64_t sizes[] = {4, 2, 4, 4, 2, 2, 1, 1, 1, 8, 8};
  for(size_t i = 0; i < ARRAY_COUNT(bases); i++)
    if(type >= bases[i] && type <= bases[i] + 3)
      return sizes[i] * (type == bases[i] ? 1 : type == bases[i] + 1 ? 2 : 4);
  return 0;
}

template <typename SerialiserType>
bool WrappedMTLArgumentEncoder::Serialise_constantDataAtIndex(SerialiserType &ser, NS::UInteger index)
{
  SERIALISE_ELEMENT_LOCAL(ArgumentEncoder, this);
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!ArgumentEncoder || ArgumentEncoder->m_Type != eResArgumentEncoder ||
       !ArgumentEncoder->m_Real || !ArgumentEncoder->m_ArgumentBuffer || index >= 32 ||
       ConstantSize(ArgumentEncoder->m_MemberTypes[index]) == 0)
    {
      RDCERR("Invalid or unsupported Metal argument constant member or selection");
      return false;
    }
    // Never serialise an application pointer. Initial contents and CPU-update chunks carry bytes.
    const uintptr_t base = (uintptr_t)Unwrap(ArgumentEncoder->m_ArgumentBuffer)->contents() +
                           ArgumentEncoder->m_ArgumentOffset;
    const uintptr_t data = (uintptr_t)Unwrap(ArgumentEncoder)->constantData(index);
    const uint64_t length = Unwrap(ArgumentEncoder)->encodedLength();
    if(data < base || data - base > length ||
       ConstantSize(ArgumentEncoder->m_MemberTypes[index]) > length - (data - base))
    {
      RDCERR("Invalid Metal argument constant range");
      return false;
    }
  }
  return true;
}

void *WrappedMTLArgumentEncoder::constantDataAtIndex(NS::UInteger index)
{
  void *result = NULL;
  SERIALISE_TIME_CALL(result = Unwrap(this)->constantData(index));
  if(IsCaptureMode(m_State) && m_ArgumentBuffer && result)
  {
    GetResourceManager()->MarkDirtyResource(GetResID(m_ArgumentBuffer));
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLArgumentEncoder_constantDataAtIndex);
    Serialise_constantDataAtIndex(ser, index);
    GetRecord(m_ArgumentBuffer)->AddChunk(scope.Get());
  }
  return result;
}

bool WrappedMTLArgumentEncoder::CheckCaptureMutation()
{
  if(!m_ArgumentBuffer)
    return false;
  if(IsActiveCapturing(m_State))
  {
    // CPU resource writes cannot be hoisted into initialisation across submissions. Data-only
    // constant writes remain supported through the shared-buffer CPU snapshot path.
    unsupportedEncoding();
    return false;
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLArgumentEncoder::Serialise_unsupportedEncoding(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(ArgumentEncoder, this);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    RDCERR("Unsupported Metal argument resource re-encoding during frame capture");
    return false;
  }
  return true;
}

void WrappedMTLArgumentEncoder::unsupportedEncoding()
{
  CACHE_THREAD_SERIALISER();
  SCOPED_SERIALISE_CHUNK(MetalChunk::MTLArgumentEncoder_unsupportedEncoding);
  Serialise_unsupportedEncoding(ser);
  GetRecord(m_ArgumentBuffer)->AddChunk(scope.Get());
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLArgumentEncoder, void, setArgumentBufferArray,
                                WrappedMTLBuffer *argumentBuffer, NS::UInteger startOffset,
                                NS::UInteger arrayElement);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLArgumentEncoder, void, setBuffer,
                                WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLArgumentEncoder, void *, constantDataAtIndex, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLArgumentEncoder, void, unsupportedEncoding);

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLArgumentEncoder, void, setArgumentBuffer,
                                WrappedMTLBuffer *argumentBuffer, NS::UInteger offset);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLArgumentEncoder, void, setTexture,
                                WrappedMTLTexture *texture, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLArgumentEncoder, void, setSamplerState,
                                WrappedMTLSamplerState *sampler, NS::UInteger index);
