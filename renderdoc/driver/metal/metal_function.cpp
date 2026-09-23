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

#include "metal_function.h"
#include "metal_argument_encoder.h"
#include "metal_device.h"

WrappedMTLFunction::WrappedMTLFunction(MTL::Function *realMTLFunction, ResourceId objId,
                                       WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLFunction, objId, wrappedMTLDevice, wrappedMTLDevice->GetStateRef())
{
  if(realMTLFunction && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLFunction::Serialise_newArgumentEncoder(SerialiserType &ser,
                                                       WrappedMTLArgumentEncoder *argumentEncoder,
                                                       NS::UInteger bufferIndex)
{
  SERIALISE_ELEMENT_LOCAL(Function, this);
  SERIALISE_ELEMENT_LOCAL(ArgumentEncoder, GetResID(argumentEncoder))
      .TypedAs("MTLArgumentEncoder"_lit);
  SERIALISE_ELEMENT(bufferIndex).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    MTL::ArgumentEncoder *realArgumentEncoder = Unwrap(Function)->newArgumentEncoder(bufferIndex);
    if(realArgumentEncoder == NULL)
    {
      RDCERR("Failed to recreate Metal argument encoder for buffer index %llu", bufferIndex);
      return false;
    }
    WrappedMTLArgumentEncoder *wrappedArgumentEncoder = NULL;
    GetResourceManager()->WrapResource(ArgumentEncoder, realArgumentEncoder, wrappedArgumentEncoder,
                                       true);
    m_Device->AddResource(ArgumentEncoder, ResourceType::StateObject, "Argument Encoder");
    m_Device->DerivedResource(Function, ArgumentEncoder);
  }
  return true;
}

WrappedMTLArgumentEncoder *WrappedMTLFunction::newArgumentEncoder(NS::UInteger bufferIndex)
{
  MTL::ArgumentEncoder *realArgumentEncoder = NULL;
  SERIALISE_TIME_CALL(realArgumentEncoder = Unwrap(this)->newArgumentEncoder(bufferIndex));
  if(realArgumentEncoder == NULL)
    return NULL;

  WrappedMTLArgumentEncoder *wrappedArgumentEncoder = NULL;
  GetResourceManager()->WrapResource(ResourceId(), realArgumentEncoder, wrappedArgumentEncoder);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLFunction_newArgumentEncoderWithBufferIndex);
    Serialise_newArgumentEncoder(ser, wrappedArgumentEncoder, bufferIndex);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedArgumentEncoder);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(this));
  }
  return wrappedArgumentEncoder;
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLFunction,
                                            WrappedMTLArgumentEncoder *argumentEncoder,
                                            newArgumentEncoder, NS::UInteger bufferIndex);
