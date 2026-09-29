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
#include "metal_common.h"
#include "metal_device.h"
#include "metal_texture.h"

// Only the three block formats observed in the UE frame are admitted. Each mip is tightly
// serialized as rows of 4x4 blocks; the staging row stride may be larger for Metal alignment.
static uint32_t BCBlockBytes(MTL::PixelFormat format)
{
  if(format == MTL::PixelFormatBC1_RGBA || format == MTL::PixelFormatBC1_RGBA_sRGB)
    return 8;
  if(format == MTL::PixelFormatBC5_RGUnorm)
    return 16;
  return 0;
}

static bool BCTextureLayout(MTL::Texture *texture, uint64_t &total)
{
  total = 0;
  if(!texture || texture->textureType() != MTL::TextureType2D ||
     texture->storageMode() != MTL::StorageModePrivate || texture->sampleCount() != 1 ||
     texture->arrayLength() != 1 || !BCBlockBytes(texture->pixelFormat()) ||
     !texture->width() || !texture->height() || texture->width() > 4096 ||
     texture->height() > 4096 || !texture->mipmapLevelCount() ||
     texture->mipmapLevelCount() > 13)
    return false;
  for(uint64_t mip = 0; mip < texture->mipmapLevelCount(); mip++)
  {
    const uint64_t width = RDCMAX(1ULL, uint64_t(texture->width()) >> mip);
    const uint64_t height = RDCMAX(1ULL, uint64_t(texture->height()) >> mip);
    total += ((width + 3) / 4) * ((height + 3) / 4) * BCBlockBytes(texture->pixelFormat());
    if(total > 32ULL * 1024 * 1024)
      return false;
  }
  return true;
}

static rdcliteral NameOfType(MetalResourceType type)
{
  switch(type)
  {
    case eResBuffer: return "MTLBuffer"_lit;
    case eResTexture: return "MTLTexture"_lit;
    default: break;
  }
  return "MTLResource"_lit;
}

