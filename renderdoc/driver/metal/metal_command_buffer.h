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

#include "metal_command_queue.h"
#include "metal_common.h"
#include "metal_device.h"
#include "metal_resources.h"

bool ValidateMetalRenderPassCounters(WrappedMTLDevice *device,
    const rdcarray<RDMTL::RenderPassSampleBufferAttachmentDescriptor> &attachments);

bool ValidateMetalComputePassCounters(WrappedMTLDevice *device,
    const rdcarray<RDMTL::ComputePassSampleBufferAttachmentDescriptor> &attachments);

class WrappedMTLCommandBuffer : public WrappedMTLObject
{
public:
  DECLARE_FUNCTION_SERIALISED(void, encodeEvent, WrappedMTLEvent *event, uint64_t value, bool signal);
  void CaptureEvent(WrappedMTLEvent *event, uint64_t value, bool signal);
  WrappedMTLCommandBuffer(MTL::CommandBuffer *realMTLCommandBuffer, ResourceId objId,
                          WrappedMTLDevice *wrappedMTLDevice);
  ~WrappedMTLCommandBuffer();

  void SetCommandQueue(WrappedMTLCommandQueue *commandQueue);
  WrappedMTLCommandQueue *GetCommandQueue() { return m_CommandQueue; }
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLBlitCommandEncoder *, blitCommandEncoder);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLAccelerationStructureCommandEncoder *,
                                          accelerationStructureCommandEncoder);
  WrappedMTLAccelerationStructureCommandEncoder *accelerationStructureCommandEncoderWithDescriptor(
      MTL::AccelerationStructurePassDescriptor *descriptor);
  template <typename SerialiserType>
  bool Serialise_accelerationStructureCommandEncoderWithDescriptor(
      SerialiserType &ser, WrappedMTLAccelerationStructureCommandEncoder *encoder,
      bool hasSampleBuffers);
  WrappedMTLBlitCommandEncoder *blitCommandEncoderWithDescriptor(MTL::BlitPassDescriptor *descriptor);
  template <typename SerialiserType>
  bool Serialise_blitCommandEncoderWithDescriptor(SerialiserType &ser,
                                                   WrappedMTLBlitCommandEncoder *encoder,
                                                   bool hasSampleBuffers,
                                                   rdcarray<RDMTL::BlitPassSampleBufferAttachmentDescriptor> attachments);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLComputeCommandEncoder *,
                                          computeCommandEncoder);
  WrappedMTLComputeCommandEncoder *computeCommandEncoderWithDescriptor(
      MTL::ComputePassDescriptor *descriptor);
  template <typename SerialiserType>
  bool Serialise_computeCommandEncoderWithDescriptor(
      SerialiserType &ser, WrappedMTLComputeCommandEncoder *encoder,
      MTL::DispatchType dispatchType,
      rdcarray<RDMTL::ComputePassSampleBufferAttachmentDescriptor> attachments);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLComputeCommandEncoder *,
                                          computeCommandEncoder, MTL::DispatchType dispatchType);
  DECLARE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLRenderCommandEncoder *,
                                          renderCommandEncoderWithDescriptor,
                                          RDMTL::RenderPassDescriptor &descriptor);
  WrappedMTLParallelRenderCommandEncoder *parallelRenderCommandEncoderWithDescriptor(
      RDMTL::RenderPassDescriptor &descriptor);
  template <typename SerialiserType>
  bool Serialise_parallelRenderCommandEncoderWithDescriptor(
      SerialiserType &ser, WrappedMTLParallelRenderCommandEncoder *encoder,
      RDMTL::RenderPassDescriptor &descriptor);
  void presentDrawable(MTL::Drawable *drawable);
  void presentDrawable(MTL::Drawable *drawable, double time, bool minimumDuration);
  template <typename SerialiserType>
  bool Serialise_presentDrawableTimed(SerialiserType &ser, WrappedMTLTexture *presentedImage,
                                     double time, bool minimumDuration);
  template <typename SerialiserType>
  bool Serialise_presentDrawable(SerialiserType &ser, WrappedMTLTexture *presentedImage);
  void CapturePresent(MTL::Drawable *drawable, MetalChunk chunk, double time);
  DECLARE_FUNCTION_SERIALISED(void, commit);
  DECLARE_FUNCTION_SERIALISED(void, enqueue);
  DECLARE_FUNCTION_SERIALISED(void, pushDebugGroup, NS::String *string);
  DECLARE_FUNCTION_SERIALISED(void, popDebugGroup);
  DECLARE_FUNCTION_SERIALISED(void, waitUntilScheduled);
  DECLARE_FUNCTION_SERIALISED(void, waitUntilCompleted);
  // Native blocks remain in the ObjC bridge. Only their registration is serialised.
  void CaptureHandlerRegistration(bool completed);
  template <typename SerialiserType>
  bool Serialise_handlerRegistration(SerialiserType &ser);

  enum
  {
    TypeEnum = eResCommandBuffer
  };

private:
  bool ReplayBlitCommandEncoder(ResourceId id, MTL::BlitCommandEncoder *realEncoder);
  bool ReplayComputeCommandEncoder(ResourceId id, MTL::ComputeCommandEncoder *realEncoder);
  WrappedMTLCommandQueue *m_CommandQueue = NULL;
  NS::Object *m_CaptureQueueProxy = NULL;
};
