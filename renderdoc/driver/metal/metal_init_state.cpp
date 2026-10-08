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
#include "metal_heap.h"
#include "metal_common.h"
#include "metal_device.h"
#include "metal_texture.h"
#include "metal_acceleration_structure.h"

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
     (!depthStencil && !GetTextureDataBlockShape(texture->pixelFormat(), blockWidth, blockHeight, blockBytes)))
    return false;
  // Use the same logical restoration contract as frame factories and views.
  // Do not infer staging size from a historical square-dimension limit.
  RDMTL::TextureDescriptor descriptor;
  descriptor.textureType = texture->textureType();
  descriptor.pixelFormat = texture->pixelFormat();
  descriptor.width = texture->width();
  descriptor.height = texture->height();
  descriptor.depth = texture->depth();
  descriptor.arrayLength = texture->arrayLength();
  descriptor.mipmapLevelCount = texture->mipmapLevelCount();
  descriptor.sampleCount = texture->sampleCount();
  descriptor.storageMode = texture->storageMode();
  uint64_t logicalBytes = 0;
  if(!MetalTextureReplayLayout(descriptor, logicalBytes)) return false;
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
  return total == logicalBytes;
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
    case eResAccelerationStructure: return "MTLAccelerationStructure"_lit;
    default: break;
  }
  return "MTLResource"_lit;
}

