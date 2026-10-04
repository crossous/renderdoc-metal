// SPDX-License-Identifier: MIT
#include "metal_device.h"
#include "metal_render_command_encoder.h"
#include "metal_buffer.h"
#include "metal_texture.h"
#include "metal_sampler_state.h"
#include "metal_heap.h"
#include "metal_event.h"
#include "metal_command_buffer.h"
#include "metal_compute_command_encoder.h"
#include "metal_replay.h"
#include "metal_replay_budget.h"
#include "serialise/rdcfile.h"

// Initial contents are logical subresources, not native heap footprints.
// Reuse initial-state format/mip rules when budgeting sourced background textures.
static bool DescriptorTextureInitialSize(const RDMTL::TextureDescriptor &d, uint64_t &size,
                                         bool allFamilies, bool managed)
{
  size = 0;
  uint32_t bw = 0, bh = 0, bytes = 0;
  const bool depthStencil = d.pixelFormat == MTL::PixelFormatDepth32Float_Stencil8;
  const bool depth = depthStencil || d.pixelFormat == MTL::PixelFormatDepth16Unorm ||
      d.pixelFormat == MTL::PixelFormatDepth32Float;
  if(!d.width || d.width > 8192 || !d.height || d.height > 8192 ||
     !d.depth || d.depth > 256 || !d.arrayLength || d.arrayLength > 128 ||
     d.sampleCount != 1 || d.mipmapLevelCount > 14 ||
     !ValidTextureMipCount(d.width, d.height, d.depth, d.mipmapLevelCount) ||
     (d.storageMode != MTL::StorageModePrivate && d.storageMode != MTL::StorageModeShared && !(managed && d.storageMode == MTL::StorageModeManaged)) ||
     (!allFamilies && (depth || d.textureType != MTL::TextureType2D)) ||
     (!depthStencil && !GetTextureDataBlockShape(d.pixelFormat, bw, bh, bytes))) return false;
  if(depthStencil) { bw = bh = 1; bytes = 5; }
  uint64_t slices = 1;
  if(d.textureType == MTL::TextureType2DArray) slices = d.arrayLength;
  else if(d.textureType == MTL::TextureTypeCube || d.textureType == MTL::TextureTypeCubeArray)
  {
    if(d.width != d.height) return false;
    slices = 6 * d.arrayLength;
  }
  else if(d.textureType != MTL::TextureType2D && d.textureType != MTL::TextureType3D) return false;
  if((d.textureType != MTL::TextureType3D && d.depth != 1) ||
     ((d.textureType == MTL::TextureType2D || d.textureType == MTL::TextureType3D ||
       d.textureType == MTL::TextureTypeCube) && d.arrayLength != 1) ||
     (depth && d.textureType == MTL::TextureType3D)) return false;
  for(uint64_t mip = 0; mip < d.mipmapLevelCount; mip++)
  {
    const uint64_t w = RDCMAX(1ULL, uint64_t(d.width) >> mip);
    const uint64_t h = RDCMAX(1ULL, uint64_t(d.height) >> mip);
    const uint64_t z = d.textureType == MTL::TextureType3D ? RDCMAX(1ULL, uint64_t(d.depth) >> mip) : 1;
    size += ((w + bw - 1) / bw) * ((h + bh - 1) / bh) * bytes * z * slices;
    if(size > 128ULL * 1024 * 1024) return false;
  }
  return true;
}

static bool ValidDescriptorFrameTexture(const RDMTL::TextureDescriptor &d, uint32_t coverage)
{
  const bool tracked = d.storageMode == MTL::StorageModePrivate &&
      d.cpuCacheMode == MTL::CPUCacheModeDefaultCache &&
      d.hazardTrackingMode == MTL::HazardTrackingModeTracked &&
      d.resourceOptions == (MTL::ResourceStorageModePrivate | MTL::ResourceHazardTrackingModeTracked) &&
      d.allowGPUOptimizedContents && d.swizzle.red == MTL::TextureSwizzleRed &&
      d.swizzle.green == MTL::TextureSwizzleGreen && d.swizzle.blue == MTL::TextureSwizzleBlue &&
      d.swizzle.alpha == MTL::TextureSwizzleAlpha;
  if(coverage >= 53 && (d.pixelFormat == MTL::PixelFormatDepth16Unorm ||
      d.pixelFormat == MTL::PixelFormatDepth32Float || d.pixelFormat == MTL::PixelFormatDepth32Float_Stencil8))
  {
    uint64_t logicalSize = 0;
    const uint64_t rt = MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead;
    return tracked && d.textureType == MTL::TextureType2D && d.width <= 512 && d.height <= 512 &&
        d.depth == 1 && d.arrayLength == 1 && d.mipmapLevelCount == 1 &&
        (d.usage == rt || d.usage == (rt | MTL::TextureUsagePixelFormatView)) &&
        DescriptorTextureInitialSize(d, logicalSize, true, false);
  }
  if(coverage >= 44 && (d.pixelFormat == MTL::PixelFormatR32Uint ||
      d.pixelFormat == MTL::PixelFormatR8Unorm || d.pixelFormat == MTL::PixelFormatRGB10A2Unorm))
  {
    uint64_t logicalSize = 0;
    const bool integer = d.pixelFormat == MTL::PixelFormatR32Uint;
    const bool shape = integer ? (d.textureType == MTL::TextureType2D || d.textureType == MTL::TextureType2DArray) :
        d.pixelFormat == MTL::PixelFormatR8Unorm ? d.textureType == MTL::TextureType2D :
        d.textureType == MTL::TextureType3D;
    const uint64_t rw = MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite;
    return tracked && shape && d.width <= 512 && d.height <= 512 && d.depth <= 16 &&
        d.arrayLength <= 8 && d.mipmapLevelCount == 1 &&
        (d.usage == rw || d.usage == (rw | MTL::TextureUsageRenderTarget) ||
         (integer && d.textureType == MTL::TextureType2D && d.usage == (rw | MTL::TextureUsageShaderAtomic))) &&
        DescriptorTextureInitialSize(d, logicalSize, true, false);
  }
  if(coverage >= 39 && (d.textureType == MTL::TextureType2DArray || d.textureType == MTL::TextureType3D ||
      (coverage >= 43 && d.textureType == MTL::TextureType2D &&
       (d.pixelFormat == MTL::PixelFormatR32Float || d.pixelFormat == MTL::PixelFormatRGBA16Float ||
        d.pixelFormat == MTL::PixelFormatRGBA32Float))))
  {
    uint64_t logicalSize = 0;
    // v65 uses the existing native heap extent/budget and lifetime checks for
    // 2D float images up to the same limit as other frame colour targets. Keep
    // array/volume and older coverage bounds unchanged.
    const uint64_t extent = coverage >= 65 && d.textureType == MTL::TextureType2D ? 512 : 64;
    return tracked && d.width <= extent && d.height <= extent && d.depth <= (coverage >= 43 ? 64U : 16U) && d.arrayLength <= 8 &&
        d.mipmapLevelCount == 1 &&
        (d.usage == (MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite) ||
         d.usage == (MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite | MTL::TextureUsageRenderTarget)) &&
        (d.pixelFormat == MTL::PixelFormatR32Float || d.pixelFormat == MTL::PixelFormatRGBA16Float ||
         d.pixelFormat == MTL::PixelFormatRGBA32Float) &&
        DescriptorTextureInitialSize(d, logicalSize, true, false);
  }
  return d.textureType == MTL::TextureType2D && d.width &&
      d.width <= (coverage >= 38 ? 512U : 2U) && d.height &&
      d.height <= (coverage >= 38 ? 512U : 2U) &&
      d.depth == 1 && d.mipmapLevelCount == 1 && d.arrayLength == 1 && d.sampleCount == 1 &&
      (d.pixelFormat == MTL::PixelFormatRGBA8Unorm || d.pixelFormat == MTL::PixelFormatBGRA8Unorm ||
       (coverage >= 38 && (d.pixelFormat == MTL::PixelFormatR16Float ||
                          d.pixelFormat == MTL::PixelFormatRG11B10Float))) &&
      d.storageMode == MTL::StorageModePrivate && d.cpuCacheMode == MTL::CPUCacheModeDefaultCache &&
      d.hazardTrackingMode == MTL::HazardTrackingModeTracked &&
      d.resourceOptions == (MTL::ResourceStorageModePrivate | MTL::ResourceHazardTrackingModeTracked) &&
      (d.usage == (MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead) ||
       (coverage >= 38 && (d.usage == (MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite) ||
         d.usage == (MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite)))) &&
      d.allowGPUOptimizedContents && d.swizzle.red == MTL::TextureSwizzleRed &&
      d.swizzle.green == MTL::TextureSwizzleGreen && d.swizzle.blue == MTL::TextureSwizzleBlue &&
      d.swizzle.alpha == MTL::TextureSwizzleAlpha;
}

static bool ValidDescriptorDrawableTexture(MTL::Texture *texture)
{
  return texture && texture->textureType()==MTL::TextureType2D &&
      (texture->pixelFormat()==MTL::PixelFormatBGRA8Unorm ||
       texture->pixelFormat()==MTL::PixelFormatRGB10A2Unorm) &&
      texture->width() && texture->width()<=2048 && texture->height() && texture->height()<=2048 &&
      texture->depth()==1 && texture->arrayLength()==1 && texture->mipmapLevelCount()==1 &&
      texture->sampleCount()==1 && !texture->framebufferOnly() &&
      (texture->storageMode()==MTL::StorageModePrivate || texture->storageMode()==MTL::StorageModeShared ||
       texture->storageMode()==MTL::StorageModeManaged);
}

static uint64_t DescriptorFrameBufferLimit(bool heap, uint32_t coverage)
{
  if(coverage >= 65 && heap) return 1024ULL * 1024;
  return coverage >= 40 ? (heap ? 128ULL * 1024 : 8ULL * 1024 * 1024) : 64ULL * 1024;
}

// Logical descriptor capacity is independent of native allocator rounding.
// v41 retains explicit slot/source identities and validates every unknown slot.
static uint64_t DescriptorTableBufferLimit(uint32_t coverage)
{
  return coverage >= 41 ? 32ULL * 1024 * 1024 : 64ULL * 1024;
}

static uint64_t DescriptorPlainCopyLimit(uint32_t coverage)
{
  return coverage >= 47 ? 1024ULL * 1024 : 64ULL * 1024;
}

static bool ValidDescriptorLayout(uint64_t offset, uint64_t count, uint64_t stride, uint32_t schema)
{
  const uint64_t minimum = schema == 0 ? 8 : 24;
  return schema <= 3 && count && count <= 786432 && stride >= minimum && stride <= 4096 &&
         stride % 8 == 0 && offset % 8 == 0 && count <= (32 * 1024 * 1024ULL) / stride &&
         offset <= ~0ULL - count * stride;
}

// Apple IR runtime direct draw parameters contain five 32-bit scalar fields;
// slot 5 contains the 16-bit index kind (or UE's explicit 32-bit null value).
// Other inline payloads still need sourced GPU-address layouts.
static bool ValidInlineDrawConstants(uint32_t stage, uint64_t index, uint64_t length)
{
  return stage == 1 && ((index == 4 && length == 20) ||
                       (index == 5 && (length == 2 || length == 4)));
}

static bool DescriptorChunkStartsWith(const rdcstr &name, const char *prefix)
{
  return !strncmp(name.c_str(), prefix, strlen(prefix));
}

uint32_t MetalCapturer::SetObjectAnnotation(void *object, const char *key,
    RENDERDOC_AnnotationType type, uint32_t width, const RENDERDOC_AnnotationValue *value)
{
  return m_Device.AnnotateDescriptorTable(object, key, type, width, value);
}

uint32_t WrappedMTLDevice::AnnotateDescriptorTable(void *object, const char *key,
    RENDERDOC_AnnotationType type, uint32_t width, const RENDERDOC_AnnotationValue *value)
{
  SCOPED_READLOCK(m_CapTransitionLock);
  SCOPED_LOCK(m_DescriptorMetadataLock);
  // The controlled-capture client can acknowledge a presentation only after the native
  // present hook has supplied a backbuffer. No chunk or GPU work is produced by this probe.
  if(key && !strcmp(key, "metal.capturePresented"))
    return object == this && type == eRENDERDOC_Empty && width == 0 && !value &&
           IsActiveCapturing(m_State) && m_CapturedBackbuffer.load() ? 0 : 2;
  if(!key || !value)
    return 2;
  // Execution-point layouts are diagnostic until all stages and lifetime rules are
  // validated. stage: compute=0, vertex=1, fragment=2, object=3, mesh=4.
  if(!strcmp(key, "metal.descriptorInlineLayout") || !strcmp(key, "metal.descriptorInlineBinding") ||
     !strcmp(key, "metal.inlineDrawConstants"))
  {
    if(!IsActiveCapturing(m_State) || type != eRENDERDOC_UInt64 || width != 4)
      return 2;
    WrappedMTLObject *wrapped = GetResourceManager()->FindAnnotationObject(object);
    const bool binding = !strcmp(key, "metal.descriptorInlineBinding");
    const bool constants = !strcmp(key, "metal.inlineDrawConstants");
    const uint64_t stage = binding ? value->vector.uint64[0] >> 32 : value->vector.uint64[0];
    const uint64_t index = binding ? value->vector.uint64[0] & 0xffffffffULL : value->vector.uint64[1];
    if(!wrapped || !wrapped->m_Real || stage > 4 || index >= 31 ||
       wrapped->m_Type != (stage == 0 ? eResComputeCommandEncoder : eResRenderCommandEncoder))
      return 2;
    CACHE_THREAD_SERIALISER();
    if(binding)
    {
      const uint64_t entry = value->vector.uint64[1], memberOffset = value->vector.uint64[3];
      WrappedMTLObject *source = GetResourceManager()->FindAnnotationObject(
          (void *)(uintptr_t)value->vector.uint64[2]);
      if(entry >= 512 || !source || !source->m_Real || source->m_Type != eResBuffer ||
         memberOffset >= Unwrap((WrappedMTLBuffer *)source)->length())
        return 2;
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandEncoder_DescriptorInlineBinding);
      Serialise_DescriptorInlineBinding(ser, GetResID(wrapped), (uint32_t)stage,
          (uint32_t)index, entry, GetResID(source), memberOffset);
      AddFrameCaptureRecordChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(source), eFrameRef_Read);
    }
    else
    {
      const uint64_t count = constants ? 0 : value->vector.uint64[2];
      const uint64_t stride = constants ? value->vector.uint64[2] : value->vector.uint64[3];
      if(constants ? (!ValidInlineDrawConstants((uint32_t)stage, index, stride) || value->vector.uint64[3]) :
          (!ValidDescriptorLayout(0, count, stride, 0) || count * stride > 4096))
        return 2;
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLCommandEncoder_DescriptorInlineLayout);
      Serialise_DescriptorInlineLayout(ser, GetResID(wrapped), (uint32_t)stage,
          (uint32_t)index, count, stride);
      AddFrameCaptureRecordChunk(scope.Get());
    }
    return 0;
  }
  if(!strcmp(key, "metal.descriptorSlotProducer"))
  {
    if(!IsActiveCapturing(m_State) || type != eRENDERDOC_UInt64 || width != 4) return 2;
    WrappedMTLObject *buffer = GetResourceManager()->FindAnnotationObject(object);
    WrappedMTLObject *encoder = GetResourceManager()->FindAnnotationObject((void *)(uintptr_t)value->vector.uint64[1]);
    WrappedMTLObject *source = GetResourceManager()->FindAnnotationObject((void *)(uintptr_t)value->vector.uint64[2]);
    const uint64_t offset = value->vector.uint64[0], sourceOffset = value->vector.uint64[3];
    if(!buffer || buffer->m_Type != eResBuffer || !buffer->m_Real || offset % 8 ||
       !encoder || encoder->m_Type != eResComputeCommandEncoder || !encoder->m_Real ||
       !source || source->m_Type != eResBuffer || !source->m_Real || sourceOffset % 8)
      return 2;
    const uint64_t length = Unwrap((WrappedMTLBuffer *)buffer)->length();
    const uint64_t sourceLength = Unwrap((WrappedMTLBuffer *)source)->length();
    if(offset > length || 24 > length - offset || sourceOffset > sourceLength || 24 > sourceLength - sourceOffset) return 2;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_DescriptorSlotProducer);
    Serialise_DescriptorSlotProducer(ser, GetResID(buffer), offset, GetResID(encoder), GetResID(source), sourceOffset);
    AddFrameCaptureRecordChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_ReadBeforeWrite);
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(source), eFrameRef_Read);
    return 0;
  }
  if(!strcmp(key, "metal.descriptorSlotBinding"))
  {
    if(!IsCaptureMode(m_State) || type != eRENDERDOC_UInt64 || width != 4)
      return 2;
    WrappedMTLObject *wrapped = GetResourceManager()->FindAnnotationObject(object);
    const uint64_t offset = value->vector.uint64[0], kind = value->vector.uint64[1],
        memberOffset = value->vector.uint64[3];
    WrappedMTLObject *source = GetResourceManager()->FindAnnotationObject(
        (void *)(uintptr_t)value->vector.uint64[2]);
    if(!wrapped || wrapped->m_Type != eResBuffer || !wrapped->m_Real || offset % 8 ||
       kind > 2 || !source || !source->m_Real ||
       source->m_Type != (kind == 0 ? eResBuffer : kind == 1 ? eResTexture : eResSamplerState))
      return 2;
    MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)wrapped);
    if(offset > buffer->length() || 24 > buffer->length() - offset ||
       (kind == 0 ? memberOffset >= Unwrap((WrappedMTLBuffer *)source)->length() : memberOffset != 0))
      return 2;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_DescriptorSlotBinding);
    Serialise_DescriptorSlotBinding(ser, GetResID(wrapped), offset, GetResID(source),
        (uint32_t)kind, memberOffset);
    Chunk *captured = scope.Get();
    if(IsActiveCapturing(m_State))
    {
      AddFrameCaptureRecordChunk(captured);
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(wrapped), eFrameRef_Read);
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(source), eFrameRef_Read);
    }
    m_CapturedDescriptorSlotSources[make_rdcpair(GetResID(wrapped), offset)][(uint32_t)kind] = GetResID(source);
    SaveDescriptorHistoryChunk(GetResID(wrapped), captured);
    return 0;
  }
  // Diagnostic provider records deliberately do not grant relocation coverage. As in
  // D3D12's descriptor shadow, the producer records allocation/update/free explicitly;
  // stale non-zero storage is not evidence that a descriptor remains allocated.
  if(!strcmp(key, "metal.descriptorSlotEvent") || !strcmp(key, "metal.descriptorSlotGPUValue"))
  {
    if(!IsCaptureMode(m_State) || type != eRENDERDOC_UInt64 || width != 4)
      return 2;
    WrappedMTLObject *wrapped = GetResourceManager()->FindAnnotationObject(object);
    const bool gpuValue = !strcmp(key, "metal.descriptorSlotGPUValue");
    const uint64_t offset = value->vector.uint64[0], generation = gpuValue ? 0 : value->vector.uint64[1];
    const uint64_t event = gpuValue ? 3 : value->vector.uint64[2];
    const uint64_t descriptorType = gpuValue ? 0 : value->vector.uint64[3];
    if(!wrapped || wrapped->m_Type != eResBuffer || !wrapped->m_Real || offset % 8 ||
       (!gpuValue && (!generation || event > 2 || descriptorType > 255)))
      return 2;
    MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)wrapped);
    if(offset > buffer->length() || 24 > buffer->length() - offset)
      return 2;
    bytebuf data;
    if(gpuValue)
      data.assign((const byte *)&value->vector.uint64[1], 24);
    else if(event == 2)
    {
      if(buffer->storageMode() != MTL::StorageModeShared || !buffer->contents())
        return 2;
      data.assign((const byte *)buffer->contents() + offset, 24);
    }
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_DescriptorSlotEvent);
    Serialise_DescriptorSlotEvent(ser, GetResID(wrapped), offset, generation,
        (uint32_t)event, (uint32_t)descriptorType, data);
    Chunk *captured = scope.Get();
    if(IsActiveCapturing(m_State))
    {
      AddFrameCaptureRecordChunk(captured);
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(wrapped), eFrameRef_Read);
    }
    const auto slotKey = make_rdcpair(GetResID(wrapped), offset);
    if(event == 0) m_CapturedLiveDescriptorSlots.insert(slotKey);
    if(event == 1) m_CapturedLiveDescriptorSlots.erase(slotKey);
    if(event != 0) m_CapturedDescriptorSlotSources.erase(slotKey);
    SaveDescriptorHistoryChunk(GetResID(wrapped), captured);
    return 0;
  }
  if(!strcmp(key, "metal.descriptorCPUWrite"))
  {
    if(!IsActiveCapturing(m_State) || m_DescriptorCoverage < 2 ||
       type != eRENDERDOC_UInt64 || width != 2)
      return 2;
    WrappedMTLObject *wrapped = GetResourceManager()->FindAnnotationObject(object);
    const uint64_t start = value->vector.uint64[0], size = value->vector.uint64[1];
    if(!wrapped || wrapped->m_Type != eResBuffer || !wrapped->m_Real ||
       !m_DescriptorGPUWrittenBuffers.count(GetResID(wrapped)) ||
       !ValidDescriptorCPUWrite(GetResID(wrapped), start, size))
      return 2;
    MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)wrapped);
    if(buffer->storageMode() != MTL::StorageModeShared || !buffer->contents() ||
       start > buffer->length() || size > buffer->length() - start)
      return 2;
    bytebuf data((byte *)buffer->contents() + start, size);
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_DescriptorCPUWrite);
    Serialise_DescriptorCPUWrite(ser, GetResID(wrapped), start, data);
    AddFrameCaptureRecordChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(wrapped), eFrameRef_Read);
    return 0;
  }
  if(!strcmp(key, "metal.descriptorBytes"))
  {
    WrappedMTLObject *wrapped = GetResourceManager()->FindAnnotationObject(object);
    const uint64_t index = value->vector.uint64[0], offset = value->vector.uint64[1],
        count = value->vector.uint64[2], stride = value->vector.uint64[3];
    if(!IsActiveCapturing(m_State) || m_DescriptorCoverage != 3 ||
       type != eRENDERDOC_UInt64 || width != 4 || !wrapped ||
       wrapped->m_Type != eResComputeCommandEncoder || !wrapped->m_Real || index >= 31 ||
       !ValidDescriptorLayout(offset, count, stride, 0) || offset > 4096 || count * stride > 4096 - offset)
      return 2;
    auto &layouts = m_DescriptorBytes[GetResID(wrapped)];
    if(layouts.count(index))
    {
      const DescriptorTable &old = layouts[index];
      return old.offset == offset && old.count == count && old.stride == stride ? 0 : 2;
    }
    layouts[index] = {GetResID(wrapped), offset, count, stride, 0};
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputeCommandEncoder_DeclareDescriptorBytes);
    Serialise_DeclareDescriptorBytes(ser, GetResID(wrapped), index, offset, count, stride);
    AddFrameCaptureRecordChunk(scope.Get());
    return 0;
  }
  if(!strcmp(key,"metal.renderWritesDeclared"))
  {
    if(!IsCaptureMode(m_State))return 2;
    WrappedMTLObject *wrapped=GetResourceManager()->FindAnnotationObject(object);
    if(type!=eRENDERDOC_UInt32 || width!=0 || value->uint32!=1 || !wrapped ||
       wrapped->m_Type!=eResRenderCommandEncoder)return 2;
    ((WrappedMTLRenderCommandEncoder *)wrapped)->MarkCaptureWritesDeclared();
    return 0;
  }
  const bool frameDeclaration = IsActiveCapturing(m_State) &&
      (!strcmp(key, "metal.descriptorTable") || !strcmp(key, "metal.descriptorGPUWrites"));
  if(!IsBackgroundCapturing(m_State) && !frameDeclaration)
    return 2;
  if(!strcmp(key, "metal.descriptorCoverage"))
  {
    if(object != this || type != eRENDERDOC_UInt32 || width != 0 ||
       (value->uint32 < 1 || value->uint32 > 66))
      return 2;
    if(m_DescriptorCoverage)
      return m_DescriptorCoverage == value->uint32 ? 0 : 2;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_DeclareDescriptorCoverage);
    Serialise_DeclareDescriptorCoverage(ser, value->uint32);
    GetRecord(this)->AddChunk(scope.Get());
    m_DescriptorCoverage = value->uint32;
    return 0;
  }
  if(!strcmp(key, "metal.descriptorGPUWrites"))
  {
    WrappedMTLObject *wrapped = GetResourceManager()->FindAnnotationObject(object);
    if(!wrapped || wrapped->m_Type != eResBuffer || !wrapped->m_Real ||
       type != eRENDERDOC_UInt32 || width != 0 || value->uint32 != 1)
      return 2;
    bool declared = false;
    for(const DescriptorTable &layout : m_DescriptorTables)
      declared |= layout.buffer == GetResID(wrapped);
    if(!declared)
      return 2;
    if(!m_DescriptorGPUWrittenBuffers.insert(GetResID(wrapped)).second)
      return 0;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_DeclareDescriptorGPUWrites);
    Serialise_DeclareDescriptorGPUWrites(ser, GetResID(wrapped));
    Chunk *declaration = scope.Get();
    GetRecord(wrapped)->AddChunk(declaration);
    if(frameDeclaration) AddFrameCaptureRecordChunk(declaration->Duplicate());
    return 0;
  }
  if(strcmp(key, "metal.descriptorTable") || type != eRENDERDOC_UInt64 || width != 4)
    return 2;
  WrappedMTLObject *wrapped = GetResourceManager()->FindAnnotationObject(object);
  if(!wrapped || wrapped->m_Type != eResBuffer || !wrapped->m_Real)
    return 2;
  const uint64_t schema = value->vector.uint64[0], offset = value->vector.uint64[1],
                 count = value->vector.uint64[2], stride = value->vector.uint64[3];
  MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)wrapped);
  if(schema > 3 || !ValidDescriptorLayout(offset, count, stride, (uint32_t)schema) ||
     buffer->storageMode() != MTL::StorageModeShared || offset > buffer->length() ||
     count * stride > buffer->length() - offset)
    return 2;
  for(const DescriptorTable &layout : m_DescriptorTables)
    if(layout.buffer == GetResID(wrapped) &&
       offset < layout.offset + layout.count * layout.stride &&
       layout.offset < offset + count * stride)
      return layout.offset == offset && layout.count == count && layout.stride == stride &&
             layout.schema == schema ? 0 : 2;
  DescriptorTable layout = {GetResID(wrapped), offset, count, stride, (uint32_t)schema};
  m_DescriptorTables.push_back(layout);
  CACHE_THREAD_SERIALISER();
  SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBuffer_DeclareDescriptorTable);
  Serialise_DeclareDescriptorTable(ser, layout.buffer, offset, count, stride, layout.schema);
  Chunk *declaration = scope.Get();
  GetRecord(wrapped)->AddChunk(declaration);
  if(frameDeclaration) AddFrameCaptureRecordChunk(declaration->Duplicate());
  return 0;
}

void WrappedMTLDevice::SaveDescriptorHistoryChunk(ResourceId buffer, Chunk *chunk)
{
  // Active chunks are owned by the frame record. Background diagnostics have their
  // own owner instead of a mutable resource creation record. A frozen start snapshot
  // prevents post-EndFrameCapture retirement callbacks from changing this capture.
  m_DescriptorHistory.push_back({buffer, IsActiveCapturing(m_State) ? chunk->Duplicate() : chunk});
}

void WrappedMTLDevice::ClearDescriptorHistorySnapshot()
{
  for(Chunk *chunk : m_DescriptorHistorySnapshot)
    chunk->Delete();
  m_DescriptorHistorySnapshot.clear();
}

