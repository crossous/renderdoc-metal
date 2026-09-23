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

#include "metal_blit_command_encoder.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_replay.h"
#include "metal_texture.h"

static bool ValidBufferRange(WrappedMTLBuffer *buffer, NS::UInteger offset, NS::UInteger size)
{
  if(buffer == NULL || Unwrap(buffer) == NULL || size == 0)
    return false;
  const uint64_t length = Unwrap(buffer)->length();
  return uint64_t(offset) <= length && uint64_t(size) <= length - uint64_t(offset);
}

static uint64_t TextureSliceCount(MTL::Texture *texture)
{
  if(texture == NULL)
    return 0;
  if(texture->textureType() == MTL::TextureTypeCube)
    return 6;
  if(texture->textureType() == MTL::TextureTypeCubeArray)
    return uint64_t(texture->arrayLength()) * 6;
  return RDCMAX(1ULL, uint64_t(texture->arrayLength()));
}

static bool ValidTextureSubresource(WrappedMTLTexture *texture, NS::UInteger slice,
                                    NS::UInteger level)
{
  MTL::Texture *real = texture ? Unwrap(texture) : NULL;
  return real != NULL && uint64_t(level) < real->mipmapLevelCount() &&
         uint64_t(slice) < TextureSliceCount(real);
}

static bool ValidTextureRegion(WrappedMTLTexture *texture, NS::UInteger slice, NS::UInteger level,
                               const MTL::Origin &origin, const MTL::Size &size)
{
  if(!ValidTextureSubresource(texture, slice, level))
    return false;

  MTL::Texture *real = Unwrap(texture);
  const uint64_t width = RDCMAX(1ULL, uint64_t(real->width()) >> level);
  const uint64_t height = RDCMAX(1ULL, uint64_t(real->height()) >> level);
  const uint64_t depth = RDCMAX(1ULL, uint64_t(real->depth()) >> level);
  return size.width > 0 && size.height > 0 && size.depth > 0 &&
         uint64_t(origin.x) <= width && uint64_t(size.width) <= width - uint64_t(origin.x) &&
         uint64_t(origin.y) <= height && uint64_t(size.height) <= height - uint64_t(origin.y) &&
         uint64_t(origin.z) <= depth && uint64_t(size.depth) <= depth - uint64_t(origin.z);
}

