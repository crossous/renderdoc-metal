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
#include "metal_visible_function_table.h"
#include "metal_function.h"
#include "metal_replay_budget.h"
#include "metal_air_access.h"
#include "metal_descriptor_types.h"
#include "serialise/rdcfile.h"

// Native integer texture operations return integer bits. A converted shader
// can retain an unsigned AIR read/write ABI for a signed integer view. Preserve
// that original format and shader; signedness is not a missing resource or
// address. Floating/normalized and integer families remain distinct.
static bool TextureNumericFamily(char numeric, CompType type)
{
  const bool integer=type==CompType::UInt || type==CompType::SInt;
  return (numeric=='u' || numeric=='s')?integer:numeric=='f' && !integer;
}

// Frame factories are qualified by the same logical restoration layout as
// background images. Protocol coverage does not grant texture combinations.
static bool ValidDescriptorFrameTexture(const RDMTL::TextureDescriptor &d, uint32_t)
{
  uint64_t logicalBytes = 0;
  const uint64_t knownUsage = MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite |
      MTL::TextureUsageRenderTarget | MTL::TextureUsagePixelFormatView | MTL::TextureUsageShaderAtomic;
  return MetalTextureReplayLayout(d, logicalBytes) &&
      d.storageMode == MTL::StorageModePrivate &&
      d.cpuCacheMode == MTL::CPUCacheModeDefaultCache &&
      d.hazardTrackingMode == MTL::HazardTrackingModeTracked &&
      d.resourceOptions == (MTL::ResourceStorageModePrivate | MTL::ResourceHazardTrackingModeTracked) &&
      !(uint64_t(d.usage) & ~knownUsage);
}