// Capture original backing bytes before returning a new placement buffer to the
// application. There is no raw whole-heap Metal buffer as in DX12/Vulkan.
// Caller holds the transition read lock; serialize this observation with commits.
void WrappedMTLDevice::CaptureHeapBufferBirth(WrappedMTLBuffer *buffer, WrappedMTLHeap *parent, bool fresh)
{
  // Capture API state independently of optional descriptor protocol declarations.
  // In particular, diagnostic providers may declare typed records without coverage.
  if(!IsActiveCapturing(m_State) || !buffer || !buffer->m_Real ||
     !parent || !parent->m_Real || parent->m_Device!=this)return;
  SCOPED_LOCK(m_CaptureCommandBuffersLock);
  auto native=Unwrap(buffer);auto heap=native->heap();
  const uint64_t length=native->length(),capacity=16ULL*1024*1024;
  auto trace=[&](const char *outcome) {
    if(getenv("RENDERDOC_METAL_TRACE_HEAP_BIRTH"))
      fprintf(stderr,"Metal heap birth observation: buffer=%s heap=%s offset=%llu length=%llu fresh=%d outcome=%s\n",
          ToStr(GetResID(buffer)).c_str(),ToStr(GetResID(parent)).c_str(),
          (unsigned long long)native->heapOffset(),(unsigned long long)length,int(fresh),outcome);
  };
  if(heap!=Unwrap(parent) || heap->type()!=MTL::HeapTypePlacement || native->storageMode()!=MTL::StorageModePrivate ||
     !length || (!fresh && (length>capacity || length>128ULL*1024*1024-m_CaptureHeapBirthBytes)))
  {trace("factory-or-budget");return;}
  if(!IsCapturedFrameResource(GetResID(buffer))) {trace("not-frame-birth");return;}
  if(fresh)
  {
    // A range with no prior resource in this heap lifetime has no inherited
    // API-defined bytes. Record this fact, without copying or inventing values.
    // Address fields remain subject to their separate relocation contract.
    WriteSerialiser &ser=GetThreadSerialiser();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_CaptureHeapBirthUnspecified);
    Serialise_CaptureHeapBirthContents(ser,GetResID(buffer),GetResID(parent),native->heapOffset(),bytebuf(),true);
    AddFrameCaptureRecordChunk(scope.Get());
    trace("API-unspecified");
    return;
  }
  // This state belongs to this new logical object's birth, after the original
  // preceding commands. It is never a frame-start initial for earlier heap
  // consumers. An unrelated prior heap use is not an objection to this input.
  for(auto record:m_CaptureCommandBuffersSubmitted)
  {
    std::unordered_set<ResourceId> references;record->AddReferencedIDs(references);
    if(references.count(GetResID(buffer))) {trace("already-used-object");return;}
  }
  // Queue ordering alone is insufficient for an internal copy of aliased heap
  // memory. Close the already committed prefix before reading it, without
  // waiting for application CPU completion handlers under the capture locks.
  // An uncommitted reservation could require a future commit blocked by this
  // factory. Do not put a marker behind it or wait across that boundary.
  for(const auto &entry : m_CaptureCommandBuffersEnqueued)
    if(!entry.second.empty()) { trace("pending-queue-reservation"); return; }
  PerformanceTimer prefixTimer;
  prefixTimer.Restart();
  bool waited = false;
  {
    SCOPED_LOCK(m_CapturePendingGPULock);
    for(auto pending : m_CapturePendingGPU)
    {
      while(pending->status() != MTL::CommandBufferStatusCompleted &&
            pending->status() != MTL::CommandBufferStatusError)
      {
        // Bound this optional capture observation. External event waits may
        // depend on future work; failure retains the original recovery checks.
        if(prefixTimer.GetMilliseconds() >= 200 || m_CaptureHeapBirthWaitMS >= 2000)
        { trace("prior-GPU-observation-budget"); return; }
        const double before = prefixTimer.GetMilliseconds();
        Threading::Sleep(1);
        m_CaptureHeapBirthWaitMS += prefixTimer.GetMilliseconds() - before;
        waited = true;
      }
      if(pending->error() || pending->status() == MTL::CommandBufferStatusError)
      { trace("prior-GPU-error"); return; }
    }
  }
  InitialTextureAutoreleasePool pool;
  auto staging=NS::TransferPtr(Unwrap(this)->newBuffer(length,MTL::ResourceStorageModeShared));
  auto command=m_mtlCommandQueue->commandBuffer();
  auto blit=command && staging?command->blitCommandEncoder():NULL;
  if(!blit) {trace("staging-unavailable");return;}
  blit->copyFromBuffer(native,0,staging.get(),0,length);blit->endEncoding();
  command->commit();command->waitUntilCompleted();
  if(command->status()!=MTL::CommandBufferStatusCompleted || command->error() || !staging->contents())
  {trace("staging-copy-failed");return;}
  bytebuf contents((byte *)staging->contents(),length);
  m_CaptureHeapBirthBytes+=length;
  WriteSerialiser &ser=GetThreadSerialiser();
  SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_CaptureHeapBirthContents);
  Serialise_CaptureHeapBirthContents(ser,GetResID(buffer),GetResID(parent),native->heapOffset(),contents,false);
  AddFrameCaptureRecordChunk(scope.Get());
  trace(waited ? "GPU-prefix-original-birth-bytes" : "original-birth-bytes");
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_CaptureHeapBirthContents(SerialiserType &ser,
    ResourceId buffer,ResourceId heap,uint64_t offset,const bytebuf &contents,bool apiUnspecified)
{
  SERIALISE_ELEMENT(buffer).TypedAs("MTLBuffer"_lit);
  SERIALISE_ELEMENT(heap).TypedAs("MTLHeap"_lit);
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT_LOCAL(data,contents);
  apiUnspecified=MetalChunk(ser.ChunkMetadata().chunkID)==MetalChunk::MTLBuffer_CaptureHeapBirthUnspecified;
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    auto object=GetResourceManager()->GetResource(buffer,true);
    auto parent=GetResourceManager()->GetResource(heap,true);
    auto initial=m_ReplayHeapBufferBirthContents.find(buffer);
    if(!object || object->m_Type!=eResBuffer || !object->m_Real || !parent || parent->m_Type!=eResHeap ||
       !parent->m_Real)return false;
    // Typed preflight validates the complete frame before any GPU work. Ordinary
    // legacy replay still validates this API record against its actual objects.
    if(m_DescriptorCoverage>=4 && initial==m_ReplayHeapBufferBirthContents.end())return false;
    if(initial!=m_ReplayHeapBufferBirthContents.end() && (initial->second!=data ||
       apiUnspecified!=(m_ReplayHeapBufferBirthUnspecified.count(buffer)!=0)))return false;
    auto native=Unwrap((WrappedMTLBuffer *)object);
    if(native->heap()!=Unwrap((WrappedMTLHeap *)parent) || native->heapOffset()!=offset ||
       native->storageMode()!=MTL::StorageModePrivate ||
       (apiUnspecified?!data.empty():(data.size()!=native->length() || data.empty())))return false;
    if(apiUnspecified)
    {
      const auto layout=Unwrap(this)->heapBufferSizeAndAlign(native->length(),native->resourceOptions());
      return layout.size && offset<=Unwrap((WrappedMTLHeap *)parent)->size() &&
          layout.size<=Unwrap((WrappedMTLHeap *)parent)->size()-offset &&
          !((WrappedMTLHeap *)parent)->HasOtherPlacementOverlap(offset,offset+layout.size,buffer);
    }
    // The capture observed every preceding submitted GPU operation completed
    // before these creation bytes. Re-establish that boundary before the
    // internal upload; do not submit any still-encoded application command.
    for(const auto &entry:m_ReplayCommandBuffers)
      if(entry.second.committed &&
         !WaitReplayCommandBuffer(entry.second.buffer,"heap birth input boundary"))return false;
    InitialTextureAutoreleasePool pool;
    auto upload=NS::TransferPtr(Unwrap(this)->newBuffer(data.data(),data.size(),MTL::ResourceStorageModeShared));
    auto command=m_mtlCommandQueue->commandBuffer();auto blit=command && upload?command->blitCommandEncoder():NULL;
    if(!blit)return false;
    blit->copyFromBuffer(upload.get(),0,native,0,data.size());blit->endEncoding();
    command->commit();command->waitUntilCompleted();
    return command->status()==MTL::CommandBufferStatusCompleted && !command->error();
  }
  return true;
}
template bool WrappedMTLDevice::Serialise_CaptureHeapBirthContents(ReadSerialiser &,ResourceId,ResourceId,uint64_t,const bytebuf &,bool);
template bool WrappedMTLDevice::Serialise_CaptureHeapBirthContents(WriteSerialiser &,ResourceId,ResourceId,uint64_t,const bytebuf &,bool);