WrappedMTLBlitCommandEncoder::WrappedMTLBlitCommandEncoder(
    MTL::BlitCommandEncoder *realMTLBlitCommandEncoder, ResourceId objId,
    WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLBlitCommandEncoder, objId, wrappedMTLDevice,
                       wrappedMTLDevice->GetStateRef())
{
  if(realMTLBlitCommandEncoder && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLBlitCommandEncoder::Serialise_setLabel(SerialiserType &ser, NS::String *value)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(value).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
    Unwrap(BlitCommandEncoder)->setLabel(value);
  return true;
}

void WrappedMTLBlitCommandEncoder::setLabel(NS::String *value)
{
  SERIALISE_TIME_CALL(Unwrap(this)->setLabel(value));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_setLabel);
      Serialise_setLabel(ser, value);
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
bool WrappedMTLBlitCommandEncoder::Serialise_endEncoding(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(BlitCommandEncoder)->endEncoding();
    m_Device->SetReplayBlitCommandEncoder(NULL);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "End Metal Blit Pass";
      action.flags = ActionFlags::PassBoundary | ActionFlags::EndPass;
      AddAction(action);
    }
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::endEncoding()
{
  SERIALISE_TIME_CALL(Unwrap(this)->endEncoding());

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_endEncoding);
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

template <typename SerialiserType>
bool WrappedMTLBlitCommandEncoder::Serialise_insertDebugSignpost(SerialiserType &ser,
                                                                 NS::String *string)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(string).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::insertDebugSignpost(NS::String *string)
{
  SERIALISE_TIME_CALL(Unwrap(this)->insertDebugSignpost(string));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_insertDebugSignpost);
      Serialise_insertDebugSignpost(ser, string);
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
bool WrappedMTLBlitCommandEncoder::Serialise_pushDebugGroup(SerialiserType &ser, NS::String *string)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(string).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::pushDebugGroup(NS::String *string)
{
  SERIALISE_TIME_CALL(Unwrap(this)->pushDebugGroup(string));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_pushDebugGroup);
      Serialise_pushDebugGroup(ser, string);
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
bool WrappedMTLBlitCommandEncoder::Serialise_popDebugGroup(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::popDebugGroup()
{
  SERIALISE_TIME_CALL(Unwrap(this)->popDebugGroup());

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_popDebugGroup);
      Serialise_popDebugGroup(ser);
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
bool WrappedMTLBlitCommandEncoder::Serialise_synchronizeResource(SerialiserType &ser,
                                                                 WrappedMTLResource *resource)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(resource).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::synchronizeResource(WrappedMTLResource *resource)
{
  SERIALISE_TIME_CALL(Unwrap(this)->synchronizeResource(Unwrap(resource)));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_synchronizeResource);
      Serialise_synchronizeResource(ser, resource);
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
bool WrappedMTLBlitCommandEncoder::Serialise_synchronizeTexture(SerialiserType &ser,
                                                                WrappedMTLTexture *texture,
                                                                NS::UInteger slice,
                                                                NS::UInteger level)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(slice);
  SERIALISE_ELEMENT(level);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::synchronizeTexture(WrappedMTLTexture *texture,
                                                      NS::UInteger slice, NS::UInteger level)
{
  SERIALISE_TIME_CALL(Unwrap(this)->synchronizeTexture(Unwrap(texture), slice, level));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_synchronizeTexture);
      Serialise_synchronizeTexture(ser, texture, slice, level);
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
bool WrappedMTLBlitCommandEncoder::Serialise_copyFromBuffer(
    SerialiserType &ser, WrappedMTLBuffer *sourceBuffer, NS::UInteger sourceOffset,
    WrappedMTLBuffer *destinationBuffer, NS::UInteger destinationOffset, NS::UInteger size)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(sourceBuffer).Important();
  SERIALISE_ELEMENT(sourceOffset);
  SERIALISE_ELEMENT(destinationBuffer).Important();
  SERIALISE_ELEMENT(destinationOffset);
  SERIALISE_ELEMENT(size);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ValidBufferRange(sourceBuffer, sourceOffset, size) ||
       !ValidBufferRange(destinationBuffer, destinationOffset, size))
    {
      RDCERR("Invalid Metal buffer copy range: src=%llu+%llu dst=%llu+%llu",
             uint64_t(sourceOffset), uint64_t(size), uint64_t(destinationOffset), uint64_t(size));
      return false;
    }

    Unwrap(BlitCommandEncoder)
        ->copyFromBuffer(Unwrap(sourceBuffer), sourceOffset, Unwrap(destinationBuffer),
                         destinationOffset, size);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("copyFromBuffer(%llu bytes, %llu -> %llu)",
                                            uint64_t(size), uint64_t(sourceOffset),
                                            uint64_t(destinationOffset));
      action.flags = ActionFlags::Copy;
      action.copySource = GetResID(sourceBuffer);
      action.copyDestination = GetResID(destinationBuffer);
      AddAction(action);
      m_Device->GetReplay()->AddUsage(action.copySource, ResourceUsage::CopySrc);
      m_Device->GetReplay()->AddUsage(action.copyDestination, ResourceUsage::CopyDst);
    }
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::copyFromBuffer(WrappedMTLBuffer *sourceBuffer,
                                                  NS::UInteger sourceOffset,
                                                  WrappedMTLBuffer *destinationBuffer,
                                                  NS::UInteger destinationOffset, NS::UInteger size)
{
  SERIALISE_TIME_CALL(Unwrap(this)->copyFromBuffer(
      Unwrap(sourceBuffer), sourceOffset, Unwrap(destinationBuffer), destinationOffset, size));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toBuffer);
      Serialise_copyFromBuffer(ser, sourceBuffer, sourceOffset, destinationBuffer,
                               destinationOffset, size);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(sourceBuffer), eFrameRef_Read);
    bufferRecord->MarkResourceFrameReferenced(GetResID(destinationBuffer), eFrameRef_PartialWrite);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLBlitCommandEncoder::Serialise_copyFromBuffer(
    SerialiserType &ser, WrappedMTLBuffer *sourceBuffer, NS::UInteger sourceOffset,
    NS::UInteger sourceBytesPerRow, NS::UInteger sourceBytesPerImage, MTL::Size &sourceSize,
    WrappedMTLTexture *destinationTexture, NS::UInteger destinationSlice,
    NS::UInteger destinationLevel, MTL::Origin &destinationOrigin, MTL::BlitOption options)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(sourceBuffer).Important();
  SERIALISE_ELEMENT(sourceOffset);
  SERIALISE_ELEMENT(sourceBytesPerRow);
  SERIALISE_ELEMENT(sourceBytesPerImage);
  SERIALISE_ELEMENT(sourceSize);
  SERIALISE_ELEMENT(destinationTexture).Important();
  SERIALISE_ELEMENT(destinationSlice);
  SERIALISE_ELEMENT(destinationLevel);
  SERIALISE_ELEMENT(destinationOrigin);
  SERIALISE_ELEMENT(options);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::copyFromBuffer(
    WrappedMTLBuffer *sourceBuffer, NS::UInteger sourceOffset, NS::UInteger sourceBytesPerRow,
    NS::UInteger sourceBytesPerImage, MTL::Size &sourceSize, WrappedMTLTexture *destinationTexture,
    NS::UInteger destinationSlice, NS::UInteger destinationLevel, MTL::Origin &destinationOrigin,
    MTL::BlitOption options)
{
  SERIALISE_TIME_CALL(Unwrap(this)->copyFromBuffer(
      Unwrap(sourceBuffer), sourceOffset, sourceBytesPerRow, sourceBytesPerImage, sourceSize,
      Unwrap(destinationTexture), destinationSlice, destinationLevel, destinationOrigin, options));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toTexture_options);
      Serialise_copyFromBuffer(ser, sourceBuffer, sourceOffset, sourceBytesPerRow,
                               sourceBytesPerImage, sourceSize, destinationTexture,
                               destinationSlice, destinationLevel, destinationOrigin, options);
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
bool WrappedMTLBlitCommandEncoder::Serialise_copyFromTexture(
    SerialiserType &ser, WrappedMTLTexture *sourceTexture, NS::UInteger sourceSlice,
    NS::UInteger sourceLevel, MTL::Origin &sourceOrigin, MTL::Size &sourceSize,
    WrappedMTLTexture *destinationTexture, NS::UInteger destinationSlice,
    NS::UInteger destinationLevel, MTL::Origin &destinationOrigin)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(sourceTexture).Important();
  SERIALISE_ELEMENT(sourceSlice);
  SERIALISE_ELEMENT(sourceLevel);
  SERIALISE_ELEMENT(sourceOrigin);
  SERIALISE_ELEMENT(sourceSize);
  SERIALISE_ELEMENT(destinationTexture).Important();
  SERIALISE_ELEMENT(destinationSlice);
  SERIALISE_ELEMENT(destinationLevel);
  SERIALISE_ELEMENT(destinationOrigin);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ValidTextureRegion(sourceTexture, sourceSlice, sourceLevel, sourceOrigin, sourceSize) ||
       !ValidTextureRegion(destinationTexture, destinationSlice, destinationLevel,
                           destinationOrigin, sourceSize) ||
       Unwrap(sourceTexture)->pixelFormat() != Unwrap(destinationTexture)->pixelFormat())
    {
      RDCERR("Invalid Metal texture copy subresource or region");
      return false;
    }

    Unwrap(BlitCommandEncoder)
        ->copyFromTexture(Unwrap(sourceTexture), sourceSlice, sourceLevel, sourceOrigin, sourceSize,
                          Unwrap(destinationTexture), destinationSlice, destinationLevel,
                          destinationOrigin);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt(
          "copyFromTexture(mip %llu slice %llu -> mip %llu slice %llu, %llux%llux%llu)",
          uint64_t(sourceLevel), uint64_t(sourceSlice), uint64_t(destinationLevel),
          uint64_t(destinationSlice), uint64_t(sourceSize.width), uint64_t(sourceSize.height),
          uint64_t(sourceSize.depth));
      action.flags = ActionFlags::Copy;
      action.copySource = GetResID(sourceTexture);
      action.copySourceSubresource = Subresource((uint32_t)sourceLevel, (uint32_t)sourceSlice);
      action.copyDestination = GetResID(destinationTexture);
      action.copyDestinationSubresource =
          Subresource((uint32_t)destinationLevel, (uint32_t)destinationSlice);
      AddAction(action);
      m_Device->GetReplay()->AddUsage(action.copySource, ResourceUsage::CopySrc);
      m_Device->GetReplay()->AddUsage(action.copyDestination, ResourceUsage::CopyDst);
    }
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::copyFromTexture(
    WrappedMTLTexture *sourceTexture, NS::UInteger sourceSlice, NS::UInteger sourceLevel,
    MTL::Origin &sourceOrigin, MTL::Size &sourceSize, WrappedMTLTexture *destinationTexture,
    NS::UInteger destinationSlice, NS::UInteger destinationLevel, MTL::Origin &destinationOrigin)
{
  SERIALISE_TIME_CALL(Unwrap(this)->copyFromTexture(
      Unwrap(sourceTexture), sourceSlice, sourceLevel, sourceOrigin, sourceSize,
      Unwrap(destinationTexture), destinationSlice, destinationLevel, destinationOrigin));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(
          MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_origin);
      Serialise_copyFromTexture(ser, sourceTexture, sourceSlice, sourceLevel, sourceOrigin,
                                sourceSize, destinationTexture, destinationSlice, destinationLevel,
                                destinationOrigin);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(sourceTexture), eFrameRef_Read);
    bufferRecord->MarkResourceFrameReferenced(GetResID(destinationTexture), eFrameRef_PartialWrite);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLBlitCommandEncoder::Serialise_copyFromTexture(
    SerialiserType &ser, WrappedMTLTexture *sourceTexture, NS::UInteger sourceSlice,
    NS::UInteger sourceLevel, MTL::Origin &sourceOrigin, MTL::Size &sourceSize,
    WrappedMTLBuffer *destinationBuffer, NS::UInteger destinationOffset,
    NS::UInteger destinationBytesPerRow, NS::UInteger destinationBytesPerImage,
    MTL::BlitOption options)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(sourceTexture).Important();
  SERIALISE_ELEMENT(sourceSlice);
  SERIALISE_ELEMENT(sourceLevel);
  SERIALISE_ELEMENT(sourceOrigin);
  SERIALISE_ELEMENT(sourceSize);
  SERIALISE_ELEMENT(destinationBuffer).Important();
  SERIALISE_ELEMENT(destinationOffset);
  SERIALISE_ELEMENT(destinationBytesPerRow);
  SERIALISE_ELEMENT(destinationBytesPerImage);
  SERIALISE_ELEMENT(options);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::copyFromTexture(
    WrappedMTLTexture *sourceTexture, NS::UInteger sourceSlice, NS::UInteger sourceLevel,
    MTL::Origin &sourceOrigin, MTL::Size &sourceSize, WrappedMTLBuffer *destinationBuffer,
    NS::UInteger destinationOffset, NS::UInteger destinationBytesPerRow,
    NS::UInteger destinationBytesPerImage, MTL::BlitOption options)
{
  SERIALISE_TIME_CALL(
      Unwrap(this)->copyFromTexture(Unwrap(sourceTexture), sourceSlice, sourceLevel, sourceOrigin,
                                    sourceSize, Unwrap(destinationBuffer), destinationOffset,
                                    destinationBytesPerRow, destinationBytesPerImage, options));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toBuffer_options);
      Serialise_copyFromTexture(ser, sourceTexture, sourceSlice, sourceLevel, sourceOrigin,
                                sourceSize, destinationBuffer, destinationOffset,
                                destinationBytesPerRow, destinationBytesPerImage, options);
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
bool WrappedMTLBlitCommandEncoder::Serialise_copyFromTexture(SerialiserType &ser,
                                                             WrappedMTLTexture *sourceTexture,
                                                             WrappedMTLTexture *destinationTexture)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(sourceTexture).Important();
  SERIALISE_ELEMENT(destinationTexture).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::copyFromTexture(WrappedMTLTexture *sourceTexture,
                                                   WrappedMTLTexture *destinationTexture)
{
  SERIALISE_TIME_CALL(
      Unwrap(this)->copyFromTexture(Unwrap(sourceTexture), Unwrap(destinationTexture)));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture);
      Serialise_copyFromTexture(ser, sourceTexture, destinationTexture);
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
bool WrappedMTLBlitCommandEncoder::Serialise_copyFromTexture(
    SerialiserType &ser, WrappedMTLTexture *sourceTexture, NS::UInteger sourceSlice,
    NS::UInteger sourceLevel, WrappedMTLTexture *destinationTexture, NS::UInteger destinationSlice,
    NS::UInteger destinationLevel, NS::UInteger sliceCount, NS::UInteger levelCount)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(sourceTexture).Important();
  SERIALISE_ELEMENT(sourceSlice);
  SERIALISE_ELEMENT(sourceLevel);
  SERIALISE_ELEMENT(destinationTexture).Important();
  SERIALISE_ELEMENT(destinationSlice);
  SERIALISE_ELEMENT(destinationLevel);
  SERIALISE_ELEMENT(sliceCount);
  SERIALISE_ELEMENT(levelCount);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::copyFromTexture(WrappedMTLTexture *sourceTexture,
                                                   NS::UInteger sourceSlice, NS::UInteger sourceLevel,
                                                   WrappedMTLTexture *destinationTexture,
                                                   NS::UInteger destinationSlice,
                                                   NS::UInteger destinationLevel,
                                                   NS::UInteger sliceCount, NS::UInteger levelCount)
{
  SERIALISE_TIME_CALL(Unwrap(this)->copyFromTexture(Unwrap(sourceTexture), sourceSlice, sourceLevel,
                                                    Unwrap(destinationTexture), destinationSlice,
                                                    destinationLevel, sliceCount, levelCount));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(
          MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_count);
      Serialise_copyFromTexture(ser, sourceTexture, sourceSlice, sourceLevel, destinationTexture,
                                destinationSlice, destinationLevel, sliceCount, levelCount);
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
bool WrappedMTLBlitCommandEncoder::Serialise_generateMipmapsForTexture(SerialiserType &ser,
                                                                       WrappedMTLTexture *texture)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ValidTextureSubresource(texture, 0, 0) || Unwrap(texture)->mipmapLevelCount() < 2)
    {
      RDCERR("Invalid Metal mipmap generation target");
      return false;
    }

    Unwrap(BlitCommandEncoder)->generateMipmaps(Unwrap(texture));
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = "generateMipmapsForTexture";
      action.flags = ActionFlags::GenMips;
      action.copySource = GetResID(texture);
      action.copyDestination = GetResID(texture);
      AddAction(action);
      m_Device->GetReplay()->AddUsage(GetResID(texture), ResourceUsage::GenMips);
    }
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::generateMipmapsForTexture(WrappedMTLTexture *texture)
{
  SERIALISE_TIME_CALL(Unwrap(this)->generateMipmaps(Unwrap(texture)));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_generateMipmapsForTexture);
      Serialise_generateMipmapsForTexture(ser, texture);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(texture), eFrameRef_ReadBeforeWrite);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLBlitCommandEncoder::Serialise_fillBuffer(SerialiserType &ser, WrappedMTLBuffer *buffer,
                                                        NS::Range &range, uint8_t value)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(range);
  SERIALISE_ELEMENT(value).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!ValidBufferRange(buffer, range.location, range.length))
    {
      RDCERR("Invalid Metal buffer fill range: %llu+%llu", uint64_t(range.location),
             uint64_t(range.length));
      return false;
    }

    Unwrap(BlitCommandEncoder)->fillBuffer(Unwrap(buffer), range, value);
    if(IsLoading(m_State))
    {
      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("fillBuffer(%llu bytes at %llu, 0x%02x)",
                                            uint64_t(range.length), uint64_t(range.location), value);
      action.flags = ActionFlags::Clear;
      action.copyDestination = GetResID(buffer);
      AddAction(action);
      m_Device->GetReplay()->AddUsage(action.copyDestination, ResourceUsage::Clear);
    }
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::fillBuffer(WrappedMTLBuffer *buffer, NS::Range &range,
                                              uint8_t value)
{
  SERIALISE_TIME_CALL(Unwrap(this)->fillBuffer(Unwrap(buffer), range, value));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_fillBuffer);
      Serialise_fillBuffer(ser, buffer, range, value);
      chunk = scope.Get();
    }
    MetalResourceRecord *bufferRecord = GetRecord(m_CommandBuffer);
    bufferRecord->AddChunk(chunk);
    bufferRecord->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_PartialWrite);
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLBlitCommandEncoder::Serialise_updateFence(SerialiserType &ser, WrappedMTLFence *fence)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  // TODO: when WrappedMTLFence exists
  //  SERIALISE_ELEMENT(fence).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return false;
}