void WrappedMTLDevice::SnapshotDescriptorHistory()
{
  SCOPED_LOCK(m_DescriptorMetadataLock);
  ClearDescriptorHistorySnapshot();
  m_CaptureRetiredDescriptorBackings.clear();
  rdcarray<DescriptorHistoryChunk> live;
  for(const DescriptorHistoryChunk &entry : m_DescriptorHistory)
  {
    if(!GetResourceManager()->HasResource(entry.buffer))
    {
      entry.chunk->Delete();
      continue;
    }
    live.push_back(entry);
    auto object = GetResourceManager()->GetResource(entry.buffer, true);
    if(m_DescriptorCoverage >= 25 && object &&
       Atomic::CmpExch32(&object->m_CapturedAliasable, 0, 0))
    {
      bool referenced = false;
      for(const auto &slot : m_CapturedLiveDescriptorSlots)
      {
        referenced |= slot.first == entry.buffer;
        auto sources = m_CapturedDescriptorSlotSources.find(slot);
        if(sources != m_CapturedDescriptorSlotSources.end())
          for(const auto &source : sources->second) referenced |= source.second == entry.buffer;
      }
      // A prior frame's explicitly retired backing has no live logical descriptor
      // to seed. Keeping its history must not resurrect its creation over a live
      // placement allocation. Preserve history for a later slot generation.
      if(!referenced)
      {
        m_CaptureRetiredDescriptorBackings.insert(entry.buffer);
        continue;
      }
    }
    m_DescriptorHistorySnapshot.push_back(entry.chunk->Duplicate());
    GetResourceManager()->MarkResourceFrameReferenced(entry.buffer, eFrameRef_Read);
  }
  m_DescriptorHistory.swap(live);
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareDescriptorTable(SerialiserType &ser, ResourceId buffer,
    uint64_t offset, uint64_t count, uint64_t stride, uint32_t schema)
{
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_ELEMENT(stride).Important();
  SERIALISE_ELEMENT(schema).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    WrappedMTLObject *object = GetResourceManager()->GetResource(buffer, true);
    if(!object || object->m_Type != eResBuffer || !object->m_Real ||
       !ValidDescriptorLayout(offset, count, stride, schema))
      return false;
    MTL::Buffer *real = Unwrap((WrappedMTLBuffer *)object);
    if(real->storageMode() != MTL::StorageModeShared || offset > real->length() ||
       count * stride > real->length() - offset)
      return false;
  }
  return true;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareDescriptorCoverage(SerialiserType &ser, uint32_t version)
{
  SERIALISE_ELEMENT(version).Important();
  SERIALISE_CHECK_READ_ERRORS();
  return IsStructuredExporting(m_State) || (version >= 1 && version <= 66);
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareDescriptorGPUWrites(SerialiserType &ser, ResourceId buffer)
{
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_CHECK_READ_ERRORS();
  return IsStructuredExporting(m_State) || m_DescriptorGPUWrittenBuffers.count(buffer) != 0;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DescriptorCPUWrite(SerialiserType &ser, ResourceId buffer,
                                                    uint64_t start, bytebuf data)
{
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(start).Important();
  SERIALISE_ELEMENT(data);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    WrappedMTLObject *object = GetResourceManager()->GetResource(buffer, true);
    if(m_DescriptorCoverage < 2 || !m_DescriptorGPUWrittenBuffers.count(buffer) ||
       !ValidDescriptorCPUWrite(buffer, start, data.size()) || !object ||
       object->m_Type != eResBuffer || !object->m_Real)
      return false;
    return ReplayCPUBufferUpdate((WrappedMTLBuffer *)object, start, data);
  }
  return true;
}

bool WrappedMTLDevice::ValidDescriptorCPUWrite(ResourceId buffer, uint64_t start, uint64_t size) const
{
  if(!size || start > ~0ULL - size)
    return false;
  // GPU-owned allocations require complete CPU-written entries. Partial byte diffs could
  // otherwise merge with stale capture bytes after a GPU update to the same entry.
  for(const DescriptorTable &layout : m_DescriptorTables)
    if(layout.buffer == buffer && start >= layout.offset &&
       start - layout.offset < layout.count * layout.stride &&
       size <= layout.count * layout.stride - (start - layout.offset) &&
       (start - layout.offset) % layout.stride == 0 && size % layout.stride == 0)
      return true;
  return false;
}

template bool WrappedMTLDevice::Serialise_DeclareDescriptorGPUWrites(ReadSerialiser &, ResourceId);
template bool WrappedMTLDevice::Serialise_DeclareDescriptorGPUWrites(WriteSerialiser &, ResourceId);
template bool WrappedMTLDevice::Serialise_DescriptorCPUWrite(ReadSerialiser &, ResourceId,
                                                            uint64_t, bytebuf);
template bool WrappedMTLDevice::Serialise_DescriptorCPUWrite(WriteSerialiser &, ResourceId,
                                                            uint64_t, bytebuf);

template bool WrappedMTLDevice::Serialise_DeclareDescriptorTable(ReadSerialiser &, ResourceId,
    uint64_t, uint64_t, uint64_t, uint32_t);
template bool WrappedMTLDevice::Serialise_DeclareDescriptorTable(WriteSerialiser &, ResourceId,
    uint64_t, uint64_t, uint64_t, uint32_t);
template bool WrappedMTLDevice::Serialise_DeclareDescriptorCoverage(ReadSerialiser &, uint32_t);
template bool WrappedMTLDevice::Serialise_DeclareDescriptorCoverage(WriteSerialiser &, uint32_t);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DescriptorSlotEvent(SerialiserType &ser, ResourceId buffer,
    uint64_t offset, uint64_t generation, uint32_t event, uint32_t descriptorType, bytebuf data)
{
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(generation).Important();
  SERIALISE_ELEMENT(event).Important();
  SERIALISE_ELEMENT(descriptorType).Important();
  SERIALISE_ELEMENT(data);
  SERIALISE_CHECK_READ_ERRORS();
  // CPU export is supported. Normal replay is refused by ScanDescriptorMetadata until
  // allocation generations, temporal slices and all shader stages have a complete contract.
  return !IsReplayingAndReading() || IsStructuredExporting(m_State) ||
      (m_DescriptorCoverage >= 4 && ReadDescriptorSlotEvent(buffer, offset, generation, event, descriptorType, data));
}

template bool WrappedMTLDevice::Serialise_DescriptorSlotEvent(ReadSerialiser &, ResourceId,
    uint64_t, uint64_t, uint32_t, uint32_t, bytebuf);
template bool WrappedMTLDevice::Serialise_DescriptorSlotEvent(WriteSerialiser &, ResourceId,
    uint64_t, uint64_t, uint32_t, uint32_t, bytebuf);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DescriptorSlotBinding(SerialiserType &ser, ResourceId buffer,
    uint64_t offset, ResourceId resource, uint32_t kind, uint64_t memberOffset)
{
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(resource).Important();
  SERIALISE_ELEMENT(kind).Important();
  SERIALISE_ELEMENT(memberOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  return !IsReplayingAndReading() || IsStructuredExporting(m_State) ||
      (m_DescriptorCoverage >= 4 && ReadDescriptorSlotBinding(buffer, offset, resource, kind, memberOffset));
}

template bool WrappedMTLDevice::Serialise_DescriptorSlotBinding(ReadSerialiser &, ResourceId,
    uint64_t, ResourceId, uint32_t, uint64_t);
template bool WrappedMTLDevice::Serialise_DescriptorSlotBinding(WriteSerialiser &, ResourceId,
    uint64_t, ResourceId, uint32_t, uint64_t);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DescriptorInlineLayout(SerialiserType &ser, ResourceId encoder,
    uint32_t stage, uint32_t index, uint64_t count, uint64_t stride)
{
  SERIALISE_ELEMENT(encoder).Important();
  SERIALISE_ELEMENT(stage).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_ELEMENT(stride).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && m_DescriptorCoverage >= 4)
  {
    const auto key = make_rdcpair(encoder, uint64_t(stage) << 32 | index);
    if(!m_DescriptorShadowFrame || (stage != 0 && (m_DescriptorCoverage < 9 || stage > (m_DescriptorCoverage >= 66 ? 4U : 2U))) || index >= 31 ||
       (count ? (!ValidDescriptorLayout(0, count, stride, 0) || count * stride > 4096) :
        (m_DescriptorCoverage < 14 || !ValidInlineDrawConstants(stage, index, stride))) ||
       m_DescriptorInlineShadow.count(key))
      return false;
    m_DescriptorInlineShadow[key] = {count, stride, {}};
    return true;
  }
  return !IsReplayingAndReading() || IsStructuredExporting(m_State);
}

template bool WrappedMTLDevice::Serialise_DescriptorInlineLayout(ReadSerialiser &, ResourceId,
    uint32_t, uint32_t, uint64_t, uint64_t);
template bool WrappedMTLDevice::Serialise_DescriptorInlineLayout(WriteSerialiser &, ResourceId,
    uint32_t, uint32_t, uint64_t, uint64_t);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DescriptorInlineBinding(SerialiserType &ser, ResourceId encoder,
    uint32_t stage, uint32_t index, uint64_t entry, ResourceId resource, uint64_t memberOffset)
{
  SERIALISE_ELEMENT(encoder).Important();
  SERIALISE_ELEMENT(stage).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_ELEMENT(entry).Important();
  SERIALISE_ELEMENT(resource).Important();
  SERIALISE_ELEMENT(memberOffset).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && m_DescriptorCoverage >= 4)
  {
    const auto key = make_rdcpair(encoder, uint64_t(stage) << 32 | index);
    auto layout = m_DescriptorInlineShadow.find(key);
    if((stage != 0 && (m_DescriptorCoverage < 9 || stage > (m_DescriptorCoverage >= 66 ? 4U : 2U))) || layout == m_DescriptorInlineShadow.end() || entry >= layout->second.count ||
       resource == ResourceId() || layout->second.sources.count(entry))
      return false;
    layout->second.sources[entry] = {resource, memberOffset};
    return true;
  }
  return !IsReplayingAndReading() || IsStructuredExporting(m_State);
}

template bool WrappedMTLDevice::Serialise_DescriptorInlineBinding(ReadSerialiser &, ResourceId,
    uint32_t, uint32_t, uint64_t, ResourceId, uint64_t);
template bool WrappedMTLDevice::Serialise_DescriptorInlineBinding(WriteSerialiser &, ResourceId,
    uint32_t, uint32_t, uint64_t, ResourceId, uint64_t);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DeclareDescriptorBytes(SerialiserType &ser, ResourceId encoder,
    uint64_t index, uint64_t offset, uint64_t count, uint64_t stride)
{
  SERIALISE_ELEMENT(encoder).Important();
  SERIALISE_ELEMENT(index).Important();
  SERIALISE_ELEMENT(offset).Important();
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_ELEMENT(stride).Important();
  SERIALISE_CHECK_READ_ERRORS();
  return IsStructuredExporting(m_State) || (m_DescriptorCoverage == 3 &&
      encoder != ResourceId() && index < 31 && ValidDescriptorLayout(offset, count, stride, 0) &&
      offset <= 4096 && count * stride <= 4096 - offset);
}

template bool WrappedMTLDevice::Serialise_DeclareDescriptorBytes(ReadSerialiser &, ResourceId,
    uint64_t, uint64_t, uint64_t, uint64_t);
template bool WrappedMTLDevice::Serialise_DeclareDescriptorBytes(WriteSerialiser &, ResourceId,
    uint64_t, uint64_t, uint64_t, uint64_t);

bool WrappedMTLDevice::RelocateDescriptorBytes(ResourceId encoder, uint64_t index, rdcarray<byte> &data)
{
  if(m_DescriptorCoverage >= 4)
    return RelocateDescriptorInlineShadow(encoder, 0, index, data);
  auto declarations = m_DescriptorBytes.find(encoder);
  if(declarations == m_DescriptorBytes.end() || !declarations->second.count(index))
    return m_DescriptorTables.empty();
  const DescriptorTable &layout = declarations->second[index];
  if(data.size() > 4096 || layout.offset > data.size() ||
     layout.count * layout.stride > data.size() - layout.offset)
    return false;
  for(uint64_t entry = 0; entry < layout.count; entry++)
  {
    const uint64_t offset = layout.offset + entry * layout.stride;
    uint64_t captured = 0, replacement = 0;
    memcpy(&captured, data.data() + offset, 8);
    if(!captured)
      continue;
    uint32_t matches = 0;
    for(const auto &identity : m_ReplayGPUIdentities)
    {
      if(identity.second.kind != 0 || captured < identity.second.value)
        continue;
      uint64_t length = 0;
      WrappedMTLObject *object = NULL;
      auto future = m_DescriptorFrameBuffers.find(identity.first);
      if(m_DescriptorPreflight && future != m_DescriptorFrameBuffers.end())
      {
        if(!m_DescriptorPreflightLiveBuffers.count(identity.first))
          continue;
        length = future->second.length;
      }
      else
      {
        object = GetResourceManager()->GetResource(identity.first, true);
        if(!object || object->m_Type != eResBuffer || !object->m_Real)
          continue;
        length = Unwrap((WrappedMTLBuffer *)object)->length();
      }
      const uint64_t memberOffset = captured - identity.second.value;
      if(memberOffset >= length)
        continue;
      if(matches++)
        return false;
      if(!m_DescriptorPreflight)
        replacement = Unwrap((WrappedMTLBuffer *)object)->gpuAddress() + memberOffset;
    }
    if(matches != 1 || (!m_DescriptorPreflight && !replacement))
      return false;
    if(!m_DescriptorPreflight)
      memcpy(data.data() + offset, &replacement, 8);
  }
  return true;
}

RDResult WrappedMTLDevice::ScanDescriptorMetadata(RDCFile *rdc, int section, uint32_t diagnosticCoverage)
{
  if(IsStructuredExporting(m_State))
    return ResultCode::Succeeded;
  // Only ReadLogInitialisation's mandatory-exit CPU diagnostic path can supply
  // this candidate version. It does not authorize loading or GPU replay.
  if(diagnosticCoverage) m_DescriptorCoverage=diagnosticCoverage;
  bool declaredCoverage=false;
  ReadSerialiser scan(rdc->ReadSection(section), Ownership::Stream);
  scan.SetVersion(m_SectionVersion);
  m_CapturedComputeIndirectArguments.clear();
  m_CapturedComputeIndirectArgumentsCount = 0;
  m_HasCapturedComputeIndirectArguments = false;
  std::map<ResourceId, ResourceId> indirectEncoderCommands;
  std::map<ResourceId, uint32_t> indirectOrdinals;
  uint32_t matchedIndirectArguments = 0;
  m_CapturedRenderIndirectArguments.clear();
  m_HasCapturedRenderIndirectArguments=false;
  m_CapturedRenderIndirectArgumentsCount=0;
  std::map<ResourceId,ResourceId> renderIndirectCommands,renderIndirectPasses;
  std::map<ResourceId,uint32_t> renderIndirectOrdinals;
  uint32_t matchedRenderIndirectArguments=0;
  uint64_t renderIndirectWork=0;
  bool frame = false, identities = false, slotEvents = false, retirementPrefix = true;
  std::set<ResourceId> retirementCommands, retirementBlits;
  uint64_t retirementCopyBytes = 0;
  uint32_t retirementCopies = 0;
  m_DescriptorDrawableTextures.clear();
  m_DescriptorSubmissionInitialBuffers.clear();
  m_DescriptorPreludeRetirements.clear();
  m_DescriptorPreludeBufferCopies.clear();
  m_DescriptorPreludeBlitEncoders.clear();
  while(!scan.GetReader()->AtEnd() && !scan.IsErrored())
  {
    MetalChunk chunk = scan.ReadChunk<MetalChunk>();
    if((SystemChunk)chunk == SystemChunk::CaptureScope)
      frame = true;
    if(chunk == MetalChunk::MTLDevice_CaptureComputeIndirectArgumentsCount)
    {
      uint32_t count=0; scan.Serialise("count"_lit,count);
      if(frame || m_HasCapturedComputeIndirectArguments || count > 1024)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal capture indirect evidence header");
      m_HasCapturedComputeIndirectArguments=true; m_CapturedComputeIndirectArgumentsCount=count;
    }
    if(chunk == MetalChunk::MTLComputeCommandEncoder_CaptureIndirectArguments)
    {
      ResourceId command,encoder,buffer; uint32_t ordinal=0; uint64_t offset=0;
      rdcarray<uint32_t> groups;
      scan.Serialise("command"_lit,command); scan.Serialise("encoder"_lit,encoder);
      scan.Serialise("ordinal"_lit,ordinal); scan.Serialise("buffer"_lit,buffer);
      scan.Serialise("offset"_lit,offset); scan.Serialise("groups"_lit,groups);
      if(frame || !m_HasCapturedComputeIndirectArguments || command == ResourceId() ||
         encoder == ResourceId() || buffer == ResourceId() || (offset & 3) || groups.size()!=3 ||
         ordinal >= 1024 || !m_CapturedComputeIndirectArguments.insert(
             {make_rdcpair(encoder,ordinal),{command,buffer,offset,groups}}).second)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal captured per-use indirect arguments");
    }
    if(chunk==MetalChunk::MTLDevice_CaptureRenderIndirectArgumentsCount)
    {
      uint32_t count=0;scan.Serialise("count"_lit,count);
      if(frame || m_HasCapturedRenderIndirectArguments || count>512)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid Metal render indirect evidence header");
      m_HasCapturedRenderIndirectArguments=true;m_CapturedRenderIndirectArgumentsCount=count;
    }
    if(chunk==MetalChunk::MTLRenderCommandEncoder_CaptureIndirectArguments)
    {
      ResourceId command,encoder,pass,buffer;uint32_t ordinal=0,wordCount=0;uint64_t offset=0;
      bool writesDeclared=false;rdcarray<uint32_t> arguments;
      scan.Serialise("command"_lit,command);scan.Serialise("encoder"_lit,encoder);scan.Serialise("pass"_lit,pass);
      scan.Serialise("ordinal"_lit,ordinal);scan.Serialise("buffer"_lit,buffer);scan.Serialise("offset"_lit,offset);
      scan.Serialise("wordCount"_lit,wordCount);scan.Serialise("writesDeclared"_lit,writesDeclared);
      scan.Serialise("arguments"_lit,arguments);
      if(frame || !m_HasCapturedRenderIndirectArguments || command==ResourceId() || encoder==ResourceId() ||
         pass==ResourceId() || buffer==ResourceId() || (offset&3) || (wordCount!=4 && wordCount!=5) ||
         arguments.size()!=wordCount || !writesDeclared || ordinal>=512 ||
         !m_CapturedRenderIndirectArguments.insert({make_rdcpair(encoder,ordinal),
             {command,pass,buffer,offset,wordCount,arguments}}).second)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid Metal immutable render indirect evidence");
      const uint64_t work=uint64_t(arguments[0])*arguments[1];
      if(arguments[0]>1048576 || arguments[1]>1048576 || work>1048576 ||
         uint64_t(arguments[2])+arguments[0]>UINT32_MAX ||
         uint64_t(arguments[wordCount-1])+arguments[1]>UINT32_MAX ||
         renderIndirectWork>8388608-work)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Metal captured render indirect workload exceeds bounded replay");
      renderIndirectWork+=work;
    }
    if(frame && m_HasCapturedRenderIndirectArguments)
    {
      const uint64_t evidencePosition=scan.GetReader()->GetOffset();
      if(chunk==MetalChunk::MTLCommandBuffer_renderCommandEncoderWithDescriptor ||
         chunk==MetalChunk::MTLCommandBuffer_parallelRenderCommandEncoderWithDescriptor)
      {
        ResourceId command,encoder;scan.Serialise("CommandBuffer"_lit,command);
        if(chunk==MetalChunk::MTLCommandBuffer_renderCommandEncoderWithDescriptor)
          scan.Serialise("RenderCommandEncoder"_lit,encoder);
        else scan.Serialise("ParallelRenderCommandEncoder"_lit,encoder);
        if(!renderIndirectCommands.insert({encoder,command}).second)
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Duplicate Metal render indirect pass");
        renderIndirectPasses[encoder]=encoder;
      }
      if(chunk==MetalChunk::MTLParallelRenderCommandEncoder_renderCommandEncoder)
      {
        ResourceId parent,encoder;scan.Serialise("ParallelRenderCommandEncoder"_lit,parent);
        scan.Serialise("RenderCommandEncoder"_lit,encoder);
        if(!renderIndirectCommands.count(parent) ||
           !renderIndirectCommands.insert({encoder,renderIndirectCommands[parent]}).second)
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid Metal render indirect child");
        renderIndirectPasses[encoder]=parent;
      }
      if(chunk==MetalChunk::MTLRenderCommandEncoder_drawPrimitives_indirect ||
         chunk==MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_indirect)
      {
        const bool indexed=chunk==MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_indirect;
        ResourceId encoder,buffer;MTL::PrimitiveType primitive;uint64_t offset=0;
        scan.Serialise("RenderCommandEncoder"_lit,encoder);scan.Serialise("primitiveType"_lit,primitive);
        if(indexed) {
          MTL::IndexType type;ResourceId indexBuffer;uint64_t indexOffset=0;
          scan.Serialise("indexType"_lit,type);scan.Serialise("indexBuffer"_lit,indexBuffer);
          scan.Serialise("indexBufferOffset"_lit,indexOffset);
        }
        scan.Serialise("indirectBuffer"_lit,buffer);scan.Serialise("indirectBufferOffset"_lit,offset);
        const auto found=m_CapturedRenderIndirectArguments.find(make_rdcpair(encoder,renderIndirectOrdinals[encoder]++));
        if(found==m_CapturedRenderIndirectArguments.end() || !renderIndirectCommands.count(encoder) ||
           found->second.command!=renderIndirectCommands[encoder] || found->second.pass!=renderIndirectPasses[encoder] ||
           found->second.buffer!=buffer || found->second.offset!=offset || found->second.wordCount!=(indexed?5U:4U))
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Metal render indirect evidence does not match original draw");
        matchedRenderIndirectArguments++;
      }
      scan.GetReader()->SetOffset(evidencePosition);
    }
    if(frame && m_HasCapturedComputeIndirectArguments)
    {
      const uint64_t evidencePosition=scan.GetReader()->GetOffset();
      if(chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoder ||
         chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDispatchType ||
         chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDescriptor)
      {
        ResourceId command,encoder;
        scan.Serialise("CommandBuffer"_lit,command); scan.Serialise("ComputeCommandEncoder"_lit,encoder);
        if(!indirectEncoderCommands.insert({encoder,command}).second)
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Duplicate captured Metal indirect encoder");
      }
      if(chunk == MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups_indirect)
      {
        ResourceId encoder,buffer; uint64_t offset=0;
        scan.Serialise("ComputeCommandEncoder"_lit,encoder); scan.Serialise("indirectBuffer"_lit,buffer);
        scan.Serialise("indirectBufferOffset"_lit,offset);
        const auto found=m_CapturedComputeIndirectArguments.find(make_rdcpair(encoder,indirectOrdinals[encoder]++));
        if(found == m_CapturedComputeIndirectArguments.end() ||
           !indirectEncoderCommands.count(encoder) || found->second.command != indirectEncoderCommands[encoder] ||
           found->second.buffer != buffer || found->second.offset != offset)
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Missing or mismatched Metal per-use indirect evidence");
        matchedIndirectArguments++;
      }
      scan.GetReader()->SetOffset(evidencePosition);
    }
    if(frame && m_DescriptorCoverage >= 16 && retirementPrefix &&
       chunk != MetalChunk::MTLResource_setLabel && // Metadata does not consume descriptor generations.
       (SystemChunk)chunk != SystemChunk::CaptureScope && (SystemChunk)chunk != SystemChunk::CaptureBegin)
    {
      if(chunk == MetalChunk::MTLBuffer_DescriptorSlotEvent)
      {
        ResourceId buffer; uint64_t offset = 0, generation = 0;
        uint32_t event = 0, type = 0; bytebuf data;
        scan.Serialise("buffer"_lit, buffer); scan.Serialise("offset"_lit, offset);
        scan.Serialise("generation"_lit, generation); scan.Serialise("event"_lit, event);
        scan.Serialise("descriptorType"_lit, type); scan.Serialise("data"_lit, data);
        retirementPrefix = event == 1 || (m_DescriptorCoverage >= 29 && (event == 0 || event == 2));
        if(event == 1)
        {
          DescriptorSlotShadow retirement;
          retirement.generation = generation; retirement.type = type;
          if(buffer == ResourceId() || offset % 8 || !generation || type > 7 || !data.empty() ||
             m_DescriptorPreludeRetirements.size() >= (m_DescriptorCoverage >= 30 ? 256U : 64U) ||
             !m_DescriptorPreludeRetirements.insert({{buffer, offset}, retirement}).second)
            RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid leading Metal descriptor retirement");
        }
      }
      else if(m_DescriptorCoverage >= 30 &&
              (chunk == MetalChunk::MTLCommandQueue_commandBuffer ||
               chunk == MetalChunk::MTLCommandQueue_commandBufferWithDescriptor ||
               chunk == MetalChunk::MTLCommandQueue_commandBufferWithUnretainedReferences))
      {
        ResourceId queue, command;
        scan.Serialise("CommandQueue"_lit, queue); scan.Serialise("CommandBuffer"_lit, command);
        if(queue == ResourceId() || command == ResourceId() || retirementCommands.size() >= 8 ||
           !retirementCommands.insert(command).second)
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid unsubmitted retirement command buffer");
      }
      else if(m_DescriptorCoverage >= 30 &&
              (chunk == MetalChunk::MTLCommandBuffer_blitCommandEncoder ||
               chunk == MetalChunk::MTLCommandBuffer_blitCommandEncoderWithDescriptor))
      {
        ResourceId command, encoder;
        scan.Serialise("CommandBuffer"_lit, command); scan.Serialise("BlitCommandEncoder"_lit, encoder);
        if(!retirementCommands.count(command) || encoder == ResourceId() ||
           retirementBlits.size() >= 8 || !retirementBlits.insert(encoder).second)
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid unsubmitted retirement buffer blit");
        m_DescriptorPreludeBlitEncoders.insert(encoder);
      }
      else if(m_DescriptorCoverage >= 30 && chunk == MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toBuffer)
      {
        ResourceId encoder, source, destination; uint64_t sourceOffset = 0, destinationOffset = 0, size = 0;
        scan.Serialise("BlitCommandEncoder"_lit, encoder); scan.Serialise("sourceBuffer"_lit, source);
        scan.Serialise("sourceOffset"_lit, sourceOffset); scan.Serialise("destinationBuffer"_lit, destination);
        scan.Serialise("destinationOffset"_lit, destinationOffset); scan.Serialise("size"_lit, size);
        auto src = m_DescriptorFrameBuffers.find(source), dst = m_DescriptorFrameBuffers.find(destination);
        // A static staging upload is outside the proven frame-to-frame retirement
        // prefix. Stop that proof; ordinary sourced copy preflight validates it.
        if(m_DescriptorCoverage >= 40 &&
           (src == m_DescriptorFrameBuffers.end() || dst == m_DescriptorFrameBuffers.end()))
          retirementPrefix = false;
        else
        {
          // These copies carry bytes, without dereferencing descriptor fields. Both objects must
          // be known frame-born buffers; ordinary preflight still validates their live aliases.
          if(!retirementBlits.count(encoder) || source == destination ||
             src == m_DescriptorFrameBuffers.end() || dst == m_DescriptorFrameBuffers.end() ||
             !size || sourceOffset > src->second.length || size > src->second.length - sourceOffset ||
             destinationOffset > dst->second.length || size > dst->second.length - destinationOffset ||
             retirementCopies >= 16 || size > 64 * 1024 - retirementCopyBytes)
            RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid unsubmitted retirement buffer copy");
          retirementCopies++; retirementCopyBytes += size;
          m_DescriptorPreludeBufferCopies.push_back({source, destination, sourceOffset, destinationOffset, size});
        }
      }
      else if(m_DescriptorCoverage >= 30 &&
              (chunk == MetalChunk::MTLBlitCommandEncoder_setLabel ||
               chunk == MetalChunk::MTLBlitCommandEncoder_endEncoding))
      {
        ResourceId encoder; scan.Serialise("BlitCommandEncoder"_lit, encoder);
        if(!retirementBlits.count(encoder))
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid retirement blit encoder reference");
        if(chunk == MetalChunk::MTLBlitCommandEncoder_endEncoding) retirementBlits.erase(encoder);
      }
      else
      {
        // Vulkan allows stale descriptors when no command consumes them. v29 admits only
        // CPU buffer births/diagnostics interspersed with exact-generation frees. Stop at
        // every unproved operation. v30 additionally permits only recorded unsubmitted
        // buffer blits; v31 admits validated CPU texture/view births.
        retirementPrefix = m_DescriptorCoverage >= 29 &&
            (chunk == MetalChunk::MTLDevice_newBufferWithLength ||
             chunk == MetalChunk::MTLDevice_newBufferWithBytes ||
             chunk == MetalChunk::MTLHeap_newBuffer ||
             chunk == MetalChunk::MTLHeap_newBufferWithOffset ||
             chunk == MetalChunk::MTLResource_CaptureGPUIdentity ||
             chunk == MetalChunk::MTLBuffer_DescriptorSlotBinding ||
             chunk == MetalChunk::MTLBuffer_DeclareDescriptorTable ||
             chunk == MetalChunk::MTLBuffer_DeclareDescriptorGPUWrites ||
             (m_DescriptorCoverage >= 31 &&
              (chunk == MetalChunk::MTLBuffer_newTextureWithDescriptor ||
               chunk == MetalChunk::MTLHeap_newTextureWithOffset)));
      }
    }
    if(frame && m_DescriptorCoverage >= 8 &&
       (chunk == MetalChunk::MTLDevice_newBufferWithLength ||
        chunk == MetalChunk::MTLDevice_newBufferWithBytes))
    {
      ResourceId buffer; bytebuf initial; uint64_t length = 0, options = 0;
      scan.Serialise("Buffer"_lit, buffer); scan.Serialise("initialData"_lit, initial);
      scan.Serialise("length"_lit, length); scan.Serialise("options"_lit, options);
      if(buffer == ResourceId() || !length || length > DescriptorFrameBufferLimit(false, m_DescriptorCoverage) ||
         (!initial.empty() && initial.size() != length) ||
         (options != MTL::ResourceStorageModeShared &&
          options != (MTL::ResourceStorageModeShared | MTL::ResourceHazardTrackingModeTracked)) ||
         !m_DescriptorFrameBuffers.insert({buffer, {ResourceId(), length, options, 0}}).second)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid frame sourced Metal buffer creation");
    }
    if(chunk == MetalChunk::MTLBuffer_DescriptorSlotEvent ||
       chunk == MetalChunk::MTLBuffer_DescriptorSlotBinding ||
       chunk == MetalChunk::MTLCommandEncoder_DescriptorInlineLayout ||
       chunk == MetalChunk::MTLCommandEncoder_DescriptorInlineBinding)
      slotEvents = true;
    if(frame && (m_DescriptorCoverage == 3 || m_DescriptorCoverage >= 11) && chunk == MetalChunk::MTLHeap_newBufferWithOffset)
    {
      ResourceId heap, buffer;
      uint64_t length = 0, options = 0, offset = 0;
      scan.Serialise("Heap"_lit, heap);
      scan.Serialise("Buffer"_lit, buffer);
      scan.Serialise("length"_lit, length);
      scan.Serialise("options"_lit, options);
      scan.Serialise("offset"_lit, offset);
      if(heap == ResourceId() || buffer == ResourceId() || !length ||
         (m_DescriptorCoverage >= 11 && (length > DescriptorFrameBufferLimit(true, m_DescriptorCoverage) ||
           (options != MTL::ResourceStorageModeShared &&
            options != (MTL::ResourceStorageModeShared | MTL::ResourceHazardTrackingModeTracked) &&
            !(m_DescriptorCoverage >= 23 &&
              options == (MTL::ResourceStorageModePrivate | MTL::ResourceHazardTrackingModeTracked))))) ||
         length > 128 * 1024 * 1024ULL ||
         (options != MTL::ResourceStorageModeShared &&
          options != (MTL::ResourceStorageModeShared | MTL::ResourceHazardTrackingModeTracked) &&
          options != MTL::ResourceStorageModePrivate &&
          options != (MTL::ResourceStorageModePrivate | MTL::ResourceHazardTrackingModeTracked)) ||
         !m_DescriptorFrameBuffers.insert({buffer, {heap, length, options, offset}}).second)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid frame Metal descriptor buffer creation");
    }
    if(frame && (m_DescriptorCoverage == 3 || m_DescriptorCoverage >= 31) &&
       chunk == MetalChunk::MTLBuffer_newTextureWithDescriptor)
    {
      ResourceId buffer, texture;
      scan.Serialise("Buffer"_lit, buffer);
      scan.Serialise("Texture"_lit, texture);
      if(buffer == ResourceId() || texture == ResourceId() ||
         !m_DescriptorFrameViews.insert({texture, buffer}).second)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid frame Metal descriptor texture view creation");
    }
    if(frame && m_DescriptorCoverage >= 21 && chunk == MetalChunk::MTLHeap_newTextureWithOffset)
    {
      ResourceId heap, texture; RDMTL::TextureDescriptor descriptor; uint64_t offset = 0;
      scan.Serialise("Heap"_lit, heap); scan.Serialise("Texture"_lit, texture);
      scan.Serialise("descriptor"_lit, descriptor); scan.Serialise("offset"_lit, offset);
      if(heap == ResourceId() || texture == ResourceId() || !ValidDescriptorFrameTexture(descriptor, m_DescriptorCoverage) ||
         !m_DescriptorFrameTextures.insert({texture, {heap, descriptor, offset}}).second)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
            "Invalid frame sourced Metal texture creation: texture %s heap %s, %s %s %llux%llux%llu, mips %llu samples %llu array %llu usage %llu",
            ToStr(texture).c_str(), ToStr(heap).c_str(), ToStr(descriptor.textureType).c_str(),
            ToStr(descriptor.pixelFormat).c_str(), (unsigned long long)descriptor.width,
            (unsigned long long)descriptor.height, (unsigned long long)descriptor.depth,
            (unsigned long long)descriptor.mipmapLevelCount, (unsigned long long)descriptor.sampleCount,
            (unsigned long long)descriptor.arrayLength, (unsigned long long)descriptor.usage);
    }
    if(frame && m_DescriptorCoverage >= 26 &&
       chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset)
    {
      ResourceId source, view; MTL::PixelFormat format; MTL::TextureType type;
      NS::Range levels(0, 0), slices(0, 0); MTL::TextureSwizzleChannels swizzle;
      scan.Serialise("Source"_lit, source); scan.Serialise("View"_lit, view);
      scan.Serialise("format"_lit, format); scan.Serialise("type"_lit, type);
      scan.Serialise("levels"_lit, levels); scan.Serialise("slices"_lit, slices);
      scan.Serialise("swizzle"_lit, swizzle);
      auto parent = m_DescriptorFrameTextures.find(source);
      if(view == ResourceId() || source == view || parent == m_DescriptorFrameTextures.end() ||
         m_DescriptorFrameTextureViewParents.count(source) ||
         format != parent->second.descriptor.pixelFormat || type != MTL::TextureType2D ||
         levels.location || levels.length != 1 || slices.location || slices.length != 1 ||
         swizzle.red != MTL::TextureSwizzleRed || swizzle.green != MTL::TextureSwizzleGreen ||
         swizzle.blue != MTL::TextureSwizzleBlue || swizzle.alpha != MTL::TextureSwizzleAlpha ||
         !m_DescriptorFrameTextureViewParents.insert({view, source}).second ||
         !m_DescriptorFrameTextures.insert({view, parent->second}).second)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid frame sourced Metal texture view");
    }
    if(chunk == MetalChunk::MTLComputeCommandEncoder_DeclareDescriptorBytes)
    {
      ResourceId encoder;
      uint64_t index = 0, offset = 0, count = 0, stride = 0;
      scan.Serialise("encoder"_lit, encoder); scan.Serialise("index"_lit, index);
      scan.Serialise("offset"_lit, offset); scan.Serialise("count"_lit, count);
      scan.Serialise("stride"_lit, stride);
      if(!frame || m_DescriptorCoverage != 3 || encoder == ResourceId() || index >= 31 ||
         !ValidDescriptorLayout(offset, count, stride, 0) || offset > 4096 || count * stride > 4096 - offset ||
         m_DescriptorBytes[encoder].count(index))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal inline descriptor declaration");
      m_DescriptorBytes[encoder][index] = {encoder, offset, count, stride, 0};
    }
    if(chunk == MetalChunk::MTLResource_CaptureGPUIdentity)
    {
      ResourceId resource;
      uint32_t kind = 0;
      uint64_t value = 0;
      scan.Serialise("resource"_lit, resource);
      scan.Serialise("kind"_lit, kind);
      scan.Serialise("value"_lit, value);
      if(resource == ResourceId() || kind > 2 || !value)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
            "Metal descriptor relocation is unsupported for frame-born or invalid GPU identity");
      auto old = m_ReplayGPUIdentities.find(resource);
      if(old != m_ReplayGPUIdentities.end() && (old->second.kind != kind || old->second.value != value))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Conflicting Metal GPU identity metadata");
      // A first query during capture adds record metadata as well as a frame copy.
      // Identical pre-frame identities are duplicates. v3 accepts a new buffer/view identity
      // only after a supported placement or buffer-view creation; other births stay unsupported.
      if(frame && old == m_ReplayGPUIdentities.end() &&
         ((m_DescriptorCoverage != 3 && m_DescriptorCoverage < 8) ||
          !((kind == 0 && m_DescriptorFrameBuffers.count(resource)) ||
            (kind == 1 && (m_DescriptorFrameViews.count(resource) || m_DescriptorFrameTextures.count(resource))))))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
            "Metal descriptor relocation is unsupported for frame-born or invalid GPU identity");
      m_ReplayGPUIdentities[resource] = {kind, value};
      identities = true;
    }
    if(chunk == MetalChunk::MTLDevice_DeclareDescriptorCoverage)
    {
      uint32_t version = 0;
      scan.Serialise("version"_lit, version);
      if(frame || (version < 1 || version > 66) || declaredCoverage ||
         (m_DescriptorCoverage && !diagnosticCoverage))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal descriptor coverage declaration");
      declaredCoverage=true;
      if(!diagnosticCoverage) m_DescriptorCoverage = version;
    }
    if(chunk == MetalChunk::MTLBuffer_DeclareDescriptorGPUWrites)
    {
      ResourceId buffer;
      scan.Serialise("buffer"_lit, buffer);
      if(frame || buffer == ResourceId() || !m_DescriptorGPUWrittenBuffers.insert(buffer).second)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal descriptor GPU-write declaration");
    }
    if(chunk == MetalChunk::MTLBuffer_DescriptorCPUWrite && !frame)
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Descriptor CPU write outside frame");
    if(chunk == MetalChunk::MTLBuffer_DeclareDescriptorTable)
    {
      DescriptorTable layout;
      scan.Serialise("buffer"_lit, layout.buffer);
      scan.Serialise("offset"_lit, layout.offset);
      scan.Serialise("count"_lit, layout.count);
      scan.Serialise("stride"_lit, layout.stride);
      scan.Serialise("schema"_lit, layout.schema);
      if((frame && (m_DescriptorCoverage < 8 || !m_DescriptorFrameBuffers.count(layout.buffer))) ||
         layout.buffer == ResourceId() ||
         !ValidDescriptorLayout(layout.offset, layout.count, layout.stride, layout.schema))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal descriptor table layout");
      for(const DescriptorTable &other : m_DescriptorTables)
        if(other.buffer == layout.buffer && layout.offset < other.offset + other.count * other.stride &&
           other.offset < layout.offset + layout.count * layout.stride)
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Overlapping Metal descriptor tables");
      m_DescriptorTables.push_back(layout);
    }
    scan.EndChunk();
  }
  if(m_HasCapturedRenderIndirectArguments &&
     (matchedRenderIndirectArguments!=m_CapturedRenderIndirectArgumentsCount ||
      m_CapturedRenderIndirectArguments.size()!=m_CapturedRenderIndirectArgumentsCount))
    RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Incomplete Metal render indirect evidence");
  if(m_HasCapturedComputeIndirectArguments &&
     (m_CapturedComputeIndirectArguments.size() != m_CapturedComputeIndirectArgumentsCount ||
      matchedIndirectArguments != m_CapturedComputeIndirectArgumentsCount))
    RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Incomplete Metal per-use indirect capture evidence");
  if(scan.IsErrored())
    return RDResult(ResultCode::APIDataCorrupted, scan.GetError().message);
  for(const auto &copy : m_DescriptorPreludeBufferCopies)
    for(const auto &table : m_DescriptorTables)
      if(copy.source == table.buffer || copy.destination == table.buffer)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Retirement buffer copy overlaps a descriptor backing");
  if(m_DescriptorCoverage >= 4)
  {
    uint64_t sourcedAllocationBytes = 0;
    MetalReplayAllocationBudget nativeBudget;
    bool sourcedBudgetInvalid = false;
    std::set<ResourceId> budgetHeaps;
    std::map<ResourceId, uint64_t> largeBackgroundBuffers, bufferInitialSizes;
    std::map<ResourceId, uint64_t> budgetBackgroundBuffers;
    std::map<ResourceId, uint64_t> sharedBackgroundBuffers;
    std::map<ResourceId, uint64_t> colorInitialTextures, textureInitialSizes;
    ReadSerialiser budget(rdc->ReadSection(section), Ownership::Stream);
    budget.SetVersion(m_SectionVersion);
    frame = false;
    while(!budget.GetReader()->AtEnd() && !budget.IsErrored())
    {
      const bool previouslyInvalid = sourcedBudgetInvalid;
      const uint64_t budgetOffset = budget.GetReader()->GetOffset();
      MetalChunk chunk = budget.ReadChunk<MetalChunk>();
      if((SystemChunk)chunk == SystemChunk::CaptureScope) frame = true;
      if(m_DescriptorCoverage >= 34 && (SystemChunk)chunk == SystemChunk::InitialContents)
      {
        MetalResourceType type; ResourceId id; bytebuf contents;
        budget.Serialise("type"_lit, type); budget.Serialise("id"_lit, id);
        if(type == eResBuffer || type == eResTexture)
          budget.Serialise("Contents"_lit, contents);
        if(m_DescriptorCoverage >= 60 && (type == eResBuffer || type == eResTexture))
          nativeBudget.Add(nativeBudget.initialBytes, contents.size());
        if(type == eResBuffer)
          sourcedBudgetInvalid |= frame || !bufferInitialSizes.insert({id, contents.size()}).second;
        if(type == eResTexture)
          sourcedBudgetInvalid |= frame || !textureInitialSizes.insert({id, contents.size()}).second;
      }
      if(chunk == MetalChunk::MTLDevice_newHeapWithDescriptor)
      {
        ResourceId heap; uint64_t size = 0;
        budget.Serialise("Heap"_lit, heap); budget.Serialise("size"_lit, size);
        const uint64_t heapLimit = m_DescriptorCoverage >= 60 ? 128ULL * 1024 * 1024 :
            m_DescriptorCoverage >= 43 ? 16ULL * 1024 * 1024 : 1024 * 1024;
        sourcedBudgetInvalid |= frame || !size || size > heapLimit;
        sourcedAllocationBytes += RDCMIN(size, heapLimit + 1);
        if(m_DescriptorCoverage >= 60)
        {
          sourcedBudgetInvalid |= heap == ResourceId() || size < 4096 || !budgetHeaps.insert(heap).second;
          nativeBudget.Add(nativeBudget.nativeBytes, AlignUp(RDCMIN(size, heapLimit + 1), uint64_t(64*1024)));
        }
      }
      if(chunk == MetalChunk::MTLDevice_newBufferWithLength ||
         chunk == MetalChunk::MTLDevice_newBufferWithBytes ||
         chunk == MetalChunk::MTLHeap_newBuffer ||
         chunk == MetalChunk::MTLHeap_newBufferWithOffset)
      {
        ResourceId heap, buffer; uint64_t length = 0;
        if(chunk == MetalChunk::MTLHeap_newBuffer || chunk == MetalChunk::MTLHeap_newBufferWithOffset)
          budget.Serialise("Heap"_lit, heap);
        budget.Serialise("Buffer"_lit, buffer);
        if(chunk == MetalChunk::MTLDevice_newBufferWithBytes || chunk == MetalChunk::MTLDevice_newBufferWithLength)
        {
          bytebuf initial; budget.Serialise("initialData"_lit, initial);
        }
        budget.Serialise("length"_lit, length);
        MTL::ResourceOptions options; budget.Serialise("options"_lit, options);
        if(!frame && m_DescriptorCoverage >= 65 && length <= 1024ULL * 1024 &&
           (options == MTL::ResourceStorageModeShared ||
            options == (MTL::ResourceStorageModeShared | MTL::ResourceHazardTrackingModeTracked)))
          sharedBackgroundBuffers[buffer] = length;
        sourcedBudgetInvalid |= (frame && (m_DescriptorCoverage < 8 ||
            !m_DescriptorFrameBuffers.count(buffer) ||
            (m_DescriptorFrameBuffers[buffer].heap != ResourceId() && m_DescriptorCoverage < 11))) ||
            !length || length > ((!frame && m_DescriptorCoverage >= 34) ?
                128ULL * 1024 * 1024 : DescriptorFrameBufferLimit(heap != ResourceId(), m_DescriptorCoverage));
        if(!frame && m_DescriptorCoverage >= 34 && length > 64 * 1024)
          sourcedBudgetInvalid |= !largeBackgroundBuffers.insert({buffer, length}).second;
        sourcedAllocationBytes += RDCMIN(length, uint64_t(128 * 1024 * 1024));
        if(m_DescriptorCoverage >= 60 && !frame)
          sourcedBudgetInvalid |= !budgetBackgroundBuffers.insert({buffer,length}).second;
        if(m_DescriptorCoverage >= 60 && heap == ResourceId())
        {
          const uint64_t allowed = MTL::ResourceCPUCacheModeWriteCombined | MTL::ResourceStorageModePrivate |
              MTL::ResourceStorageModeManaged | MTL::ResourceHazardTrackingModeTracked;
          const uint64_t storage = uint64_t(options) & MTL::ResourceStorageModeMemoryless;
          if(!length || length > 128ULL * 1024 * 1024 || (uint64_t(options) & ~allowed) ||
             storage == MTL::ResourceStorageModeMemoryless)
            sourcedBudgetInvalid = true;
          else
          {
            const auto allocation = Unwrap(this)->heapBufferSizeAndAlign(length, options);
            sourcedBudgetInvalid |= allocation.size < length || !allocation.align;
            nativeBudget.Add(nativeBudget.nativeBytes, allocation.size);
          }
        }
      }
      if(chunk == MetalChunk::MTLDevice_newTextureWithDescriptor ||
         chunk == MetalChunk::MTLDevice_newTextureWithDescriptor_nextDrawable)
      {
        ResourceId texture; RDMTL::TextureDescriptor descriptor;
        budget.Serialise("Texture"_lit, texture); budget.Serialise("descriptor"_lit, descriptor);
        if(chunk==MetalChunk::MTLDevice_newTextureWithDescriptor_nextDrawable)
          m_DescriptorDrawableTextures.insert(texture);
        const bool privateInitial = m_DescriptorCoverage >= 27 && !frame &&
            chunk == MetalChunk::MTLDevice_newTextureWithDescriptor &&
            descriptor.storageMode == MTL::StorageModePrivate &&
            descriptor.textureType == MTL::TextureType2D &&
            (descriptor.pixelFormat == MTL::PixelFormatRGBA8Unorm ||
             descriptor.pixelFormat == MTL::PixelFormatBGRA8Unorm);
        uint64_t colorSize = 0;
        const bool colorInitial = m_DescriptorCoverage >= 35 && !frame &&
            (chunk == MetalChunk::MTLDevice_newTextureWithDescriptor ||
             (m_DescriptorCoverage >= 37 && chunk == MetalChunk::MTLDevice_newTextureWithDescriptor_nextDrawable)) &&
            DescriptorTextureInitialSize(descriptor, colorSize, m_DescriptorCoverage >= 36, m_DescriptorCoverage >= 37);
        if(colorInitial)
        {
          // Current-frame drawables are represented by standalone replay images.
          // They need no initial data until read as a sourced descriptor; that
          // requirement is checked by PatchDescriptorSlotField before upload.
          if(chunk != MetalChunk::MTLDevice_newTextureWithDescriptor_nextDrawable)
            sourcedBudgetInvalid |= !colorInitialTextures.insert({texture, colorSize}).second;
          sourcedAllocationBytes += colorSize;
          if(m_DescriptorCoverage >= 60)
          {
            MTL::TextureDescriptor *native = (MTL::TextureDescriptor *)descriptor;
            const auto allocation = Unwrap(this)->heapTextureSizeAndAlign(native);
            native->release();
            sourcedBudgetInvalid |= !allocation.size || !allocation.align;
            nativeBudget.Add(nativeBudget.nativeBytes, allocation.size);
          }
        }
        else sourcedBudgetInvalid |= descriptor.width > (privateInitial ? 8U : 2U) ||
            descriptor.height > (privateInitial ? 8U : 2U) || descriptor.depth != 1 ||
            !descriptor.mipmapLevelCount || descriptor.mipmapLevelCount > (privateInitial ? 4U : 1U) ||
            descriptor.arrayLength != 1 || descriptor.sampleCount != 1;
        if(privateInitial && !colorInitial)
          sourcedAllocationBytes += RDCMIN(uint64_t(4096), descriptor.width * descriptor.height * 16);
        if(m_DescriptorCoverage >= 60 && !colorInitial)
          sourcedBudgetInvalid = true;
      }
      if(m_DescriptorCoverage >= 60 &&
         (chunk == MetalChunk::MTLBuffer_InternalModifyCPUContents ||
          chunk == MetalChunk::MTLBuffer_DescriptorCPUWrite))
      {
        ResourceId buffer; uint64_t start=0, size=0; bytebuf data;
        if(chunk == MetalChunk::MTLBuffer_InternalModifyCPUContents)
        {
          budget.Serialise("Buffer"_lit, buffer); budget.Serialise("start"_lit, start);
          budget.Serialise("size"_lit, size); budget.Serialise("data"_lit, data);
          sourcedBudgetInvalid |= size != data.size();
        }
        else
        {
          budget.Serialise("buffer"_lit, buffer); budget.Serialise("start"_lit, start);
          budget.Serialise("data"_lit, data);
        }
        sourcedBudgetInvalid |= !frame;
        nativeBudget.Add(nativeBudget.snapshotBytes, data.size());
      }
      if(!previouslyInvalid && sourcedBudgetInvalid && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal allocation metadata rejection: offset=%llu chunk=%s frame=%d\n",
            (unsigned long long)budgetOffset,GetChunkName((uint32_t)chunk).c_str(),int(frame));
      budget.EndChunk();
    }
    for(const auto &buffer : largeBackgroundBuffers)
    {
      // A write-only or unused Shared allocation need not carry frame-initial bytes.
      // Every consumer must prove a full submission-owned CPU snapshot below.
      // Never turn a later snapshot into an initial state or synthesize its contents.
      if(!bufferInitialSizes.count(buffer.first) && sharedBackgroundBuffers.count(buffer.first))
      {
        m_DescriptorSubmissionInitialBuffers[buffer.first] = buffer.second;
        continue;
      }
      if((!bufferInitialSizes.count(buffer.first) || bufferInitialSizes[buffer.first] != buffer.second) &&
         getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal allocation initial buffer rejection: resource=%s bytes=%llu initial=%llu\n",
            ToStr(buffer.first).c_str(),(unsigned long long)buffer.second,
            (unsigned long long)(bufferInitialSizes.count(buffer.first)?bufferInitialSizes[buffer.first]:0));
      sourcedBudgetInvalid |= !bufferInitialSizes.count(buffer.first) ||
          bufferInitialSizes[buffer.first] != buffer.second;
    }
    for(const auto &texture : colorInitialTextures)
      sourcedBudgetInvalid |= !textureInitialSizes.count(texture.first) ||
          textureInitialSizes[texture.first] != texture.second;
    if(m_DescriptorCoverage >= 60)
      for(const auto &buffer : budgetBackgroundBuffers)
        if(!bufferInitialSizes.count(buffer.first))
        {
          // Shared buffers can require fallback snapshots even if their data
          // was carried by the creation chunk instead of Initial Contents.
          nativeBudget.Add(nativeBudget.snapshotBytes,buffer.second);
          nativeBudget.Add(nativeBudget.snapshotBytes,buffer.second);
        }
    if(m_DescriptorCoverage >= 60 && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
      fprintf(stderr, "Metal replay allocation budget: native=%llu initial=%llu snapshots=%llu recommended=%llu allocated=%llu\n",
          (unsigned long long)nativeBudget.nativeBytes, (unsigned long long)nativeBudget.initialBytes,
          (unsigned long long)nativeBudget.snapshotBytes, (unsigned long long)Unwrap(this)->recommendedMaxWorkingSetSize(),
          (unsigned long long)Unwrap(this)->currentAllocatedSize());
    if(budget.IsErrored() || sourcedBudgetInvalid || (m_DescriptorCoverage >= 60 ?
        !nativeBudget.Fits(Unwrap(this)->recommendedMaxWorkingSetSize(), Unwrap(this)->currentAllocatedSize()) :
        sourcedAllocationBytes > (m_DescriptorCoverage >= 34 ? 256ULL * 1024 * 1024 : 1024 * 1024)))
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
          "Sourced Metal descriptors currently require bounded static allocations");
  }
  if(slotEvents && m_DescriptorCoverage < 4)
    RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
        "Metal descriptor slot diagnostics require a complete lifetime relocation contract; "
        "CPU XML export remains available.");
  if(identities && (!m_DescriptorCoverage || m_DescriptorTables.empty()))
    RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
        "Metal capture uses raw GPU addresses/resource IDs; descriptor relocation is unsupported "
        "without explicit table layouts and coverage. CPU XML export remains available.");
  if(!m_DescriptorTables.empty() && !m_DescriptorCoverage)
    RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Metal descriptor layouts require explicit coverage");
  for(const auto &view : m_DescriptorFrameViews)
    if(m_DescriptorFrameBuffers.count(view.first))
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Conflicting frame Metal descriptor resource creation");
  for(const auto &texture : m_DescriptorFrameTextures)
    if(m_DescriptorFrameBuffers.count(texture.first) || m_DescriptorFrameViews.count(texture.first))
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Conflicting frame sourced Metal texture identity");
  for(ResourceId buffer : m_DescriptorGPUWrittenBuffers)
  {
    bool declared = false;
    for(const DescriptorTable &layout : m_DescriptorTables)
      declared |= layout.buffer == buffer;
    if(m_DescriptorCoverage < 2 || !declared)
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "GPU-written descriptors require provenance coverage v2");
  }
  return ResultCode::Succeeded;
}

