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

#pragma once

#include "metal_common.h"
#include "metal_device.h"
#include "metal_resources.h"


// Pure descriptor/range/alignment validation, shared by preflight and native creation.
bool ValidateMetalBufferTexture(MTL::Device *device, const RDMTL::TextureDescriptor &descriptor,
                               uint64_t bufferLength, MTL::StorageMode bufferStorage,
                               uint64_t offset, uint64_t bytesPerRow);

class WrappedMTLBuffer : public WrappedMTLObject
{
public:
  WrappedMTLBuffer(MTL::Buffer *realMTLBuffer, ResourceId objId, WrappedMTLDevice *wrappedMTLDevice);

  void *contents();
  void makeAliasable();
  template <typename SerialiserType>
  bool Serialise_makeAliasable(SerialiserType &ser);
  DECLARE_FUNCTION_SERIALISED(MTL::PurgeableState, setPurgeableState, MTL::PurgeableState state);
  DECLARE_FUNCTION_SERIALISED(void, addDebugMarker, NS::String *marker, NS::Range range);
  DECLARE_FUNCTION_SERIALISED(void, removeAllDebugMarkers);

  DECLARE_FUNCTION_SERIALISED(void, didModifyRange, NS::Range &range);
  WrappedMTLTexture *newTextureWithDescriptor(RDMTL::TextureDescriptor &descriptor,
                                              NS::UInteger offset, NS::UInteger bytesPerRow);
  template <typename SerialiserType>
  bool Serialise_newTextureWithDescriptor(SerialiserType &ser, WrappedMTLTexture *texture,
                                          RDMTL::TextureDescriptor &descriptor,
                                          NS::UInteger offset, NS::UInteger bytesPerRow);
  template <typename SerialiserType>
  bool Serialise_InternalModifyCPUContents(SerialiserType &ser, uint64_t start, uint64_t end,
                                           MetalBufferInfo *bufInfo);

  enum
  {
    TypeEnum = eResBuffer
  };

private:
};