bool WrappedMTLDevice::Prepare_InitialState(WrappedMTLObject *res)
{
  ResourceId id = GetResourceManager()->GetID(res);
  // Dirty tracking can outlive an explicitly retired placement resource retained by
  // completed command buffers. It has no valid initial allocation to read back.
  if(Atomic::CmpExch32(&res->m_CapturedAliasable, 0, 0)) return false;

  MetalResourceType type = res->m_Record->m_Type;

  if(type == eResAccelerationStructure)
  {
    auto structure = (WrappedMTLAccelerationStructure *)res;
    const auto build = structure->m_CapturedInitialBuild;
    if(!MaterialiseMetalASInitialBuild(build)) return false;
    // Never wait for completion handlers under the capture-transition lock:
    // an earlier application handler may itself be waiting for that lock.
    if(!build->submission || build->submission->status() != MTL::CommandBufferStatusCompleted ||
       build->submission->error()) return false;
    if(build->kind == 5 || build->kind == 9)
    {
      if(build->children.size() != build->childBuilds.size()) return false;
      for(size_t i = 0; i < build->children.size(); i++)
      {
        auto child = GetResourceManager()->GetResource(build->children[i], true);
        if(!child || child->m_Type != eResAccelerationStructure ||
           ((WrappedMTLAccelerationStructure *)child)->m_CapturedInitialBuild != build->childBuilds[i])
          return false;
      }
    }
    MetalInitialContents contents(type, build->vertices);
    contents.asSource = build->source;
    contents.asParameters = build->parameters;
    contents.asKind = build->kind;
    contents.asCompacted = build->compacted;
    contents.asChildren = build->children;
    contents.asIndexSource = build->indexSource;
    contents.asIndices = build->indices;
    if(structure->m_LastCompactedWriteCommandBuffer != ResourceId() &&
       structure->m_LastBuildCommandBuffer == structure->m_LastCompactedWriteCommandBuffer)
    {
      auto output = GetResourceManager()->GetResource(structure->m_LastCompactedSizeBuffer, true);
      if(output && output->m_Type == eResBuffer && output->m_Real &&
         Unwrap((WrappedMTLBuffer *)output)->storageMode() == MTL::StorageModeShared)
      {
        contents.asSizeSource = structure->m_LastCompactedSizeBuffer;
        contents.asSizeParameters = {structure->m_LastCompactedSizeOffset,
            uint64_t(structure->m_LastCompactedSizeType)};
      }
    }
    GetResourceManager()->SetInitialContents(id, contents);
    return true;
  }
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
  if(initial.type == eResAccelerationStructure)
    return ret + initial.resourceContents.size() + initial.asIndices.size() + initial.asParameters.size()*sizeof(uint64_t) +
        initial.asSizeParameters.size()*sizeof(uint64_t) + sizeof(ResourceId) + sizeof(uint64_t) +
        initial.asChildren.size()*sizeof(ResourceId) + WriteSerialiser::GetChunkAlignment();
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
  if(type == eResAccelerationStructure)
  {
    uint32_t schema = initial && initial->asKind == 11 ? 9 : initial && initial->asKind == 10 ? 8 : initial && initial->asKind == 9 ? 7 :
        initial && initial->asKind == 5 && initial->asParameters.size() == 8 &&
        initial->asParameters[2] == uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeUserID) ?
        6 : initial && initial->asKind == 8 ? 5 : 4;
    uint32_t kind = initial ? initial->asKind : 1;
    rdcarray<ResourceId> children = initial ? initial->asChildren : rdcarray<ResourceId>();
    ResourceId source = initial ? initial->asSource : ResourceId();
    rdcarray<uint64_t> parameters = initial ? initial->asParameters : rdcarray<uint64_t>();
    bytebuf vertices = initial ? initial->resourceContents : bytebuf();
    ResourceId indexSource = initial ? initial->asIndexSource : ResourceId();
    bytebuf indices = initial ? initial->asIndices : bytebuf();
    bool compacted = initial ? initial->asCompacted : false;
    ResourceId sizeSource = initial ? initial->asSizeSource : ResourceId();
    rdcarray<uint64_t> sizeParameters = initial ? initial->asSizeParameters : rdcarray<uint64_t>();
    SERIALISE_ELEMENT(schema);
    SERIALISE_ELEMENT(source).TypedAs("MTLBuffer"_lit);
    SERIALISE_ELEMENT(parameters);
    SERIALISE_ELEMENT(vertices);
    if(schema >= 2)
    {
      SERIALISE_ELEMENT(kind);
      SERIALISE_ELEMENT(children);
    }
    if(schema >= 3)
    {
      SERIALISE_ELEMENT(indexSource).TypedAs("MTLBuffer"_lit);
      SERIALISE_ELEMENT(indices);
    }
    if(schema >= 4)
    {
      SERIALISE_ELEMENT(compacted);
      SERIALISE_ELEMENT(sizeSource).TypedAs("MTLBuffer"_lit);
      SERIALISE_ELEMENT(sizeParameters);
    }
    SERIALISE_CHECK_READ_ERRORS();
    const bool userID = kind == 5 && parameters.size() == 8 &&
        parameters[2] == uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeUserID);
    if(schema < 1 || schema > 9 || (schema == 5) != (kind == 8) ||
       (schema == 6) != userID || (schema == 7) != (kind == 9) ||
       (schema == 8) != (kind == 10) || (schema == 9) != (kind == 11)) return false;
    if(IsReplayingAndReading()) return RecordReplayASInitialContents(id, source, parameters, vertices,
        kind, children, indexSource, indices, compacted, sizeSource, sizeParameters);
    return true;
  }
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

