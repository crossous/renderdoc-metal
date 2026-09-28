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

#include "metal_texture.h"
#include "metal_device.h"
#include "metal_manager.h"
#include "metal_replay.h"

// No host pointer or returned bytes are stored. Validate the recorded linear layout even though
// replay does not perform a CPU read: application writes derived from it are captured separately.
// Packed/compressed/depth formats require dedicated block/aspect pitch rules.
static bool ValidCPUTextureRead(WrappedMTLTexture *texture, const MTL::Region &region,
                                uint64_t level, uint64_t slice, uint64_t rowPitch,
                                uint64_t imagePitch)
{
  if(!texture || texture->m_Type != eResTexture || !Unwrap(texture))
    return false;
  MTL::Texture *real = Unwrap(texture);
  if(level >= real->mipmapLevelCount() || level >= 64 || real->sampleCount() != 1 ||
     real->framebufferOnly() || (real->storageMode() != MTL::StorageModeShared &&
                                 real->storageMode() != MTL::StorageModeManaged))
    return false;
  uint64_t slices = RDCMAX(1ULL, uint64_t(real->arrayLength()));
  if(real->textureType() == MTL::TextureTypeCube || real->textureType() == MTL::TextureTypeCubeArray)
    slices *= 6;
  const uint64_t width = RDCMAX(1ULL, uint64_t(real->width()) >> level);
  const uint64_t height = RDCMAX(1ULL, uint64_t(real->height()) >> level);
  const uint64_t depth = RDCMAX(1ULL, uint64_t(real->depth()) >> level);
  const MTL::Origin &origin = region.origin;
  const MTL::Size &size = region.size;
  if(slice >= slices || !size.width || !size.height || !size.depth || origin.x > width ||
     size.width > width - origin.x || origin.y > height || size.height > height - origin.y ||
     origin.z > depth || size.depth > depth - origin.z)
    return false;
  const ResourceFormat format = MakeResourceFormat(real->pixelFormat());
  if(format.Special() || format.compType == CompType::Depth || !format.compByteWidth ||
     !format.compCount)
    return false;
  const uint64_t pixelSize = uint64_t(format.compByteWidth) * format.compCount;
  if(size.width > UINT64_MAX / pixelSize || rowPitch < size.width * pixelSize ||
     rowPitch % pixelSize || size.height - 1 > (UINT64_MAX - size.width * pixelSize) / rowPitch)
    return false;
  const uint64_t imageBytes = (size.height - 1) * rowPitch + size.width * pixelSize;
  if(size.depth > 1 && (!imagePitch || imagePitch % rowPitch ||
                        size.height > imagePitch / rowPitch ||
                        size.depth - 1 > (UINT64_MAX - imageBytes) / imagePitch))
    return false;
  return true;
}

WrappedMTLTexture::WrappedMTLTexture(MTL::Texture *realMTLTexture, ResourceId objId,
                                     WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLTexture, objId, wrappedMTLDevice, wrappedMTLDevice->GetStateRef())
{
  if(realMTLTexture && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLTexture::Serialise_makeAliasable(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(Texture, this).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!Texture || Texture->m_Type != eResTexture || !Texture->m_Real ||
       !Unwrap(Texture)->heap())
    {
      RDCERR("Invalid Metal heap texture aliasable resource");
      return false;
    }
    Unwrap(Texture)->makeAliasable();
  }
  return true;
}

void WrappedMTLTexture::makeAliasable()
{
  SERIALISE_TIME_CALL(Unwrap(this)->makeAliasable());
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLTexture_makeAliasable);
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

template bool WrappedMTLTexture::Serialise_makeAliasable(ReadSerialiser &);
template bool WrappedMTLTexture::Serialise_makeAliasable(WriteSerialiser &);

template <typename SerialiserType>
bool WrappedMTLTexture::Serialise_newSharedTextureHandle(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(Texture, GetResID(this)).TypedAs("MTLTexture"_lit).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(Texture == ResourceId() || !GetResourceManager()->HasResource(Texture))
    {
      RDCERR("Invalid Metal shared texture handle source identity");
      return false;
    }
    WrappedMTLTexture *source =
        (WrappedMTLTexture *)GetResourceManager()->GetResource(Texture);
    if(!source || source->m_Type != eResTexture || !source->m_Real)
    {
      RDCERR("Invalid Metal shared texture handle source type");
      return false;
    }
  }
  // Exporting a handle has no GPU effect. Imports recreate it from this source.
  return true;
}

