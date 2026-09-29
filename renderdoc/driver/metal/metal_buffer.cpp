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

#include "metal_buffer.h"
#include "core/core.h"
#include "metal_device.h"
#include "metal_manager.h"
#include "metal_replay.h"
#include "metal_resources.h"
#include "metal_texture.h"

WrappedMTLBuffer::WrappedMTLBuffer(MTL::Buffer *realMTLBuffer, ResourceId objId,
                                   WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLBuffer, objId, wrappedMTLDevice, wrappedMTLDevice->GetStateRef())
{
  if(realMTLBuffer && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLBuffer::Serialise_makeAliasable(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(Buffer, this).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Buffer || Buffer->m_Type != eResBuffer || !Buffer->m_Real ||
       !Unwrap(Buffer)->heap())
    {
      RDCERR("Invalid Metal heap buffer aliasable resource");
      return false;
    }
    Unwrap(Buffer)->makeAliasable();
  }
  return true;
}

void WrappedMTLBuffer::makeAliasable()
{
  SERIALISE_TIME_CALL(Unwrap(this)->makeAliasable());
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_makeAliasable);
    Serialise_makeAliasable(ser);
    if(IsActiveCapturing(m_State))
    {
      m_Device->AddFrameCaptureRecordChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(m_ID, eFrameRef_Read);
    }
    else
      GetRecord(this)->AddChunk(scope.Get());
  }
}

template bool WrappedMTLBuffer::Serialise_makeAliasable(ReadSerialiser &);
template bool WrappedMTLBuffer::Serialise_makeAliasable(WriteSerialiser &);

template <typename SerialiserType>
bool WrappedMTLBuffer::Serialise_setPurgeableState(SerialiserType &ser, MTL::PurgeableState state)
{
  SERIALISE_ELEMENT_LOCAL(Buffer, this).Important();
  SERIALISE_ELEMENT_LOCAL(State, (uint32_t)state).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!Buffer || Buffer->m_Type != eResBuffer || !Buffer->m_Real ||
       (State != MTL::PurgeableStateKeepCurrent && State != MTL::PurgeableStateNonVolatile &&
        State != MTL::PurgeableStateEmpty) ||
       (m_Device->GetReplayEpoch() != 0 &&
        (State == MTL::PurgeableStateVolatile || State == MTL::PurgeableStateEmpty)))
    {
      RDCERR("Invalid or unsupported Metal buffer purgeable state or replay lifetime");
      return false;
    }
    Unwrap(Buffer)->setPurgeableState((MTL::PurgeableState)State);
  }
  return true;
}

MTL::PurgeableState WrappedMTLBuffer::setPurgeableState(MTL::PurgeableState state)
{
  MTL::PurgeableState previous = MTL::PurgeableStateKeepCurrent;
  SERIALISE_TIME_CALL(previous = Unwrap(this)->setPurgeableState(state));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_setPurgeableState);
    Serialise_setPurgeableState(ser, state);
    if(IsActiveCapturing(m_State))
    {
      m_Device->AddFrameCaptureRecordChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(m_ID, eFrameRef_Read);
    }
    else
    {
      GetRecord(this)->AddChunk(scope.Get());
    }
  }
  return previous;
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBuffer, MTL::PurgeableState, setPurgeableState,
                                MTL::PurgeableState);