// Store portable build inputs, following the Vulkan/D3D12 AS initial-state model.
// Metal's opaque acceleration-structure allocation is never copied as raw bytes.
static MTL::AccelerationStructureDescriptor *InitialASDescriptor(
    MTL::Buffer *input, uint64_t offset, const rdcarray<uint64_t> &p,
    const rdcarray<MTL::AccelerationStructure *> &children = {},
    MTL::Buffer *indices = NULL, uint64_t indexOffset = 0, uint32_t kind = 1)
{
  if(kind == 10 || kind == 11)
  {
    auto descriptor = MTL::InstanceAccelerationStructureDescriptor::descriptor();
    descriptor->setInstanceDescriptorBuffer(input);
    descriptor->setInstanceDescriptorBufferOffset(offset);
    descriptor->setInstanceDescriptorStride(kind == 11 ? 72 : p[1]);
    descriptor->setInstanceDescriptorType(MTL::AccelerationStructureInstanceDescriptorTypeIndirect);
    descriptor->setInstanceCount(p[3]);
    return descriptor;
  }
  if(kind == 8) return MetalASMultiIndexedDescriptor(input, indices, p);
  if(!children.empty())
  {
    auto descriptor = MTL::InstanceAccelerationStructureDescriptor::descriptor();
    descriptor->setInstanceDescriptorBuffer(input);
    descriptor->setInstanceCount(p[3]);
    descriptor->setInstanceDescriptorType(kind == 9 ? MTL::AccelerationStructureInstanceDescriptorTypeUserID :
        MTL::AccelerationStructureInstanceDescriptorType(p[2]));
    descriptor->setInstanceDescriptorStride(kind == 9 || p[2] == uint64_t(MTL::AccelerationStructureInstanceDescriptorTypeUserID) ?
        sizeof(MTL::AccelerationStructureUserIDInstanceDescriptor) : sizeof(MTL::AccelerationStructureInstanceDescriptor));
    descriptor->setInstancedAccelerationStructures(NS::Array::array(
        (const NS::Object *const *)children.data(), children.size()));
    return descriptor;
  }
  if(kind == 3)
  {
    auto geometry = MTL::AccelerationStructureBoundingBoxGeometryDescriptor::descriptor();
    geometry->setBoundingBoxBuffer(input);
    geometry->setBoundingBoxBufferOffset(offset);
    geometry->setBoundingBoxStride(p[1]); geometry->setBoundingBoxCount(p[3]);
    geometry->setIntersectionFunctionTableOffset(p[4]);
    geometry->setOpaque(p[5] != 0);
    geometry->setAllowDuplicateIntersectionFunctionInvocation(p[6] != 0);
    auto descriptor = MTL::PrimitiveAccelerationStructureDescriptor::descriptor();
    descriptor->setUsage(MTL::AccelerationStructureUsage(p[7]));
    descriptor->setGeometryDescriptors(NS::Array::array(geometry));
    return descriptor;
  }
  auto geometry = MTL::AccelerationStructureTriangleGeometryDescriptor::descriptor();
  geometry->setVertexBuffer(input);
  geometry->setVertexBufferOffset(offset);
  if(indices)
  {
    geometry->setIndexBuffer(indices);
    geometry->setIndexBufferOffset(indexOffset);
    geometry->setIndexType(MTL::IndexType(p[9]));
  }
  geometry->setVertexStride(p[1]);
  geometry->setVertexFormat(MTL::AttributeFormat(p[2]));
  geometry->setTriangleCount(p[3]);
  geometry->setIntersectionFunctionTableOffset(p[4]);
  geometry->setOpaque(p[5] != 0);
  geometry->setAllowDuplicateIntersectionFunctionInvocation(p[6] != 0);
  auto descriptor = MTL::PrimitiveAccelerationStructureDescriptor::descriptor();
  descriptor->setUsage(MTL::AccelerationStructureUsage(p[7]));
  descriptor->setGeometryDescriptors(NS::Array::array(geometry));
  return descriptor;
}

static bytebuf InitialASUpload(const bytebuf &vertices, uint32_t kind,
    const rdcarray<uint64_t> &p, const rdcarray<uint64_t> &identities = {})
{
  if(kind == 10)
  {
    // Native empty builds read no bytes; use a minimal independent allocation.
    bytebuf upload;
    upload.resize(sizeof(MTL::IndirectAccelerationStructureInstanceDescriptor));
    memset(upload.data(), 0, upload.size());
    return upload;
  }
  bytebuf upload = vertices;
  if(kind == 9)
  {
    if(!ConvertMetalASIndirectInstances(vertices, p[3], identities, upload)) return {};
  }
  else if(kind != 5 && kind != 8 && kind != 11)
  {
    const uint64_t formatBytes = kind == 3 ? 24 :
                                 p[2] == uint64_t(MTL::AttributeFormatFloat4) ? 16 : 12;
    // Metal validates complete strides, including the unused final padding.
    upload.resize(size_t(vertices.size() + p[1] - formatBytes));
    memset(upload.data() + vertices.size(), 0, upload.size() - vertices.size());
  }
  return upload;
}

bool WrappedMTLDevice::RecordReplayASInitialContents(ResourceId id, ResourceId source,
    const rdcarray<uint64_t> &p, const bytebuf &vertices, uint32_t kind,
    const rdcarray<ResourceId> &children, ResourceId indexSource, const bytebuf &indices,
    bool compacted, ResourceId sizeSource, const rdcarray<uint64_t> &sizeParameters)
{
  return RecordReplayASContents(id, source, p, vertices, kind, children, indexSource, indices,
      compacted, sizeSource, sizeParameters, m_ReplayASInitialContents, ResourceId());
}

