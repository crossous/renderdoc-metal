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

// Initial texture data uses tightly packed logical mip rows, as in D3D12/Vulkan.
// Native texture heap footprints are opaque and must not be copied as linear pixels.
struct InitialTextureMip
{
  uint64_t width, height, depth, slice, mip, rows, rowBytes, pitch, offset, stagingOffset;
  MTL::BlitOption options;
};

static bool InitialTextureLayout(MTL::Texture *texture, rdcarray<InitialTextureMip> &mips,
                                  uint64_t &total, uint64_t &stagingSize)
{
  total = stagingSize = 0;
  mips.clear();
  uint32_t blockWidth = 0, blockHeight = 0, blockBytes = 0;
  const bool depthStencil = texture && texture->pixelFormat() == MTL::PixelFormatDepth32Float_Stencil8;
  if(!texture || texture->parentTexture() || texture->buffer() || texture->framebufferOnly() ||
     (texture->storageMode() != MTL::StorageModePrivate &&
      texture->storageMode() != MTL::StorageModeShared &&
      texture->storageMode() != MTL::StorageModeManaged) || texture->sampleCount() != 1 ||
     !texture->arrayLength() || texture->arrayLength() > 128 ||
     (!depthStencil && !GetTextureDataBlockShape(texture->pixelFormat(), blockWidth, blockHeight, blockBytes)) ||
     !texture->width() || !texture->height() || !texture->depth() || texture->width() > 8192 ||
     texture->height() > 8192 || texture->depth() > 256 ||
     !ValidTextureMipCount(texture->width(), texture->height(), texture->depth(), texture->mipmapLevelCount()) ||
     texture->mipmapLevelCount() > 14)
    return false;
  uint64_t slices = 1;
  const MTL::TextureType type = texture->textureType();
  if(type == MTL::TextureType2DArray) slices = texture->arrayLength();
  else if(type == MTL::TextureTypeCube || type == MTL::TextureTypeCubeArray)
  {
    if(texture->width() != texture->height()) return false;
    slices = 6 * texture->arrayLength();
  }
  else if(type != MTL::TextureType2D && type != MTL::TextureType3D) return false;
  if((type != MTL::TextureType3D && texture->depth() != 1) ||
     ((type == MTL::TextureType2D || type == MTL::TextureType3D || type == MTL::TextureTypeCube) &&
      texture->arrayLength() != 1)) return false;
  const bool depth = depthStencil || texture->pixelFormat() == MTL::PixelFormatDepth16Unorm ||
                     texture->pixelFormat() == MTL::PixelFormatDepth32Float;
  if(depth && type == MTL::TextureType3D) return false;
  // Both compressed and depth formats are excluded from the linear color alignment query.
  const uint64_t alignment = depth ? 256 : blockWidth > 1 || blockHeight > 1 ? 1 :
      RDCMAX(1ULL, uint64_t(texture->device()->minimumLinearTextureAlignmentForPixelFormat(texture->pixelFormat())));
  for(uint64_t slice = 0; slice < slices; slice++)
    for(uint64_t mip = 0; mip < texture->mipmapLevelCount(); mip++)
      for(uint32_t plane = 0; plane < (depthStencil ? 2U : 1U); plane++)
      {
        const uint32_t bw = depthStencil ? 1 : blockWidth;
        const uint32_t bh = depthStencil ? 1 : blockHeight;
        const uint32_t bytes = depthStencil ? (plane ? 1 : 4) : blockBytes;
        InitialTextureMip layout = {};
        layout.slice = slice; layout.mip = mip;
        layout.options = depthStencil ? (plane ? MTL::BlitOptionStencilFromDepthStencil :
                                                MTL::BlitOptionDepthFromDepthStencil) : MTL::BlitOptionNone;
        layout.width = RDCMAX(1ULL, uint64_t(texture->width()) >> mip);
        layout.height = RDCMAX(1ULL, uint64_t(texture->height()) >> mip);
        layout.depth = type == MTL::TextureType3D ? RDCMAX(1ULL, uint64_t(texture->depth()) >> mip) : 1;
        layout.rows = (layout.height + bh - 1) / bh;
        layout.rowBytes = ((layout.width + bw - 1) / bw) * bytes;
        layout.pitch = AlignUp(layout.rowBytes, alignment);
        layout.offset = total; layout.stagingOffset = stagingSize;
        total += layout.rowBytes * layout.rows * layout.depth;
        stagingSize += layout.pitch * layout.rows * layout.depth;
        if(total > 128ULL * 1024 * 1024 || stagingSize > 128ULL * 1024 * 1024)
          return false;
        mips.push_back(layout);
      }
  return true;
}