template <typename SerialiserType>
bool WrappedMTLBuffer::Serialise_newTextureWithDescriptor(
    SerialiserType &ser, WrappedMTLTexture *texture, RDMTL::TextureDescriptor &descriptor,
    NS::UInteger offset, NS::UInteger bytesPerRow)
{
  SERIALISE_ELEMENT_LOCAL(Buffer, this).Important();
  SERIALISE_ELEMENT_LOCAL(Texture, GetResID(texture)).TypedAs("MTLTexture"_lit).Important();
  SERIALISE_ELEMENT(descriptor).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(bytesPerRow).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    const bool frameView = m_Device->GetReplayEpoch() != 0;
    const bool recreateFrameView =
        IsActiveReplaying(m_State) && m_Device->IsFrameBufferTextureView(Texture) &&
        GetResourceManager()->HasResource(Texture) &&
        !GetResourceManager()->GetResource(Texture)->m_Real;
    uint64_t pixelBytes = 0;
    switch(descriptor.pixelFormat)
    {
      case MTL::PixelFormatR16Float: pixelBytes = 2; break;
      case MTL::PixelFormatR32Uint:
      case MTL::PixelFormatR32Sint:
      case MTL::PixelFormatR32Float:
      case MTL::PixelFormatRG16Float:
      case MTL::PixelFormatRGBA8Snorm: pixelBytes = 4; break;
      case MTL::PixelFormatRG32Uint:
      case MTL::PixelFormatRG32Float:
      case MTL::PixelFormatRGBA16Snorm:
      case MTL::PixelFormatRGBA16Float: pixelBytes = 8; break;
      case MTL::PixelFormatRGBA32Uint:
      case MTL::PixelFormatRGBA32Float: pixelBytes = 16; break;
      // The earlier Shared 2D fixture remains supported.
      case MTL::PixelFormatRGBA8Unorm:
      case MTL::PixelFormatBGRA8Unorm: pixelBytes = 4; break;
      default: break;
    }
    const bool textureBuffer = descriptor.textureType == MTL::TextureTypeTextureBuffer;
    const bool shared2D = descriptor.textureType == MTL::TextureType2D &&
                          (descriptor.pixelFormat == MTL::PixelFormatRGBA8Unorm ||
                           descriptor.pixelFormat == MTL::PixelFormatBGRA8Unorm);
    const uint64_t bufferLength = Buffer && Buffer->m_Type == eResBuffer && Buffer->m_Real
                                      ? Unwrap(Buffer)->length() : 0;
    if(!Buffer || Buffer->m_Type != eResBuffer || !Buffer->m_Real ||
       Texture == ResourceId() ||
       (GetResourceManager()->HasResource(Texture) && !recreateFrameView) ||
       (!shared2D && !textureBuffer) || !pixelBytes ||
       (shared2D && (Unwrap(Buffer)->storageMode() != MTL::StorageModeShared ||
                     descriptor.storageMode != MTL::StorageModeShared)) ||
       (textureBuffer && (Unwrap(Buffer)->storageMode() != MTL::StorageModePrivate ||
                          descriptor.storageMode != MTL::StorageModePrivate ||
                          descriptor.resourceOptions != MTL::ResourceStorageModePrivate ||
                          descriptor.hazardTrackingMode != MTL::HazardTrackingModeDefault ||
                          descriptor.allowGPUOptimizedContents ||
                          (descriptor.usage != MTL::TextureUsageShaderRead &&
                           descriptor.usage != (MTL::TextureUsageShaderRead |
                                                MTL::TextureUsageShaderWrite)))) ||
       descriptor.width == 0 || descriptor.height == 0 || descriptor.depth != 1 ||
       descriptor.mipmapLevelCount != 1 || descriptor.arrayLength != 1 ||
       descriptor.sampleCount != 1 || descriptor.width > UINT64_MAX / pixelBytes ||
       descriptor.width > (textureBuffer ? 1114112 : 16384) ||
       (textureBuffer && descriptor.height != 1) ||
       bytesPerRow < descriptor.width * pixelBytes || offset > bufferLength ||
       bytesPerRow == 0 || descriptor.height > (bufferLength - offset) / bytesPerRow)
    {
      RDCERR("Invalid or unsupported Metal buffer-backed texture identity, descriptor or range");
      fprintf(stderr, "Metal buffer texture rejected: type=%llu format=%llu width=%llu height=%llu options=%llu storage=%llu hazard=%llu usage=%llu optimized=%d offset=%llu row=%llu bufferLength=%llu bufferStorage=%llu\n",
              (uint64_t)descriptor.textureType, (uint64_t)descriptor.pixelFormat,
              (uint64_t)descriptor.width, (uint64_t)descriptor.height,
              (uint64_t)descriptor.resourceOptions, (uint64_t)descriptor.storageMode,
              (uint64_t)descriptor.hazardTrackingMode, (uint64_t)descriptor.usage,
              descriptor.allowGPUOptimizedContents ? 1 : 0, (uint64_t)offset,
              (uint64_t)bytesPerRow, bufferLength,
              Buffer && Buffer->m_Type == eResBuffer && Buffer->m_Real
                  ? (uint64_t)Unwrap(Buffer)->storageMode() : 999ULL);
      return false;
    }
    const uint64_t linearAlignment = Unwrap(m_Device)->minimumLinearTextureAlignmentForPixelFormat(
        descriptor.pixelFormat);
    const uint64_t bufferAlignment = Unwrap(m_Device)->minimumTextureBufferAlignmentForPixelFormat(
        descriptor.pixelFormat);
    if(linearAlignment == 0 || bufferAlignment == 0 || offset % bufferAlignment ||
       bytesPerRow % linearAlignment)
    {
      RDCERR("Invalid Metal buffer-backed texture alignment");
      return false;
    }
    MTL::TextureDescriptor *nativeDescriptor(descriptor);
    MTL::Texture *real = Unwrap(Buffer)->newTexture(nativeDescriptor, offset, bytesPerRow);
    nativeDescriptor->release();
    if(!real)
    {
      RDCERR("Metal failed to create buffer-backed texture from captured parameters");
      return false;
    }
    if(recreateFrameView)
      GetResourceManager()->ReplaceRealResource(GetResourceManager()->GetResource(Texture), real, true);
    else
    {
      WrappedMTLTexture *wrapped = NULL;
      GetResourceManager()->WrapResource(Texture, real, wrapped, true);
      m_Device->AddResource(Texture, ResourceType::Texture, "Buffer Texture");
      m_Device->GetReplay()->AddTexture(Texture, real, false);
      m_Device->DerivedResource(Buffer, Texture);
      if(frameView && IsLoading(m_State))
        m_Device->RegisterFrameBufferTextureView(Texture);
    }
  }
  return true;
}