bool WrappedMTLDevice::RecordReplayASContents(ResourceId id, ResourceId source,
    const rdcarray<uint64_t> &p, const bytebuf &vertices, uint32_t kind,
    const rdcarray<ResourceId> &children, ResourceId indexSource, const bytebuf &indices,
    bool compacted, ResourceId sizeSource, const rdcarray<uint64_t> &sizeParameters,
    std::map<ResourceId, std::shared_ptr<MetalASInitialBuild>> &contents, ResourceId scratch, uint64_t scratchOffset)
{
  InitialTextureAutoreleasePool pool;
  auto object = GetResourceManager()->GetResource(id, true);
  auto inputObject = GetResourceManager()->GetResource(source, true);
  if(!object || object->m_Type != eResAccelerationStructure || !object->m_Real ||
     !inputObject || inputObject->m_Type != eResBuffer || !inputObject->m_Real ||
     (scratch == ResourceId() && contents.count(id)) ||
     (kind == 8 ? !ValidMetalASMultiIndexedInputs(p, vertices, indices) :
                  p.size() != (kind == 2 ? 10U : 8U)) ||
     (kind != 1 && kind != 2 && kind != 3 && kind != 5 && kind != 8 && kind != 9 && kind != 10 && kind != 11) ||
     (kind != 2 && kind != 8 && (indexSource != ResourceId() || !indices.empty())))
  {
    RDCERR("Invalid Metal acceleration structure initial contents");
    return false;
  }
  auto input = Unwrap((WrappedMTLBuffer *)inputObject);
  if(sizeSource == ResourceId())
  {
    if(!sizeParameters.empty()) return false;
  }
  else
  {
    auto sizeObject = GetResourceManager()->GetResource(sizeSource, true);
    if(!sizeObject || sizeObject->m_Type != eResBuffer || !sizeObject->m_Real ||
       sizeParameters.size() != 2) return false;
    auto sizeBuffer = Unwrap((WrappedMTLBuffer *)sizeObject);
    const uint64_t bytes = sizeParameters[1] == uint64_t(MTL::DataTypeUInt) ? 4 :
                           sizeParameters[1] == uint64_t(MTL::DataTypeULong) ? 8 : 0;
    if(!bytes || sizeBuffer->storageMode() != MTL::StorageModeShared ||
       sizeParameters[0] % bytes || sizeParameters[0] > sizeBuffer->length() ||
       bytes > sizeBuffer->length() - sizeParameters[0]) return false;
  }
  // Typed Private placement recipes own frozen geometry/instance inputs.
  // Source heap bytes are never consumed by the reconstructed build.
  // Preserve all original offsets/strides and known source allocation bounds.
  if(input->storageMode() == MTL::StorageModeMemoryless ||
     (input->heap() && ((kind != 1 && kind != 2 && kind != 8 && kind != 9 && kind != 10 && kind != 11) || input->storageMode() != MTL::StorageModePrivate ||
        input->heap()->type() != MTL::HeapTypePlacement ||
        input->heap()->hazardTrackingMode() != MTL::HazardTrackingModeTracked))) return false;
  rdcarray<MTL::AccelerationStructure *> nativeChildren;
  rdcarray<uint64_t> identities;
  MTL::Buffer *indexInput = NULL;
  if(kind == 10)
  {
    if(!ValidMetalASEmptyIndirectParameters(p, input->length()) || !children.empty() ||
       !vertices.empty() || compacted || sizeSource != ResourceId()) return false;
  }
  else if(kind == 11)
  {
    uint64_t bytes = 0;
    if(!MetalASIndirectInstanceSpan(p, input->length(), bytes) || !children.empty() ||
       vertices.size() != bytes || !ValidMetalASInactiveInstances(vertices, p[3]) ||
       compacted || sizeSource != ResourceId()) return false;
  }
  else if(kind == 8)
  {
    auto indexObject = GetResourceManager()->GetResource(indexSource, true);
    if(!indexObject || indexObject->m_Type != eResBuffer || !indexObject->m_Real ||
       !children.empty() || vertices.size() != input->length()) return false;
    indexInput = Unwrap((WrappedMTLBuffer *)indexObject);
    if((indexInput->heap() && !ValidMetalASPrivateInstanceInput((WrappedMTLBuffer *)indexObject)) || indexInput->storageMode() == MTL::StorageModeMemoryless ||
       indices.size() != indexInput->length()) return false;
  }
  else if(kind == 2)
  {
    auto indexObject = GetResourceManager()->GetResource(indexSource, true);
    if(!indexObject || indexObject->m_Type != eResBuffer || !indexObject->m_Real || !children.empty()) return false;
    indexInput = Unwrap((WrappedMTLBuffer *)indexObject);
    uint64_t bytes = 0;
    const uint64_t formatBytes = p[2] == uint64_t(MTL::AttributeFormatFloat4) ? 16 : 12;
    if(indexInput->storageMode() == MTL::StorageModeMemoryless ||
       (indexInput->heap() && !ValidMetalASPrivateInstanceInput((WrappedMTLBuffer *)indexObject)) ||
       !MetalASInitialIndexedSpan(p, indices, bytes) || vertices.size() != bytes ||
       bytes + p[1] - formatBytes > 64*1024*1024 || p[0] > input->length() ||
       bytes + p[1] - formatBytes > input->length() - p[0] ||
       p[8] > indexInput->length() || indices.size() > indexInput->length() - p[8]) return false;
  }
  else if(kind == 3)
  {
    uint64_t bytes = 0;
    if(!children.empty() || !MetalASInitialBoxSpan(p, bytes) ||
       !ValidMetalASInitialBoxes(p, vertices) || p[0] > input->length() ||
       bytes + p[1] - 24 > input->length() - p[0]) return false;
  }
  else if(kind == 5 || kind == 9)
  {
    uint64_t packedBytes = 0;
    const bool span = kind == 9 ? MetalASIndirectInstanceSpan(p, input->length(), packedBytes) :
        MetalASInitialInstanceSpan(p, input->length(), packedBytes);
    if(!span || vertices.size() != packedBytes || children.empty() ||
       children.size() > (kind == 9 ? MetalMaxIndirectASChildren : 4) ||
       (kind == 5 && !ValidMetalASInitialInstances(vertices, p[3], children.size(), p[2]))) return false;
    std::set<ResourceId> ids;
    for(ResourceId child : children)
    {
      auto childObject = GetResourceManager()->GetResource(child, true);
      if(child == id || !ids.insert(child).second || !childObject ||
         childObject->m_Type != eResAccelerationStructure || !childObject->m_Real) return false;
      nativeChildren.push_back(Unwrap((WrappedMTLAccelerationStructure *)childObject));
      if(kind == 9) identities.push_back(((WrappedMTLAccelerationStructure *)childObject)->m_CapturedGPUResourceID);
    }
  }
  else
  {
    if(!children.empty() || p[3] == 0 || p[3] > 1000000 || p[1] < 12 || p[1] > 1024*1024 ||
       p[1] % 4 || p[0] % 4 || p[4] > 31 || p[5] > 1 || p[6] > 1 ||
       (p[7] != 0 && p[7] != uint64_t(MTL::AccelerationStructureUsageRefit))) return false;
    const uint64_t vertexSize = p[2] == uint64_t(MTL::AttributeFormatFloat3) ? 12 :
                                p[2] == uint64_t(MTL::AttributeFormatFloat4) ? 16 : 0;
    const uint64_t bytes = (p[3] * 3 - 1) * p[1] + vertexSize;
    if(!vertexSize || p[1] < vertexSize || bytes + p[1] - vertexSize > 64*1024*1024 || vertices.size() != bytes ||
       p[0] > input->length() || bytes + p[1] - vertexSize > input->length() - p[0]) return false;
  }
  // Even a sizes query must use the frozen inputs, rather than buffers the
  // application may have overwritten after the original AS completed.
  const bytebuf upload = InitialASUpload(vertices, kind, p, identities);
  if(upload.empty()) return false;
  auto staging = NS::TransferPtr(Unwrap(this)->newBuffer(upload.data(), upload.size(),
      MTL::ResourceStorageModeShared));
  NS::SharedPtr<MTL::Buffer> indexStaging;
  if(kind == 2 || kind == 8)
    indexStaging = NS::TransferPtr(Unwrap(this)->newBuffer(indices.data(), indices.size(),
        MTL::ResourceStorageModeShared));
  if(!staging || ((kind == 2 || kind == 8) && !indexStaging)) return false;
  auto sizes = Unwrap(this)->accelerationStructureSizes(InitialASDescriptor(staging.get(), 0, p,
      nativeChildren, indexStaging.get(), 0, kind));
  if(!sizes.accelerationStructureSize || !sizes.buildScratchBufferSize ||
     (!compacted && sizes.accelerationStructureSize > ((WrappedMTLAccelerationStructure *)object)->m_Size))
    return false;
  if(scratch != ResourceId())
  {
    auto scratchObject = GetResourceManager()->GetResource(scratch, true);
    if(!scratchObject || scratchObject->m_Type != eResBuffer || !scratchObject->m_Real ||
       scratchObject->m_CapturedAliasable) return false;
    auto scratchBuffer = Unwrap((WrappedMTLBuffer *)scratchObject);
    if(scratchBuffer->storageMode() != MTL::StorageModePrivate ||
       scratchBuffer->hazardTrackingMode() != MTL::HazardTrackingModeTracked ||
       scratchOffset % 256 || scratchOffset > scratchBuffer->length() ||
       sizes.buildScratchBufferSize > scratchBuffer->length() - scratchOffset) return false;
  }
  auto build = std::make_shared<MetalASInitialBuild>();
  build->source = source; build->parameters = p; build->vertices = vertices;
  build->kind = kind; build->children = children;
  build->childGPUIdentities = identities;
  build->compacted = compacted;
  build->sizeSource = sizeSource; build->sizeParameters = sizeParameters;
  build->indexSource = indexSource; build->indices = indices;
  contents[id] = build;
  return true;
}