static bool ProjectDescriptorFrameTextureView(const RDMTL::TextureDescriptor &parent,
    MTL::PixelFormat format, MTL::TextureType type, NS::Range levels, NS::Range slices,
    MTL::TextureSwizzleChannels swizzle, uint32_t, RDMTL::TextureDescriptor &view)
{
  return ValidDescriptorFrameTexture(parent, 0) &&
      ProjectMetalTextureView(parent, format, type, levels, slices, swizzle, view);
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

// Uniform staging/allocation policy, not protocol-version permission levels.
static uint64_t DescriptorFrameBufferLimit(bool, uint32_t)
{
  return 128ULL * 1024 * 1024;
}

static uint64_t DescriptorTableBufferLimit(uint32_t)
{
  return 32ULL * 1024 * 1024;
}

static uint64_t DescriptorPlainCopyLimit(uint32_t)
{
  return 1024ULL * 1024;
}

static bool ValidDescriptorLayout(uint64_t offset, uint64_t count, uint64_t stride, uint32_t schema)
{
  const uint64_t minimum = schema == 0 || schema >= 4 ? 8 : 24;
  return schema <= 6 && count && count <= 786432 && stride >= minimum && stride <= 4096 &&
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
  if(!strcmp(key, "metal.irComputeReflection"))
  {
    if(!IsBackgroundCapturing(m_State) || type != eRENDERDOC_String || width != 0 ||
       !value->string) return 2;
    const size_t length = strnlen(value->string, 64 * 1024 + 1);
    auto pipeline = GetResourceManager()->FindAnnotationObject(object);
    if(!length || length > 64 * 1024 || !pipeline ||
       pipeline->m_Type != eResComputePipelineState || !pipeline->m_Real) return 2;
    const ResourceId id = GetResID(pipeline);
    const rdcstr reflection(value->string, length);
    MetalIRComputeRuntimeABI abi;
    if(ParseMetalIRComputeRuntimeABI(reflection.c_str(), reflection.size(), abi) ==
       MetalIRComputeABIResult::Invalid) return 2;
    const auto old = m_IRComputeReflections.find(id);
    if(old != m_IRComputeReflections.end()) return old->second == reflection ? 0 : 2;
    if(m_IRComputeReflections.size() >= 4096 ||
       m_IRComputeReflectionBytes > 16 * 1024 * 1024 - length) return 2;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputePipelineState_CaptureIRComputeReflection);
    Serialise_CaptureIRComputeReflection(ser, id, reflection);
    GetRecord(pipeline)->AddChunk(scope.Get());
    m_IRComputeReflections[id] = reflection;
    m_IRComputeReflectionBytes += length;
    return 0;
  }
  if(!strcmp(key, "metal.rayIRHeapEntry"))
  {
    if(!IsBackgroundCapturing(m_State) || m_DescriptorCoverage!=3 ||
       type!=eRENDERDOC_UInt64 || width!=4) return 2;
    auto pipeline=GetResourceManager()->FindAnnotationObject(object);
    if(!pipeline || pipeline->m_Type!=eResComputePipelineState || !pipeline->m_Real ||
       !HasRayIRPipeline(GetResID(pipeline))) return 2;
    RayIRHeapEntry entry={value->vector.uint64[0],value->vector.uint64[1],
        value->vector.uint64[2],value->vector.uint64[3]};
    size_t oldCount=m_RayIRHeapEntries[GetResID(pipeline)].size();
    if(!AddRayIRHeapEntry(GetResID(pipeline),entry,true)) return 2;
    if(m_RayIRHeapEntries[GetResID(pipeline)].size()==oldCount) return 0;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputePipelineState_DeclareRayIRHeapEntry);
    Serialise_DeclareRayIRHeapEntry(ser,GetResID(pipeline),entry.heap,entry.index,entry.kind,entry.bytes);
    GetRecord(pipeline)->AddChunk(scope.Get());return 0;
  }
  if(!strcmp(key, "metal.rayIRGlobalRoot"))
  {
    if(!IsBackgroundCapturing(m_State) || m_DescriptorCoverage != 3 ||
       type != eRENDERDOC_UInt64 || width != 4) return 2;
    auto pipeline = GetResourceManager()->FindAnnotationObject(object);
    if(!pipeline || pipeline->m_Type != eResComputePipelineState || !pipeline->m_Real ||
       !HasRayIRPipeline(GetResID(pipeline))) return 2;
    RayIRLocalRoot root = {value->vector.uint64[0],value->vector.uint64[1],
        value->vector.uint64[2],value->vector.uint64[3]};
    size_t oldCount = m_RayIRGlobalRoots[GetResID(pipeline)].size();
    if(!AddRayIRGlobalRoot(GetResID(pipeline),root,true)) return 2;
    if(m_RayIRGlobalRoots[GetResID(pipeline)].size() == oldCount) return 0;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputePipelineState_DeclareRayIRGlobalRoot);
    Serialise_DeclareRayIRGlobalRoot(ser,GetResID(pipeline),root.offset,root.kind,root.count,root.bytes);
    GetRecord(pipeline)->AddChunk(scope.Get());
    return 0;
  }
  if(!strcmp(key, "metal.rayIRLocalRoot"))
  {
    if(!IsBackgroundCapturing(m_State) || m_DescriptorCoverage != 3 ||
       type != eRENDERDOC_UInt64 || width != 4) return 2;
    auto handle = GetResourceManager()->FindAnnotationObject(object);
    if(!handle || handle->m_Type != eResFunctionHandle || !handle->m_Real ||
       !m_RayIRShaderRoles.count(GetResID(handle))) return 2;
    RayIRLocalRoot root = {value->vector.uint64[0], value->vector.uint64[1],
        value->vector.uint64[2], value->vector.uint64[3]};
    size_t oldCount = m_RayIRLocalRoots[GetResID(handle)].size();
    if(!AddRayIRLocalRoot(GetResID(handle),root,true)) return 2;
    if(m_RayIRLocalRoots[GetResID(handle)].size() == oldCount) return 0;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLFunctionHandle_DeclareRayIRLocalRoot);
    Serialise_DeclareRayIRLocalRoot(ser,GetResID(handle),root.offset,root.kind,root.count,root.bytes);
    GetRecord(handle)->AddChunk(scope.Get());
    return 0;
  }
  if(!strcmp(key, "metal.rayIRShaderRole"))
  {
    if(!IsBackgroundCapturing(m_State) || m_DescriptorCoverage != 3 ||
       type != eRENDERDOC_UInt32 || width != 0 || value->uint32 > 3)
      return 2;
    auto objectHandle = GetResourceManager()->FindAnnotationObject(object);
    if(!objectHandle || objectHandle->m_Type != eResFunctionHandle || !objectHandle->m_Real)
      return 2;
    auto handle = (WrappedMTLFunctionHandle *)objectHandle;
    if(!handle->m_Pipeline || handle->m_Pipeline->m_Type != eResComputePipelineState ||
       !HasRayIRPipeline(GetResID(handle->m_Pipeline)) || handle->m_Stage != 0 ||
       !handle->m_Function || !handle->m_Function->m_Real ||
       Unwrap(handle->m_Function)->functionType() != MTL::FunctionTypeVisible)
      return 2;
    auto old = m_RayIRShaderRoles.find(GetResID(handle));
    if(old != m_RayIRShaderRoles.end()) return old->second == value->uint32 ? 0 : 2;
    if(m_RayIRShaderRoles.size() >= 256) return 2;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLFunctionHandle_DeclareRayIRShaderRole);
    Serialise_DeclareRayIRShaderRole(ser, GetResID(handle), value->uint32);
    GetRecord(handle)->AddChunk(scope.Get());
    m_RayIRShaderRoles[GetResID(handle)] = value->uint32;
    return 0;
  }
  if(!strcmp(key, "metal.rayIRDispatch"))
  {
    if(!IsBackgroundCapturing(m_State) || m_DescriptorCoverage != 3 ||
       type != eRENDERDOC_UInt64 || width != 4 || value->vector.uint64[3] < 2 || value->vector.uint64[3] > 256)
      return 2;
    auto pipeline = GetResourceManager()->FindAnnotationObject(object);
    auto buffer = GetResourceManager()->FindAnnotationObject((void *)(uintptr_t)value->vector.uint64[0]);
    const auto offset = value->vector.uint64[1];
    if(!pipeline || pipeline->m_Type != eResComputePipelineState || !pipeline->m_Real ||
       !buffer || buffer->m_Type != eResBuffer || !buffer->m_Real || offset % 8 ||
       value->vector.uint64[2] != 1 || m_RayIRDispatches.size() >= 64)
      return 2;
    auto native = Unwrap((WrappedMTLBuffer *)buffer);
    if(native->storageMode() != MTL::StorageModeShared || offset > native->length() ||
       152 > native->length() - offset)
      return 2;
    for(const auto &other : m_RayIRDispatches)
      if(other.pipeline == GetResID(pipeline) && other.buffer == GetResID(buffer) && other.offset == offset)
        return 2;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputePipelineState_DeclareRayIRDispatch);
    Serialise_DeclareRayIRDispatch(ser, GetResID(pipeline), GetResID(buffer), offset, value->vector.uint64[3]);
    GetRecord(pipeline)->AddParent(GetRecord(buffer));
    GetRecord(pipeline)->AddChunk(scope.Get());
    m_RayIRDispatches.push_back({GetResID(pipeline), GetResID(buffer), offset, value->vector.uint64[3]});
    return 0;
  }
  if(!strcmp(key, "metal.rayASHeader"))
    return type == eRENDERDOC_UInt64 && width == 4 ? CaptureRayASHeader(object, value) : 2;
  if(!strcmp(key, "metal.irComputeRoot") || !strcmp(key, "metal.irComputeHeapEntry"))
  {
    if(!IsBackgroundCapturing(m_State) || m_DescriptorCoverage!=65 ||
       type!=eRENDERDOC_UInt64 || width!=4) return 2;
    auto pipeline=GetResourceManager()->FindAnnotationObject(object);
    if(!pipeline || pipeline->m_Type!=eResComputePipelineState || !pipeline->m_Real) return 2;
    const ResourceId id=GetResID(pipeline);
    const auto *v=value->vector.uint64;
    CACHE_THREAD_SERIALISER();
    if(!strcmp(key, "metal.irComputeRoot"))
    {
      RayIRLocalRoot root={v[0],v[1],v[2],v[3]};
      const size_t count=m_IRComputeRoots[id].size();
      if(!AddIRComputeRoot(id,root,true)) return 2;
      if(m_IRComputeRoots[id].size()==count) return 0;
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputePipelineState_DeclareIRComputeRoot);
      Serialise_DeclareIRComputeRoot(ser,id,root.offset,root.kind,root.count,root.bytes);
      GetRecord(pipeline)->AddChunk(scope.Get());
    }
    else
    {
      RayIRHeapEntry entry={v[0],v[1],v[2],v[3]};
      const size_t count=m_IRComputeHeapEntries[id].size();
      if(!AddIRComputeHeapEntry(id,entry,true)) return 2;
      if(m_IRComputeHeapEntries[id].size()==count) return 0;
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputePipelineState_DeclareIRComputeHeapEntry);
      Serialise_DeclareIRComputeHeapEntry(ser,id,entry.heap,entry.index,entry.kind,entry.bytes);
      GetRecord(pipeline)->AddChunk(scope.Get());
    }
    return 0;
  }
  if(!strcmp(key, "metal.rayQueryHeapCBVRoot"))
  {
    if(!IsBackgroundCapturing(m_State) || m_DescriptorCoverage!=65 ||
       type!=eRENDERDOC_UInt64 || width!=4) return 2;
    auto pipeline=GetResourceManager()->FindAnnotationObject(object);
    if(!pipeline || pipeline->m_Type!=eResComputePipelineState || !pipeline->m_Real) return 2;
    const ResourceId id=GetResID(pipeline);
    RayQueryHeapCBVRoot root={value->vector.uint64[0],value->vector.uint64[1],
                             value->vector.uint64[2],value->vector.uint64[3]};
    const size_t count=m_RayQueryHeapCBVRoots[id].size();
    if(!AddRayQueryHeapCBVRoot(id,root,true)) return 2;
    if(m_RayQueryHeapCBVRoots[id].size()==count) return 0;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputePipelineState_DeclareRayQueryHeapCBVRoot);
    Serialise_DeclareRayQueryHeapCBVRoot(ser,id,root.offset,root.bytes,root.outputSlot,root.rootCount);
    GetRecord(pipeline)->AddChunk(scope.Get());return 0;
  }
  if(!strcmp(key, "metal.rayQueryHeapDispatch"))
  {
    if(!IsBackgroundCapturing(m_State) || m_DescriptorCoverage!=65 ||
       type!=eRENDERDOC_UInt64 || width!=4 || value->vector.uint64[3]!=0 ||
       m_RayQueryHeapDispatches.size()>=64) return 2;
    auto pipeline=GetResourceManager()->FindAnnotationObject(object);
    auto heap=GetResourceManager()->FindAnnotationObject((void *)(uintptr_t)value->vector.uint64[0]);
    auto output=GetResourceManager()->FindAnnotationObject((void *)(uintptr_t)value->vector.uint64[2]);
    const uint64_t slot=value->vector.uint64[1];
    if(!pipeline || pipeline->m_Type!=eResComputePipelineState || !pipeline->m_Real ||
       HasRayIRPipeline(GetResID(pipeline)) || HasRayQueryPipeline(GetResID(pipeline)) ||
       !heap || !output || heap==output || slot%24) return 2;
    if(heap->m_Type!=eResBuffer || !heap->m_Real || heap->m_CapturedAliasable ||
       !IsRayQueryHeapOutputResource(output)) return 2;
    auto native=Unwrap((WrappedMTLBuffer *)heap);
    if(native->storageMode()!=MTL::StorageModeShared || native->heap() || native->length()>16*1024 ||
       slot>native->length() || 24>native->length()-slot) return 2;
    for(const auto &d:m_RayQueryHeapDispatches) if(d.pipeline==GetResID(pipeline)) return 2;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputePipelineState_DeclareRayQueryHeapDispatch);
    Serialise_DeclareRayQueryHeapDispatch(ser,GetResID(pipeline),GetResID(heap),slot,GetResID(output));
    GetRecord(pipeline)->AddParent(GetRecord(heap)); GetRecord(pipeline)->AddParent(GetRecord(output));
    GetRecord(pipeline)->AddChunk(scope.Get());
    m_RayQueryHeapDispatches.push_back({GetResID(pipeline),GetResID(heap),GetResID(output),slot});
    return 0;
  }
  if(!strcmp(key, "metal.rayQueryDispatch"))
  {
    if(!IsBackgroundCapturing(m_State) || m_DescriptorCoverage != 3 ||
       type != eRENDERDOC_UInt64 || width != 4 || m_RayQueryDispatches.size() >= 64) return 2;
    auto pipeline = GetResourceManager()->FindAnnotationObject(object);
    auto roots = GetResourceManager()->FindAnnotationObject((void *)(uintptr_t)value->vector.uint64[0]);
    auto header = GetResourceManager()->FindAnnotationObject((void *)(uintptr_t)value->vector.uint64[2]);
    auto output = GetResourceManager()->FindAnnotationObject((void *)(uintptr_t)value->vector.uint64[3]);
    const uint64_t offset = value->vector.uint64[1];
    if(!pipeline || pipeline->m_Type != eResComputePipelineState || !pipeline->m_Real ||
       HasRayIRPipeline(GetResID(pipeline)) || offset % 8 || !roots || !header || !output ||
       roots == header || roots == output || header == output) return 2;
    bool typedHeader=false;
    for(const auto &h:m_RayASHeaders) typedHeader |= h.second.buffer==GetResID(header);
    for(auto buffer : {roots, header, output})
      if(buffer->m_Type != eResBuffer || !buffer->m_Real || buffer->m_CapturedAliasable ||
         (Unwrap((WrappedMTLBuffer *)buffer)->heap() && (buffer!=header || !typedHeader)) ||
         Unwrap((WrappedMTLBuffer *)buffer)->storageMode() != MTL::StorageModeShared ||
         Unwrap((WrappedMTLBuffer *)buffer)->length() > 16 * 1024) return 2;
    if(offset > Unwrap((WrappedMTLBuffer *)roots)->length() ||
       16 > Unwrap((WrappedMTLBuffer *)roots)->length() - offset ||
       Unwrap((WrappedMTLBuffer *)header)->length() < 64 ||
       !Unwrap((WrappedMTLBuffer *)output)->length() || Unwrap((WrappedMTLBuffer *)output)->length() % 4) return 2;
    for(const auto &other : m_RayQueryDispatches)
      if(other.pipeline == GetResID(pipeline) && other.roots == GetResID(roots) && other.offset == offset) return 2;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLComputePipelineState_DeclareRayQueryDispatch);
    Serialise_DeclareRayQueryDispatch(ser, GetResID(pipeline), GetResID(roots), offset,
        GetResID(header), GetResID(output));
    GetRecord(pipeline)->AddParent(GetRecord(roots)); GetRecord(pipeline)->AddParent(GetRecord(header));
    GetRecord(pipeline)->AddParent(GetRecord(output)); GetRecord(pipeline)->AddChunk(scope.Get());
    m_RayQueryDispatches.push_back({GetResID(pipeline), GetResID(roots), GetResID(header), GetResID(output), offset});
    return 0;
  }
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
       kind > 3 || !source || !source->m_Real ||
       source->m_Type != (kind == 0 || kind == 3 ? eResBuffer : kind == 1 ? eResTexture : eResSamplerState))
      return 2;
    MTL::Buffer *buffer = Unwrap((WrappedMTLBuffer *)wrapped);
    if(offset > buffer->length() || 24 > buffer->length() - offset ||
       (kind == 0 || kind == 3 ? memberOffset >= Unwrap((WrappedMTLBuffer *)source)->length() : memberOffset != 0) ||
       (kind == 3 && (memberOffset % 8 || 64 > Unwrap((WrappedMTLBuffer *)source)->length() - memberOffset)))
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
  if(schema > 6 || (schema >= 4 && (m_DescriptorCoverage != 3 ||
      (m_RayIRDispatches.empty() && (schema != 4 || m_RayQueryDispatches.empty())))) ||
     !ValidDescriptorLayout(offset, count, stride, (uint32_t)schema) ||
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
  // Preserve the distinction between an explicit null unbind and a missing
  // nonzero captured object. Pointer deserialisation otherwise loses this fact.
  // Unlike Vulkan's unused descriptor updates, these Metal binds retain every
  // nonnull target as a capture parent. Validate recorded births before loading
  // initial state can submit any GPU work. Build recipes, pipeline/stage, alias
  // lifetime and submission checks remain in their existing replay paths.
  {
    ReadSerialiser objects(rdc->ReadSection(section), Ownership::Stream);
    objects.SetVersion(m_SectionVersion);
    std::map<ResourceId, MetalResourceType> rayObjects;
    MetalReplayPreflightBudget objectBudget;
    while(!objects.GetReader()->AtEnd() && !objects.IsErrored())
    {
      const MetalChunk chunk = objects.ReadChunk<MetalChunk>();
      MetalResourceType kind = eResUnknown;
      bool birth = false, owner = true, array = false;
      switch(chunk)
      {
        case MetalChunk::MTLDevice_newAccelerationStructureWithSize:
        case MetalChunk::MTLDevice_newAccelerationStructureWithDescriptor:
          owner = false;
          // fall through
        case MetalChunk::MTLHeap_newAccelerationStructure:
          kind = eResAccelerationStructure; birth = true; break;
        case MetalChunk::MTLComputePipelineState_newVisibleFunctionTableWithDescriptor:
        case MetalChunk::MTLRenderPipelineState_newVisibleFunctionTableWithDescriptor:
          kind = eResVisibleFunctionTable; birth = true; break;
        case MetalChunk::MTLComputePipelineState_newIntersectionFunctionTableWithDescriptor:
        case MetalChunk::MTLRenderPipelineState_newIntersectionFunctionTableWithDescriptor:
          kind = eResIntersectionFunctionTable; birth = true; break;
        case MetalChunk::MTLComputeCommandEncoder_setAccelerationStructure:
        case MetalChunk::MTLRenderCommandEncoder_setVertexAccelerationStructure:
        case MetalChunk::MTLRenderCommandEncoder_setFragmentAccelerationStructure:
        case MetalChunk::MTLRenderCommandEncoder_setTileAccelerationStructure:
        case MetalChunk::MTLArgumentEncoder_setAccelerationStructure:
          kind = eResAccelerationStructure; break;
        case MetalChunk::MTLComputeCommandEncoder_setVisibleFunctionTables:
        case MetalChunk::MTLRenderCommandEncoder_setVertexVisibleFunctionTables:
        case MetalChunk::MTLRenderCommandEncoder_setFragmentVisibleFunctionTables:
        case MetalChunk::MTLRenderCommandEncoder_setTileVisibleFunctionTables:
        case MetalChunk::MTLIntersectionFunctionTable_setVisibleFunctionTables:
          array = true;
          // fall through
        case MetalChunk::MTLComputeCommandEncoder_setVisibleFunctionTable:
        case MetalChunk::MTLRenderCommandEncoder_setVertexVisibleFunctionTable:
        case MetalChunk::MTLRenderCommandEncoder_setFragmentVisibleFunctionTable:
        case MetalChunk::MTLRenderCommandEncoder_setTileVisibleFunctionTable:
        case MetalChunk::MTLArgumentEncoder_setVisibleFunctionTable:
        case MetalChunk::MTLIntersectionFunctionTable_setVisibleFunctionTable:
          kind = eResVisibleFunctionTable; break;
        case MetalChunk::MTLComputeCommandEncoder_setIntersectionFunctionTables:
        case MetalChunk::MTLRenderCommandEncoder_setVertexIntersectionFunctionTables:
        case MetalChunk::MTLRenderCommandEncoder_setFragmentIntersectionFunctionTables:
        case MetalChunk::MTLRenderCommandEncoder_setTileIntersectionFunctionTables:
          array = true;
          // fall through
        case MetalChunk::MTLComputeCommandEncoder_setIntersectionFunctionTable:
        case MetalChunk::MTLRenderCommandEncoder_setVertexIntersectionFunctionTable:
        case MetalChunk::MTLRenderCommandEncoder_setFragmentIntersectionFunctionTable:
        case MetalChunk::MTLRenderCommandEncoder_setTileIntersectionFunctionTable:
        case MetalChunk::MTLArgumentEncoder_setIntersectionFunctionTable:
          kind = eResIntersectionFunctionTable; break;
        default: break;
      }
      if(kind != eResUnknown)
      {
        if(!objectBudget.Consume(64))
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Metal ray object metadata budget exhausted");
        ResourceId parent, object;
        if(owner) objects.Serialise("owner"_lit, parent);
        if(birth)
        {
          objects.Serialise("object"_lit, object);
          if(object == ResourceId() ||
             (rayObjects.count(object) && rayObjects[object] != kind))
            RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal ray object birth identity/type");
          rayObjects[object] = kind;
        }
        else
        {
          rdcarray<ResourceId> targets;
          if(array) objects.Serialise("tables"_lit, targets);
          else { objects.Serialise("object"_lit, object); targets.push_back(object); }
          for(ResourceId target : targets)
          {
            const auto found = rayObjects.find(target);
            if(target != ResourceId() && (found == rayObjects.end() || found->second != kind))
              RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
                  "Missing or wrong-type Metal ray binding object %s in %s",
                  ToStr(target).c_str(), GetChunkName((uint32_t)chunk).c_str());
          }
        }
      }
      objects.EndChunk();
    }
    if(objects.IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, objects.GetError().message);
  }
  // Only ReadLogInitialisation's mandatory-exit CPU diagnostic path can supply
  // this candidate version. It does not authorize loading or GPU replay.
  if(diagnosticCoverage) m_DescriptorCoverage=diagnosticCoverage;
  m_RayIRDispatches.clear();
  m_RayQueryDispatches.clear();
  m_RayQueryHeapDispatches.clear();
  m_RayQueryHeapCBVRoots.clear();
  m_IRComputeRoots.clear();
  m_IRComputeHeapEntries.clear();
  m_IRComputeReflections.clear();
  m_IRComputeRuntimeABIs.clear();
  m_IRComputeReflectionBytes = 0;
  m_IRComputeFrameCBVReadRanges.clear();
  m_IRComputeUniformReadContents.clear();
  m_IRRuntimeDescriptorAccesses.clear();
  m_IRRuntimeDescriptorAccessComplete.clear();
  m_IRRuntimeRestoredBindings.clear();
  m_IRComputeFrameCBVResources.clear();
  m_DescriptorTextureUsages.clear();
  m_RayASHeaders.clear();
  m_RayASHeaderCurrent.clear();
  m_RayASHeaderFrameWrites.clear();
  m_RayASHeaderFrameCursor=0;
  m_RayIRShaderRoles.clear();
  m_RayIRLocalRoots.clear();
  m_RayIRGlobalRoots.clear();
  m_RayIRHeapEntries.clear();
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
    if(frame && chunk == MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups)
    {
      ResourceId encoder; MTL::Size groups={},threads={};
      scan.Serialise("ComputeCommandEncoder"_lit,encoder);
      scan.Serialise("groups"_lit,groups); scan.Serialise("threadsPerGroup"_lit,threads);
      const uint64_t groupAxes[] = {groups.width, groups.height, groups.depth};
      const uint64_t threadAxes[] = {threads.width, threads.height, threads.depth};
      if(!MetalComputeDispatchExtentFits(groupAxes, threadAxes))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid or overflowing Metal compute dispatch extent");
    }
    if(chunk == MetalChunk::MTLComputePipelineState_CaptureIRComputeReflection)
    {
      ResourceId pipeline; rdcstr reflection;
      scan.Serialise("pipeline"_lit, pipeline); scan.Serialise("reflection"_lit, reflection);
      if(frame || pipeline == ResourceId() || reflection.empty() || reflection.size() > 64 * 1024 ||
         m_IRComputeReflections.size() >= 4096 ||
         m_IRComputeReflectionBytes > 16 * 1024 * 1024 - reflection.size() ||
         !m_IRComputeReflections.insert({pipeline, reflection}).second)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid immutable Metal IR compute reflection");
      m_IRComputeReflectionBytes += reflection.size();
      MetalIRComputeRuntimeABI abi;
      const auto parsed = ParseMetalIRComputeRuntimeABI(reflection.c_str(), reflection.size(), abi);
      if(parsed == MetalIRComputeABIResult::Invalid)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal IR compute binding metadata");
      if(parsed == MetalIRComputeABIResult::RuntimeBindings)
        m_IRComputeRuntimeABIs[pipeline] = abi;
    }
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
    // RDC sections can be forward-only compressed/file streams, as in VK/DX12.
    // Evidence consumes only its own encoder/draw/dispatch chunks; the other
    // metadata cases below are disjoint, and EndChunk skips the unread tail.
    if(frame && m_HasCapturedRenderIndirectArguments)
    {
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
    }
    if(frame && m_HasCapturedComputeIndirectArguments)
    {
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
         (chunk==MetalChunk::MTLDevice_newBufferWithBytes?initial.size()!=length:!initial.empty()) ||
         (options != MTL::ResourceStorageModeShared &&
          options != (MTL::ResourceStorageModeShared | MTL::ResourceHazardTrackingModeTracked) &&
          !(m_DescriptorCoverage>=65 &&
            (options==MTL::ResourceCPUCacheModeWriteCombined ||
             options==(MTL::ResourceCPUCacheModeWriteCombined|MTL::ResourceHazardTrackingModeTracked))) &&
          !(m_DescriptorCoverage>=65 && initial.empty() &&
            (options==MTL::ResourceStorageModePrivate ||
             options==(MTL::ResourceStorageModePrivate|MTL::ResourceHazardTrackingModeTracked)))) ||
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
      if(heap == ResourceId() || texture == ResourceId() ||
         descriptor.pixelFormat == MTL::PixelFormatX32_Stencil8 ||
         !ValidDescriptorFrameTexture(descriptor, m_DescriptorCoverage) ||
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
       (chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset ||
        chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset_swizzle ||
        chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat))
    {
      ResourceId source, view; MTL::PixelFormat format; MTL::TextureType type;
      NS::Range levels(0, 0), slices(0, 0); MTL::TextureSwizzleChannels swizzle;
      scan.Serialise("Source"_lit, source); scan.Serialise("View"_lit, view);
      scan.Serialise("format"_lit, format); scan.Serialise("type"_lit, type);
      scan.Serialise("levels"_lit, levels); scan.Serialise("slices"_lit, slices);
      scan.Serialise("swizzle"_lit, swizzle);
      auto parent = m_DescriptorFrameTextures.find(source);
      if(chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat &&
         parent != m_DescriptorFrameTextures.end())
      {
        const auto &d = parent->second.descriptor;
        type = d.textureType;
        levels = NS::Range(0, d.mipmapLevelCount);
        slices = NS::Range(0, type == MTL::TextureTypeCube || type == MTL::TextureTypeCubeArray ?
            6 * d.arrayLength : type == MTL::TextureType2DArray ? d.arrayLength : 1);
      }
      RDMTL::TextureDescriptor projected;
      if(view == ResourceId() || source == view || parent == m_DescriptorFrameTextures.end() ||
         m_DescriptorFrameTextureViewParents.count(source) ||
         !ProjectDescriptorFrameTextureView(parent->second.descriptor, format, type, levels,
                                           slices, swizzle, m_DescriptorCoverage, projected) ||
         !m_DescriptorFrameTextureViewParents.insert({view, source}).second ||
         !m_DescriptorFrameTextureViewMipLevels.insert({view, levels.location}).second ||
         !m_DescriptorFrameTextureViewSlices.insert({view, slices.location}).second ||
         !m_DescriptorFrameTextures.insert({view, {parent->second.heap, projected, parent->second.offset}}).second)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,
            "Invalid frame sourced Metal texture view: view %s source %s parent mips %llu, level %llu count %llu, slice %llu count %llu",
            ToStr(view).c_str(), ToStr(source).c_str(),
            (unsigned long long)(parent == m_DescriptorFrameTextures.end() ? 0 : parent->second.descriptor.mipmapLevelCount),
            (unsigned long long)levels.location, (unsigned long long)levels.length,
            (unsigned long long)slices.location, (unsigned long long)slices.length);
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
    if(chunk==MetalChunk::MTLComputePipelineState_DeclareRayIRHeapEntry)
    {
      ResourceId pipeline;RayIRHeapEntry entry;
      scan.Serialise("pipeline"_lit,pipeline);scan.Serialise("heap"_lit,entry.heap);
      scan.Serialise("index"_lit,entry.index);scan.Serialise("kind"_lit,entry.kind);
      scan.Serialise("bytes"_lit,entry.bytes);
      if(frame || !AddRayIRHeapEntry(pipeline,entry,false))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid Metal IR heap entry declaration");
    }
    if(chunk == MetalChunk::MTLComputePipelineState_DeclareRayIRGlobalRoot)
    {
      ResourceId pipeline; RayIRLocalRoot root;
      scan.Serialise("pipeline"_lit,pipeline); scan.Serialise("offset"_lit,root.offset);
      scan.Serialise("kind"_lit,root.kind); scan.Serialise("count"_lit,root.count);
      scan.Serialise("bytes"_lit,root.bytes);
      if(frame || !AddRayIRGlobalRoot(pipeline,root,false))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid Metal IR global root declaration");
    }
    if(chunk == MetalChunk::MTLFunctionHandle_DeclareRayIRLocalRoot)
    {
      ResourceId function; RayIRLocalRoot root;
      scan.Serialise("function"_lit,function); scan.Serialise("offset"_lit,root.offset);
      scan.Serialise("kind"_lit,root.kind); scan.Serialise("count"_lit,root.count);
      scan.Serialise("bytes"_lit,root.bytes);
      if(frame || !AddRayIRLocalRoot(function,root,false))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal IR local root declaration");
    }
    if(chunk == MetalChunk::MTLFunctionHandle_DeclareRayIRShaderRole)
    {
      ResourceId function; uint32_t role = 0;
      scan.Serialise("function"_lit, function); scan.Serialise("role"_lit, role);
      if(frame || function == ResourceId() || role > 3 || m_RayIRShaderRoles.size() >= 256 ||
         !m_RayIRShaderRoles.insert({function, role}).second)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal IR shader role declaration");
    }
    if(chunk == MetalChunk::MTLComputePipelineState_DeclareIRComputeRoot)
    {
      ResourceId pipeline; RayIRLocalRoot root;
      scan.Serialise("pipeline"_lit,pipeline); scan.Serialise("offset"_lit,root.offset);
      scan.Serialise("kind"_lit,root.kind); scan.Serialise("count"_lit,root.count);
      scan.Serialise("bytes"_lit,root.bytes);
      if(frame || m_DescriptorCoverage!=65 || !AddIRComputeRoot(pipeline,root,false))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid Metal IR compute root declaration");
    }
    if(chunk == MetalChunk::MTLComputePipelineState_DeclareIRComputeHeapEntry)
    {
      ResourceId pipeline; RayIRHeapEntry entry;
      scan.Serialise("pipeline"_lit,pipeline); scan.Serialise("heap"_lit,entry.heap);
      scan.Serialise("index"_lit,entry.index); scan.Serialise("kind"_lit,entry.kind);
      scan.Serialise("bytes"_lit,entry.bytes);
      if(frame || m_DescriptorCoverage!=65 || !AddIRComputeHeapEntry(pipeline,entry,false))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid Metal IR compute heap entry declaration");
    }
    if(chunk == MetalChunk::MTLComputePipelineState_DeclareRayQueryHeapCBVRoot)
    {
      ResourceId pipeline; RayQueryHeapCBVRoot root={};
      scan.Serialise("pipeline"_lit,pipeline); scan.Serialise("offset"_lit,root.offset);
      scan.Serialise("bytes"_lit,root.bytes); scan.Serialise("outputSlot"_lit,root.outputSlot);
      scan.Serialise("rootCount"_lit,root.rootCount);
      if(frame || m_DescriptorCoverage!=65 || !AddRayQueryHeapCBVRoot(pipeline,root,false))
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid typed Metal heap-query CBV root declaration");
    }
    if(chunk == MetalChunk::MTLComputePipelineState_DeclareRayQueryHeapDispatch)
    {
      RayQueryHeapDispatch d;
      scan.Serialise("pipeline"_lit,d.pipeline); scan.Serialise("heap"_lit,d.heap);
      scan.Serialise("slotOffset"_lit,d.slotOffset); scan.Serialise("output"_lit,d.output);
      if(frame || m_DescriptorCoverage!=65 || d.pipeline==ResourceId() ||
         d.heap==ResourceId() || d.output==ResourceId() || d.heap==d.output ||
         d.slotOffset%24 || m_RayQueryHeapDispatches.size()>=64)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid typed Metal heap-query declaration");
      for(const auto &old:m_RayQueryHeapDispatches) if(old.pipeline==d.pipeline)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Duplicate typed Metal heap-query pipeline");
      m_RayQueryHeapDispatches.push_back(d);
    }
    if(chunk == MetalChunk::MTLBuffer_DeclareRayASHeader)
    {
      RayASHeader h;
      scan.Serialise("buffer"_lit,h.buffer); scan.Serialise("offset"_lit,h.offset);
      scan.Serialise("structure"_lit,h.structure); scan.Serialise("contributions"_lit,h.contributions);
      scan.Serialise("contributionOffset"_lit,h.contributionOffset); scan.Serialise("bytes"_lit,h.bytes);
      if(h.buffer == ResourceId() || h.structure == ResourceId() ||
         h.contributions == ResourceId() || h.buffer == h.contributions || h.offset % 8 ||
         h.contributionOffset % 4 || h.bytes.size() != 64 || h.offset > UINT64_MAX-64)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid Metal typed AS header declaration");
      if(frame)
      {
        const bool typedHeaders=m_DescriptorCoverage==65;
        const auto born=m_DescriptorFrameBuffers.find(h.buffer);
        if((m_DescriptorCoverage!=3 && !typedHeaders) || m_RayASHeaderFrameWrites.size()>=128 ||
           (!m_RayASHeaders.count(make_rdcpair(h.buffer,h.offset)) &&
            (!typedHeaders || born==m_DescriptorFrameBuffers.end() || born->second.length!=64 || h.offset)))
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Dynamic Metal AS header requires typed coverage and a known initial or bounded frame-born header");
        m_RayASHeaderFrameWrites.push_back(h);
      }
      else if(m_RayASHeaders.size()>=128 ||
              !m_RayASHeaders.insert({make_rdcpair(h.buffer,h.offset),h}).second)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Duplicate or excessive initial Metal typed AS header declaration");
    }
    if(chunk == MetalChunk::MTLComputePipelineState_DeclareRayQueryDispatch)
    {
      RayQueryDispatch d;
      scan.Serialise("pipeline"_lit,d.pipeline); scan.Serialise("roots"_lit,d.roots);
      scan.Serialise("offset"_lit,d.offset); scan.Serialise("header"_lit,d.header); scan.Serialise("output"_lit,d.output);
      if(frame || d.pipeline == ResourceId() || d.roots == ResourceId() || d.header == ResourceId() ||
         d.output == ResourceId() || d.roots == d.header || d.roots == d.output || d.header == d.output ||
         d.offset % 8 || m_RayQueryDispatches.size() >= 64)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid Metal query dispatch declaration");
      for(const auto &other : m_RayQueryDispatches)
        if(other.pipeline == d.pipeline && other.roots == d.roots && other.offset == d.offset)
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Duplicate Metal query dispatch declaration");
      m_RayQueryDispatches.push_back(d);
    }
    if(chunk == MetalChunk::MTLComputePipelineState_DeclareRayIRDispatch)
    {
      RayIRDispatch d;
      scan.Serialise("pipeline"_lit, d.pipeline); scan.Serialise("buffer"_lit, d.buffer);
      scan.Serialise("offset"_lit, d.offset); scan.Serialise("rootCount"_lit, d.rootCount);
      if(frame || d.pipeline == ResourceId() || d.buffer == ResourceId() ||
         d.offset % 8 || d.rootCount < 2 || d.rootCount > 256 || m_RayIRDispatches.size() >= 64)
        RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Invalid Metal IR ray dispatch declaration");
      for(const auto &other : m_RayIRDispatches)
        if(other.pipeline == d.pipeline && other.buffer == d.buffer && other.offset == d.offset)
          RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Duplicate Metal IR ray dispatch declaration");
      m_RayIRDispatches.push_back(d);
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
    std::map<ResourceId, uint64_t> largeBackgroundBuffers, bufferInitialSizes, bufferCreationSizes;
    std::map<ResourceId, uint64_t> budgetBackgroundBuffers;
    std::map<ResourceId, uint64_t> sharedBackgroundBuffers;
    std::map<ResourceId, uint64_t> colorInitialTextures, textureInitialSizes;
    std::map<ResourceId, bool> initialDataRequirements;
    ReadSerialiser budget(rdc->ReadSection(section), Ownership::Stream);
    budget.SetVersion(m_SectionVersion);
    frame = false;
    while(!budget.GetReader()->AtEnd() && !budget.IsErrored())
    {
      const bool previouslyInvalid = sourcedBudgetInvalid;
      const uint64_t budgetOffset = budget.GetReader()->GetOffset();
      MetalChunk chunk = budget.ReadChunk<MetalChunk>();
      if((SystemChunk)chunk == SystemChunk::CaptureScope) frame = true;
      if((SystemChunk)chunk == SystemChunk::InitialContentsList)
      {
        rdcarray<ResourceManagerInternal::WrittenRecord> needed;
        budget.Serialise("NeededInitials"_lit, needed);
        // Resource creation records are hoisted before CaptureScope even for
        // objects created during capture. The common initial-content contract,
        // not their position in the file, says whether captured data exists.
        // Dirty references and prepared initial data can produce duplicate IDs;
        // any data-bearing entry requires that data to remain present.
        for(const auto &entry : needed)
          initialDataRequirements[entry.id] |= entry.written;
      }
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
        MTL::StorageMode storageMode; MTL::CPUCacheMode cacheMode;
        MTL::HazardTrackingMode hazardMode; uint32_t type = 0;
        budget.Serialise("storageMode"_lit, storageMode); budget.Serialise("cacheMode"_lit, cacheMode);
        budget.Serialise("hazardMode"_lit, hazardMode); budget.Serialise("type"_lit, type);
        // Match the already supported native heap range for the v65 contract.
        // Charge the entire backing, including unused placement space, before
        // allocating anything. Child extents/aliases are checked separately.
        const uint64_t heapLimit = m_DescriptorCoverage >= 65 ? 576ULL * 1024 * 1024 :
            m_DescriptorCoverage >= 60 ? 128ULL * 1024 * 1024 :
            m_DescriptorCoverage >= 43 ? 16ULL * 1024 * 1024 : 1024 * 1024;
        sourcedBudgetInvalid |= frame || !size || size > heapLimit;
        sourcedAllocationBytes += RDCMIN(size, heapLimit + 1);
        if(m_DescriptorCoverage >= 60)
        {
          // Metal can place small textures in sub-page heaps. Account for the
          // rounded backing below; child native size/alignment checks, rather
          // than a host page-size minimum, determine whether placements fit.
          sourcedBudgetInvalid |= heap == ResourceId() || !size || !budgetHeaps.insert(heap).second;
          sourcedBudgetInvalid |= (storageMode != MTL::StorageModePrivate &&
              !(storageMode == MTL::StorageModeShared && type == uint32_t(MTL::HeapTypePlacement))) ||
              cacheMode != MTL::CPUCacheModeDefaultCache || hazardMode > MTL::HazardTrackingModeTracked ||
              (type != uint32_t(MTL::HeapTypeAutomatic) && type != uint32_t(MTL::HeapTypePlacement));
          nativeBudget.Add(nativeBudget.nativeBytes, AlignUp(RDCMIN(size, heapLimit + 1), uint64_t(64*1024)));
        }
        if(!previouslyInvalid && sourcedBudgetInvalid && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal heap metadata rejection: heap=%s bytes=%llu limit=%llu storage=%u cache=%u hazard=%u type=%u frame=%d\n",
              ToStr(heap).c_str(),(unsigned long long)size,(unsigned long long)heapLimit,
              uint32_t(storageMode),uint32_t(cacheMode),uint32_t(hazardMode),type,int(frame));
      }
      if(chunk == MetalChunk::MTLDevice_newBufferWithLength ||
         chunk == MetalChunk::MTLDevice_newBufferWithBytes ||
         chunk == MetalChunk::MTLHeap_newBuffer ||
         chunk == MetalChunk::MTLHeap_newBufferWithOffset)
      {
        ResourceId heap, buffer; bytebuf creationData; uint64_t length = 0;
        if(chunk == MetalChunk::MTLHeap_newBuffer || chunk == MetalChunk::MTLHeap_newBufferWithOffset)
          budget.Serialise("Heap"_lit, heap);
        budget.Serialise("Buffer"_lit, buffer);
        if(chunk == MetalChunk::MTLDevice_newBufferWithBytes || chunk == MetalChunk::MTLDevice_newBufferWithLength)
          budget.Serialise("initialData"_lit, creationData);
        budget.Serialise("length"_lit, length);
        if(!creationData.empty())
        {
          sourcedBudgetInvalid |= creationData.size()!=length;
          if(!frame)
          {
            sourcedBudgetInvalid |= !bufferCreationSizes.insert({buffer,creationData.size()}).second;
            nativeBudget.Add(nativeBudget.initialBytes,creationData.size());
          }
        }
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
        sourcedBudgetInvalid |= !m_DescriptorTextureUsages.insert({texture,uint64_t(descriptor.usage)}).second;
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
            descriptor.pixelFormat != MTL::PixelFormatX32_Stencil8 &&
            (chunk == MetalChunk::MTLDevice_newTextureWithDescriptor ||
             (m_DescriptorCoverage >= 37 && chunk == MetalChunk::MTLDevice_newTextureWithDescriptor_nextDrawable)) &&
            MetalTextureReplayLayout(descriptor, colorSize);
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
      if(chunk == MetalChunk::MTLHeap_newTexture ||
         chunk == MetalChunk::MTLHeap_newTextureWithOffset ||
         chunk == MetalChunk::MTLBuffer_newTextureWithDescriptor)
      {
        ResourceId parent, texture; RDMTL::TextureDescriptor descriptor;
        if(chunk == MetalChunk::MTLBuffer_newTextureWithDescriptor)
          budget.Serialise("Buffer"_lit, parent);
        else
          budget.Serialise("Heap"_lit, parent);
        budget.Serialise("Texture"_lit, texture); budget.Serialise("descriptor"_lit, descriptor);
        sourcedBudgetInvalid |= texture == ResourceId() ||
            !m_DescriptorTextureUsages.insert({texture,uint64_t(descriptor.usage)}).second;
      }
      if(chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat ||
         chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset ||
         chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset_swizzle)
      {
        ResourceId source, view;
        budget.Serialise("Source"_lit, source); budget.Serialise("View"_lit, view);
        const auto parent = m_DescriptorTextureUsages.find(source);
        sourcedBudgetInvalid |= parent == m_DescriptorTextureUsages.end() || view == ResourceId();
        if(parent != m_DescriptorTextureUsages.end())
          sourcedBudgetInvalid |= !m_DescriptorTextureUsages.insert({view,parent->second}).second;
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
      if(chunk==MetalChunk::MTLBuffer_CaptureHeapBirthContents ||
         chunk==MetalChunk::MTLBuffer_CaptureHeapBirthUnspecified)
      {
        ResourceId buffer,heap;uint64_t offset=0;bytebuf data;
        budget.Serialise("buffer"_lit,buffer);budget.Serialise("heap"_lit,heap);
        budget.Serialise("offset"_lit,offset);budget.Serialise("data"_lit,data);
        const bool apiUnspecified=chunk==MetalChunk::MTLBuffer_CaptureHeapBirthUnspecified;
        sourcedBudgetInvalid |= !frame || (apiUnspecified?!data.empty():data.empty()) || data.size()>16ULL*1024*1024;
        nativeBudget.Add(nativeBudget.snapshotBytes,data.size());
      }
      if(!previouslyInvalid && sourcedBudgetInvalid && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal allocation metadata rejection: offset=%llu chunk=%s frame=%d\n",
            (unsigned long long)budgetOffset,GetChunkName((uint32_t)chunk).c_str(),int(frame));
      budget.EndChunk();
    }
    for(const auto &buffer : largeBackgroundBuffers)
    {
      // Creation bytes are authoritative when no frame-initial snapshot exists.
      // A later InitialContents record still takes precedence; never infer data
      // from a future submission or lower the allocation/initial-data budget.
      if(!bufferInitialSizes.count(buffer.first) && bufferCreationSizes.count(buffer.first) &&
         bufferCreationSizes.at(buffer.first)==buffer.second) continue;
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
    {
      const auto requirement = initialDataRequirements.find(texture.first);
      // A frame-created standalone image (e.g. a native transfer destination)
      // has no frame-start snapshot. Recreate its original object and execute
      // its producer/copy and synchronization; do not invent initial texels.
      // Missing data-bearing snapshots, short data, and absent contracts retain
      // the existing failure. Any supplied snapshot must still match its layout.
      if(!textureInitialSizes.count(texture.first) &&
         requirement != initialDataRequirements.end() && !requirement->second)
        continue;
      const bool invalid = !textureInitialSizes.count(texture.first) ||
          textureInitialSizes[texture.first] != texture.second;
      if(invalid && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal required texture initial rejection: resource=%s expected=%llu initial=%llu\n",
            ToStr(texture.first).c_str(), (unsigned long long)texture.second,
            (unsigned long long)(textureInitialSizes.count(texture.first) ? textureInitialSizes.at(texture.first) : 0));
      sourcedBudgetInvalid |= invalid;
    }
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
  for(const auto &dispatch : m_RayIRDispatches)
    if(dispatch.rootCount != 2 && !m_RayIRGlobalRoots.count(dispatch.pipeline))
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Invalid Metal IR ray dispatch declaration: missing global roots");
  if(!m_RayIRShaderRoles.empty() && (m_RayIRDispatches.empty() || m_DescriptorCoverage != 3))
    RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Metal IR shader roles require static typed dispatch coverage3");
  if(!m_RayIRDispatches.empty() && m_DescriptorCoverage != 3)
    RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Metal IR ray dispatch requires static typed coverage3");
  m_RayASHeaderCurrent=m_RayASHeaders;
  for(const auto &h : m_RayASHeaders)
  {
    bool queryHeader=false;
    for(const auto &q : m_RayQueryDispatches) queryHeader |= q.header == h.second.buffer;
    if((m_DescriptorCoverage != 3 || !queryHeader) &&
       m_DescriptorCoverage!=65)
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Typed Metal AS header requires typed descriptor coverage or a validated static query");
  }
  for(const auto &query : m_RayQueryDispatches)
    if(m_DescriptorCoverage != 3 || HasRayIRPipeline(query.pipeline))
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed,"Metal query dispatch requires separate static typed coverage3");
  for(const auto &layout : m_DescriptorTables)
    if(layout.schema >= 4 && (m_DescriptorCoverage != 3 ||
        (m_RayIRDispatches.empty() && (layout.schema != 4 || m_RayQueryDispatches.empty()))))
      RETURN_ERROR_RESULT(ResultCode::APIReplayFailed, "Metal ray resource layout requires an IR dispatch declaration");
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
        if(offset > raw.size() || 8 > raw.size() - offset) return false;
        memcpy(&captured, raw.data() + offset, 8);
        if(layout.schema >= 4)
        {
          ResourceId resource;
          if(!ResolveRayIRIdentity(layout.schema, captured, resource, replacement)) return false;
          memcpy(patched.data() + offset, &replacement, 8);
          continue;
        }
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
  if(!PatchRayASHeaders(bufferId, raw, patched)) return false;
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
        if(layout.schema >= 4)
        {
          ResourceId resource; uint64_t replacement = 0;
          if(!ResolveRayIRIdentity(layout.schema, captured, resource, replacement)) return false;
          continue;
        }
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
  bytebuf typed = raw;
  return PatchRayASHeaders(bufferId, raw, typed);
}

bool WrappedMTLDevice::PrepareDescriptorTables()
{
  if(m_DescriptorCoverage >= 4)
    return PrepareRayASHeaderInitialContents() && PrepareDescriptorSlotShadow();
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
  // A complete captured creation payload is also an authoritative initial
  // state. Root addresses are relocated before the consumer validates its
  // declared read range, so do not depend on that later validation populating
  // m_ReplayBufferInitialContents first.
  const bool creationInitial = m_ReplayBuffersWithCreationContents.count(source.resource) &&
      buffer->storageMode() == MTL::StorageModeShared && buffer->contents();
  const bool generalInitial = m_DescriptorCoverage >= 34 && (complete || creationInitial);
  const bool frameSource = m_DescriptorCoverage >= 40 && future != m_DescriptorFrameBuffers.end();
  const bool frameCBVSource=m_DescriptorCoverage>=65 && future!=m_DescriptorFrameBuffers.end() &&
      m_IRComputeFrameCBVReadRanges.count(make_rdcpair(source.resource,source.offset));
  // Address relocation depends on the actual frame-born allocation, not on
  // whether a later shader happens to classify it as a CBV. The validated
  // factory/lifetime and the consumer's independent restoration/producer
  // checks cover ordinary buffers with partial uploads as well.
  const bool framePrivateSource=frameSource && future->second.heap==ResourceId() &&
      !buffer->heap() && buffer->length()==future->second.length &&
      buffer->storageMode()==MTL::StorageModePrivate &&
      buffer->hazardTrackingMode()==MTL::HazardTrackingModeTracked;
  if((m_DescriptorCoverage >= 13 && IsReplayResourceAliasable(source.resource)) ||
     (buffer->storageMode() != MTL::StorageModeShared &&
      !(m_DescriptorCoverage >= 23 && buffer->storageMode() == MTL::StorageModePrivate &&
        (((generalInitial || frameCBVSource || framePrivateSource) && !buffer->heap()) ||
         (buffer->heap() && buffer->hazardTrackingMode() == MTL::HazardTrackingModeTracked)) &&
        (frameCBVSource || framePrivateSource || IsFramePlacementResource(source.resource) ||
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

static bool DescriptorIRFields(uint32_t type, std::map<uint32_t, uint64_t> &fields)
{
  // Buffer SRV/UAV, typed buffer SRV/UAV, texture SRV/UAV, CBV, sampler.
  // These are capture packet tags, distinct from Native MTLArgumentType.
  if(type == 0 || type == 1 || type == 6) fields[0] = 0;
  else if(type == 2 || type == 3) { fields[0] = 0; fields[1] = 8; }
  // A view can contain both a buffer pointer and a texture handle. The sourced
  // factory fields determine the layout; a logical view tag alone does not.
  else if(type == 4 || type == 5) { fields[0] = 0; fields[1] = 8; }
  else if(type == 7) fields[2] = 0;
  else return false;
  return true;
}

bool WrappedMTLDevice::PatchDescriptorSlotField(const DescriptorSource &source, uint32_t kind,
    uint64_t captured, uint64_t &replacement)
{
  if(kind==3)
  {
    auto h=m_RayASHeaderCurrent.find(make_rdcpair(source.resource,source.offset));
    if(m_DescriptorCoverage!=65 ||
       h==m_RayASHeaderCurrent.end() || !ValidateRayASHeader(h->second)) return false;
    return PatchDescriptorSource(source,captured,replacement);
  }
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
    if(!DescriptorIRFields(slot.type, fields)) return false;
    if(slot.sources.count(3))
    {
      if(m_DescriptorCoverage!=65 || slot.type!=4 || slot.sources.size()!=1) return false;
      fields.clear(); fields[3]=0;
    }
    uint64_t first = 0, second = 0;
    memcpy(&first, slot.data.data(), 8); memcpy(&second, slot.data.data() + 8, 8);
    if(slot.sources.count(3) && second) return false;
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
    old->second.gpuCopyPublished = m_DescriptorShadowFrame;
    old->second.initialRestored = false;
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
  old->second.gpuCopyPublished = false;
  old->second.initialRestored = false;
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
    if(!DescriptorIRFields(found->second.type, fields)) return false;
    if(kind==3)
    {
      if(m_DescriptorCoverage!=65 || found->second.type!=4 || !found->second.sources.empty()) return false;
      fields.clear(); fields[3]=0;
    }
    if(!fields.count(kind) || (kind!=3 && found->second.sources.count(3))) return false;
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
  if(m_DescriptorCoverage < 5 || size != 24 || !m_DescriptorGPUWrittenBuffers.count(destination) ||
     sourceOffset>UINT64_MAX-size || destinationOffset>UINT64_MAX-size ||
     (source==destination && sourceOffset<destinationOffset+size && destinationOffset<sourceOffset+size))
    return false;
  const auto from = m_DescriptorSlotShadow.find(make_rdcpair(source, sourceOffset));
  const auto to = m_DescriptorSlotShadow.find(make_rdcpair(destination, destinationOffset));
  const auto key = make_rdcpair(destination, destinationOffset);
  if(from == m_DescriptorSlotShadow.end() || to == m_DescriptorSlotShadow.end() ||
     !from->second.live || !to->second.live ||
     (m_DescriptorGPUWrittenBuffers.count(source) && !from->second.initialRestored) ||
     (from->second.gpuExpected && !from->second.initialRestored) ||
     from->second.type != to->second.type || m_DescriptorGPUCopyExpected.count(key))
    return false;
  bytebuf patched;
  if(!PatchDescriptorSlot(from->second, patched))return false;
  if(m_DescriptorCoverage>=7 && from->second.sources.empty())
  {
    // Null API descriptors have no referenced objects. Validate every address
    // field, rather than confusing an empty ref map with a missing producer.
    std::map<uint32_t,uint64_t> fields;
    if(!DescriptorIRFields(from->second.type,fields))return false;
    for(const auto &field:fields)
    {uint64_t value=0;memcpy(&value,from->second.data.data()+field.second,8);if(value)return false;}
  }
  else if(m_DescriptorCoverage<7 && from->second.source.resource==ResourceId())return false;
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
      if(entry.second.gpuExpected && !entry.second.initialRestored && !initialRestore) continue;
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
        if(!DescriptorIRFields(entry.second.type, fields)) return false;
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
  // A table may be empty at frame start and receive its first typed publication
  // after a frame resource is born. Validate its actual initial bytes below;
  // consumption still requires the normal slot/source/publication preflight.
  // The presence of an unrelated background slot is not a restoration contract.
  if(m_DescriptorCoverage < 5 && !m_DescriptorGPUWrittenBuffers.empty()) return false;
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
  // Initial descriptor identities are reconstructed from captured bytes and
  // typed sources, independently of how the application produced those bytes
  // before capture (CPU or GPU). Unused retired sources were not validated.
  for(auto &entry:m_DescriptorSlotShadow)
    entry.second.initialRestored=entry.second.live &&
        !IsDescriptorPreludeRetirement(entry.first,entry.second);
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
  // setBytes also carries ordinary scalar/vector data. Preserve its exact API
  // payload, rather than inventing an address layout. Every actual typed-table
  // consumer below still validates pointer/handle origins independently.
  if(layout == m_DescriptorInlineShadow.end() && m_DescriptorCoverage >= 65 &&
     stage <= 2 && index < 31 && !data.empty() && data.size() <= 4096)
    return true;
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
  m_RayIRASCurrentContents=m_ReplayASInitialContents;
  struct HeaderScope
  {
    WrappedMTLDevice &device;
    decltype(m_DescriptorRawContents) raw;
    HeaderScope(WrappedMTLDevice &d):device(d),raw(d.m_DescriptorRawContents)
    { d.m_RayASHeaderCurrent=d.m_RayASHeaders; d.m_RayASHeaderFrameCursor=0; }
    ~HeaderScope()
    { device.m_DescriptorRawContents=raw; device.m_RayASHeaderCurrent=device.m_RayASHeaders;
      device.m_RayASHeaderFrameCursor=0; }
  } headers(*this);
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
  m_ReplayHeapBufferBirthContents.clear();
  m_ReplayHeapBufferBirthUnspecified.clear();
  m_DescriptorPreflightLiveTextures.clear();
  m_DescriptorPreflightLiveViews.clear();
  m_DescriptorPreflightLiveTables.clear();
  m_DescriptorPreflightAliasedBuffers.clear();
  m_DescriptorInlineShadow.clear();
  m_DescriptorGPUCopyExpected.clear();
  m_DescriptorDispatches.clear();
  struct ComputeSnapshot
  {
    ResourceId pipeline;
    std::map<uint32_t, MetalPipe::BufferBinding> buffers;
    std::map<uint32_t, uint64_t> bytes;
    std::map<ResourceId,uint64_t> residency;
    std::map<uint32_t, rdcarray<byte>> inlineData;
    std::map<uint32_t,std::map<uint64_t,DescriptorSource>> pointers;
  };
  std::map<ResourceId, ComputeSnapshot> computes;
  std::map<rdcpair<ResourceId, uint32_t>, ComputeSnapshot> graphics;
  std::set<ResourceId> runtimeConsumerEncoders;
  std::set<ResourceId> graphicsConsumerEncoders;
  std::set<ResourceId> liveRenders;
  bool success = true, consumed = false;
  std::set<ResourceId> liveBlits;
  std::set<ResourceId> liveAS;
  std::map<ResourceId, ResourceId> rayASBuildCommands;
  std::map<ResourceId, ResourceId> typedTextureWriteCommands;
  struct RuntimeTextureView
  {
    RDMTL::TextureDescriptor descriptor;
    uint64_t offset=0,bytesPerRow=0;
  };
  // Preflight follows frame births without creating Native objects. Only a
  // validated buffer-view factory may publish the view's metadata here.
  std::map<ResourceId,RuntimeTextureView> runtimeTextureViews;
  std::map<ResourceId, std::set<ResourceId>> typedQueryWriteResidency;
  std::map<ResourceId,uint32_t> asDebugDepth;
  uint32_t rayBuildCount=0;
  std::map<ResourceId,uint32_t> blitDebugDepth;
  uint32_t commandCount = 0, dispatchCount = 0, copyCount = 0, drawCount = 0;
  uint32_t producerCount = 0, meshDrawCount = 0;
  uint64_t drawWork = 0;
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
  MetalReplayPreflightBudget preflightBudget;
  // Immutable, dispatch-owned facts about one physical address field. These
  // are captured initial bytes/current publications, not shader expressions
  // or logical allocation permissions. Frame retirement changes no Native bytes;
  // initial restoration separately invalidates captured stale identity fields.
  using NullFieldProofs=std::map<DescriptorSlotKey,std::shared_ptr<bytebuf>>;
  NullFieldProofs nullBufferFields;
  std::map<DescriptorSlotKey,ResourceId> nullGPUOwners, pendingNullGPUOwners;
  auto writableNullMask=[&](const DescriptorTable &layout) -> bytebuf * {
    auto &mask=nullBufferFields[make_rdcpair(layout.buffer,layout.offset)];
    if(!mask || mask.use_count()!=1)
    {
      const uint64_t bytes=(layout.count+7)/8;
      if(!preflightBudget.Consume(bytes)){success=false;return NULL;}
      auto next=std::make_shared<bytebuf>();
      if(mask)*next=*mask;else {next->resize(bytes);memset(next->data(),0,next->size());}
      mask=next;
    }
    return mask.get();
  };
  auto setNullBufferField=[&](ResourceId table,uint64_t offset,bool isNull) {
    for(const auto &layout:m_DescriptorTables)
      if(layout.buffer==table && layout.stride==24 && offset>=layout.offset &&
         (offset-layout.offset)%24==0 && (offset-layout.offset)/24<layout.count)
      {
        auto mask=writableNullMask(layout);if(!mask)return;
        const uint64_t row=(offset-layout.offset)/24;
        (*mask)[row/8]=byte(((*mask)[row/8]&~(1U<<(row%8)))|(isNull?1U<<(row%8):0));
      }
  };
  auto invalidateNullBufferFields=[&](ResourceId table) {
    for(auto &proof:nullBufferFields)if(proof.first.first==table)proof.second.reset();
    for(auto i=nullGPUOwners.begin();i!=nullGPUOwners.end();)
      if(i->first.first==table)i=nullGPUOwners.erase(i);else ++i;
  };
  for(const auto &layout:m_DescriptorTables)
  {
    const auto initial=m_ReplayBufferInitialContents.find(layout.buffer);
    if(layout.stride!=24 || initial==m_ReplayBufferInitialContents.end())continue;
    auto mask=writableNullMask(layout);if(!mask)break;
    for(uint64_t row=0;row<layout.count;row++)
    {
      const uint64_t offset=layout.offset+row*24;
      if(offset>initial->second.size() || 8>initial->second.size()-offset){success=false;break;}
      uint64_t word=0;memcpy(&word,initial->second.data()+offset,8);
      const DescriptorSlotKey key=make_rdcpair(layout.buffer,offset);
      const auto slot=m_DescriptorSlotInitial.find(key);
      // OverlayDescriptorSlotBuffer(initialRestore=true) clears stale identity
      // fields of an explicitly retired initial generation (or a validated
      // leading retirement). Match that actual Native reconstruction, rather
      // than treating its old captured address as a missing live publication.
      // A frame retirement does not acquire this fact; later CPU/GPU writes
      // still invalidate/revalidate it at the consuming submission.
      const bool invalidatedInitial=slot!=m_DescriptorSlotInitial.end() &&
          (!slot->second.live || IsDescriptorPreludeRetirement(key,slot->second));
      if(!word || invalidatedInitial)(*mask)[row/8]|=byte(1U<<(row%8));
    }
  }
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
    // The map is ordered by table resource, then byte offset. Resource
    // dependency tracking must visit this table's fields, not rescan every
    // unrelated descriptor for each ordinary buffer/texture use.
    for(auto slot=m_DescriptorSlotShadow.lower_bound(make_rdcpair(resource,0ULL));
        slot!=m_DescriptorSlotShadow.end() && slot->first.first==resource;++slot)
      if(slot->second.live)
      {
        const auto &entry=*slot;
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
    // This qualification belongs to this exact draw. A later mesh/other PSO
    // on the same encoder cannot borrow its pending graphics proof.
    const bool graphicsConsumer=graphicsConsumerEncoders.erase(encoder)!=0 && liveRenders.count(encoder);
    // An encoded draw/dispatch owns its resource closure, as in D3D12/Vulkan.
    // Unrelated values may still be waiting for their source annotation when
    // this command buffer is submitted.
    for(const auto &slot : m_DescriptorSlotShadow)
      if(slot.second.live && encoderResources[encoder].count(slot.first.first) &&
         !(m_DescriptorCoverage >= 64 && slot.second.data.empty()))
      {
        if(slot.second.sources.count(3))
        {
          const auto compute=computes.find(encoder);
          const bool consumer = (liveEncoders.count(encoder) && compute!=computes.end() &&
              (HasRayQueryHeapPipeline(compute->second.pipeline) ||
               runtimeConsumerEncoders.count(encoder))) ||
              graphicsConsumer;
          if(!consumer && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
            fprintf(stderr, "Metal AS submission closure: encoder=%s pipeline=%s table=%s offset=%llu live=%d qualified=%d\n",
                ToStr(encoder).c_str(), ToStr(compute!=computes.end()?compute->second.pipeline:
                    graphics[{encoder,1}].pipeline).c_str(),
                ToStr(slot.first.first).c_str(), (unsigned long long)slot.first.second,
                liveEncoders.count(encoder) != 0,
                compute!=computes.end() && HasRayQueryHeapPipeline(compute->second.pipeline));
          success &= consumer;
        }
        m_DescriptorSubmissionSlots[encoderCommands[encoder]].insert(slot.first);
      }
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
  std::set<ResourceId> liveParallelRenders, knownEncoders;
  std::map<ResourceId, ResourceId> childParents, activeChildren;
  std::map<ResourceId,bool> renderAttachmentless;
  std::map<ResourceId,ResourceId> renderVisibilityBuffers;
  std::map<ResourceId,std::set<ResourceId>> colorWriteCommands, nativeTextureClearCommands;
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
  std::map<std::tuple<ResourceId, bool, uint64_t, uint64_t>, ResourceId> initializedDepthCommands;
  std::map<std::tuple<ResourceId, uint64_t, uint64_t>, ResourceId> initializedColorCommands;
  auto hasNativeTextureProducer = [&](ResourceId resource, ResourceId command) {
    const ResourceId backing = viewBacking(resource);
    MTL::PixelFormat format = MTL::PixelFormatInvalid;
    uint64_t firstMip=0, firstSlice=0, mips=1, slices=1, volumeDepth=0;
    const auto future=m_DescriptorFrameTextures.find(resource);
    if(future!=m_DescriptorFrameTextures.end())
    {
      const auto &d=future->second.descriptor;
      format=d.pixelFormat; mips=d.mipmapLevelCount;
      if(d.textureType==MTL::TextureType3D)volumeDepth=d.depth;
      slices=d.textureType==MTL::TextureType2DArray?d.arrayLength:
          d.textureType==MTL::TextureTypeCube || d.textureType==MTL::TextureTypeCubeArray?6*d.arrayLength:1;
      if(m_DescriptorFrameTextureViewMipLevels.count(resource))
        firstMip=m_DescriptorFrameTextureViewMipLevels.at(resource);
      if(m_DescriptorFrameTextureViewSlices.count(resource))
        firstSlice=m_DescriptorFrameTextureViewSlices.at(resource);
    }
    else
    {
      const auto object=GetResourceManager()->GetResource(resource,true);
      const auto native=object && object->m_Type==eResTexture?Unwrap((WrappedMTLTexture *)object):NULL;
      if(native)
      {
        format=native->pixelFormat(); mips=native->mipmapLevelCount();
        if(native->textureType()==MTL::TextureType3D)volumeDepth=native->depth();
        firstMip=native->parentRelativeLevel(); firstSlice=native->parentRelativeSlice();
        slices=native->textureType()==MTL::TextureType2DArray?native->arrayLength():
            native->textureType()==MTL::TextureTypeCube || native->textureType()==MTL::TextureTypeCubeArray?6*native->arrayLength():1;
      }
    }
    const bool stencil=format==MTL::PixelFormatX32_Stencil8 || format==MTL::PixelFormatStencil8;
    const bool depth=stencil || format==MTL::PixelFormatDepth32Float_Stencil8 ||
        format==MTL::PixelFormatDepth32Float || format==MTL::PixelFormatDepth16Unorm;
    for(ResourceId source : {resource,backing})
    {
      const auto writer=typedTextureWriteCommands.find(source);
      if(writer!=typedTextureWriteCommands.end() &&
         (writer->second==command || committed.count(writer->second)))return true;
      if(!depth)
      {
        for(ResourceId clear:nativeTextureClearCommands[source])
          if(clear==command || committed.count(clear))return true;
        // A projected view retains real color clears in its mip/layer region.
        // This records an original producer, never a whole-image pixel proof.
        const uint64_t mipBase=source==resource?0:firstMip;
        const uint64_t sliceBase=source==resource?0:firstSlice;
        for(const auto &clear:initializedColorCommands)
        {
          const uint64_t mip=std::get<1>(clear.first),slice=std::get<2>(clear.first);
          if(std::get<0>(clear.first)!=source || mip<mipBase || mip-mipBase>=mips)continue;
          const uint64_t layers=volumeDepth?RDCMAX(1ULL,volumeDepth>>(mip-mipBase)):slices;
          if(slice>=sliceBase && slice-sliceBase<layers &&
             (clear.second==command || committed.count(clear.second)))return true;
        }
      }
      // Preserve plane/subresource identity. A depth clear is not a stencil
      // producer; a clear outside a view's projection is not its input either.
      for(const auto &clear:initializedDepthCommands)
        if(depth && std::get<0>(clear.first)==source && std::get<1>(clear.first)==stencil &&
           std::get<2>(clear.first)>=firstMip && std::get<2>(clear.first)-firstMip<mips &&
           std::get<3>(clear.first)>=firstSlice && std::get<3>(clear.first)-firstSlice<slices &&
           (clear.second==command || committed.count(clear.second)))return true;
    }
    return false;
  };
  std::map<ResourceId, std::set<uint64_t>> appliedInline;
  std::map<ResourceId, uint32_t> capturedIndirectOrdinals;
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
  struct ComputeCBVOperation
  {
    ResourceId buffer;
    uint64_t offset, bytes;
    bool read, known;
    uint64_t readOffset;
    bytebuf data;
    // Resolve GPU copies at submission, after prior CPU updates/copies/writers,
    // rather than freezing the source's pre-frame bytes while scanning commands.
    ResourceId source;
    uint64_t sourceOffset = 0;
    uint32_t stage = 0;
    // Native indexed input is an actual API byte interval. Its restored state
    // is required; CPU vertex_id values are only optional shader analysis.
    bool indexedInput = false;
    bool indirectInput = false;
  };
  std::map<ResourceId,rdcarray<ComputeCBVOperation>> computeCBVOperations;
  struct UniformDispatch
  {
    ResourceId pipeline,heap;
    rdcarray<byte> roots;
    std::map<uint64_t,DescriptorSource> pointers;
    std::map<uint64_t,DescriptorSlotShadow> slots;
    std::map<DescriptorSlotKey,DescriptorSlotShadow> samplers;
    std::map<DescriptorSlotKey,RayASHeader> headers;
    std::vector<uint64_t> invocationExtent;
    std::set<ResourceId> writes;
  };
  std::map<uint64_t,UniformDispatch> uniformDispatches;
  auto validateUniformDispatch = [&](uint64_t readOffset) {
    const auto found=uniformDispatches.find(readOffset);
    if(found==uniformDispatches.end())return false;
    const auto &dispatch=found->second;
    rdcstr entry;const rdcstr air=GetReplay()->GetComputeAIR(dispatch.pipeline,entry);
    using Value=MetalAIR::Value;
    Value root;root.kind=Value::Pointer;root.slot=2;root.metadata=true;
    Value heap;heap.kind=Value::Pointer;heap.metadata=true;memcpy(&heap.object,&dispatch.heap,sizeof(heap.object));
    auto load=[&](const Value &address,unsigned bytes,Value::Kind kind) {
      Value result;
      if(address.slot==2)
      {
        if(address.offset>dispatch.roots.size() || bytes>dispatch.roots.size()-address.offset)return result;
        if(kind==Value::Pointer)
        {
          const auto pointer=dispatch.pointers.find(address.offset);
          if(pointer==dispatch.pointers.end())return result;
          result.kind=kind;memcpy(&result.object,&pointer->second.resource,sizeof(result.object));
          result.offset=pointer->second.offset;
          const auto typed=m_IRComputeRoots.find(dispatch.pipeline);
          if(typed!=m_IRComputeRoots.end())
            for(const auto &declaration:typed->second)
              result.metadata |= declaration.offset==address.offset && declaration.kind==3;
        }
        else if(kind==Value::Integer)
        {result.kind=kind;memcpy(&result.offset,dispatch.roots.data()+address.offset,bytes);}
        return result;
      }
      ResourceId buffer;memcpy(&buffer,&address.object,sizeof(buffer));
      if(kind==Value::Sampler)
      {
        const auto sampler=dispatch.samplers.find(make_rdcpair(buffer,address.offset));
        if(sampler!=dispatch.samplers.end() && sampler->second.live &&
           sampler->second.type==7 && sampler->second.sources.count(2))
        {result=address;result.kind=kind;}
        return result;
      }
      if(buffer==dispatch.heap)
      {
        const uint64_t field=address.offset%24,slotOffset=address.offset-field;
        const auto slot=dispatch.slots.find(slotOffset);
        if(slot==dispatch.slots.end())return result;
        const uint32_t sourceKind=kind==Value::Texture && field==8?1:
            kind==Value::Pointer && !field?(slot->second.sources.count(3)?3:0):UINT32_MAX;
        const auto source=slot->second.sources.find(sourceKind);
        if(source==slot->second.sources.end())return result;
        result=address;result.kind=kind;
        if(kind==Value::Pointer)
        {
          memcpy(&result.object,&source->second.resource,sizeof(result.object));result.offset=source->second.offset;
          result.descriptorObject=address.object;result.descriptorOffset=slotOffset;
          result.metadata=sourceKind==3;
        }
        return result;
      }
      if(kind==Value::AccelerationStructure &&
         dispatch.headers.count(make_rdcpair(buffer,address.offset)))
      {result=address;result.kind=kind;return result;}
      if(kind==Value::Integer)
      {
        bytebuf data;
        if(!ReadProvenIRComputeUniformBytes(readOffset,buffer,address.offset,bytes,data))
        {
          const auto initial=m_ReplayBufferInitialContents.find(buffer);
          if(initial==m_ReplayBufferInitialContents.end() || address.offset>initial->second.size() ||
             bytes>initial->second.size()-address.offset)return result;
          data.assign(initial->second.data()+address.offset,bytes);
        }
        result.kind=kind;memcpy(&result.offset,data.data(),bytes);
      }
      return result;
    };
    const auto report=MetalAIR::UniformResourceAccess(air.c_str(),entry.c_str(),{{0,heap},{2,root}},load,dispatch.invocationExtent);
    if(Process::GetEnvVariable("RENDERDOC_METAL_TRACE_UNIFORM_PROOFS")=="1")
      RDCLOG("MetalUniformConsumer chunk=%llu valid=%u AS=%u unknownAS=%u texture=%u unknownTexture=%u sampler=%u unknownSampler=%u bufferRead=%u bufferWrite=%u unknownBuffer=%u",
          (unsigned long long)readOffset,uint32_t(report.validModule),report.queryResets,
          report.unresolvedStructures,report.textureCalls,report.unresolvedTextures,
          report.samplerCalls,report.unresolvedSamplers,report.bufferReads,report.bufferWrites,report.unresolvedBuffers);
    if(!report.validModule || !report.queryResets || report.unresolvedStructures || report.unresolvedTextures ||
       report.unresolvedSamplers || report.unresolvedBuffers)
      return false;
    for(const auto &access:report.buffers)
    {
      ResourceId buffer;memcpy(&buffer,&access.address.object,sizeof(buffer));
      if(!access.address.offsetKnown)return false;
      if(access.address.descriptorObject)
      {
        const auto slot=dispatch.slots.find(access.address.descriptorOffset);
        if(slot==dispatch.slots.end() || !slot->second.sources.count(0) ||
           slot->second.sources.at(0).resource!=buffer || !access.write ||
           !dispatch.writes.count(buffer))return false;
        bool output=false;
        for(const auto &cbv:m_RayQueryHeapCBVRoots.at(dispatch.pipeline))
          output |= cbv.outputSlot==access.address.descriptorOffset;
        const auto description=GetReplay()->GetBuffer(buffer);
        if(!output || access.address.High()>description.length || access.bytes>description.length-access.address.High())return false;
      }
      else
      {
        if(access.write)return false;
        bool bounded=false;
        for(const auto &cbv:m_RayQueryHeapCBVRoots.at(dispatch.pipeline))
        {
          const auto pointer=dispatch.pointers.find(cbv.offset);
          if(pointer!=dispatch.pointers.end() && pointer->second.resource==buffer &&
             access.address.offset>=pointer->second.offset &&
             access.address.High()-pointer->second.offset<=cbv.bytes &&
             access.bytes<=cbv.bytes-(access.address.High()-pointer->second.offset))bounded=true;
        }
        if(!bounded)return false;
      }
    }
    const auto declared=m_IRComputeHeapEntries.find(dispatch.pipeline);
    if(declared==m_IRComputeHeapEntries.end() && report.textureCalls)return false;
    for(const auto &access:report.accesses)
    {
      if(access.kind==Value::Sampler)
      {
        ResourceId table;memcpy(&table,&access.object,sizeof(table));
        if(!dispatch.samplers.count(make_rdcpair(table,access.offset)))return false;
        continue;
      }
      const bool structure=access.kind==Value::AccelerationStructure;
      const uint64_t offset=structure?access.descriptorOffset:access.offset-8;
      bool role=false;
      if(structure)
        for(const auto &query:m_RayQueryHeapDispatches)
          role |= query.pipeline==dispatch.pipeline && query.heap==dispatch.heap && query.slotOffset==offset;
      if(declared!=m_IRComputeHeapEntries.end())
        for(const auto &declaration:declared->second)
          if(declaration.heap==0 && declaration.index*24==offset)
            role |= structure?declaration.kind==3:
                (access.write || access.ordering)?(declaration.kind==4 || declaration.kind==8):
                access.resourceOnly?(declaration.kind==1 || declaration.kind==4 || declaration.kind==8):declaration.kind==1;
      if(!role)return false;
      if(!structure)
      {
        const auto slot=dispatch.slots.find(offset);
        if(slot==dispatch.slots.end() || !slot->second.sources.count(1))return false;
        const auto type=GetReplay()->GetTexture(slot->second.sources.at(1).resource).format.compType;
        if(!access.resourceOnly && !TextureNumericFamily(access.numeric,type))return false;
      }
    }
    return true;
  };
  struct ComputeCBVBytes { bytebuf data, known; };
  std::map<ResourceId,ComputeCBVBytes> computeCBVKnownBytes;
  // Sparse exact scalar versions avoid allocating a whole large UAV just to
  // establish a counter. Resource/physical alias writers invalidate them.
  struct ScalarByte { byte value; uint64_t writer; };
  std::map<ResourceId,std::map<uint64_t,ScalarByte>> computeCBVScalarKnownBytes;
  std::map<ResourceId,uint64_t> opaqueScalarWriteVersions;
  std::set<ResourceId> computeInvalidatedBuffers;
  uint64_t computeCBVProofBytes=0;
  auto noteOpaqueScalarWrite=[&](ResourceId buffer,uint64_t version) {
    auto invalidate=[&](ResourceId target) {
      opaqueScalarWriteVersions[target]=RDCMAX(opaqueScalarWriteVersions[target],version);
      computeCBVScalarKnownBytes.erase(target);computeInvalidatedBuffers.insert(target);
      const auto known=computeCBVKnownBytes.find(target);
      if(known!=computeCBVKnownBytes.end())memset(known->second.known.data(),0,known->second.known.size());
    };
    invalidate(buffer);
    const auto write=indirectFootprint(buffer);
    if(!write.valid || !write.heap)return;
    std::set<ResourceId> candidates=m_DescriptorPreflightLiveBuffers;
    for(const auto &candidate:GetReplay()->GetBuffers())candidates.insert(candidate.resourceId);
    for(ResourceId candidate:candidates)
    {
      const auto alias=indirectFootprint(candidate);
      if(alias.valid && alias.heap==write.heap && alias.begin<write.end && write.begin<alias.end)
        invalidate(candidate);
    }
  };
  auto preserveCreationInput=[&](ResourceId buffer) {
    if(m_ReplayBufferInitialContents.count(buffer))return true;
    // Like DX12/Vulkan initial state, creation bytes are authoritative only
    // before a producer changes them. Never rediscover scalar facts from a
    // GPU-written buffer or from relocated descriptor words.
    if(!m_ReplayBuffersWithCreationContents.count(buffer) ||
       m_DescriptorFrameBuffers.count(buffer) || computeInvalidatedBuffers.count(buffer) ||
       modifiedBuffers.count(buffer) || isTable(buffer))return false;
    auto object=GetResourceManager()->GetResource(buffer,true);
    MTL::Buffer *native=object && object->m_Type==eResBuffer && object->m_Real ?
        Unwrap((WrappedMTLBuffer *)object):NULL;
    if(!native || native->storageMode()!=MTL::StorageModeShared || !native->contents() ||
       !native->length() || native->length()>64ULL*1024*1024-computeCBVProofBytes)return false;
    computeCBVProofBytes+=native->length();
    m_ReplayBufferInitialContents[buffer]=bytebuf((byte *)native->contents(),native->length());
    return true;
  };
  auto computeByteState=[&](ResourceId buffer) -> ComputeCBVBytes * {
      const auto future=m_DescriptorFrameBuffers.find(buffer);
      const auto layout=indirectFootprint(buffer);
      const uint64_t length=future!=m_DescriptorFrameBuffers.end()?future->second.length:
          GetReplay()->GetBuffer(buffer).length;
      if(!length || !layout.valid || !layout.buffer ||
         length>DescriptorFrameBufferLimit(bool(layout.heap),m_DescriptorCoverage))return NULL;
      auto &values=computeCBVKnownBytes[buffer];
      if(values.known.empty())
      {
        if(length>(64ULL*1024*1024-computeCBVProofBytes)/2) return NULL;
        computeCBVProofBytes+=length*2;
        values.known.resize(length);memset(values.known.data(),0,values.known.size());
        values.data.resize(length);
        if(future==m_DescriptorFrameBuffers.end() && !computeInvalidatedBuffers.count(buffer))
        {
          preserveCreationInput(buffer);
          const auto initial=m_ReplayBufferInitialContents.find(buffer);
          if(initial!=m_ReplayBufferInitialContents.end() && initial->second.size()==length)
          {
            memcpy(values.data.data(),initial->second.data(),length);
            memset(values.known.data(),1,length);
          }
        }
      }
      const auto scalars=computeCBVScalarKnownBytes.find(buffer);
      if(scalars!=computeCBVScalarKnownBytes.end())
        for(const auto &known:scalars->second)
          if(known.first<length){values.data[known.first]=known.second.value;values.known[known.first]=1;}
      return &values;
    };
  // Resource contents can be restored without their numerical values being
  // available to CPU access display. Keep copy/upload coverage independent of
  // scalar proofs; a root upload does not initialize disjoint texel pixels.
  using RestoredRange = rdcpair<uint64_t,uint64_t>;
  std::map<ResourceId,std::vector<RestoredRange>> restoredBufferRanges;
  std::map<MTL::Heap *,std::vector<RestoredRange>> restoredPlacementBufferRanges;
  std::set<ResourceId> restoredBufferSeeds;
  auto insertRestoredRange=[](std::vector<RestoredRange> &ranges,uint64_t begin,uint64_t end) {
    if(begin>=end)return;
    auto i=ranges.begin();
    while(i!=ranges.end() && i->second<begin)++i;
    while(i!=ranges.end() && i->first<=end)
    {begin=RDCMIN(begin,i->first);end=RDCMAX(end,i->second);i=ranges.erase(i);}
    ranges.insert(i,{begin,end});
  };
  auto removeRestoredRange=[](std::vector<RestoredRange> &ranges,uint64_t begin,uint64_t end) {
    std::vector<RestoredRange> remaining;
    for(const auto &range:ranges)
    {
      if(range.second<=begin || range.first>=end){remaining.push_back(range);continue;}
      if(range.first<begin)remaining.push_back({range.first,begin});
      if(range.second>end)remaining.push_back({end,range.second});
    }
    ranges.swap(remaining);
  };
  // Buffer aliases share bytes, not scalar values or typed pointer origins.
  // Native texture/AS footprints are opaque and never qualify raw buffer data.
  auto placementBufferRange=[&](ResourceId buffer,MTL::Heap *&heap,uint64_t &base,uint64_t &length) {
    const auto footprint=indirectFootprint(buffer);
    length=m_DescriptorFrameBuffers.count(buffer)?m_DescriptorFrameBuffers.at(buffer).length:
        GetReplay()->GetBuffer(buffer).length;
    if(!footprint.valid || !footprint.buffer || !footprint.heap || !length ||
       footprint.begin>UINT64_MAX-length || footprint.begin+length>footprint.end)return false;
    heap=footprint.heap;base=footprint.begin;return true;
  };
  auto addRestoredRange=[&](ResourceId buffer,uint64_t begin,uint64_t end) {
    if(begin>=end)return;
    MTL::Heap *heap=NULL;uint64_t base=0,length=0;
    if(placementBufferRange(buffer,heap,base,length))
    {
      if(end>length)return;
      insertRestoredRange(restoredPlacementBufferRanges[heap],base+begin,base+end);
    }
    else insertRestoredRange(restoredBufferRanges[buffer],begin,end);
  };
  auto eraseRestoredRange=[&](ResourceId buffer,uint64_t begin,uint64_t end) {
    MTL::Heap *heap=NULL;uint64_t base=0,length=0;
    if(placementBufferRange(buffer,heap,base,length))
    {
      if(begin>end || end>length)return;
      removeRestoredRange(restoredPlacementBufferRanges[heap],base+begin,base+end);
    }
    else removeRestoredRange(restoredBufferRanges[buffer],begin,end);
  };
  // Seed captured initial bytes before any submission mutates this physical
  // state. A later alias must not resurrect another logical object's old initial.
  for(const auto &initial:m_ReplayBufferInitialContents)
    if(!m_DescriptorFrameBuffers.count(initial.first) &&
       initial.second.size()==GetReplay()->GetBuffer(initial.first).length)
    {restoredBufferSeeds.insert(initial.first);addRestoredRange(initial.first,0,initial.second.size());}
  auto bufferRestoredRanges=[&](ResourceId buffer) -> const std::vector<RestoredRange> & {
    if(restoredBufferSeeds.insert(buffer).second && !m_DescriptorFrameBuffers.count(buffer))
    {
      preserveCreationInput(buffer);
      const auto initial=m_ReplayBufferInitialContents.find(buffer);
      const uint64_t length=GetReplay()->GetBuffer(buffer).length;
      if(length && initial!=m_ReplayBufferInitialContents.end() && initial->second.size()==length)
        addRestoredRange(buffer,0,length);
    }
    MTL::Heap *heap=NULL;uint64_t base=0,length=0;
    if(placementBufferRange(buffer,heap,base,length))
    {
      auto &projected=restoredBufferRanges[buffer];projected.clear();
      const auto physical=restoredPlacementBufferRanges.find(heap);
      if(physical!=restoredPlacementBufferRanges.end())for(const auto &range:physical->second)
      {
        const uint64_t low=RDCMAX(base,range.first),high=RDCMIN(base+length,range.second);
        if(low<high)projected.push_back({low-base,high-base});
      }
    }
    return restoredBufferRanges[buffer];
  };
  std::set<ResourceId> restoredIndexInputs;
  struct RuntimeDispatch
  {
    NullFieldProofs nullBufferFields;
    std::map<DescriptorSlotKey,ResourceId> nullGPUOwners;
    ResourceId pipeline, heap, samplerHeap;
    uint64_t heapOffset=0, samplerHeapOffset=0;
    MetalIRComputeRuntimeABI abi;
    rdcarray<byte> roots;
    std::map<uint64_t,DescriptorSource> pointers;
    std::map<DescriptorSlotKey,DescriptorSlotShadow> slots;
    std::set<ResourceId> opaque;
    std::map<rdcpair<ResourceId,uint64_t>,RayASHeader> headers;
    std::map<ResourceId,std::shared_ptr<MetalASInitialBuild>> structures;
    std::map<ResourceId,ResourceId> structureOwners;
    std::vector<uint64_t> invocationExtent;
    uint32_t stage=0;
    ComputeSnapshot graphics;
    bool nativeCompute=false;
    MetalAIR::InvocationValues builtins;
    ResourceId indexBuffer;
    uint64_t indexOffset=0,indexCount=0,indexStride=0,vertexStart=0,instanceCount=0,baseInstance=0;
    int64_t baseVertex=0;
  };
  std::map<rdcpair<uint64_t,uint32_t>,RuntimeDispatch> runtimeDispatches;
  auto validateRuntimeDispatch=[&](uint64_t readOffset,ResourceId command,uint32_t stage) {
    const auto &dispatch=runtimeDispatches.at(make_rdcpair(readOffset,stage));
    const auto &abi=dispatch.abi;
    using Value=MetalAIR::Value;
    Value root;root.kind=Value::Pointer;root.slot=abi.rootBindPoint;root.metadata=true;
    Value heap;heap.kind=Value::Pointer;heap.offset=dispatch.heapOffset;heap.metadata=true;
    memcpy(&heap.object,&dispatch.heap,sizeof(heap.object));
    std::set<ResourceId> rootBuffers;
    ResourceId samplerTable;
    uint64_t samplerOffset=0;
    struct ScalarRead { ResourceId buffer; uint64_t begin,end; };
    std::vector<ScalarRead> scalarReads;
    // Numerical load facts are optional. Object/typed-field relocation below
    // is independent and must remain valid after a Native read/modify/write.
    std::map<ResourceId,std::vector<RestoredRange>> mutableScalarInputs;
    for(uint32_t i=0;!dispatch.stage && i<abi.cbvCount;i++)
    {
      const auto pointer=dispatch.pointers.find(uint64_t(i)*8);
      if(pointer==dispatch.pointers.end() || pointer->second.resource==ResourceId() ||
         isTable(pointer->second.resource))return false;
      rootBuffers.insert(pointer->second.resource);
    }
    if(!dispatch.stage && abi.staticSamplerCount)
    {
      const auto pointer=dispatch.pointers.find(uint64_t(abi.cbvCount)*8);
      if(pointer==dispatch.pointers.end() || !isTable(pointer->second.resource))return false;
      samplerTable=pointer->second.resource;samplerOffset=pointer->second.offset;
    }
    if(dispatch.stage || dispatch.nativeCompute)
    {
      for(const auto &binding:dispatch.graphics.buffers)
        if(!isTable(binding.second.resourceId) && (!dispatch.nativeCompute ||
           GetReplay()->IsComputeBufferReadOnly(dispatch.pipeline,binding.first)))
          rootBuffers.insert(binding.second.resourceId);
      if(!dispatch.nativeCompute)for(const auto &binding:dispatch.graphics.pointers)
        for(const auto &pointer:binding.second)
          if(!isTable(pointer.second.resource))rootBuffers.insert(pointer.second.resource);
    }
    auto load=[&](const Value &address,unsigned bytes,Value::Kind kind) {
      Value result;
      if((dispatch.stage || dispatch.nativeCompute) && address.slot>=0)
      {
        const auto data=dispatch.graphics.inlineData.find(uint32_t(address.slot));
        if(data==dispatch.graphics.inlineData.end() || !address.offsetKnown || address.ranged ||
           address.offset>data->second.size() || bytes>data->second.size()-address.offset)return result;
        const auto binding=dispatch.graphics.pointers.find(uint32_t(address.slot));
        if(kind==Value::Pointer && bytes==8 && binding!=dispatch.graphics.pointers.end())
        {
          const auto pointer=binding->second.find(address.offset);
          if(pointer==binding->second.end())return result;
          result.kind=kind;memcpy(&result.object,&pointer->second.resource,sizeof(result.object));
          result.offset=pointer->second.offset;result.metadata=isTable(pointer->second.resource);
        }
        else if(kind==Value::Integer && bytes<=8)
        {result.kind=kind;memcpy(&result.offset,data->second.data()+address.offset,bytes);}
        return result;
      }
      if(!dispatch.stage && address.slot==int(abi.rootBindPoint))
      {
        if(address.ranged || !address.offsetKnown || kind!=Value::Pointer || bytes!=8 || address.offset%8 ||
           address.offset>dispatch.roots.size() || bytes>dispatch.roots.size()-address.offset)return result;
        const auto pointer=dispatch.pointers.find(address.offset);
        if(pointer==dispatch.pointers.end())return result;
        const bool sampler=abi.staticSamplerCount && address.offset==uint64_t(abi.cbvCount)*8;
        if(address.offset>=uint64_t(abi.cbvCount)*8 && !sampler)return result;
        result.kind=kind;memcpy(&result.object,&pointer->second.resource,sizeof(result.object));
        result.offset=pointer->second.offset;result.metadata=sampler;
        return result;
      }
      ResourceId buffer;memcpy(&buffer,&address.object,sizeof(buffer));
      const uint64_t field=address.offset%24;
      const auto slot=dispatch.slots.find(make_rdcpair(buffer,address.offset-field));
      if(address.metadata && isTable(buffer))
      {
        if(!address.offsetKnown || address.ranged)
        {
          // A compiler-projected row in the current typed table is a restored
          // namespace, not an integer GPU-address guess. GPU indices execute
          // natively; only the ABI field and complete namespace are proved.
          if(kind!=Value::Pointer || bytes!=8 || address.namespaceField ||
             address.namespaceStride!=24)return result;
          for(const auto &layout:m_DescriptorTables)
            if(layout.buffer==buffer && layout.stride==address.namespaceStride &&
               address.namespaceBase>=layout.offset &&
               (address.namespaceBase-layout.offset)%layout.stride==0 &&
               (address.namespaceBase-layout.offset)/layout.stride<layout.count &&
               (!address.namespaceEnd || (address.namespaceEnd>address.namespaceBase &&
                 address.namespaceEnd>=layout.offset && (address.namespaceEnd-layout.offset)%layout.stride==0 &&
                 (address.namespaceEnd-layout.offset)/layout.stride<=layout.count)))
            {
              result.kind=Value::DescriptorPointer;result.object=address.object;
              result.namespaceBase=address.namespaceBase;result.namespaceStride=layout.stride;
              result.namespaceEnd=address.namespaceEnd;
              result.offsetKnown=false;result.descriptorObject=address.object;
              return result;
            }
          return result;
        }
        if(address.ranged)return result;
        bool sourcedGPUCopy=slot!=dispatch.slots.end() && slot->second.gpuExpected &&
            slot->second.gpuCopyPublished && slot->second.sources.size()==slot->second.gpuCopySources.size();
        if(sourcedGPUCopy)for(const auto &source:slot->second.sources)
        {
          const auto expected=slot->second.gpuCopySources.find(source.first);
          sourcedGPUCopy &= expected!=slot->second.gpuCopySources.end() &&
              expected->second.resource==source.second.resource && expected->second.offset==source.second.offset;
        }
        // Initial GPU-produced values have already passed captured-byte/typed
        // identity validation and initial relocation. Frame GPU values require
        // their actual producer/copy sources instead; publications clear the
        // initial qualification. Neither case manufactures a frame CPU write.
        if(slot==dispatch.slots.end() || !slot->second.live ||
           (slot->second.gpuExpected && !slot->second.initialRestored && !sourcedGPUCopy))
        {
          if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
            fprintf(stderr,"Metal runtime descriptor unavailable: table=%s offset=%llu field=%llu missing=%d live=%d GPUexpected=%d\n",
                ToStr(buffer).c_str(),(unsigned long long)(address.offset-field),(unsigned long long)field,
                int(slot==dispatch.slots.end()),int(slot!=dispatch.slots.end() && slot->second.live),
                int(slot!=dispatch.slots.end() && slot->second.gpuExpected));
          return result;
        }
        // Converted roots have a fixed sampler ABI. Native typed tables use
        // their actual sourceKind=2 and restored slot identity below.
        if(!dispatch.stage && !dispatch.nativeCompute && kind==Value::Sampler)
        {
          const bool nativeHeap=dispatch.samplerHeap!=ResourceId() && buffer==dispatch.samplerHeap &&
              address.offset>=dispatch.samplerHeapOffset && !field;
          const bool staticRoot=abi.staticSamplerCount && buffer==samplerTable &&
              address.offset>=samplerOffset && (address.offset-samplerOffset)%24==0 &&
              (address.offset-samplerOffset)/24<abi.staticSamplerCount;
          if(!nativeHeap && !staticRoot)return result;
        }
        if(kind==Value::Integer && field==16 && bytes<=8 && slot->second.data.size()>=24)
        {result.kind=kind;memcpy(&result.offset,slot->second.data.data()+16,bytes);return result;}
        const uint32_t sourceKind=kind==Value::Texture && field==8?1:
            kind==Value::Sampler && !field?2:kind==Value::Pointer && !field?
            (slot->second.sources.count(3)?3:0):UINT32_MAX;
        const auto source=slot->second.sources.find(sourceKind);
        if(source==slot->second.sources.end())return result;
        result=address;result.kind=kind;result.slot=-1;
        if(kind==Value::Pointer)
        {
          memcpy(&result.object,&source->second.resource,sizeof(result.object));
          result.offset=source->second.offset;result.metadata=sourceKind==3;
          result.descriptorObject=address.object;result.descriptorOffset=address.offset;
        }
        return result;
      }
      // The typed AS descriptor points at a public two-field header. Both
      // fields are relocated from explicit resource identities, independently
      // of Native query results or CPU index analysis.
      if(address.metadata && address.offsetKnown && !address.ranged && bytes==8)
      {
        const uint64_t headerOffset=kind==Value::Pointer && address.offset>=8?address.offset-8:address.offset;
        const auto header=dispatch.headers.find(make_rdcpair(buffer,headerOffset));
        if(header!=dispatch.headers.end() && !dispatch.opaque.count(buffer))
        {
          if(kind==Value::AccelerationStructure)
          {result=address;result.kind=kind;return result;}
          if(kind==Value::Pointer && address.offset==header->second.offset+8)
          {
            result=address;result.kind=Value::Pointer;result.metadata=false;
            memcpy(&result.object,&header->second.contributions,sizeof(result.object));
            result.offset=header->second.contributionOffset;
            result.descriptorObject=0;result.descriptorOffset=0;
            rootBuffers.insert(header->second.contributions);
            return result;
          }
        }
      }
      if(kind==Value::Integer && bytes<=8 && address.offsetKnown &&
         address.High()>=address.offset)
      {
        auto unknownScalar=[&](const char *reason) {
          if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
            fprintf(stderr,"Metal runtime scalar unavailable: resource=%s offset=%llu high=%llu bytes=%u reason=%s opaque=%d proofBytes=%llu\n",
                ToStr(buffer).c_str(),(unsigned long long)address.offset,(unsigned long long)address.High(),
                bytes,reason,int(dispatch.opaque.count(buffer)),(unsigned long long)computeCBVProofBytes);
          return result;
        };
        const auto mutableInput=mutableScalarInputs.find(buffer);
        if(mutableInput!=mutableScalarInputs.end())for(const auto &range:mutableInput->second)
          if(address.offset<range.second && range.first<address.High()+bytes)
            return unknownScalar("same-dispatch mutable input");
        const uint64_t span=address.High()-address.offset+bytes;
        if(span>64*1024 || span<bytes || address.offset%bytes || span%bytes)return result;
        // Current byte validity is maintained in submission order. Opaque
        // writers invalidate both dense and sparse facts, and prevent initial
        // bytes from being seeded again. A later validated CPU upload/copy can
        // restore exact bytes even though the allocation's historical opaque
        // writer flag remains set. That flag is not the current byte version.
        auto values=computeByteState(buffer);
        bytebuf observed,known;
        const byte *sourceBytes=NULL;
        if(values)
        {
          if(address.offset>values->data.size() || span>values->data.size()-address.offset)
            return unknownScalar("scalar range");
          for(uint64_t i=0;i<span;i++)if(!values->known[address.offset+i])return unknownScalar("unknown byte");
          sourceBytes=values->data.data()+address.offset;
        }
        else
        {
          // Optional dense scalar caching must not require two copies of an
          // entire allocation for a small ordinary control input. Read only
          // the demanded interval from authoritative initial/CPU publications.
          // Never recover old bytes after a GPU/alias writer, or consult a
          // future submission. Typed pointer loads still use the separate
          // relocation/publication path above; this observes integers only.
          const uint64_t length=m_DescriptorFrameBuffers.count(buffer)?m_DescriptorFrameBuffers.at(buffer).length:
              GetReplay()->GetBuffer(buffer).length;
          if(!length || address.offset>length || span>length-address.offset ||
             computeInvalidatedBuffers.count(buffer) || opaqueScalarWriteVersions.count(buffer))
            return unknownScalar("unavailable current scalar version");
          observed.resize(span);known.resize(span);memset(known.data(),0,span);
          const auto initial=m_ReplayBufferInitialContents.find(buffer);
          if(!m_DescriptorFrameBuffers.count(buffer) && initial!=m_ReplayBufferInitialContents.end() &&
             initial->second.size()==length)
          {memcpy(observed.data(),initial->second.data()+address.offset,span);memset(known.data(),1,span);}
          auto overlay=[&](ResourceId owner) {
            for(const auto &update:submissionUpdates[owner])if(update.buffer==buffer)
            {
              if(update.start>length || update.data.size()>length-update.start)return false;
              const uint64_t low=RDCMAX(address.offset,update.start);
              const uint64_t high=RDCMIN(address.offset+span,update.start+update.data.size());
              if(low<high)
              {memcpy(observed.data()+low-address.offset,update.data.data()+low-update.start,high-low);
               memset(known.data()+low-address.offset,1,high-low);}
            }
            return true;
          };
          for(ResourceId prior:submissionOrder)if(!overlay(prior))return unknownScalar("invalid CPU publication");
          if(!overlay(command))return unknownScalar("invalid CPU publication");
          for(byte value:known)if(!value)return unknownScalar("missing scalar publication");
          sourceBytes=observed.data();
          if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
            fprintf(stderr,"Metal runtime scalar interval observation: resource=%s offset=%llu bytes=%llu\n",
                ToStr(buffer).c_str(),(unsigned long long)address.offset,(unsigned long long)span);
        }
        auto &proof=m_IRComputeUniformReadContents[readOffset][make_rdcpair(buffer,address.offset)];
        const uint64_t common=RDCMIN(uint64_t(proof.size()),span);
        if(common && memcmp(proof.data(),sourceBytes,common))return result;
        if(proof.size()<span)
        {
          const uint64_t extra=span-proof.size();
          if(extra>64ULL*1024*1024-computeCBVProofBytes)return result;
          computeCBVProofBytes+=extra;proof.assign(sourceBytes,span);
        }
        uint64_t low=UINT64_MAX,high=0;
        for(uint64_t i=0;i<span;i+=bytes)
        {uint64_t scalar=0;memcpy(&scalar,proof.data()+i,bytes);low=RDCMIN(low,scalar);high=RDCMAX(high,scalar);}
        result=Value::Range(low,high);scalarReads.push_back({buffer,address.offset,address.offset+span});
      }
      return result;
    };
    rdcstr entry,air;std::map<rdcstr,rdcpair<rdcstr,rdcstr>> linked;
    std::map<unsigned,Value> bindings;
    if(dispatch.stage || dispatch.nativeCompute)
    {
      if(dispatch.nativeCompute)
      {air=GetReplay()->GetComputeAIR(dispatch.pipeline,entry);if(entry.empty())return false;}
      else if(!GetReplay()->GetGraphicsAIR(dispatch.pipeline,dispatch.stage,entry,air,linked) || entry.empty())return false;
      for(const auto &binding:dispatch.graphics.buffers)
      {
        Value v;v.kind=Value::Pointer;memcpy(&v.object,&binding.second.resourceId,sizeof(v.object));
        v.offset=binding.second.byteOffset;v.metadata=isTable(binding.second.resourceId);
        bindings[binding.first]=v;
      }
      for(const auto &binding:dispatch.graphics.inlineData)
      {Value v;v.kind=Value::Pointer;v.slot=binding.first;v.metadata=true;bindings[binding.first]=v;}
    }
    else
    {
      air=GetReplay()->GetComputeAIR(dispatch.pipeline,entry);
      bindings={{abi.resourceBindPoint,heap},{abi.rootBindPoint,root}};
      if(dispatch.samplerHeap!=ResourceId())
      {
        Value samplers;samplers.kind=Value::Pointer;samplers.metadata=true;
        memcpy(&samplers.object,&dispatch.samplerHeap,sizeof(samplers.object));
        samplers.offset=dispatch.samplerHeapOffset;bindings[abi.samplerBindPoint]=samplers;
      }
    }
    if(dispatch.nativeCompute && air.empty())
    {
      // With no original AIR, undeclared inline fields cannot be certified as
      // non-address inputs by this path. Keep the existing explicit-layout
      // recovery requirement; optional access display remains unknown.
      for(const auto &inlineData:dispatch.graphics.inlineData)
      {
        const auto origins=dispatch.graphics.pointers.find(inlineData.first);
        if(origins==dispatch.graphics.pointers.end() || origins->second.empty())
          return false;
      }
      // Source-compiled Native PSOs may have no captured AIR for display.
      // All pointer fields/typed handles were restored independently. Keep
      // original Native execution and prove explicit input producers without
      // inventing a shader access list or numerical output.
      std::set<ResourceId> nativeWrites;
      auto nativeResources=dispatch.graphics.residency;
      // Argument-buffer resources may be made resident by useHeap. Derive
      // their possible effects from restored typed fields in the tables
      // actually bound to this dispatch, never from every resident heap slot.
      for(const auto &binding:dispatch.graphics.pointers)
        for(const auto &pointer:binding.second)
          if(isTable(pointer.second.resource))
            for(const auto &slot:dispatch.slots)
              if(slot.first.first==pointer.second.resource && slot.second.live &&
                 MetalDescriptor::Texture(slot.second.type))
              {
                const auto source=slot.second.sources.find(1);
                if(source==slot.second.sources.end())return false;
                nativeResources[source->second.resource] |= MetalDescriptor::Writable(slot.second.type)
                    ? MTL::ResourceUsageWrite : MTL::ResourceUsageRead;
              }
      for(const auto &residency:nativeResources)
      {
        const ResourceId resource=residency.first, parent=viewBacking(resource);
        const bool texture=m_DescriptorFrameTextures.count(resource) ||
            GetReplay()->GetTexture(resource).resourceId!=ResourceId();
        if(!texture)continue;
        if(residency.second & MTL::ResourceUsageRead)
        {
          bool restored=HasReplayTextureInitialContents(resource) || HasReplayTextureInitialContents(parent);
          restored |= hasNativeTextureProducer(resource,command);
          if(!restored)return false;
        }
        if(residency.second & MTL::ResourceUsageWrite)nativeWrites.insert(resource);
      }
      // Only an actual dispatch establishes a possible opaque producer. Its
      // bytes remain unknown to CPU analysis; a residency call alone does not.
      for(ResourceId resource:nativeWrites)
      {
        typedTextureWriteCommands[resource]=command;
        modifiedBuffers.insert(resource);opaqueWrites.insert(resource);
      }
      const auto key=make_rdcpair(readOffset,dispatch.stage);
      m_IRRuntimeDescriptorAccessComplete[key]=false;
      m_IRRuntimeDescriptorAccesses[key].clear();
      if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal Native restoration accepted: offset=%llu AIR=unavailable accessDisplay=unknown actualDispatch=1 opaqueTextures=%zu\n",
            (unsigned long long)readOffset,nativeWrites.size());
      return true;
    }
    std::set<std::string> callStack;unsigned callCount=0;
    std::function<MetalAIR::UniformAccessReport(const std::string &,const std::string &,
        const MetalAIR::InvocationValues &)> analyse;
    analyse=[&](const std::string &module,const std::string &name,const MetalAIR::InvocationValues &values) {
      if(callStack.size()>=8 || !callStack.insert(name).second)return MetalAIR::UniformAccessReport();
      // Resolve aliases only from this caller's AIR metadata and the functions
      // attached to the actual Native stage. A label or suffix is not identity.
      std::map<std::string,std::string> aliases;
      const std::regex reference(R"ref(!"air.visible_function_reference", [^\n]*@([-a-zA-Z$._0-9]+), !"([^"\n]+)")ref");
      for(std::sregex_iterator i(module.begin(),module.end(),reference),end;i!=end;++i)
        aliases[(*i)[1]]=(*i)[2];
      // Internal helpers already belong to this captured Native module. Their
      // call parameters are sourced at the caller, not Metal binding slots.
      std::set<std::string> localDefinitions;
      std::istringstream definitions(module);std::string definition;
      while(std::getline(definitions,definition))
        if(definition.compare(0,16,"define internal ")==0 ||
           definition.compare(0,15,"define private ")==0)
        {
          const size_t nameBegin=definition.find('@'),nameEnd=definition.find('(',nameBegin);
          if(nameBegin!=std::string::npos && nameEnd!=std::string::npos)
            localDefinitions.insert(definition.substr(nameBegin+1,nameEnd-nameBegin-1));
        }
      auto resolver=[&](const std::string &symbol,const std::vector<Value> &parameters) {
        if(localDefinitions.count(symbol))
        {
          if(++callCount>128)return MetalAIR::UniformAccessReport();
          MetalAIR::InvocationValues invocation;invocation.localDefinition=true;
          for(unsigned i=0;i<parameters.size();i++)invocation.arguments[i]=parameters[i];
          return analyse(module,symbol,invocation);
        }
        const auto alias=aliases.find(symbol);const rdcstr target=(alias==aliases.end()?symbol:alias->second).c_str();
        const auto function=linked.find(target);
        if(function==linked.end() || ++callCount>128)return MetalAIR::UniformAccessReport();
        MetalAIR::InvocationValues invocation;
        for(unsigned i=0;i<parameters.size();i++)invocation.arguments[i]=parameters[i];
        return analyse(function->second.second.c_str(),function->second.first.c_str(),invocation);
      };
      auto report=MetalAIR::UniformResourceAccess(module,name,callStack.size()==1?bindings:
          std::map<unsigned,Value>(),load,dispatch.invocationExtent,true,values,resolver,dispatch.stage!=0);
      callStack.erase(name);return report;
    };
    auto builtins=dispatch.builtins;
    if(dispatch.stage==1)
    {
      uint64_t low=dispatch.vertexStart,high=low+dispatch.indexCount-1;
      if(dispatch.indexBuffer!=ResourceId())
      {
        auto data=computeByteState(dispatch.indexBuffer);
        const auto count=dispatch.indexCount,stride=dispatch.indexStride,begin=dispatch.indexOffset;
        if(!restoredIndexInputs.count(dispatch.indexBuffer))return false;
        bool numerical=data && count && (stride==2 || stride==4) && begin<=data->data.size() &&
            count<=(data->data.size()-begin)/stride;
        low=UINT64_MAX;high=0;
        for(uint64_t i=0;numerical && i<count;i++)
        {
          for(uint64_t b=0;b<stride;b++)numerical &= data->known[begin+i*stride+b]!=0;
          if(!numerical)break;
          uint32_t index=0;memcpy(&index,data->data.data()+begin+i*stride,stride);
          // Native vertex_id is an unsigned 32-bit builtin. Do not impose a
          // scene-sized vertex limit on the original indexed draw operation.
          const uint32_t vertex=uint32_t(uint64_t(index)+uint64_t(dispatch.baseVertex));
          low=RDCMIN(low,uint64_t(vertex));high=RDCMAX(high,uint64_t(vertex));
        }
        rootBuffers.insert(dispatch.indexBuffer);
        if(numerical)scalarReads.push_back({dispatch.indexBuffer,begin,begin+count*stride});
        else
        {
          low=UINT64_MAX;high=0;
          if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
            fprintf(stderr,"Metal indexed input restored: consumer=%llu buffer=%s offset=%llu count=%llu stride=%llu CPUstate=%d scalarBudget=%llu vertexID=unknown\n",
                (unsigned long long)readOffset,ToStr(dispatch.indexBuffer).c_str(),
                (unsigned long long)begin,(unsigned long long)count,(unsigned long long)stride,
                int(data!=NULL),(unsigned long long)computeCBVProofBytes);
        }
      }
      builtins.builtins["vertex_id"]=low<=high?Value::Range(low,high):Value();
      builtins.builtins["instance_id"]=Value::Range(dispatch.baseInstance,
          dispatch.baseInstance+dispatch.instanceCount-1);
      builtins.builtins["base_instance"]=Value::Number(dispatch.baseInstance);
      builtins.builtins["base_vertex"]=Value::Number(uint32_t(dispatch.baseVertex));
    }
    // Recompute display/namespace analysis without pre-dispatch scalar values
    // from potentially written allocations. A Native shader can read its own
    // writes; the old initial bytes cannot authorize a descriptor selection.
    // Keep typed object identity and metadata, and reject any namespace that
    // becomes unresolved. Unknown write ranges conservatively cover their
    // allocation; known ranges retain disjoint roots/texels on shared backing.
    const auto requiredScalarReads=scalarReads; // Native indexed draw inputs
    MetalAIR::UniformAccessReport report;
    for(unsigned pass=0;;pass++)
    {
      // Uniform CPU analysis budget, independent of scene/shader identity.
      if(pass==8)return false;
      scalarReads=requiredScalarReads;
      callCount=0;
      report=analyse(air.c_str(),entry.c_str(),builtins);
      struct PotentialWrite { ResourceId resource; uint64_t begin,end; };
      std::vector<PotentialWrite> potentialWrites;
      auto bufferLength=[&](ResourceId resource) {
        return m_DescriptorFrameBuffers.count(resource)?m_DescriptorFrameBuffers.at(resource).length:
            GetReplay()->GetBuffer(resource).length;
      };
      for(const auto &access:report.buffers)if(access.write)
      {
        ResourceId resource;memcpy(&resource,&access.address.object,sizeof(resource));
        const uint64_t length=bufferLength(resource);
        const bool bounded=access.address.offsetKnown && access.address.High()<=length &&
            access.bytes<=length-access.address.High();
        potentialWrites.push_back({resource,bounded?access.address.offset:0,
            bounded?access.address.High()+access.bytes:length});
      }
      for(const auto &access:report.accesses)if(access.kind==Value::Texture && access.write && access.offset>=8)
      {
        ResourceId table;memcpy(&table,&access.object,sizeof(table));
        const auto slot=dispatch.slots.find(make_rdcpair(table,access.offset-8));
        if(slot!=dispatch.slots.end() && slot->second.sources.count(1))
        {
          const auto texture=slot->second.sources.at(1).resource;
          const auto backing=viewBacking(texture);
          const auto target=backing!=ResourceId()?backing:texture;
          uint64_t begin=0,end=bufferLength(target),width=0;
          if(!end)
          {
            const auto allocation=indirectFootprint(target);
            if(allocation.valid)end=allocation.end-allocation.begin;
          }
          MTL::PixelFormat format=MTL::PixelFormatInvalid;
          const auto future=runtimeTextureViews.find(texture);
          if(future!=runtimeTextureViews.end() && future->second.descriptor.textureType==MTL::TextureTypeTextureBuffer)
          {begin=future->second.offset;width=future->second.descriptor.width;format=future->second.descriptor.pixelFormat;}
          else
          {
            auto object=GetResourceManager()->GetResource(texture,true);
            auto native=object && object->m_Type==eResTexture?Unwrap((WrappedMTLTexture *)object):NULL;
            if(native && native->textureType()==MTL::TextureTypeTextureBuffer)
            {begin=native->bufferOffset();width=native->width();format=native->pixelFormat();}
          }
          if(width)
          {
            uint32_t bw=0,bh=0,pixelBytes=0;
            if(!GetTextureDataBlockShape(format,bw,bh,pixelBytes) || bw!=1 || bh!=1 || !pixelBytes ||
               begin>end || width>(end-begin)/pixelBytes)return false;
            end=begin+width*pixelBytes;
          }
          potentialWrites.push_back({target,begin,end});
        }
      }
      bool changed=false;
      for(const auto &read:scalarReads)for(const auto &writer:potentialWrites)
      {
        const auto write=indirectFootprint(writer.resource),source=indirectFootprint(read.buffer);
        const bool alias=write.valid && source.valid && write.heap && write.heap==source.heap &&
            source.begin<write.end && write.begin<source.end;
        const bool same=writer.resource==read.buffer;
        if(!same && !alias)continue;
        const uint64_t writeBase=same?0:write.begin,readBase=same?0:source.begin;
        if(writeBase+writer.begin>=readBase+read.end || readBase+read.begin>=writeBase+writer.end)continue;
        // Index bytes determine Native draw invocation bounds, not just the
        // optional shader display. Their simultaneous mutation stays rejected.
        for(const auto &required:requiredScalarReads)if(required.buffer==read.buffer)return false;
        mutableScalarInputs[read.buffer].push_back({read.begin,read.end});changed=true;
      }
      if(!changed)break;
      auto &proofs=m_IRComputeUniformReadContents[readOffset];
      for(auto proof=proofs.begin();proof!=proofs.end();)
      {
        bool invalid=false;
        const auto input=mutableScalarInputs.find(proof->first.first);
        if(input!=mutableScalarInputs.end())for(const auto &range:input->second)
          invalid |= proof->first.second<range.second && range.first<proof->first.second+proof->second.size();
        if(invalid){computeCBVProofBytes-=proof->second.size();proof=proofs.erase(proof);}
        else ++proof;
      }
      if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal runtime mutable scalar facts invalidated: offset=%llu pass=%u buffers=%zu\n",
            (unsigned long long)readOffset,pass+1,mutableScalarInputs.size());
    }
    if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
      fprintf(stderr,"Metal runtime consumer: offset=%llu pipeline=%s valid=%d AS=%u unknownAS=%u read=%u write=%u unknownBuffer=%u texture=%u unknownTexture=%u sampler=%u unknownSampler=%u unknownCall=%u loops=%u stage=%u linked=%u shaderConstantReads=%u\n",
          (unsigned long long)readOffset,ToStr(dispatch.pipeline).c_str(),report.validModule,
          report.queryResets,report.unresolvedStructures,report.bufferReads,report.bufferWrites,
          report.unresolvedBuffers,report.textureCalls,report.unresolvedTextures,report.samplerCalls,
          report.unresolvedSamplers,report.unresolvedCalls,report.boundedLoops,dispatch.stage,report.linkedCalls,
          report.shaderConstantReads);
    if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
      for(const auto &instruction:report.unresolvedBufferInstructions)
        fprintf(stderr,"Metal runtime unresolved buffer instruction: consumer=%llu %s\n",
            (unsigned long long)readOffset,instruction.c_str());
    // Shader bytecode constants are independent of initial/GPU-produced data.
    // An embedded captured resource VA would remain stale in the unmodified
    // shader, even if optional CPU analysis currently picks another branch.
    // Match registered identities/ranges, never a numeric address heuristic.
    auto requiresRelocation=[&](uint64_t literal,uint64_t bytes) {
      for(const auto &identity:m_ReplayGPUIdentities)
      {
        if(identity.second.kind!=0)continue;
        const auto born=m_DescriptorFrameBuffers.find(identity.first);
        const uint64_t length=born!=m_DescriptorFrameBuffers.end()?born->second.length:
            GetReplay()->GetBuffer(identity.first).length;
        if(length && ((literal>=identity.second.value && literal-identity.second.value<length) ||
           (literal<identity.second.value && identity.second.value-literal<bytes)))
        {
          if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
            fprintf(stderr,"Metal runtime relocation required: embedded shader address=%llu capturedResource=%s offset=%llu\n",
                (unsigned long long)literal,ToStr(identity.first).c_str(),
                (unsigned long long)(literal>=identity.second.value?literal-identity.second.value:0));
          return true;
        }
      }
      return false;
    };
    for(uint64_t literal:report.literalPointers)
      if(requiresRelocation(literal,1))return false;
    for(const auto &span:report.literalAccesses)
      if(requiresRelocation(span.first,span.second))return false;
    // Missing pointer origins/calls still need independent namespace recovery.
    // Runtime RT requires its AS/Header closure, not numerical shader emulation.
    if(!report.validModule || report.unresolvedStructures ||
       report.unresolvedBuffers || report.unresolvedTextures || report.unresolvedSamplers ||
       report.unresolvedCalls)return false;
    // Resolve dynamic buffer selections against the complete current typed
    // namespace. These are restoration candidates, never actual shader access
    // feedback. A missing/opaque publication, stale source, absent initial
    // contents or producer cannot borrow another slot's successful recovery.
    bool dynamicNamespace=false;
    std::vector<MetalAIR::UniformAccessReport::BufferAccess> restoredAccesses;
    std::map<rdcpair<ResourceId,rdcpair<uint64_t,uint64_t>>,uint64_t> restoredNamespaces;
    for(const auto &access:report.buffers)
    {
      if(access.address.kind!=Value::DescriptorPointer)
      {restoredAccesses.push_back(access);continue;}
      dynamicNamespace=true;
      if(access.write)
      {
        // The read closure cannot certify writable descriptor types or the
        // initialized ranges produced by a GPU-selected destination.
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal dynamic descriptor writer coverage unavailable: table=%llu base=%llu\n",
              (unsigned long long)access.address.object,
              (unsigned long long)access.address.namespaceBase);
        return false;
      }
      ResourceId table;memcpy(&table,&access.address.object,sizeof(table));
      const auto namespaceKey=make_rdcpair(table,make_rdcpair(access.address.namespaceBase,access.address.namespaceEnd));
      const auto priorNamespace=restoredNamespaces.find(namespaceKey);
      // Reuse only a closure proved for at least this read width. Allocation
      // capacity is not a contents proof for a later wider load.
      if(priorNamespace!=restoredNamespaces.end() && access.bytes<=priorNamespace->second)continue;
      bool recovered=false;
      for(const auto &layout:m_DescriptorTables)
      {
        if(layout.buffer!=table || layout.stride!=access.address.namespaceStride ||
           access.address.namespaceBase<layout.offset ||
           (access.address.namespaceBase-layout.offset)%layout.stride)continue;
        const uint64_t first=(access.address.namespaceBase-layout.offset)/layout.stride;
        if(first>=layout.count)continue;
        uint64_t last=layout.count;
        if(access.address.namespaceEnd)
        {
          if(access.address.namespaceEnd<=access.address.namespaceBase || access.address.namespaceEnd<layout.offset ||
             (access.address.namespaceEnd-layout.offset)%layout.stride)return false;
          last=(access.address.namespaceEnd-layout.offset)/layout.stride;
          if(last>layout.count)return false;
        }
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal dynamic descriptor namespace: table=%s base=%llu first=%llu rows=%llu readBytes=%llu\n",
              ToStr(table).c_str(),(unsigned long long)access.address.namespaceBase,
              (unsigned long long)first,(unsigned long long)(last-first),(unsigned long long)access.bytes);
        // Entries already obey the uniform table/snapshot budget. Do not
        // replace the old 64-value display cap with a scene-sized pointer set.
        for(uint64_t row=first;row<last;row++)
        {
          const uint64_t slotOffset=layout.offset+row*layout.stride;
          const auto nulls=dispatch.nullBufferFields.find(make_rdcpair(table,layout.offset));
          if(nulls!=dispatch.nullBufferFields.end() && nulls->second &&
             row/8<nulls->second->size() && ((*nulls->second)[row/8]&(1U<<(row%8))))
          {
            // Commit-owned mapped-memory updates precede this command's GPU
            // work. They cannot borrow a stale null fact; only a later matched
            // typed GPU copy in this command may restore that zero field.
            for(const auto &update:submissionUpdates[command])
              if(update.buffer==table && update.start<slotOffset+8 &&
                 slotOffset<update.start+update.data.size())
              {
                const auto producer=dispatch.nullGPUOwners.find(make_rdcpair(table,slotOffset));
                if(producer!=dispatch.nullGPUOwners.end() && producer->second==command)continue;
                if(update.start>slotOffset || slotOffset+8>update.start+update.data.size())return false;
                uint64_t word=0;memcpy(&word,update.data.data()+slotOffset-update.start,8);
                if(word)return false;
              }
            continue;
          }
          const auto slot=dispatch.slots.find(make_rdcpair(table,slotOffset));
          if(slot==dispatch.slots.end() || !slot->second.live)
          {
            if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
              fprintf(stderr,"Metal dynamic descriptor publication unavailable: table=%s slot=%llu missing=%d\n",
                  ToStr(table).c_str(),(unsigned long long)slotOffset,int(slot==dispatch.slots.end()));
            return false;
          }
          const auto &state=slot->second;
          bool published=!state.gpuExpected || state.initialRestored;
          if(state.gpuExpected && state.gpuCopyPublished &&
             state.sources.size()==state.gpuCopySources.size())
          {
            published=true;
            for(const auto &source:state.sources)
            {
              const auto copied=state.gpuCopySources.find(source.first);
              published &= copied!=state.gpuCopySources.end() &&
                  copied->second.resource==source.second.resource && copied->second.offset==source.second.offset;
            }
          }
          if(!published)return false;
          // All row identities were validated/relocated independently. Only
          // this API buffer field participates in the possible read closure.
          if(!MetalDescriptor::Buffer(state.type) || state.sources.count(3))continue;
          uint64_t captured=0;if(state.data.size()!=24)return false;
          memcpy(&captured,state.data.data(),8);
          const auto source=state.sources.find(0);
          if(!captured)
          {if(source!=state.sources.end())return false;continue;}
          if(source==state.sources.end())return false;
          const ResourceId buffer=source->second.resource;
          const uint64_t length=m_DescriptorFrameBuffers.count(buffer)?m_DescriptorFrameBuffers.at(buffer).length:
              GetReplay()->GetBuffer(buffer).length;
          uint32_t declared=0;memcpy(&declared,state.data.data()+16,4);
          const uint64_t begin=source->second.offset;
          const uint64_t size=!dispatch.nativeCompute && declared?declared:(begin<=length?length-begin:0);
          if(begin>length || !size || size>length-begin || access.bytes>size ||
             (m_DescriptorFrameBuffers.count(buffer) && !m_DescriptorPreflightLiveBuffers.count(buffer)) ||
             m_DescriptorPreflightAliasedBuffers.count(buffer))return false;
          bool initialized=false;
          for(const auto &range:bufferRestoredRanges(buffer))
          {
            // A dynamic binding is a possible dependency, not evidence that
            // the shader reads its entire allocation. Retain the actual
            // restored intervals (including partial Native uploads), without
            // promoting undefined padding to initialized bytes. Native code
            // selects legal offsets; absent contents/producer still reject.
            const uint64_t low=RDCMAX(begin,range.first);
            const uint64_t high=RDCMIN(begin+size,range.second);
            initialized |= low<high && access.bytes<=high-low;
          }
          // This is an ordinary-data state, not initialized-byte or pointer
          // proof. A never-owned physical range has API-unspecified contents;
          // Native execution may select it without CPU producer analysis.
          // Captured defined inputs still require their real payload/producer.
          if(!initialized && !m_ReplayHeapBufferBirthUnspecified.count(buffer))
          {
            if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
              fprintf(stderr,"Metal dynamic descriptor contents unavailable: table=%s slot=%llu resource=%s range=%llu/%llu\n",
                  ToStr(table).c_str(),(unsigned long long)slotOffset,ToStr(buffer).c_str(),
                  (unsigned long long)begin,(unsigned long long)size);
            return false;
          }
          auto candidate=access;candidate.address.kind=Value::Pointer;
          memcpy(&candidate.address.object,&buffer,sizeof(buffer));
          candidate.address.offset=begin;candidate.address.offsetKnown=false;
          candidate.address.descriptorObject=access.address.object;
          candidate.address.descriptorOffset=slotOffset;
          restoredAccesses.push_back(candidate);recovered=true;
          resourceCommands[buffer].insert(command);
        }
      }
      if(!recovered)return false;
      restoredNamespaces[namespaceKey]=access.bytes;
    }
    report.buffers.swap(restoredAccesses);
    // Binding restoration is independent of the optional numerical access
    // display. A sourced pointer can have an unknown GPU-computed byte offset:
    // replay the Native shader against the restored allocation, rather than
    // requiring a CPU implementation of its index/branch expressions.
    struct RuntimeBindingRestoration
    {
      std::set<ResourceId> writes;
      bool completeAccessDisplay=true;
    } restoration;
    restoration.completeAccessDisplay=!dynamicNamespace && report.pointerSelections==0 && !report.shaderConstantReads;
    auto &writes=restoration.writes;
    for(const auto &access:report.buffers)
    {
      ResourceId buffer;memcpy(&buffer,&access.address.object,sizeof(buffer));
      const uint64_t length=m_DescriptorFrameBuffers.count(buffer)?m_DescriptorFrameBuffers.at(buffer).length:
          GetReplay()->GetBuffer(buffer).length;
      if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal runtime buffer range: resource=%s offset=%llu high=%llu bytes=%llu write=%d native=%llu table=%llu slot=%llu known=%d\n",
            ToStr(buffer).c_str(),(unsigned long long)access.address.offset,
            (unsigned long long)access.address.High(),(unsigned long long)access.bytes,int(access.write),
            (unsigned long long)length,(unsigned long long)access.address.descriptorObject,
            (unsigned long long)access.address.descriptorOffset,int(access.address.offsetKnown));
      if(!length || !access.bytes || access.bytes>length ||
         m_DescriptorPreflightAliasedBuffers.count(buffer))return false;
      bool knownRange=access.address.offsetKnown;
      if(knownRange && (access.address.High()>length ||
         access.bytes>length-access.address.High()))
      {
        // A branch may restrict invocation coverage. Its static candidate
        // interval cannot reject a restored binding as an actual GPU overrun.
        if(!report.conditionalAccesses)return false;
        knownRange=false;
      }
      if(access.address.descriptorObject)
      {
        ResourceId table;memcpy(&table,&access.address.descriptorObject,sizeof(table));
        const auto slot=dispatch.slots.find(make_rdcpair(table,access.address.descriptorOffset));
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT") && slot!=dispatch.slots.end())
          fprintf(stderr,"Metal runtime buffer source: table=%s slot=%llu type=%u source0=%d\n",
              ToStr(table).c_str(),(unsigned long long)access.address.descriptorOffset,
              slot->second.type,int(slot->second.sources.count(0)));
        if(slot==dispatch.slots.end() || !slot->second.sources.count(0) ||
           slot->second.sources.at(0).resource!=buffer ||
           !MetalDescriptor::Buffer(slot->second.type) ||
           (access.write && !MetalDescriptor::Writable(slot->second.type)))return false;
        const uint64_t begin=slot->second.sources.at(0).offset;
        uint32_t declared=0;
        // Converted IR descriptors carry a byte-count field here. Native
        // argument-buffer payloads can contain arbitrary user metadata; their
        // buffer extent comes from the restored Native allocation/source.
        if(!dispatch.nativeCompute && slot->second.data.size()>=24)
          memcpy(&declared,slot->second.data.data()+16,4);
        // Validate the relocated view itself even when numerical indexing
        // cannot be displayed. Never substitute an unresolved GPU pointer.
        if(begin>length || (declared && declared>length-begin))return false;
        if(knownRange && (access.address.offset<begin || (declared &&
           (access.address.High()>begin+declared ||
            access.bytes>begin+declared-access.address.High()))))
        {
          if(!report.conditionalAccesses)return false;
          knownRange=false;
        }
      }
      else if(dispatch.nativeCompute)
      {
        bool bound=false;
        for(const auto &binding:dispatch.graphics.buffers)
          if(binding.second.resourceId==buffer && (!knownRange ||
             access.address.offset>=binding.second.byteOffset))
          {
            if(access.write && GetReplay()->IsComputeBufferReadOnly(dispatch.pipeline,binding.first))return false;
            bound=true;
          }
        for(const auto &binding:dispatch.graphics.pointers)
          for(const auto &pointer:binding.second)
            bound |= pointer.second.resource==buffer && (!knownRange || access.address.offset>=pointer.second.offset);
        if(!bound)return false;
      }
      else if(access.write || !rootBuffers.count(buffer))return false;
      restoration.completeAccessDisplay &= knownRange;
      if(access.write)
      {
        // Submitted scalar/index facts cannot describe a loop or a parallel
        // indirect address if this invocation can overwrite the same bytes.
        const auto write=indirectFootprint(buffer);
        for(const auto &read:scalarReads)
        {
          const auto source=indirectFootprint(read.buffer);
          const bool same=read.buffer==buffer;
          const bool alias=write.valid && source.valid && write.heap && write.heap==source.heap;
          if(!same && !alias)continue;
          const uint64_t writeBase=same?0:write.begin,readBase=same?0:source.begin;
          if(knownRange && writeBase+access.address.offset<readBase+read.end &&
             readBase+read.begin<writeBase+access.address.High()+access.bytes)
          {
            if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
              fprintf(stderr,"Metal runtime scalar writer overlap: output=%s input=%s inputRange=%llu/%llu\n",
                  ToStr(buffer).c_str(),ToStr(read.buffer).c_str(),
                  (unsigned long long)read.begin,(unsigned long long)read.end);
            return false;
          }
        }
        writes.insert(buffer);
      }
      else if(!modifiedBuffers.count(buffer) && !m_ReplayBufferInitialContents.count(buffer) &&
              !m_DescriptorFrameBuffers.count(buffer) && !preserveCreationInput(buffer))return false;
    }
    for(const auto &access:report.accesses)
    {
      ResourceId table;memcpy(&table,&access.object,sizeof(table));
      if(access.kind==Value::AccelerationStructure)
      {
        const auto header=dispatch.headers.find(make_rdcpair(table,access.offset));
        if(header==dispatch.headers.end() || dispatch.opaque.count(table) ||
           !access.metadata || !access.offsetKnown || access.ranged || !access.descriptorObject)return false;
        ResourceId descriptorHeap;memcpy(&descriptorHeap,&access.descriptorObject,sizeof(descriptorHeap));
        const auto slot=dispatch.slots.find(make_rdcpair(descriptorHeap,access.descriptorOffset));
        if(slot==dispatch.slots.end() || !slot->second.live || !slot->second.sources.count(3) ||
           slot->second.sources.at(3).resource!=table || slot->second.sources.at(3).offset!=access.offset)return false;
        const auto structure=header->second.structure;
        const auto recipe=dispatch.structures.find(structure);
        auto object=GetResourceManager()->GetResource(structure,true);
        if(!object || object->m_Type!=eResAccelerationStructure || !object->m_Real ||
           recipe==dispatch.structures.end() || !recipe->second || recipe->second->parameters.size()!=8)return false;
        // Recipes were validated at initial-state restoration or the actual
        // typed build command. Freeze the graph at this dispatch, so a later
        // build of the same AS cannot supply an earlier missing producer.
        const auto &build=*recipe->second;
        if(build.kind!=5 && build.kind!=9 && build.kind!=10 && build.kind!=11)return false;
        auto readyStructure=[&](ResourceId resource) {
          const auto owner=dispatch.structureOwners.find(resource);
          return owner==dispatch.structureOwners.end() || owner->second==command || committed.count(owner->second)!=0;
        };
        if(!readyStructure(structure))return false;
        for(ResourceId child:build.children)
        {
          const auto primitive=dispatch.structures.find(child);
          auto childObject=GetResourceManager()->GetResource(child,true);
          if(!childObject || childObject->m_Type!=eResAccelerationStructure || !childObject->m_Real ||
             primitive==dispatch.structures.end() || !primitive->second ||
             (primitive->second->kind!=1 && primitive->second->kind!=2 && primitive->second->kind!=3 && primitive->second->kind!=8) ||
             !readyStructure(child))return false;
          resourceCommands[child].insert(command);
        }
        resourceCommands[structure].insert(command);
        resourceCommands[table].insert(command);
        resourceCommands[header->second.contributions].insert(command);
        continue;
      }
      if(access.kind==Value::Sampler)
      {
        const auto slot=dispatch.slots.find(make_rdcpair(table,access.offset));
        if(slot==dispatch.slots.end() || slot->second.type!=7 || !slot->second.sources.count(2))return false;
        continue;
      }
      if(access.kind!=Value::Texture || access.offset<8)return false;
      const auto slot=dispatch.slots.find(make_rdcpair(table,access.offset-8));
      if(slot==dispatch.slots.end() || !slot->second.sources.count(1) ||
         !MetalDescriptor::Texture(slot->second.type) ||
         ((access.write || access.ordering) && !MetalDescriptor::Writable(slot->second.type)))
      {
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal runtime texture binding rejected: consumer=%llu table=%s slot=%llu present=%d type=%u source=%d read=%d write=%d ordering=%d\n",
              (unsigned long long)readOffset,ToStr(table).c_str(),(unsigned long long)(access.offset-8),
              int(slot!=dispatch.slots.end()),slot!=dispatch.slots.end()?slot->second.type:0,
              int(slot!=dispatch.slots.end() && slot->second.sources.count(1)),
              int(access.read),int(access.write),int(access.ordering));
        return false;
      }
      const ResourceId texture=slot->second.sources.at(1).resource;
      auto description=GetReplay()->GetTexture(texture);
      uint64_t textureUsage=0;
      auto textureObject=GetResourceManager()->GetResource(texture,true);
      if(textureObject && textureObject->m_Type==eResTexture && textureObject->m_Real)
        textureUsage=uint64_t(Unwrap((WrappedMTLTexture *)textureObject)->usage());
      // During preflight a frame object is represented by its validated
      // factory record; its Native object/public description is born later.
      const auto future=m_DescriptorFrameTextures.find(texture);
      if(m_DescriptorPreflight && future!=m_DescriptorFrameTextures.end() &&
         m_DescriptorPreflightLiveTextures.count(texture))
      {
        description.resourceId=texture;
        description.width=uint32_t(future->second.descriptor.width);
        description.height=uint32_t(future->second.descriptor.height);
        description.format=MakeResourceFormat(future->second.descriptor.pixelFormat);
        textureUsage=uint64_t(future->second.descriptor.usage);
      }
      const auto futureView=runtimeTextureViews.find(texture);
      if(m_DescriptorPreflight && futureView!=runtimeTextureViews.end() &&
         m_DescriptorPreflightLiveViews.count(texture))
      {
        description.resourceId=texture;
        description.width=uint32_t(futureView->second.descriptor.width);
        description.height=uint32_t(futureView->second.descriptor.height);
        description.format=MakeResourceFormat(futureView->second.descriptor.pixelFormat);
        textureUsage=uint64_t(futureView->second.descriptor.usage);
      }
      // Unknown usage permits all uses. Explicit usage must retain the actual
      // shader operation's API permission, including a writable texture fence.
      const uint64_t requiredUsage=(access.read?MTL::TextureUsageShaderRead:0) |
          ((access.write || access.ordering)?MTL::TextureUsageShaderWrite:0);
      if(textureUsage && (textureUsage & requiredUsage)!=requiredUsage)
      {
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal runtime texture usage rejected: consumer=%llu source=%s usage=%llu required=%llu\n",
              (unsigned long long)readOffset,ToStr(texture).c_str(),
              (unsigned long long)textureUsage,(unsigned long long)requiredUsage);
        return false;
      }
      if(description.resourceId!=texture || !description.width || !description.height)
      {
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal runtime texture metadata unavailable: resource=%s frameView=%d liveView=%d\n",
              ToStr(texture).c_str(),int(m_DescriptorFrameViews.count(texture)),
              int(m_DescriptorPreflightLiveViews.count(texture)));
        return false;
      }
      // RenderDoc displays S8 as a depth/stencil aspect; Metal shader reads
      // this aspect as unsigned integers. Display categorisation is not the ABI.
      const auto type=description.format.type == ResourceFormatType::S8 ?
          CompType::UInt : description.format.compType;
      if(!access.resourceOnly && !TextureNumericFamily(access.numeric,type))
      {
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal runtime texture numeric ABI rejected: consumer=%llu source=%s type=%u numeric=%c read=%d write=%d\n",
              (unsigned long long)readOffset,ToStr(texture).c_str(),uint32_t(type),
              access.numeric?access.numeric:'?',int(access.read),int(access.write));
        return false;
      }
      uint64_t viewBegin=0,viewEnd=0;
      bool boundedView=false;
      uint64_t width=0;
      MTL::PixelFormat format=MTL::PixelFormatInvalid;
      if(futureView!=runtimeTextureViews.end() &&
         futureView->second.descriptor.textureType==MTL::TextureTypeTextureBuffer)
      {
        viewBegin=futureView->second.offset;width=futureView->second.descriptor.width;
        format=futureView->second.descriptor.pixelFormat;boundedView=true;
      }
      else
      {
        auto object=GetResourceManager()->GetResource(texture,true);
        auto native=object && object->m_Type==eResTexture?Unwrap((WrappedMTLTexture *)object):NULL;
        if(native && native->textureType()==MTL::TextureTypeTextureBuffer)
        {viewBegin=native->bufferOffset();width=native->width();format=native->pixelFormat();boundedView=true;}
      }
      const ResourceId backing=viewBacking(texture);
      const ResourceId target=backing!=ResourceId()?backing:texture;
      if(boundedView)
      {
        uint32_t bw=0,bh=0,pixelBytes=0;
        const uint64_t length=m_DescriptorFrameBuffers.count(target)?m_DescriptorFrameBuffers.at(target).length:
            GetReplay()->GetBuffer(target).length;
        if(!GetTextureDataBlockShape(format,bw,bh,pixelBytes) || bw!=1 || bh!=1 || !pixelBytes ||
           viewBegin>length || width>(length-viewBegin)/pixelBytes)return false;
        viewEnd=viewBegin+width*pixelBytes;
      }
      // Atomic load/RMW consume prior pixels just like ordinary reads; an
      // atomic store does not. The producer must belong to this submission or
      // an already committed submission, independently of optional display.
      if(access.read)
      {
        bool restored=HasReplayTextureInitialContents(texture);
        for(ResourceId source : {texture,backing})
        {
          const auto writer=typedTextureWriteCommands.find(source);
          if(writer!=typedTextureWriteCommands.end())
          {
            if(writer->second!=command && !committed.count(writer->second))
            {
              if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
                fprintf(stderr,"Metal texture submit dependency rejected: consumer=%llu texture=%s source=%s writer=%s command=%s\n",
                    (unsigned long long)readOffset,ToStr(texture).c_str(),ToStr(source).c_str(),
                    ToStr(writer->second).c_str(),ToStr(command).c_str());
              return false;
            }
            restored=true;
          }
        }
        if(backing!=ResourceId() && boundedView)
        {
          for(const auto &range:bufferRestoredRanges(backing))
            // A Native buffer-texture view can expose partially initialized
            // storage. Restore the actual upload/copy ranges; untouched texels
            // remain undefined, just as they did in the original API program.
            // The whole view is not a CPU assertion of pixels actually read by
            // conditional/dynamic shader invocations. Disjoint root uploads do
            // not qualify any pixels, and an absent pixel producer still fails.
            restored |= range.first<viewEnd && viewBegin<range.second;
        }
        else
        {
          if(dispatch.nativeCompute)
          {
            // Residency is permission, not a producer. Native argument-buffer
            // reads require restored initial pixels or an actual preceding
            // shader/clear operation, including their view's backing parent.
            restored |= HasReplayTextureInitialContents(backing);
            restored |= hasNativeTextureProducer(texture,command);
          }
          else restored |= modifiedBuffers.count(texture)!=0 || modifiedBuffers.count(backing)!=0;
        }
        if(!restored)
        {
          if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
            fprintf(stderr,"Metal runtime texture contents unavailable: resource=%s pixelRead=1\n",ToStr(texture).c_str());
          return false;
        }
      }
      if(access.write)
      {
        const auto write=indirectFootprint(target);
        for(const auto &read:scalarReads)
        {
          const auto source=indirectFootprint(read.buffer);
          const bool same=read.buffer==target;
          const bool alias=write.valid && source.valid && write.heap && write.heap==source.heap;
          const bool overlap=same?(!boundedView || (viewBegin<read.end && read.begin<viewEnd)):
              alias && (boundedView?(write.begin+viewBegin<source.begin+read.end &&
                  source.begin+read.begin<write.begin+viewEnd):
                  (write.begin<source.begin+read.end && source.begin+read.begin<write.end));
          if(overlap)
          {
            if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
              fprintf(stderr,"Metal runtime texture scalar writer overlap: view=%s buffer=%s input=%s range=%llu/%llu\n",
                  ToStr(texture).c_str(),ToStr(target).c_str(),ToStr(read.buffer).c_str(),
                  (unsigned long long)read.begin,(unsigned long long)read.end);
            return false;
          }
        }
        writes.insert(texture);
      }
    }
    // Retain only identified descriptors for access display. Unknown offsets
    // do not become fictitious per-invocation accesses or all-heap feedback.
    {
      const auto key=make_rdcpair(readOffset,dispatch.stage);
      // Typed raw tables do not require an MTLArgumentEncoder packet. Retain
      // the exact Native bindings only after identity, publication, address,
      // input state and submit dependencies above have all been restored.
      auto &restoredBindings=m_IRRuntimeRestoredBindings[key];
      restoredBindings.pipeline=dispatch.pipeline;restoredBindings.tables.clear();
      for(const auto &binding:dispatch.graphics.buffers)
        if(isTable(binding.second.resourceId))restoredBindings.tables[binding.first]=binding.second;
      m_IRRuntimeDescriptorAccessComplete[key]=restoration.completeAccessDisplay;
      auto &dependencies=m_IRRuntimeDescriptorAccesses[key];
      dependencies.clear();
      auto save=[&](Value value) {
        IRRuntimeDescriptorAccess access;
        access.pipeline=dispatch.pipeline;access.object=value.object;access.offset=value.offset;
        access.descriptorObject=value.descriptorObject;access.descriptorOffset=value.descriptorOffset;
        access.kind=uint32_t(value.kind);access.write=value.write;access.resourceOnly=value.resourceOnly;
        dependencies.push_back(access);
      };
      // Ordering references are restoration dependencies, not pixel accesses.
      for(const auto &value:report.accesses)if(!value.ordering)save(value);
      for(const auto &buffer:report.buffers)
      {
        if(buffer.address.namespaceStride)continue; // namespace candidates are not actual accesses
        Value value=buffer.address;value.kind=Value::Buffer;value.write=buffer.write;save(value);
      }
    }
    if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT") &&
       !restoration.completeAccessDisplay)
      fprintf(stderr,"Metal runtime restoration accepted: offset=%llu stage=%u accessDisplay=partial sourcedBuffers=%zu writers=%zu\n",
          (unsigned long long)readOffset,dispatch.stage,report.buffers.size(),writes.size());
    // A shader can write known allocations without publishing authoritative CPU
    // scalar bytes. Invalidate them in submission order, including physical aliases.
    for(ResourceId resource:writes)
    {
      modifiedBuffers.insert(resource);opaqueWrites.insert(resource);
      const ResourceId backing=viewBacking(resource);
      const ResourceId buffer=backing!=ResourceId()?backing:resource;
      modifiedBuffers.insert(buffer);opaqueWrites.insert(buffer);
      computeInvalidatedBuffers.insert(buffer);
      noteOpaqueScalarWrite(buffer,readOffset);
      computeCBVScalarKnownBytes.erase(buffer);
      const auto write=indirectFootprint(buffer);
      for(auto &known:computeCBVKnownBytes)
      {
        const auto alias=indirectFootprint(known.first);
        if(known.first==buffer || (write.valid && alias.valid && write.heap && write.heap==alias.heap &&
            alias.begin<write.end && write.begin<alias.end))
          memset(known.second.known.data(),0,known.second.known.size());
      }
      if(write.valid && write.heap)
      {
        std::set<ResourceId> candidates=m_DescriptorPreflightLiveBuffers;
        for(const auto &candidate:GetReplay()->GetBuffers())candidates.insert(candidate.resourceId);
        for(ResourceId candidate:candidates)
        {
          const auto alias=indirectFootprint(candidate);
          if(alias.valid && alias.heap==write.heap && alias.begin<write.end && write.begin<alias.end)
          {modifiedBuffers.insert(candidate);opaqueWrites.insert(candidate);computeInvalidatedBuffers.insert(candidate);computeCBVScalarKnownBytes.erase(candidate);}
        }
      }
      if(GetReplay()->GetTexture(resource).resourceId!=ResourceId() ||
         (m_DescriptorFrameTextures.count(resource) && m_DescriptorPreflightLiveTextures.count(resource)) ||
         (runtimeTextureViews.count(resource) && m_DescriptorPreflightLiveViews.count(resource)))
        typedTextureWriteCommands[resource]=command;
    }
    // A validated, executed Native store defines its byte interval even when
    // its computed value is unavailable to CPU access display. Publication is
    // in this consumer's submission order, never from a future GPU producer.
    // Conditional/ranged/atomic writes and raster coverage cannot establish a
    // new exact interval. Descriptor address fields need their separate typed
    // relocation/publication contract, not ordinary byte initialization.
    if(!dispatch.stage && dispatch.invocationExtent.size()==3 && dispatch.invocationExtent[0] &&
       dispatch.invocationExtent[1] && dispatch.invocationExtent[2])
      for(const auto &access:report.buffers)
      {
        if(!access.write || !access.definiteWrite)continue;
        ResourceId buffer;memcpy(&buffer,&access.address.object,sizeof(buffer));
        if(isTable(buffer))continue;
        if(!preflightBudget.Consume(access.bytes))return false;
        bufferRestoredRanges(buffer);
        addRestoredRange(buffer,access.address.offset,access.address.offset+access.bytes);
        resourceCommands[buffer].insert(command);
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal runtime GPU-defined range: resource=%s offset=%llu bytes=%llu command=%s scalarKnown=%d\n",
              ToStr(buffer).c_str(),(unsigned long long)access.address.offset,
              (unsigned long long)access.bytes,ToStr(command).c_str(),int(access.definiteStore));
      }
    // Preserve only exact unconditional integer stores, in instruction order.
    // A later unknown/ranged/atomic store clears any overlapping earlier fact.
    // Texture writes have a separate operation order, so mixed writers remain
    // conservative and establish no scalar facts here. Raster coverage can
    // execute zero fragment invocations, so this proof applies only to a
    // validated nonzero compute dispatch.
    bool textureWriter=false;
    for(const auto &access:report.accesses)textureWriter|=access.kind==Value::Texture && access.write;
    if(!textureWriter && !dispatch.stage && dispatch.invocationExtent.size()==3 &&
       dispatch.invocationExtent[0] && dispatch.invocationExtent[1] && dispatch.invocationExtent[2])
    for(const auto &access:report.buffers)
    {
      if(!access.write || !access.address.offsetKnown)continue;
      ResourceId buffer;memcpy(&buffer,&access.address.object,sizeof(buffer));
      const auto write=indirectFootprint(buffer);
      auto overlap=[&](ResourceId target,uint64_t &begin,uint64_t &end) {
        const auto alias=indirectFootprint(target);
        if(target==buffer){begin=access.address.offset;end=access.address.High()+access.bytes;return true;}
        if(!write.valid || !alias.valid || !write.heap || write.heap!=alias.heap)return false;
        const uint64_t low=write.begin+access.address.offset,high=write.begin+access.address.High()+access.bytes;
        if(low>=alias.end || high<=alias.begin)return false;
        begin=RDCMAX(low,alias.begin)-alias.begin;end=RDCMIN(high,alias.end)-alias.begin;return true;
      };
      for(auto &facts:computeCBVScalarKnownBytes)
      {uint64_t begin=0,end=0;if(overlap(facts.first,begin,end))
        facts.second.erase(facts.second.lower_bound(begin),facts.second.lower_bound(end));}
      for(auto &known:computeCBVKnownBytes)
      {uint64_t begin=0,end=0;if(overlap(known.first,begin,end))
        for(uint64_t i=begin;i<end && i<known.second.known.size();i++)known.second.known[i]=0;}
      if(!access.definiteStore || !access.bytes || access.bytes>8 || isTable(buffer))continue;
      const uint64_t sparseCost=access.bytes*64;
      if(sparseCost>64ULL*1024*1024-computeCBVProofBytes)return false;
      computeCBVProofBytes+=sparseCost;
      auto &facts=computeCBVScalarKnownBytes[buffer];
      for(uint64_t i=0;i<access.bytes;i++)facts[access.address.offset+i]={byte(access.stored.offset>>(i*8)),readOffset};
      if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal runtime exact scalar store: resource=%s offset=%llu bytes=%llu value=%llu\n",
            ToStr(buffer).c_str(),(unsigned long long)access.address.offset,
            (unsigned long long)access.bytes,(unsigned long long)access.stored.offset);
    }
    return true;
  };
  auto invalidateScalarRange=[&](ResourceId buffer,uint64_t offset,uint64_t bytes) {
    const auto write=indirectFootprint(buffer);
    auto overlap=[&](ResourceId target,uint64_t &begin,uint64_t &end) {
      if(target==buffer){begin=offset;end=offset+bytes;return true;}
      const auto alias=indirectFootprint(target);
      if(!write.valid || !alias.valid || !write.heap || write.heap!=alias.heap)return false;
      const uint64_t low=write.begin+offset,high=low+bytes;
      if(low>=alias.end || high<=alias.begin)return false;
      begin=RDCMAX(low,alias.begin)-alias.begin;end=RDCMIN(high,alias.end)-alias.begin;return true;
    };
    for(auto &facts:computeCBVScalarKnownBytes)
    {uint64_t begin=0,end=0;if(overlap(facts.first,begin,end))
      facts.second.erase(facts.second.lower_bound(begin),facts.second.lower_bound(end));}
    for(auto &known:computeCBVKnownBytes)
    {uint64_t begin=0,end=0;if(overlap(known.first,begin,end))
      for(uint64_t i=begin;i<end && i<known.second.known.size();i++)known.second.known[i]=0;}
    computeInvalidatedBuffers.insert(buffer);
    if(write.valid && write.heap)
    {
      std::set<ResourceId> candidates=m_DescriptorPreflightLiveBuffers;
      for(const auto &candidate:GetReplay()->GetBuffers())candidates.insert(candidate.resourceId);
      for(ResourceId candidate:candidates)
      {uint64_t begin=0,end=0;if(overlap(candidate,begin,end))computeInvalidatedBuffers.insert(candidate);}
    }
  };
  auto validateComputeCBVSubmission = [&](ResourceId command) {
    if(m_RayQueryHeapDispatches.empty() && m_IRComputeRuntimeABIs.empty() && runtimeDispatches.empty()) return true;
    auto &state=computeByteState;
    // Captured CPU snapshots are applied before their submission, as on the
    // existing index/descriptor upload path. Only proved bytes become scalar
    // inputs. Address values still need independent typed pointer provenance.
    for(const CPUUpdate &update:submissionUpdates[command])
    {
      // Native input restoration does not depend on optional scalar cache
      // availability. The original CPU upload is replayed in this submission.
      const uint64_t length=m_DescriptorFrameBuffers.count(update.buffer)?m_DescriptorFrameBuffers.at(update.buffer).length:
          GetReplay()->GetBuffer(update.buffer).length;
      if(update.start>length || update.data.size()>length-update.start)return false;
      bufferRestoredRanges(update.buffer);
      addRestoredRange(update.buffer,update.start,update.start+update.data.size());
      auto values=state(update.buffer);if(!values)continue;
      if(update.start>values->known.size() || update.data.size()>values->known.size()-update.start) return false;
      invalidateScalarRange(update.buffer,update.start,update.data.size());
      memset(values->known.data()+update.start,1,update.data.size());
      memcpy(values->data.data()+update.start,update.data.data(),update.data.size());
    }
    for(const auto &op:computeCBVOperations[command])
    {
      auto rejectOperation=[&](const char *reason) {
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal submission input rejected: command=%s resource=%s source=%s offset=%llu bytes=%llu consumer=%llu stage=%u read=%d known=%d reason=%s\n",
              ToStr(command).c_str(),ToStr(op.buffer).c_str(),ToStr(op.source).c_str(),
              (unsigned long long)op.offset,(unsigned long long)op.bytes,
              (unsigned long long)op.readOffset,op.stage,int(op.read),int(op.known),reason);
        return false;
      };
      if(op.buffer==ResourceId())
      {
        const bool runtime=runtimeDispatches.count(make_rdcpair(op.readOffset,op.stage))!=0;
        if(!(runtime?validateRuntimeDispatch(op.readOffset,command,op.stage):validateUniformDispatch(op.readOffset)))return rejectOperation("shader resource/address restoration");
        continue;
      }
      if(op.indexedInput || op.indirectInput)
      {
        const uint64_t length=m_DescriptorFrameBuffers.count(op.buffer)?m_DescriptorFrameBuffers.at(op.buffer).length:
            GetReplay()->GetBuffer(op.buffer).length;
        if(!op.bytes || op.offset>length || op.bytes>length-op.offset || isTable(op.buffer))
          return rejectOperation("invalid Native API input interval");
        bool restored=false;
        for(const auto &range:bufferRestoredRanges(op.buffer))
          restored |= range.first<=op.offset && op.offset+op.bytes<=range.second;
        if(!restored)return rejectOperation("missing Native API input state");
        if(op.indexedInput)restoredIndexInputs.insert(op.buffer);
        resourceCommands[op.buffer].insert(command);
        continue;
      }
      auto values=state(op.buffer);
      if(!values || op.offset>values->known.size() || op.bytes>values->known.size()-op.offset) return rejectOperation("state/range/budget");
      if(op.read)
      {
        for(uint64_t i=0;i<op.bytes;i++) if(!values->known[op.offset+i]) return rejectOperation("unknown CPU scalar byte");
        auto &data=m_IRComputeUniformReadContents[op.readOffset][make_rdcpair(op.buffer,op.offset)];
        const uint64_t common=RDCMIN(uint64_t(data.size()),op.bytes);
        if(common && memcmp(data.data(),values->data.data()+op.offset,common))return rejectOperation("inconsistent scalar snapshot");
        if(op.bytes>data.size())
        {
          const uint64_t extra=op.bytes-data.size();
          if(extra>64ULL*1024*1024-computeCBVProofBytes)return rejectOperation("scalar snapshot budget");
          computeCBVProofBytes+=extra;
          data.assign(values->data.data()+op.offset,op.bytes);
        }
      }
      else
      {
        if(op.source!=ResourceId())
        {
          // A source may be Private or frame-born. Only bytes established by
          // this submission's predecessors are propagated. Unknown bytes stay
          // unknown, even if the allocation had an older complete initial state.
          auto source=state(op.source);
          bytebuf copied,known;
          copied.resize(op.bytes);memset(copied.data(),0,copied.size());
          known.resize(op.bytes);memset(known.data(),0,known.size());
          if(source && op.sourceOffset<=source->data.size() &&
             op.bytes<=source->data.size()-op.sourceOffset)
          {
            memcpy(copied.data(),source->data.data()+op.sourceOffset,op.bytes);
            memcpy(known.data(),source->known.data()+op.sourceOffset,op.bytes);
          }
          else if(!opaqueWrites.count(op.source) && op.known && op.data.size()==op.bytes)
          {
            memcpy(copied.data(),op.data.data(),op.bytes);memset(known.data(),1,known.size());
          }
          // Snapshot before assigning: legal non-overlapping copies may use
          // one allocation as both source and destination.
          invalidateScalarRange(op.buffer,op.offset,op.bytes);
          memcpy(values->data.data()+op.offset,copied.data(),op.bytes);
          memcpy(values->known.data()+op.offset,known.data(),op.bytes);
          // Copy the restoration intervals independently of known scalar
          // values. Snapshot first for legal same-allocation copies.
          const auto ranges=bufferRestoredRanges(op.source);
          bufferRestoredRanges(op.buffer);
          // An uninitialized source must not inherit the destination's older
          // initialized pixels. Native copy replaces exactly this interval.
          eraseRestoredRange(op.buffer,op.offset,op.offset+op.bytes);
          for(const auto &range:ranges)
          {
            const uint64_t begin=RDCMAX(op.sourceOffset,range.first);
            const uint64_t end=RDCMIN(op.sourceOffset+op.bytes,range.second);
            if(begin<end)addRestoredRange(op.buffer,op.offset+begin-op.sourceOffset,
                                         op.offset+end-op.sourceOffset);
          }
          continue;
        }
        if(op.known && op.data.size()!=op.bytes)return rejectOperation("missing ordinary API payload");
        invalidateScalarRange(op.buffer,op.offset,op.bytes);
        memset(values->known.data()+op.offset,op.known?1:0,op.bytes);
        if(op.known)memcpy(values->data.data()+op.offset,op.data.data(),op.bytes);
        if(op.known)
        {bufferRestoredRanges(op.buffer);addRestoredRange(op.buffer,op.offset,op.offset+op.bytes);}
      }
    }
    return true;
  };
  auto invalidateAliasedBufferContents=[&](ResourceId written) {
    if(m_DescriptorCoverage<65)return;
    const auto write=indirectFootprint(written);
    if(!write.valid || !write.heap)return;
    std::set<ResourceId> candidates=m_DescriptorPreflightLiveBuffers;
    for(const auto &buffer:GetReplay()->GetBuffers())candidates.insert(buffer.resourceId);
    for(ResourceId candidate:candidates)
    {
      if(candidate==written || m_DescriptorPreflightAliasedBuffers.count(candidate))continue;
      const auto alias=indirectFootprint(candidate);
      if(alias.valid && alias.buffer && alias.heap==write.heap &&
         alias.begin<write.end && write.begin<alias.end)
      {
        // Tracked heaps order real GPU writes across aliases. CPU scalar proofs
        // belong to logical buffers, so an alias writer invalidates their old
        // bytes until a producer supplies a new, authoritative range snapshot.
        modifiedBuffers.insert(candidate);opaqueWrites.insert(candidate);
        invalidateNullBufferFields(candidate);
        noteOpaqueScalarWrite(candidate,opaqueScalarWriteVersions[written]);
        if(getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
          fprintf(stderr,"Metal buffer alias writer: source=%s affected=%s\n",
              ToStr(written).c_str(),ToStr(candidate).c_str());
      }
    }
  };
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
    const bool heapQuery=m_DescriptorCoverage==65 &&
        (!m_RayQueryHeapDispatches.empty() || !m_RayASHeaders.empty() || !m_RayASHeaderFrameWrites.empty());
    if(heapQuery && chunk==MetalChunk::MTLBuffer_DeclareRayASHeader)
    {
      RayASHeader h;
      scan.Serialise("buffer"_lit,h.buffer); scan.Serialise("offset"_lit,h.offset);
      scan.Serialise("structure"_lit,h.structure); scan.Serialise("contributions"_lit,h.contributions);
      scan.Serialise("contributionOffset"_lit,h.contributionOffset); scan.Serialise("bytes"_lit,h.bytes);
      // Publishing the AS/contribution pointers does not read the pointed-to
      // allocation, just like a DX12 root SRV or Vulkan buffer descriptor.
      // Its actual consumers separately validate current bytes and submission
      // dependencies. The CPU header itself must still be free of GPU writers.
      success = !modifiedBuffers.count(h.buffer) && !opaqueWrites.count(h.buffer);
      for(ResourceId reader:resourceCommands[h.buffer]) success &= committed.count(reader)!=0;
      if(success) success=ApplyRayASHeaderWrite(h);
      if(!success && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal AS header publication proof: buffer=%s offset=%llu structure=%s contributions=%s contributionOffset=%llu headerModified=%d headerOpaque=%d contributionModified=%d contributionOpaque=%d contract=%d cursor=%zu count=%zu raw=%zu\n",
            ToStr(h.buffer).c_str(),(unsigned long long)h.offset,ToStr(h.structure).c_str(),
            ToStr(h.contributions).c_str(),(unsigned long long)h.contributionOffset,
            int(modifiedBuffers.count(h.buffer)),int(opaqueWrites.count(h.buffer)),
            int(modifiedBuffers.count(h.contributions)),int(opaqueWrites.count(h.contributions)),
            ValidateRayASHeader(h),m_RayASHeaderFrameCursor,m_RayASHeaderFrameWrites.size(),
            m_DescriptorRawContents.count(h.buffer)?m_DescriptorRawContents.at(h.buffer).size():0);
    }
    else if(heapQuery && chunk==MetalChunk::MTLCommandBuffer_accelerationStructureCommandEncoder)
    {
      ResourceId command,encoder;
      scan.Serialise("CommandBuffer"_lit,command);scan.Serialise("Encoder"_lit,encoder);
      success=encoder!=ResourceId() && validEncoderCommand(command) &&
          knownEncoders.insert(encoder).second && liveAS.insert(encoder).second;
      if(success) encoderCommands[encoder]=command;
      m_DescriptorPartialCopySubmissions[command].valid=false;
    }
    else if(heapQuery && (chunk==MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstances ||
                         chunk==MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstancesWithScratchOffset ||
                         chunk==MetalChunk::MTLAccelerationStructureCommandEncoder_buildFrozenTriangles ||
                         chunk==MetalChunk::MTLAccelerationStructureCommandEncoder_buildFrozenMultiIndexed))
    {
      ResourceId encoder,structure,source,indices,scratch;
      uint64_t scratchOffset=0;uint32_t kind=0;
      rdcarray<ResourceId> children;rdcarray<uint64_t> parameters;bytebuf bytes,indexBytes;
      scan.Serialise("Encoder"_lit,encoder);scan.Serialise("structure"_lit,structure);
      const bool instance=chunk==MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstances ||
                          chunk==MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstancesWithScratchOffset;
      if(instance)
      {
        scan.Serialise("children"_lit,children);scan.Serialise("instances"_lit,source);
        scan.Serialise("scratch"_lit,scratch);scan.Serialise("parameters"_lit,parameters);
        scan.Serialise("descriptorBytes"_lit,bytes);
        if(chunk==MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstancesWithScratchOffset)
          scan.Serialise("scratchOffset"_lit,scratchOffset);
      }
      else
      {
        scan.Serialise("vertices"_lit,source);scan.Serialise("indices"_lit,indices);
        scan.Serialise("kind"_lit,kind);scan.Serialise("parameters"_lit,parameters);
        scan.Serialise("scratch"_lit,scratch);scan.Serialise("scratchOffset"_lit,scratchOffset);
        scan.Serialise("vertexBytes"_lit,bytes);scan.Serialise("indexBytes"_lit,indexBytes);
      }
      success=liveAS.count(encoder) && ++rayBuildCount<=128 &&
          !isTable(source) && !isTable(indices) && !isTable(scratch) &&
          !m_DescriptorPreflightAliasedBuffers.count(source) && !m_DescriptorPreflightAliasedBuffers.count(scratch);
      const ResourceId command=encoderCommands[encoder];
      for(ResourceId reader:resourceCommands[structure]) success &= reader==command || committed.count(reader)!=0;
      for(ResourceId child:children)
        if(rayASBuildCommands.count(child))
          success &= rayASBuildCommands[child]==command || committed.count(rayASBuildCommands[child])!=0;
      for(const auto &q:m_RayQueryHeapDispatches)
        success &= q.output!=source && q.output!=indices && q.output!=scratch;
      for(const auto &h:m_RayASHeaderFrameWrites)
        success &= h.buffer!=source && h.buffer!=indices && h.buffer!=scratch &&
                   h.contributions!=source && h.contributions!=indices && h.contributions!=scratch;
      for(const auto &h:m_RayASHeaders)
        success &= h.second.buffer!=source && h.second.buffer!=indices && h.second.buffer!=scratch &&
                   h.second.contributions!=source && h.second.contributions!=indices && h.second.contributions!=scratch;
      if(success)
        success=instance ? RecordRayIRIndirectASBuild(structure,children,source,scratch,parameters,bytes,scratchOffset) :
            ((chunk==MetalChunk::MTLAccelerationStructureCommandEncoder_buildFrozenMultiIndexed)==(kind==8) &&
             RecordRayIRGeometryASBuild(structure,source,indices,kind,parameters,scratch,scratchOffset,bytes,indexBytes));
      if(success)
      {
        rayASBuildCommands[structure]=command;
        for(ResourceId id:{structure,source,indices,scratch}) noteResource(id,encoder);
        for(ResourceId id:children) noteResource(id,encoder);
        modifiedBuffers.insert(scratch);opaqueWrites.insert(scratch);noteOpaqueScalarWrite(scratch,chunkOffset);pendingWork.insert(command);
      }
    }
    else if(heapQuery && DescriptorChunkStartsWith(GetChunkName((uint32_t)chunk),"MTLAccelerationStructureCommandEncoder::"))
    {
      ResourceId encoder;scan.Serialise("Encoder"_lit,encoder);
      success=liveAS.count(encoder)!=0;
      if(chunk==MetalChunk::MTLAccelerationStructureCommandEncoder_endEncoding)
      {
        success &= asDebugDepth[encoder]==0;
        liveAS.erase(encoder);encoderCommands.erase(encoder);
      }
      else if(chunk==MetalChunk::MTLAccelerationStructureCommandEncoder_pushDebugGroup) ++asDebugDepth[encoder];
      else if(chunk==MetalChunk::MTLAccelerationStructureCommandEncoder_popDebugGroup)
      {
        success &= asDebugDepth[encoder]>0;if(success)--asDebugDepth[encoder];
      }
      else success &= chunk==MetalChunk::MTLAccelerationStructureCommandEncoder_insertDebugSignpost;
    }
    else if(heapQuery && chunk==MetalChunk::MTLBlitCommandEncoder_fillBuffer)
    {
      ResourceId encoder,destination;NS::Range range=NS::Range::Make(0,0);uint8_t value=0;
      scan.Serialise("BlitCommandEncoder"_lit,encoder);scan.Serialise("buffer"_lit,destination);
      scan.Serialise("range"_lit,range);scan.Serialise("value"_lit,value);
      auto object=GetResourceManager()->GetResource(destination,true);
      auto native=object && object->m_Type==eResBuffer ? Unwrap((WrappedMTLBuffer *)object):NULL;
      success=liveBlits.count(encoder) && native && native->storageMode()==MTL::StorageModePrivate &&
          !object->m_CapturedAliasable && !isTable(destination) && value==0 &&
          range.location<=native->length() && range.length<=native->length()-range.location;
      for(const auto &q:m_RayQueryHeapDispatches) success &= q.output!=destination;
      for(const auto &h:m_RayASHeaders) success &= h.second.buffer!=destination && h.second.contributions!=destination;
      for(const auto &h:m_RayASHeaderFrameWrites) success &= h.buffer!=destination && h.contributions!=destination;
      if(success)
      {
        noteResource(destination,encoder);modifiedBuffers.insert(destination);opaqueWrites.insert(destination);noteOpaqueScalarWrite(destination,chunkOffset);
        invalidateAliasedBufferContents(destination);
        pendingWork.insert(encoderCommands[encoder]);m_DescriptorPartialCopySubmissions[encoderCommands[encoder]].valid=false;
      }
    }
    else if(m_DescriptorCoverage >= 14 && chunk == MetalChunk::MTLBuffer_InternalModifyCPUContents)
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
      auto header=m_DescriptorRawContents.find(buffer);
      if(header!=m_DescriptorRawContents.end() && !m_RayQueryHeapDispatches.empty())
        success &= start<=header->second.size() && size<=header->second.size()-start &&
            memcmp(header->second.data()+start,data.data(),size)==0;
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
      ++commandCount;
      success = queue != ResourceId() && command != ResourceId() &&
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
          m_DescriptorPreflightLiveBuffers.insert(buffer).second &&
          (chunk==MetalChunk::MTLDevice_newBufferWithBytes?initial.size()==length:initial.empty());
      if(success)
      {
        // MTLDevice newBuffer(length) guarantees zero bytes; newBuffer(bytes)
        // carries its complete API input. Native recreation repeats that exact
        // birth state on every seek. This is not a heap/undefined-content policy,
        // and not a substitute for a background resource's captured frame initial.
        addRestoredRange(buffer,0,length);
      }
    }
    else if(chunk==MetalChunk::MTLBuffer_CaptureHeapBirthContents ||
         chunk==MetalChunk::MTLBuffer_CaptureHeapBirthUnspecified)
    {
      ResourceId buffer,heap;uint64_t offset=0;bytebuf data;
      scan.Serialise("buffer"_lit,buffer);scan.Serialise("heap"_lit,heap);
      scan.Serialise("offset"_lit,offset);scan.Serialise("data"_lit,data);
      const bool apiUnspecified=chunk==MetalChunk::MTLBuffer_CaptureHeapBirthUnspecified;
      auto birth=m_DescriptorFrameBuffers.find(buffer);
      success=birth!=m_DescriptorFrameBuffers.end() && birth->second.heap==heap &&
          birth->second.offset==offset && (apiUnspecified?data.empty():(birth->second.length==data.size() && !data.empty())) &&
          data.size()<=16ULL*1024*1024 && m_DescriptorPreflightLiveBuffers.count(buffer) &&
          !m_DescriptorPreflightAliasedBuffers.count(buffer) && !m_ReplayHeapBufferBirthContents.count(buffer);
      if(success)
      {
        const auto footprint=indirectFootprint(buffer);
        success=footprint.valid && footprint.buffer && footprint.heap;
        // The recorded bytes are this object's creation input at this point in
        // the original stream, not an initial for preceding heap consumers.
        // Reject an observation moved beyond an already encoded object use;
        // completed unrelated prefix work does not invalidate a later birth.
        if(!apiUnspecified)success &= resourceCommands[buffer].empty();
        if(success)
        {
          // Fresh backing must not overlap any earlier captured object. The
          // capture-side lifetime history also includes retired objects that
          // no longer occur in this frame's resource records.
          if(apiUnspecified)
          {
            auto parent=GetResourceManager()->GetResource(heap,true);
            success=parent && parent->m_Type==eResHeap && parent->m_Real &&
                !((WrappedMTLHeap *)parent)->HasOtherPlacementOverlap(footprint.begin,footprint.end,buffer);
            std::set<ResourceId> earlierObjects=m_DescriptorPreflightLiveBuffers;
            earlierObjects.insert(m_DescriptorPreflightLiveTextures.begin(),m_DescriptorPreflightLiveTextures.end());
            for(ResourceId other:earlierObjects)
            {
              if(other==buffer)continue;
              const auto earlier=indirectFootprint(other);
              if(earlier.valid && earlier.heap==footprint.heap && earlier.begin<footprint.end &&
                 footprint.begin<earlier.end){success=false;break;}
            }
          }
          if(success)
          {
            m_ReplayHeapBufferBirthContents[buffer]=data;
            if(apiUnspecified)m_ReplayHeapBufferBirthUnspecified.insert(buffer);
            else addRestoredRange(buffer,0,data.size());
          }
        }
      }
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
          if(backgroundOverlap && length>DescriptorPlacementBufferAliasLimit() &&
             getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
            fprintf(stderr,"Metal placement alias budget: heap=%s buffer=%s length=%llu offset=%llu footprint=%llu limit=%llu\n",
                ToStr(heap).c_str(),ToStr(buffer).c_str(),(unsigned long long)length,
                (unsigned long long)offset,(unsigned long long)layout.size,
                (unsigned long long)DescriptorPlacementBufferAliasLimit());
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
              length <= DescriptorPlacementBufferAliasLimit() &&
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
                  length <= DescriptorPlacementBufferAliasLimit() &&
                  previous->second.length <= DescriptorPlacementBufferAliasLimit() &&
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
                implicit = length <= DescriptorPlacementBufferAliasLimit() &&
                    range.end - range.begin <= DescriptorPlacementAliasLimit() &&
                    validateRetiredTextureAlias(range.buffer, buffer);
              if(!implicit) success = false;
            }
          if(success) heapRanges[heap].push_back({offset, end, buffer});
        }
      }
      if(!success && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal frame placement buffer: heap=%s buffer=%s length=%llu options=%llu offset=%llu future=%d parent=%d\n",
            ToStr(heap).c_str(),ToStr(buffer).c_str(),(unsigned long long)length,
            (unsigned long long)options,(unsigned long long)offset,
            future!=m_DescriptorFrameBuffers.end(),object && object->m_Type==eResHeap && object->m_Real);
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
        if(success)runtimeTextureViews[texture]={descriptor,offset,bytesPerRow};
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
        (chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset ||
        chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat_subset_swizzle ||
        chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat))
    {
      ResourceId source, view; MTL::PixelFormat format; MTL::TextureType type;
      NS::Range levels(0, 0), slices(0, 0); MTL::TextureSwizzleChannels swizzle;
      scan.Serialise("Source"_lit, source); scan.Serialise("View"_lit, view);
      scan.Serialise("format"_lit, format); scan.Serialise("type"_lit, type);
      scan.Serialise("levels"_lit, levels); scan.Serialise("slices"_lit, slices);
      scan.Serialise("swizzle"_lit, swizzle);
      auto recorded = m_DescriptorFrameTextureViewParents.find(view);
      auto parent = m_DescriptorFrameTextures.find(source);
      if(chunk == MetalChunk::MTLTexture_newTextureViewWithPixelFormat &&
         parent != m_DescriptorFrameTextures.end())
      {
        const auto &d = parent->second.descriptor;
        type = d.textureType;
        levels = NS::Range(0, d.mipmapLevelCount);
        slices = NS::Range(0, type == MTL::TextureTypeCube || type == MTL::TextureTypeCubeArray ?
            6 * d.arrayLength : type == MTL::TextureType2DArray ? d.arrayLength : 1);
      }
      auto recordedLevel = m_DescriptorFrameTextureViewMipLevels.find(view);
      auto recordedTexture = m_DescriptorFrameTextures.find(view);
      RDMTL::TextureDescriptor projected;
      success = recorded != m_DescriptorFrameTextureViewParents.end() && recorded->second == source &&
          parent != m_DescriptorFrameTextures.end() && !m_DescriptorFrameTextureViewParents.count(source) &&
          m_DescriptorPreflightLiveTextures.count(source) && !GetResourceManager()->HasResource(view) &&
          ProjectDescriptorFrameTextureView(parent->second.descriptor, format, type, levels,
                                            slices, swizzle, m_DescriptorCoverage, projected) &&
          recordedLevel != m_DescriptorFrameTextureViewMipLevels.end() && recordedLevel->second == levels.location &&
          recordedTexture != m_DescriptorFrameTextures.end() &&
          recordedTexture->second.descriptor.width == projected.width &&
          recordedTexture->second.descriptor.height == projected.height &&
          recordedTexture->second.descriptor.mipmapLevelCount == projected.mipmapLevelCount &&
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
      // The no-argument entry creates a serial encoder and records only IDs.
      // Read dispatchType only from the two APIs that actually serialise it.
      if(m_DescriptorCoverage >= 54 && chunk != MetalChunk::MTLCommandBuffer_computeCommandEncoder)
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
        computes[encoder].inlineData.erase((uint32_t)index);
        computes[encoder].pointers.erase((uint32_t)index);
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
        if(success)resourceCommands[heap].insert(encoderCommands[encoder]);
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
        if(compute)for(ResourceId resource:resources)computes[encoder].residency[resource] |= usage;
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
          if(compute && (HasRayQueryHeapPipeline(computes[encoder].pipeline) ||
              m_IRComputeRuntimeABIs.count(computes[encoder].pipeline)))
            typedQueryWriteResidency[encoder].insert(resources.begin(),resources.end());
          else
          {
            modifiedBuffers.insert(resources.begin(), resources.end());
            opaqueWrites.insert(resources.begin(), resources.end());
            for(ResourceId resource:resources)noteOpaqueScalarWrite(resource,chunkOffset);
          }
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
      // Both background and frame-born attachments use the same public API
      // layout. Initial state and original producers are separate from this
      // geometry; a format/type/size combination is not a scenario licence.
      auto attachmentLayout = [&](const RDMTL::RenderPassAttachmentDescriptor &attachment,
                                  RDMTL::TextureDescriptor &d) {
        const auto future=m_DescriptorFrameTextures.find(attachment.textureId);
        MTL::Texture *texture=Unwrap(attachment.texture);
        if(future!=m_DescriptorFrameTextures.end())
        {
          if(!m_DescriptorPreflightLiveTextures.count(attachment.textureId))return false;
          d=future->second.descriptor;
          if(!ValidDescriptorFrameTexture(d,m_DescriptorCoverage))return false;
        }
        else
        {
          if(!texture || !attachment.texture || attachment.texture->m_Device!=this)return false;
          d.textureType=texture->textureType();d.pixelFormat=texture->pixelFormat();
          d.width=texture->width();d.height=texture->height();d.depth=texture->depth();
          d.arrayLength=texture->arrayLength();d.mipmapLevelCount=texture->mipmapLevelCount();
          d.sampleCount=texture->sampleCount();d.storageMode=texture->storageMode();d.usage=texture->usage();
        }
        uint64_t bytes=0;
        if(!MetalTextureReplayLayout(d,bytes) ||
           (d.usage && !(d.usage & MTL::TextureUsageRenderTarget)) ||
           attachment.level>=d.mipmapLevelCount)return false;
        const uint64_t width=RDCMAX(1ULL,uint64_t(d.width)>>attachment.level);
        const uint64_t height=RDCMAX(1ULL,uint64_t(d.height)>>attachment.level);
        // Zero array length disables layered rendering. Other attachments must
        // each contain the active range, including the selected mip/plane.
        const uint64_t layers=RDCMAX(1ULL,uint64_t(pass.renderTargetArrayLength));
        uint64_t available=d.textureType==MTL::TextureType2DArray?d.arrayLength:
            d.textureType==MTL::TextureTypeCube || d.textureType==MTL::TextureTypeCubeArray?6*d.arrayLength:1;
        uint64_t first=attachment.slice;
        if(d.textureType==MTL::TextureType3D)
        {
          if(attachment.slice)return false;
          available=RDCMAX(1ULL,uint64_t(d.depth)>>attachment.level);first=attachment.depthPlane;
        }
        return pass.renderTargetWidth<=width && pass.renderTargetHeight<=height &&
            first<available && layers<=available-first;
      };
      for(uint32_t index = 0; index < pass.colorAttachments.size(); index++)
      {
        const auto &attachment = pass.colorAttachments[index];
        if(attachment.textureId == ResourceId()) continue;
        const auto future=m_DescriptorFrameTextures.find(attachment.textureId);
        MTL::Texture *texture=Unwrap(attachment.texture);
        RDMTL::TextureDescriptor layout;
        const bool geometry=attachmentLayout(attachment,layout);
        const MTL::PixelFormat format=layout.pixelFormat;
        uint32_t blockWidth=0,blockHeight=0,blockBytes=0;
        const ResourceFormat colorFormat=MakeResourceFormat(format);
        const bool colorLayout=colorFormat.compType!=CompType::Depth &&
            GetTextureDataBlockShape(format,blockWidth,blockHeight,blockBytes) &&
            blockWidth==1 && blockHeight==1 && blockBytes && blockBytes<=16;
        const bool nativeLoad=attachment.loadAction==MTL::LoadActionClear ||
            attachment.loadAction==MTL::LoadActionLoad || attachment.loadAction==MTL::LoadActionDontCare;
        bool priorFrameWrite=false;
        for(ResourceId writer:colorWriteCommands[attachment.textureId])
          priorFrameWrite |= writer==command || committed.count(writer)!=0;
        const bool currentDrawableTarget=m_DescriptorDrawableTextures.count(attachment.textureId) &&
            ValidDescriptorDrawableTexture(texture) &&
            (!pass.renderTargetWidth || pass.renderTargetWidth==texture->width()) &&
            (!pass.renderTargetHeight || pass.renderTargetHeight==texture->height()) &&
            (attachment.loadAction==MTL::LoadActionClear ||
             (priorFrameWrite && attachment.loadAction==MTL::LoadActionLoad));
        if(m_DescriptorDrawableTextures.count(attachment.textureId) &&
           !HasReplayTextureInitialContents(attachment.textureId))success &= currentDrawableTarget;
        // Preserve Load/DontCare and frame-created ordinary undefined pixels.
        // Background Load still needs the real saved input or preceding work;
        // no new clear or CPU scalar proof is manufactured by the geometry.
        const bool restoredLoad=future!=m_DescriptorFrameTextures.end() ||
            HasReplayTextureInitialContents(attachment.textureId) || priorFrameWrite || currentDrawableTarget;
        success &= geometry && colorLayout && nativeLoad &&
            (attachment.loadAction!=MTL::LoadActionLoad || restoredLoad) &&
            targetResources.insert(attachment.textureId).second &&
            attachment.resolveTextureId==ResourceId() &&
            attachment.storeActionOptions==MTL::StoreActionOptionNone &&
            (attachment.storeAction==MTL::StoreActionStore || attachment.storeAction==MTL::StoreActionUnknown);
        if(attachment.storeAction==MTL::StoreActionUnknown)deferredStores[encoder].insert(index);
        renderTargets[encoder][index]=format;
        if(attachment.loadAction==MTL::LoadActionClear)
        {
          colorWriteCommands[attachment.textureId].insert(command);
          // Keep the existing whole-image clear producer fact honest. A partial
          // mip/layer clear is an original command, not a whole-image input.
          const uint64_t layers=RDCMAX(1ULL,uint64_t(pass.renderTargetArrayLength));
          const uint64_t firstLayer=layout.textureType==MTL::TextureType3D?
              attachment.depthPlane:attachment.slice;
          if(geometry)
            for(uint64_t layer=0;layer<layers;layer++)
              initializedColorCommands[{attachment.textureId,attachment.level,firstLayer+layer}]=command;
          const uint64_t allLayers=layout.textureType==MTL::TextureType3D?layout.depth:
              layout.textureType==MTL::TextureType2DArray?layout.arrayLength:
              layout.textureType==MTL::TextureTypeCube || layout.textureType==MTL::TextureTypeCubeArray?6*layout.arrayLength:1;
          if(geometry && layout.mipmapLevelCount==1 && !attachment.level && !attachment.slice &&
             !attachment.depthPlane && layers==allLayers)
            nativeTextureClearCommands[attachment.textureId].insert(command);
        }
        targets++;
      }
      if(m_DescriptorCoverage >= 53)
      {
        auto validateDepth = [&](const RDMTL::RenderPassAttachmentDescriptor &attachment, bool stencil) {
          MTL::PixelFormat format = MTL::PixelFormatInvalid;
          if(attachment.textureId == ResourceId()) return format;
          const auto future=m_DescriptorFrameTextures.find(attachment.textureId);
          RDMTL::TextureDescriptor layoutDescriptor;
          const bool geometry=attachmentLayout(attachment,layoutDescriptor);
          success &= geometry;
          format=layoutDescriptor.pixelFormat;
          const auto initialized = initializedDepthCommands.find({attachment.textureId, stencil, attachment.level, attachment.slice});
          const bool priorClear = initialized != initializedDepthCommands.end() &&
              (initialized->second == command || committed.count(initialized->second));
          const bool frameNativeLoad=m_DescriptorCoverage>=65 &&
              future!=m_DescriptorFrameTextures.end() &&
              (attachment.loadAction==MTL::LoadActionLoad || attachment.loadAction==MTL::LoadActionDontCare);
          success &= (stencil ? format == MTL::PixelFormatDepth32Float_Stencil8 :
              (format == MTL::PixelFormatDepth16Unorm || format == MTL::PixelFormatDepth32Float ||
               format == MTL::PixelFormatDepth32Float_Stencil8)) &&
              attachment.resolveTextureId == ResourceId() && !attachment.depthPlane && (attachment.storeAction == MTL::StoreActionStore ||
               (m_DescriptorCoverage >= 55 && attachment.storeAction == MTL::StoreActionUnknown)) &&
              attachment.storeActionOptions == MTL::StoreActionOptionNone &&
              (frameNativeLoad || attachment.loadAction == MTL::LoadActionClear ||
               (attachment.loadAction == MTL::LoadActionLoad &&
                (HasReplayTextureInitialContents(attachment.textureId) || priorClear)));
          if(attachment.storeAction == MTL::StoreActionUnknown)
            deferredStores[encoder].insert(stencil ? 9U : 8U);
          if(geometry && attachment.loadAction == MTL::LoadActionClear)
            for(uint64_t layer=0;layer<RDCMAX(1ULL,uint64_t(pass.renderTargetArrayLength));layer++)
              initializedDepthCommands[{attachment.textureId,stencil,attachment.level,attachment.slice+layer}]=command;
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
      // With no attachment, explicit raster dimensions replace attachment
      // extents. They are Native API state, not a 512-square scene permission.
      const bool validAttachmentless=m_DescriptorCoverage>=59 && renderAttachmentless[encoder] &&
          pass.renderTargetWidth && pass.renderTargetWidth<=16384 &&
          pass.renderTargetHeight && pass.renderTargetHeight<=16384 &&
          pass.defaultRasterSampleCount==1 && pass.renderTargetArrayLength==1;
      success &= (targets >= 1 || validAttachmentless || (m_DescriptorCoverage >= 56 &&
          renderDepthTargets[encoder] != MTL::PixelFormatInvalid)) &&
          targets <= (m_DescriptorCoverage >= 65 ? 8U : m_DescriptorCoverage >= 20 ? 5U : 2U);
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
        noteResource(buffer,encoder);modifiedBuffers.insert(buffer);opaqueWrites.insert(buffer);noteOpaqueScalarWrite(buffer,chunkOffset);
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
      success &= GetReplay()->ValidateGraphicsTargets(pipeline, renderTargets[encoder], m_DescriptorCoverage >= 65 ? 8U : 5U,
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
        snapshot.inlineData.erase((uint32_t)index);snapshot.pointers.erase((uint32_t)index);
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
      const auto &layout=m_DescriptorInlineShadow[make_rdcpair(encoder, uint64_t(stage) << 32 | index)];
      std::map<uint64_t,DescriptorSource> pointers;
      for(const auto &source:layout.sources)pointers[source.first*layout.stride]=source.second;
      const auto original=data;
      for(const auto &source : pointers)
        noteResource(source.second.resource, encoder);
      success = liveRenders.count(encoder) && RelocateDescriptorInlineShadow(encoder, stage, index, data);
      if(success)
      {
        graphics[{encoder, stage}].bytes[(uint32_t)index] = data.size();
        graphics[{encoder, stage}].inlineData[(uint32_t)index]=original;
        graphics[{encoder, stage}].pointers[(uint32_t)index]=pointers;
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
            indirectValid && vertexCount<=65536 &&
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
            (m_DescriptorCoverage>=65 || (!opaqueWrites.count(indexBuffer) &&
             !m_DescriptorGPUWrittenBuffers.count(indexBuffer))) &&
            (m_DescriptorCoverage>=65 || (frameIndex && m_DescriptorCoverage >= 51) || contents != indexContents.end()) &&
            indexOffset % stride == 0 && indexOffset <= indexLength &&
            vertexCount <= (indexLength - indexOffset) / stride;
        for(const auto &table : m_DescriptorTables) success &= table.buffer != indexBuffer;
        if(m_DescriptorCoverage>=65 && success) {
          ComputeCBVOperation input;input.buffer=indexBuffer;input.offset=indexOffset;
          input.bytes=vertexCount*stride;input.readOffset=chunkOffset;input.indexedInput=true;
          computeCBVOperations[encoderCommands[encoder]].push_back(input);
          noteResource(indexBuffer,encoder);
        }
        else if(frameIndex && m_DescriptorCoverage >= 51 && success) {
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
        if(success && m_DescriptorCoverage<65 && !(frameIndex && m_DescriptorCoverage >= 51))
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
      success &= GetReplay()->ValidateGraphicsTargets(graphics[{encoder, 1}].pipeline, renderTargets[encoder], m_DescriptorCoverage >= 65 ? 8U : m_DescriptorCoverage >= 20 ? 5U : 2U,
          renderDepthTargets.count(encoder) ? renderDepthTargets[encoder] : MTL::PixelFormatInvalid,
          renderStencilTargets.count(encoder) ? renderStencilTargets[encoder] : MTL::PixelFormatInvalid,
          m_DescriptorCoverage>=59 && renderAttachmentless[encoder]);
      for(uint32_t stage = 1; stage <= 2; stage++)
      {
        const auto &snapshot = graphics[{encoder, stage}];
        success &= GetReplay()->ValidateGraphicsBufferSnapshot(snapshot.pipeline, stage, snapshot.buffers, snapshot.bytes,
            m_DescriptorCoverage >= 65 || (m_DescriptorCoverage >= 56 && renderTargets[encoder].empty() &&
            renderDepthTargets[encoder] != MTL::PixelFormatInvalid));
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
      bool boundASHeap=false;
      if(success && m_DescriptorCoverage>=65 && vertexCount && instanceCount)
        for(const auto &slot:m_DescriptorSlotShadow)
          boundASHeap |= slot.second.live && slot.second.sources.count(3) &&
              encoderResources[encoder].count(slot.first.first);
      if(boundASHeap)
      {
        for(uint32_t stage=1;success && stage<=2;stage++)
        {
          const auto &snapshot=graphics[{encoder,stage}];
          rdcstr entry,air;std::map<rdcstr,rdcpair<rdcstr,rdcstr>> linked;
          success=GetReplay()->GetGraphicsAIR(snapshot.pipeline,stage,entry,air,linked);
          if(!success || entry.empty())continue;
          RuntimeDispatch draw;draw.pipeline=snapshot.pipeline;draw.stage=stage;draw.graphics=snapshot;
          draw.nullBufferFields=nullBufferFields;draw.nullGPUOwners=nullGPUOwners;
          draw.opaque=opaqueWrites;draw.indexBuffer=indexed?indexBuffer:ResourceId();
          draw.indexOffset=indexOffset;draw.indexCount=vertexCount;draw.indexStride=indexType==MTL::IndexTypeUInt16?2:4;
          draw.baseVertex=baseVertex;draw.vertexStart=vertexStart;draw.instanceCount=instanceCount;draw.baseInstance=baseInstance;
          for(const auto &slot:m_DescriptorSlotShadow)
            if(slot.second.live && !slot.second.data.empty())draw.slots.insert(slot);
          draw.headers=m_RayASHeaderCurrent;draw.structures=m_RayIRASCurrentContents;
          draw.structureOwners=rayASBuildCommands;
          runtimeDispatches[make_rdcpair(chunkOffset,stage)]=draw;
          ComputeCBVOperation proof={ResourceId(),0,0,false,false,chunkOffset,{}};proof.stage=stage;
          computeCBVOperations[encoderCommands[encoder]].push_back(proof);
        }
        if(success)graphicsConsumerEncoders.insert(encoder);
      }
      // The captured per-use indirect arguments (or direct API arguments) are
      // authoritative here. An empty draw executes no shader and consumes no AS
      // build closure, just as an empty compute grid has no dispatch consumers.
      // Slot relocation/publication and indirect-input validation above remain.
      if(success && vertexCount && instanceCount) noteSubmissionSlots(encoder);
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
      if(success && (event==2 || event==3))
      {
        uint64_t word=0;if(data.size()!=24)success=false;else memcpy(&word,data.data(),8);
        const auto &published=m_DescriptorSlotShadow.at(key);
        setNullBufferField(buffer,offset,!word && (!published.gpuExpected ||
            published.initialRestored || published.gpuCopyPublished));
        nullGPUOwners.erase(key);
        const auto owner=pendingNullGPUOwners.find(key);
        if(event==3 && !word && owner!=pendingNullGPUOwners.end())nullGPUOwners[key]=owner->second;
        pendingNullGPUOwners.erase(key);
      }
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
      // A shader can scatter a bounded list of explicitly sourced descriptors.
      // Each 24-byte source/destination still needs its exact logical slot, value,
      // type and Native source proof; do not expand ordinary blit allowances.
      success = success && liveEncoders.count(encoder) &&
          (m_DescriptorCoverage >= 45 ? ++producerCount <= 256 : ++copyCount <= 2) &&
          Serialise_DescriptorSlotProducer(scan, ResourceId(), 0, ResourceId(), ResourceId(), 0);
      if(success) gpuCopyEncoders[make_rdcpair(buffer, offset)] = encoder;
      if(success)
      {
        setNullBufferField(buffer,offset,false);
        pendingNullGPUOwners[make_rdcpair(buffer,offset)]=encoderCommands[encoder];
      }
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
    else if(chunk == MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_origin ||
            chunk == MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture ||
            chunk == MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_count)
    {
      ResourceId encoder, source, destination;
      uint64_t sourceSlice=0, sourceLevel=0, destinationSlice=0, destinationLevel=0;
      uint64_t sliceCount=1, levelCount=1;
      MTL::Origin sourceOrigin, destinationOrigin;
      MTL::Size size;
      scan.Serialise("BlitCommandEncoder"_lit,encoder);
      scan.Serialise("sourceTexture"_lit,source);
      const bool region=chunk==MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture_slice_level_origin;
      const bool whole=chunk==MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toTexture;
      if(!whole) {
        scan.Serialise("sourceSlice"_lit,sourceSlice);scan.Serialise("sourceLevel"_lit,sourceLevel);
        if(region) {scan.Serialise("sourceOrigin"_lit,sourceOrigin);scan.Serialise("sourceSize"_lit,size);}
      }
      scan.Serialise("destinationTexture"_lit,destination);
      if(!whole) {
        scan.Serialise("destinationSlice"_lit,destinationSlice);scan.Serialise("destinationLevel"_lit,destinationLevel);
        if(region) scan.Serialise("destinationOrigin"_lit,destinationOrigin);
        else {scan.Serialise("sliceCount"_lit,sliceCount);scan.Serialise("levelCount"_lit,levelCount);}
      }
      auto describe=[&](ResourceId id,RDMTL::TextureDescriptor &d) {
        if(m_DescriptorPreflightAliasedBuffers.count(id) ||
           m_DescriptorPreflightAliasedBuffers.count(viewBacking(id))) return false;
        auto future=m_DescriptorFrameTextures.find(id);
        if(future!=m_DescriptorFrameTextures.end()) {
          if(!m_DescriptorPreflightLiveTextures.count(id)) return false;
          d=future->second.descriptor;return true;
        }
        auto object=GetResourceManager()->GetResource(id,true);
        auto native=object && object->m_Type==eResTexture && object->m_Device==this ?
            Unwrap((WrappedMTLTexture *)object):NULL;
        if(!native)return false;
        d.textureType=native->textureType();d.pixelFormat=native->pixelFormat();
        d.width=native->width();d.height=native->height();d.depth=native->depth();
        d.arrayLength=native->arrayLength();d.mipmapLevelCount=native->mipmapLevelCount();
        d.sampleCount=native->sampleCount();return true;
      };
      RDMTL::TextureDescriptor src,dst;
      success=liveBlits.count(encoder) && describe(source,src) && describe(destination,dst) &&
          src.pixelFormat==dst.pixelFormat && src.sampleCount==dst.sampleCount;
      if(success && whole) {
        sliceCount=src.textureType==MTL::TextureTypeCube?6:src.arrayLength;
        if(src.textureType==MTL::TextureTypeCubeArray) {
          success=sliceCount<=UINT64_MAX/6;sliceCount*=6;
        }
        levelCount=src.mipmapLevelCount;
        success &= src.textureType==dst.textureType && src.arrayLength==dst.arrayLength &&
            src.width==dst.width && src.height==dst.height && src.depth==dst.depth &&
            src.mipmapLevelCount==dst.mipmapLevelCount;
      }
      success &= sliceCount && levelCount && sourceSlice<=UINT64_MAX-sliceCount &&
          destinationSlice<=UINT64_MAX-sliceCount && sourceLevel<=UINT64_MAX-levelCount &&
          destinationLevel<=UINT64_MAX-levelCount && levelCount<=64 &&
          sourceLevel<src.mipmapLevelCount && levelCount<=src.mipmapLevelCount-sourceLevel &&
          destinationLevel<dst.mipmapLevelCount && levelCount<=dst.mipmapLevelCount-destinationLevel &&
          sliceCount<=UINT64_MAX/levelCount && preflightBudget.Consume(sliceCount*levelCount);
      if(success && !region) {
        success=src.textureType==dst.textureType &&
            RDCMAX(1ULL,uint64_t(src.width)>>sourceLevel)==RDCMAX(1ULL,uint64_t(dst.width)>>destinationLevel) &&
            RDCMAX(1ULL,uint64_t(src.height)>>sourceLevel)==RDCMAX(1ULL,uint64_t(dst.height)>>destinationLevel) &&
            RDCMAX(1ULL,uint64_t(src.depth)>>sourceLevel)==RDCMAX(1ULL,uint64_t(dst.depth)>>destinationLevel);
        if(source==destination && sourceSlice<destinationSlice+sliceCount &&
           destinationSlice<sourceSlice+sliceCount && sourceLevel<destinationLevel+levelCount &&
           destinationLevel<sourceLevel+levelCount) success=false;
      }
      for(uint64_t slice=0;success && slice<sliceCount;slice++)
        for(uint64_t level=0;success && level<levelCount;level++) {
          if(!region) {
            size=MTL::Size(RDCMAX(1ULL,uint64_t(src.width)>>(sourceLevel+level)),
                RDCMAX(1ULL,uint64_t(src.height)>>(sourceLevel+level)),
                RDCMAX(1ULL,uint64_t(src.depth)>>(sourceLevel+level)));
            sourceOrigin=destinationOrigin=MTL::Origin(0,0,0);
          }
          uint64_t footprint=0;
          success=ValidateMetalTextureRegion(src,sourceSlice+slice,sourceLevel+level,sourceOrigin,size,&footprint) &&
              ValidateMetalTextureRegion(dst,destinationSlice+slice,destinationLevel+level,destinationOrigin,size) &&
              preflightBudget.Consume(footprint);
        }
      if(success) {
        // Original Native copies produce ordinary pixels; no CPU shader/content
        // proof replaces their restored inputs and encoded submission order.
        noteResource(source,encoder);noteResource(destination,encoder);
        typedTextureWriteCommands[destination]=encoderCommands[encoder];
        colorWriteCommands[destination].insert(encoderCommands[encoder]);
        modifiedBuffers.insert(destination);opaqueWrites.insert(destination);
        pendingWork.insert(encoderCommands[encoder]);
        m_DescriptorPartialCopySubmissions[encoderCommands[encoder]].valid=false;
      }
    }
    else if(chunk==MetalChunk::MTLBlitCommandEncoder_synchronizeResource ||
            chunk==MetalChunk::MTLBlitCommandEncoder_synchronizeTexture)
    {
      ResourceId encoder,resource;uint64_t slice=0,level=0;
      scan.Serialise("BlitCommandEncoder"_lit,encoder);
      scan.Serialise(chunk==MetalChunk::MTLBlitCommandEncoder_synchronizeResource?"resource"_lit:"texture"_lit,resource);
      if(chunk==MetalChunk::MTLBlitCommandEncoder_synchronizeTexture) {
        scan.Serialise("slice"_lit,slice);scan.Serialise("level"_lit,level);
      }
      auto object=GetResourceManager()->GetResource(resource,true);
      auto native=object && object->m_Real && object->m_Device==this &&
          (object->m_Type==eResTexture || object->m_Type==eResBuffer)?Unwrap((WrappedMTLResource *)object):NULL;
      success=liveBlits.count(encoder) && native && native->storageMode()==MTL::StorageModeManaged &&
          !m_DescriptorPreflightAliasedBuffers.count(resource) && !isTable(resource);
      if(success && chunk==MetalChunk::MTLBlitCommandEncoder_synchronizeTexture) {
        auto texture=object->m_Type==eResTexture?Unwrap((WrappedMTLTexture *)object):NULL;
        success=texture && level<texture->mipmapLevelCount() && slice<
            (texture->textureType()==MTL::TextureTypeCube?6ULL:
             uint64_t(texture->arrayLength())*(texture->textureType()==MTL::TextureTypeCubeArray?6ULL:1ULL));
      }
      if(success) {
        noteResource(resource,encoder);pendingWork.insert(encoderCommands[encoder]);
        m_DescriptorPartialCopySubmissions[encoderCommands[encoder]].valid=false;
      }
    }
    else if(chunk==MetalChunk::MTLTexture_getBytes || chunk==MetalChunk::MTLTexture_getBytes_slice)
    {
      ResourceId texture;MTL::Region region;uint64_t row=0,image=0,level=0,slice=0;
      scan.Serialise("Texture"_lit,texture);scan.Serialise("bytesPerRow"_lit,row);
      if(chunk==MetalChunk::MTLTexture_getBytes_slice) scan.Serialise("bytesPerImage"_lit,image);
      scan.Serialise("region"_lit,region);scan.Serialise("level"_lit,level);
      if(chunk==MetalChunk::MTLTexture_getBytes_slice) scan.Serialise("slice"_lit,slice);
      auto object=GetResourceManager()->GetResource(texture,true);
      success=object && object->m_Type==eResTexture && object->m_Device==this &&
          !m_DescriptorPreflightAliasedBuffers.count(texture) &&
          ValidateMetalCPUTextureRead((WrappedMTLTexture *)object,region,level,slice,row,image);
      // Like D3D12 MapDataWrite/Vulkan mapped-memory publication, replay restores
      // derived CPU writes rather than running the application's CPU read again.
      // The original host may have waited through a completion callback, without
      // a waitUntilCompleted chunk. Require preceding submissions, not that one
      // host API spelling. ApplyReplayCPUBufferUpdates retains the actual waits.
      for(ResourceId command:resourceCommands[texture]) success &= committed.count(command)!=0;
      if(!success && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal CPU texture observation rejected: resource=%s object=%d row=%llu image=%llu level=%llu slice=%llu\n",
            ToStr(texture).c_str(),int(object!=NULL),(unsigned long long)row,
            (unsigned long long)image,(unsigned long long)level,(unsigned long long)slice);
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
      success=liveBlits.count(encoder) && !isTable(source) &&
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
        success &= preflightBudget.Consume(footprint);
      }
      if(success) {
        plainCopyBytes+=footprint;
        noteResource(source,encoder);noteResource(destination,encoder);
        modifiedBuffers.insert(destination);opaqueWrites.insert(destination);noteOpaqueScalarWrite(destination,chunkOffset);
        pendingWork.insert(encoderCommands[encoder]);
        m_DescriptorPartialCopySubmissions[encoderCommands[encoder]].valid=false;
      }
    }
    else if(m_DescriptorCoverage >= 65 &&
       (chunk == MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toBuffer ||
        chunk == MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toBuffer_options))
    {
      ResourceId encoder,source,destination;
      uint64_t slice=0,level=0,offset=0,rowPitch=0,imagePitch=0;
      MTL::Origin origin;MTL::Size size;MTL::BlitOption options=MTL::BlitOptionNone;
      scan.Serialise("BlitCommandEncoder"_lit,encoder);
      scan.Serialise("sourceTexture"_lit,source);scan.Serialise("sourceSlice"_lit,slice);
      scan.Serialise("sourceLevel"_lit,level);scan.Serialise("sourceOrigin"_lit,origin);
      scan.Serialise("sourceSize"_lit,size);scan.Serialise("destinationBuffer"_lit,destination);
      scan.Serialise("destinationOffset"_lit,offset);scan.Serialise("destinationBytesPerRow"_lit,rowPitch);
      scan.Serialise("destinationBytesPerImage"_lit,imagePitch);
      if(chunk==MetalChunk::MTLBlitCommandEncoder_copyFromTexture_toBuffer_options)
        scan.Serialise("options"_lit,options);
      const ResourceId command=encoderCommands[encoder];
      auto srcObject=GetResourceManager()->GetResource(source,true);
      auto dstObject=GetResourceManager()->GetResource(destination,true);
      WrappedMTLTexture *texture=srcObject && srcObject->m_Type==eResTexture?(WrappedMTLTexture *)srcObject:NULL;
      MTL::Buffer *buffer=dstObject && dstObject->m_Type==eResBuffer?Unwrap((WrappedMTLBuffer *)dstObject):NULL;
      const auto future=m_DescriptorFrameBuffers.find(destination);
      const auto futureTexture=m_DescriptorFrameTextures.find(source);
      const uint64_t length=future!=m_DescriptorFrameBuffers.end()?future->second.length:buffer?buffer->length():0;
      const auto writer=typedTextureWriteCommands.find(source);
      const bool produced=writer!=typedTextureWriteCommands.end() &&
          (writer->second==command || committed.count(writer->second));
      uint64_t footprint=0;
      const bool validLayout=futureTexture!=m_DescriptorFrameTextures.end()?
          m_DescriptorPreflightLiveTextures.count(source) &&
          ValidateMetalLinearTextureCopy(futureTexture->second.descriptor,slice,level,origin,size,
              length,offset,rowPitch,imagePitch,options,&footprint):
          ValidateMetalLinearTextureCopy(texture,slice,level,origin,size,length,offset,rowPitch,imagePitch,options,&footprint);
      success=liveBlits.count(encoder) && !isTable(destination) &&
          ((futureTexture!=m_DescriptorFrameTextures.end() && m_DescriptorPreflightLiveTextures.count(source)) ||
           (texture && texture->m_Device==this)) && buffer && dstObject->m_Device==this &&
          !m_DescriptorPreflightAliasedBuffers.count(source) && !m_DescriptorPreflightAliasedBuffers.count(destination) &&
          (future==m_DescriptorFrameBuffers.end() || m_DescriptorPreflightLiveBuffers.count(destination)) &&
          (HasReplayTextureInitialContents(source) || produced) &&
          validLayout;
      if(success)
      {
        success=preflightBudget.Consume(footprint);
        if(success)
        {
          plainCopyBytes+=footprint;noteResource(source,encoder);noteResource(destination,encoder);
          modifiedBuffers.insert(destination);opaqueWrites.insert(destination);
          noteOpaqueScalarWrite(destination,chunkOffset);invalidateAliasedBufferContents(destination);
          pendingWork.insert(command);m_DescriptorPartialCopySubmissions[command].valid=false;
        }
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
        const uint64_t budget = DescriptorPlainCopyLimit(m_DescriptorCoverage);
        success = size <= budget && preflightBudget.Consume(size) &&
            liveBlits.count(encoder) &&
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
          invalidateAliasedBufferContents(destination);
          plainCopyBytes += size;
          ++plainCopyCount;
          const auto cbvTarget=m_DescriptorFrameBuffers.find(destination);
          const auto cbvLayout=indirectFootprint(destination);
          const uint64_t cbvLength=cbvTarget!=m_DescriptorFrameBuffers.end()?cbvTarget->second.length:
              GetReplay()->GetBuffer(destination).length;
          if((!m_RayQueryHeapDispatches.empty() || !m_IRComputeRuntimeABIs.empty()) &&
             cbvLength && cbvLength<=DescriptorFrameBufferLimit(bool(cbvLayout.heap),m_DescriptorCoverage))
          {
            bytebuf known;
            const bool sourced=readIndexUpload(source,encoderCommands[encoder],sourceOffset,size,known);
            computeCBVOperations[encoderCommands[encoder]].push_back(
                {destination,destinationOffset,size,false,sourced,0,known,source,sourceOffset});
          }
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
        const uint64_t budget=DescriptorPlainCopyLimit(m_DescriptorCoverage);
        success = size<=budget && preflightBudget.Consume(size) && liveBlits.count(encoder) &&
            TrackDescriptorGPUCopy(source, sourceOffset, destination, destinationOffset, size);
        if(success)
        {
          ++copyCount;plainCopyBytes+=size;
          setNullBufferField(destination,destinationOffset,false);
          pendingNullGPUOwners[make_rdcpair(destination,destinationOffset)]=encoderCommands[encoder];
        }
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
      const auto layout=m_DescriptorInlineShadow.find(make_rdcpair(encoder,index));
      if(layout!=m_DescriptorInlineShadow.end())
        for(const auto &source:layout->second.sources)noteResource(source.second.resource,encoder);
      std::map<uint64_t,DescriptorSource> pointers;
      if(layout!=m_DescriptorInlineShadow.end())
        for(const auto &source:layout->second.sources)
          pointers[source.first*layout->second.stride]=source.second;
      success = liveEncoders.count(encoder) && RelocateDescriptorInlineShadow(encoder, 0, index, data);
      if(success)
      {
        appliedInline[encoder].insert(index);
        computes[encoder].buffers.erase((uint32_t)index);
        computes[encoder].bytes[(uint32_t)index] = data.size();
        computes[encoder].inlineData[(uint32_t)index] = data;
        computes[encoder].pointers[(uint32_t)index] = pointers;
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
          if(success) success=validateComputeCBVSubmission(command);
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
          MTL::Size groups={}, threads={};
          ResourceId encoder; scan.Serialise("ComputeCommandEncoder"_lit, encoder);
          dispatchEncoder=encoder;
          runtimeConsumerEncoders.erase(encoder);
          ++dispatchCount;
          success &= liveEncoders.count(encoder) && !appliedInline[encoder].empty() &&
              GetReplay()->ValidateComputeBufferSnapshot(computes[encoder].pipeline,
                  computes[encoder].buffers, computes[encoder].bytes);
          const bool indirectDispatch = m_DescriptorCoverage >= 57 &&
              chunk == MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups_indirect;
          if(chunk != MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups && !indirectDispatch) success = false;
          else
          {
            ResourceId indirectArguments;
            if(indirectDispatch)
            {
              ResourceId arguments; uint64_t offset=0;
              scan.Serialise("indirectBuffer"_lit, arguments);
              indirectArguments=arguments;
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
              if(success && m_DescriptorCoverage>=65)
              {
                ComputeCBVOperation input;
                input.buffer=arguments;input.offset=offset;input.bytes=12;
                input.read=true;input.known=false;input.readOffset=chunkOffset;input.indirectInput=true;
                computeCBVOperations[encoderCommands[encoder]].push_back(input);
              }
            }
            else scan.Serialise("groups"_lit, groups);
            scan.Serialise("threadsPerGroup"_lit, threads);
            // Capture-time counts are observations of ordinary GPU data. They
            // cannot prove a Native indirect consumer inactive on replay.
            dispatchRuns=indirectDispatch || (groups.width && groups.height && groups.depth);
            if(HasRayQueryHeapPipeline(computes[encoder].pipeline))
            {
              const auto heap=computes[encoder].buffers[0];
              success &= ValidateRayQueryHeapDispatch(computes[encoder].pipeline,
                  heap.resourceId,heap.byteOffset,computes[encoder].inlineData[2],groups,threads,indirectDispatch,
                  [&](ResourceId buffer,uint64_t offset,uint64_t bytes) {
                    if(opaqueWrites.count(buffer) || m_DescriptorPreflightAliasedBuffers.count(buffer))return false;
                    computeCBVOperations[encoderCommands[encoder]].push_back({buffer,offset,bytes,true,false,chunkOffset,{}});
                    return true;
                  });
              if(indirectDispatch)
                success &= !m_IRComputeWriteResources.count(indirectArguments) &&
                           !m_RayIRReadResources.count(indirectArguments);
              if(success)
              {
                if(m_RayQueryHeapCBVRoots.count(computes[encoder].pipeline))
                {
                  UniformDispatch uniform;
                  uniform.pipeline=computes[encoder].pipeline;uniform.heap=heap.resourceId;
                  uniform.roots=computes[encoder].inlineData[2];uniform.pointers=computes[encoder].pointers[2];
                  uniform.invocationExtent={groups.width*threads.width,groups.height*threads.height,groups.depth*threads.depth};
                  uniform.writes=m_IRComputeWriteResources;
                  const auto roots=m_IRComputeRoots.find(uniform.pipeline);
                  if(roots!=m_IRComputeRoots.end())
                  for(const auto &root:roots->second)
                    if(root.kind==3)
                    {
                      const auto pointer=uniform.pointers.find(root.offset);
                      if(pointer==uniform.pointers.end()) {success=false;break;}
                      for(uint64_t i=0;i<root.count;i++)
                      {
                        const auto key=make_rdcpair(pointer->second.resource,pointer->second.offset+i*24);
                        const auto sampler=m_DescriptorSlotShadow.find(key);
                        if(sampler==m_DescriptorSlotShadow.end()) {success=false;break;}
                        uniform.samplers[key]=sampler->second;
                      }
                    }
                  auto saveSlot=[&](uint64_t offset) {
                    const auto slot=m_DescriptorSlotShadow.find(make_rdcpair(uniform.heap,offset));
                    if(slot==m_DescriptorSlotShadow.end())return;
                    uniform.slots[offset]=slot->second;
                    const auto source=slot->second.sources.find(3);
                    if(source!=slot->second.sources.end())
                    {
                      const auto key=make_rdcpair(source->second.resource,source->second.offset);
                      const auto header=m_RayASHeaderCurrent.find(key);
                      if(header!=m_RayASHeaderCurrent.end())uniform.headers[key]=header->second;
                    }
                  };
                  for(const auto &query:m_RayQueryHeapDispatches)
                    if(query.pipeline==uniform.pipeline && query.heap==uniform.heap)saveSlot(query.slotOffset);
                  for(const auto &cbv:m_RayQueryHeapCBVRoots.at(uniform.pipeline))saveSlot(cbv.outputSlot);
                  const auto entries=m_IRComputeHeapEntries.find(uniform.pipeline);
                  if(entries!=m_IRComputeHeapEntries.end())
                    for(const auto &declaration:entries->second)
                      if(!declaration.heap)
                        saveSlot(declaration.index*24);
                  uniformDispatches[chunkOffset]=uniform;
                  computeCBVOperations[encoderCommands[encoder]].push_back({ResourceId(),0,0,false,false,chunkOffset,{}});
                }
                // Explicit write residency must fit the typed dispatch closure.
                // Defer these writes until validation so a proven earlier UAV
                // producer can be read later without becoming an opaque writer.
                for(ResourceId resource:typedQueryWriteResidency[encoder])
                  success &= m_IRComputeWriteResources.count(resource)!=0;
                for(ResourceId resource:m_RayIRReadResources)
                {
                  success &= (!modifiedBuffers.count(resource) || m_IRComputeFrameCBVResources.count(resource) ||
                              typedTextureWriteCommands.count(resource)) &&
                             !opaqueWrites.count(resource);
                  if(typedTextureWriteCommands.count(resource))
                    success &= typedTextureWriteCommands[resource]==encoderCommands[encoder] ||
                               committed.count(typedTextureWriteCommands[resource])!=0;
                  if(rayASBuildCommands.count(resource))
                    success &= rayASBuildCommands[resource]==encoderCommands[encoder] ||
                               committed.count(rayASBuildCommands[resource])!=0;
                  noteResource(resource,encoder);
                }
                for(ResourceId output:m_IRComputeWriteResources)
                {
                  noteResource(output,encoder);
                  auto object=GetResourceManager()->GetResource(output,true);
                  if(dispatchRuns && object)
                  {
                    modifiedBuffers.insert(output);
                    invalidateAliasedBufferContents(output);
                    if(object->m_Type==eResTexture)
                      typedTextureWriteCommands[output]=encoderCommands[encoder];
                    else
                      {opaqueWrites.insert(output);noteOpaqueScalarWrite(output,chunkOffset);}
                  }
                }
              }
            }
            if(success && dispatchRuns && m_DescriptorCoverage>=65 &&
               !HasRayQueryHeapPipeline(computes[encoder].pipeline) &&
               m_IRComputeRuntimeABIs.count(computes[encoder].pipeline))
            {
              const auto &snapshot=computes[encoder];
              const auto &abi=m_IRComputeRuntimeABIs.at(snapshot.pipeline);
              const auto root=snapshot.inlineData.find(abi.rootBindPoint);
              const auto pointers=snapshot.pointers.find(abi.rootBindPoint);
              const auto heap=snapshot.buffers.find(abi.resourceBindPoint);
              success &= root!=snapshot.inlineData.end() && pointers!=snapshot.pointers.end() &&
                  heap!=snapshot.buffers.end() && root->second.size()==abi.rootBytes &&
                  isTable(heap->second.resourceId);
              if(success)
              {
                RuntimeDispatch uniform;
                uniform.nullBufferFields=nullBufferFields;uniform.nullGPUOwners=nullGPUOwners;
                uniform.pipeline=snapshot.pipeline;uniform.abi=abi;uniform.roots=root->second;
                uniform.pointers=pointers->second;uniform.heap=heap->second.resourceId;
                uniform.heapOffset=heap->second.byteOffset;
                const auto samplers=snapshot.buffers.find(abi.samplerBindPoint);
                if(samplers!=snapshot.buffers.end() && isTable(samplers->second.resourceId))
                {uniform.samplerHeap=samplers->second.resourceId;uniform.samplerHeapOffset=samplers->second.byteOffset;}
                if(!indirectDispatch)
                  uniform.invocationExtent={groups.width*threads.width,groups.height*threads.height,groups.depth*threads.depth};
                // Only a direct dispatch supplies statically known Native
                // builtins. Group coordinates are distinct from global IDs;
                // the local scalar index spans the full 3D threadgroup.
                MetalAIR::Value group;
                group.kind=MetalAIR::Value::Aggregate;
                for(uint64_t count:{groups.width,groups.height,groups.depth})
                  group.fields.push_back(indirectDispatch?MetalAIR::Value():MetalAIR::Value::Range(0,count-1));
                uniform.builtins.builtins["threadgroup_position_in_grid"]=group;
                uniform.builtins.builtins["thread_index_in_threadgroup"]=MetalAIR::Value::Range(
                    0,threads.width*threads.height*threads.depth-1);
                uniform.opaque=opaqueWrites;
                for(const auto &slot:m_DescriptorSlotShadow)
                  if(slot.second.live && !slot.second.data.empty())uniform.slots.insert(slot);
                uniform.headers=m_RayASHeaderCurrent;uniform.structures=m_RayIRASCurrentContents;
                uniform.structureOwners=rayASBuildCommands;
                runtimeDispatches[make_rdcpair(chunkOffset,0U)]=uniform;
                computeCBVOperations[encoderCommands[encoder]].push_back({ResourceId(),0,0,false,false,chunkOffset,{}});
                // This only schedules a proof. A failed runtime resource closure
                // aborts the complete frame before any Native submission.
                runtimeConsumerEncoders.insert(encoder);
              }
            }
            if(m_DescriptorCoverage >= 46)
            {
              // Reuse the normal dispatch compiler/device/threadgroup checks on
              // this CPU snapshot. Dynamic threadgroup bindings remain outside
              // the sourced contract until they have a corresponding snapshot.
              success &= GetReplay()->ValidateComputeThreadgroupSnapshot(
                  computes[encoder].pipeline, threads, {});
              const auto abi = m_IRComputeRuntimeABIs.find(computes[encoder].pipeline);
              if(abi != m_IRComputeRuntimeABIs.end())
              {
                const auto roots = computes[encoder].inlineData.find(abi->second.rootBindPoint);
                const auto bytes = computes[encoder].bytes.find(abi->second.rootBindPoint);
                success &= bytes != computes[encoder].bytes.end() &&
                           bytes->second == abi->second.rootBytes &&
                           roots != computes[encoder].inlineData.end() &&
                           roots->second.size() == abi->second.rootBytes;
              }
              // Native indirect counts are ordinary execution-time GPU data.
              // The per-use GPU guard charges the actual work before execution;
              // an old capture observation must not decide its replay budget.
              // Explicit legacy query recipes still require their frozen
              // footprint, which the same guard verifies on the GPU.
              if(!indirectDispatch)
              {
                const uint64_t groupAxes[] = {groups.width, groups.height, groups.depth};
                const uint64_t threadAxes[] = {threads.width, threads.height, threads.depth};
                success &= MetalComputeDispatchExtentFits(groupAxes, threadAxes);
              }
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
          if(success && dispatchRuns && !HasRayQueryHeapPipeline(computes[encoder].pipeline) &&
             !m_IRComputeRuntimeABIs.count(computes[encoder].pipeline))
          {
            // Native argument-buffer shaders use the actual inline pointer
            // declarations, not the converted runtime root ABI. Reuse the
            // same restoration/dependency proof in submission order; write
            // residency alone cannot manufacture a texture producer.
            const auto &snapshot=computes[encoder];
            // Ordinary inline bytes are admitted without a fabricated pointer
            // layout, but actual pointer loads still need certified origins.
            // Schedule AIR recovery even if a missing inline declaration has
            // hidden the typed-table root from the declared binding map.
            bool descriptors=!snapshot.inlineData.empty();
            // A typed table can be bound directly, without an inline root.
            // Its actual Native slot/resource/offset has the same restoration
            // obligations as a table reached through declared inline pointers.
            for(const auto &binding:snapshot.buffers)
              descriptors |= isTable(binding.second.resourceId);
            for(const auto &binding:snapshot.pointers)
              for(const auto &pointer:binding.second)descriptors |= isTable(pointer.second.resource);
            if(descriptors)
            {
              RuntimeDispatch native;
              native.nullBufferFields=nullBufferFields;native.nullGPUOwners=nullGPUOwners;
              native.pipeline=snapshot.pipeline;native.nativeCompute=true;native.graphics=snapshot;native.opaque=opaqueWrites;
              if(!indirectDispatch)
                native.invocationExtent={groups.width*threads.width,groups.height*threads.height,groups.depth*threads.depth};
              for(const auto &slot:m_DescriptorSlotShadow)
                if(slot.second.live && !slot.second.data.empty())native.slots.insert(slot);
              native.headers=m_RayASHeaderCurrent;native.structures=m_RayIRASCurrentContents;
              native.structureOwners=rayASBuildCommands;
              runtimeDispatches[make_rdcpair(chunkOffset,0U)]=native;
              computeCBVOperations[encoderCommands[encoder]].push_back({ResourceId(),0,0,false,false,chunkOffset,{}});
              runtimeConsumerEncoders.insert(encoder);
            }
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
                modifiedBuffers.insert(binding.second.resourceId); opaqueWrites.insert(binding.second.resourceId);noteOpaqueScalarWrite(binding.second.resourceId,chunkOffset);
                if(m_DescriptorCoverage>=64 && m_DescriptorGPUWrittenBuffers.count(binding.second.resourceId) &&
                   !producerDestinations[encoder].count(binding.second.resourceId))
                  opaqueTableWrites.insert(binding.second.resourceId);
                if(!producerDestinations[encoder].count(binding.second.resourceId))
                  invalidateNullBufferFields(binding.second.resourceId);
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
    const uint64_t endOffset = m_FrameReader->GetOffset();
    if(endOffset < chunkOffset || !preflightBudget.Consume(endOffset - chunkOffset))
    {
      success = false;
      RDCERR("Metal replay preflight bookkeeping budget exhausted");
    }
  }
  success &= (m_DescriptorCoverage != 66 || meshDrawCount != 0) &&
             (m_RayQueryHeapDispatches.empty() || dispatchCount!=0) &&
             !scan.IsErrored() && liveAS.empty() && m_RayASHeaderFrameCursor==m_RayASHeaderFrameWrites.size() &&
             m_DescriptorInlineShadow.empty() &&
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
    if(m_DescriptorCoverage>=65)
    {
      success &= restoredIndexInputs.count(indexBuffer)!=0;
      continue;
    }
    success &= (!modifiedBuffers.count(indexBuffer) || copiedFrameIndices.count(indexBuffer)) && !opaqueWrites.count(indexBuffer);
    const auto contents = indexContents.find(indexBuffer);
    for(const CPUUpdate &update : cpuUpdates)
      if(update.buffer == indexBuffer)
        success &= contents != indexContents.end() && update.start <= contents->second.size() &&
            update.data.size() <= contents->second.size() - update.start &&
            memcmp(contents->second.data() + update.start, update.data.data(), update.data.size()) == 0;
  }
  if(m_DescriptorCoverage==65)
  {
    // Native queries dereference an independently relocated header and ordinary
    // contribution data at their actual submission. The latter can come from
    // original GPU producers; it is not another immutable address publication.
    // Explicit legacy dispatch annotations still use frozen scalar snapshots.
    const bool frozenContributions=!m_RayIRDispatches.empty() ||
        !m_RayQueryDispatches.empty() || !m_RayQueryHeapDispatches.empty();
    auto validateHeaderWrites=[&](const RayASHeader &h) {
      const bool header=!modifiedBuffers.count(h.buffer) && !opaqueWrites.count(h.buffer);
      const bool contribution=!frozenContributions ||
          (!modifiedBuffers.count(h.contributions) && !opaqueWrites.count(h.contributions));
      if((!header || !contribution) && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
        fprintf(stderr,"Metal AS final state rejected: header=%s contributions=%s headerRestored=%d frozenContributions=%d contributionRestored=%d\n",
            ToStr(h.buffer).c_str(),ToStr(h.contributions).c_str(),int(header),
            int(frozenContributions),int(contribution));
      return header && contribution;
    };
    for(const auto &h:m_RayASHeaders)success &= validateHeaderWrites(h.second);
    for(const auto &h:m_RayASHeaderFrameWrites)success &= validateHeaderWrites(h);
  }
  if(!success && getenv("RENDERDOC_METAL_TRACE_DESCRIPTOR_PREFLIGHT"))
    fprintf(stderr,"Metal final frame state rejected: scanError=%d ASencoders=%zu headerCursor=%zu/%zu inline=%zu GPUcopies=%zu blits=%zu renders=%zu computes=%zu parallel=%zu children=%zu parents=%zu committed=%zu/%zu\n",
        int(scan.IsErrored()),liveAS.size(),m_RayASHeaderFrameCursor,m_RayASHeaderFrameWrites.size(),
        m_DescriptorInlineShadow.size(),m_DescriptorGPUCopyExpected.size(),liveBlits.size(),
        liveRenders.size(),liveEncoders.size(),liveParallelRenders.size(),activeChildren.size(),
        childParents.size(),committed.size(),commands.size());
  if(success && m_DescriptorCoverage >= 52) m_DescriptorSubmissionOrder = submissionOrder;
  m_DescriptorGPUCopyExpected.clear();
  m_DescriptorDispatches.clear();
  m_DescriptorSlotShadow = m_DescriptorSlotInitial;
  m_DescriptorPreflightLiveBuffers.clear();
  m_DescriptorPreflightLiveTables.clear();
  m_DescriptorPreflightAliasedBuffers.clear();
  m_DescriptorInlineShadow.clear();
  m_DescriptorPreflight = false;
  m_RayIRASCurrentContents=m_ReplayASInitialContents;
  if(!success) m_DescriptorSubmissionSlots.clear();
  if(!success) { m_ValidatedDescriptorBackingAliases.clear(); m_DescriptorBackingAliasConsumers.clear(); m_RetiredTextureAliasConsumers.clear(); m_DescriptorValidatedComputeResources.clear(); m_DescriptorSubmissionSnapshotOwners.clear(); m_DescriptorPartialCopySubmissions.clear(); m_DescriptorFrameBufferBirthOffsets.clear(); m_DescriptorSubmissionOrder.clear(); }
  m_FrameReader->SetOffset(0);
  return success;
}

bool WrappedMTLDevice::ApplyDescriptorCPUUpdate(ResourceId buffer, uint64_t start, const bytebuf &data)
{
  if(m_DescriptorCoverage >= 4)
  {
    auto header=m_DescriptorRawContents.find(buffer);
    bool headerBuffer=false;
    for(const auto &entry:m_RayASHeaderCurrent)
      headerBuffer |= entry.second.buffer==buffer;
    // A commit-owned CPU snapshot contains capture-process IDs and addresses.
    // Relocate an already validated public header regardless of which PSO uses it.
    if(header!=m_DescriptorRawContents.end() && headerBuffer)
    {
      if(start>header->second.size() || data.size()>header->second.size()-start ||
         memcmp(header->second.data()+start,data.data(),data.size())) return false;
      return m_DescriptorPreflight || RestoreDescriptorTable(buffer,header->second);
    }
    return m_DescriptorPreflight ||
        (m_DescriptorCoverage >= 65 ?
         (OverlayDescriptorSlotBuffer(buffer, false, start, data.size()) &&
          OverlayAliasedDescriptorTables(buffer, start, data.size())) :
         (OverlayDescriptorSlotBuffer(buffer) && OverlayAliasedDescriptorTables(buffer)));
  }
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
  auto reject = [](uint32_t line) {
    RDCERR("Invalid Metal descriptor frame closure at validation line %u", line);
    fprintf(stderr, "Invalid Metal descriptor frame closure at validation line %u\n", line);
    return false;
  };
  if(m_DescriptorCoverage >= 4)
    return ValidateDescriptorSlotFrame();
  if(m_DescriptorTables.empty())
    return true;
  struct ValidationScope
  {
    bool &flag;
    std::map<ResourceId, std::shared_ptr<MetalASInitialBuild>> &current;
    const std::map<ResourceId, std::shared_ptr<MetalASInitialBuild>> &initial;
    ValidationScope(bool &f, decltype(current) c, decltype(initial) i)
        : flag(f), current(c), initial(i) { flag = true; current = initial; }
    ~ValidationScope() { flag = false; current = initial; }
  } validation(m_DescriptorPreflight, m_RayIRASCurrentContents, m_ReplayASInitialContents);
  struct HeaderScope
  {
    WrappedMTLDevice &device;
    decltype(m_DescriptorRawContents) raw;
    HeaderScope(WrappedMTLDevice &d):device(d),raw(d.m_DescriptorRawContents)
    { d.m_RayASHeaderCurrent=d.m_RayASHeaders; d.m_RayASHeaderFrameCursor=0; }
    ~HeaderScope()
    { device.m_DescriptorRawContents=raw; device.m_RayASHeaderCurrent=device.m_RayASHeaders;
      device.m_RayASHeaderFrameCursor=0; }
  } headers(*this);
  m_DescriptorPreflightLiveBuffers.clear();
  m_DescriptorPreflightLiveViews.clear();
  std::map<ResourceId, rdcarray<rdcpair<uint64_t, uint64_t>>> futureHeapRanges;
  struct ComputeState { ResourceId command, pipeline; std::map<uint32_t, ResourceId> buffers; std::map<uint32_t,uint64_t> offsets; };
  std::map<ResourceId, ComputeState> states;
  std::map<ResourceId, ResourceId> commandQueues, asEncoders, blitEncoders;
  std::set<ResourceId> rayInputs, gpuWrites;
  std::set<ResourceId> committedCommands, pendingASCommands;
  ResourceId rayQueue;
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
    // IR records/headers/function tables stay immutable. Frame AS input snapshots
    // update a separate current recipe, before loading can encode a ray dispatch.
    if((!m_RayIRDispatches.empty() || !m_RayQueryDispatches.empty()))
    {
      const rdcstr name = ToStr(chunk);
      if(chunk==MetalChunk::MTLBuffer_DeclareRayASHeader)
      {
        RayASHeader h;
        scan.Serialise("buffer"_lit,h.buffer); scan.Serialise("offset"_lit,h.offset);
        scan.Serialise("structure"_lit,h.structure); scan.Serialise("contributions"_lit,h.contributions);
        scan.Serialise("contributionOffset"_lit,h.contributionOffset); scan.Serialise("bytes"_lit,h.bytes);
        // No header publication may change bytes a still-uncommitted query reads.
        for(const auto &state:states)
          if(!committedCommands.count(state.second.command) && rayInputs.count(h.buffer))
            return reject(__LINE__);
        if(scan.IsErrored() || gpuWrites.count(h.buffer) || !ApplyRayASHeaderWrite(h)) return reject(__LINE__);
        scan.EndChunk(); continue;
      }
      if(chunk == MetalChunk::MTLCommandQueue_commandBuffer ||
         chunk == MetalChunk::MTLCommandQueue_commandBufferWithDescriptor ||
         chunk == MetalChunk::MTLCommandQueue_commandBufferWithUnretainedReferences)
      {
        ResourceId queue, command;
        scan.Serialise("CommandQueue"_lit, queue); scan.Serialise("CommandBuffer"_lit, command);
        if(queue == ResourceId() || command == ResourceId() ||
           !commandQueues.emplace(command, queue).second) return reject(__LINE__);
        scan.EndChunk(); continue;
      }
      if(chunk == MetalChunk::MTLCommandBuffer_accelerationStructureCommandEncoder)
      {
        ResourceId command, encoder;
        scan.Serialise("CommandBuffer"_lit, command); scan.Serialise("Encoder"_lit, encoder);
        if(!commandQueues.count(command) || committedCommands.count(command) ||
           encoder == ResourceId() || !asEncoders.emplace(encoder, command).second ||
           (rayQueue != ResourceId() && rayQueue != commandQueues[command])) return reject(__LINE__);
        for(const auto &blit : blitEncoders) if(blit.second == command) return reject(__LINE__);
        for(auto compute : inlineLiveEncoders) if(states[compute].command == command) return reject(__LINE__);
        rayQueue = commandQueues[command]; pendingASCommands.insert(command);
        scan.EndChunk(); continue;
      }
      if(chunk == MetalChunk::MTLAccelerationStructureCommandEncoder_buildFrozenTriangles ||
         chunk == MetalChunk::MTLAccelerationStructureCommandEncoder_buildFrozenMultiIndexed)
      {
        ResourceId encoder, structure, vertices, indices, scratch;
        uint32_t kind = 0; uint64_t scratchOffset = 0;
        rdcarray<uint64_t> parameters; bytebuf vertexBytes, indexBytes;
        scan.Serialise("Encoder"_lit, encoder); scan.Serialise("structure"_lit, structure);
        scan.Serialise("vertices"_lit, vertices); scan.Serialise("indices"_lit, indices);
        scan.Serialise("kind"_lit, kind); scan.Serialise("parameters"_lit, parameters);
        scan.Serialise("scratch"_lit, scratch); scan.Serialise("scratchOffset"_lit, scratchOffset);
        scan.Serialise("vertexBytes"_lit, vertexBytes); scan.Serialise("indexBytes"_lit, indexBytes);
        if(scan.IsErrored() || !asEncoders.count(encoder) ||
           ((chunk == MetalChunk::MTLAccelerationStructureCommandEncoder_buildFrozenMultiIndexed) != (kind == 8)) ||
           (chunk == MetalChunk::MTLAccelerationStructureCommandEncoder_buildFrozenTriangles && kind != 1 && kind != 2) ||
           !RecordRayIRGeometryASBuild(structure, vertices, indices, kind, parameters,
               scratch, scratchOffset, vertexBytes, indexBytes)) return reject(__LINE__);
        scan.EndChunk(); continue;
      }
      if(chunk == MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstances ||
         chunk == MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstancesWithScratchOffset)
      {
        ResourceId encoder, structure, instances, scratch;
        rdcarray<ResourceId> children; rdcarray<uint64_t> parameters; bytebuf bytes; uint64_t scratchOffset = 0;
        scan.Serialise("Encoder"_lit, encoder); scan.Serialise("structure"_lit, structure);
        scan.Serialise("children"_lit, children); scan.Serialise("instances"_lit, instances);
        scan.Serialise("scratch"_lit, scratch); scan.Serialise("parameters"_lit, parameters);
        scan.Serialise("descriptorBytes"_lit, bytes);
        if(chunk == MetalChunk::MTLAccelerationStructureCommandEncoder_buildIndirectInstancesWithScratchOffset)
          scan.Serialise("scratchOffset"_lit, scratchOffset);
        if(scan.IsErrored() || !asEncoders.count(encoder) ||
           !RecordRayIRIndirectASBuild(structure, children, instances, scratch, parameters, bytes, scratchOffset))
          return reject(__LINE__);
        scan.EndChunk(); continue;
      }
      if(chunk == MetalChunk::MTLAccelerationStructureCommandEncoder_endEncoding)
      {
        ResourceId encoder; scan.Serialise("Encoder"_lit, encoder);
        if(!asEncoders.erase(encoder)) return reject(__LINE__);
        scan.EndChunk(); continue;
      }
      if(chunk == MetalChunk::MTLCommandBuffer_commit)
      {
        ResourceId command; scan.Serialise("CommandBuffer"_lit, command);
        if(!commandQueues.count(command) || !committedCommands.insert(command).second) return reject(__LINE__);
        for(const auto &encoder : asEncoders) if(encoder.second == command) return reject(__LINE__);
        for(const auto &blit : blitEncoders) if(blit.second == command) return reject(__LINE__);
        for(auto compute : inlineLiveEncoders) if(states[compute].command == command) return reject(__LINE__);
        pendingASCommands.erase(command);
        scan.EndChunk(); continue;
      }
      if(chunk == MetalChunk::MTLCommandBuffer_blitCommandEncoder ||
         chunk == MetalChunk::MTLCommandBuffer_blitCommandEncoderWithDescriptor)
      {
        ResourceId command, encoder;
        scan.Serialise("CommandBuffer"_lit, command); scan.Serialise("BlitCommandEncoder"_lit, encoder);
        if(!commandQueues.count(command) || committedCommands.count(command) ||
           encoder == ResourceId() || !blitEncoders.emplace(encoder, command).second ||
           (rayQueue != ResourceId() && rayQueue != commandQueues[command])) return reject(__LINE__);
        for(const auto &as : asEncoders) if(as.second == command) return reject(__LINE__);
        for(auto compute : inlineLiveEncoders) if(states[compute].command == command) return reject(__LINE__);
        rayQueue = commandQueues[command];
        scan.EndChunk(); continue;
      }
      if(chunk == MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toBuffer ||
         chunk == MetalChunk::MTLBlitCommandEncoder_fillBuffer)
      {
        ResourceId encoder, source, destination;
        uint64_t sourceOffset = 0, destinationOffset = 0, bytes = 0;
        scan.Serialise("BlitCommandEncoder"_lit, encoder);
        if(chunk == MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toBuffer)
        {
          scan.Serialise("sourceBuffer"_lit, source); scan.Serialise("sourceOffset"_lit, sourceOffset);
          scan.Serialise("destinationBuffer"_lit, destination); scan.Serialise("destinationOffset"_lit, destinationOffset);
          scan.Serialise("size"_lit, bytes);
        }
        else
        {
          NS::Range range = NS::Range::Make(0, 0); uint8_t value = 0;
          scan.Serialise("buffer"_lit, destination); scan.Serialise("range"_lit, range);
          scan.Serialise("value"_lit, value); destinationOffset = range.location; bytes = range.length;
        }
        auto validRange = [&](ResourceId buffer, uint64_t offset) {
          auto object = GetResourceManager()->GetResource(buffer, true);
          if(!object || object->m_Type != eResBuffer || !object->m_Real || object->m_CapturedAliasable) return false;
          auto native = Unwrap((WrappedMTLBuffer *)object);
          return offset <= native->length() && bytes <= native->length() - offset;
        };
        if(scan.IsErrored() || !blitEncoders.count(encoder) ||
           !validRange(destination, destinationOffset) ||
           (source != ResourceId() && !validRange(source, sourceOffset)) ||
           (chunk == MetalChunk::MTLBlitCommandEncoder_copyFromBuffer_toBuffer && source == ResourceId()))
          return reject(__LINE__);
        // Contribution backing on this typed query contract is immutable across
        // the frame. A GPU producer needs a separate per-use contents contract.
        if(bytes)
        {
          for(const auto &h:m_RayASHeaders) if(h.second.contributions==destination) return reject(__LINE__);
          for(const auto &h:m_RayASHeaderFrameWrites) if(h.contributions==destination) return reject(__LINE__);
          gpuWrites.insert(destination);
        }
        scan.EndChunk(); continue;
      }
      if(chunk == MetalChunk::MTLBlitCommandEncoder_endEncoding)
      {
        ResourceId encoder; scan.Serialise("BlitCommandEncoder"_lit, encoder);
        if(!blitEncoders.erase(encoder)) return reject(__LINE__);
        scan.EndChunk(); continue;
      }
      if(DescriptorChunkStartsWith(name, "MTLBlitCommandEncoder::"))
      {
        if(chunk != MetalChunk::MTLBlitCommandEncoder_setLabel &&
           chunk != MetalChunk::MTLBlitCommandEncoder_pushDebugGroup &&
           chunk != MetalChunk::MTLBlitCommandEncoder_popDebugGroup &&
           chunk != MetalChunk::MTLBlitCommandEncoder_insertDebugSignpost) return reject(__LINE__);
        ResourceId encoder; scan.Serialise("BlitCommandEncoder"_lit, encoder);
        if(!blitEncoders.count(encoder)) return reject(__LINE__);
        scan.EndChunk(); continue;
      }
      if(chunk == MetalChunk::MTLBuffer_InternalModifyCPUContents)
      {
        ResourceId buffer; uint64_t start = 0, size = 0; bytebuf data;
        scan.Serialise("Buffer"_lit, buffer); scan.Serialise("start"_lit, start);
        scan.Serialise("size"_lit, size); scan.Serialise("data"_lit, data);
        auto initial = m_ReplayBufferInitialContents.find(buffer);
        if(initial == m_ReplayBufferInitialContents.end() &&
           m_ReplayBuffersWithCreationContents.count(buffer))
        {
          auto object = GetResourceManager()->GetResource(buffer, true);
          if(!object || object->m_Type != eResBuffer || !object->m_Real ||
             object->m_CapturedAliasable) return reject(__LINE__);
          auto native = Unwrap((WrappedMTLBuffer *)object);
          if(native->storageMode() != MTL::StorageModeShared || native->heap() ||
             !native->contents() || native->length() > 64*1024) return reject(__LINE__);
          // An untouched creation packet is authoritative even for a blit-only
          // upload/readback buffer that no ray dispatch directly references.
          m_ReplayBufferInitialContents[buffer] = bytebuf((byte *)native->contents(), native->length());
          m_ReplayCPUUpdatedBuffers.insert(buffer);
          initial = m_ReplayBufferInitialContents.find(buffer);
        }
        // Commit-time coherent snapshots are recorded even when unchanged.
        // Accept their original bytes, but reject actual ABI mutations.
        auto current=m_DescriptorRawContents.find(buffer);
        const bytebuf *expected=initial==m_ReplayBufferInitialContents.end() ? NULL : &initial->second;
        bool header=false; for(const auto &h:m_RayASHeaders) header |= h.second.buffer==buffer;
        if(header && current!=m_DescriptorRawContents.end()) expected=&current->second;
        if(scan.IsErrored() || !expected || size != data.size() || start > expected->size() ||
           size > expected->size()-start ||
           (size && memcmp(expected->data()+start,data.data(),size))) return reject(__LINE__);
        scan.EndChunk(); continue;
      }
      if(chunk == MetalChunk::MTLCommandBuffer_renderCommandEncoderWithDescriptor ||
         chunk == MetalChunk::MTLCommandBuffer_parallelRenderCommandEncoderWithDescriptor ||
         DescriptorChunkStartsWith(name, "MTLRenderCommandEncoder::") ||
         DescriptorChunkStartsWith(name, "MTLIndirect") ||
         chunk == MetalChunk::MTLComputePipelineState_DeclareRayIRHeapEntry ||
         chunk == MetalChunk::MTLComputePipelineState_DeclareRayIRGlobalRoot ||
         chunk == MetalChunk::MTLFunctionHandle_DeclareRayIRLocalRoot ||
         chunk == MetalChunk::MTLFunctionHandle_DeclareRayIRShaderRole ||
         chunk == MetalChunk::MTLBuffer_DescriptorCPUWrite ||
         DescriptorChunkStartsWith(name, "MTLAccelerationStructureCommandEncoder::") ||
         DescriptorChunkStartsWith(name, "MTLVisibleFunctionTable::set") ||
         DescriptorChunkStartsWith(name, "MTLIntersectionFunctionTable::set")) return reject(__LINE__);
    }
    if(chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoder ||
       chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDispatchType ||
       chunk == MetalChunk::MTLCommandBuffer_computeCommandEncoderWithDescriptor)
    {
      ResourceId command, encoder;
      scan.Serialise("CommandBuffer"_lit, command);
      scan.Serialise("ComputeCommandEncoder"_lit, encoder);
      if(encoder == ResourceId() || !inlineLiveEncoders.insert(encoder).second)
        return reject(__LINE__);
      states[encoder].command = command;
      if((!m_RayIRDispatches.empty() || !m_RayQueryDispatches.empty()))
      {
        if(!commandQueues.count(command) || committedCommands.count(command) ||
           (rayQueue != ResourceId() && rayQueue != commandQueues[command])) return reject(__LINE__);
        for(const auto &as : asEncoders) if(as.second == command) return reject(__LINE__);
        for(const auto &blit : blitEncoders) if(blit.second == command) return reject(__LINE__);
        rayQueue = commandQueues[command];
      }
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
        return reject(__LINE__);
      WrappedMTLHeap *wrapped = (WrappedMTLHeap *)object;
      MTL::Heap *native = Unwrap(wrapped);
      const MTL::SizeAndAlign allocation = Unwrap(this)->heapBufferSizeAndAlign(length, (MTL::ResourceOptions)options);
      const MTL::StorageMode storage = (options & MTL::ResourceStorageModePrivate) ?
          MTL::StorageModePrivate : MTL::StorageModeShared;
      if(native->type() != MTL::HeapTypePlacement || native->storageMode() != storage ||
         !allocation.size || !allocation.align || offset % allocation.align || offset > native->size() ||
         allocation.size > native->size() - offset || wrapped->HasPlacementOverlap(offset, offset + allocation.size))
        return reject(__LINE__);
      for(const auto &range : futureHeapRanges[heap])
        if(offset < range.second && range.first < offset + allocation.size)
          return reject(__LINE__);
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
        return reject(__LINE__);
      uint64_t length = 0;
      MTL::StorageMode storage = MTL::StorageModeShared;
      auto future = m_DescriptorFrameBuffers.find(buffer);
      if(future != m_DescriptorFrameBuffers.end())
      {
        if(!m_DescriptorPreflightLiveBuffers.count(buffer))
          return reject(__LINE__);
        length = future->second.length;
        storage = (future->second.options & MTL::ResourceStorageModePrivate) ?
            MTL::StorageModePrivate : MTL::StorageModeShared;
      }
      else
      {
        WrappedMTLObject *object = GetResourceManager()->GetResource(buffer, true);
        if(!object || object->m_Type != eResBuffer || !object->m_Real)
          return reject(__LINE__);
        length = Unwrap((WrappedMTLBuffer *)object)->length();
        storage = Unwrap((WrappedMTLBuffer *)object)->storageMode();
      }
      if(!ValidateMetalBufferTexture(Unwrap(this), descriptor, length, storage, offset, bytesPerRow))
        return reject(__LINE__);
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
        return reject(__LINE__);
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
          return reject(__LINE__);
        if(start > m_DescriptorRawContents[buffer].size() ||
           size > m_DescriptorRawContents[buffer].size() - start)
          return reject(__LINE__);
        bytebuf data;
        scan.Serialise("data"_lit, data);
        if(scan.IsErrored() || size != data.size() || !ApplyDescriptorCPUUpdate(buffer, start, data))
          return reject(__LINE__);
      }
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_DeclareDescriptorBytes)
    {
      ResourceId encoder;
      uint64_t index = 0;
      scan.Serialise("encoder"_lit, encoder); scan.Serialise("index"_lit, index);
      if(!inlineLiveEncoders.count(encoder) || !inlineDeclared[encoder].insert(index).second)
        return reject(__LINE__);
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
        return reject(__LINE__);
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
        return reject(__LINE__);
      states[encoder].buffers[(uint32_t)index] = buffer;
      states[encoder].offsets[(uint32_t)index] = offset;
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_dispatchThreadgroups)
    {
      ResourceId encoder;
      scan.Serialise("ComputeCommandEncoder"_lit, encoder);
      if((!m_RayIRDispatches.empty() || !m_RayQueryDispatches.empty()))
      {
        for(auto command : pendingASCommands)
          if(command != states[encoder].command) return reject(__LINE__);
        if(!asEncoders.empty() || !inlineLiveEncoders.count(encoder)) return reject(__LINE__);
        MTL::Size groups, threads;
        scan.Serialise("groups"_lit, groups); scan.Serialise("threadsPerGroup"_lit, threads);
        const bool query = HasRayQueryPipeline(states[encoder].pipeline);
        if(query ? !ValidateRayQueryDispatch(states[encoder].pipeline, states[encoder].buffers[2],
                     states[encoder].offsets[2], groups, threads) :
                   (!HasRayIRPipeline(states[encoder].pipeline) ||
                    !ValidateRayIRDispatch(states[encoder].pipeline, states[encoder].buffers[3],
                     states[encoder].offsets[3], groups, threads))) return reject(__LINE__);
        rayInputs.insert(m_RayIRReadResources.begin(), m_RayIRReadResources.end());
        gpuWrites.insert(m_RayIROutput);
      }
      for(const auto &binding : states[encoder].buffers)
        if(tables.count(binding.second) &&
           !m_DescriptorGPUWrittenBuffers.count(binding.second) &&
           !GetReplay()->IsComputeBufferReadOnly(states[encoder].pipeline, binding.first))
        {
          RDCERR("GPU write access to explicit descriptor tables is unsupported in v1");
          return reject(__LINE__);
        }
    }
    else if(chunk == MetalChunk::MTLComputeCommandEncoder_setBuffers ||
            chunk == MetalChunk::MTLComputeCommandEncoder_setBufferOffset ||
            chunk == MetalChunk::MTLBuffer_didModifyRange)
      return reject(__LINE__);
    else
    {
      const rdcstr name = ToStr(chunk);
      if(strstr(name.c_str(), "MTLComputeCommandEncoder::dispatch"))
        return reject(__LINE__);
      if(strstr(name.c_str(), "::draw") || strstr(name.c_str(), "makeAliasable") ||
         strstr(name.c_str(), "setPurgeableState") ||
         DescriptorChunkStartsWith(name, "MTLDevice::newBuffer") ||
         DescriptorChunkStartsWith(name, "MTLDevice::newTexture") ||
         DescriptorChunkStartsWith(name, "MTLDevice::newSampler") ||
         DescriptorChunkStartsWith(name, "MTLHeap::newBuffer") ||
         DescriptorChunkStartsWith(name, "MTLHeap::newTexture"))
        return reject(__LINE__);
      // v1 is a compute-read path. Blit/render/ICB accesses to table allocations require
      // separate execution-point semantics; conservatively reject before the loading pass.
      if(DescriptorChunkStartsWith(name, "MTLBlitCommandEncoder::") ||
         DescriptorChunkStartsWith(name, "MTLRenderCommandEncoder::") ||
         DescriptorChunkStartsWith(name, "MTLIndirect"))
      {
        bytebuf payload;
        payload.resize(scan.ChunkMetadata().length);
        if(!m_FrameReader->Read(payload.data(), payload.size()))
          return reject(__LINE__);
        for(size_t i = 0; i + sizeof(ResourceId) <= payload.size(); i++)
        {
          ResourceId id;
          memcpy(&id, payload.data() + i, sizeof(id));
          if(tables.count(id))
            return reject(__LINE__);
        }
      }
    }
    scan.EndChunk();
  }
  // Every dispatch consumes immutable IR inputs. Check the complete frame so a
  // GPU write before the first use is rejected too, including scalar contribution
  // buffers without descriptor fields. Use typed destinations, never payload integers.
  for(ResourceId written : gpuWrites) if(rayInputs.count(written)) return reject(__LINE__);
  const bool valid = !scan.IsErrored() && m_RayASHeaderFrameCursor==m_RayASHeaderFrameWrites.size() &&
      asEncoders.empty() && pendingASCommands.empty() &&
      ((m_RayIRDispatches.empty() && m_RayQueryDispatches.empty()) || (blitEncoders.empty() && inlineLiveEncoders.empty()));
  m_FrameReader->SetOffset(0);
  return valid;
}