bool WrappedMTLDevice::RestoreDescriptorTable(ResourceId bufferId, const bytebuf &raw,
                                                uint64_t start, uint64_t size)
{
  WrappedMTLObject *object = GetResourceManager()->GetResource(bufferId, true);
  if(!object || object->m_Type != eResBuffer || !object->m_Real)
    return false;
  MTL::Buffer *destination = Unwrap((WrappedMTLBuffer *)object);
  if(destination->storageMode() != MTL::StorageModeShared || !destination->contents() ||
     raw.size() != destination->length())
    return false;
  bytebuf patched = raw;
  if(size == ~0ULL)
    size = raw.size();
  if(start > raw.size() || size > raw.size() - start)
    return false;
  if(m_DescriptorPreflight)
    return ValidateDescriptorValues(bufferId, raw);
  for(const DescriptorTable &layout : m_DescriptorTables)
  {
    if(layout.buffer != bufferId)
      continue;
    for(uint64_t entry = 0; entry < layout.count; entry++)
    {
      const uint32_t fields = layout.schema == 3 ? 3 : layout.schema == 1 ? 2 : 1;
      for(uint32_t field = 0; field < fields; field++)
      {
        const uint32_t kind = layout.schema == 2 ? 2 : field;
        const uint64_t offset = layout.offset + entry * layout.stride + field * 8;
        uint64_t captured = 0, replacement = 0;
        memcpy(&captured, raw.data() + offset, 8);
        if(captured)
        {
          WrappedMTLObject *chosen = NULL;
          uint64_t chosenOffset = 0;
          for(const auto &identity : m_ReplayGPUIdentities)
          {
            if(identity.second.kind != kind)
              continue;
            WrappedMTLObject *candidate = GetResourceManager()->GetResource(identity.first, true);
            if(!candidate || !candidate->m_Real)
              continue;
            uint64_t memberOffset = 0;
            if(kind == 0)
            {
              if(candidate->m_Type != eResBuffer || captured < identity.second.value)
                continue;
              memberOffset = captured - identity.second.value;
              if(memberOffset >= Unwrap((WrappedMTLBuffer *)candidate)->length())
                continue;
              for(const DescriptorTable &other : m_DescriptorTables)
                if(other.buffer == identity.first && m_DescriptorCoverage < 2)
                {
                  RDCERR("GPU-authored descriptor tables require a later relocation version");
                  return false;
                }
            }
            else if(captured != identity.second.value ||
                    candidate->m_Type != (kind == 1 ? eResTexture : eResSamplerState))
              continue;
            // Verify immutable sampler aliases against replay native state. Buffer ranges
            // and texture views require a unique resource; never guess through heap aliases.
            if(chosen && kind != 2)
            {
              RDCERR("Ambiguous raw Metal descriptor identity");
              return false;
            }
            if(chosen && kind == 2 &&
               Unwrap((WrappedMTLSamplerState *)chosen)->gpuResourceID()._impl !=
               Unwrap((WrappedMTLSamplerState *)candidate)->gpuResourceID()._impl)
            {
              RDCERR("Conflicting immutable Metal sampler descriptor aliases");
              return false;
            }
            if(!chosen)
            {
              chosen = candidate;
              chosenOffset = memberOffset;
            }
          }
          if(!chosen)
          {
            RDCERR("Unresolved Metal descriptor: buffer=%s entry=%llu kind=%u value=%llu",
                   ToStr(bufferId).c_str(), entry, kind, captured);
            return false;
          }
          if(kind == 0)
            replacement = Unwrap((WrappedMTLBuffer *)chosen)->gpuAddress() + chosenOffset;
          else if(kind == 1)
            replacement = Unwrap((WrappedMTLTexture *)chosen)->gpuResourceID()._impl;
          else
            replacement = Unwrap((WrappedMTLSamplerState *)chosen)->gpuResourceID()._impl;
          if(!replacement)
            return false;
        }
        memcpy(patched.data() + offset, &replacement, 8);
      }
    }
  }
  memcpy((byte *)destination->contents() + start, patched.data() + start, size);
  return true;
}

bool WrappedMTLDevice::ValidateDescriptorValues(ResourceId bufferId, const bytebuf &raw)
{
  // Validate capture-time fields without allocating future resources or encoding GPU work.
  // Frame-created buffers/views enter the candidate set only after their creation chunk.
  for(const DescriptorTable &layout : m_DescriptorTables)
  {
    if(layout.buffer != bufferId)
      continue;
    for(uint64_t entry = 0; entry < layout.count; entry++)
    {
      const uint32_t fields = layout.schema == 3 ? 3 : layout.schema == 1 ? 2 : 1;
      for(uint32_t field = 0; field < fields; field++)
      {
        const uint32_t kind = layout.schema == 2 ? 2 : field;
        const uint64_t offset = layout.offset + entry * layout.stride + field * 8;
        if(offset > raw.size() || 8 > raw.size() - offset)
          return false;
        uint64_t captured = 0;
        memcpy(&captured, raw.data() + offset, 8);
        if(!captured)
          continue;
        uint32_t matches = 0;
        uint64_t samplerID = 0;
        for(const auto &identity : m_ReplayGPUIdentities)
        {
          if(identity.second.kind != kind)
            continue;
          uint64_t length = 0;
          WrappedMTLObject *object = NULL;
          auto future = m_DescriptorFrameBuffers.find(identity.first);
          if(future != m_DescriptorFrameBuffers.end())
          {
            if(kind != 0 || !m_DescriptorPreflightLiveBuffers.count(identity.first))
              continue;
            length = future->second.length;
          }
          else if(m_DescriptorFrameTextures.count(identity.first))
          {
            if(kind != 1 || !m_DescriptorPreflightLiveTextures.count(identity.first)) continue;
          }
          else if(m_DescriptorFrameViews.count(identity.first))
          {
            if(kind != 1 || !m_DescriptorPreflightLiveViews.count(identity.first))
              continue;
          }
          else
          {
            object = GetResourceManager()->GetResource(identity.first, true);
            const MetalResourceType type = kind == 0 ? eResBuffer : kind == 1 ? eResTexture : eResSamplerState;
            if(!object || !object->m_Real || object->m_Type != type)
              continue;
            if(kind == 0)
              length = Unwrap((WrappedMTLBuffer *)object)->length();
          }
          if(kind == 0)
          {
            if(captured < identity.second.value || captured - identity.second.value >= length)
              continue;
            for(const DescriptorTable &other : m_DescriptorTables)
              if(other.buffer == identity.first && m_DescriptorCoverage < 2)
                return false;
          }
          else if(captured != identity.second.value)
            continue;
          if(kind == 2)
          {
            const uint64_t native = Unwrap((WrappedMTLSamplerState *)object)->gpuResourceID()._impl;
            if(!native || (matches && native != samplerID))
              return false;
            samplerID = native;
          }
          else if(matches)
            return false;
          matches++;
        }
        if(!matches)
          return false;
      }
    }
  }
  return true;
}

bool WrappedMTLDevice::PrepareDescriptorTables()
{
  if(m_DescriptorCoverage >= 4)
    return PrepareDescriptorSlotShadow();
  for(const DescriptorTable &layout : m_DescriptorTables)
  {
    if(m_DescriptorRawContents.count(layout.buffer))
      continue;
    WrappedMTLObject *object = GetResourceManager()->GetResource(layout.buffer, true);
    if(!object || object->m_Type != eResBuffer || !object->m_Real)
      return false;
    MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)object);
    if(buffer->storageMode() != MTL::StorageModeShared || !buffer->contents())
      return false;
    if(!m_ReplayBufferInitialContents.count(layout.buffer))
      m_ReplayBufferInitialContents[layout.buffer] = bytebuf((byte *)buffer->contents(), buffer->length());
    m_DescriptorRawContents[layout.buffer] = m_ReplayBufferInitialContents[layout.buffer];
    m_ReplayCPUUpdatedBuffers.insert(layout.buffer);
  }
  for(const auto &raw : m_DescriptorRawContents)
    if(!RestoreDescriptorTable(raw.first, raw.second))
      return false;
  return true;
}

bool WrappedMTLDevice::PatchDescriptorSource(const DescriptorSource &source,
    uint64_t captured, uint64_t &replacement)
{
  if(m_DescriptorPreflight && m_DescriptorPreflightAliasedBuffers.count(source.resource)) return false;
  auto identity = m_ReplayGPUIdentities.find(source.resource);
  auto future = m_DescriptorFrameBuffers.find(source.resource);
  if(m_DescriptorPreflight && m_DescriptorCoverage >= 8 && future != m_DescriptorFrameBuffers.end())
  {
    if(!m_DescriptorPreflightLiveBuffers.count(source.resource) || !captured ||
       identity == m_ReplayGPUIdentities.end() || identity->second.kind != 0 ||
       source.offset >= future->second.length || identity->second.value > ~0ULL - source.offset ||
       identity->second.value + source.offset != captured)
      return false;
    replacement = captured; // CPU validation only, no future native allocation is created.
    return true;
  }
  WrappedMTLObject *object = GetResourceManager()->GetResource(source.resource, true);
  if(!captured || identity == m_ReplayGPUIdentities.end() || identity->second.kind != 0 ||
     !object || object->m_Type != eResBuffer || !object->m_Real)
    return false;
  MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)object);
  auto initial = m_ReplayBufferInitialContents.find(source.resource);
  const bool complete = initial != m_ReplayBufferInitialContents.end() &&
                        initial->second.size() == buffer->length();
  const bool generalInitial = m_DescriptorCoverage >= 34 && complete;
  const bool frameSource = m_DescriptorCoverage >= 40 && future != m_DescriptorFrameBuffers.end();
  if((m_DescriptorCoverage >= 13 && IsReplayResourceAliasable(source.resource)) ||
     (buffer->storageMode() != MTL::StorageModeShared &&
      !(m_DescriptorCoverage >= 23 && buffer->storageMode() == MTL::StorageModePrivate &&
        ((generalInitial && !buffer->heap()) ||
         (buffer->heap() && buffer->hazardTrackingMode() == MTL::HazardTrackingModeTracked)) &&
        (IsFramePlacementResource(source.resource) ||
         (m_ReplayBufferInitialContents.count(source.resource) &&
          m_ReplayBufferInitialContents[source.resource].size() == buffer->length())))) ||
     buffer->length() > (frameSource ? DescriptorFrameBufferLimit(future->second.heap != ResourceId(), m_DescriptorCoverage) :
         generalInitial ? 128ULL * 1024 * 1024 : 64 * 1024) ||
     source.offset >= buffer->length() || identity->second.value > ~0ULL - source.offset ||
     identity->second.value + source.offset != captured)
    return false;
  replacement = buffer->gpuAddress() + source.offset;
  return replacement != 0;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_DescriptorSlotProducer(SerialiserType &ser, ResourceId buffer,
    uint64_t offset, ResourceId encoder, ResourceId source, uint64_t sourceOffset)
{
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(encoder).Important();
  SERIALISE_ELEMENT(source).Important();
  SERIALISE_ELEMENT(sourceOffset);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsStructuredExporting(m_State) || !IsReplayingAndReading()) return true;
  if(m_DescriptorCoverage < 6 || !m_DescriptorShadowFrame || m_DescriptorDispatches[encoder] != 1)
    return false;
  if(!m_DescriptorPreflight)
  {
    WrappedMTLObject *object = GetResourceManager()->GetResource(encoder, true);
    if(!object || object->m_Type != eResComputeCommandEncoder ||
       GetReplayComputeCommandEncoder((WrappedMTLComputeCommandEncoder *)object) != object)
      return false;
  }
  return TrackDescriptorGPUCopy(source, sourceOffset, buffer, offset, 24);
}
template bool WrappedMTLDevice::Serialise_DescriptorSlotProducer(ReadSerialiser &, ResourceId,
    uint64_t, ResourceId, ResourceId, uint64_t);
template bool WrappedMTLDevice::Serialise_DescriptorSlotProducer(WriteSerialiser &, ResourceId,
    uint64_t, ResourceId, ResourceId, uint64_t);

static bool DescriptorUEFields(uint32_t type, std::map<uint32_t, uint64_t> &fields)
{
  // UE 5.8 ERHIDescriptorType, not the synthetic v4-v6 fixture markers.
  if(type == 0 || type == 1 || type == 6) fields[0] = 0;
  else if(type == 2 || type == 3) { fields[0] = 0; fields[1] = 8; }
  // UE MetalUAV allocates TextureSRV/UAV handles for FBufferView as well as
  // FTextureBufferBacked. The IR factory/source bindings determine which native
  // fields are populated; the logical RHI handle alone is not a packet layout.
  else if(type == 4 || type == 5) { fields[0] = 0; fields[1] = 8; }
  else if(type == 7) fields[2] = 0;
  else return false;
  return true;
}

bool WrappedMTLDevice::PatchDescriptorSlotField(const DescriptorSource &source, uint32_t kind,
    uint64_t captured, uint64_t &replacement)
{
  if(kind == 0) return PatchDescriptorSource(source, captured, replacement);
  auto identity = m_ReplayGPUIdentities.find(source.resource);
  if(kind == 1 && m_DescriptorCoverage >= 36)
  {
    auto futureParent = m_DescriptorFrameTextureViewParents.find(source.resource);
    const ResourceId parent = futureParent != m_DescriptorFrameTextureViewParents.end() ?
        futureParent->second : GetReplayTextureViewParent(source.resource);
    if(m_DescriptorPreflight ? (m_DescriptorPreflightAliasedBuffers.count(source.resource) ||
        m_DescriptorPreflightAliasedBuffers.count(parent)) :
        (IsReplayResourceAliasable(source.resource) || IsReplayResourceAliasable(parent))) return false;
  }
  if(m_DescriptorPreflight && kind == 1 && m_DescriptorFrameTextures.count(source.resource))
  {
    if(!captured || source.offset || !m_DescriptorPreflightLiveTextures.count(source.resource) ||
       identity == m_ReplayGPUIdentities.end() || identity->second.kind != 1 || identity->second.value != captured)
      return false;
    replacement = captured; // Identity validation only; native texture is created at its birth.
    return true;
  }
  if(m_DescriptorPreflight && kind == 1 && m_DescriptorCoverage >= 31 &&
     m_DescriptorFrameViews.count(source.resource))
  {
    ResourceId parent = m_DescriptorFrameViews[source.resource];
    auto object=GetResourceManager()->GetResource(parent,true);
    MTL::Buffer *native=object && object->m_Type==eResBuffer ? Unwrap((WrappedMTLBuffer *)object) : NULL;
    auto initial=m_ReplayBufferInitialContents.find(parent);
    const bool backgroundParent=m_DescriptorCoverage>=65 && !m_DescriptorFrameBuffers.count(parent) &&
        native && native->storageMode()==MTL::StorageModePrivate && native->length()<=128ULL*1024*1024 &&
        initial!=m_ReplayBufferInitialContents.end() && initial->second.size()==native->length();
    if(!captured || source.offset || !m_DescriptorPreflightLiveViews.count(source.resource) ||
       (!m_DescriptorPreflightLiveBuffers.count(parent) && !backgroundParent) || m_DescriptorPreflightAliasedBuffers.count(parent) ||
       identity == m_ReplayGPUIdentities.end() || identity->second.kind != 1 || identity->second.value != captured)
      return false;
    replacement = captured;
    return true;
  }
  WrappedMTLObject *object = GetResourceManager()->GetResource(source.resource, true);
  if(!captured || source.offset || identity == m_ReplayGPUIdentities.end() ||
     identity->second.kind != kind || identity->second.value != captured || !object || !object->m_Real)
    return false;
  if(kind == 1 && object->m_Type == eResTexture)
  {
    MTL::Texture *texture = Unwrap((WrappedMTLTexture *)object);
    if(m_DescriptorCoverage >= 32 && texture->textureType() == MTL::TextureTypeTextureBuffer)
    {
      ResourceId parent = GetReplay()->GetBufferTextureSource(source.resource);
      auto backing = GetResourceManager()->GetResource(parent, true);
      if(parent == ResourceId() || !backing || backing->m_Type != eResBuffer || !backing->m_Real ||
         texture->buffer() != Unwrap((WrappedMTLBuffer *)backing) ||
         texture->storageMode() != MTL::StorageModePrivate ||
         (m_DescriptorPreflight && m_DescriptorPreflightAliasedBuffers.count(parent))) return false;
      auto initial = m_ReplayBufferInitialContents.find(parent);
      if(!m_DescriptorFrameViews.count(source.resource) &&
         (initial == m_ReplayBufferInitialContents.end() ||
          initial->second.size() != Unwrap((WrappedMTLBuffer *)backing)->length())) return false;
      replacement = texture->gpuResourceID()._impl;
      return replacement != 0;
    }
    const bool hasPrivateInitialContents = m_DescriptorCoverage >= 27 &&
        !m_DescriptorFrameTextures.count(source.resource) &&
        (m_ReplayTextureInitialContents.count(source.resource) ||
         (m_DescriptorCoverage >= 28 && HasReplayTextureInitialContents(source.resource)));
    // Swapchain/drawable handles are valid before their first clear, as in
    // Vulkan. Preflight independently proves initialization before shader reads.
    if(m_DescriptorCoverage>=65 && m_DescriptorDrawableTextures.count(source.resource) &&
       ValidDescriptorDrawableTexture(texture)) {
      replacement=texture->gpuResourceID()._impl;return replacement!=0;
    }
    if(m_DescriptorCoverage >= 37 && !hasPrivateInitialContents &&
       !m_DescriptorFrameTextures.count(source.resource)) return false;
    if(m_DescriptorCoverage >= 36 && hasPrivateInitialContents && !texture->framebufferOnly() &&
       texture->sampleCount() == 1 &&
       (texture->storageMode() == MTL::StorageModePrivate || texture->storageMode() == MTL::StorageModeShared ||
        (m_DescriptorCoverage >= 37 && texture->storageMode() == MTL::StorageModeManaged)) &&
       (texture->textureType() == MTL::TextureType2D || texture->textureType() == MTL::TextureType2DArray ||
        texture->textureType() == MTL::TextureTypeCube || texture->textureType() == MTL::TextureTypeCubeArray ||
        texture->textureType() == MTL::TextureType3D))
    {
      uint32_t bw = 0, bh = 0, bytes = 0;
      if(texture->pixelFormat() != MTL::PixelFormatDepth32Float_Stencil8 &&
         texture->pixelFormat() != MTL::PixelFormatX32_Stencil8 &&
         !GetTextureDataBlockShape(texture->pixelFormat(), bw, bh, bytes)) return false;
      replacement = texture->gpuResourceID()._impl;
      return replacement != 0;
    }
    if(m_DescriptorCoverage >= 35 && hasPrivateInitialContents &&
       (texture->storageMode() == MTL::StorageModePrivate || texture->storageMode() == MTL::StorageModeShared) &&
       texture->textureType() == MTL::TextureType2D && !texture->framebufferOnly() &&
       texture->sampleCount() == 1 && texture->pixelFormat() != MTL::PixelFormatDepth16Unorm &&
       texture->pixelFormat() != MTL::PixelFormatDepth32Float)
    {
      uint32_t bw = 0, bh = 0, bytes = 0;
      if(!GetTextureDataBlockShape(texture->pixelFormat(), bw, bh, bytes)) return false;
      replacement = texture->gpuResourceID()._impl;
      return replacement != 0;
    }
    if(m_DescriptorCoverage >= 33 && hasPrivateInitialContents &&
       texture->storageMode() == MTL::StorageModePrivate && texture->textureType() == MTL::TextureTypeCube &&
       texture->width() && texture->width() <= 8192 && texture->height() == texture->width() &&
       texture->depth() == 1 && texture->arrayLength() == 1 && texture->sampleCount() == 1 &&
       texture->mipmapLevelCount() && texture->mipmapLevelCount() <= 14 && !texture->framebufferOnly())
    {
      uint32_t bw = 0, bh = 0, bytes = 0;
      if(!GetTextureDataBlockShape(texture->pixelFormat(), bw, bh, bytes)) return false;
      replacement = texture->gpuResourceID()._impl;
      return replacement != 0;
    }
    if(m_DescriptorCoverage >= 38 && m_DescriptorFrameTextures.count(source.resource) &&
       (IsFramePlacementResource(source.resource) ||
        (m_DescriptorFrameTextureViewParents.count(source.resource) && IsFrameBufferTextureView(source.resource))) &&
       ValidDescriptorFrameTexture(m_DescriptorFrameTextures[source.resource].descriptor, m_DescriptorCoverage) &&
       texture->storageMode() == MTL::StorageModePrivate && !texture->framebufferOnly())
    {
      replacement = texture->gpuResourceID()._impl;
      return replacement != 0;
    }
    if((texture->storageMode() != MTL::StorageModeShared &&
        !(texture->storageMode() == MTL::StorageModePrivate && hasPrivateInitialContents) &&
        !(m_DescriptorCoverage >= 21 && m_DescriptorFrameTextures.count(source.resource) &&
          (IsFramePlacementResource(source.resource) ||
           (m_DescriptorCoverage >= 26 && m_DescriptorFrameTextureViewParents.count(source.resource) &&
            IsFrameBufferTextureView(source.resource))) && texture->storageMode() == MTL::StorageModePrivate)) ||
       texture->textureType() != MTL::TextureType2D ||
       texture->width() > 2 || texture->height() > 2 || texture->depth() != 1 ||
       texture->mipmapLevelCount() != 1 || texture->arrayLength() != 1 || texture->sampleCount() != 1 ||
       (texture->pixelFormat() != MTL::PixelFormatRGBA8Unorm && texture->pixelFormat() != MTL::PixelFormatBGRA8Unorm))
      return false;
    replacement = texture->gpuResourceID()._impl;
  }
  else if(kind == 2 && object->m_Type == eResSamplerState &&
          GetReplay()->SupportsSamplerArgumentBuffers(source.resource))
    replacement = Unwrap((WrappedMTLSamplerState *)object)->gpuResourceID()._impl;
  else return false;
  return replacement != 0;
}

bool WrappedMTLDevice::PatchDescriptorSlot(const DescriptorSlotShadow &slot, bytebuf &data)
{
  if(!slot.live || slot.data.size() != 24) return false;
  if(m_DescriptorCoverage >= 7)
  {
    std::map<uint32_t, uint64_t> fields;
    if(!DescriptorUEFields(slot.type, fields)) return false;
    uint64_t first = 0, second = 0;
    memcpy(&first, slot.data.data(), 8); memcpy(&second, slot.data.data() + 8, 8);
    if((slot.type == 0 || slot.type == 1 || slot.type == 6) && second) return false;
    for(const auto &source : slot.sources) if(!fields.count(source.first)) return false;
    data = slot.data;
    for(const auto &field : fields)
    {
      uint64_t captured = 0, replacement = 0;
      memcpy(&captured, data.data() + field.second, 8);
      auto source = slot.sources.find(field.first);
      if(captured ? (source == slot.sources.end() ||
          !PatchDescriptorSlotField(source->second, field.first, captured, replacement)) :
          source != slot.sources.end()) return false;
      memcpy(data.data() + field.second, &replacement, 8);
    }
    return true;
  }
  if(slot.type != 4 && slot.type != 5) return false;
  uint64_t captured = 0, texture = 0, replacement = 0;
  memcpy(&captured, slot.data.data(), 8);
  memcpy(&texture, slot.data.data() + 8, 8);
  if(texture || (captured ? !PatchDescriptorSource(slot.source, captured, replacement) :
                   slot.source.resource != ResourceId()))
    return false;
  data = slot.data;
  memcpy(data.data(), &replacement, 8);
  return true;
}

bool WrappedMTLDevice::IsDescriptorPreludeRetirement(const DescriptorSlotKey &key,
                                                     const DescriptorSlotShadow &slot) const
{
  const auto retirement = m_DescriptorPreludeRetirements.find(key);
  return m_DescriptorCoverage >= 16 && slot.live && retirement != m_DescriptorPreludeRetirements.end() &&
      retirement->second.generation == slot.generation && retirement->second.type == slot.type;
}