bool WrappedMTLDevice::RecordRayIRGeometryASBuild(ResourceId structure, ResourceId vertices,
    ResourceId indices, uint32_t kind, const rdcarray<uint64_t> &parameters,
    ResourceId scratch, uint64_t scratchOffset, const bytebuf &vertexBytes, const bytebuf &indexBytes)
{
  if((kind != 1 && kind != 2 && kind != 8) || scratch == ResourceId() ||
     structure == ResourceId() || vertices == scratch || indices == scratch ||
     (kind == 8 ? parameters.size() < 20 || parameters.size() > 640 || parameters.size() % 10 :
      parameters.size() != (kind == 2 ? 10U : 8U)) || parameters[7] != 0) return false;
  auto privateInput = [&](ResourceId id) {
    auto object = GetResourceManager()->GetResource(id, true);
    if(!object || object->m_Type != eResBuffer || !object->m_Real || object->m_CapturedAliasable) return false;
    auto input = Unwrap((WrappedMTLBuffer *)object);
    return ValidMetalASPrivateInstanceInput((WrappedMTLBuffer *)object);
  };
  if(!privateInput(vertices) || ((kind == 2 || kind == 8) ? !privateInput(indices) : indices != ResourceId())) return false;
  auto prior = m_RayIRASCurrentContents.find(structure);
  if(prior != m_RayIRASCurrentContents.end() && prior->second &&
     prior->second->kind != 1 && prior->second->kind != 2 && prior->second->kind != 8) return false;
  std::map<ResourceId, std::shared_ptr<MetalASInitialBuild>> validated;
  if(!RecordReplayASContents(structure, vertices, parameters, vertexBytes, kind, {}, indices,
      indexBytes, false, ResourceId(), {}, validated, scratch, scratchOffset)) return false;
  auto finiteVertex = [&](size_t at) {
    for(size_t component = 0; component < 3; component++)
    {
      float value = 0; memcpy(&value, vertexBytes.data()+at+component*4, 4);
      if(!std::isfinite(value)) return false;
    }
    return true;
  };
  if(kind == 8)
  {
    // Whole buffers include padding and unrelated bytes. Validate only vertices
    // actually referenced by each typed geometry/index range.
    for(size_t base = 0; base < parameters.size(); base += 10)
    {
      const auto *g = parameters.data()+base;
      const size_t bytes = g[9] == uint64_t(MTL::IndexTypeUInt16) ? 2 : 4;
      for(uint64_t i=0; i<g[3]*3; i++)
      {
        uint32_t index=0; memcpy(&index,indexBytes.data()+g[8]+i*bytes,bytes);
        if(!finiteVertex(g[0]+index*g[1])) return false;
      }
    }
  }
  else
  {
    const uint64_t format = parameters[2] == uint64_t(MTL::AttributeFormatFloat4) ? 16 : 12;
    for(size_t at = 0; at + format <= vertexBytes.size(); at += parameters[1])
      if(!finiteVertex(at)) return false;
  }
  if(!m_RayIRDispatches.empty() || !m_RayQueryDispatches.empty() || !m_RayQueryHeapDispatches.empty())
    m_RayIRASCurrentContents[structure] = validated[structure];
  return true;
}

