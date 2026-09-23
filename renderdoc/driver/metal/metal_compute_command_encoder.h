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
#include "metal_device.h"
#include "metal_resources.h"

class WrappedMTLComputeCommandEncoder : public WrappedMTLObject
{
public:
  WrappedMTLComputeCommandEncoder(MTL::ComputeCommandEncoder *real, ResourceId id,
                                  WrappedMTLDevice *device);

  void SetCommandBuffer(WrappedMTLCommandBuffer *commandBuffer) { m_CommandBuffer = commandBuffer; }
  DECLARE_FUNCTION_SERIALISED(void, endEncoding);
  DECLARE_FUNCTION_SERIALISED(void, setComputePipelineState,
                              WrappedMTLComputePipelineState *pipeline);
  DECLARE_FUNCTION_SERIALISED(void, setTexture, WrappedMTLTexture *texture, NS::UInteger index);
  DECLARE_FUNCTION_SERIALISED(void, dispatchThreadgroups, MTL::Size &groups,
                              MTL::Size &threadsPerGroup);

  enum
  {
    TypeEnum = eResComputeCommandEncoder
  };

private:
  WrappedMTLCommandBuffer *m_CommandBuffer = NULL;
  WrappedMTLComputePipelineState *m_Pipeline = NULL;
  WrappedMTLTexture *m_Textures[2] = {};
};