WrappedMTLTexture *WrappedMTLBuffer::newTextureWithDescriptor(
    RDMTL::TextureDescriptor &descriptor, NS::UInteger offset, NS::UInteger bytesPerRow)
{
  MTL::TextureDescriptor *nativeDescriptor(descriptor);
  MTL::Texture *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newTexture(nativeDescriptor, offset, bytesPerRow));
  nativeDescriptor->release();
  if(!real)
    return NULL;
  WrappedMTLTexture *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_newTextureWithDescriptor);
    Serialise_newTextureWithDescriptor(ser, wrapped, descriptor, offset, bytesPerRow);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    Chunk *creation = scope.Get();
    record->AddChunk(creation);
    if(IsActiveCapturing(m_State))
    {
      // A view of a buffer created during this frame must be replayed after its parent.
      // Keep the creation record for later captures, but emit this capture's creation in
      // the ordered frame stream just like placement buffer creation.
      m_Device->AddFrameCaptureRecordChunk(creation->Duplicate());
      m_Device->RegisterCapturedFrameResource(GetResID(wrapped));
    }
    m_Device->RegisterBufferTextureParent(GetResID(wrapped), GetResID(this));
  }
  return wrapped;
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBuffer, bool, newTextureWithDescriptor,
                                WrappedMTLTexture *, RDMTL::TextureDescriptor &,
                                NS::UInteger, NS::UInteger);

template <typename SerialiserType>
bool WrappedMTLBuffer::Serialise_addDebugMarker(SerialiserType &ser, NS::String *marker,
                                                        NS::Range range)
{
  SERIALISE_ELEMENT_LOCAL(Buffer, this);
  SERIALISE_ELEMENT(marker).Important();
  SERIALISE_ELEMENT(range).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Buffer || Buffer->m_Type != eResBuffer || !Buffer->m_Real ||
       range.location > Unwrap(Buffer)->length() ||
       range.length > Unwrap(Buffer)->length() - range.location)
    {
      RDCERR("Invalid Metal buffer debug marker resource or range");
      return false;
    }
    // Preserve annotations as structured data, not persistent native debug state across seeks.
  }
  return true;
}

void WrappedMTLBuffer::addDebugMarker(NS::String *marker, NS::Range range)
{
  SERIALISE_TIME_CALL(Unwrap(this)->addDebugMarker(marker, range));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_addDebugMarker);
    Serialise_addDebugMarker(ser, marker, range);
    if(IsActiveCapturing(m_State))
    {
      m_Device->AddFrameCaptureRecordChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(m_ID, eFrameRef_Read);
    }
    else
    {
      GetRecord(this)->AddChunk(scope.Get());
    }
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBuffer, void, addDebugMarker, NS::String *marker,
                                NS::Range range);

template <typename SerialiserType>
bool WrappedMTLBuffer::Serialise_removeAllDebugMarkers(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(Buffer, this);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Buffer || Buffer->m_Type != eResBuffer || !Buffer->m_Real)
    {
      RDCERR("Invalid Metal buffer debug marker resource or range");
      return false;
    }
    // Preserve annotations as structured data, not persistent native debug state across seeks.
  }
  return true;
}