bool WrappedMTLDevice::RecordRayIRIndirectASBuild(ResourceId structure,
    const rdcarray<ResourceId> &children, ResourceId instances, ResourceId scratch,
    const rdcarray<uint64_t> &parameters, const bytebuf &descriptorBytes, uint64_t scratchOffset)
{
  if(m_RayIRDispatches.empty() && m_RayQueryDispatches.empty() && m_RayQueryHeapDispatches.empty()) return true;
  // Like DX12/Vulkan build-input copies, a frame recipe owns frozen input bytes
  // and typed BLAS references. Never mutate the initial recipe used by EID0.
  auto source = GetResourceManager()->GetResource(instances, true);
  if(parameters.size() != 8 || parameters[3] > 64 || scratch == ResourceId() ||
     !source || source->m_Type != eResBuffer || !source->m_Real ||
     !ValidMetalASPrivateInstanceInput((WrappedMTLBuffer *)source)) return false;
  for(ResourceId child : children)
  {
    auto primitive = m_RayIRASCurrentContents.find(child);
    if(primitive == m_RayIRASCurrentContents.end() || !primitive->second ||
       (primitive->second->kind != 1 && primitive->second->kind != 2 &&
        primitive->second->kind != 8)) return false;
  }
  const uint32_t kind = parameters[3] == 0 ? 10 : children.empty() ? 11 : 9;
  return RecordReplayASContents(structure, instances, parameters, descriptorBytes, kind, children,
      ResourceId(), bytebuf(), false, ResourceId(), rdcarray<uint64_t>(),
      m_RayIRASCurrentContents, scratch, scratchOffset);
}