void WrappedMTLBlitCommandEncoder::updateFence(WrappedMTLFence *fence)
{
  SERIALISE_TIME_CALL(Unwrap(this)->updateFence(Unwrap(fence)));

  // TODO: when WrappedMTLFence exists
  METAL_CAPTURE_NOT_IMPLEMENTED();
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_updateFence);
      Serialise_updateFence(ser, fence);
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
bool WrappedMTLBlitCommandEncoder::Serialise_waitForFence(SerialiserType &ser, WrappedMTLFence *fence)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  // TODO: when WrappedMTLFence exists
  //  SERIALISE_ELEMENT(fence).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return false;
}

void WrappedMTLBlitCommandEncoder::waitForFence(WrappedMTLFence *fence)
{
  SERIALISE_TIME_CALL(Unwrap(this)->waitForFence(Unwrap(fence)));

  // TODO: when WrappedMTLFence exists
  METAL_CAPTURE_NOT_IMPLEMENTED();
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_waitForFence);
      Serialise_waitForFence(ser, fence);
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
bool WrappedMTLBlitCommandEncoder::Serialise_getTextureAccessCounters(
    SerialiserType &ser, WrappedMTLTexture *texture, MTL::Region &region, NS::UInteger mipLevel,
    NS::UInteger slice, bool resetCounters, WrappedMTLBuffer *countersBuffer,
    NS::UInteger countersBufferOffset)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(region);
  SERIALISE_ELEMENT(mipLevel);
  SERIALISE_ELEMENT(slice);
  SERIALISE_ELEMENT(resetCounters);
  SERIALISE_ELEMENT(countersBuffer);
  SERIALISE_ELEMENT(countersBufferOffset);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::getTextureAccessCounters(
    WrappedMTLTexture *texture, MTL::Region &region, NS::UInteger mipLevel, NS::UInteger slice,
    bool resetCounters, WrappedMTLBuffer *countersBuffer, NS::UInteger countersBufferOffset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->getTextureAccessCounters(
      Unwrap(texture), region, mipLevel, slice, resetCounters, Unwrap(countersBuffer),
      countersBufferOffset));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_getTextureAccessCounters);
      Serialise_getTextureAccessCounters(ser, texture, region, mipLevel, slice, resetCounters,
                                         countersBuffer, countersBufferOffset);
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
bool WrappedMTLBlitCommandEncoder::Serialise_resetTextureAccessCounters(SerialiserType &ser,
                                                                        WrappedMTLTexture *texture,
                                                                        MTL::Region &region,
                                                                        NS::UInteger mipLevel,
                                                                        NS::UInteger slice)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(region);
  SERIALISE_ELEMENT(mipLevel);
  SERIALISE_ELEMENT(slice);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::resetTextureAccessCounters(WrappedMTLTexture *texture,
                                                              MTL::Region &region,
                                                              NS::UInteger mipLevel,
                                                              NS::UInteger slice)
{
  SERIALISE_TIME_CALL(
      Unwrap(this)->resetTextureAccessCounters(Unwrap(texture), region, mipLevel, slice));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_resetTextureAccessCounters);
      Serialise_resetTextureAccessCounters(ser, texture, region, mipLevel, slice);
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
bool WrappedMTLBlitCommandEncoder::Serialise_optimizeContentsForGPUAccess(SerialiserType &ser,
                                                                          WrappedMTLTexture *texture)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::optimizeContentsForGPUAccess(WrappedMTLTexture *texture)
{
  SERIALISE_TIME_CALL(Unwrap(this)->optimizeContentsForGPUAccess(Unwrap(texture)));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_optimizeContentsForGPUAccess);
      Serialise_optimizeContentsForGPUAccess(ser, texture);
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
bool WrappedMTLBlitCommandEncoder::Serialise_optimizeContentsForGPUAccess(SerialiserType &ser,
                                                                          WrappedMTLTexture *texture,
                                                                          NS::UInteger slice,
                                                                          NS::UInteger level)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(slice);
  SERIALISE_ELEMENT(level);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::optimizeContentsForGPUAccess(WrappedMTLTexture *texture,
                                                                NS::UInteger slice,
                                                                NS::UInteger level)
{
  SERIALISE_TIME_CALL(Unwrap(this)->optimizeContentsForGPUAccess(Unwrap(texture), slice, level));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(
          MetalChunk::MTLBlitCommandEncoder_optimizeContentsForGPUAccess_slice_level);
      Serialise_optimizeContentsForGPUAccess(ser, texture, slice, level);
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
bool WrappedMTLBlitCommandEncoder::Serialise_optimizeContentsForCPUAccess(SerialiserType &ser,
                                                                          WrappedMTLTexture *texture)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::optimizeContentsForCPUAccess(WrappedMTLTexture *texture)
{
  SERIALISE_TIME_CALL(Unwrap(this)->optimizeContentsForCPUAccess(Unwrap(texture)));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_optimizeContentsForCPUAccess);
      Serialise_optimizeContentsForCPUAccess(ser, texture);
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
bool WrappedMTLBlitCommandEncoder::Serialise_optimizeContentsForCPUAccess(SerialiserType &ser,
                                                                          WrappedMTLTexture *texture,
                                                                          NS::UInteger slice,
                                                                          NS::UInteger level)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  SERIALISE_ELEMENT(texture).Important();
  SERIALISE_ELEMENT(slice);
  SERIALISE_ELEMENT(level);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return true;
}

