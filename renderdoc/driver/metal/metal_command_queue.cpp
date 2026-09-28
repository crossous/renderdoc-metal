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

#include "metal_command_queue.h"
#include "metal_command_buffer.h"
#include "metal_device.h"

WrappedMTLCommandQueue::WrappedMTLCommandQueue(MTL::CommandQueue *realMTLCommandQueue,
                                               ResourceId objId, WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLCommandQueue, objId, wrappedMTLDevice, wrappedMTLDevice->GetStateRef())
{
  if(realMTLCommandQueue && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

bool WrappedMTLCommandQueue::ReplayCommandBuffer(ResourceId id, MTL::CommandBuffer *real)
{
  if(!real)
    return false;

  WrappedMTLCommandBuffer *wrapped =
      (WrappedMTLCommandBuffer *)GetResourceManager()->GetResource(id, true);
  if(wrapped)
    GetResourceManager()->ReplaceRealResource(wrapped, real);
  else
    GetResourceManager()->WrapResource(id, real, wrapped);
  wrapped->SetCommandQueue(this);
  if(!m_Device->SetReplayCommandBuffer(wrapped))
    return false;
  if(IsLoading(m_State))
  {
    m_Device->AddResource(id, ResourceType::CommandBuffer, "Command Buffer");
    m_Device->DerivedResource(GetResID(this), id);
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLCommandQueue::Serialise_commandBuffer(SerialiserType &ser,
                                                     WrappedMTLCommandBuffer *buffer)
{
  SERIALISE_ELEMENT_LOCAL(CommandQueue, this);
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, GetResID(buffer)).TypedAs("MTLCommandBuffer"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    MTL::CommandBuffer *realMTLCommandBuffer = Unwrap(CommandQueue)->commandBuffer();
    if(!realMTLCommandBuffer)
      return false;

    WrappedMTLCommandBuffer *wrappedMTLCommandBuffer =
        (WrappedMTLCommandBuffer *)GetResourceManager()->GetResource(CommandBuffer, true);
    if(wrappedMTLCommandBuffer)
      GetResourceManager()->ReplaceRealResource(wrappedMTLCommandBuffer, realMTLCommandBuffer);
    else
      GetResourceManager()->WrapResource(CommandBuffer, realMTLCommandBuffer,
                                         wrappedMTLCommandBuffer);
    wrappedMTLCommandBuffer->SetCommandQueue(CommandQueue);
    if(!m_Device->SetReplayCommandBuffer(wrappedMTLCommandBuffer))
      return false;
    if(IsLoading(m_State))
    {
      m_Device->AddResource(CommandBuffer, ResourceType::CommandBuffer, "Command Buffer");
      m_Device->DerivedResource(CommandQueue, CommandBuffer);
    }
  }
  return true;
}

WrappedMTLCommandBuffer *WrappedMTLCommandQueue::commandBuffer()
{
  MTL::CommandBuffer *realMTLCommandBuffer;
  SERIALISE_TIME_CALL(realMTLCommandBuffer = Unwrap(this)->commandBuffer());
  WrappedMTLCommandBuffer *wrappedMTLCommandBuffer;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), realMTLCommandBuffer,
                                                     wrappedMTLCommandBuffer);
  wrappedMTLCommandBuffer->SetCommandQueue(this);

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandQueue_commandBuffer);
      Serialise_commandBuffer(ser, wrappedMTLCommandBuffer);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord =
        GetResourceManager()->AddResourceRecord(wrappedMTLCommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->cmdInfo = new MetalCmdBufferRecordingInfo(this);
  }
  else
  {
    // TODO: implement RD MTL replay
    GetResourceManager()->AddResource(id, wrappedMTLCommandBuffer);
  }

  return wrappedMTLCommandBuffer;
}

template <typename SerialiserType>
bool WrappedMTLCommandQueue::Serialise_commandBufferWithDescriptor(
    SerialiserType &ser, WrappedMTLCommandBuffer *buffer, bool retainedReferences,
    uint64_t errorOptions)
{
  SERIALISE_ELEMENT_LOCAL(CommandQueue, this);
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, GetResID(buffer)).TypedAs("MTLCommandBuffer"_lit);
  SERIALISE_ELEMENT(retainedReferences).Important();
  SERIALISE_ELEMENT(errorOptions).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(errorOptions > (uint64_t)MTL::CommandBufferErrorOptionEncoderExecutionStatus)
    {
      RDCERR("Invalid Metal command-buffer error options 0x%llx", errorOptions);
      return false;
    }
    MTL::CommandBufferDescriptor *descriptor = MTL::CommandBufferDescriptor::alloc()->init();
    descriptor->setRetainedReferences(retainedReferences);
    descriptor->setErrorOptions((MTL::CommandBufferErrorOption)errorOptions);
    MTL::CommandBuffer *real = Unwrap(CommandQueue)->commandBuffer(descriptor);
    descriptor->release();
    return ReplayCommandBuffer(CommandBuffer, real);
  }
  return true;
}

WrappedMTLCommandBuffer *WrappedMTLCommandQueue::commandBufferWithDescriptor(
    MTL::CommandBufferDescriptor *descriptor)
{
  if(!descriptor)
    return NULL;
  const bool retainedReferences = descriptor->retainedReferences();
  const uint64_t errorOptions = (uint64_t)descriptor->errorOptions();
  MTL::CommandBuffer *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->commandBuffer(descriptor));
  if(!real)
    return NULL;

  WrappedMTLCommandBuffer *wrapped = NULL;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->SetCommandQueue(this);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandQueue_commandBufferWithDescriptor);
    Serialise_commandBufferWithDescriptor(ser, wrapped, retainedReferences, errorOptions);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    record->cmdInfo = new MetalCmdBufferRecordingInfo(this);
  }
  else
  {
    GetResourceManager()->AddResource(id, wrapped);
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLCommandQueue::Serialise_commandBufferWithUnretainedReferences(
    SerialiserType &ser, WrappedMTLCommandBuffer *buffer)
{
  SERIALISE_ELEMENT_LOCAL(CommandQueue, this);
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, GetResID(buffer)).TypedAs("MTLCommandBuffer"_lit);
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
    return ReplayCommandBuffer(CommandBuffer,
                               Unwrap(CommandQueue)->commandBufferWithUnretainedReferences());
  return true;
}

WrappedMTLCommandBuffer *WrappedMTLCommandQueue::commandBufferWithUnretainedReferences()
{
  MTL::CommandBuffer *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->commandBufferWithUnretainedReferences());
  if(!real)
    return NULL;

  WrappedMTLCommandBuffer *wrapped = NULL;
  ResourceId id = GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  wrapped->SetCommandQueue(this);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandQueue_commandBufferWithUnretainedReferences);
    Serialise_commandBufferWithUnretainedReferences(ser, wrapped);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    record->cmdInfo = new MetalCmdBufferRecordingInfo(this);
  }
  else
  {
    GetResourceManager()->AddResource(id, wrapped);
  }
  return wrapped;
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandQueue, WrappedMTLCommandBuffer *,
                                            commandBuffer);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandQueue, WrappedMTLCommandBuffer *,
                                            commandBufferWithDescriptor, bool retainedReferences,
                                            uint64_t errorOptions);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLCommandQueue, WrappedMTLCommandBuffer *,
                                            commandBufferWithUnretainedReferences);