bool WrappedMTLDevice::ReadDescriptorSlotEvent(ResourceId buffer, uint64_t offset,
    uint64_t generation, uint32_t event, uint32_t type, const bytebuf &data, bool initialCPUValue)
{
  // Sourced contracts exclude render consumption, frame births and aliases. v5
  // additionally validates full-entry GPU copies from a typed CPU payload; their
  // expected-value metadata must never overwrite the GPU destination on the CPU.
  const bool gpuValue = event == 3;
  if(m_DescriptorPreflight && m_DescriptorPreflightAliasedBuffers.count(buffer)) return false;
  if(buffer == ResourceId() || offset % 8 || event > 3 ||
     (gpuValue ? (m_DescriptorCoverage < 5 || generation || type) :
                  (!generation || (m_DescriptorCoverage >= 7 ? type > 7 : (type != 4 && type != 5)))))
    return false;
  WrappedMTLObject *object = GetResourceManager()->GetResource(buffer, true);
  uint64_t length = 0;
  auto future = m_DescriptorFrameBuffers.find(buffer);
  if(m_DescriptorPreflight && m_DescriptorCoverage >= 8 && future != m_DescriptorFrameBuffers.end())
  {
    if(!m_DescriptorPreflightLiveBuffers.count(buffer) ||
       !m_DescriptorPreflightLiveTables.count(buffer)) return false;
    length = future->second.length;
  }
  else
  {
    if(!object || object->m_Type != eResBuffer || !object->m_Real) return false;
    MTL::Buffer *native = Unwrap((WrappedMTLBuffer *)object);
    if(native->storageMode() != MTL::StorageModeShared) return false;
    length = native->length();
  }
  if(length > DescriptorTableBufferLimit(m_DescriptorCoverage) || offset > length || 24 > length - offset)
    return false;
  const DescriptorSlotKey key = make_rdcpair(buffer, offset);
  auto old = m_DescriptorSlotShadow.find(key);
  if(event == 0)
  {
    if(!data.empty() || (old != m_DescriptorSlotShadow.end() &&
       (old->second.live || generation <= old->second.generation)))
      return false;
    for(const auto &other : m_DescriptorSlotShadow)
      if(other.second.live && other.first.first == buffer && !(other.first == key) &&
         offset < other.first.second + 24 && other.first.second < offset + 24)
        return false;
    DescriptorSlotShadow slot;
    slot.generation = generation;
    slot.type = type;
    slot.live = true;
    m_DescriptorSlotShadow[key] = slot;
    return true;
  }
  if(old == m_DescriptorSlotShadow.end() || !old->second.live ||
     (!gpuValue && (generation != old->second.generation || type != old->second.type)))
    return false;
  if(gpuValue)
  {
    if(!m_DescriptorGPUWrittenBuffers.count(buffer) || data.size() != 24) return false;
    DescriptorSource expectedSource;
    std::map<uint32_t, DescriptorSource> expectedSources;
    if(m_DescriptorShadowFrame)
    {
      auto expected = m_DescriptorGPUCopyExpected.find(key);
      if(expected == m_DescriptorGPUCopyExpected.end() || data != expected->second.data)
        return false;
      expectedSource = expected->second.source;
      expectedSources = expected->second.sources;
      m_DescriptorGPUCopyExpected.erase(expected);
    }
    old->second.data = data;
    old->second.source = {};
    old->second.gpuExpected = true;
    old->second.gpuCopySource = expectedSource;
    old->second.sources.clear();
    old->second.gpuCopySources = expectedSources;
    // GPU expected metadata validates the copy; it must never become a CPU write.
    return true;
  }
  const bool provenInitialCPUValue=m_DescriptorCoverage>=61 && event==2 &&
      ValidDescriptorCPUWrite(buffer,offset,data.size()) &&
      (m_DescriptorPreflight ? initialCPUValue : m_DescriptorInitialCPUValueOffsets.count(m_CurChunkOffset));
  if(m_DescriptorShadowFrame && m_DescriptorGPUWrittenBuffers.count(buffer) &&
     !provenInitialCPUValue && !(event == 1 && (IsDescriptorPreludeRetirement(key, old->second) ||
       (m_DescriptorCoverage>=63 && old->second.data.empty()) ||
       m_DescriptorCoverage>=65))) return false;
  if(event == 1)
  {
    if(!data.empty()) return false;
    old->second.live = false;
    return true;
  }
  if(data.size() != 24) return false;
  old->second.data = data;
  old->second.source = {};
  old->second.gpuExpected = false;
  old->second.gpuCopySource = {};
  old->second.sources.clear();
  old->second.gpuCopySources.clear();
  uint64_t captured = 0;
  memcpy(&captured, data.data(), 8);
  if(m_DescriptorShadowFrame && !m_DescriptorPreflight && m_DescriptorCoverage >= 7)
  {
    bytebuf patched;
    if(PatchDescriptorSlot(old->second, patched))
      return ReplayCPUBufferUpdate((WrappedMTLBuffer *)object, offset, patched);
    return true; // Required nonzero sources are validated before the next dispatch.
  }
  if(m_DescriptorShadowFrame && !m_DescriptorPreflight && !captured)
  {
    bytebuf patched;
    if(!PatchDescriptorSlot(old->second, patched)) return false;
    return ReplayCPUBufferUpdate((WrappedMTLBuffer *)object, offset, patched);
  }
  return true;
}

bool WrappedMTLDevice::ReadDescriptorSlotBinding(ResourceId buffer, uint64_t offset,
    ResourceId source, uint32_t kind, uint64_t memberOffset)
{
  auto found = m_DescriptorSlotShadow.find(make_rdcpair(buffer, offset));
  if(m_DescriptorCoverage >= 7)
  {
    if(source == ResourceId() || found == m_DescriptorSlotShadow.end() ||
       !found->second.live || found->second.data.size() != 24 || found->second.sources.count(kind)) return false;
    std::map<uint32_t, uint64_t> fields;
    if(!DescriptorUEFields(found->second.type, fields) || !fields.count(kind)) return false;
    if(m_DescriptorShadowFrame && found->second.gpuExpected)
    {
      auto expected = found->second.gpuCopySources.find(kind);
      if(expected == found->second.gpuCopySources.end() || expected->second.resource != source ||
         expected->second.offset != memberOffset) return false;
    }
    found->second.sources[kind] = {source, memberOffset};
    if(!m_DescriptorShadowFrame) return true;
    uint64_t captured = 0, replacement = 0;
    memcpy(&captured, found->second.data.data() + fields[kind], 8);
    if(!PatchDescriptorSlotField(found->second.sources[kind], kind, captured, replacement)) return false;
    for(const auto &field : fields)
    {
      memcpy(&captured, found->second.data.data() + field.second, 8);
      if(captured && !found->second.sources.count(field.first)) return true;
    }
    bytebuf patched;
    if(!PatchDescriptorSlot(found->second, patched)) return false;
    if(m_DescriptorPreflight || found->second.gpuExpected) return true;
    auto object = GetResourceManager()->GetResource(buffer, true);
    return object && ReplayCPUBufferUpdate((WrappedMTLBuffer *)object, offset, patched);
  }
  if(kind != 0 || source == ResourceId() || found == m_DescriptorSlotShadow.end() ||
     !found->second.live || found->second.data.size() != 24 ||
     found->second.source.resource != ResourceId())
    return false;
  if(m_DescriptorShadowFrame && found->second.gpuExpected &&
     (found->second.gpuCopySource.resource != source ||
      found->second.gpuCopySource.offset != memberOffset))
    return false;
  found->second.source = {source, memberOffset};
  // Background history may mention a retired source. Only its final live shadow
  // participates in the initial state, matching D3D12's logical descriptor shadow.
  if(!m_DescriptorShadowFrame) return true;
  bytebuf patched;
  if(!PatchDescriptorSlot(found->second, patched)) return false;
  if(m_DescriptorPreflight || found->second.gpuExpected) return true;
  auto object = GetResourceManager()->GetResource(buffer, true);
  return object && ReplayCPUBufferUpdate((WrappedMTLBuffer *)object, offset, patched);
}

bool WrappedMTLDevice::TrackDescriptorGPUCopy(ResourceId source, uint64_t sourceOffset,
    ResourceId destination, uint64_t destinationOffset, uint64_t size)
{
  if(m_DescriptorCoverage < 4) return true;
  if(m_DescriptorCoverage >= 30)
    for(const auto &copy : m_DescriptorPreludeBufferCopies)
      if(source == copy.source && destination == copy.destination && sourceOffset == copy.sourceOffset &&
         destinationOffset == copy.destinationOffset && size == copy.size) return true;
  if(m_DescriptorCoverage >= 32)
  {
    bool table = false;
    for(const auto &layout : m_DescriptorTables)
      table |= layout.buffer == source || layout.buffer == destination;
    if(!table)
    {
      auto length = [&](ResourceId id) -> uint64_t {
        auto future = m_DescriptorFrameBuffers.find(id);
        if(m_DescriptorPreflight && future != m_DescriptorFrameBuffers.end())
          return m_DescriptorPreflightLiveBuffers.count(id) &&
                 !m_DescriptorPreflightAliasedBuffers.count(id) ? future->second.length : 0;
        auto object = GetResourceManager()->GetResource(id, true);
        if(!object || object->m_Type != eResBuffer || !object->m_Real ||
           (m_DescriptorPreflight && m_DescriptorPreflightAliasedBuffers.count(id))) return 0;
        return Unwrap((WrappedMTLBuffer *)object)->length();
      };
      const uint64_t srcLength = length(source), dstLength = length(destination);
      return source != destination && size && size <= DescriptorPlainCopyLimit(m_DescriptorCoverage) &&
          sourceOffset <= srcLength && size <= srcLength - sourceOffset &&
          destinationOffset <= dstLength && size <= dstLength - destinationOffset;
    }
  }
  if(m_DescriptorCoverage < 5 || size != 24 || source == destination ||
     m_DescriptorGPUWrittenBuffers.count(source) || !m_DescriptorGPUWrittenBuffers.count(destination))
    return false;
  const auto from = m_DescriptorSlotShadow.find(make_rdcpair(source, sourceOffset));
  const auto to = m_DescriptorSlotShadow.find(make_rdcpair(destination, destinationOffset));
  const auto key = make_rdcpair(destination, destinationOffset);
  if(from == m_DescriptorSlotShadow.end() || to == m_DescriptorSlotShadow.end() ||
     !from->second.live || !to->second.live || from->second.gpuExpected ||
     from->second.type != to->second.type || m_DescriptorGPUCopyExpected.count(key))
    return false;
  bytebuf patched;
  if(!PatchDescriptorSlot(from->second, patched) ||
     (m_DescriptorCoverage >= 7 ? from->second.sources.empty() : from->second.source.resource == ResourceId()))
    return false;
  m_DescriptorGPUCopyExpected[key] = from->second;
  return true;
}

bool WrappedMTLDevice::IsRetiredDescriptorBacking(ResourceId buffer) const
{
  if(m_DescriptorCoverage < 25) return false;
  for(const auto &pair : m_ValidatedDescriptorBackingAliases)
    if(pair.first == buffer)
    {
      auto replacement = m_ResourceManager->GetResource(pair.second, true);
      if(!replacement || !replacement->m_Real) continue;
      // Frame natives disappear before each seek. The proof remains attached to
      // ResourceIds, but becomes effective only at the replacement's actual birth.
      for(const auto &slot : m_DescriptorSlotShadow)
        if(slot.first.first == buffer && slot.second.live) return false;
      return true;
    }
  return false;
}

bool WrappedMTLDevice::OverlayAliasedDescriptorTables(ResourceId buffer, uint64_t start, uint64_t size)
{
  if(m_DescriptorCoverage < 25 || m_ValidatedDescriptorBackingAliases.empty()) return true;
  auto object = GetResourceManager()->GetResource(buffer, true);
  MTL::Buffer *source = object && object->m_Type == eResBuffer ? Unwrap((WrappedMTLBuffer *)object) : NULL;
  if(!source || !source->heap()) return true;
  if(start > source->length()) return false;
  if(size == ~0ULL) size = source->length() - start;
  if(size > source->length() - start) return false;
  const uint64_t begin = source->heapOffset() + start, end = begin + size;
  std::set<ResourceId> restored;
  for(const auto &layout : m_DescriptorTables)
  {
    if(layout.buffer == buffer || restored.count(layout.buffer)) continue;
    auto targetObject = GetResourceManager()->GetResource(layout.buffer, true);
    MTL::Buffer *target = targetObject && targetObject->m_Type == eResBuffer ?
        Unwrap((WrappedMTLBuffer *)targetObject) : NULL;
    if(!target || target->heap() != source->heap() || IsRetiredDescriptorBacking(layout.buffer)) continue;
    if(begin < target->heapOffset() + target->length() && target->heapOffset() < end)
    {
      // Conservative useHeaps snapshots can include both the retired backing
      // and its replacement. Restore physical bytes, then re-encode live logical
      // slots; never interpret a duplicate raw captured VA as a new identity.
      const uint64_t targetBegin = RDCMAX(begin, target->heapOffset());
      const uint64_t targetEnd = RDCMIN(end, target->heapOffset() + target->length());
      if(m_DescriptorGPUWrittenBuffers.count(layout.buffer) ||
         !OverlayDescriptorSlotBuffer(layout.buffer, false, targetBegin - target->heapOffset(),
                                      targetEnd - targetBegin)) return false;
      restored.insert(layout.buffer);
    }
  }
  return true;
}

bool WrappedMTLDevice::OverlayDescriptorSlotBuffer(ResourceId buffer, bool initialRestore,
                                                   uint64_t start, uint64_t size)
{
  if(IsRetiredDescriptorBacking(buffer)) return true;
  bool needed = false;
  for(const auto &entry : m_DescriptorSlotShadow)
    needed |= entry.first.first == buffer;
  if(!needed) return true;
  WrappedMTLObject *object = GetResourceManager()->GetResource(buffer, true);
  if(!object || object->m_Type != eResBuffer || !object->m_Real)
    return false;
  MTL::Buffer *native = Unwrap((WrappedMTLBuffer *)object);
  if(native->storageMode() != MTL::StorageModeShared || !native->contents()) return false;
  if(start > native->length()) return false;
  if(size == ~0ULL) size = native->length() - start;
  if(size > native->length() - start) return false;
  for(const auto &entry : m_DescriptorSlotShadow)
  {
    if(entry.first.first != buffer) continue;
    const uint64_t offset = entry.first.second;
    if(offset > native->length() || 24 > native->length() - offset) return false;
    if(offset >= start + size || start >= offset + 24) continue;
    if(entry.second.live && !IsDescriptorPreludeRetirement(entry.first, entry.second))
    {
      if(entry.second.gpuExpected && !initialRestore) continue;
      if(m_DescriptorCoverage>=63 && !initialRestore && entry.second.data.empty() &&
         m_DescriptorGPUWrittenBuffers.count(buffer)) continue;
      bytebuf patched;
      if(!PatchDescriptorSlot(entry.second, patched))
      {
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        {
          fprintf(stderr,"Metal descriptor overlay slot failed: buffer=%s offset=%llu generation=%llu type=%u bytes=%zu initial=%d sources=%zu\n",
              ToStr(buffer).c_str(),(unsigned long long)offset,
              (unsigned long long)entry.second.generation,entry.second.type,entry.second.data.size(),initialRestore,entry.second.sources.size());
          for(const auto &source:entry.second.sources)
          {
            auto sourceObject=GetResourceManager()->GetResource(source.second.resource,true);
            fprintf(stderr,"Metal descriptor overlay source: kind=%u resource=%s offset=%llu real=%d\n",
                source.first,ToStr(source.second.resource).c_str(),(unsigned long long)source.second.offset,
                sourceObject && sourceObject->m_Real);
          }
        }
        return false;
      }
      memcpy((byte *)native->contents() + offset, patched.data(), 24);
    }
    else
    {
      // Frame retirement only removes logical lookup. Earlier encoded work
      // retains Native descriptor bytes and referenced resources, as in the
      // D3D12/Vulkan descriptor shadow. Only initial restoration clears stale IDs.
      // A released logical entry does not own its old byte range forever. UE's
      // Shared upload arena can reuse it for ordinary shader constants without
      // allocating another descriptor generation. Clearing a historical field
      // here would corrupt those captured CPU writes (and pending GPU readers).
      if(m_DescriptorCoverage >= 65 && !initialRestore) continue;
      const uint64_t zero = 0;
      if(m_DescriptorCoverage >= 16)
      {
        // These entries are proven to retire before the first driver operation.
        // Preserve ordinary metadata but clear every declared GPU identity field;
        // a stale captured address or texture ID must never pass through to the GPU.
        std::map<uint32_t, uint64_t> fields;
        if(!DescriptorUEFields(entry.second.type, fields)) return false;
        for(const auto &field : fields)
          memcpy((byte *)native->contents() + offset + field.second, &zero, 8);
      }
      else memcpy((byte *)native->contents() + offset, &zero, 8);
    }
  }
  return true;
}

bool WrappedMTLDevice::PrepareDescriptorSlotShadow()
{
  if(m_DescriptorSlotShadow.empty() ||
     (m_DescriptorCoverage < 5 && !m_DescriptorGPUWrittenBuffers.empty())) return false;
  for(const DescriptorTable &layout : m_DescriptorTables)
  {
    if((layout.schema != 1 && !(m_DescriptorCoverage >= 7 && layout.schema == 2)) ||
       layout.stride != 24 || layout.count > DescriptorTableBufferLimit(m_DescriptorCoverage) / 24) return false;
    if(m_DescriptorCoverage >= 8 && m_DescriptorFrameBuffers.count(layout.buffer))
      continue;
    WrappedMTLObject *object = GetResourceManager()->GetResource(layout.buffer, true);
    if(!object || object->m_Type != eResBuffer || !object->m_Real) return false;
    MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)object);
    if(buffer->storageMode() != MTL::StorageModeShared || buffer->length() > DescriptorTableBufferLimit(m_DescriptorCoverage) ||
       layout.offset > buffer->length() || layout.count * layout.stride > buffer->length() - layout.offset)
      return false;
    if(!m_ReplayBufferInitialContents.count(layout.buffer))
      m_ReplayBufferInitialContents[layout.buffer] = bytebuf((byte *)buffer->contents(), buffer->length());
    const bytebuf &raw = m_ReplayBufferInitialContents[layout.buffer];
    if(raw.size() != buffer->length()) return false;
    for(uint64_t entry = 0; entry < layout.count; entry++)
    {
      const uint64_t offset = layout.offset + entry * layout.stride;
      if(m_DescriptorSlotShadow.count(make_rdcpair(layout.buffer, offset))) continue;
      uint64_t pointer = 0, texture = 0;
      memcpy(&pointer, raw.data() + offset, 8);
      memcpy(&texture, raw.data() + offset + 8, 8);
      if(pointer || (layout.schema != 2 && texture)) return false;
    }
  }
  std::set<ResourceId> buffers;
  for(const auto &entry : m_DescriptorSlotShadow)
  {
    buffers.insert(entry.first.first);
    bytebuf patched;
    if(entry.second.live)
    {
      auto raw = m_ReplayBufferInitialContents.find(entry.first.first);
      if(entry.second.data.size() != 24 || raw == m_ReplayBufferInitialContents.end() ||
         entry.first.second > raw->second.size() || 24 > raw->second.size() - entry.first.second ||
         memcmp(raw->second.data() + entry.first.second, entry.second.data.data(), 24) ||
         (!IsDescriptorPreludeRetirement(entry.first, entry.second) &&
          !PatchDescriptorSlot(entry.second, patched))) return false;
    }
  }
  // Vulkan explicitly permits stale, unused descriptor bindings after the source
  // resource is released. Here the leading matching-generation free prefix proves
  // non-consumption; keep the logical live state only to validate the captured frees.
  for(const auto &retirement : m_DescriptorPreludeRetirements)
  {
    auto slot = m_DescriptorSlotShadow.find(retirement.first);
    if(slot == m_DescriptorSlotShadow.end() ||
       !IsDescriptorPreludeRetirement(retirement.first, slot->second)) return false;
  }
  m_DescriptorSlotInitial = m_DescriptorSlotShadow;
  for(ResourceId id : buffers)
  {
    MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)GetResourceManager()->GetResource(id));
    if(!m_ReplayBufferInitialContents.count(id))
      m_ReplayBufferInitialContents[id] = bytebuf((byte *)buffer->contents(), buffer->length());
    m_ReplayCPUUpdatedBuffers.insert(id);
    if(!OverlayDescriptorSlotBuffer(id, true)) return false;
  }
  m_DescriptorShadowFrame = true;
  return true;
}

bool WrappedMTLDevice::RelocateDescriptorInlineShadow(ResourceId encoder, uint32_t stage,
    uint64_t index, rdcarray<byte> &data)
{
  const auto key = make_rdcpair(encoder, uint64_t(stage) << 32 | index);
  auto layout = m_DescriptorInlineShadow.find(key);
  if((stage != 0 && (m_DescriptorCoverage < 9 || stage > (m_DescriptorCoverage >= 66 ? 4U : 2U))) || layout == m_DescriptorInlineShadow.end() ||
     (layout->second.count ? layout->second.count * layout->second.stride : layout->second.stride) != data.size())
    return false;
  if(!layout->second.count)
  {
    if(m_DescriptorCoverage < 14 || !ValidInlineDrawConstants(stage, index, data.size()) ||
       !layout->second.sources.empty()) return false;
    if(index == 5)
    {
      uint32_t value = 0;
      memcpy(&value, data.data(), data.size());
      if(data.size() == 4 ? value != 0 : value > 2) return false;
    }
    m_DescriptorInlineShadow.erase(layout);
    return true;
  }
  for(uint64_t entry = 0; entry < layout->second.count; entry++)
  {
    uint64_t captured = 0, replacement = 0;
    memcpy(&captured, data.data() + entry * layout->second.stride, 8);
    auto source = layout->second.sources.find(entry);
    if(captured ? (source == layout->second.sources.end() ||
        !PatchDescriptorSource(source->second, captured, replacement)) :
        source != layout->second.sources.end())
      return false;
    if(!m_DescriptorPreflight)
      memcpy(data.data() + entry * layout->second.stride, &replacement, 8);
  }
  m_DescriptorInlineShadow.erase(layout);
  return true;
}