void WrappedMTLBlitCommandEncoder::optimizeContentsForCPUAccess(WrappedMTLTexture *texture,
                                                                NS::UInteger slice,
                                                                NS::UInteger level)
{
  SERIALISE_TIME_CALL(Unwrap(this)->optimizeContentsForCPUAccess(Unwrap(texture), slice, level));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_optimizeContentsForCPUAccess);
      Serialise_optimizeContentsForCPUAccess(ser, texture, slice, level);
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
bool WrappedMTLBlitCommandEncoder::Serialise_resetCommandsInBuffer(
    SerialiserType &ser, WrappedMTLIndirectCommandBuffer *buffer, NS::Range &range)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  // TODO: when WrappedMTLIndirectCommandBuffer exists
  //  SERIALISE_ELEMENT(buffer).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return false;
}

void WrappedMTLBlitCommandEncoder::resetCommandsInBuffer(WrappedMTLIndirectCommandBuffer *buffer,
                                                         NS::Range &range)
{
  SERIALISE_TIME_CALL(Unwrap(this)->resetCommandsInBuffer(Unwrap(buffer), range));

  // TODO: when WrappedMTLIndirectCommandBuffer exists
  METAL_CAPTURE_NOT_IMPLEMENTED();
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_resetCommandsInBuffer);
      Serialise_resetCommandsInBuffer(ser, buffer, range);
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
bool WrappedMTLBlitCommandEncoder::Serialise_copyIndirectCommandBuffer(
    SerialiserType &ser, WrappedMTLIndirectCommandBuffer *source, NS::Range &sourceRange,
    WrappedMTLIndirectCommandBuffer *destination, NS::UInteger destinationIndex)
{
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  // TODO: when WrappedMTLIndirectCommandBuffer exists
  //  SERIALISE_ELEMENT(source).Important();
  SERIALISE_ELEMENT(sourceRange);
  //  SERIALISE_ELEMENT(destination).Important();
  SERIALISE_ELEMENT(destinationIndex);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return false;
}