struct InitialTextureAutoreleasePool
{
  NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
  ~InitialTextureAutoreleasePool() { pool->drain(); }
};

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
  // Dirty tracking can outlive an explicitly retired placement resource retained by
  // completed command buffers. It has no valid initial allocation to read back.
  if(Atomic::CmpExch32(&res->m_CapturedAliasable, 0, 0)) return false;

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
      // StartFrameCapture waits for all committed application queues before this
      // CPU read. Shared visibility alone does not imply GPU completion.
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
    InitialTextureAutoreleasePool pool;
    MTL::Texture *texture = Unwrap((WrappedMTLTexture *)res);
    uint64_t total = 0, stagingSize = 0;
    rdcarray<InitialTextureMip> mips;
    if(!InitialTextureLayout(texture, mips, total, stagingSize)) return false;
    bytebuf bytes;
    bytes.resize((size_t)total);
    MTL::Device *device = Unwrap(this);
    MTL::Buffer *staging = device->newBuffer(stagingSize, MTL::ResourceStorageModeShared);
    if(!staging || !staging->contents()) { if(staging) staging->release(); return false; }
    MTL::CommandBuffer *command = m_mtlCommandQueue->commandBuffer();
    MTL::BlitCommandEncoder *blit = command ? command->blitCommandEncoder() : NULL;
    if(!blit) { staging->release(); return false; }
    for(const InitialTextureMip &layout : mips)
      blit->copyFromTexture(texture, layout.slice, layout.mip, MTL::Origin::Make(0,0,0),
          MTL::Size::Make(layout.width,layout.height,layout.depth), staging, layout.stagingOffset,
          layout.pitch, layout.pitch * layout.rows, layout.options);
    blit->endEncoding(); command->commit(); command->waitUntilCompleted();
    if(command->status() != MTL::CommandBufferStatusCompleted) { staging->release(); return false; }
    for(const InitialTextureMip &layout : mips)
      for(uint64_t z = 0; z < layout.depth; z++)
        for(uint64_t y = 0; y < layout.rows; y++)
          memcpy(bytes.data() + layout.offset + (z * layout.rows + y) * layout.rowBytes,
                 (byte *)staging->contents() + layout.stagingOffset + (z * layout.rows + y) * layout.pitch,
                 layout.rowBytes);
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
                                : RecordReplayTextureInitialContents(id, contents);
    }
    return true;
  }
  RDCERR("Unhandled resource type %d", type);
  return false;
}

bool WrappedMTLDevice::RecordReplayTextureInitialContents(ResourceId id, const bytebuf &contents)
{
  WrappedMTLObject *object = GetResourceManager()->GetResource(id, true);
  uint64_t expected = 0, stagingSize = 0;
  rdcarray<InitialTextureMip> mips;
  if(!object || object->m_Type != eResTexture || !object->m_Real ||
     !InitialTextureLayout(Unwrap((WrappedMTLTexture *)object), mips, expected, stagingSize) ||
     contents.size() != expected || m_ReplayTextureInitialContents.count(id))
  {
    RDCERR("Invalid Metal texture initial contents");
    return false;
  }
  m_ReplayTextureInitialContents[id] = contents;
  return true;
}

bool WrappedMTLDevice::RestoreReplayTextureInitialContents()
{
  MTL::Device *device = Unwrap(this);
  for(const auto &entry : m_ReplayTextureInitialContents)
  {
    InitialTextureAutoreleasePool pool;
    WrappedMTLObject *object = GetResourceManager()->GetResource(entry.first, true);
    uint64_t expected = 0, stagingSize = 0;
    rdcarray<InitialTextureMip> mips;
    MTL::Texture *texture = object && object->m_Type == eResTexture ?
                            Unwrap((WrappedMTLTexture *)object) : NULL;
    if(!InitialTextureLayout(texture, mips, expected, stagingSize) ||
       expected != entry.second.size()) return false;
    if(getenv("RENDERDOC_METAL_TRACE_INITIAL_PRIVATE"))
      fprintf(stderr, "Metal texture initial contents upload id=%s bytes=%llu mips=%zu storage=%s\n",
              ToStr(entry.first).c_str(), (unsigned long long)entry.second.size(), mips.size(),
              texture->storageMode() == MTL::StorageModePrivate ? "Private" :
              texture->storageMode() == MTL::StorageModeManaged ? "Managed" : "Shared");
    MTL::Buffer *staging = device->newBuffer(stagingSize, MTL::ResourceStorageModeShared);
    if(!staging || !staging->contents()) { if(staging) staging->release(); return false; }
    for(const InitialTextureMip &layout : mips)
      for(uint64_t z = 0; z < layout.depth; z++)
        for(uint64_t y = 0; y < layout.rows; y++)
          memcpy((byte *)staging->contents() + layout.stagingOffset + (z * layout.rows + y) * layout.pitch,
                 entry.second.data() + layout.offset + (z * layout.rows + y) * layout.rowBytes,
                 layout.rowBytes);
    MTL::CommandBuffer *command = m_mtlCommandQueue->commandBuffer();
    MTL::BlitCommandEncoder *blit = command ? command->blitCommandEncoder() : NULL;
    if(!blit) { staging->release(); return false; }
    for(const InitialTextureMip &layout : mips)
      blit->copyFromBuffer(staging, layout.stagingOffset, layout.pitch, layout.pitch * layout.rows,
          MTL::Size::Make(layout.width,layout.height,layout.depth), texture, layout.slice, layout.mip,
          MTL::Origin::Make(0,0,0), layout.options);
    blit->endEncoding(); command->commit(); command->waitUntilCompleted();
    if(command->status() != MTL::CommandBufferStatusCompleted) { staging->release(); return false; }
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