void WrappedMTLBuffer::removeAllDebugMarkers()
{
  SERIALISE_TIME_CALL(Unwrap(this)->removeAllDebugMarkers());
  if(IsCaptureMode(m_State))
  {
    if(IsBackgroundCapturing(m_State))
      GetRecord(this)->DiscardBackgroundBufferMarkers();
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_removeAllDebugMarkers);
    Serialise_removeAllDebugMarkers(ser);
    if(IsActiveCapturing(m_State))
    {
      m_Device->AddFrameCaptureRecordChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(m_ID, eFrameRef_Read);
    }
    else
    {
      GetRecord(this)->AddChunk(scope.Get());
    }
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBuffer, void, removeAllDebugMarkers);


void *WrappedMTLBuffer::contents()
{
  void *data = Unwrap(this)->contents();

  if(IsCaptureMode(m_State))
  {
    // Snapshot potentially CPU modified buffer if the returned pointer is not NULL
    if(data)
    {
      GetResourceManager()->MarkDirtyResource(m_ID);
    }
  }
  else
  {
    // TODO: implement RD MTL replay
  }
  return data;
}

template <typename SerialiserType>
bool WrappedMTLBuffer::Serialise_didModifyRange(SerialiserType &ser, NS::Range &range)
{
  SERIALISE_ELEMENT_LOCAL(Buffer, this);
  SERIALISE_ELEMENT(range).Important();
  byte *pData = NULL;
  uint64_t memSize = range.length;
  if(ser.IsWriting())
  {
    pData = (byte *)Unwrap(this)->contents() + range.location;
  }
  if(IsReplayingAndReading())
  {
    pData = (byte *)Unwrap(Buffer)->contents() + range.location;
  }

  // serialise directly using buffer memory
  ser.Serialise("data"_lit, pData, memSize, SerialiserFlags::NoFlags).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    Unwrap(Buffer)->didModifyRange(range);
  }
  return true;
}

void WrappedMTLBuffer::didModifyRange(NS::Range &range)
{
  SERIALISE_TIME_CALL(Unwrap(this)->didModifyRange(range));
  if(IsCaptureMode(m_State))
  {
    if(IsBackgroundCapturing(m_State))
    {
      // Snapshot potentially CPU modified buffer
      GetResourceManager()->MarkDirtyResource(m_ID);
    }
    else
    {
      Chunk *chunk = NULL;
      {
        CACHE_THREAD_SERIALISER();
        SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_didModifyRange);
        Serialise_didModifyRange(ser, range);
        chunk = scope.Get();
      }
      m_Device->AddFrameCaptureRecordChunk(chunk);
    }
  }
  else
  {
    // TODO: implement RD MTL replay
  }
}

template <typename SerialiserType>
bool WrappedMTLBuffer::Serialise_InternalModifyCPUContents(SerialiserType &ser, uint64_t start,
                                                           uint64_t end, MetalBufferInfo *bufInfo)
{
  SERIALISE_ELEMENT_LOCAL(Buffer, this).Important();
  SERIALISE_ELEMENT(start).Important();
  uint64_t size = ser.IsWriting() ? end - start : 0;
  SERIALISE_ELEMENT(size).Important();
  bytebuf data;
  if(ser.IsWriting())
  {
    data.assign((byte *)Unwrap(this)->contents() + start, size);
  }

  // bytebuf uses the same length/alignment/payload format as the old raw byte pointer path.
  // Read into owned memory so malformed payload lengths cannot overwrite a live GPU buffer.
  ser.Serialise("data"_lit, data);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() &&
     (size != data.size() || !m_Device->ReplayCPUBufferUpdate(Buffer, start, data)))
  {
    RDCERR("Invalid Metal CPU buffer update range, payload or resource");
    return false;
  }

  if(IsCaptureMode(m_State))
  {
    // update the base snapshot from the serialised data
    if(bufInfo->baseSnapshot.isEmpty())
      bufInfo->baseSnapshot.resize(bufInfo->length);
    RDCASSERTEQUAL(bufInfo->baseSnapshot.size(), bufInfo->length);
    memcpy(bufInfo->baseSnapshot.data() + start, data.data(), size);
  }

  SERIALISE_CHECK_READ_ERRORS();

  return true;
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBuffer, void, didModifyRange, NS::Range &);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLBuffer, void, InternalModifyCPUContents, uint64_t,
                                uint64_t, MetalBufferInfo *);