bool WrappedMTLDevice::Prepare_InitialState(WrappedMTLObject *res)
{
  ResourceId id = GetResourceManager()->GetID(res);

  MetalResourceType type = res->m_Record->m_Type;

  if(type == eResBuffer)
  {
    WrappedMTLBuffer *buffer = (WrappedMTLBuffer *)res;
    MTL::Buffer *mtlBuffer = Unwrap(buffer);
    MTL::Buffer *mtlSharedBuffer = NULL;
    MTL::StorageMode storageMode = mtlBuffer->storageMode();
    size_t len = mtlBuffer->length();
    byte *data = NULL;
    if(storageMode == MTL::StorageModeShared)
    {
      // MTLStorageModeShared buffers are automatically synchronized
      data = (byte *)mtlBuffer->contents();
    }
    else if(storageMode == MTL::StorageModeManaged)
    {
      // MTLStorageModeManaged buffers need to call MTLBlitCommandEncoder::synchronizeResource
      MTL::CommandBuffer *mtlCommandBuffer = m_mtlCommandQueue->commandBuffer();
      MTL::BlitCommandEncoder *mtlBlitEncoder = mtlCommandBuffer->blitCommandEncoder();
      mtlBlitEncoder->synchronizeResource(mtlBuffer);
      mtlBlitEncoder->endEncoding();
      mtlCommandBuffer->commit();
      mtlCommandBuffer->waitUntilCompleted();
      data = (byte *)mtlBuffer->contents();
    }
    else if(storageMode == MTL::StorageModePrivate)
    {
      // TODO: postpone readback until data is required
      // TODO: batch readback for multiple resources to avoid sync per resource
      // MTLStorageModePrivate buffer need to copy into a temporary MTLStorageModeShared buffer
      mtlSharedBuffer = Unwrap(this)->newBuffer(len, MTL::ResourceStorageModeShared);
      MTL::CommandBuffer *mtlCommandBuffer = m_mtlCommandQueue->commandBuffer();
      MTL::BlitCommandEncoder *mtlBlitEncoder = mtlCommandBuffer->blitCommandEncoder();
      mtlBlitEncoder->copyFromBuffer(mtlBuffer, 0, mtlSharedBuffer, 0, len);
      mtlBlitEncoder->endEncoding();
      mtlCommandBuffer->commit();
      mtlCommandBuffer->waitUntilCompleted();
      data = (byte *)mtlSharedBuffer->contents();
    }
    else
    {
      RDCERR("Unhandled buffer storage mode 0x%X", storageMode);
    }

    bytebuf bufferContents(data, len);
    MetalInitialContents initialContents(type, bufferContents);
    GetResourceManager()->SetInitialContents(id, initialContents);
    if(mtlSharedBuffer)
    {
      mtlSharedBuffer->release();
    }
    if(storageMode == MTL::StorageModeShared)
    {
      // Set the base snapshot to match the initial contents
      MetalBufferInfo *bufInfo = res->m_Record->bufInfo;
      if(bufInfo->baseSnapshot.isEmpty())
        bufInfo->baseSnapshot.resize(len);
      RDCASSERTEQUAL(bufInfo->baseSnapshot.size(), len);
      memcpy(bufInfo->baseSnapshot.data(), bufferContents.data(), len);
    }
    return true;
  }
  else if(type == eResTexture)
  {
    MTL::Texture *texture = Unwrap((WrappedMTLTexture *)res);
    uint64_t total = 0;
    if(!BCTextureLayout(texture, total)) return false;
    bytebuf bytes;
    bytes.resize((size_t)total);
    MTL::Device *device = Unwrap(this);
    // Metal's linear-texture alignment query asserts for compressed formats. Native
    // BC blits accept a tightly packed row of complete 4x4 blocks.
    const uint64_t alignment = 1;
    uint64_t stagingSize = 0;
    for(uint64_t mip = 0; mip < texture->mipmapLevelCount(); mip++)
    {
      const uint64_t width = RDCMAX(1ULL, uint64_t(texture->width()) >> mip);
      const uint64_t height = RDCMAX(1ULL, uint64_t(texture->height()) >> mip);
      const uint64_t row = ((width + 3) / 4) * BCBlockBytes(texture->pixelFormat());
      stagingSize = RDCMAX(stagingSize, AlignUp(row, alignment) * ((height + 3) / 4));
    }
    MTL::Buffer *staging = device->newBuffer(stagingSize, MTL::ResourceStorageModeShared);
    if(!staging || !staging->contents()) { if(staging) staging->release(); return false; }
    uint64_t cursor = 0;
    for(uint64_t mip = 0; mip < texture->mipmapLevelCount(); mip++)
    {
      const uint64_t width = RDCMAX(1ULL, uint64_t(texture->width()) >> mip);
      const uint64_t height = RDCMAX(1ULL, uint64_t(texture->height()) >> mip);
      const uint64_t rows = (height + 3) / 4;
      const uint64_t row = ((width + 3) / 4) * BCBlockBytes(texture->pixelFormat());
      const uint64_t pitch = AlignUp(row, alignment);
      MTL::CommandBuffer *command = m_mtlCommandQueue->commandBuffer();
      MTL::BlitCommandEncoder *blit = command ? command->blitCommandEncoder() : NULL;
      if(!blit) { staging->release(); return false; }
      blit->copyFromTexture(texture, 0, mip, MTL::Origin::Make(0,0,0),
                            MTL::Size::Make(width,height,1), staging, 0, pitch, pitch * rows);
      blit->endEncoding();
      command->commit();
      command->waitUntilCompleted();
      if(command->status() != MTL::CommandBufferStatusCompleted) { staging->release(); return false; }
      for(uint64_t y = 0; y < rows; y++)
        memcpy(bytes.data() + cursor + y * row, (byte *)staging->contents() + y * pitch, row);
      cursor += row * rows;
    }
    staging->release();
    GetResourceManager()->SetInitialContents(id, MetalInitialContents(type, bytes));
    return true;
  }
  else
  {
    RDCERR("Unhandled resource type %d", type);
  }

  return false;
}