void WrappedMTLBlitCommandEncoder::copyIndirectCommandBuffer(
    WrappedMTLIndirectCommandBuffer *source, NS::Range &sourceRange,
    WrappedMTLIndirectCommandBuffer *destination, NS::UInteger destinationIndex)
{
  SERIALISE_TIME_CALL(Unwrap(this)->copyIndirectCommandBuffer(
      Unwrap(source), sourceRange, Unwrap(destination), destinationIndex));

  // TODO: when WrappedMTLIndirectCommandBuffer exists
  METAL_CAPTURE_NOT_IMPLEMENTED();
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_copyIndirectCommandBuffer);
      Serialise_copyIndirectCommandBuffer(ser, source, sourceRange, destination, destinationIndex);
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
bool WrappedMTLBlitCommandEncoder::Serialise_optimizeIndirectCommandBuffer(
    SerialiserType &ser, WrappedMTLIndirectCommandBuffer *indirectCommandBuffer, NS::Range &range)
{
  // TODO: when WrappedMTLIndirectCommandBuffer exists
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  //  SERIALISE_ELEMENT(indirectCommandBuffer).Important();
  SERIALISE_ELEMENT(range);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return false;
}

void WrappedMTLBlitCommandEncoder::optimizeIndirectCommandBuffer(
    WrappedMTLIndirectCommandBuffer *indirectCommandBuffer, NS::Range &range)
{
  SERIALISE_TIME_CALL(
      Unwrap(this)->optimizeIndirectCommandBuffer(Unwrap(indirectCommandBuffer), range));

  // TODO: when WrappedMTLIndirectCommandBuffer exists
  METAL_CAPTURE_NOT_IMPLEMENTED();
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_optimizeIndirectCommandBuffer);
      Serialise_optimizeIndirectCommandBuffer(ser, indirectCommandBuffer, range);
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
bool WrappedMTLBlitCommandEncoder::Serialise_sampleCountersInBuffer(
    SerialiserType &ser, WrappedMTLCounterSampleBuffer *sampleBuffer, NS::UInteger sampleIndex,
    bool barrier)
{
  // TODO: when WrappedMTLCounterSampleBuffer exists
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  //  SERIALISE_ELEMENT(sampleBuffer).Important();
  SERIALISE_ELEMENT(sampleIndex);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return false;
}