void WrappedMTLTexture::newSharedTextureHandle()
{
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLTexture_newSharedTextureHandle);
    Serialise_newSharedTextureHandle(ser);
    GetRecord(this)->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLTexture, void, newSharedTextureHandle);

template <typename SerialiserType>
bool WrappedMTLTexture::Serialise_setPurgeableState(SerialiserType &ser, MTL::PurgeableState state)
{
  SERIALISE_ELEMENT_LOCAL(Texture, this).Important();
  SERIALISE_ELEMENT_LOCAL(State, (uint32_t)state).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!Texture || Texture->m_Type != eResTexture || !Texture->m_Real ||
       (State != MTL::PurgeableStateKeepCurrent && State != MTL::PurgeableStateNonVolatile))
    {
      RDCERR("Invalid or unsupported Metal texture purgeable state");
      return false;
    }
    Unwrap(Texture)->setPurgeableState((MTL::PurgeableState)State);
  }
  return true;
}

MTL::PurgeableState WrappedMTLTexture::setPurgeableState(MTL::PurgeableState state)
{
  MTL::PurgeableState previous = MTL::PurgeableStateKeepCurrent;
  SERIALISE_TIME_CALL(previous = Unwrap(this)->setPurgeableState(state));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLTexture_setPurgeableState);
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

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLTexture, MTL::PurgeableState, setPurgeableState,
                                MTL::PurgeableState);

static bool ValidTextureView(MTL::Texture *source, MTL::PixelFormat format,
                             MTL::TextureType type, NS::Range levels, NS::Range slices,
                             MTL::TextureSwizzleChannels swizzle, uint32_t variant)
{
  if(!source || variant > 2 ||
     (source->pixelFormat() != MTL::PixelFormatRGBA8Unorm &&
      source->pixelFormat() != MTL::PixelFormatBGRA8Unorm) ||
     format != source->pixelFormat() || source->storageMode() != MTL::StorageModeShared ||
     source->sampleCount() != 1 || source->framebufferOnly() || source->parentTexture())
    return false;
  if(source->textureType() != MTL::TextureType2D &&
     source->textureType() != MTL::TextureType2DArray)
    return false;
  if(variant == 0)
    return true;
  if(type != MTL::TextureType2D && type != MTL::TextureType2DArray)
    return false;
  const uint64_t availableSlices = source->textureType() == MTL::TextureType2D ? 1 :
                                   source->arrayLength();
  if(!levels.length || levels.location >= source->mipmapLevelCount() ||
     levels.length > source->mipmapLevelCount() - levels.location || !slices.length ||
     slices.location >= availableSlices || slices.length > availableSlices - slices.location ||
     (type == MTL::TextureType2D && slices.length != 1) ||
     (type == MTL::TextureType2DArray && source->textureType() != MTL::TextureType2DArray))
    return false;
  if(variant == 2)
    for(MTL::TextureSwizzle channel : {swizzle.red, swizzle.green, swizzle.blue, swizzle.alpha})
      if(channel > MTL::TextureSwizzleAlpha)
        return false;
  return true;
}

template <typename SerialiserType>
bool WrappedMTLTexture::Serialise_newTextureView(SerialiserType &ser, WrappedMTLTexture *view,
                                                 MTL::PixelFormat format, MTL::TextureType type,
                                                 NS::Range levels, NS::Range slices,
                                                 MTL::TextureSwizzleChannels swizzle,
                                                 uint32_t variant)
{
  SERIALISE_ELEMENT_LOCAL(Source, this).Important();
  SERIALISE_ELEMENT_LOCAL(View, GetResID(view)).TypedAs("MTLTexture"_lit).Important();
  SERIALISE_ELEMENT(format).Important();
  SERIALISE_ELEMENT(type).Important();
  SERIALISE_ELEMENT(levels).Important();
  SERIALISE_ELEMENT(slices).Important();
  SERIALISE_ELEMENT(swizzle).Important();
  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(!Source || Source->m_Type != eResTexture || !Source->m_Real ||
       View == ResourceId() || GetResourceManager()->HasResource(View) ||
       !ValidTextureView(Unwrap(Source), format, type, levels, slices, swizzle, variant))
    {
      RDCERR("Invalid or unsupported Metal texture view source, format or subresource range");
      return false;
    }
    MTL::Texture *real = NULL;
    if(variant == 0)
      real = Unwrap(Source)->newTextureView(format);
    else if(variant == 1)
      real = Unwrap(Source)->newTextureView(format, type, levels, slices);
    else
      real = Unwrap(Source)->newTextureView(format, type, levels, slices, swizzle);
    if(!real)
    {
      RDCERR("Metal failed to create texture view from captured parameters");
      return false;
    }
    WrappedMTLTexture *wrapped = NULL;
    GetResourceManager()->WrapResource(View, real, wrapped, true);
    m_Device->AddResource(View, ResourceType::Texture, "Texture View");
    m_Device->GetReplay()->AddTexture(View, real, false);
    m_Device->GetReplay()->RegisterTextureViewSource(GetResID(Source));
    m_Device->DerivedResource(Source, View);
  }
  return true;
}

