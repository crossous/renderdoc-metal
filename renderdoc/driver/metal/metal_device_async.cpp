// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_device.h"
#include "metal_library.h"
#include "metal_dynamic_library.h"
#include "metal_binary_archive.h"
#include "metal_function.h"
#include "metal_render_pipeline_state.h"
#include "metal_compute_pipeline_state.h"

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_asyncLibrary(SerialiserType &ser, WrappedMTLLibrary *library,
                                             NS::String *source, MTL::CompileOptions *options,
                                             bool supported)
{
  // The synchronous source-library contract serializes the supported compile-options subset.
  SERIALISE_ELEMENT(supported);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && !supported)
  {
    RDCERR("Unsupported Metal asynchronous source library compile options");
    return false;
  }
  return Serialise_newLibraryWithSource(ser, library, source, options, NULL);
}

WrappedMTLLibrary *WrappedMTLDevice::CaptureAsyncLibrary(MTL::Library *real, NS::String *source,
                                                        MTL::CompileOptions *options, bool supported)
{
  if(!real) return NULL;
  // Completion arguments are borrowed. The returned proxy owns one native reference and has +1;
  // the bridge releases its temporary proxy reference after the application callback returns.
  real->retain();
  WrappedMTLLibrary *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newLibraryWithSource_async);
    Serialise_asyncLibrary(ser, wrapped, source, options, supported);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    NS::Array *dependencies = options ? options->libraries() : NULL;
    for(NS::UInteger i = 0; dependencies && i < dependencies->count(); i++)
    {
      MTL::DynamicLibrary *dependency = dependencies->object<MTL::DynamicLibrary>(i);
      if(MetalDynamicLibraryIsWrapped(dependency))
        record->AddParent(GetRecord(GetWrapped(dependency)));
    }
  }
  return wrapped;
}

WrappedMTLRenderPipelineState *WrappedMTLDevice::CaptureAsyncRenderPipeline(
    MTL::RenderPipelineState *real, MTL::RenderPipelineDescriptor *descriptor,
    MTL::PipelineOption options, MetalChunk chunk)
{
  if(!real) return NULL;
  real->retain();
  WrappedMTLRenderPipelineState *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    RDMTL::RenderPipelineDescriptor captured(descriptor);
    bool archivesValid = captured.binaryArchives.size() == descriptor->binaryArchives()->count() &&
                         captured.binaryArchives.size() <= 8;
    for(WrappedMTLBinaryArchive *archive : captured.binaryArchives)
      archivesValid &= archive && archive->m_Type == eResBinaryArchive && archive->m_Real;
    const bool supported = archivesValid &&
                           captured.vertexPreloadedLibraries.size() ==
                               descriptor->vertexPreloadedLibraries()->count() &&
                           captured.fragmentPreloadedLibraries.size() ==
                               descriptor->fragmentPreloadedLibraries()->count() &&
                           captured.vertexPreloadedLibraries.size() <= 8 &&
                           captured.fragmentPreloadedLibraries.size() <= 8;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_newRenderPipelineStateWithDescriptorOptions(ser, wrapped, captured, options, supported);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    if(captured.vertexFunction) record->AddParent(GetRecord(captured.vertexFunction));
    if(captured.fragmentFunction) record->AddParent(GetRecord(captured.fragmentFunction));
    for(WrappedMTLDynamicLibrary *library : captured.vertexPreloadedLibraries)
      record->AddParent(GetRecord(library));
    for(WrappedMTLDynamicLibrary *library : captured.fragmentPreloadedLibraries)
      record->AddParent(GetRecord(library));
    for(WrappedMTLBinaryArchive *archive : captured.binaryArchives)
      if(archive) record->AddParent(GetRecord(archive));
    for(WrappedMTLFunction *function : captured.vertexLinkedFunctions.functions)
      if(function) record->AddParent(GetRecord(function));
    for(WrappedMTLFunction *function : captured.fragmentLinkedFunctions.functions)
      if(function) record->AddParent(GetRecord(function));
  }
  return wrapped;
}

WrappedMTLRenderPipelineState *WrappedMTLDevice::CaptureAsyncTilePipeline(
    MTL::RenderPipelineState *real, MTL::TileRenderPipelineDescriptor *descriptor,
    WrappedMTLFunction *tileFunction, MTL::PipelineOption options, bool supported)
{
  if(!real) return NULL;
  real->retain();
  WrappedMTLRenderPipelineState *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    rdcarray<uint32_t> colorFormats;
    for(uint32_t i = 0; i < 8; i++)
      colorFormats.push_back((uint32_t)descriptor->colorAttachments()->object(i)->pixelFormat());
    RDMTL::LinkedFunctions links(descriptor->linkedFunctions());
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newRenderPipelineStateWithTileDescriptor_async);
    Serialise_newTileRenderPipelineState(ser, wrapped, tileFunction, colorFormats,
        descriptor->rasterSampleCount(), descriptor->maxTotalThreadsPerThreadgroup(),
        descriptor->threadgroupSizeMatchesTileSize(), (uint32_t)options, supported,
        links.functions);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    if(tileFunction) record->AddParent(GetRecord(tileFunction));
    for(WrappedMTLFunction *function : links.functions)
      if(function) record->AddParent(GetRecord(function));
  }
  return wrapped;
}

