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

WrappedMTLArgumentEncoder::WrappedMTLArgumentEncoder(MTL::ArgumentEncoder *realArgumentEncoder,
                                                     ResourceId objId,
                                                     WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realArgumentEncoder, objId, wrappedMTLDevice,
                       wrappedMTLDevice->GetStateRef())
{
  if(realArgumentEncoder && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
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
    if(ArgumentEncoder == NULL || argumentBuffer == NULL)
    {
      RDCERR("Cannot replay Metal argument-buffer selection with a null encoder or buffer");
      return false;
    }
    Unwrap(ArgumentEncoder)->setArgumentBuffer(Unwrap(argumentBuffer), offset);
    ArgumentEncoder->m_ArgumentBuffer = argumentBuffer;
  }
  return true;
}

void WrappedMTLArgumentEncoder::setArgumentBuffer(WrappedMTLBuffer *argumentBuffer,
                                                  NS::UInteger offset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setArgumentBuffer(Unwrap(argumentBuffer), offset));
  m_ArgumentBuffer = argumentBuffer;
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
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(ArgumentEncoder == NULL || ArgumentEncoder->m_ArgumentBuffer == NULL)
    {
      RDCERR("Cannot replay Metal argument texture before setArgumentBuffer");
      return false;
    }
    Unwrap(ArgumentEncoder)->setTexture(Unwrap(texture), index);
    m_Device->GetReplay()->SetArgumentBufferTexture(
        GetResID(ArgumentEncoder->m_ArgumentBuffer), (uint32_t)index, GetResID(texture));
  }
  return true;
}

void WrappedMTLArgumentEncoder::setTexture(WrappedMTLTexture *texture, NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setTexture(Unwrap(texture), index));
  if(IsCaptureMode(m_State))
  {
    if(m_ArgumentBuffer == NULL)
    {
      RDCERR("Ignoring Metal argument texture written before setArgumentBuffer");
      return;
    }
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
  SERIALISE_ELEMENT(sampler).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(ArgumentEncoder == NULL || ArgumentEncoder->m_ArgumentBuffer == NULL)
    {
      RDCERR("Cannot replay Metal argument sampler before setArgumentBuffer");
      return false;
    }
    Unwrap(ArgumentEncoder)->setSamplerState(Unwrap(sampler), index);
    m_Device->GetReplay()->SetArgumentBufferSampler(
        GetResID(ArgumentEncoder->m_ArgumentBuffer), (uint32_t)index, GetResID(sampler));
  }
  return true;
}

void WrappedMTLArgumentEncoder::setSamplerState(WrappedMTLSamplerState *sampler,
                                                NS::UInteger index)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setSamplerState(Unwrap(sampler), index));
  if(IsCaptureMode(m_State))
  {
    if(m_ArgumentBuffer == NULL)
    {
      RDCERR("Ignoring Metal argument sampler written before setArgumentBuffer");
      return;
    }
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLArgumentEncoder_setSamplerState);
    Serialise_setSamplerState(ser, sampler, index);
    MetalResourceRecord *record = GetRecord(m_ArgumentBuffer);
    record->AddChunk(scope.Get());
    if(sampler)
      record->AddParent(GetRecord(sampler));
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLArgumentEncoder, void, setArgumentBuffer,
                                WrappedMTLBuffer *argumentBuffer, NS::UInteger offset);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLArgumentEncoder, void, setTexture,
                                WrappedMTLTexture *texture, NS::UInteger index);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLArgumentEncoder, void, setSamplerState,
                                WrappedMTLSamplerState *sampler, NS::UInteger index);