uint64_t WrappedMTLDevice::GetSize_InitialState(ResourceId id, const MetalInitialContents &initial)
{
  uint64_t ret = 128;

  if(initial.type == eResBuffer)
  {
    ret += uint64_t(initial.resourceContents.size() + WriteSerialiser::GetChunkAlignment());
    return ret;
  }
  if(initial.type == eResTexture)
    return ret + uint64_t(initial.resourceContents.size() + WriteSerialiser::GetChunkAlignment());

  RDCERR("Unhandled resource type %s", ToStr(initial.type).c_str());
  return 0;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_InitialState(SerialiserType &ser, ResourceId id,
                                              MetalResourceRecord *record,
                                              const MetalInitialContents *initial)
{
  SERIALISE_ELEMENT_LOCAL(type, initial->type);
  SERIALISE_ELEMENT(id).TypedAs(NameOfType(type)).Important();
  if(type == eResBuffer || type == eResTexture)
  {
    SERIALISE_CHECK_READ_ERRORS();

    bytebuf contents;
    if(ser.IsWriting())
    {
      ser.Serialise("Contents"_lit, initial->resourceContents);
    }
    else
    {
      ser.Serialise("Contents"_lit, contents);
    }

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
    {
      return type == eResBuffer ? RecordReplayBufferInitialContents(id, contents)
                                : RecordReplayBCTextureInitialContents(id, contents);
    }
    return true;
  }
  RDCERR("Unhandled resource type %d", type);
  return false;
}

bool WrappedMTLDevice::RecordReplayBCTextureInitialContents(ResourceId id, const bytebuf &contents)
{
  WrappedMTLObject *object = GetResourceManager()->GetResource(id, true);
  uint64_t expected = 0;
  if(!object || object->m_Type != eResTexture || !object->m_Real ||
     !BCTextureLayout(Unwrap((WrappedMTLTexture *)object), expected) ||
     contents.size() != expected || m_ReplayBCTextureInitialContents.count(id))
  {
    RDCERR("Invalid Metal BC texture initial contents");
    return false;
  }
  m_ReplayBCTextureInitialContents[id] = contents;
  return true;
}

bool WrappedMTLDevice::RestoreReplayBCTextureInitialContents()
{
  MTL::Device *device = Unwrap(this);
  for(const auto &entry : m_ReplayBCTextureInitialContents)
  {
    WrappedMTLObject *object = GetResourceManager()->GetResource(entry.first, true);
    uint64_t expected = 0;
    MTL::Texture *texture = object && object->m_Type == eResTexture ?
                            Unwrap((WrappedMTLTexture *)object) : NULL;
    if(!BCTextureLayout(texture, expected) || expected != entry.second.size()) return false;
    const uint64_t alignment = 1;
    uint64_t stagingSize = 0;
    for(uint64_t mip = 0; mip < texture->mipmapLevelCount(); mip++)
    {
      const uint64_t width = RDCMAX(1ULL, uint64_t(texture->width()) >> mip);
      const uint64_t height = RDCMAX(1ULL, uint64_t(texture->height()) >> mip);
      const uint64_t row = ((width + 3) / 4) * BCBlockBytes(texture->pixelFormat());
      stagingSize = RDCMAX(stagingSize, AlignUp(row, alignment) * ((height + 3) / 4));
    }
    MTL::Buffer *staging = device->newBuffer(stagingSize, MTL::ResourceStorageModeShared);
    if(!staging || !staging->contents()) { if(staging) staging->release(); return false; }
    uint64_t cursor = 0;
    for(uint64_t mip = 0; mip < texture->mipmapLevelCount(); mip++)
    {
      const uint64_t width = RDCMAX(1ULL, uint64_t(texture->width()) >> mip);
      const uint64_t height = RDCMAX(1ULL, uint64_t(texture->height()) >> mip);
      const uint64_t rows = (height + 3) / 4;
      const uint64_t row = ((width + 3) / 4) * BCBlockBytes(texture->pixelFormat());
      const uint64_t pitch = AlignUp(row, alignment);
      for(uint64_t y = 0; y < rows; y++)
        memcpy((byte *)staging->contents() + y * pitch, entry.second.data() + cursor + y * row, row);
      MTL::CommandBuffer *command = m_mtlCommandQueue->commandBuffer();
      MTL::BlitCommandEncoder *blit = command ? command->blitCommandEncoder() : NULL;
      if(!blit) { staging->release(); return false; }
      blit->copyFromBuffer(staging, 0, pitch, pitch * rows,
                           MTL::Size::Make(width,height,1), texture, 0, mip,
                           MTL::Origin::Make(0,0,0), MTL::BlitOptionNone);
      blit->endEncoding();
      command->commit();
      command->waitUntilCompleted();
      if(command->status() != MTL::CommandBufferStatusCompleted) { staging->release(); return false; }
      cursor += row * rows;
    }
    staging->release();
  }
  return true;
}

void WrappedMTLDevice::Create_InitialState(ResourceId id, WrappedMTLObject *live, bool hasData)
{
  METAL_NOT_IMPLEMENTED();
}

void WrappedMTLDevice::Apply_InitialState(WrappedMTLObject *live, MetalInitialContents &initial)
{
  METAL_NOT_IMPLEMENTED();
}

INSTANTIATE_FUNCTION_SERIALISED(WrappedMTLDevice, void, InitialState, ResourceId id,
                                MetalResourceRecord *record, const MetalInitialContents *initial);