WrappedMTLRenderPipelineState *WrappedMTLDevice::CaptureAsyncMeshPipeline(
    MTL::RenderPipelineState *real, MTL::MeshRenderPipelineDescriptor *descriptor,
    WrappedMTLFunction *objectFunction, WrappedMTLFunction *meshFunction,
    WrappedMTLFunction *fragmentFunction, MTL::PipelineOption options, bool supported)
{
  if(!real) return NULL;
  real->retain();
  WrappedMTLRenderPipelineState *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    rdcarray<uint32_t> colorFormats;
    for(uint32_t i = 0; i < 8; i++)
      colorFormats.push_back((uint32_t)descriptor->colorAttachments()->object(i)->pixelFormat());
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(objectFunction ?
        MetalChunk::MTLDevice_newRenderPipelineStateWithObjectMeshDescriptor_async :
        MetalChunk::MTLDevice_newRenderPipelineStateWithMeshDescriptor_async);
    if(objectFunction)
      Serialise_newObjectMeshPipelineState(ser, wrapped, objectFunction, meshFunction,
          fragmentFunction, colorFormats, descriptor->rasterSampleCount(),
          descriptor->maxTotalThreadsPerObjectThreadgroup(),
          descriptor->maxTotalThreadsPerMeshThreadgroup(), descriptor->payloadMemoryLength(),
          descriptor->maxTotalThreadgroupsPerMeshGrid(), (uint32_t)options, supported);
    else
      Serialise_newMeshRenderPipelineState(ser, wrapped, objectFunction, meshFunction,
          fragmentFunction, colorFormats, descriptor->rasterSampleCount(),
          descriptor->maxTotalThreadsPerMeshThreadgroup(), (uint32_t)options, supported,
          descriptor->maxTotalThreadgroupsPerMeshGrid());
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    if(objectFunction) record->AddParent(GetRecord(objectFunction));
    if(meshFunction) record->AddParent(GetRecord(meshFunction));
    if(fragmentFunction) record->AddParent(GetRecord(fragmentFunction));
  }
  return wrapped;
}

WrappedMTLComputePipelineState *WrappedMTLDevice::CaptureAsyncComputePipeline(
    MTL::ComputePipelineState *real, WrappedMTLFunction *function, MTL::PipelineOption options,
    MetalChunk chunk)
{
  if(!real) return NULL;
  real->retain();
  WrappedMTLComputePipelineState *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_newComputePipelineStateWithFunctionOptions(ser, wrapped, function, options, NULL, NULL);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    if(function) record->AddParent(GetRecord(function));
  }
  return wrapped;
}

WrappedMTLComputePipelineState *WrappedMTLDevice::CaptureAsyncComputeDescriptor(
    MTL::ComputePipelineState *real, MTL::ComputePipelineDescriptor *descriptor,
    MTL::PipelineOption options)
{
  if(!real) return NULL;
  real->retain();
  WrappedMTLComputePipelineState *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    RDMTL::ComputePipelineDescriptor captured(descriptor);
    bool archivesValid = captured.binaryArchives.size() == descriptor->binaryArchives()->count() &&
                         captured.binaryArchives.size() <= 8;
    for(WrappedMTLBinaryArchive *archive : captured.binaryArchives)
      archivesValid &= archive && archive->m_Type == eResBinaryArchive && archive->m_Real;
    const bool supported = archivesValid &&
                           captured.preloadedLibraries.size() ==
                               descriptor->preloadedLibraries()->count() &&
                           captured.preloadedLibraries.size() <= 8;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newComputePipelineStateWithDescriptor_async);
    Serialise_newComputePipelineStateWithDescriptor(ser, wrapped, captured, options, supported);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    if(captured.computeFunction) record->AddParent(GetRecord(captured.computeFunction));
    for(WrappedMTLDynamicLibrary *library : captured.preloadedLibraries)
      record->AddParent(GetRecord(library));
    for(WrappedMTLBinaryArchive *archive : captured.binaryArchives)
      if(archive) record->AddParent(GetRecord(archive));
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_asyncLibrary(ReadSerialiser &, WrappedMTLLibrary *, NS::String *, MTL::CompileOptions *, bool);
template bool WrappedMTLDevice::Serialise_asyncLibrary(WriteSerialiser &, WrappedMTLLibrary *, NS::String *, MTL::CompileOptions *, bool);