void WrappedMTLBlitCommandEncoder::sampleCountersInBuffer(WrappedMTLCounterSampleBuffer *sampleBuffer,
                                                          NS::UInteger sampleIndex, bool barrier)
{
  SERIALISE_TIME_CALL(
      Unwrap(this)->sampleCountersInBuffer(Unwrap(sampleBuffer), sampleIndex, barrier));

  // TODO: when WrappedMTLCounterSampleBuffer exists
  METAL_CAPTURE_NOT_IMPLEMENTED();
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_sampleCountersInBuffer);
      Serialise_sampleCountersInBuffer(ser, sampleBuffer, sampleIndex, barrier);
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
bool WrappedMTLBlitCommandEncoder::Serialise_resolveCounters(
    SerialiserType &ser, WrappedMTLCounterSampleBuffer *sampleBuffer, NS::Range &range,
    WrappedMTLBuffer *destinationBuffer, NS::UInteger destinationOffset)
{
  // TODO: when WrappedMTLCounterSampleBuffer exists
  SERIALISE_ELEMENT_LOCAL(BlitCommandEncoder, this);
  //  SERIALISE_ELEMENT(sampleBuffer).Important();
  SERIALISE_ELEMENT(range);
  SERIALISE_ELEMENT(destinationBuffer).Important();
  SERIALISE_ELEMENT(destinationOffset);

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
  }
  return false;
}