bool WrappedMTLDevice::ValidateDescriptorSlotFrame()
{
  m_DescriptorPreflight = true;
  m_DescriptorValidatedComputeResources.clear();
  m_DescriptorSubmissionSlots.clear();
  m_DescriptorSubmissionSnapshotOwners.clear();
  m_DescriptorInitialCPUValueOffsets.clear();
  m_DescriptorPartialCopySubmissions.clear(); m_DescriptorFrameBufferBirthOffsets.clear();
  m_DescriptorBackingAliasConsumers.clear();
  m_RetiredTextureAliasConsumers.clear();
  m_DescriptorSubmissionOrder.clear();
  m_ValidatedDescriptorBackingAliases.clear();
  m_DescriptorPreflightLiveBuffers.clear();
  m_DescriptorPreflightLiveTextures.clear();
  m_DescriptorPreflightLiveViews.clear();
  m_DescriptorPreflightLiveTables.clear();
  m_DescriptorPreflightAliasedBuffers.clear();
  m_DescriptorInlineShadow.clear();
  m_DescriptorGPUCopyExpected.clear();
  m_DescriptorDispatches.clear();
  bool success = true, consumed = false;
  std::set<ResourceId> liveBlits;
  std::map<ResourceId,uint32_t> blitDebugDepth;
  uint32_t commandCount = 0, dispatchCount = 0, copyCount = 0, drawCount = 0;
  uint32_t producerCount = 0, meshDrawCount = 0;
  uint64_t dispatchWork = 0, drawWork = 0;
  uint32_t plainCopyCount = 0; uint64_t plainCopyBytes = 0;
  ResourceId submissionQueue, currentCommand;
  std::set<ResourceId> commands, committed, completed;
  std::set<ResourceId> liveEncoders;
  // Match the replay core and D3D12/Vulkan: command identity and submission order
  // are independent of the order in which the application creates command buffers.
  std::map<ResourceId, ResourceId> encoderCommands;
  // Only explicit shader-producer annotations have encoder ownership. Ordinary
  // blit expectations must retain their existing global completion requirement.
  std::map<DescriptorSlotKey, ResourceId> gpuCopyEncoders;
  std::map<ResourceId, std::set<ResourceId>> resourceCommands, encoderResources;
  std::set<DescriptorSlotKey> borrowedSlots, freshCPUValues, writtenSlots;
  std::map<DescriptorSlotKey,std::set<ResourceId>> slotConsumers;
  std::set<ResourceId> opaqueTableWrites;
  auto viewBacking = [&](ResourceId resource) {
    auto future = m_DescriptorFrameViews.find(resource);
    ResourceId parent = future != m_DescriptorFrameViews.end() ? future->second :
        m_DescriptorCoverage >= 32 ? GetReplay()->GetBufferTextureSource(resource) : ResourceId();
    if(parent == ResourceId() && m_DescriptorCoverage >= 36)
    {
      auto textureParent = m_DescriptorFrameTextureViewParents.find(resource);
      parent = textureParent != m_DescriptorFrameTextureViewParents.end() ? textureParent->second :
          GetReplayTextureViewParent(resource);
    }
    return parent;
  };
  auto noteResource = [&](ResourceId resource, ResourceId encoder) {
    if(resource == ResourceId()) return;
    auto noteBacking = [&](ResourceId view) {
      ResourceId parent = viewBacking(view);
      if(parent == ResourceId()) return;
      if((m_DescriptorFrameViews.count(view) && !m_DescriptorPreflightLiveViews.count(view)) ||
         (m_DescriptorFrameTextures.count(view) && !m_DescriptorPreflightLiveTextures.count(view)) ||
         (m_DescriptorFrameBuffers.count(parent) && !m_DescriptorPreflightLiveBuffers.count(parent)) ||
         (m_DescriptorFrameTextures.count(parent) && !m_DescriptorPreflightLiveTextures.count(parent)) ||
         m_DescriptorPreflightAliasedBuffers.count(parent)) success = false;
      encoderResources[encoder].insert(parent);
      resourceCommands[parent].insert(encoderCommands[encoder]);
    };
    if(m_DescriptorPreflightAliasedBuffers.count(resource)) success = false;
    noteBacking(resource);
    encoderResources[encoder].insert(resource);
    resourceCommands[resource].insert(encoderCommands[encoder]);
    for(const auto &entry : m_DescriptorSlotShadow)
      if(entry.first.first == resource && entry.second.live)
      {
        if(m_DescriptorCoverage<64 || !entry.second.data.empty()) {
          borrowedSlots.insert(entry.first);
          slotConsumers[entry.first].insert(encoderCommands[encoder]);
        }
        for(const auto &source : entry.second.sources)
        {
          encoderResources[encoder].insert(source.second.resource);
          resourceCommands[source.second.resource].insert(encoderCommands[encoder]);
          noteBacking(source.second.resource);
        }
      }
  };
  auto isTable = [&](ResourceId resource) {
    for(const auto &table : m_DescriptorTables)
      if(table.buffer == resource) return true;
    return false;
  };
  auto noteSubmissionSlots = [&](ResourceId encoder) {
    // An encoded draw/dispatch owns its resource closure, as in D3D12/Vulkan.
    // Unrelated values may still be waiting for their source annotation when
    // this command buffer is submitted.
    for(const auto &slot : m_DescriptorSlotShadow)
      if(slot.second.live && encoderResources[encoder].count(slot.first.first) &&
         !(m_DescriptorCoverage >= 64 && slot.second.data.empty()))
        m_DescriptorSubmissionSlots[encoderCommands[encoder]].insert(slot.first);
  };
  auto validateBackingAlias = [&](ResourceId before, ResourceId after) {
    if(!isTable(before) && !isTable(after)) return true;
    if(m_DescriptorCoverage < 25 || m_DescriptorGPUWrittenBuffers.count(before) ||
       m_DescriptorGPUWrittenBuffers.count(after)) return false;
    bool knownSlot = false;
    for(const auto &entry : m_DescriptorSlotShadow)
      if(entry.first.first == before)
      {
        knownSlot = true;
        if(entry.second.live) return false;
      }
    if(isTable(before) && !knownSlot) return false;
    for(ResourceId command : resourceCommands[before])
      if(!committed.count(command)) return false;
    // Native resources stay retained. This retires the old logical backing only,
    // matching D3D12/Vulkan's independent descriptor shadow and GPU lifetime.
    m_ValidatedDescriptorBackingAliases.insert(make_rdcpair(before, after));
    m_DescriptorBackingAliasConsumers[make_rdcpair(before, after)] = resourceCommands[before];
    m_DescriptorPreflightAliasedBuffers.insert(before);
    return true;
  };
  auto validateRetiredTextureAlias = [&](ResourceId before, ResourceId after) {
    if(m_DescriptorCoverage < 65 || isTable(after)) return false;
    // Descriptor retirement and Native lifetime are separate, as in the D3D12
    // and Vulkan resource managers. No live logical source may name this backing.
    for(const auto &entry : m_DescriptorSlotShadow)
      if(entry.second.live)
      {
        for(const auto &source : entry.second.sources)
          if(source.second.resource == before || viewBacking(source.second.resource) == before) return false;
        for(const auto &source : entry.second.gpuCopySources)
          if(source.second.resource == before || viewBacking(source.second.resource) == before) return false;
      }
    for(ResourceId command : resourceCommands[before])
      if(!committed.count(command)) return false;
    m_RetiredTextureAliasConsumers[make_rdcpair(before, after)] = resourceCommands[before];
    m_DescriptorPreflightAliasedBuffers.insert(before);
    return true;
  };
  std::set<ResourceId> pendingWork, enqueued;
  rdcarray<ResourceId> submissionOrder, reservationOrder;
  std::map<ResourceId, uint64_t> encodedSignals, submittedSignals;
  std::map<ResourceId, std::map<ResourceId, uint64_t>> commandSignals, commandFirstSignals;
  uint32_t signalCount = 0;
  auto hasLiveEncoder = [&](ResourceId command) {
    for(const auto &entry : encoderCommands)
      if(entry.second == command) return true;
    return false;
  };
  auto inlineCompleteForCommand = [&](ResourceId command) {
    if(m_DescriptorCoverage < 65) return m_DescriptorInlineShadow.empty();
    for(const auto &entry : m_DescriptorInlineShadow)
    {
      auto owner = encoderCommands.find(entry.first.first);
      // Other command buffers may be in the middle of recording a typed binding.
      // Unknown or finished encoder ownership is never a valid exemption.
      if(owner == encoderCommands.end() || owner->second == command) return false;
    }
    return true;
  };
  auto gpuCopyCompleteForCommand = [&](ResourceId command) {
    if(m_DescriptorCoverage < 65) return m_DescriptorGPUCopyExpected.empty();
    for(const auto &entry : m_DescriptorGPUCopyExpected)
    {
      auto producer = gpuCopyEncoders.find(entry.first);
      if(producer == gpuCopyEncoders.end() || !liveEncoders.count(producer->second)) return false;
      auto owner = encoderCommands.find(producer->second);
      // Like the D3D12 command-list descriptor state, an annotation being
      // recorded by a different live encoder does not gate this submission.
      // Missing, ended, own-command or already submitted ownership still fails.
      if(owner == encoderCommands.end() || owner->second == command ||
         !commands.count(owner->second) || committed.count(owner->second)) return false;
    }
    return true;
  };
  auto validEncoderCommand = [&](ResourceId command) {
    return m_DescriptorCoverage < 10 ||
        (m_DescriptorCoverage >= 15 ? commands.count(command) && !committed.count(command) &&
                                     !hasLiveEncoder(command) :
                                     command == currentCommand && !committed.count(command));
  };
  struct FrameHeapRange { uint64_t begin, end; ResourceId buffer; };
  std::map<ResourceId, rdcarray<FrameHeapRange>> heapRanges;
  std::set<ResourceId> liveRenders, liveParallelRenders, knownEncoders;
  std::map<ResourceId, ResourceId> childParents, activeChildren;
  std::map<ResourceId,bool> renderAttachmentless;
  std::map<ResourceId,ResourceId> renderVisibilityBuffers;
  std::map<ResourceId,std::set<ResourceId>> colorWriteCommands;
  auto validateDrawableReads = [&](ResourceId encoder) {
    if(m_DescriptorCoverage<65)return true;
    for(ResourceId resource:encoderResources[encoder])
      if(m_DescriptorDrawableTextures.count(resource) && !HasReplayTextureInitialContents(resource)) {
        bool initialized=false;
        for(ResourceId writer:colorWriteCommands[resource])
          initialized |= writer==encoderCommands[encoder] || committed.count(writer)!=0;
        if(!initialized)return false;
      }
    return true;
  };
  std::map<ResourceId,uint32_t> capturedRenderIndirectOrdinals;
  std::map<ResourceId,std::set<ResourceId>> renderIndirectSources,renderIndirectWrites;
  // Use current Native allocation footprints; future placement resources use the
  // same device size/alignment queries and creation descriptors as the existing
  // heap preflight. No resource or command queue is created for this check.
  struct RenderIndirectFootprint
  {
    ResourceId owner;
    MTL::Heap *heap=NULL;
    MTL::Resource *nativeOwner=NULL;
    uint64_t begin=0,end=0,gpuAddress=0,length=0;
    bool buffer=false,valid=false;
  };
  auto indirectFootprint=[&](ResourceId resource) {
    RenderIndirectFootprint result;
    for(uint32_t depth=0;depth<8;depth++)
    {
      const ResourceId parent=viewBacking(resource);
      if(parent==ResourceId())break;
      if(parent==resource || depth==7)return result;
      resource=parent;
    }
    result.owner=resource;
    auto buffer=m_DescriptorFrameBuffers.find(resource);
    auto texture=m_DescriptorFrameTextures.find(resource);
    if(buffer!=m_DescriptorFrameBuffers.end() || texture!=m_DescriptorFrameTextures.end())
    {
      const ResourceId heap=buffer!=m_DescriptorFrameBuffers.end()?buffer->second.heap:texture->second.heap;
      result.buffer=buffer!=m_DescriptorFrameBuffers.end();
      if(heap==ResourceId()) { result.valid=true;return result; }
      auto object=GetResourceManager()->GetResource(heap,true);
      if(!object || object->m_Type!=eResHeap || !object->m_Real)return result;
      result.heap=Unwrap((WrappedMTLHeap *)object);
      MTL::SizeAndAlign layout={};
      if(result.buffer)
      {
        result.begin=buffer->second.offset;
        layout=Unwrap(this)->heapBufferSizeAndAlign(buffer->second.length,(MTL::ResourceOptions)buffer->second.options);
      }
      else
      {
        result.begin=texture->second.offset;
        MTL::TextureDescriptor *query(texture->second.descriptor);
        layout=Unwrap(this)->heapTextureSizeAndAlign(query);query->release();
      }
      if(!layout.size || result.begin>result.heap->size() || layout.size>result.heap->size()-result.begin)return result;
      result.end=result.begin+layout.size;result.valid=true;return result;
    }
    auto object=GetResourceManager()->GetResource(resource,true);
    if(!object || !object->m_Real || (object->m_Type!=eResBuffer && object->m_Type!=eResTexture))return result;
    auto native=GetMetalIndirectWriteFootprint(Unwrap((WrappedMTLResource *)object),object->m_Type==eResTexture);
    result.heap=native.heap.get();result.nativeOwner=native.owner.get();result.begin=native.begin;result.end=native.end;
    result.buffer=native.buffer;result.valid=native.valid;
    if(native.buffer && native.owner)
    {
      auto allocation=(MTL::Buffer *)native.owner.get();
      result.gpuAddress=allocation->gpuAddress();result.length=allocation->length();
    }
    return result;
  };
  auto validateIndirectPass=[&](ResourceId pass) {
    if(m_DescriptorCoverage<58)return true;
    bool valid=true;
    if(m_DescriptorCoverage>=59 && renderAttachmentless[pass] && !renderIndirectSources[pass].empty())
      valid &= !renderIndirectWrites[pass].empty();
    for(ResourceId source:renderIndirectSources[pass])
    {
      const auto argument=indirectFootprint(source);valid &= argument.valid && argument.buffer;
      for(ResourceId resource:renderIndirectWrites[pass])
      {
        const auto write=indirectFootprint(resource);
        valid &= write.valid && write.owner!=argument.owner;
        if(argument.nativeOwner && write.nativeOwner)valid &= argument.nativeOwner!=write.nativeOwner;
        if(argument.heap && write.heap==argument.heap)
          valid &= !(argument.begin<write.end && write.begin<argument.end);
        if(argument.gpuAddress && write.gpuAddress)
          valid &= !(argument.gpuAddress<write.gpuAddress+write.length &&
                     write.gpuAddress<argument.gpuAddress+argument.length);
      }
    }
    renderIndirectSources.erase(pass);renderIndirectWrites.erase(pass);return valid;
  };

  std::map<ResourceId, std::set<uint32_t>> deferredStores;
  std::map<ResourceId, std::map<uint32_t, MTL::PixelFormat>> renderTargets;
  std::map<ResourceId, MTL::PixelFormat> renderDepthTargets, renderStencilTargets;
  std::map<std::pair<ResourceId, bool>, ResourceId> initializedDepthCommands;
  std::map<ResourceId, std::set<uint64_t>> appliedInline;
  struct ComputeSnapshot
  {
    ResourceId pipeline;
    std::map<uint32_t, MetalPipe::BufferBinding> buffers;
    std::map<uint32_t, uint64_t> bytes;
  };
  std::map<ResourceId, ComputeSnapshot> computes;
  std::map<ResourceId, uint32_t> capturedIndirectOrdinals;
  std::map<rdcpair<ResourceId, uint32_t>, ComputeSnapshot> graphics;
  std::map<ResourceId, std::map<uint32_t, rdcarray<byte>>> drawConstants;
  std::set<ResourceId> indexBuffers, modifiedBuffers;
  struct CPUUpdate { ResourceId buffer; uint64_t start; bytebuf data; };
  rdcarray<CPUUpdate> cpuUpdates;
  std::map<ResourceId, bytebuf> indexContents, indexKnownBytes;
  std::set<ResourceId> frameIndexCandidates, copiedFrameIndices, opaqueWrites;
  std::map<ResourceId, std::set<ResourceId>> indexCopyCommands;
  struct IndexOperation {
    ResourceId buffer; uint64_t offset; bytebuf data;
    uint64_t count; uint64_t stride; int64_t baseVertex;
  };
  std::map<ResourceId, rdcarray<IndexOperation>> submissionIndexOperations;
  auto validateSubmissionIndices = [&](ResourceId command) {
    // Evaluate GPU-visible index data in submission order, not API encoding order.
    // A producer can be encoded later in the file while submitting before its consumer.
    for(const IndexOperation &op : submissionIndexOperations[command]) {
      if(!op.count) {
        const auto target = m_DescriptorFrameBuffers.find(op.buffer);
        if(target == m_DescriptorFrameBuffers.end() || op.offset > target->second.length ||
           op.data.size() > target->second.length - op.offset) return false;
        auto &contents = indexContents[op.buffer]; auto &known = indexKnownBytes[op.buffer];
        contents.resize(target->second.length);
        if(known.empty()) { known.resize(target->second.length); memset(known.data(), 0, known.size()); }
        memcpy(contents.data() + op.offset, op.data.data(), op.data.size());
        memset(known.data() + op.offset, 1, op.data.size());
      } else {
        const auto contents = indexContents.find(op.buffer), known = indexKnownBytes.find(op.buffer);
        if(contents == indexContents.end() || known == indexKnownBytes.end() ||
           op.offset > contents->second.size() || op.count > (contents->second.size() - op.offset) / op.stride)
          return false;
        for(uint64_t index = 0; index < op.count; index++) {
          uint32_t value = 0;
          for(uint64_t byte = 0; byte < op.stride; byte++)
            if(!known->second[op.offset + index * op.stride + byte]) return false;
          memcpy(&value, contents->second.data() + op.offset + index * op.stride, op.stride);
          const int64_t effective = int64_t(value) + op.baseVertex;
          if(effective < 0 || effective >= 65535) return false;
        }
      }
    }
    return true;
  };
  std::map<ResourceId, rdcarray<CPUUpdate>> submissionUpdates;
  rdcarray<ResourceId> capturedSubmissionOrder;
  std::map<ResourceId,std::set<ResourceId>> producerDestinations;
  std::map<ResourceId,std::set<DescriptorSlotKey>> producerSlots;
  if(m_DescriptorCoverage >= 50)
  {
    // Match core submission snapshot ownership without applying any Native data.
    // Shared updates are serialized after encoding and immediately before commit.
    ReadSerialiser ownership(m_FrameReader, Ownership::Nothing);
    ownership.SetVersion(m_SectionVersion); ownership.SetUserData(GetResourceManager());
    m_FrameReader->SetOffset(0);
    rdcarray<CPUUpdate> pending; rdcarray<uint64_t> pendingOffsets;
    uint64_t bytes = 0; uint32_t count = 0;
    while(!m_FrameReader->AtEnd() && !ownership.IsErrored() && success)
    {
      const uint64_t offset = m_FrameReader->GetOffset();
      MetalChunk chunk = ownership.ReadChunk<MetalChunk>();
      if(chunk == MetalChunk::MTLBuffer_InternalModifyCPUContents)
      {
        ResourceId buffer; uint64_t start = 0, size = 0; bytebuf data;
        ownership.Serialise("Buffer"_lit, buffer); ownership.Serialise("start"_lit, start);
        ownership.Serialise("size"_lit, size); ownership.Serialise("data"_lit, data);
        success = size == data.size() && size <= 64ULL * 1024 * 1024 - bytes && ++count <= 1024;
        if(success) { bytes += size; pending.push_back({buffer, start, data}); pendingOffsets.push_back(offset); }
      }
      else if(m_DescriptorCoverage >= 52 && (chunk == MetalChunk::MTLHeap_newBufferWithOffset ||
              chunk == MetalChunk::MTLDevice_newBufferWithLength))
      {
        ResourceId buffer;
        if(chunk == MetalChunk::MTLHeap_newBufferWithOffset) { ResourceId heap; ownership.Serialise("Heap"_lit, heap); }
        ownership.Serialise("Buffer"_lit, buffer);
        if(m_DescriptorFrameBuffers.count(buffer)) m_DescriptorFrameBufferBirthOffsets[buffer] = offset;
      }
      else if(m_DescriptorCoverage>=63 && chunk==MetalChunk::MTLBuffer_DescriptorSlotProducer)
      {
        ResourceId buffer,encoder,source; uint64_t targetOffset=0,sourceOffset=0;
        ownership.Serialise("buffer"_lit,buffer); ownership.Serialise("offset"_lit,targetOffset);
        ownership.Serialise("encoder"_lit,encoder); ownership.Serialise("source"_lit,source);
        ownership.Serialise("sourceOffset"_lit,sourceOffset);
        producerDestinations[encoder].insert(buffer);
        producerSlots[encoder].insert(make_rdcpair(buffer,targetOffset));
      }
      else if(chunk == MetalChunk::MTLCommandBuffer_commit)
      {
        ResourceId command; ownership.Serialise("CommandBuffer"_lit, command);
        success = !submissionUpdates.count(command);
        if(success) {
          submissionUpdates[command] = pending; pending.clear();
          capturedSubmissionOrder.push_back(command);
          if(m_DescriptorCoverage >= 51)
            for(uint64_t snapshot : pendingOffsets) m_DescriptorSubmissionSnapshotOwners[snapshot] = command;
          pendingOffsets.clear();
        }
      }
      else if(chunk == MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives ||
              chunk == MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced ||
              chunk == MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced_base ||
              (m_DescriptorCoverage>=58 && chunk==MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_indirect))
      {
        ResourceId encoder, buffer; MTL::PrimitiveType primitive; MTL::IndexType type; uint64_t indexCount = 0;
        ownership.Serialise("RenderCommandEncoder"_lit, encoder); ownership.Serialise("primitiveType"_lit, primitive);
        if(chunk!=MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_indirect)
          ownership.Serialise("indexCount"_lit, indexCount);
        ownership.Serialise("indexType"_lit, type);
        ownership.Serialise("indexBuffer"_lit, buffer);
        if(m_DescriptorFrameBuffers.count(buffer)) frameIndexCandidates.insert(buffer);
      }
      ownership.EndChunk();
    }
    success &= !ownership.IsErrored() && pending.empty();
    m_FrameReader->SetOffset(0);
  }
  auto readIndexUpload = [&](ResourceId source, ResourceId command, uint64_t offset, uint64_t size, bytebuf &data) {
    auto future = m_DescriptorFrameBuffers.find(source);
    auto object = GetResourceManager()->GetResource(source, true);
    MTL::Buffer *native = object && object->m_Type == eResBuffer ? Unwrap((WrappedMTLBuffer *)object) : NULL;
    const uint64_t length = future != m_DescriptorFrameBuffers.end() ? future->second.length : native ? native->length() : 0;
    const bool shared = future != m_DescriptorFrameBuffers.end() ?
        (future->second.options & 0xf0ULL) == MTL::ResourceStorageModeShared : native && native->storageMode() == MTL::StorageModeShared;
    if(!shared || !size || size > (m_DescriptorCoverage >= 52 ? DescriptorPlainCopyLimit(m_DescriptorCoverage) : 64 * 1024) || offset > length || size > length - offset ||
       modifiedBuffers.count(source) || m_DescriptorGPUWrittenBuffers.count(source) || isTable(source)) return false;
    data.resize(size); bytebuf known; known.resize(size); memset(known.data(), 0, size);
    auto initial = m_ReplayBufferInitialContents.find(source);
    if(initial != m_ReplayBufferInitialContents.end() && initial->second.size() == length) {
      memcpy(data.data(), initial->second.data() + offset, size); memset(known.data(), 1, size);
    } else if(future == m_DescriptorFrameBuffers.end() && native && native->contents() &&
              m_ReplayBuffersWithCreationContents.count(source)) {
      memcpy(data.data(), (byte *)native->contents() + offset, size); memset(known.data(), 1, size);
    }
    auto overlay = [&](ResourceId owner) {
      for(const CPUUpdate &update : submissionUpdates[owner]) if(update.buffer == source) {
        if(update.start > length || update.data.size() > length - update.start) return false;
        const uint64_t begin = RDCMAX(offset, update.start), end = RDCMIN(offset + size, update.start + update.data.size());
        if(begin < end) { memcpy(data.data() + begin - offset, update.data.data() + begin - update.start, end - begin); memset(known.data() + begin - offset, 1, end - begin); }
      }
      return true;
    };
    for(ResourceId prior : submissionOrder) if(!overlay(prior)) return false;
    if(!overlay(command)) return false;
    for(byte value : known) if(!value) return false;
    return true;
  };
  auto readVisibilityInitial = [&](ResourceId buffer, ResourceId command, uint64_t length, bytebuf &data) {
    if(modifiedBuffers.count(buffer) || m_DescriptorGPUWrittenBuffers.count(buffer) || isTable(buffer))
      return false;
    // Encoder creation can precede the submission carrying this new buffer's
    // initial CPU snapshot. Use captured commit order (as replay restoration
    // does), not the position of this pass in the encoded chunk stream. Accept
    // only a complete snapshot no later than the query submission.
    bool foundCommand = false;
    for(ResourceId owner : capturedSubmissionOrder)
    {
      for(const CPUUpdate &update : submissionUpdates[owner])
        if(update.buffer == buffer)
        {
          if(update.start || update.data.size() != length) return false;
          data = update.data;
        }
      if(owner == command) { foundCommand = true; break; }
    }
    return foundCommand && data.size() == length;
  };
  ReadSerialiser scan(m_FrameReader, Ownership::Nothing);
  scan.SetVersion(m_SectionVersion);
  scan.SetUserData(GetResourceManager());
  m_FrameReader->SetOffset(0);
  while(!m_FrameReader->AtEnd() && !scan.IsErrored() && success)
  {
    const uint64_t chunkOffset = m_FrameReader->GetOffset();
    MetalChunk chunk = scan.ReadChunk<MetalChunk>();
    if(m_DescriptorCoverage >= 14 && chunk == MetalChunk::MTLBuffer_InternalModifyCPUContents)
    {
      ResourceId buffer; uint64_t start = 0, size = 0; bytebuf data;
      scan.Serialise("Buffer"_lit, buffer); scan.Serialise("start"_lit, start);
      scan.Serialise("size"_lit, size); scan.Serialise("data"_lit, data);
      success = size == data.size();
      if(m_DescriptorCoverage >= 40)
      {
        auto future = m_DescriptorFrameBuffers.find(buffer);
        auto object = GetResourceManager()->GetResource(buffer, true);
        MTL::Buffer *native = object && object->m_Type == eResBuffer ? Unwrap((WrappedMTLBuffer *)object) : NULL;
        const uint64_t length = future != m_DescriptorFrameBuffers.end() ? future->second.length : native ? native->length() : 0;
        const bool shared = future != m_DescriptorFrameBuffers.end() ?
            (future->second.options & 0xf0ULL) == MTL::ResourceStorageModeShared :
            native && native->storageMode() == MTL::StorageModeShared;
        success &= length && shared && start <= length && size <= length - start &&
            (future == m_DescriptorFrameBuffers.end() || m_DescriptorPreflightLiveBuffers.count(buffer));
      }
      cpuUpdates.push_back({buffer, start, data});
      if(m_DescriptorCoverage >= 52) {
        const auto owner = m_DescriptorSubmissionSnapshotOwners.find(chunkOffset);
        if(owner == m_DescriptorSubmissionSnapshotOwners.end()) success = false;
        else { m_DescriptorPartialCopySubmissions[owner->second].chunks.push_back(chunkOffset);
          m_DescriptorPartialCopySubmissions[owner->second].buffers.insert(buffer); }
      }
    }
    else if(m_DescriptorCoverage >= 10 && (chunk == MetalChunk::MTLCommandQueue_commandBuffer ||
        chunk == MetalChunk::MTLCommandQueue_commandBufferWithDescriptor ||
        chunk == MetalChunk::MTLCommandQueue_commandBufferWithUnretainedReferences))
    {
      ResourceId queue, command;
      scan.Serialise("CommandQueue"_lit, queue); scan.Serialise("CommandBuffer"_lit, command);
      success = ++commandCount <= (m_DescriptorCoverage >= 15 ? 256U : 2U) && queue != ResourceId() && command != ResourceId() &&
          (submissionQueue == ResourceId() || submissionQueue == queue) &&
          (m_DescriptorCoverage >= 15 || currentCommand == ResourceId() || completed.count(currentCommand) ||
           (m_DescriptorCoverage >= 12 && committed.count(currentCommand))) && commands.insert(command).second;
      if(chunk == MetalChunk::MTLCommandQueue_commandBufferWithDescriptor)
      {
        bool retainedReferences = false; uint64_t errorOptions = 0;
        scan.Serialise("retainedReferences"_lit, retainedReferences); scan.Serialise("errorOptions"_lit, errorOptions);
        success &= errorOptions <= (uint64_t)MTL::CommandBufferErrorOptionEncoderExecutionStatus;
      }
      submissionQueue = queue; currentCommand = command;
      if(m_DescriptorCoverage >= 52) m_DescriptorPartialCopySubmissions[command].chunks.push_back(chunkOffset);
    }
    else if(m_DescriptorCoverage >= 15 && chunk == MetalChunk::MTLCommandBuffer_enqueue)
    {
      ResourceId command; scan.Serialise("CommandBuffer"_lit, command);
      success = commands.count(command) && !committed.count(command) &&
          !hasLiveEncoder(command) && enqueued.insert(command).second;
      if(success) reservationOrder.push_back(command);
      if(m_DescriptorCoverage >= 52) m_DescriptorPartialCopySubmissions[command].valid = false;
    }
    else if(m_DescriptorCoverage >= 10 && chunk == MetalChunk::MTLCommandBuffer_waitUntilCompleted)
    {
      ResourceId command; scan.Serialise("CommandBuffer"_lit, command);
      success = (m_DescriptorCoverage >= 15 ? !hasLiveEncoder(command) :
          command == currentCommand && liveEncoders.empty() && liveRenders.empty() && liveBlits.empty()) &&
          committed.count(command) && inlineCompleteForCommand(command) &&
          m_DescriptorGPUCopyExpected.empty() && completed.insert(command).second;
      if(success)
      {
        consumed = m_DescriptorCoverage >= 15 && !pendingWork.empty();
        // A queue completion covers the prefix ending at this submission, not later work.
        if(m_DescriptorCoverage >= 15)
        {
          for(ResourceId prior : submissionOrder)
          {
            completed.insert(prior);
            if(prior == command) break;
          }
        }
        else if(m_DescriptorCoverage >= 12) completed.insert(committed.begin(), committed.end());
      }
    }
    else if(chunk == MetalChunk::MTLCommandBuffer_encodeMetalFXSpatial)
    {
      ResourceId command, colour, output, fence;
      rdcarray<uint64_t> p;
      scan.Serialise("CommandBuffer"_lit, command); scan.Serialise("colour"_lit, colour);
      scan.Serialise("output"_lit, output); scan.Serialise("fence"_lit, fence);
      scan.Serialise("parameters"_lit, p);
      success = commands.count(command) && !committed.count(command) && !hasLiveEncoder(command) &&
          colour != ResourceId() && output != ResourceId() && colour != output && p.size() == 9;
      if(success)
        success = p[0] && p[1] && p[2] && p[3] && p[0] <= 16384 && p[1] <= 16384 &&
            p[2] <= 16384 && p[3] <= 16384 && p[2] >= p[0] && p[3] >= p[1] && p[6] <= 2 &&
            p[7] && p[8] && p[7] <= p[0] && p[8] <= p[1];
      for(ResourceId texture : {colour, output})
      {
        auto future = m_DescriptorFrameTextures.find(texture);
        WrappedMTLObject *object = GetResourceManager()->GetResource(texture, true);
        success &= future != m_DescriptorFrameTextures.end() ? m_DescriptorPreflightLiveTextures.count(texture) != 0 :
            object && object->m_Type == eResTexture && object->m_Real && object->m_Device == this;
        resourceCommands[texture].insert(command);
      }
      if(fence != ResourceId())
      {
        auto object = GetResourceManager()->GetResource(fence, true);
        success &= object && object->m_Type == eResFence && object->m_Real && object->m_Device == this;
      }
      // The copy-only dependency shortcut cannot replay opaque MetalFX work.
      if(m_DescriptorCoverage >= 52) m_DescriptorPartialCopySubmissions[command].valid = false;
    }
    else if(m_DescriptorCoverage >= 19 && chunk == MetalChunk::MTLCommandBuffer_encodeSignalEvent)
    {
      ResourceId command, event; uint64_t value = 0;
      scan.Serialise("CommandBuffer"_lit, command); scan.Serialise("event"_lit, event);
      scan.Serialise("value"_lit, value);
      if(m_DescriptorCoverage >= 65)
      {
        // Preserve a preflighted earlier signal submission at a partial event boundary.
        // Enqueue reservations, waits and non-copy encoders still invalidate this plan.
        auto &plan = m_DescriptorPartialCopySubmissions[command];
        plan.chunks.push_back(chunkOffset);
        plan.signals++;
      }
      else if(m_DescriptorCoverage >= 52) m_DescriptorPartialCopySubmissions[command].valid = false;
      WrappedMTLObject *object = GetResourceManager()->GetResource(event, true);
      success = ++signalCount <= 256 && commands.count(command) && !committed.count(command) &&
          !hasLiveEncoder(command) && inlineCompleteForCommand(command) &&
          m_DescriptorGPUCopyExpected.empty() && object && object->m_Type == eResEvent &&
          object->m_Real && object->m_Device == this;
      if(success)
      {
        WrappedMTLEvent *timeline = (WrappedMTLEvent *)object;
        success = timeline->AliasRoot() == timeline;
        const uint64_t previous = encodedSignals.count(event) ? encodedSignals[event] :
                                  timeline->GetInitialHostValue();
        success &= value > previous;
        if(success)
        {
          encodedSignals[event] = value;
          if(!commandFirstSignals[command].count(event)) commandFirstSignals[command][event] = value;
          commandSignals[command][event] = value;
        }
      }
      if(!success && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal signal proof: command=%s event=%s value=%llu known=%d committed=%d liveEncoder=%d inline=%zu expected=%zu object=%d type=%u device=%d count=%u\n",
            ToStr(command).c_str(),ToStr(event).c_str(),(unsigned long long)value,
            int(commands.count(command)),int(committed.count(command)),hasLiveEncoder(command),
            m_DescriptorInlineShadow.size(),m_DescriptorGPUCopyExpected.size(),object!=NULL,
            object?uint32_t(object->m_Type):0,object && object->m_Device==this,signalCount);
    }
    else if(m_DescriptorCoverage >= 8 &&
       (chunk == MetalChunk::MTLDevice_newBufferWithLength || chunk == MetalChunk::MTLDevice_newBufferWithBytes))
    {
      ResourceId buffer; bytebuf initial; uint64_t length = 0, options = 0;
      scan.Serialise("Buffer"_lit, buffer); scan.Serialise("initialData"_lit, initial);
      scan.Serialise("length"_lit, length); scan.Serialise("options"_lit, options);
      auto future = m_DescriptorFrameBuffers.find(buffer);
      success = (m_DescriptorCoverage >= 24 || !consumed) && future != m_DescriptorFrameBuffers.end() && future->second.length == length &&
          future->second.options == options && !GetResourceManager()->HasResource(buffer) &&
          m_DescriptorPreflightLiveBuffers.insert(buffer).second;
    }
    else if(m_DescriptorCoverage >= 11 && chunk == MetalChunk::MTLHeap_newBufferWithOffset)
    {
      ResourceId heap, buffer; uint64_t length = 0, options = 0, offset = 0;
      scan.Serialise("Heap"_lit, heap); scan.Serialise("Buffer"_lit, buffer);
      scan.Serialise("length"_lit, length); scan.Serialise("options"_lit, options); scan.Serialise("offset"_lit, offset);
      auto future = m_DescriptorFrameBuffers.find(buffer);
      WrappedMTLObject *object = GetResourceManager()->GetResource(heap, true);
      success = (m_DescriptorCoverage >= 24 || !consumed) && future != m_DescriptorFrameBuffers.end() && future->second.heap == heap &&
          future->second.length == length && future->second.options == options && future->second.offset == offset &&
          !GetResourceManager()->HasResource(buffer) && m_DescriptorPreflightLiveBuffers.insert(buffer).second &&
          object && object->m_Type == eResHeap && object->m_Real;
      if(success)
      {
        WrappedMTLHeap *parent = (WrappedMTLHeap *)object;
        MTL::Heap *native = Unwrap(parent);
        const MTL::SizeAndAlign layout = Unwrap(this)->heapBufferSizeAndAlign(length, (MTL::ResourceOptions)options);
        const MTL::StorageMode storage = (options & MTL::ResourceStorageModePrivate) ?
            MTL::StorageModePrivate : MTL::StorageModeShared;
        success = native->type() == MTL::HeapTypePlacement && native->storageMode() == storage &&
            (storage == MTL::StorageModeShared ||
             (m_DescriptorCoverage >= 23 && native->hazardTrackingMode() == MTL::HazardTrackingModeTracked)) &&
            layout.size && layout.align && offset % layout.align == 0 && offset <= native->size() &&
            layout.size <= native->size() - offset;
        if(success)
        {
          const uint64_t end = offset + layout.size;
          const bool backgroundOverlap = parent->HasPlacementOverlap(offset, end);
          if(m_DescriptorCoverage >= 25 && backgroundOverlap)
            for(const auto &description : GetReplay()->GetBuffers())
            {
              auto previous = GetResourceManager()->GetResource(description.resourceId, true);
              MTL::Buffer *old = previous && previous->m_Type == eResBuffer ?
                  Unwrap((WrappedMTLBuffer *)previous) : NULL;
              if(!old || old->heap() != native) continue;
              const auto oldLayout = Unwrap(this)->heapBufferSizeAndAlign(old->length(), old->resourceOptions());
              if(offset < old->heapOffset() + oldLayout.size && old->heapOffset() < end)
                success &= validateBackingAlias(description.resourceId, buffer);
            }
          if(m_DescriptorCoverage >= 65 && backgroundOverlap && storage == MTL::StorageModePrivate)
            for(const auto &description : GetReplay()->GetTextures())
            {
              auto previous = GetResourceManager()->GetResource(description.resourceId, true);
              MTL::Texture *old = previous && previous->m_Type == eResTexture ?
                  Unwrap((WrappedMTLTexture *)previous) : NULL;
              if(!old || old->heap() != native || viewBacking(description.resourceId) != ResourceId()) continue;
              auto footprint = GetMetalIndirectWriteFootprint(old, true);
              if(!footprint.valid || footprint.buffer) { success = false; break; }
              if(offset < footprint.end && footprint.begin < end)
                success &= footprint.end - footprint.begin <= DescriptorPlacementAliasLimit() &&
                    validateRetiredTextureAlias(description.resourceId, buffer);
            }
          success &= !backgroundOverlap || (m_DescriptorCoverage >= 18 &&
              length <= DescriptorPlacementAliasLimit() &&
              (m_DescriptorCoverage >= 24 ||
               (committed.size() == commands.size() &&
                (m_DescriptorCoverage >= 22 || completed.size() == commands.size()) &&
                liveEncoders.empty() && liveRenders.empty() && liveBlits.empty() &&
                m_DescriptorInlineShadow.empty() && m_DescriptorGPUCopyExpected.empty())) &&
              parent->CanImplicitlyAliasBuffers(offset, end, buffer));
          for(const auto &range : heapRanges[heap])
            if(offset < range.end && range.begin < end &&
               !m_DescriptorPreflightAliasedBuffers.count(range.buffer))
            {
              // Placement buffers may share storage while both logical objects and their
              // descriptors remain live. This differs from explicit makeAliasable retirement.
              // v17/18 require captured CPU completion; v22 uses native tracked-heap ordering.
              auto previous = m_DescriptorFrameBuffers.find(range.buffer);
              bool implicit = m_DescriptorCoverage >= 17 &&
                  previous != m_DescriptorFrameBuffers.end() &&
                  length <= DescriptorPlacementAliasLimit() &&
                  previous->second.length <= DescriptorPlacementAliasLimit() &&
                  (m_DescriptorCoverage >= 24 ||
                   (committed.size() == commands.size() &&
                    (m_DescriptorCoverage >= 22 || completed.size() == commands.size()) &&
                    liveEncoders.empty() && liveRenders.empty() && liveBlits.empty() &&
                    m_DescriptorInlineShadow.empty() && m_DescriptorGPUCopyExpected.empty()));
              if(isTable(range.buffer) || isTable(buffer))
                implicit &= storage == MTL::StorageModeShared && validateBackingAlias(range.buffer, buffer);
              auto texture = m_DescriptorFrameTextures.find(range.buffer);
              if(!implicit && m_DescriptorCoverage >= 65 && storage == MTL::StorageModePrivate &&
                 texture != m_DescriptorFrameTextures.end())
                implicit = length <= DescriptorPlacementAliasLimit() &&
                    range.end - range.begin <= DescriptorPlacementAliasLimit() &&
                    validateRetiredTextureAlias(range.buffer, buffer);
              if(!implicit) success = false;
            }
          if(success) heapRanges[heap].push_back({offset, end, buffer});
        }
      }
    }
    else if(m_DescriptorCoverage >= 31 && chunk == MetalChunk::MTLBuffer_newTextureWithDescriptor)
    {
      ResourceId buffer, texture; RDMTL::TextureDescriptor descriptor;
      uint64_t offset = 0, bytesPerRow = 0;
      scan.Serialise("Buffer"_lit, buffer); scan.Serialise("Texture"_lit, texture);
      scan.Serialise("descriptor"_lit, descriptor); scan.Serialise("offset"_lit, offset);
      scan.Serialise("bytesPerRow"_lit, bytesPerRow);
      auto recorded = m_DescriptorFrameViews.find(texture);
      auto parent = m_DescriptorFrameBuffers.find(buffer);
      auto object=GetResourceManager()->GetResource(buffer,true);
      MTL::Buffer *native=object && object->m_Type==eResBuffer ? Unwrap((WrappedMTLBuffer *)object) : NULL;
      auto initial=m_ReplayBufferInitialContents.find(buffer);
      const bool backgroundParent=m_DescriptorCoverage>=65 && parent==m_DescriptorFrameBuffers.end() &&
          native && native->storageMode()==MTL::StorageModePrivate && native->length()<=128ULL*1024*1024 &&
          initial!=m_ReplayBufferInitialContents.end() && initial->second.size()==native->length();
      success = recorded != m_DescriptorFrameViews.end() && recorded->second == buffer &&
          ((parent != m_DescriptorFrameBuffers.end() && m_DescriptorPreflightLiveBuffers.count(buffer)) || backgroundParent) &&
          !m_DescriptorPreflightAliasedBuffers.count(buffer) && !GetResourceManager()->HasResource(texture) &&
          m_DescriptorPreflightLiveViews.insert(texture).second;
      if(success)
      {
        const MTL::StorageMode storage = backgroundParent ? native->storageMode() :
            (parent->second.options & MTL::ResourceStorageModePrivate) ? MTL::StorageModePrivate : MTL::StorageModeShared;
        const uint64_t length=backgroundParent?native->length():parent->second.length;
        success = ValidateMetalBufferTexture(Unwrap(this), descriptor, length, storage, offset, bytesPerRow);
      }
      if(!success && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal frame buffer view: view=%s parent=%s future=%d live=%d aliased=%d width=%llu pitch=%llu offset=%llu\n",
            ToStr(texture).c_str(),ToStr(buffer).c_str(),parent!=m_DescriptorFrameBuffers.end(),
            m_DescriptorPreflightLiveBuffers.count(buffer)!=0,m_DescriptorPreflightAliasedBuffers.count(buffer)!=0,
            (unsigned long long)descriptor.width,(unsigned long long)bytesPerRow,(unsigned long long)offset);
    }
    else if(m_DescriptorCoverage >= 21 && chunk == MetalChunk::MTLHeap_newTextureWithOffset)
    {
      ResourceId heap, texture; RDMTL::TextureDescriptor descriptor; uint64_t offset = 0;
      scan.Serialise("Heap"_lit, heap); scan.Serialise("Texture"_lit, texture);
      scan.Serialise("descriptor"_lit, descriptor); scan.Serialise("offset"_lit, offset);
      auto future = m_DescriptorFrameTextures.find(texture);
      auto object = GetResourceManager()->GetResource(heap, true);
      success = (m_DescriptorCoverage >= 24 || !consumed) && future != m_DescriptorFrameTextures.end() && future->second.heap == heap &&
          future->second.offset == offset && ValidDescriptorFrameTexture(descriptor, m_DescriptorCoverage) &&
          !GetResourceManager()->HasResource(texture) && m_DescriptorPreflightLiveTextures.insert(texture).second &&
          object && object->m_Type == eResHeap && object->m_Real;
      if(success)
      {
        auto parent = (WrappedMTLHeap *)object; MTL::Heap *native = Unwrap(parent);
        MTL::TextureDescriptor *query(descriptor);
        const MTL::SizeAndAlign layout = Unwrap(this)->heapTextureSizeAndAlign(query); query->release();
        success = native->type() == MTL::HeapTypePlacement && native->storageMode() == MTL::StorageModePrivate &&
            native->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
            layout.size && layout.align && offset % layout.align == 0 && offset <= native->size() &&
            layout.size <= native->size() - offset;
        if(success)
        {
          const uint64_t end = offset + layout.size;
          success = !parent->HasPlacementOverlap(offset, end) ||
              (m_DescriptorCoverage >= 24 && parent->CanImplicitlyAliasBuffers(offset, end, texture));
          for(const auto &range : heapRanges[heap])
            if(offset < range.end && range.begin < end)
            {
              auto previous = m_DescriptorFrameBuffers.find(range.buffer);
              bool implicit = m_DescriptorCoverage >= 24 && previous != m_DescriptorFrameBuffers.end() &&
                  previous->second.options == (MTL::ResourceStorageModePrivate | MTL::ResourceHazardTrackingModeTracked);
              for(const auto &table : m_DescriptorTables)
                if(table.buffer == range.buffer) implicit = false;
              success &= implicit;
            }
          heapRanges[heap].push_back({offset, end, texture});
        }
      }
    }
    else if(m_DescriptorCoverage >= 26 &&
        chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset)
    {
      ResourceId source, view; MTL::PixelFormat format; MTL::TextureType type;
      NS::Range levels(0, 0), slices(0, 0); MTL::TextureSwizzleChannels swizzle;
      scan.Serialise("Source"_lit, source); scan.Serialise("View"_lit, view);
      scan.Serialise("format"_lit, format); scan.Serialise("type"_lit, type);
      scan.Serialise("levels"_lit, levels); scan.Serialise("slices"_lit, slices);
      scan.Serialise("swizzle"_lit, swizzle);
      auto recorded = m_DescriptorFrameTextureViewParents.find(view);
      auto parent = m_DescriptorFrameTextures.find(source);
      success = recorded != m_DescriptorFrameTextureViewParents.end() && recorded->second == source &&
          parent != m_DescriptorFrameTextures.end() && !m_DescriptorFrameTextureViewParents.count(source) &&
          m_DescriptorPreflightLiveTextures.count(source) && !GetResourceManager()->HasResource(view) &&
          format == parent->second.descriptor.pixelFormat && type == MTL::TextureType2D &&
          !levels.location && levels.length == 1 && !slices.location && slices.length == 1 &&
          swizzle.red == MTL::TextureSwizzleRed && swizzle.green == MTL::TextureSwizzleGreen &&
          swizzle.blue == MTL::TextureSwizzleBlue && swizzle.alpha == MTL::TextureSwizzleAlpha &&
          m_DescriptorPreflightLiveTextures.insert(view).second;
    }
    else if(m_DescriptorCoverage >= 13 && chunk == MetalChunk::MTLBuffer_makeAliasable)
    {
      ResourceId buffer; scan.Serialise("Buffer"_lit, buffer);
      auto future = m_DescriptorFrameBuffers.find(buffer);
      success = future != m_DescriptorFrameBuffers.end() && future->second.heap != ResourceId() &&
          m_DescriptorPreflightLiveBuffers.count(buffer) &&
          !m_DescriptorPreflightAliasedBuffers.count(buffer) &&
          committed.size() == commands.size() && completed.size() == commands.size() &&
          liveEncoders.empty() && liveRenders.empty() && liveBlits.empty() &&
          m_DescriptorInlineShadow.empty() && m_DescriptorGPUCopyExpected.empty();
      // Retire logical descriptors before retiring the heap allocation. A captured VA can be
      // identical for the replacement allocation, so ResourceId remains authoritative.
      for(const auto &entry : m_DescriptorSlotShadow)
        if(entry.second.live)
        {
          if(entry.first.first == buffer) success = false;
          for(const auto &source : entry.second.sources)
            if(source.second.resource == buffer ||
               viewBacking(source.second.resource) == buffer) success = false;
          for(const auto &source : entry.second.gpuCopySources)
            if(source.second.resource == buffer ||
               viewBacking(source.second.resource) == buffer) success = false;
        }
      if(success) m_DescriptorPreflightAliasedBuffers.insert(buffer);
    }
    else if(m_DescriptorCoverage >= 8 && chunk == MetalChunk::MTLBuffer_DeclareDescriptorTable)
    {
      ResourceId buffer; uint64_t offset = 0, count = 0, stride = 0; uint32_t schema = 0;
      scan.Serialise("buffer"_lit, buffer); scan.Serialise("offset"_lit, offset);
      scan.Serialise("count"_lit, count); scan.Serialise("stride"_lit, stride); scan.Serialise("schema"_lit, schema);
      auto future = m_DescriptorFrameBuffers.find(buffer);
      success = m_DescriptorPreflightLiveBuffers.count(buffer) && future != m_DescriptorFrameBuffers.end() &&
          (future->second.options & 0xf0ULL) == MTL::ResourceStorageModeShared &&
          (schema == 1 || schema == 2) && stride == 24 && offset <= future->second.length &&
          count * stride <= future->second.length - offset;
      if(success) m_DescriptorPreflightLiveTables.insert(buffer);
    }
    else if(m_DescriptorCoverage >= 8 && chunk == MetalChunk::MTLBuffer_DeclareDescriptorGPUWrites)
    {
      ResourceId buffer; scan.Serialise("buffer"_lit, buffer);
      success = m_DescriptorPreflightLiveBuffers.count(buffer);
    }
    else if(chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoder ||
       chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDispatchType ||
       chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDescriptor)
    {
      ResourceId command, encoder;
      scan.Serialise("CommandBuffer"_lit, command);
      scan.Serialise("ComputeCommandEncoder"_lit, encoder);
      bool validPass = true;
      if(m_DescriptorCoverage >= 54)
      {
        MTL::DispatchType dispatchType;
        scan.Serialise("dispatchType"_lit, dispatchType);
        validPass = dispatchType == MTL::DispatchTypeSerial || dispatchType == MTL::DispatchTypeConcurrent;
        if(chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDescriptor && scan.VersionAtLeast(0xE))
        {
          rdcarray<RDMTL::ComputePassSampleBufferAttachmentDescriptor> attachments;
          scan.Serialise("attachments"_lit, attachments);
          validPass &= ValidateMetalComputePassCounters(this, attachments);
        }
      }
      success = validPass && encoder != ResourceId() && liveEncoders.insert(encoder).second &&
          (m_DescriptorCoverage < 20 || knownEncoders.insert(encoder).second) && validEncoderCommand(command);
      if(success) encoderCommands[encoder] = command;
      if(m_DescriptorCoverage >= 52) m_DescriptorPartialCopySubmissions[command].valid = false;
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_endEncoding)
    {
      ResourceId encoder; scan.Serialise("ComputeCommandEncoder"_lit, encoder);
      success = liveEncoders.erase(encoder) == 1;
      if(success) encoderCommands.erase(encoder);
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_setComputePipelineState)
    {
      ResourceId encoder, pipeline;
      scan.Serialise("ComputeCommandEncoder"_lit, encoder); scan.Serialise("pipeline"_lit, pipeline);
      success = liveEncoders.count(encoder) && pipeline != ResourceId();
      computes[encoder].pipeline = pipeline;
      if(m_DescriptorCoverage >= 52) m_DescriptorPartialCopySubmissions[encoderCommands[encoder]].valid = false;
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_setBuffer)
    {
      ResourceId encoder, buffer; uint64_t offset = 0, index = 0;
      scan.Serialise("ComputeCommandEncoder"_lit, encoder); scan.Serialise("buffer"_lit, buffer);
      scan.Serialise("offset"_lit, offset); scan.Serialise("index"_lit, index);
      WrappedMTLObject *object = GetResourceManager()->GetResource(buffer, true);
      auto future = m_DescriptorFrameBuffers.find(buffer);
      uint64_t length = 0;
      if(m_DescriptorCoverage >= 8 && future != m_DescriptorFrameBuffers.end() &&
         m_DescriptorPreflightLiveBuffers.count(buffer)) length = future->second.length;
      else if(object && object->m_Type == eResBuffer && object->m_Real)
        length = Unwrap((WrappedMTLBuffer *)object)->length();
      if(!liveEncoders.count(encoder) || index >= 31 || !length ||
         m_DescriptorPreflightAliasedBuffers.count(buffer))
        success = false;
      else
      {
        success = offset < length;
        MetalPipe::BufferBinding binding;
        binding.resourceId = buffer; binding.byteOffset = offset;
        binding.byteSize = offset < length ? length - offset : 0;
        computes[encoder].buffers[(uint32_t)index] = binding;
        computes[encoder].bytes.erase((uint32_t)index);
        appliedInline[encoder].erase(index);
        noteResource(buffer, encoder);
      }
    }
    else if(m_DescriptorCoverage >= 22 &&
        (chunk == MetalChunk::MTLComputeCommandEncoder_useHeap ||
         chunk == MetalChunk::MTLComputeCommandEncoder_useHeaps ||
         chunk == MetalChunk::MTLRenderCommandEncoder_useHeap ||
         chunk == MetalChunk::MTLRenderCommandEncoder_useHeap_stages ||
         chunk == MetalChunk::MTLRenderCommandEncoder_useHeaps ||
         chunk == MetalChunk::MTLRenderCommandEncoder_useHeaps_stages))
    {
      const bool compute = chunk == MetalChunk::MTLComputeCommandEncoder_useHeap ||
                           chunk == MetalChunk::MTLComputeCommandEncoder_useHeaps;
      const bool array = chunk == MetalChunk::MTLComputeCommandEncoder_useHeaps ||
                         chunk == MetalChunk::MTLRenderCommandEncoder_useHeaps ||
                         chunk == MetalChunk::MTLRenderCommandEncoder_useHeaps_stages;
      const bool stages = chunk == MetalChunk::MTLRenderCommandEncoder_useHeap_stages ||
                          chunk == MetalChunk::MTLRenderCommandEncoder_useHeaps_stages;
      ResourceId encoder; rdcarray<ResourceId> heaps;
      scan.Serialise(compute ? "ComputeCommandEncoder"_lit : "RenderCommandEncoder"_lit, encoder);
      scan.Serialise("heaps"_lit, heaps);
      success = (compute ? liveEncoders.count(encoder) : liveRenders.count(encoder)) &&
          heaps.size() <= 32 && (array || heaps.size() == 1);
      if(compute)
      {
        bool arrayVariant = false; scan.Serialise("arrayVariant"_lit, arrayVariant);
        success &= arrayVariant == array;
      }
      else
      {
        uint64_t value = 0; scan.Serialise("stagesValue"_lit, value);
        success &= stages ? value && !(value & ~3ULL) : value == (uint64_t)MTL::RenderStageVertex;
      }
      std::set<ResourceId> unique;
      for(ResourceId heap : heaps)
      {
        auto object = GetResourceManager()->GetResource(heap, true);
        const bool first = unique.insert(heap).second;
        // Residency declarations may repeat the same heap. Preserve the captured array;
        // repeats do not create resources or imply additional writes or ownership.
        success &= heap != ResourceId() && (first || m_DescriptorCoverage >= 65) && object &&
            object->m_Type == eResHeap && object->m_Real && object->m_Device == this;
        if(success) success &= Unwrap((WrappedMTLHeap *)object)->hazardTrackingMode() == MTL::HazardTrackingModeTracked;
        if(!success && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal heap residency proof: encoder=%s compute=%d live=%d heap=%s unique=%zu count=%zu object=%d type=%u real=%d sameDevice=%d hazard=%llu\n",
              ToStr(encoder).c_str(),compute,compute?int(liveEncoders.count(encoder)):int(liveRenders.count(encoder)),
              ToStr(heap).c_str(),unique.size(),heaps.size(),object!=NULL,object?uint32_t(object->m_Type):0,
              object && object->m_Real,object && object->m_Device==this,
              (unsigned long long)(object && object->m_Type==eResHeap && object->m_Real ?
                Unwrap((WrappedMTLHeap *)object)->hazardTrackingMode():~0ULL));
      }
    }
    else if(m_DescriptorCoverage >= 13 &&
        (chunk == MetalChunk::MTLComputeCommandEncoder_useResource ||
         chunk == MetalChunk::MTLRenderCommandEncoder_useResource ||
         chunk == MetalChunk::MTLRenderCommandEncoder_useResource_stages ||
         chunk == MetalChunk::MTLComputeCommandEncoder_useResources ||
         chunk == MetalChunk::MTLRenderCommandEncoder_useResources ||
         chunk == MetalChunk::MTLRenderCommandEncoder_useResources_stages))
    {
      const bool compute = chunk == MetalChunk::MTLComputeCommandEncoder_useResource ||
                           chunk == MetalChunk::MTLComputeCommandEncoder_useResources;
      const bool array = chunk == MetalChunk::MTLComputeCommandEncoder_useResources ||
                         chunk == MetalChunk::MTLRenderCommandEncoder_useResources ||
                         chunk == MetalChunk::MTLRenderCommandEncoder_useResources_stages;
      ResourceId encoder;
      if(compute) scan.Serialise("ComputeCommandEncoder"_lit, encoder);
      else scan.Serialise("RenderCommandEncoder"_lit, encoder);
      rdcarray<ResourceId> resources;
      if(array) scan.Serialise("resources"_lit, resources);
      else { ResourceId resource; scan.Serialise("resource"_lit, resource); resources.push_back(resource); }
      success = compute ? liveEncoders.count(encoder) != 0 : liveRenders.count(encoder) != 0;
      for(ResourceId resource : resources)
        success &= resource != ResourceId() && !m_DescriptorPreflightAliasedBuffers.count(resource) &&
            (!m_DescriptorFrameBuffers.count(resource) || m_DescriptorPreflightLiveBuffers.count(resource)) &&
            (!m_DescriptorFrameTextures.count(resource) || m_DescriptorPreflightLiveTextures.count(resource));
      for(ResourceId resource : resources) noteResource(resource, encoder);
      if(m_DescriptorCoverage >= 14)
      {
        uint64_t usage = 0; scan.Serialise("usageValue"_lit, usage);
        if(usage & MTL::ResourceUsageWrite) {
          if(!compute && m_DescriptorCoverage>=58)
          {
            const ResourceId pass=childParents.count(encoder)?childParents[encoder]:encoder;
            renderIndirectWrites[pass].insert(resources.begin(),resources.end());
          }
          if(m_DescriptorCoverage>=64)
            for(ResourceId resource:resources)
              if(m_DescriptorGPUWrittenBuffers.count(resource) &&
                 !producerDestinations[encoder].count(resource)) opaqueTableWrites.insert(resource);
          modifiedBuffers.insert(resources.begin(), resources.end());
          opaqueWrites.insert(resources.begin(), resources.end());
        }
      }
    }
    else if(m_DescriptorCoverage >= 9 &&
        (chunk == MetalChunk::MTLCommandBuffer_renderCommandEncoderWithDescriptor ||
         (m_DescriptorCoverage >= 20 && chunk == MetalChunk::MTLCommandBuffer_parallelRenderCommandEncoderWithDescriptor)))
    {
      const bool parallel = chunk == MetalChunk::MTLCommandBuffer_parallelRenderCommandEncoderWithDescriptor;
      ResourceId command, encoder; RDMTL::RenderPassDescriptor pass;
      scan.Serialise("CommandBuffer"_lit, command);
      if(parallel) scan.Serialise("ParallelRenderCommandEncoder"_lit, encoder);
      else scan.Serialise("RenderCommandEncoder"_lit, encoder);
      scan.Serialise("descriptor"_lit, pass);
      if(m_DescriptorCoverage >= 52) m_DescriptorPartialCopySubmissions[command].valid = false;
      success = encoder != ResourceId() &&
          (parallel ? liveParallelRenders.insert(encoder).second : liveRenders.insert(encoder).second) &&
          (m_DescriptorCoverage < 20 || knownEncoders.insert(encoder).second) && validEncoderCommand(command) &&
          (m_DescriptorCoverage >= 53 || (pass.depthAttachment.textureId == ResourceId() && pass.stencilAttachment.textureId == ResourceId())) &&
          (m_DescriptorCoverage >= 65 || pass.visibilityResultBufferId == ResourceId()) &&
          !pass.rasterizationRateMap && pass.rasterizationRateMapId == ResourceId() &&
          pass.renderTargetArrayLength <= (m_DescriptorCoverage >= 65 ? 64U : 1U) &&
          pass.renderTargetWidth <= (m_DescriptorCoverage >= 35 ? 8192U : 2U) &&
          pass.renderTargetHeight <= (m_DescriptorCoverage >= 35 ? 8192U : 2U) &&
          pass.defaultRasterSampleCount <= 1 &&
          (m_DescriptorCoverage >= 54 ? ValidateMetalRenderPassCounters(this, pass.sampleBufferAttachments) :
           pass.sampleBufferAttachments.empty()) &&
          !pass.imageblockSampleLength && !pass.threadgroupMemoryLength && !pass.tileWidth && !pass.tileHeight;
      uint32_t targets = 0;
      std::set<ResourceId> targetResources;
      if(pass.visibilityResultBufferId != ResourceId())
      {
        const ResourceId buffer=pass.visibilityResultBufferId;
        auto object=GetResourceManager()->GetResource(buffer,true);
        MTL::Buffer *native=object && object->m_Type==eResBuffer ? Unwrap((WrappedMTLBuffer *)object):NULL;
        auto initial=m_ReplayBufferInitialContents.find(buffer);
        auto future=m_DescriptorFrameBuffers.find(buffer);
        bytebuf captured;
        const bool frameBuffer=future!=m_DescriptorFrameBuffers.end() &&
            m_DescriptorPreflightLiveBuffers.count(buffer) && future->second.length>=8 &&
            future->second.length<=1024*1024 &&
            future->second.options==(MTL::ResourceStorageModeShared | MTL::ResourceHazardTrackingModeTracked) &&
            readVisibilityInitial(buffer,command,future->second.length,captured);
        const bool backgroundBuffer=native && object->m_Device==this && native->length()>=8 &&
            native->length()<=1024*1024 && native->hazardTrackingMode()==MTL::HazardTrackingModeTracked &&
            native->storageMode()==MTL::StorageModeShared &&
            initial!=m_ReplayBufferInitialContents.end() && initial->second.size()==native->length();
        success &= (frameBuffer || backgroundBuffer) && !m_DescriptorPreflightAliasedBuffers.count(buffer);
        if(!success && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal visibility buffer proof: buffer=%s command=%s future=%d live=%d length=%llu options=%llu snapshot=%zu modified=%d table=%d aliased=%d background=%d\n",
              ToStr(buffer).c_str(),ToStr(command).c_str(),future!=m_DescriptorFrameBuffers.end(),
              int(m_DescriptorPreflightLiveBuffers.count(buffer)),
              (unsigned long long)(future!=m_DescriptorFrameBuffers.end()?future->second.length:0),
              (unsigned long long)(future!=m_DescriptorFrameBuffers.end()?future->second.options:0),
              captured.size(),int(modifiedBuffers.count(buffer)),int(isTable(buffer)),
              int(m_DescriptorPreflightAliasedBuffers.count(buffer)),backgroundBuffer);
        if(success) {renderVisibilityBuffers[encoder]=buffer;targetResources.insert(buffer);}
      }
      for(uint32_t index = 0; index < pass.colorAttachments.size(); index++)
      {
        const auto &attachment = pass.colorAttachments[index];
        if(attachment.textureId == ResourceId()) continue;
        auto future = m_DescriptorFrameTextures.find(attachment.textureId);
        MTL::Texture *texture = Unwrap(attachment.texture);
        MTL::PixelFormat format = MTL::PixelFormatInvalid;
        bool priorFrameWrite=false;
        if(m_DescriptorCoverage>=65)
          for(ResourceId writer:colorWriteCommands[attachment.textureId])
            priorFrameWrite |= writer==command || committed.count(writer)!=0;
        const bool extendedInitialColor = m_DescriptorCoverage >= 65 && texture &&
            texture->storageMode() == MTL::StorageModePrivate &&
            texture->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
            (texture->usage() & MTL::TextureUsageRenderTarget) &&
            (texture->pixelFormat() == MTL::PixelFormatR32Uint ||
             texture->pixelFormat() == MTL::PixelFormatR8Unorm ||
             texture->pixelFormat() == MTL::PixelFormatRGB10A2Unorm ||
             texture->pixelFormat() == MTL::PixelFormatRGBA16Unorm ||
             texture->pixelFormat() == MTL::PixelFormatRGBA16Float ||
             texture->pixelFormat() == MTL::PixelFormatBGRA8Unorm_sRGB);
        const bool initialCubeTarget=m_DescriptorCoverage>=65 && texture &&
            texture->textureType()==MTL::TextureTypeCube && texture->pixelFormat()==MTL::PixelFormatRG11B10Float &&
            texture->storageMode()==MTL::StorageModePrivate && texture->hazardTrackingMode()==MTL::HazardTrackingModeTracked &&
            (texture->usage()&MTL::TextureUsageRenderTarget) && HasReplayTextureInitialContents(attachment.textureId);
        // Layered volume attachments retain the original native pass and depth range.
        const bool layeredFrameVolume = m_DescriptorCoverage >= 65 &&
            future != m_DescriptorFrameTextures.end() &&
            future->second.descriptor.textureType == MTL::TextureType3D &&
            future->second.descriptor.pixelFormat == MTL::PixelFormatRGBA16Float &&
            ValidDescriptorFrameTexture(future->second.descriptor, m_DescriptorCoverage) &&
            (future->second.descriptor.usage & MTL::TextureUsageRenderTarget) &&
            pass.renderTargetArrayLength >= 1 &&
            pass.renderTargetArrayLength <= future->second.descriptor.depth &&
            pass.depthAttachment.textureId == ResourceId() &&
            pass.stencilAttachment.textureId == ResourceId();
        success &= pass.renderTargetArrayLength <= 1 || layeredFrameVolume;
        const bool currentDrawableTarget=m_DescriptorCoverage>=65 &&
            m_DescriptorDrawableTextures.count(attachment.textureId) &&
            ValidDescriptorDrawableTexture(texture) &&
            (!pass.renderTargetWidth || pass.renderTargetWidth==texture->width()) &&
            (!pass.renderTargetHeight || pass.renderTargetHeight==texture->height()) &&
            (attachment.loadAction==MTL::LoadActionClear ||
             (priorFrameWrite && attachment.loadAction==MTL::LoadActionLoad));
        if(m_DescriptorCoverage>=65 && m_DescriptorDrawableTextures.count(attachment.textureId) &&
           !HasReplayTextureInitialContents(attachment.textureId)) success &= currentDrawableTarget;
        const bool initializedDrawableTarget=m_DescriptorCoverage>=65 &&
            m_DescriptorDrawableTextures.count(attachment.textureId) &&
            ValidDescriptorDrawableTexture(texture) && HasReplayTextureInitialContents(attachment.textureId);
        const bool initialColorTarget = currentDrawableTarget || initializedDrawableTarget || (m_DescriptorCoverage >= 35 && texture &&
            (texture->textureType() == MTL::TextureType2D || initialCubeTarget) &&
            HasReplayTextureInitialContents(attachment.textureId) &&
            (texture->pixelFormat() == MTL::PixelFormatR16Float ||
             texture->pixelFormat() == MTL::PixelFormatRG11B10Float ||
             texture->pixelFormat() == MTL::PixelFormatRGBA8Unorm ||
             texture->pixelFormat() == MTL::PixelFormatBGRA8Unorm || extendedInitialColor));
        if(m_DescriptorCoverage >= 21 && future != m_DescriptorFrameTextures.end())
        {
          success &= m_DescriptorPreflightLiveTextures.count(attachment.textureId) != 0;
          format = future->second.descriptor.pixelFormat;
          success &= pass.renderTargetWidth <= future->second.descriptor.width &&
              pass.renderTargetHeight <= future->second.descriptor.height;
        }
        else
        {
          success &= texture && texture->width() <= (initialColorTarget ? 8192U : 2U) &&
              texture->height() <= (initialColorTarget ? 8192U : 2U) && texture->sampleCount() == 1;
          if(texture) success &= pass.renderTargetWidth <= (initialCubeTarget ? RDCMAX(1ULL,uint64_t(texture->width())>>RDCMIN(uint64_t(attachment.level),63ULL)):texture->width()) &&
              pass.renderTargetHeight <= (initialCubeTarget ? RDCMAX(1ULL,uint64_t(texture->height())>>RDCMIN(uint64_t(attachment.level),63ULL)):texture->height());
          if(texture) format = texture->pixelFormat();
        }
        success &= targetResources.insert(attachment.textureId).second &&
            (initialColorTarget || (m_DescriptorCoverage >= 38 && future != m_DescriptorFrameTextures.end() &&
              (future->second.descriptor.textureType == MTL::TextureType2D || layeredFrameVolume) &&
              (future->second.descriptor.usage & MTL::TextureUsageRenderTarget) &&
              (attachment.loadAction == MTL::LoadActionClear ||
               (priorFrameWrite && attachment.loadAction == MTL::LoadActionLoad))) || format == MTL::PixelFormatRGBA8Unorm || format == MTL::PixelFormatBGRA8Unorm ||
             (m_DescriptorCoverage >= 33 && format == MTL::PixelFormatRG11B10Float &&
              texture && texture->textureType() == MTL::TextureTypeCube &&
              HasReplayTextureInitialContents(attachment.textureId))) &&
            attachment.resolveTextureId == ResourceId() &&
            (initialCubeTarget ? attachment.level<texture->mipmapLevelCount() && attachment.slice<6 :
             !attachment.level && !attachment.slice) && !attachment.depthPlane &&
            (attachment.storeAction == MTL::StoreActionStore ||
             ((parallel || m_DescriptorCoverage >= 55) && attachment.storeAction == MTL::StoreActionUnknown));
        if(attachment.storeAction == MTL::StoreActionUnknown) deferredStores[encoder].insert(index);
        renderTargets[encoder][index] = format;
        if(m_DescriptorCoverage>=65 && attachment.loadAction==MTL::LoadActionClear)
          colorWriteCommands[attachment.textureId].insert(command);
        targets++;
      }
      if(m_DescriptorCoverage >= 53)
      {
        auto validateDepth = [&](const RDMTL::RenderPassAttachmentDescriptor &attachment, bool stencil) {
          MTL::PixelFormat format = MTL::PixelFormatInvalid;
          if(attachment.textureId == ResourceId()) return format;
          const auto future = m_DescriptorFrameTextures.find(attachment.textureId);
          MTL::Texture *texture = Unwrap(attachment.texture);
          if(future != m_DescriptorFrameTextures.end())
          {
            const auto &desc = future->second.descriptor;
            success &= m_DescriptorPreflightLiveTextures.count(attachment.textureId) &&
                ValidDescriptorFrameTexture(desc, m_DescriptorCoverage) &&
                pass.renderTargetWidth <= desc.width && pass.renderTargetHeight <= desc.height;
            format = desc.pixelFormat;
          }
          else
          {
            success &= texture && texture->textureType() == MTL::TextureType2D &&
                texture->width() && texture->width() <= 512 && texture->height() && texture->height() <= 512 &&
                texture->sampleCount() == 1 && texture->mipmapLevelCount() == 1 &&
                texture->storageMode() == MTL::StorageModePrivate &&
                texture->hazardTrackingMode() == MTL::HazardTrackingModeTracked &&
                (texture->usage() & MTL::TextureUsageRenderTarget) &&
                pass.renderTargetWidth <= texture->width() && pass.renderTargetHeight <= texture->height();
            if(texture) format = texture->pixelFormat();
          }
          const auto initialized = initializedDepthCommands.find({attachment.textureId, stencil});
          const bool priorClear = initialized != initializedDepthCommands.end() &&
              (initialized->second == command || committed.count(initialized->second));
          success &= (stencil ? format == MTL::PixelFormatDepth32Float_Stencil8 :
              (format == MTL::PixelFormatDepth16Unorm || format == MTL::PixelFormatDepth32Float ||
               format == MTL::PixelFormatDepth32Float_Stencil8)) &&
              attachment.resolveTextureId == ResourceId() && !attachment.level && !attachment.slice &&
              !attachment.depthPlane && (attachment.storeAction == MTL::StoreActionStore ||
               (m_DescriptorCoverage >= 55 && attachment.storeAction == MTL::StoreActionUnknown)) &&
              attachment.storeActionOptions == MTL::StoreActionOptionNone &&
              (attachment.loadAction == MTL::LoadActionClear ||
               (attachment.loadAction == MTL::LoadActionLoad &&
                (HasReplayTextureInitialContents(attachment.textureId) || priorClear)));
          if(attachment.storeAction == MTL::StoreActionUnknown)
            deferredStores[encoder].insert(stencil ? 9U : 8U);
          if(attachment.loadAction == MTL::LoadActionClear) initializedDepthCommands[{attachment.textureId, stencil}] = command;
          targetResources.insert(attachment.textureId);
          return format;
        };
        renderDepthTargets[encoder] = validateDepth(pass.depthAttachment, false);
        renderStencilTargets[encoder] = validateDepth(pass.stencilAttachment, true);
        if(pass.depthAttachment.textureId != ResourceId())
          success &= std::isfinite(pass.depthAttachment.clearDepth) &&
              pass.depthAttachment.clearDepth >= 0.0 && pass.depthAttachment.clearDepth <= 1.0 &&
              pass.depthAttachment.depthResolveFilter == MTL::MultisampleDepthResolveFilterSample0;
        if(pass.stencilAttachment.textureId != ResourceId())
          success &= pass.stencilAttachment.clearStencil <= 255 &&
              pass.stencilAttachment.stencilResolveFilter == MTL::MultisampleStencilResolveFilterSample0 &&
              pass.stencilAttachment.textureId == pass.depthAttachment.textureId;
      }
      renderAttachmentless[encoder]=!targets && renderDepthTargets[encoder]==MTL::PixelFormatInvalid &&
          renderStencilTargets[encoder]==MTL::PixelFormatInvalid;
      const bool boundedAttachmentless=m_DescriptorCoverage>=59 && renderAttachmentless[encoder] &&
          pass.renderTargetWidth && pass.renderTargetWidth<=512 &&
          pass.renderTargetHeight && pass.renderTargetHeight<=512 &&
          pass.defaultRasterSampleCount==1 && pass.renderTargetArrayLength==1;
      success &= (targets >= 1 || boundedAttachmentless || (m_DescriptorCoverage >= 56 &&
          renderDepthTargets[encoder] != MTL::PixelFormatInvalid)) &&
          targets <= (m_DescriptorCoverage >= 20 ? 5U : 2U);
      if(!success && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
      {
        fprintf(stderr,"Metal render pass proof: command=%s encoder=%s parallel=%d targets=%u width=%llu height=%llu array=%llu samples=%llu depth=%s stencil=%s depthFormat=%llu depthLoad=%llu depthStore=%llu depthInitial=%d\n",
            ToStr(command).c_str(),ToStr(encoder).c_str(),parallel,targets,
            (unsigned long long)pass.renderTargetWidth,(unsigned long long)pass.renderTargetHeight,
            (unsigned long long)pass.renderTargetArrayLength,(unsigned long long)pass.defaultRasterSampleCount,
            ToStr(pass.depthAttachment.textureId).c_str(),ToStr(pass.stencilAttachment.textureId).c_str(),
            (unsigned long long)renderDepthTargets[encoder],(unsigned long long)pass.depthAttachment.loadAction,
            (unsigned long long)pass.depthAttachment.storeAction,
            HasReplayTextureInitialContents(pass.depthAttachment.textureId));
        MTL::Texture *depth=Unwrap(pass.depthAttachment.texture);
        fprintf(stderr,"Metal render pass options: visibility=%s rateMap=%s counterValid=%d counters=%zu tile=%llu/%llu imageblock=%llu threadgroup=%llu commandKnown=%d commandCommitted=%d liveEncoders=%d\n",
            ToStr(GetResID(pass.visibilityResultBuffer)).c_str(),ToStr(pass.rasterizationRateMapId).c_str(),
            ValidateMetalRenderPassCounters(this,pass.sampleBufferAttachments),pass.sampleBufferAttachments.size(),
            (unsigned long long)pass.tileWidth,(unsigned long long)pass.tileHeight,
            (unsigned long long)pass.imageblockSampleLength,(unsigned long long)pass.threadgroupMemoryLength,
            int(commands.count(command)),int(committed.count(command)),hasLiveEncoder(command));
        if(depth) fprintf(stderr,"Metal render depth proof: width=%llu height=%llu mip=%llu samples=%llu storage=%llu hazard=%llu usage=%llu clear=%f filter=%llu stencilStore=%llu stencilLoad=%llu stencilFilter=%llu\n",
            (unsigned long long)depth->width(),(unsigned long long)depth->height(),
            (unsigned long long)depth->mipmapLevelCount(),(unsigned long long)depth->sampleCount(),
            (unsigned long long)depth->storageMode(),(unsigned long long)depth->hazardTrackingMode(),
            (unsigned long long)depth->usage(),pass.depthAttachment.clearDepth,
            (unsigned long long)pass.depthAttachment.depthResolveFilter,
            (unsigned long long)pass.stencilAttachment.storeAction,(unsigned long long)pass.stencilAttachment.loadAction,
            (unsigned long long)pass.stencilAttachment.stencilResolveFilter);
        for(uint32_t index=0;index<pass.colorAttachments.size();index++)
        {
          const auto &a=pass.colorAttachments[index];if(a.textureId==ResourceId())continue;
          MTL::Texture *t=Unwrap(a.texture);
          fprintf(stderr,"Metal render color proof: index=%u id=%s native=%d width=%llu height=%llu format=%llu load=%llu store=%llu future=%d initial=%d\n",index,ToStr(a.textureId).c_str(),t!=NULL,
              (unsigned long long)(t?t->width():0),(unsigned long long)(t?t->height():0),
              (unsigned long long)(t?t->pixelFormat():MTL::PixelFormatInvalid),
              (unsigned long long)a.loadAction,(unsigned long long)a.storeAction,
              m_DescriptorFrameTextures.count(a.textureId)!=0,HasReplayTextureInitialContents(a.textureId));
        }
      }
      if(success)
      {
        encoderCommands[encoder] = command;
        if(m_DescriptorCoverage>=58)renderIndirectWrites[encoder].insert(targetResources.begin(),targetResources.end());
        if(m_DescriptorCoverage >= 36)
        {
          for(ResourceId target : targetResources)
          {
            noteResource(target, encoder);
            modifiedBuffers.insert(target);
          }
          pendingWork.insert(command);
        }
      }
    }
    else if(m_DescriptorCoverage >= 20 && chunk == MetalChunk::MTLParallelRenderCommandEncoder_renderCommandEncoder)
    {
      ResourceId parent, child;
      scan.Serialise("ParallelRenderCommandEncoder"_lit, parent);
      scan.Serialise("RenderCommandEncoder"_lit, child);
      success = liveParallelRenders.count(parent) && !activeChildren.count(parent) &&
          child != ResourceId() && knownEncoders.insert(child).second && liveRenders.insert(child).second &&
          !GetResourceManager()->HasResource(child);
      if(success)
      {
        childParents[child] = parent; activeChildren[parent] = child;
        encoderCommands[child] = encoderCommands[parent];
        renderTargets[child] = renderTargets[parent];
        renderDepthTargets[child] = renderDepthTargets[parent];
        renderStencilTargets[child] = renderStencilTargets[parent];
        renderAttachmentless[child]=renderAttachmentless[parent];
        if(renderVisibilityBuffers.count(parent)) renderVisibilityBuffers[child]=renderVisibilityBuffers[parent];
      }
    }
    else if(m_DescriptorCoverage>=65 && chunk==MetalChunk::MTLRenderCommandEncoder_setVisibilityResultMode)
    {
      ResourceId encoder;uint64_t mode=0,offset=0;
      scan.Serialise("RenderCommandEncoder"_lit,encoder);
      scan.Serialise("mode"_lit,mode);
      scan.Serialise("offset"_lit,offset);
      const ResourceId buffer=renderVisibilityBuffers.count(encoder)?renderVisibilityBuffers[encoder]:ResourceId();
      auto object=GetResourceManager()->GetResource(buffer,true);
      MTL::Buffer *native=object && object->m_Type==eResBuffer?Unwrap((WrappedMTLBuffer *)object):NULL;
      auto future=m_DescriptorFrameBuffers.find(buffer);
      const uint64_t length=future!=m_DescriptorFrameBuffers.end() &&
          m_DescriptorPreflightLiveBuffers.count(buffer)?future->second.length:native?native->length():0;
      success=liveRenders.count(encoder) && mode<=MTL::VisibilityResultModeCounting && !(offset&7) &&
          (mode==MTL::VisibilityResultModeDisabled || (length && offset<=length &&
           8<=length-offset && !m_DescriptorPreflightAliasedBuffers.count(buffer)));
      if(success && mode!=MTL::VisibilityResultModeDisabled)
      {
        noteResource(buffer,encoder);modifiedBuffers.insert(buffer);opaqueWrites.insert(buffer);
        if(m_DescriptorCoverage>=58)renderIndirectWrites[childParents.count(encoder)?childParents[encoder]:encoder].insert(buffer);
      }
    }
    else if((m_DescriptorCoverage >= 20 && chunk == MetalChunk::MTLParallelRenderCommandEncoder_setColorStoreAction) ||
        (m_DescriptorCoverage >= 55 &&
         (chunk == MetalChunk::MTLRenderCommandEncoder_setColorStoreAction ||
          chunk == MetalChunk::MTLRenderCommandEncoder_setDepthStoreAction ||
          chunk == MetalChunk::MTLRenderCommandEncoder_setStencilStoreAction ||
          chunk == MetalChunk::MTLParallelRenderCommandEncoder_setDepthStoreAction ||
          chunk == MetalChunk::MTLParallelRenderCommandEncoder_setStencilStoreAction)))
    {
      const bool parallel = chunk == MetalChunk::MTLParallelRenderCommandEncoder_setColorStoreAction ||
          chunk == MetalChunk::MTLParallelRenderCommandEncoder_setDepthStoreAction ||
          chunk == MetalChunk::MTLParallelRenderCommandEncoder_setStencilStoreAction;
      const bool depth = chunk == MetalChunk::MTLRenderCommandEncoder_setDepthStoreAction ||
          chunk == MetalChunk::MTLParallelRenderCommandEncoder_setDepthStoreAction;
      const bool stencil = chunk == MetalChunk::MTLRenderCommandEncoder_setStencilStoreAction ||
          chunk == MetalChunk::MTLParallelRenderCommandEncoder_setStencilStoreAction;
      ResourceId encoder; uint64_t action = 0, index = depth ? 8U : stencil ? 9U : 0U;
      scan.Serialise(parallel ? "ParallelRenderCommandEncoder"_lit : "RenderCommandEncoder"_lit, encoder);
      scan.Serialise("storeAction"_lit, action);
      if(!depth && !stencil)
        scan.Serialise(parallel ? "index"_lit : "colorAttachmentIndex"_lit, index);
      success = (parallel ? liveParallelRenders.count(encoder) : liveRenders.count(encoder)) &&
          !childParents.count(encoder) && action == (uint64_t)MTL::StoreActionStore &&
          (depth ? renderDepthTargets[encoder] != MTL::PixelFormatInvalid :
           stencil ? renderStencilTargets[encoder] != MTL::PixelFormatInvalid :
           index < 8 && renderTargets[encoder].count((uint32_t)index));
      const size_t resolved = deferredStores[encoder].erase((uint32_t)index);
      if(m_DescriptorCoverage < 55) success &= resolved == 1;
    }
    else if(m_DescriptorCoverage >= 20 && chunk == MetalChunk::MTLParallelRenderCommandEncoder_endEncoding)
    {
      ResourceId parent; scan.Serialise("ParallelRenderCommandEncoder"_lit, parent);
      success = !activeChildren.count(parent) && deferredStores[parent].empty() &&
          liveParallelRenders.erase(parent) == 1;
      if(success)success=validateIndirectPass(parent);
      if(success) encoderCommands.erase(parent);
    }
    else if(m_DescriptorCoverage >= 9 && chunk == MetalChunk::MTLRenderCommandEncoder_endEncoding)
    {
      ResourceId encoder; scan.Serialise("RenderCommandEncoder"_lit, encoder);
      success = deferredStores[encoder].empty() && liveRenders.erase(encoder) == 1;
      if(success && !childParents.count(encoder))success=validateIndirectPass(encoder);
      if(success)
      {
        encoderCommands.erase(encoder);
        auto parent = childParents.find(encoder);
        if(parent != childParents.end())
        {
          success &= activeChildren[parent->second] == encoder;
          activeChildren.erase(parent->second); childParents.erase(parent);
        }
      }
    }
    else if(m_DescriptorCoverage >= 9 && chunk == MetalChunk::MTLRenderCommandEncoder_setRenderPipelineState)
    {
      ResourceId encoder, pipeline;
      scan.Serialise("RenderCommandEncoder"_lit, encoder); scan.Serialise("pipelineState"_lit, pipeline);
      success = liveRenders.count(encoder) && pipeline != ResourceId();
      for(uint32_t stage = 1; stage <= (m_DescriptorCoverage >= 66 ? 4U : 2U); stage++)
        graphics[{encoder, stage}].pipeline = pipeline;
      if(m_DescriptorCoverage >= 52) m_DescriptorPartialCopySubmissions[encoderCommands[encoder]].valid = false;
    }
    else if(m_DescriptorCoverage >= 66 &&
        (chunk == MetalChunk::MTLRenderCommandEncoder_setObjectBuffer ||
         chunk == MetalChunk::MTLRenderCommandEncoder_setObjectBytes ||
         chunk == MetalChunk::MTLRenderCommandEncoder_setObjectBufferOffset ||
         chunk == MetalChunk::MTLRenderCommandEncoder_setObjectBuffers ||
         chunk == MetalChunk::MTLRenderCommandEncoder_setMeshBuffer ||
         chunk == MetalChunk::MTLRenderCommandEncoder_setMeshBytes ||
         chunk == MetalChunk::MTLRenderCommandEncoder_setMeshBufferOffset ||
         chunk == MetalChunk::MTLRenderCommandEncoder_setMeshBuffers))
    {
      const bool object = chunk == MetalChunk::MTLRenderCommandEncoder_setObjectBuffer ||
          chunk == MetalChunk::MTLRenderCommandEncoder_setObjectBytes ||
          chunk == MetalChunk::MTLRenderCommandEncoder_setObjectBufferOffset ||
          chunk == MetalChunk::MTLRenderCommandEncoder_setObjectBuffers;
      const uint32_t stage = object ? 3U : 4U;
      const bool single = chunk == MetalChunk::MTLRenderCommandEncoder_setObjectBuffer;
      const bool bytes = chunk == MetalChunk::MTLRenderCommandEncoder_setObjectBytes ||
          chunk == MetalChunk::MTLRenderCommandEncoder_setMeshBytes;
      const bool offsetOnly = chunk == MetalChunk::MTLRenderCommandEncoder_setObjectBufferOffset ||
          chunk == MetalChunk::MTLRenderCommandEncoder_setMeshBufferOffset;
      ResourceId encoder; rdcarray<ResourceId> buffers; rdcarray<NS::UInteger> offsets;
      rdcarray<byte> data; NS::Range range = NS::Range::Make(0, 0);
      scan.Serialise("RenderCommandEncoder"_lit, encoder);
      if(single)
      {
        ResourceId buffer; uint64_t offset = 0, index = 0;
        scan.Serialise("buffer"_lit, buffer); scan.Serialise("offset"_lit, offset);
        scan.Serialise("index"_lit, index);
        buffers = {buffer}; offsets = {NS::UInteger(offset)};
        range = NS::Range::Make(index, 1);
      }
      else
      {
        scan.Serialise("buffers"_lit, buffers); scan.Serialise("offsets"_lit, offsets);
        scan.Serialise("data"_lit, data); scan.Serialise("range"_lit, range);
      }
      auto &snapshot = graphics[{encoder, stage}];
      success = liveRenders.count(encoder) && range.location < 31 && range.length &&
          range.length <= 31 - range.location;
      if(bytes)
      {
        success &= range.length == 1 && buffers.empty() && offsets.empty();
        const auto key = make_rdcpair(encoder, uint64_t(stage) << 32 | range.location);
        const auto layout = m_DescriptorInlineShadow.find(key);
        if(layout != m_DescriptorInlineShadow.end())
          for(const auto &source : layout->second.sources) noteResource(source.second.resource, encoder);
        if(success) success = RelocateDescriptorInlineShadow(encoder, stage, range.location, data);
        if(success) { snapshot.bytes[uint32_t(range.location)] = data.size(); snapshot.buffers.erase(uint32_t(range.location)); }
      }
      else
      {
        success &= data.empty() && offsets.size() == range.length &&
            (offsetOnly ? buffers.empty() && range.length == 1 : buffers.size() == range.length);
        for(size_t i = 0; success && i < range.length; i++)
        {
          const uint32_t slot = uint32_t(range.location + i);
          ResourceId buffer = offsetOnly ? snapshot.buffers[slot].resourceId : buffers[i];
          auto native = GetResourceManager()->GetResource(buffer, true);
          auto future = m_DescriptorFrameBuffers.find(buffer);
          const uint64_t length = future != m_DescriptorFrameBuffers.end() && m_DescriptorPreflightLiveBuffers.count(buffer)
              ? future->second.length : native && native->m_Type == eResBuffer && native->m_Real
              ? Unwrap((WrappedMTLBuffer *)native)->length() : 0;
          success &= !(offsets[i] % 4) && !m_DescriptorPreflightAliasedBuffers.count(buffer) &&
              (buffer == ResourceId() ? !offsetOnly && offsets[i] == 0 : offsets[i] < length);
          if(success)
          {
            snapshot.bytes.erase(slot);
            if(buffer == ResourceId()) snapshot.buffers.erase(slot);
            else { auto &binding = snapshot.buffers[slot]; binding.resourceId = buffer;
              binding.byteOffset = offsets[i]; binding.byteSize = length - offsets[i]; noteResource(buffer, encoder); }
          }
        }
      }
    }
    else if(m_DescriptorCoverage >= 66 && chunk == MetalChunk::MTLRenderCommandEncoder_drawMeshThreadgroups)
    {
      ResourceId encoder; MTL::Size grid = {}, objectThreads = {}, meshThreads = {};
      scan.Serialise("RenderCommandEncoder"_lit, encoder);
      scan.Serialise("threadgroupsPerGrid"_lit, grid);
      scan.Serialise("threadsPerObjectThreadgroup"_lit, objectThreads);
      scan.Serialise("threadsPerMeshThreadgroup"_lit, meshThreads);
      const ResourceId pipeline = graphics[{encoder, 4}].pipeline;
      auto wrapped = GetResourceManager()->GetResource(pipeline, true);
      auto native = wrapped && wrapped->m_Type == eResRenderPipelineState && wrapped->m_Real
          ? Unwrap((WrappedMTLRenderPipelineState *)wrapped) : NULL;
      auto bounded = [](const MTL::Size &size, uint64_t limit) {
        return size.width && size.height && size.depth && size.height <= limit &&
            size.width <= limit / size.height && size.width * size.height <= limit / size.depth;
      };
      success = liveRenders.count(encoder) && native && GetReplay()->IsMeshPipeline(pipeline) &&
          ++meshDrawCount <= 512 && bounded(grid, 1024) &&
          bounded(meshThreads, native->maxTotalThreadsPerMeshThreadgroup()) &&
          bounded(objectThreads, RDCMAX(1ULL, uint64_t(native->maxTotalThreadsPerObjectThreadgroup()))) &&
          native->maxTotalThreadgroupsPerMeshGrid() &&
          (native->maxTotalThreadsPerObjectThreadgroup() || bounded(grid, native->maxTotalThreadgroupsPerMeshGrid()));
      if(success)
      {
        const uint64_t groups = grid.width * grid.height * grid.depth;
        const uint64_t meshInvocations = meshThreads.width * meshThreads.height * meshThreads.depth;
        const uint64_t work = groups * meshInvocations;
        success &= work <= 1024 * 1024 - drawWork;
        if(success) drawWork += work;
      }
      success &= validateDrawableReads(encoder);
      success &= GetReplay()->ValidateGraphicsTargets(pipeline, renderTargets[encoder], 5U,
          renderDepthTargets[encoder], renderStencilTargets[encoder]);
      for(uint32_t stage : {2U, 3U, 4U})
      {
        const auto &snapshot = graphics[{encoder, stage}];
        success &= GetReplay()->ValidateGraphicsBufferSnapshot(pipeline, stage, snapshot.buffers, snapshot.bytes);
      }
      for(const auto &entry : m_DescriptorSlotShadow)
      {
        bytebuf patched;
        if(entry.second.live && !PatchDescriptorSlot(entry.second, patched)) success = false;
      }
      for(ResourceId resource : encoderResources[encoder]) noteResource(resource, encoder);
      if(success) noteSubmissionSlots(encoder);
      pendingWork.insert(encoderCommands[encoder]); consumed = true;
    }
    else if(m_DescriptorCoverage >= 65 &&
        (chunk == MetalChunk::MTLRenderCommandEncoder_setVertexBuffer ||
         chunk == MetalChunk::MTLRenderCommandEncoder_setFragmentBuffer))
    {
      ResourceId encoder, buffer; uint64_t offset = 0, index = 0;
      const uint32_t stage = chunk == MetalChunk::MTLRenderCommandEncoder_setVertexBuffer ? 1 : 2;
      scan.Serialise("RenderCommandEncoder"_lit, encoder);
      scan.Serialise("buffer"_lit, buffer);
      scan.Serialise("offset"_lit, offset);
      scan.Serialise("index"_lit, index);
      auto object = GetResourceManager()->GetResource(buffer, true);
      auto future = m_DescriptorFrameBuffers.find(buffer);
      uint64_t length = 0;
      if(future != m_DescriptorFrameBuffers.end() && m_DescriptorPreflightLiveBuffers.count(buffer))
        length = future->second.length;
      else if(object && object->m_Type == eResBuffer && object->m_Real)
        length = Unwrap((WrappedMTLBuffer *)object)->length();
      success = liveRenders.count(encoder) && index < 31 && offset % 4 == 0 &&
          !m_DescriptorPreflightAliasedBuffers.count(buffer) &&
          (buffer == ResourceId() ? offset == 0 : length && offset < length);
      if(success)
      {
        auto &snapshot = graphics[{encoder, stage}];
        snapshot.bytes.erase((uint32_t)index);
        if(buffer == ResourceId()) snapshot.buffers.erase((uint32_t)index);
        else
        {
          MetalPipe::BufferBinding binding;
          binding.resourceId = buffer; binding.byteOffset = offset; binding.byteSize = length - offset;
          snapshot.buffers[(uint32_t)index] = binding;
          noteResource(buffer, encoder);
        }
        if(stage == 1) drawConstants[encoder].erase((uint32_t)index);
      }
    }
    else if(m_DescriptorCoverage >= 9 &&
        (chunk == MetalChunk::MTLRenderCommandEncoder_setVertexBytes || chunk == MetalChunk::MTLRenderCommandEncoder_setFragmentBytes))
    {
      ResourceId encoder; uint64_t index = 0; rdcarray<byte> data;
      const uint32_t stage = chunk == MetalChunk::MTLRenderCommandEncoder_setVertexBytes ? 1 : 2;
      scan.Serialise("RenderCommandEncoder"_lit, encoder); scan.Serialise("data"_lit, data); scan.Serialise("index"_lit, index);
      for(const auto &source : m_DescriptorInlineShadow[make_rdcpair(encoder, uint64_t(stage) << 32 | index)].sources)
        noteResource(source.second.resource, encoder);
      success = liveRenders.count(encoder) && RelocateDescriptorInlineShadow(encoder, stage, index, data);
      if(success)
      {
        graphics[{encoder, stage}].bytes[(uint32_t)index] = data.size();
        if(m_DescriptorCoverage >= 65) graphics[{encoder, stage}].buffers.erase((uint32_t)index);
        if(m_DescriptorCoverage >= 14 && stage == 1 && (index == 4 || index == 5))
          drawConstants[encoder][(uint32_t)index] = data;
      }
    }
    else if(m_DescriptorCoverage >= 9 && (chunk == MetalChunk::MTLRenderCommandEncoder_drawPrimitives ||
        chunk == MetalChunk::MTLRenderCommandEncoder_drawPrimitives_instanced ||
        chunk == MetalChunk::MTLRenderCommandEncoder_drawPrimitives_instanced_base ||
        (m_DescriptorCoverage >= 14 && (chunk == MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives ||
          chunk == MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced ||
          chunk == MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced_base)) ||
        (m_DescriptorCoverage>=58 && (chunk==MetalChunk::MTLRenderCommandEncoder_drawPrimitives_indirect ||
          chunk==MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_indirect))))
    {
      const bool indirect=chunk==MetalChunk::MTLRenderCommandEncoder_drawPrimitives_indirect ||
          chunk==MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_indirect;
      const bool indexed = chunk == MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives ||
          chunk == MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced ||
          chunk == MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced_base ||
          chunk == MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_indirect;
      ResourceId encoder; MTL::PrimitiveType primitiveType; uint64_t vertexStart = 0, vertexCount = 0, instanceCount = 0, baseInstance = 0;
      int64_t baseVertex = 0;
      MTL::IndexType indexType = MTL::IndexTypeUInt16;
      ResourceId indexBuffer; uint64_t indexOffset = 0;
      scan.Serialise("RenderCommandEncoder"_lit, encoder); scan.Serialise("primitiveType"_lit, primitiveType);
      bool indirectValid=true;
      if(indirect)
      {
        if(indexed)
        {
          scan.Serialise("indexType"_lit,indexType);scan.Serialise("indexBuffer"_lit,indexBuffer);
          scan.Serialise("indexBufferOffset"_lit,indexOffset);
        }
        ResourceId buffer;uint64_t offset=0;
        scan.Serialise("indirectBuffer"_lit,buffer);scan.Serialise("indirectBufferOffset"_lit,offset);
        const uint32_t ordinal=capturedRenderIndirectOrdinals[encoder]++;
        const auto evidence=m_CapturedRenderIndirectArguments.find(make_rdcpair(encoder,ordinal));
        const ResourceId pass=childParents.count(encoder)?childParents[encoder]:encoder;
        auto object=GetResourceManager()->GetResource(buffer,true);
        auto future=m_DescriptorFrameBuffers.find(buffer);
        MTL::Buffer *native=object && object->m_Type==eResBuffer && object->m_Real?Unwrap((WrappedMTLBuffer *)object):NULL;
        const uint64_t length=future!=m_DescriptorFrameBuffers.end()?future->second.length:native?native->length():0;
        indirectValid=m_HasCapturedRenderIndirectArguments && evidence!=m_CapturedRenderIndirectArguments.end() &&
            buffer!=ResourceId() && length && length<=1024*1024 && (offset&3)==0 && offset<=length &&
            (indexed?20U:16U)<=length-offset && !m_DescriptorPreflightAliasedBuffers.count(buffer) &&
            (future==m_DescriptorFrameBuffers.end() || m_DescriptorPreflightLiveBuffers.count(buffer));
        for(const auto &table:m_DescriptorTables)indirectValid &= table.buffer!=buffer;
        if(indirectValid)
        {
          const auto &proof=evidence->second;
          indirectValid=proof.command==encoderCommands[encoder] && proof.pass==pass && proof.buffer==buffer &&
              proof.offset==offset && proof.wordCount==(indexed?5U:4U);
          if(indirectValid)
          {
            const auto &args=proof.arguments;
            vertexCount=args[0];instanceCount=args[1];baseInstance=args[indexed?4:3];
            if(indexed)
            {
              baseVertex=(int32_t)args[3];
              const uint64_t stride=indexType==MTL::IndexTypeUInt16?2:4;
              if(args[2]>(UINT64_MAX-indexOffset)/stride)indirectValid=false;
              else indexOffset+=uint64_t(args[2])*stride;
            }
            else vertexStart=args[2];
            renderIndirectSources[pass].insert(buffer);noteResource(buffer,encoder);
          }
        }
      }
      else if(indexed)
      {
        scan.Serialise("indexCount"_lit, vertexCount); scan.Serialise("indexType"_lit, indexType);
        scan.Serialise("indexBuffer"_lit, indexBuffer); scan.Serialise("indexBufferOffset"_lit, indexOffset);
        instanceCount = 1;
        if(chunk != MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives)
          scan.Serialise("instanceCount"_lit, instanceCount);
        if(chunk == MetalChunk::MTLRenderCommandEncoder_drawIndexedPrimitives_instanced_base)
        {
          scan.Serialise("baseVertex"_lit, baseVertex); scan.Serialise("baseInstance"_lit, baseInstance);
        }
      }
      else
      {
        scan.Serialise("vertexStart"_lit, vertexStart); scan.Serialise("vertexCount"_lit, vertexCount);
        scan.Serialise("instanceCount"_lit, instanceCount); scan.Serialise("baseInstance"_lit, baseInstance);
      }
      const bool boundedDraw = m_DescriptorCoverage >= 49;
      if(boundedDraw)
      {
        // Match D3D12/Vulkan: replay captured draw arguments and index order.
        // This sourced coverage remains bounded independently of descriptor work.
        success = liveRenders.count(encoder) && ++drawCount <= 512 &&
            (primitiveType == MTL::PrimitiveTypeTriangle || primitiveType == MTL::PrimitiveTypeTriangleStrip) &&
            indirectValid && (indirect || (vertexCount && instanceCount)) && vertexCount<=65536 &&
            instanceCount<=(vertexCount?65536/vertexCount:65536) &&
            vertexStart <= 65534 && vertexCount <= 65535 - vertexStart &&
            baseInstance <= UINT32_MAX && instanceCount <= UINT32_MAX - baseInstance &&
            baseVertex >= INT32_MIN && baseVertex <= INT32_MAX && indexOffset <= UINT32_MAX &&
            m_DescriptorGPUCopyExpected.empty();
        if(success)
        {
          const uint64_t work = vertexCount * instanceCount;
          success &= work <= 1024 * 1024 - drawWork;
          if(success) drawWork += work;
        }
      }
      else
        success = liveRenders.count(encoder) && ++drawCount <= 2 && primitiveType == MTL::PrimitiveTypeTriangle &&
            !vertexStart && vertexCount == 3 && instanceCount == 1 && !baseInstance && !baseVertex && m_DescriptorGPUCopyExpected.empty();
      if(indexed)
      {
        const uint64_t stride = indexType == MTL::IndexTypeUInt16 ? 2 : 4;
        auto object = GetResourceManager()->GetResource(indexBuffer, true);
        MTL::Buffer *native = object && object->m_Type == eResBuffer ? Unwrap((WrappedMTLBuffer *)object) : NULL;
        auto initial = m_ReplayBufferInitialContents.find(indexBuffer);
        const bool initialIndex = native && initial != m_ReplayBufferInitialContents.end() &&
            initial->second.size() == native->length();
        auto frame = m_DescriptorFrameBuffers.find(indexBuffer);
        const bool frameIndex = m_DescriptorCoverage >= 50 && frame != m_DescriptorFrameBuffers.end() &&
            frame->second.options == (MTL::ResourceStorageModePrivate | MTL::ResourceHazardTrackingModeTracked) &&
            m_DescriptorPreflightLiveBuffers.count(indexBuffer) &&
            (m_DescriptorCoverage >= 51 || copiedFrameIndices.count(indexBuffer));
        const uint64_t indexLength = frameIndex ? frame->second.length : native ? native->length() : 0;
        const bool readableIndex = frameIndex || (native && (native->storageMode() == MTL::StorageModeShared ||
            (m_DescriptorCoverage >= 48 && native->storageMode() == MTL::StorageModePrivate && initialIndex)));
        // Static newBufferWithBytes can carry its bytes in the creation chunk and
        // a redundant first-commit snapshot rather than an Initial Contents chunk.
        // Read only this validated, CPU-visible native wrapper, never a captured VA.
        if(!indexContents.count(indexBuffer) && readableIndex &&
           (initialIndex || (m_ReplayBuffersWithCreationContents.count(indexBuffer) && native->contents())) &&
           native->length() && native->length() <= 64 * 1024)
          indexContents[indexBuffer] = initial != m_ReplayBufferInitialContents.end() ? initial->second :
              bytebuf((byte *)native->contents(), native->length());
        auto contents = indexContents.find(indexBuffer);
        success &= readableIndex && indexLength <= 64 * 1024 &&
            (indexType == MTL::IndexTypeUInt16 || indexType == MTL::IndexTypeUInt32) &&
            (frameIndex || !m_DescriptorFrameBuffers.count(indexBuffer)) && !m_DescriptorPreflightAliasedBuffers.count(indexBuffer) &&
            !opaqueWrites.count(indexBuffer) &&
            !m_DescriptorGPUWrittenBuffers.count(indexBuffer) &&
            ((frameIndex && m_DescriptorCoverage >= 51) || contents != indexContents.end()) &&
            indexOffset % stride == 0 && indexOffset <= indexLength &&
            vertexCount <= (indexLength - indexOffset) / stride;
        for(const auto &table : m_DescriptorTables) success &= table.buffer != indexBuffer;
        if(frameIndex && m_DescriptorCoverage >= 51 && success) {
          submissionIndexOperations[encoderCommands[encoder]].push_back(
              {indexBuffer, indexOffset, {}, vertexCount, stride, baseVertex});
          noteResource(indexBuffer, encoder);
        }
        else if(frameIndex && success) {
          for(ResourceId producer : indexCopyCommands[indexBuffer])
            success &= producer == encoderCommands[encoder] || committed.count(producer);
          const auto known = indexKnownBytes.find(indexBuffer);
          success &= known != indexKnownBytes.end() && known->second.size() == indexLength;
          if(success) for(uint64_t byte = indexOffset; byte < indexOffset + vertexCount * stride; byte++)
            success &= known->second[byte] != 0;
          noteResource(indexBuffer, encoder);
        }
        if(success && !(frameIndex && m_DescriptorCoverage >= 51))
          for(uint32_t index = 0; index < vertexCount; index++)
          {
            uint32_t value = 0;
            memcpy(&value, contents->second.data() + indexOffset + index * stride, stride);
            const int64_t effective = int64_t(value) + baseVertex;
            success &= boundedDraw ? (effective >= 0 && effective < 65535) : value == index;
          }
        indexBuffers.insert(indexBuffer);
      }
      if(!indirect && m_DescriptorCoverage >= 14 && (indexed || !drawConstants[encoder].empty()))
      {
        const auto &constants = drawConstants[encoder];
        const auto params = constants.find(4), kind = constants.find(5);
        success &= params != constants.end() && params->second.size() == 20 &&
            kind != constants.end() && kind->second.size() == 2;
        if(success)
        {
          uint32_t words[5] = {}; uint16_t value = 0;
          memcpy(words, params->second.data(), sizeof(words)); memcpy(&value, kind->second.data(), sizeof(value));
          success &= words[0] == vertexCount && words[1] == instanceCount &&
              words[2] == (indexed ? indexOffset : vertexStart) && words[3] == (indexed ? uint32_t(baseVertex) : baseInstance) &&
              (!indexed || words[4] == baseInstance) && value == (indexed ? (indexType == MTL::IndexTypeUInt16 ? 1 : 2) : 0);
        }
      }
      if(!success) fprintf(stderr,"Metal graphics draw: live=%d draws=%u primitive=%llu start=%llu vertices=%llu instances=%llu base=%llu pending=%zu\n",int(liveRenders.count(encoder)),drawCount,(uint64_t)primitiveType,vertexStart,vertexCount,instanceCount,baseInstance,m_DescriptorGPUCopyExpected.size());
      success &= validateDrawableReads(encoder);
      success &= GetReplay()->ValidateGraphicsTargets(graphics[{encoder, 1}].pipeline, renderTargets[encoder], m_DescriptorCoverage >= 20 ? 5U : 2U,
          renderDepthTargets.count(encoder) ? renderDepthTargets[encoder] : MTL::PixelFormatInvalid,
          renderStencilTargets.count(encoder) ? renderStencilTargets[encoder] : MTL::PixelFormatInvalid,
          m_DescriptorCoverage>=59 && indirect && renderAttachmentless[encoder]);
      for(uint32_t stage = 1; stage <= 2; stage++)
      {
        const auto &snapshot = graphics[{encoder, stage}];
        success &= GetReplay()->ValidateGraphicsBufferSnapshot(snapshot.pipeline, stage, snapshot.buffers, snapshot.bytes,
            m_DescriptorCoverage >= 56 && renderTargets[encoder].empty() &&
            renderDepthTargets[encoder] != MTL::PixelFormatInvalid);
      }
      for(const auto &entry : m_DescriptorSlotShadow)
      {
        bytebuf patched;
        if(entry.second.live && !PatchDescriptorSlot(entry.second, patched))
            {
              success = false;
              if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
                fprintf(stderr,"Metal descriptor consumer slot: buffer=%s offset=%llu generation=%llu bytes=%zu gpuExpected=%d\n",
                    ToStr(entry.first.first).c_str(),(unsigned long long)entry.first.second,
                    (unsigned long long)entry.second.generation,entry.second.data.size(),entry.second.gpuExpected);
            }
      }
      for(ResourceId resource : encoderResources[encoder]) noteResource(resource, encoder);
      if(success) noteSubmissionSlots(encoder);
      if(m_DescriptorCoverage >= 15) pendingWork.insert(encoderCommands[encoder]);
      consumed = true;
    }
    else if(chunk == MetalChunk::MTLBuffer_DescriptorSlotEvent)
    {
      ResourceId buffer; uint64_t offset = 0, generation = 0;
      uint32_t event = 0, type = 0; bytebuf data;
      scan.Serialise("buffer"_lit, buffer); scan.Serialise("offset"_lit, offset);
      scan.Serialise("generation"_lit, generation); scan.Serialise("event"_lit, event);
      scan.Serialise("descriptorType"_lit, type); scan.Serialise("data"_lit, data);
      bool newBacking = false;
      for(const auto &pair : m_ValidatedDescriptorBackingAliases) newBacking |= pair.second == buffer;
      const bool initialCPUValue=m_DescriptorCoverage>=61 && !dispatchCount && !drawCount && event==2 &&
          resourceCommands[buffer].empty() && !modifiedBuffers.count(buffer) &&
          ValidDescriptorCPUWrite(buffer,offset,data.size());
      const DescriptorSlotKey key=make_rdcpair(buffer,offset);
      const auto existing=m_DescriptorSlotShadow.find(key);
      // A new CPU payload slot has no earlier descriptor consumer. Keep the
      // whole-buffer GPU-write exclusion: this only fills an unborrowed, empty
      // logical entry, never edits a slot already visible to encoded work.
      const bool freshCPUValue=m_DescriptorCoverage>=62 && event==2 && data.size()==24 &&
          (!m_DescriptorGPUWrittenBuffers.count(buffer) ? !modifiedBuffers.count(buffer) :
           (m_DescriptorCoverage>=64 && ValidDescriptorCPUWrite(buffer,offset,data.size()) &&
            !opaqueTableWrites.count(buffer) && !writtenSlots.count(key))) &&
          !borrowedSlots.count(key) && existing!=m_DescriptorSlotShadow.end() &&
          existing->second.live && existing->second.data.empty();
      // Retired generations may be reused after every earlier table consumer
      // is submitted. ReplayCPUBufferUpdate waits these submissions before the
      // next Native CPU value write; allocation itself changes no Native bytes.
      bool submittedGeneration = m_DescriptorCoverage >= 65 && event == 0 &&
          existing != m_DescriptorSlotShadow.end() && !existing->second.live &&
          generation > existing->second.generation && !opaqueTableWrites.count(buffer);
      for(ResourceId consumer : slotConsumers[key])
        submittedGeneration &= committed.count(consumer) != 0;
      // Allocation only changes the logical generation. It cannot overwrite
      // an older live entry and does not mutate Native descriptor memory.
      success = (!consumed || initialCPUValue || freshCPUValue || event == 3 ||
          (m_DescriptorCoverage>=61 && event==0) || (m_DescriptorCoverage >= 25 && (event == 1 || newBacking))) &&
          ReadDescriptorSlotEvent(buffer, offset, generation, event, type, data, initialCPUValue || freshCPUValue);
      if(success && !m_DescriptorGPUCopyExpected.count(key)) gpuCopyEncoders.erase(key);
      if(success && submittedGeneration) {
        borrowedSlots.erase(key);writtenSlots.erase(key);freshCPUValues.erase(key);slotConsumers.erase(key);
      }
      if(success && (initialCPUValue || (freshCPUValue && m_DescriptorGPUWrittenBuffers.count(buffer))))
        m_DescriptorInitialCPUValueOffsets.insert(chunkOffset);
      if(success && freshCPUValue) freshCPUValues.insert(key);
      if(!success && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
      {
        fprintf(stderr,"Metal descriptor slot rejected: buffer=%s offset=%llu generation=%llu event=%u type=%u bytes=%zu consumed=%d newBacking=%d streamOffset=%llu\n",
            ToStr(buffer).c_str(),(unsigned long long)offset,(unsigned long long)generation,event,type,data.size(),consumed,newBacking,(unsigned long long)chunkOffset);
        fprintf(stderr,"Metal descriptor CPU proof: draws=%u dispatches=%u borrowers=%zu modified=%d gpuWritten=%d fullEntry=%d\n",
            drawCount,dispatchCount,resourceCommands[buffer].size(),modifiedBuffers.count(buffer)!=0,
            m_DescriptorGPUWrittenBuffers.count(buffer)!=0,ValidDescriptorCPUWrite(buffer,offset,data.size()));
        fprintf(stderr,"Metal descriptor slot lifetime: borrowed=%d written=%d opaque=%d live=%d empty=%d uncommitted:",
            int(borrowedSlots.count(key)),int(writtenSlots.count(key)),int(opaqueTableWrites.count(buffer)),
            existing!=m_DescriptorSlotShadow.end() && existing->second.live,
            existing!=m_DescriptorSlotShadow.end() && existing->second.data.empty());
        for(ResourceId consumer:resourceCommands[buffer])if(!committed.count(consumer))
          fprintf(stderr," %s(live=%d)",ToStr(consumer).c_str(),hasLiveEncoder(consumer));
        fprintf(stderr,"\n");
      }
    }
    else if(chunk == MetalChunk::MTLBuffer_DescriptorSlotBinding)
    {
      ResourceId buffer, resource; uint64_t offset = 0, memberOffset = 0; uint32_t kind = 0;
      scan.Serialise("buffer"_lit, buffer); scan.Serialise("offset"_lit, offset);
      scan.Serialise("resource"_lit, resource); scan.Serialise("kind"_lit, kind);
      scan.Serialise("memberOffset"_lit, memberOffset);
      auto slot = m_DescriptorSlotShadow.find(make_rdcpair(buffer, offset));
      bool newBacking = false;
      for(const auto &pair : m_ValidatedDescriptorBackingAliases) newBacking |= pair.second == buffer;
      success = (!consumed || newBacking || freshCPUValues.count(make_rdcpair(buffer,offset)) ||
          (slot != m_DescriptorSlotShadow.end() && slot->second.gpuExpected)) &&
          ReadDescriptorSlotBinding(buffer, offset, resource, kind, memberOffset);
    }
    else if(chunk == MetalChunk::MTLBuffer_DescriptorSlotProducer)
    {
      const uint64_t position = m_FrameReader->GetOffset();
      ResourceId buffer, encoder, source; uint64_t offset = 0, sourceOffset = 0;
      scan.Serialise("buffer"_lit, buffer); scan.Serialise("offset"_lit, offset); scan.Serialise("encoder"_lit, encoder);
      scan.Serialise("source"_lit, source); scan.Serialise("sourceOffset"_lit, sourceOffset);
      noteResource(buffer, encoder); noteResource(source, encoder);
      m_FrameReader->SetOffset(position);
      // UE's single-thread UpdateDescriptorHandle kernel scatters a bounded list.
      // Each 24-byte source/destination still needs its exact logical slot, value,
      // type and Native source proof; do not expand ordinary blit allowances.
      success = success && liveEncoders.count(encoder) &&
          (m_DescriptorCoverage >= 45 ? ++producerCount <= 256 : ++copyCount <= 2) &&
          Serialise_DescriptorSlotProducer(scan, ResourceId(), 0, ResourceId(), ResourceId(), 0);
      if(success) gpuCopyEncoders[make_rdcpair(buffer, offset)] = encoder;
    }
    else if(chunk == MetalChunk::MTLCommandBuffer_blitCommandEncoder ||
            chunk == MetalChunk::MTLCommandBuffer_blitCommandEncoderWithDescriptor)
    {
      ResourceId command, encoder;
      scan.Serialise("CommandBuffer"_lit, command); scan.Serialise("BlitCommandEncoder"_lit, encoder);
      success = (m_DescriptorCoverage == 5 || (m_DescriptorCoverage >= 30 &&
                 (m_DescriptorCoverage >= 32 || m_DescriptorPreludeBlitEncoders.count(encoder)) &&
                 knownEncoders.insert(encoder).second && validEncoderCommand(command))) && encoder != ResourceId() && liveBlits.insert(encoder).second;
      if(success && m_DescriptorCoverage >= 30) encoderCommands[encoder] = command;
      if(m_DescriptorCoverage >= 52) m_DescriptorPartialCopySubmissions[command].chunks.push_back(chunkOffset);
    }
    else if(chunk == MetalChunk::MTLBlitCommandEncoder_endEncoding)
    {
      ResourceId encoder; scan.Serialise("BlitCommandEncoder"_lit, encoder);
      success = (m_DescriptorCoverage == 5 || (m_DescriptorCoverage >= 30 &&
                 (m_DescriptorCoverage >= 32 || m_DescriptorPreludeBlitEncoders.count(encoder)))) && liveBlits.erase(encoder) == 1;
      if(m_DescriptorCoverage >= 52) m_DescriptorPartialCopySubmissions[encoderCommands[encoder]].chunks.push_back(chunkOffset);
      if(success && m_DescriptorCoverage >= 30) { encoderCommands.erase(encoder);blitDebugDepth.erase(encoder); }
    }
    else if(m_DescriptorCoverage >= 30 && chunk == MetalChunk::MTLBlitCommandEncoder_setLabel)
    {
      ResourceId encoder; scan.Serialise("BlitCommandEncoder"_lit, encoder);
      success = (m_DescriptorCoverage >= 32 || m_DescriptorPreludeBlitEncoders.count(encoder)) && liveBlits.count(encoder);
    }
    else if(m_DescriptorCoverage>=65 && (chunk==MetalChunk::MTLBlitCommandEncoder_pushDebugGroup ||
        chunk==MetalChunk::MTLBlitCommandEncoder_popDebugGroup || chunk==MetalChunk::MTLBlitCommandEncoder_insertDebugSignpost))
    {
      ResourceId encoder;scan.Serialise("BlitCommandEncoder"_lit,encoder);
      success=liveBlits.count(encoder)!=0;
      if(success && chunk==MetalChunk::MTLBlitCommandEncoder_pushDebugGroup)
        success=++blitDebugDepth[encoder]<=64;
      else if(success && chunk==MetalChunk::MTLBlitCommandEncoder_popDebugGroup)
      {
        success=blitDebugDepth[encoder]!=0;
        if(success)blitDebugDepth[encoder]--;
      }
      if(success && m_DescriptorCoverage>=52)
        m_DescriptorPartialCopySubmissions[encoderCommands[encoder]].chunks.push_back(chunkOffset);
    }
    else if(m_DescriptorCoverage >= 65 &&
       (chunk == MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toTexture ||
        chunk == MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toTexture_options))
    {
      ResourceId encoder, source, destination;
      uint64_t offset=0,rowPitch=0,imagePitch=0,slice=0,level=0;
      MTL::Size size;MTL::Origin origin;MTL::BlitOption options=MTL::BlitOptionNone;
      scan.Serialise("BlitCommandEncoder"_lit,encoder);
      scan.Serialise("sourceBuffer"_lit,source);
      scan.Serialise("sourceOffset"_lit,offset);
      scan.Serialise("sourceBytesPerRow"_lit,rowPitch);
      scan.Serialise("sourceBytesPerImage"_lit,imagePitch);
      scan.Serialise("sourceSize"_lit,size);
      scan.Serialise("destinationTexture"_lit,destination);
      scan.Serialise("destinationSlice"_lit,slice);
      scan.Serialise("destinationLevel"_lit,level);
      scan.Serialise("destinationOrigin"_lit,origin);
      if(chunk==MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toTexture_options)
        scan.Serialise("options"_lit,options);
      auto srcObject=GetResourceManager()->GetResource(source,true);
      auto dstObject=GetResourceManager()->GetResource(destination,true);
      MTL::Buffer *src=srcObject && srcObject->m_Type==eResBuffer ? Unwrap((WrappedMTLBuffer *)srcObject):NULL;
      WrappedMTLTexture *dst=dstObject && dstObject->m_Type==eResTexture ? (WrappedMTLTexture *)dstObject:NULL;
      MTL::Texture *texture=Unwrap(dst);
      auto future=m_DescriptorFrameBuffers.find(source);
      const uint64_t length=future!=m_DescriptorFrameBuffers.end()?future->second.length:src?src->length():0;
      const bool shared=future!=m_DescriptorFrameBuffers.end()?
          (future->second.options&0xf0ULL)==MTL::ResourceStorageModeShared:
          src && src->storageMode()==MTL::StorageModeShared;
      success=liveBlits.count(encoder) && ++plainCopyCount<=256 && !isTable(source) &&
          shared && length && (future==m_DescriptorFrameBuffers.end() || m_DescriptorPreflightLiveBuffers.count(source)) &&
          !m_DescriptorPreflightAliasedBuffers.count(source) && !m_DescriptorPreflightAliasedBuffers.count(destination) &&
          texture && dstObject->m_Device==this && texture->textureType()==MTL::TextureType2D &&
          texture->storageMode()==MTL::StorageModePrivate && texture->hazardTrackingMode()==MTL::HazardTrackingModeTracked &&
          (texture->pixelFormat()==MTL::PixelFormatR8Unorm || texture->pixelFormat()==MTL::PixelFormatBGRA8Unorm_sRGB) &&
          texture->width()<=2048 && texture->height()<=2048 && HasReplayTextureInitialContents(destination) &&
          ValidateMetalLinearTextureCopy(dst,slice,level,origin,size,length,offset,rowPitch,imagePitch,options);
      uint64_t footprint=0;
      if(success) {
        const uint64_t bytes=texture->pixelFormat()==MTL::PixelFormatR8Unorm?1:4;
        footprint=(size.height-1)*rowPitch+size.width*bytes;
        success &= footprint<=16ULL*1024*1024-plainCopyBytes;
      }
      if(success) {
        plainCopyBytes+=footprint;
        noteResource(source,encoder);noteResource(destination,encoder);
        modifiedBuffers.insert(destination);opaqueWrites.insert(destination);
        pendingWork.insert(encoderCommands[encoder]);
        m_DescriptorPartialCopySubmissions[encoderCommands[encoder]].valid=false;
      }
    }
    else if(m_DescriptorCoverage >= 65 && chunk == MetalChunk::MTLBuffer_setPurgeableState)
    {
      ResourceId buffer; uint32_t state = 0;
      scan.Serialise("Buffer"_lit,buffer);scan.Serialise("State"_lit,state);
      auto future=m_DescriptorFrameBuffers.find(buffer);
      success = state==MTL::PurgeableStateEmpty && m_TerminalFramePurgeableBuffers.count(buffer) &&
          future!=m_DescriptorFrameBuffers.end() && future->second.heap==ResourceId() &&
          (future->second.options==MTL::ResourceStorageModeShared ||
           future->second.options==(MTL::ResourceStorageModeShared|MTL::ResourceHazardTrackingModeTracked)) &&
          m_DescriptorPreflightLiveBuffers.count(buffer) && !isTable(buffer) &&
          !m_DescriptorGPUWrittenBuffers.count(buffer) && !m_DescriptorPreflightAliasedBuffers.count(buffer);
      for(ResourceId command:resourceCommands[buffer]) success &= committed.count(command)!=0;
      // Core's whole-frame terminal-use scan and completion-deferred Native Empty
      // remain authoritative; do not discard bytes or release an in-flight object here.
    }
    else if(chunk == MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toBuffer)
    {
      ResourceId encoder, source, destination; uint64_t sourceOffset = 0, destinationOffset = 0, size = 0;
      scan.Serialise("BlitCommandEncoder"_lit, encoder); scan.Serialise("sourceBuffer"_lit, source);
      scan.Serialise("sourceOffset"_lit, sourceOffset); scan.Serialise("destinationBuffer"_lit, destination);
      scan.Serialise("destinationOffset"_lit, destinationOffset); scan.Serialise("size"_lit, size);
      if((m_DescriptorCoverage >= 32 && !isTable(source) && !isTable(destination)) ||
         (m_DescriptorCoverage >= 30 && m_DescriptorPreludeBlitEncoders.count(encoder)))
      {
        const uint64_t budget = m_DescriptorCoverage >= 47 ? 16ULL * 1024 * 1024 : 64ULL * 1024;
        success = size <= budget - plainCopyBytes &&
            ++plainCopyCount <= (m_DescriptorCoverage >= 47 ? 256U : 16U) && liveBlits.count(encoder) &&
            (!m_DescriptorFrameBuffers.count(source) || m_DescriptorPreflightLiveBuffers.count(source)) &&
            (!m_DescriptorFrameBuffers.count(destination) || m_DescriptorPreflightLiveBuffers.count(destination)) &&
            !m_DescriptorPreflightAliasedBuffers.count(source) && !m_DescriptorPreflightAliasedBuffers.count(destination) &&
            TrackDescriptorGPUCopy(source, sourceOffset, destination, destinationOffset, size);
        if(!success && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal plain buffer copy proof: encoder=%s source=%s destination=%s offsets=%llu/%llu bytes=%llu total=%llu count=%u live=%d aliased=%d/%d\n",
              ToStr(encoder).c_str(),ToStr(source).c_str(),ToStr(destination).c_str(),
              (unsigned long long)sourceOffset,(unsigned long long)destinationOffset,
              (unsigned long long)size,(unsigned long long)plainCopyBytes,plainCopyCount,
              int(liveBlits.count(encoder)),int(m_DescriptorPreflightAliasedBuffers.count(source)),
              int(m_DescriptorPreflightAliasedBuffers.count(destination)));
        if(success)
        {
          noteResource(source, encoder); noteResource(destination, encoder);
          pendingWork.insert(encoderCommands[encoder]); modifiedBuffers.insert(destination);
          plainCopyBytes += size;
          if(m_DescriptorCoverage >= 52) {
            auto &plan = m_DescriptorPartialCopySubmissions[encoderCommands[encoder]]; bytebuf known;
            plan.valid &= readIndexUpload(source, encoderCommands[encoder], sourceOffset, size, known);
            plan.copies++; plan.chunks.push_back(chunkOffset);
            plan.buffers.insert(source); plan.buffers.insert(destination);
          }
          if(m_DescriptorCoverage >= 50 && frameIndexCandidates.count(destination)) {
            auto target = m_DescriptorFrameBuffers.find(destination); bytebuf copied;
            success &= target != m_DescriptorFrameBuffers.end() && target->second.length <= 64 * 1024 &&
                target->second.options == (MTL::ResourceStorageModePrivate | MTL::ResourceHazardTrackingModeTracked) &&
                !opaqueWrites.count(destination) && readIndexUpload(source, encoderCommands[encoder], sourceOffset, size, copied);
            if(success) {
              if(m_DescriptorCoverage >= 51)
                submissionIndexOperations[encoderCommands[encoder]].push_back(
                    {destination, destinationOffset, copied, 0, 0, 0});
              else {
                auto &contents = indexContents[destination]; auto &known = indexKnownBytes[destination];
                contents.resize(target->second.length);
                if(known.empty()) { known.resize(target->second.length); memset(known.data(), 0, known.size()); }
                memcpy(contents.data() + destinationOffset, copied.data(), size); memset(known.data() + destinationOffset, 1, size);
              }
              copiedFrameIndices.insert(destination); indexCopyCommands[destination].insert(encoderCommands[encoder]);
            }
          }
        }
      }
      else {
        if(m_DescriptorCoverage >= 52) m_DescriptorPartialCopySubmissions[encoderCommands[encoder]].valid = false;
        success = ++copyCount <= 2 && liveBlits.count(encoder) && TrackDescriptorGPUCopy(source, sourceOffset, destination, destinationOffset, size);
      }
    }
    else if(chunk == MetalChunk::MTLCommandEncoder_DescriptorInlineLayout)
    {
      const uint64_t position = m_FrameReader->GetOffset();
      ResourceId encoder; uint32_t stage = 0; scan.Serialise("encoder"_lit, encoder); scan.Serialise("stage"_lit, stage);
      m_FrameReader->SetOffset(position);
      success = (stage == 0 ? liveEncoders.count(encoder) : (m_DescriptorCoverage >= 9 && stage <= (m_DescriptorCoverage >= 66 ? 4U : 2U) && liveRenders.count(encoder))) && Serialise_DescriptorInlineLayout(scan, ResourceId(), 0, 0, 0, 0);
    }
    else if(chunk == MetalChunk::MTLCommandEncoder_DescriptorInlineBinding)
      success = Serialise_DescriptorInlineBinding(scan, ResourceId(), 0, 0, 0, ResourceId(), 0);
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_setBytes)
    {
      ResourceId encoder; uint64_t index = 0; rdcarray<byte> data;
      scan.Serialise("ComputeCommandEncoder"_lit, encoder); scan.Serialise("data"_lit, data);
      scan.Serialise("index"_lit, index);
      for(const auto &source : m_DescriptorInlineShadow[make_rdcpair(encoder, index)].sources)
        noteResource(source.second.resource, encoder);
      success = liveEncoders.count(encoder) && RelocateDescriptorInlineShadow(encoder, 0, index, data);
      if(success)
      {
        appliedInline[encoder].insert(index);
        computes[encoder].buffers.erase((uint32_t)index);
        computes[encoder].bytes[(uint32_t)index] = data.size();
      }
    }
    else
    {
      rdcstr name = GetChunkName((uint32_t)chunk);
      const bool dispatch = DescriptorChunkStartsWith(name, "MTLComputeCommandEncoder::dispatch");
      if(dispatch || chunk == MetalChunk::MTLCommandBuffer_commit)
      {
        ResourceId dispatchEncoder;
        if((dispatch || m_DescriptorCoverage < 10) && !m_DescriptorGPUCopyExpected.empty()) success = false;
        if(m_DescriptorCoverage >= 10 && chunk == MetalChunk::MTLCommandBuffer_commit)
        {
          ResourceId command; scan.Serialise("CommandBuffer"_lit, command);
          success &= commands.count(command) && !committed.count(command) &&
              (m_DescriptorCoverage >= 15 ? !hasLiveEncoder(command) :
                command == currentCommand && liveEncoders.empty() && liveRenders.empty() && liveBlits.empty()) &&
              inlineCompleteForCommand(command) && gpuCopyCompleteForCommand(command);
          if(m_DescriptorCoverage >= 15)
          {
            // A commit-time Shared snapshot may wait on earlier submissions. Never
            // submit past an uncommitted native enqueue reservation on this queue.
            for(ResourceId reservation : reservationOrder)
            {
              if(reservation == command) break;
              if(!committed.count(reservation)) success = false;
            }
          }
          if(m_DescriptorCoverage >= 19)
            for(const auto &signal : commandSignals[command])
            {
              success &= commandFirstSignals[command][signal.first] > submittedSignals[signal.first];
              submittedSignals[signal.first] = signal.second;
            }
          if(m_DescriptorCoverage >= 52) m_DescriptorPartialCopySubmissions[command].chunks.push_back(chunkOffset);
          if(success && m_DescriptorCoverage >= 51) success = validateSubmissionIndices(command);
          if(success)
          {
            committed.insert(command);
            submissionOrder.push_back(command);
            pendingWork.erase(command);
          }
        }

        if(dispatch)
        {
          bool dispatchRuns=false;
          ResourceId encoder; scan.Serialise("ComputeCommandEncoder"_lit, encoder);
          dispatchEncoder=encoder;
          success &= ++dispatchCount <= (m_DescriptorCoverage >= 46 ? 128U : 4U) && liveEncoders.count(encoder) && !appliedInline[encoder].empty() &&
              GetReplay()->ValidateComputeBufferSnapshot(computes[encoder].pipeline,
                  computes[encoder].buffers, computes[encoder].bytes);
          const bool indirectDispatch = m_DescriptorCoverage >= 57 &&
              chunk == MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups_indirect;
          if(chunk != MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups && !indirectDispatch) success = false;
          else
          {
            MTL::Size groups={}, threads={};
            if(indirectDispatch)
            {
              ResourceId arguments; uint64_t offset=0;
              scan.Serialise("indirectBuffer"_lit, arguments);
              scan.Serialise("indirectBufferOffset"_lit, offset);
              const auto evidence=m_CapturedComputeIndirectArguments.find(
                  make_rdcpair(encoder,capturedIndirectOrdinals[encoder]++));
              auto object=GetResourceManager()->GetResource(arguments,true);
              MTL::Buffer *native=object && object->m_Type==eResBuffer ? Unwrap((WrappedMTLBuffer *)object) : NULL;
              auto future=m_DescriptorFrameBuffers.find(arguments);
              const bool frameArguments=future!=m_DescriptorFrameBuffers.end();
              const uint64_t length=frameArguments?future->second.length:native?native->length():0;
              success &= m_HasCapturedComputeIndirectArguments && evidence!=m_CapturedComputeIndirectArguments.end() &&
                  (!frameArguments || m_DescriptorPreflightLiveBuffers.count(arguments)) &&
                  (frameArguments || native) && length && length<=1024*1024 && !(offset&3) &&
                  offset<=length && 12<=length-offset && !m_DescriptorPreflightAliasedBuffers.count(arguments);
              for(const auto &table:m_DescriptorTables) success &= table.buffer!=arguments;
              if(evidence!=m_CapturedComputeIndirectArguments.end() && evidence->second.groups.size()==3)
              {
                success &= evidence->second.command==encoderCommands[encoder] &&
                    evidence->second.buffer==arguments && evidence->second.offset==offset;
                groups=MTL::Size(evidence->second.groups[0],evidence->second.groups[1],evidence->second.groups[2]);
              }
              else success=false;
              noteResource(arguments,encoder);
            }
            else scan.Serialise("groups"_lit, groups);
            scan.Serialise("threadsPerGroup"_lit, threads);
            dispatchRuns=groups.width && groups.height && groups.depth;
            if(m_DescriptorCoverage >= 46)
            {
              // Reuse the normal dispatch compiler/device/threadgroup checks on
              // this CPU snapshot. Dynamic threadgroup bindings remain outside
              // the sourced contract until they have a corresponding snapshot.
              success &= GetReplay()->ValidateComputeThreadgroupSnapshot(
                  computes[encoder].pipeline, threads, {});
              uint64_t work = 1;
              const uint64_t dimensions[] = {groups.width, groups.height, groups.depth,
                  threads.width, threads.height, threads.depth};
              for(uint64_t dimension : dimensions)
              {
                if((!dimension && !indirectDispatch) || dimension>262144 ||
                   (dimension && work && dimension>262144/work)) { success = false; break; }
                work *= dimension;
              }
              success &= work <= 8ULL * 1024 * 1024 - dispatchWork;
              if(success) dispatchWork += work;
            }
            else
              success &= groups.width == 1 && groups.height == 1 && groups.depth == 1 &&
                  threads.width == 1 && threads.height == 1 && threads.depth == 1;
          }
          if(m_DescriptorCoverage >= 24)
            for(const auto &binding : computes[encoder].buffers)
            {
              auto object = GetResourceManager()->GetResource(binding.second.resourceId, true);
              MTL::Buffer *native = object && object->m_Type == eResBuffer ? Unwrap((WrappedMTLBuffer *)object) : NULL;
              if(native && native->storageMode() == MTL::StorageModePrivate &&
                 GetReplay()->IsComputeBufferReadOnly(computes[encoder].pipeline, binding.first) &&
                 !modifiedBuffers.count(binding.second.resourceId))
              {
                auto initial = m_ReplayBufferInitialContents.find(binding.second.resourceId);
                success &= initial != m_ReplayBufferInitialContents.end() && initial->second.size() == native->length();
              }
            }
          if(!success && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          {
            fprintf(stderr,"Metal compute CPU proof: encoder=%s pipeline=%s count=%u live=%d inline=%zu buffers=%zu bytes=%zu snapshot=%d\n",
                ToStr(encoder).c_str(),ToStr(computes[encoder].pipeline).c_str(),dispatchCount,
                liveEncoders.count(encoder)!=0,appliedInline[encoder].size(),computes[encoder].buffers.size(),
                computes[encoder].bytes.size(),GetReplay()->ValidateComputeBufferSnapshot(computes[encoder].pipeline,
                  computes[encoder].buffers,computes[encoder].bytes));
            for(const auto &binding:computes[encoder].buffers)
              fprintf(stderr,"Metal compute buffer proof: slot=%u buffer=%s offset=%llu bytes=%llu\n",binding.first,
                  ToStr(binding.second.resourceId).c_str(),(unsigned long long)binding.second.byteOffset,
                  (unsigned long long)binding.second.byteSize);
          }
          success &= validateDrawableReads(encoder);
          if(success) NoteDescriptorDispatch(encoder);
          if(success && m_DescriptorCoverage>=64)
            writtenSlots.insert(producerSlots[encoder].begin(),producerSlots[encoder].end());
          for(ResourceId resource : encoderResources[encoder]) noteResource(resource, encoder);
          if(success && dispatchRuns) noteSubmissionSlots(encoder);
          if(success && m_DescriptorCoverage >= 39)
            m_DescriptorValidatedComputeResources[encoder] = encoderResources[encoder];
          if(success && m_DescriptorCoverage>=65 && dispatchRuns && producerDestinations[encoder].empty())
          {
            bool active=false;
            for(const auto &b:computes[encoder].buffers)
              active |= GetReplay()->IsComputeBufferActive(computes[encoder].pipeline,b.first);
            for(const auto &b:computes[encoder].bytes)
              active |= GetReplay()->IsComputeBufferActive(computes[encoder].pipeline,b.first);
            if(active)
              for(const auto &slot:m_DescriptorSlotShadow)
                if(slot.second.live && slot.second.type==5 && !slot.second.data.empty() &&
                   encoderResources[encoder].count(slot.first.first))
                {
                  auto source=slot.second.sources.find(1);
                  if(source==slot.second.sources.end())continue;
                  ResourceId texture=source->second.resource;
                  const ResourceId parent=viewBacking(texture);
                  if(parent!=ResourceId())texture=parent;
                  if(m_DescriptorFrameTextures.count(texture) && m_DescriptorPreflightLiveTextures.count(texture))
                    colorWriteCommands[texture].insert(encoderCommands[encoder]);
                }
          }
          if(m_DescriptorCoverage >= 15) pendingWork.insert(encoderCommands[encoder]);
          if(m_DescriptorCoverage >= 14)
            for(const auto &binding : computes[encoder].buffers)
              if(!GetReplay()->IsComputeBufferReadOnly(computes[encoder].pipeline, binding.first)) {
                modifiedBuffers.insert(binding.second.resourceId); opaqueWrites.insert(binding.second.resourceId);
                if(m_DescriptorCoverage>=64 && m_DescriptorGPUWrittenBuffers.count(binding.second.resourceId) &&
                   !producerDestinations[encoder].count(binding.second.resourceId))
                  opaqueTableWrites.insert(binding.second.resourceId);
              }
        }
        consumed = !(m_DescriptorCoverage >= 12 && chunk == MetalChunk::MTLCommandBuffer_commit);
        if(m_DescriptorCoverage >= 15 && chunk == MetalChunk::MTLCommandBuffer_commit)
          consumed = !pendingWork.empty();
        // Shader descriptor reads were checked at each original draw/dispatch.
        // A submission does not consume every allocated slot: CPU threads may
        // allocate an as-yet-unwritten descriptor while a plain blit is queued.
        // D3D12/Vulkan also validate consumption rather than unused heap slots.
        if(dispatch || m_DescriptorCoverage<61)
          for(const auto &entry : m_DescriptorSlotShadow)
          {
            if(m_DescriptorCoverage>=63 && dispatch && entry.second.data.empty() &&
               m_DescriptorGPUWrittenBuffers.count(entry.first.first) &&
               producerDestinations[dispatchEncoder].count(entry.first.first)) continue;
            bytebuf patched;
            if(entry.second.live && !PatchDescriptorSlot(entry.second, patched))
            {
              success = false;
              if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
                fprintf(stderr,"Metal descriptor consumer slot: buffer=%s offset=%llu generation=%llu bytes=%zu gpuExpected=%d\n",
                    ToStr(entry.first.first).c_str(),(unsigned long long)entry.first.second,
                    (unsigned long long)entry.second.generation,entry.second.data.size(),entry.second.gpuExpected);
            }
          }
      }
      if(DescriptorChunkStartsWith(name, "MTLCommandQueue::commandBuffer") &&
         (m_DescriptorCoverage >= 10 || ++commandCount > 1))
        success = false;
      if(m_DescriptorCoverage >= 12 &&
         (name.find("Fence") >= 0 || name.find("Event") >= 0)) success = false;
      if(DescriptorChunkStartsWith(name, "MTLRenderCommandEncoder::draw") ||
         name.find("::setVertexBytes") >= 0 || name.find("::setFragmentBytes") >= 0 ||
         name.find("::setVertexBuffer") >= 0 || name.find("::setFragmentBuffer") >= 0 ||
         name.find("::setMeshBytes") >= 0 || name.find("::setObjectBytes") >= 0 ||
         name.find("::newBuffer") >= 0 || name.find("::newTexture") >= 0 ||
         name.find("::newHeap") >= 0 || name.find("::makeAliasable") >= 0 ||
         name.find("::setPurgeableState") >= 0 ||
         chunk == MetalChunk::MTLComputeCommandEncoder_DeclareDescriptorBytes ||
         chunk == MetalChunk::MTLBuffer_DescriptorCPUWrite ||
         chunk == MetalChunk::MTLComputeCommandEncoder_setBuffers ||
         chunk == MetalChunk::MTLComputeCommandEncoder_setBufferOffset ||
         chunk == MetalChunk::MTLBuffer_didModifyRange ||
         DescriptorChunkStartsWith(name, "MTLBlitCommandEncoder::") ||
         DescriptorChunkStartsWith(name, "MTLIndirect"))
        success = false;
    }
    if(!success && !Process::GetEnvVariable("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT").empty())
      fprintf(stderr, "Sourced Metal descriptor preflight rejected: %s streamOffset=%llu\n", GetChunkName((uint32_t)chunk).c_str(),(unsigned long long)chunkOffset);
    scan.EndChunk();
  }
  success &= (m_DescriptorCoverage != 66 || meshDrawCount != 0) &&
             !scan.IsErrored() && m_DescriptorInlineShadow.empty() &&
             m_DescriptorGPUCopyExpected.empty() && liveBlits.empty() && liveRenders.empty() && liveEncoders.empty() &&
             liveParallelRenders.empty() && activeChildren.empty() && childParents.empty() &&
             (m_DescriptorCoverage < 10 || (committed.size() == commands.size() &&
              (m_DescriptorCoverage >= 12 || completed.size() == commands.size())));
  for(const auto &buffer : m_DescriptorSubmissionInitialBuffers)
  {
    bool initialized = false;
    for(ResourceId command : submissionOrder)
    {
      for(const CPUUpdate &update : submissionUpdates[command])
        if(update.buffer == buffer.first && update.start == 0 && update.data.size() == buffer.second)
          initialized = true;
      if(resourceCommands[buffer.first].count(command) && !initialized)
      {
        success = false;
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal submission initial buffer rejection: resource=%s command=%s bytes=%llu\n",
              ToStr(buffer.first).c_str(),ToStr(command).c_str(),(unsigned long long)buffer.second);
      }
    }
  }
  for(ResourceId indexBuffer : indexBuffers)
  {
    success &= (!modifiedBuffers.count(indexBuffer) || copiedFrameIndices.count(indexBuffer)) && !opaqueWrites.count(indexBuffer);
    const auto contents = indexContents.find(indexBuffer);
    for(const CPUUpdate &update : cpuUpdates)
      if(update.buffer == indexBuffer)
        success &= contents != indexContents.end() && update.start <= contents->second.size() &&
            update.data.size() <= contents->second.size() - update.start &&
            memcmp(contents->second.data() + update.start, update.data.data(), update.data.size()) == 0;
  }
  if(success && m_DescriptorCoverage >= 52) m_DescriptorSubmissionOrder = submissionOrder;
  m_DescriptorGPUCopyExpected.clear();
  m_DescriptorDispatches.clear();
  m_DescriptorSlotShadow = m_DescriptorSlotInitial;
  m_DescriptorPreflightLiveBuffers.clear();
  m_DescriptorPreflightLiveTables.clear();
  m_DescriptorPreflightAliasedBuffers.clear();
  m_DescriptorInlineShadow.clear();
  m_DescriptorPreflight = false;
  if(!success) m_DescriptorSubmissionSlots.clear();
  if(!success) { m_ValidatedDescriptorBackingAliases.clear(); m_DescriptorBackingAliasConsumers.clear(); m_RetiredTextureAliasConsumers.clear(); m_DescriptorValidatedComputeResources.clear(); m_DescriptorSubmissionSnapshotOwners.clear(); m_DescriptorPartialCopySubmissions.clear(); m_DescriptorFrameBufferBirthOffsets.clear(); m_DescriptorSubmissionOrder.clear(); }
  m_FrameReader->SetOffset(0);
  return success;
}

bool WrappedMTLDevice::ApplyDescriptorCPUUpdate(ResourceId buffer, uint64_t start, const bytebuf &data)
{
  if(m_DescriptorCoverage >= 4)
    return m_DescriptorPreflight ||
        (m_DescriptorCoverage >= 65 ?
         (OverlayDescriptorSlotBuffer(buffer, false, start, data.size()) &&
          OverlayAliasedDescriptorTables(buffer, start, data.size())) :
         (OverlayDescriptorSlotBuffer(buffer) && OverlayAliasedDescriptorTables(buffer)));
  auto shadow = m_DescriptorRawContents.find(buffer);
  if(shadow == m_DescriptorRawContents.end())
    return true;
  if(start > shadow->second.size() || data.size() > shadow->second.size() - start)
    return false;
  if(m_DescriptorGPUWrittenBuffers.count(buffer) &&
     !ValidDescriptorCPUWrite(buffer, start, data.size()))
    return false;
  // Partial-byte diffs must merge into capture-process bytes, never replay addresses.
  memcpy(shadow->second.data() + start, data.data(), data.size());
  // Preserve GPU output in all other entries. Restoring the full CPU shadow would undo
  // already executed descriptor copies and silently change later shader bindings.
  return m_DescriptorGPUWrittenBuffers.count(buffer) ?
      RestoreDescriptorTable(buffer, shadow->second, start, data.size()) :
      RestoreDescriptorTable(buffer, shadow->second);
}

bool WrappedMTLDevice::ValidateDescriptorFrame()
{
  if(m_DescriptorCoverage >= 4)
    return ValidateDescriptorSlotFrame();
  if(m_DescriptorTables.empty())
    return true;
  struct ValidationScope
  {
    bool &flag;
    ValidationScope(bool &f) : flag(f) { flag = true; }
    ~ValidationScope() { flag = false; }
  } validation(m_DescriptorPreflight);
  m_DescriptorPreflightLiveBuffers.clear();
  m_DescriptorPreflightLiveViews.clear();
  std::map<ResourceId, rdcarray<rdcpair<uint64_t, uint64_t>>> futureHeapRanges;
  struct ComputeState { ResourceId pipeline; std::map<uint32_t, ResourceId> buffers; };
  std::map<ResourceId, ComputeState> states;
  std::map<ResourceId, std::set<uint64_t>> inlineDeclared;
  std::set<ResourceId> inlineLiveEncoders;
  std::set<ResourceId> tables;
  for(const DescriptorTable &layout : m_DescriptorTables)
    tables.insert(layout.buffer);
  ReadSerialiser scan(m_FrameReader, Ownership::Nothing);
  scan.SetVersion(m_SectionVersion);
  m_FrameReader->SetOffset(0);
  while(!m_FrameReader->AtEnd() && !scan.IsErrored())
  {
    MetalChunk chunk = scan.ReadChunk<MetalChunk>();
    if(chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoder ||
       chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDispatchType ||
       chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDescriptor)
    {
      ResourceId command, encoder;
      scan.Serialise("CommandBuffer"_lit, command);
      scan.Serialise("ComputeCommandEncoder"_lit, encoder);
      if(encoder == ResourceId() || !inlineLiveEncoders.insert(encoder).second)
        return false;
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_setComputePipelineState)
    {
      ResourceId encoder, pipeline;
      scan.Serialise("ComputeCommandEncoder"_lit, encoder);
      scan.Serialise("pipeline"_lit, pipeline);
      states[encoder].pipeline = pipeline;
    }
    else if(chunk == MetalChunk::MTLHeap_newBufferWithOffset)
    {
      ResourceId heap, buffer;
      uint64_t length = 0, options = 0, offset = 0;
      scan.Serialise("Heap"_lit, heap);
      scan.Serialise("Buffer"_lit, buffer);
      scan.Serialise("length"_lit, length);
      scan.Serialise("options"_lit, options);
      scan.Serialise("offset"_lit, offset);
      auto recorded = m_DescriptorFrameBuffers.find(buffer);
      WrappedMTLObject *object = GetResourceManager()->GetResource(heap, true);
      if(m_DescriptorCoverage != 3 || recorded == m_DescriptorFrameBuffers.end() ||
         recorded->second.heap != heap || recorded->second.length != length ||
         recorded->second.options != options || recorded->second.offset != offset ||
         !m_DescriptorPreflightLiveBuffers.insert(buffer).second ||
         GetResourceManager()->HasResource(buffer) || !object || object->m_Type != eResHeap || !object->m_Real)
        return false;
      WrappedMTLHeap *wrapped = (WrappedMTLHeap *)object;
      MTL::Heap *native = Unwrap(wrapped);
      const MTL::SizeAndAlign allocation = Unwrap(this)->heapBufferSizeAndAlign(length, (MTL::ResourceOptions)options);
      const MTL::StorageMode storage = (options & MTL::ResourceStorageModePrivate) ?
          MTL::StorageModePrivate : MTL::StorageModeShared;
      if(native->type() != MTL::HeapTypePlacement || native->storageMode() != storage ||
         !allocation.size || !allocation.align || offset % allocation.align || offset > native->size() ||
         allocation.size > native->size() - offset || wrapped->HasPlacementOverlap(offset, offset + allocation.size))
        return false;
      for(const auto &range : futureHeapRanges[heap])
        if(offset < range.second && range.first < offset + allocation.size)
          return false;
      futureHeapRanges[heap].push_back({offset, offset + allocation.size});
    }
    else if(chunk == MetalChunk::MTLBuffer_newTextureWithDescriptor)
    {
      ResourceId buffer, texture;
      RDMTL::TextureDescriptor descriptor;
      uint64_t offset = 0, bytesPerRow = 0;
      scan.Serialise("Buffer"_lit, buffer);
      scan.Serialise("Texture"_lit, texture);
      scan.Serialise("descriptor"_lit, descriptor);
      scan.Serialise("offset"_lit, offset);
      scan.Serialise("bytesPerRow"_lit, bytesPerRow);
      auto recorded = m_DescriptorFrameViews.find(texture);
      if(scan.IsErrored() || m_DescriptorCoverage != 3 || recorded == m_DescriptorFrameViews.end() ||
         recorded->second != buffer || GetResourceManager()->HasResource(texture) ||
         !m_DescriptorPreflightLiveViews.insert(texture).second)
        return false;
      uint64_t length = 0;
      MTL::StorageMode storage = MTL::StorageModeShared;
      auto future = m_DescriptorFrameBuffers.find(buffer);
      if(future != m_DescriptorFrameBuffers.end())
      {
        if(!m_DescriptorPreflightLiveBuffers.count(buffer))
          return false;
        length = future->second.length;
        storage = (future->second.options & MTL::ResourceStorageModePrivate) ?
            MTL::StorageModePrivate : MTL::StorageModeShared;
      }
      else
      {
        WrappedMTLObject *object = GetResourceManager()->GetResource(buffer, true);
        if(!object || object->m_Type != eResBuffer || !object->m_Real)
          return false;
        length = Unwrap((WrappedMTLBuffer *)object)->length();
        storage = Unwrap((WrappedMTLBuffer *)object)->storageMode();
      }
      if(!ValidateMetalBufferTexture(Unwrap(this), descriptor, length, storage, offset, bytesPerRow))
        return false;
    }
    else if(chunk == MetalChunk::MTLBuffer_DescriptorCPUWrite)
    {
      ResourceId buffer;
      uint64_t start = 0;
      bytebuf data;
      scan.Serialise("buffer"_lit, buffer);
      scan.Serialise("start"_lit, start);
      scan.Serialise("data"_lit, data);
      if(scan.IsErrored() || m_DescriptorCoverage < 2 ||
         !m_DescriptorGPUWrittenBuffers.count(buffer) ||
         !ValidDescriptorCPUWrite(buffer, start, data.size()) ||
         !ApplyDescriptorCPUUpdate(buffer, start, data))
        return false;
    }
    else if(chunk == MetalChunk::MTLBuffer_InternalModifyCPUContents)
    {
      ResourceId buffer;
      uint64_t start = 0, size = 0;
      scan.Serialise("Buffer"_lit, buffer);
      scan.Serialise("start"_lit, start);
      scan.Serialise("size"_lit, size);
      if(tables.count(buffer))
      {
        if(m_DescriptorGPUWrittenBuffers.count(buffer))
          return false;
        if(start > m_DescriptorRawContents[buffer].size() ||
           size > m_DescriptorRawContents[buffer].size() - start)
          return false;
        bytebuf data;
        scan.Serialise("data"_lit, data);
        if(scan.IsErrored() || size != data.size() || !ApplyDescriptorCPUUpdate(buffer, start, data))
          return false;
      }
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_DeclareDescriptorBytes)
    {
      ResourceId encoder;
      uint64_t index = 0;
      scan.Serialise("encoder"_lit, encoder); scan.Serialise("index"_lit, index);
      if(!inlineLiveEncoders.count(encoder) || !inlineDeclared[encoder].insert(index).second)
        return false;
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_setBytes)
    {
      ResourceId encoder;
      rdcarray<byte> data;
      uint64_t index = 0;
      scan.Serialise("ComputeCommandEncoder"_lit, encoder);
      scan.Serialise("data"_lit, data); scan.Serialise("index"_lit, index);
      if(scan.IsErrored() || !inlineLiveEncoders.count(encoder) || !inlineDeclared[encoder].count(index) ||
         !RelocateDescriptorBytes(encoder, index, data))
        return false;
      states[encoder].buffers.erase((uint32_t)index);
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_endEncoding)
    {
      ResourceId encoder;
      scan.Serialise("ComputeCommandEncoder"_lit, encoder);
      inlineLiveEncoders.erase(encoder);
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_setBuffer)
    {
      ResourceId encoder, buffer;
      uint64_t offset = 0, index = 0;
      scan.Serialise("ComputeCommandEncoder"_lit, encoder);
      scan.Serialise("buffer"_lit, buffer);
      scan.Serialise("offset"_lit, offset);
      scan.Serialise("index"_lit, index);
      if(index >= 31)
        return false;
      states[encoder].buffers[(uint32_t)index] = buffer;
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups)
    {
      ResourceId encoder;
      scan.Serialise("ComputeCommandEncoder"_lit, encoder);
      for(const auto &binding : states[encoder].buffers)
        if(tables.count(binding.second) &&
           !m_DescriptorGPUWrittenBuffers.count(binding.second) &&
           !GetReplay()->IsComputeBufferReadOnly(states[encoder].pipeline, binding.first))
        {
          RDCERR("GPU write access to explicit descriptor tables is unsupported in v1");
          return false;
        }
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_setBuffers ||
            chunk == MetalChunk::MTLComputeCommandEncoder_setBufferOffset ||
            chunk == MetalChunk::MTLBuffer_didModifyRange)
      return false;
    else
    {
      const rdcstr name = ToStr(chunk);
      if(strstr(name.c_str(), "MTLComputeCommandEncoder::dispatch"))
        return false;
      if(strstr(name.c_str(), "::draw") || strstr(name.c_str(), "makeAliasable") ||
         strstr(name.c_str(), "setPurgeableState") ||
         DescriptorChunkStartsWith(name, "MTLDevice::newBuffer") ||
         DescriptorChunkStartsWith(name, "MTLDevice::newTexture") ||
         DescriptorChunkStartsWith(name, "MTLDevice::newSampler") ||
         DescriptorChunkStartsWith(name, "MTLHeap::newBuffer") ||
         DescriptorChunkStartsWith(name, "MTLHeap::newTexture"))
        return false;
      // v1 is a compute-read path. Blit/render/ICB accesses to table allocations require
      // separate execution-point semantics; conservatively reject before the loading pass.
      if(DescriptorChunkStartsWith(name, "MTLBlitCommandEncoder::") ||
         DescriptorChunkStartsWith(name, "MTLRenderCommandEncoder::") ||
         DescriptorChunkStartsWith(name, "MTLIndirect"))
      {
        bytebuf payload;
        payload.resize(scan.ChunkMetadata().length);
        if(!m_FrameReader->Read(payload.data(), payload.size()))
          return false;
        for(size_t i = 0; i + sizeof(ResourceId) <= payload.size(); i++)
        {
          ResourceId id;
          memcpy(&id, payload.data() + i, sizeof(id));
          if(tables.count(id))
            return false;
        }
      }
    }
    scan.EndChunk();
  }
  const bool valid = !scan.IsErrored();
  m_FrameReader->SetOffset(0);
  return valid;
}