WrappedMTLTexture *WrappedMTLTexture::newTextureView(MTL::PixelFormat format,
                                                     MTL::TextureType type, NS::Range levels,
                                                     NS::Range slices,
                                                     MTL::TextureSwizzleChannels swizzle,
                                                     uint32_t variant)
{
  MTL::Texture *real = NULL;
  if(variant == 0)
  {
    SERIALISE_TIME_CALL(real = Unwrap(this)->newTextureView(format));
  }
  else if(variant == 1)
  {
    SERIALISE_TIME_CALL(real = Unwrap(this)->newTextureView(format, type, levels, slices));
  }
  else if(variant == 2)
  {
    SERIALISE_TIME_CALL(real = Unwrap(this)->newTextureView(format, type, levels, slices, swizzle));
  }
  if(!real)
    return NULL;
  WrappedMTLTexture *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    const MetalChunk chunks[] = {MetalChunk::MTLTexture_newTextureViewWithPixelFormat,
                                 MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset,
                                 MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset_swizzle};
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunks[variant]);
    Serialise_newTextureView(ser, wrapped, format, type, levels, slices, swizzle, variant);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(this));
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLTexture::Serialise_getBytes(SerialiserType &ser, void *pixelBytes,
                                          NS::UInteger bytesPerRow, MTL::Region &region,
                                          NS::UInteger level)
{
  SERIALISE_ELEMENT_LOCAL(Texture, this).Important();
  SERIALISE_ELEMENT(bytesPerRow).Important();
  SERIALISE_ELEMENT(region).Important();
  SERIALISE_ELEMENT(level).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && !ValidCPUTextureRead(Texture, region, level, 0, bytesPerRow, 0))
  {
    RDCERR("Invalid or unsupported Metal texture CPU readback layout");
    return false;
  }
  return true;
}

void WrappedMTLTexture::getBytes(void *pixelBytes, NS::UInteger bytesPerRow, MTL::Region &region,
                                NS::UInteger level)
{
  SERIALISE_TIME_CALL(Unwrap(this)->getBytes(pixelBytes, bytesPerRow, region, level));
  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLTexture_getBytes);
    Serialise_getBytes(ser, pixelBytes, bytesPerRow, region, level);
    m_Device->AddFrameCaptureRecordChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(m_ID, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLTexture::Serialise_getBytes(SerialiserType &ser, void *pixelBytes,
                                          NS::UInteger bytesPerRow, NS::UInteger bytesPerImage,
                                          MTL::Region &region, NS::UInteger level, NS::UInteger slice)
{
  SERIALISE_ELEMENT_LOCAL(Texture, this).Important();
  SERIALISE_ELEMENT(bytesPerRow).Important();
  SERIALISE_ELEMENT(bytesPerImage).Important();
  SERIALISE_ELEMENT(region).Important();
  SERIALISE_ELEMENT(level).Important();
  SERIALISE_ELEMENT(slice).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() &&
     !ValidCPUTextureRead(Texture, region, level, slice, bytesPerRow, bytesPerImage))
  {
    RDCERR("Invalid or unsupported Metal texture CPU readback slice/layout");
    return false;
  }
  return true;
}

void WrappedMTLTexture::getBytes(void *pixelBytes, NS::UInteger bytesPerRow,
                                NS::UInteger bytesPerImage, MTL::Region &region,
                                NS::UInteger level, NS::UInteger slice)
{
  SERIALISE_TIME_CALL(Unwrap(this)->getBytes(pixelBytes, bytesPerRow, bytesPerImage, region, level, slice));
  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLTexture_getBytes_slice);
    Serialise_getBytes(ser, pixelBytes, bytesPerRow, bytesPerImage, region, level, slice);
    m_Device->AddFrameCaptureRecordChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(m_ID, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedMTLTexture::Serialise_replaceRegion(SerialiserType &ser, MTL::Region &region,
                                                 NS::UInteger level, const void *pixelBytes,
                                                 NS::UInteger bytesPerRow)
{
  SERIALISE_ELEMENT_LOCAL(Texture, this).Important();
  SERIALISE_ELEMENT(region).Important();
  SERIALISE_ELEMENT(level).Important();
  SERIALISE_ELEMENT(bytesPerRow).Important();

  bytebuf contents;
  if(ser.IsWriting() && pixelBytes && bytesPerRow > 0 && region.size.height > 0)
  {
    const size_t dataSize = size_t(bytesPerRow) * size_t(region.size.height);
    contents.assign((const byte *)pixelBytes, dataSize);
  }
  SERIALISE_ELEMENT(contents).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(Texture)->replaceRegion(region, level, contents.data(), bytesPerRow);
  }

  return true;
}