void WrappedMTLBlitCommandEncoder::resolveCounters(WrappedMTLCounterSampleBuffer *sampleBuffer,
                                                   NS::Range &range,
                                                   WrappedMTLBuffer *destinationBuffer,
                                                   NS::UInteger destinationOffset)
{
  SERIALISE_TIME_CALL(Unwrap(this)->resolveCounters(Unwrap(sampleBuffer), range,
                                                    Unwrap(destinationBuffer), destinationOffset));

  // TODO: when WrappedMTLCounterSampleBuffer exists
  METAL_CAPTURE_NOT_IMPLEMENTED();
  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBlitCommandEncoder_resolveCounters);
      Serialise_resolveCounters(ser, sampleBuffer, range, destinationBuffer, destinationOffset);
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

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, setLabel, NS::String *value);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, endEncoding);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, insertDebugSignpost,
                                NS::String *string);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, pushDebugGroup,
                                NS::String *string);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, popDebugGroup);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, synchronizeResource,
                                WrappedMTLResource *resource);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, synchronizeTexture,
                                WrappedMTLTexture *texture, NS::UInteger slice, NS::UInteger level);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, copyFromBuffer,
                                WrappedMTLBuffer *sourceBuffer, NS::UInteger sourceOffset,
                                WrappedMTLBuffer *destinationBuffer, NS::UInteger destinationOffset,
                                NS::UInteger size);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, copyFromBuffer,
                                WrappedMTLBuffer *sourceBuffer, NS::UInteger sourceOffset,
                                NS::UInteger sourceBytesPerRow, NS::UInteger sourceBytesPerImage,
                                MTL::Size &sourceSize, WrappedMTLTexture *destinationTexture,
                                NS::UInteger destinationSlice, NS::UInteger destinationLevel,
                                MTL::Origin &destinationOrigin, MTL::BlitOption options);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, copyFromTexture,
                                WrappedMTLTexture *sourceTexture, NS::UInteger sourceSlice,
                                NS::UInteger sourceLevel, MTL::Origin &sourceOrigin,
                                MTL::Size &sourceSize, WrappedMTLTexture *destinationTexture,
                                NS::UInteger destinationSlice, NS::UInteger destinationLevel,
                                MTL::Origin &destinationOrigin);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, copyFromTexture,
                                WrappedMTLTexture *sourceTexture, NS::UInteger sourceSlice,
                                NS::UInteger sourceLevel, MTL::Origin &sourceOrigin,
                                MTL::Size &sourceSize, WrappedMTLBuffer *destinationBuffer,
                                NS::UInteger destinationOffset, NS::UInteger destinationBytesPerRow,
                                NS::UInteger destinationBytesPerImage, MTL::BlitOption options);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, copyFromTexture,
                                WrappedMTLTexture *sourceTexture,
                                WrappedMTLTexture *destinationTexture);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, copyFromTexture,
                                WrappedMTLTexture *sourceTexture, NS::UInteger sourceSlice,
                                NS::UInteger sourceLevel, WrappedMTLTexture *destinationTexture,
                                NS::UInteger destinationSlice, NS::UInteger destinationLevel,
                                NS::UInteger sliceCount, NS::UInteger levelCount);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, generateMipmapsForTexture,
                                WrappedMTLTexture *texture);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, fillBuffer,
                                WrappedMTLBuffer *buffer, NS::Range &range, uint8_t value);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, updateFence,
                                WrappedMTLFence *fence);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, waitForFence,
                                WrappedMTLFence *fence);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, getTextureAccessCounters,
                                WrappedMTLTexture *texture, MTL::Region &region,
                                NS::UInteger mipLevel, NS::UInteger slice, bool resetCounters,
                                WrappedMTLBuffer *countersBuffer, NS::UInteger countersBufferOffset);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, resetTextureAccessCounters,
                                WrappedMTLTexture *texture, MTL::Region &region,
                                NS::UInteger mipLevel, NS::UInteger slice);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, optimizeContentsForGPUAccess,
                                WrappedMTLTexture *texture);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, optimizeContentsForGPUAccess,
                                WrappedMTLTexture *texture, NS::UInteger slice, NS::UInteger level);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, optimizeContentsForCPUAccess,
                                WrappedMTLTexture *texture);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, optimizeContentsForCPUAccess,
                                WrappedMTLTexture *texture, NS::UInteger slice, NS::UInteger level);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, resetCommandsInBuffer,
                                WrappedMTLIndirectCommandBuffer *buffer, NS::Range &range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, copyIndirectCommandBuffer,
                                WrappedMTLIndirectCommandBuffer *source, NS::Range &sourceRange,
                                WrappedMTLIndirectCommandBuffer *destination,
                                NS::UInteger destinationIndex);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, optimizeIndirectCommandBuffer,
                                WrappedMTLIndirectCommandBuffer *indirectCommandBuffer,
                                NS::Range &range);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, sampleCountersInBuffer,
                                WrappedMTLCounterSampleBuffer *sampleBuffer,
                                NS::UInteger sampleIndex, bool barrier);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBlitCommandEncoder, void, resolveCounters,
                                WrappedMTLCounterSampleBuffer *sampleBuffer, NS::Range &range,
                                WrappedMTLBuffer *destinationBuffer, NS::UInteger destinationOffset);