bool WrappedMTLDevice::RestoreReplayASInitialContents()
{
  m_RayIRASCurrentContents = m_ReplayASInitialContents;
  m_ReplayFrozenASInputs.clear();
  // Creation chunks are loaded once, including frame-created refit destinations.
  // Clear logical build state from the previous replay before applying the initial
  // recipes; otherwise an out-of-place refit sees an already-built destination.
  for(auto object : GetResourceManager()->GetAccelerationStructures())
  {
    auto structure = (WrappedMTLAccelerationStructure *)object;
    structure->m_LastBuildKind = 0;
    structure->m_LastTriangleCount = 0;
    structure->m_LastBuildCommandBuffer = ResourceId();
    structure->m_LastCompactedSizeBuffer = ResourceId();
    structure->m_LastCompactedWriteCommandBuffer = ResourceId();
    structure->m_LastInitialCompactedSize = 0;
  }
  // Prove all dependencies before the first reconstruction command reaches Metal.
  for(const auto &entry : m_ReplayASInitialContents)
    for(ResourceId child : entry.second->children)
    {
      auto initial = m_ReplayASInitialContents.find(child);
      if(initial == m_ReplayASInitialContents.end() ||
         (initial->second->kind != 1 && initial->second->kind != 2 &&
          initial->second->kind != 3 && initial->second->kind != 8)) return false;
    }
  // Restore primitive AS before TLAS, independent of initial-chunk/resource-ID order.
  for(uint32_t kind : {1U, 2U, 3U, 8U, 5U, 9U, 10U, 11U})
  for(const auto &entry : m_ReplayASInitialContents)
  {
    if(entry.second->kind != kind) continue;
    InitialTextureAutoreleasePool pool;
    auto object = GetResourceManager()->GetResource(entry.first, true);
    if(!object || object->m_Type != eResAccelerationStructure || !object->m_Real) return false;
    auto structure = (WrappedMTLAccelerationStructure *)object;
    const auto &build = *entry.second;
    const auto &p = build.parameters;
    const bytebuf upload = InitialASUpload(build.vertices, kind, p, build.childGPUIdentities);
    if(upload.empty()) return false;
    auto input = NS::TransferPtr(Unwrap(this)->newBuffer(upload.data(),
        upload.size(), MTL::ResourceStorageModeShared));
    if(!input) return false;
    rdcarray<MTL::AccelerationStructure *> children;
    for(ResourceId child : build.children)
    {
      auto childObject = GetResourceManager()->GetResource(child, true);
      if(!childObject || childObject->m_Type != eResAccelerationStructure || !childObject->m_Real) return false;
      children.push_back(Unwrap((WrappedMTLAccelerationStructure *)childObject));
    }
    NS::SharedPtr<MTL::Buffer> indices;
    if(kind == 2 || kind == 8)
    {
      indices = NS::TransferPtr(Unwrap(this)->newBuffer(build.indices.data(),
          build.indices.size(), MTL::ResourceStorageModeShared));
      if(!indices) return false;
    }
    auto descriptor = InitialASDescriptor(input.get(), 0, p, children, indices.get(), 0, kind);
    auto sizes = Unwrap(this)->accelerationStructureSizes(descriptor);
    if(!sizes.accelerationStructureSize || !sizes.buildScratchBufferSize ||
       (!build.compacted && sizes.accelerationStructureSize > structure->m_Size)) return false;
    NS::SharedPtr<MTL::AccelerationStructure> fullStructure;
    NS::SharedPtr<MTL::Buffer> compactedSize;
    auto target = Unwrap(structure);
    if(build.compacted)
    {
      fullStructure = NS::TransferPtr(Unwrap(this)->newAccelerationStructure(sizes.accelerationStructureSize));
      if(!fullStructure) return false;
      target = fullStructure.get();
    }
    if(build.compacted || build.sizeSource != ResourceId())
    {
      compactedSize = NS::TransferPtr(Unwrap(this)->newBuffer(8, MTL::ResourceStorageModeShared));
      if(!compactedSize) return false;
    }
    auto scratch = NS::TransferPtr(Unwrap(this)->newBuffer(sizes.buildScratchBufferSize,
        MTL::ResourceStorageModePrivate));
    auto command = m_mtlCommandQueue->commandBuffer();
    auto encoder = command && scratch ? command->accelerationStructureCommandEncoder() : NULL;
    if(!encoder) return false;
    encoder->buildAccelerationStructure(target, descriptor, scratch.get(), 0);
    if(compactedSize)
      encoder->writeCompactedAccelerationStructureSize(target, compactedSize.get(), 0, MTL::DataTypeULong);
    encoder->endEncoding(); command->commit(); command->waitUntilCompleted();
    if(command->status() != MTL::CommandBufferStatusCompleted || command->error()) return false;
    if(build.compacted)
    {
      // Rebuilding into the original compact allocation can overrun it. First
      // build a full temporary AS, then validate this device's completed size
      // query before issuing the compact copy into the captured allocation.
      uint64_t size = 0;
      memcpy(&size, compactedSize->contents(), sizeof(size));
      if(!size || size > structure->m_Size) return false;
      command = m_mtlCommandQueue->commandBuffer();
      encoder = command ? command->accelerationStructureCommandEncoder() : NULL;
      if(!encoder) return false;
      encoder->copyAndCompactAccelerationStructure(target, Unwrap(structure));
      encoder->endEncoding(); command->commit(); command->waitUntilCompleted();
      if(command->status() != MTL::CommandBufferStatusCompleted || command->error()) return false;
    }
    structure->m_LastBuildKind = kind == 8 ? 8 : kind == 5 || kind == 9 || kind == 10 || kind == 11 ? 5 : p[7] ? (kind == 2 ? 7 : kind == 3 ? 6 : 4) : kind;
    structure->m_LastTriangleCount = kind == 1 || (kind == 2 && p[7]) ? p[3] : 0;
    structure->m_LastInstanceBuffer = kind == 5 || kind == 9 || kind == 10 || kind == 11 ? build.source : ResourceId();
    structure->m_LastInstanceChild = kind == 5 || kind == 9 ? build.children[0] : ResourceId();
    structure->m_LastVertexStride = p[1];
    structure->m_LastVertexFormat = MTL::AttributeFormat(p[2]);
    structure->m_LastVertices = build.source;
    structure->m_LastBoxes = kind == 3 ? build.source : ResourceId();
    structure->m_LastBoxCount = kind == 3 ? p[3] : 0;
    structure->m_LastBoxOffset = kind == 3 ? p[0] : 0;
    structure->m_LastBoxStride = kind == 3 ? p[1] : 0;
    structure->m_LastBoxTableOffset = kind == 3 ? p[4] : 0;
    structure->m_LastBoxOpaque = kind == 3 && p[5] != 0;
    structure->m_LastIndices = build.indexSource;
    structure->m_LastIndexedVertexOffset = p[0];
    structure->m_LastIndexedIndexOffset = kind == 2 ? p[8] : 0;
    structure->m_LastIndexType = kind == 2 ? MTL::IndexType(p[9]) : MTL::IndexTypeUInt16;
    structure->m_LastIndexedTableOffset = p[4];
    structure->m_LastIndexedOpaque = p[5] != 0;
    structure->m_LastAllowDuplicateIntersectionFunctionInvocation = p[6] != 0;
    // This initial-state build has no captured command buffer. A distinct AS ID
    // represents its completed submission for existing cross-submission checks.
    structure->m_LastBuildCommandBuffer = entry.first;
    structure->m_LastCompactedSizeBuffer = ResourceId();
    structure->m_LastCompactedWriteCommandBuffer = ResourceId();
    if(build.sizeSource != ResourceId())
    {
      structure->m_LastCompactedSizeBuffer = build.sizeSource;
      structure->m_LastCompactedSizeOffset = build.sizeParameters[0];
      structure->m_LastCompactedSizeType = MTL::DataType(build.sizeParameters[1]);
      structure->m_LastCompactedWriteCommandBuffer = entry.first;
      memcpy(&structure->m_LastInitialCompactedSize, compactedSize->contents(), sizeof(uint64_t));
      if(!structure->m_LastInitialCompactedSize) return false;
    }
  }
  return true;
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