void WrappedMTLTexture::replaceRegion(MTL::Region &region, NS::UInteger level,
                                      const void *pixelBytes, NS::UInteger bytesPerRow)
{
  SERIALISE_TIME_CALL(Unwrap(this)->replaceRegion(region, level, pixelBytes, bytesPerRow));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLTexture_replaceRegion);
      Serialise_replaceRegion(ser, region, level, pixelBytes, bytesPerRow);
      chunk = scope.Get();
    }

    if(IsActiveCapturing(m_State))
    {
      m_Device->AddFrameCaptureRecordChunk(chunk);
      GetResourceManager()->MarkResourceFrameReferenced(m_ID, eFrameRef_PartialWrite);
    }
    else
    {
      GetRecord(this)->AddChunk(chunk);
    }
  }
}

template <typename SerialiserType>
bool WrappedMTLTexture::Serialise_replaceRegion(SerialiserType &ser, MTL::Region &region,
                                                 NS::UInteger level, NS::UInteger slice,
                                                 const void *pixelBytes, NS::UInteger bytesPerRow,
                                                 NS::UInteger bytesPerImage)
{
  SERIALISE_ELEMENT_LOCAL(Texture, this).Important();
  SERIALISE_ELEMENT(region).Important();
  SERIALISE_ELEMENT(level).Important();
  SERIALISE_ELEMENT(slice).Important();
  SERIALISE_ELEMENT(bytesPerRow).Important();
  SERIALISE_ELEMENT(bytesPerImage).Important();

  bytebuf contents;
  if(ser.IsWriting() && pixelBytes && bytesPerRow > 0 && region.size.height > 0)
  {
    const size_t dataSize =
        bytesPerImage > 0 ? size_t(bytesPerImage) : size_t(bytesPerRow) * size_t(region.size.height);
    contents.assign((const byte *)pixelBytes, dataSize);
  }
  SERIALISE_ELEMENT(contents).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    Unwrap(Texture)->replaceRegion(region, level, slice, contents.data(), bytesPerRow,
                                   bytesPerImage);
  }

  return true;
}

void WrappedMTLTexture::replaceRegion(MTL::Region &region, NS::UInteger level, NS::UInteger slice,
                                      const void *pixelBytes, NS::UInteger bytesPerRow,
                                      NS::UInteger bytesPerImage)
{
  SERIALISE_TIME_CALL(
      Unwrap(this)->replaceRegion(region, level, slice, pixelBytes, bytesPerRow, bytesPerImage));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLTexture_replaceRegion_slice);
      Serialise_replaceRegion(ser, region, level, slice, pixelBytes, bytesPerRow, bytesPerImage);
      chunk = scope.Get();
    }

    if(IsActiveCapturing(m_State))
    {
      m_Device->AddFrameCaptureRecordChunk(chunk);
      GetResourceManager()->MarkResourceFrameReferenced(m_ID, eFrameRef_PartialWrite);
    }
    else
    {
      GetRecord(this)->AddChunk(chunk);
    }
  }
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLTexture, void, replaceRegion, MTL::Region &,
                                NS::UInteger, const void *, NS::UInteger);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLTexture, void, getBytes, void *, NS::UInteger,
                                MTL::Region &, NS::UInteger);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLTexture, void, getBytes, void *, NS::UInteger,
                                NS::UInteger, MTL::Region &, NS::UInteger, NS::UInteger);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLTexture, void, replaceRegion, MTL::Region &,
                                NS::UInteger, NS::UInteger, const void *, NS::UInteger,
                                NS::UInteger);
INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLTexture, bool, newTextureView,
                                WrappedMTLTexture *, MTL::PixelFormat, MTL::TextureType,
                                NS::Range, NS::Range, MTL::TextureSwizzleChannels, uint32_t);
