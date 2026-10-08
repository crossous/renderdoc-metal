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

#pragma once

#include "metal_common.h"

MTL::ArrayType *MetalArgumentArray(MTL::StructMember *member);

class WrappedMTLArgumentEncoder : public WrappedMTLObject
{
public:
  WrappedMTLArgumentEncoder(MTL::ArgumentEncoder *realArgumentEncoder, ResourceId objId,
                            WrappedMTLDevice *wrappedMTLDevice);
  void ConfigureLayout(MTL::Argument *reflection);
  void ConfigureDescriptorLayout(const rdcarray<uint64_t> &descriptors);
  MTL::DataType GetMemberType(uint32_t index) const { return m_MemberTypes[index]; }
  WrappedMTLArgumentEncoder *newArgumentEncoder(NS::UInteger index);
  template <typename SerialiserType>
  bool Serialise_newArgumentEncoder(SerialiserType &ser, WrappedMTLArgumentEncoder *encoder,
                                    NS::UInteger index, uint64_t encodedLength,
                                    uint64_t alignment, bool supported);

  DECLARE_FUNCTION_SERIALISED(void, setArgumentBuffer, WrappedMTLBuffer *argumentBuffer,
                              NS::UInteger offset);
  DECLARE_FUNCTION_SERIALISED(void, setArgumentBufferArray, WrappedMTLBuffer *argumentBuffer,
                              NS::UInteger startOffset, NS::UInteger arrayElement);
  DECLARE_FUNCTION_SERIALISED(void, setBuffer, WrappedMTLBuffer *buffer,
                              NS::UInteger offset, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void *, constantDataAtIndex, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, unsupportedEncoding);
  DECLARE_FUNCTION_SERIALISED(void, setTexture, WrappedMTLTexture *texture, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setSamplerState, WrappedMTLSamplerState *sampler,
                              NS::UInteger index);
  void setVisibleFunctionTable(WrappedMTLVisibleFunctionTable *table, NS::UInteger index);
  template <typename SerialiserType>
  bool Serialise_setVisibleFunctionTable(SerialiserType &ser,
      WrappedMTLVisibleFunctionTable *table, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setIntersectionFunctionTable,
                              WrappedMTLIntersectionFunctionTable *table, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, setAccelerationStructure,
                              WrappedMTLAccelerationStructure *structure, NS::UInteger index);

  enum
  {
    TypeEnum = eResArgumentEncoder
  };

private:
  bool SelectReplayBuffer(WrappedMTLBuffer *buffer, uint64_t offset);
  bool CheckCaptureMutation();
  WrappedMTLBuffer *m_ArgumentBuffer = NULL;
  uint64_t m_ArgumentOffset = 0;
  // The current replay descriptor address space reserves 32 members per argument buffer.
  MTL::DataType m_MemberTypes[32] = {};
  uint64_t m_BufferSizes[32] = {}, m_BufferAlignments[32] = {};
  struct NestedLayout
  {
    uint32_t index = 0;
    uint64_t size = 0, alignment = 0;
    MTL::DataType memberTypes[32] = {};
    bool supported = false;
  };
  rdcarray<NestedLayout> m_NestedLayouts;
  const NestedLayout *FindNestedLayout(NS::UInteger index) const;
  void ConfigureNestedLayout(const NestedLayout &layout);
};
