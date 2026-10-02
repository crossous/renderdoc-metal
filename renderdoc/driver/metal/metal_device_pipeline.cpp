// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_device.h"
#include "metal_dynamic_library.h"
#include "metal_binary_archive.h"
#include "metal_compute_pipeline_state.h"
#include "metal_function.h"
#include "metal_render_pipeline_state.h"
#include "metal_replay.h"

namespace
{
bool ValidOptions(MTL::PipelineOption options)
{
  // Reflection is regenerated on replay, but archive-miss policy must be preserved.
  return ((uint64_t)options & ~uint64_t(7)) == 0;
}

bool ValidArchives(const rdcarray<WrappedMTLBinaryArchive *> &archives)
{
  if(archives.size() > 8) return false;
  for(WrappedMTLBinaryArchive *archive : archives)
    if(!archive || archive->m_Type != eResBinaryArchive || !archive->m_Real)
      return false;
  return true;
}

bool ValidVisibleLinks(const RDMTL::LinkedFunctions &links)
{
  if(links.functions.size() > 8 || !links.binaryFunctions.empty() || !links.groups.empty() ||
     !links.privateFunctions.empty())
    return false;
  for(WrappedMTLFunction *function : links.functions)
    if(!function || function->m_Type != eResFunction || !function->m_Real ||
       Unwrap(function)->functionType() != MTL::FunctionTypeVisible)
      return false;
  return true;
}

bool ValidIntersectionLinks(const RDMTL::LinkedFunctions &links)
{
  if(links.functions.size() > 32 || !links.binaryFunctions.empty() || !links.groups.empty() ||
     !links.privateFunctions.empty())
    return false;
  for(WrappedMTLFunction *function : links.functions)
    if(!function || function->m_Type != eResFunction || !function->m_Real ||
       (Unwrap(function)->functionType() != MTL::FunctionTypeIntersection &&
        Unwrap(function)->functionType() != MTL::FunctionTypeVisible))
      return false;
  return true;
}


bool ValidBuffers(const rdcarray<RDMTL::PipelineBufferDescriptor> &buffers)
{
  if(buffers.size() > 31)
    return false;
  for(const auto &buffer : buffers)
    if((uint64_t)buffer.mutability > 2)
      return false;
  return true;
}

bool ValidPreloadedLibraries(const rdcarray<WrappedMTLDynamicLibrary *> &libraries)
{
  if(libraries.size() > 8) return false;
  for(WrappedMTLDynamicLibrary *library : libraries)
    if(!library || library->m_Type != eResDynamicLibrary || !library->m_Real)
      return false;
  return true;
}
}

bool ValidateMetalPipelineFunction(WrappedMTLFunction *function, MTL::FunctionType type)
{
  return function && function->m_Type == eResFunction && function->m_Real &&
         Unwrap(function)->functionType() == type;
}


template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newRenderPipelineStateWithDescriptorOptions(
    SerialiserType &ser, WrappedMTLRenderPipelineState *pipeline,
    RDMTL::RenderPipelineDescriptor &descriptor, MTL::PipelineOption options, bool supported)
{
  SERIALISE_ELEMENT_LOCAL(optionsValue, (uint64_t)options).Important();
  SERIALISE_ELEMENT(supported);
  SERIALISE_ELEMENT_LOCAL(RenderPipelineState, GetResID(pipeline))
      .TypedAs("MTLRenderPipelineState"_lit);
  SERIALISE_ELEMENT(descriptor);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!supported || !ValidOptions((MTL::PipelineOption)optionsValue) ||
       RenderPipelineState == ResourceId() ||
       GetResourceManager()->HasResource(RenderPipelineState) ||
       !ValidateMetalPipelineFunction(descriptor.vertexFunction, MTL::FunctionTypeVertex) ||
       (descriptor.fragmentFunction &&
        !ValidateMetalPipelineFunction(descriptor.fragmentFunction, MTL::FunctionTypeFragment)) ||
       !ValidVisibleLinks(descriptor.vertexLinkedFunctions) ||
       !ValidVisibleLinks(descriptor.fragmentLinkedFunctions) ||
       !ValidBuffers(descriptor.vertexBuffers) || !ValidBuffers(descriptor.fragmentBuffers) ||
       !ValidPreloadedLibraries(descriptor.vertexPreloadedLibraries) ||
       !ValidPreloadedLibraries(descriptor.fragmentPreloadedLibraries) ||
       !ValidArchives(descriptor.binaryArchives) ||
       descriptor.vertexDescriptor.attributes.size() > 31 ||
       descriptor.vertexDescriptor.layouts.size() > 31 || descriptor.colorAttachments.size() > 8 ||
       descriptor.maxVertexAmplificationCount != 1 ||
       descriptor.supportAddingVertexBinaryFunctions ||
       descriptor.supportAddingFragmentBinaryFunctions ||
       !Unwrap(this)->supportsTextureSampleCount(descriptor.rasterSampleCount) ||
       !Unwrap(this)->supportsTextureSampleCount(descriptor.sampleCount))
    {
      RDCERR("Invalid or unsupported Metal render pipeline options/descriptor");
      return false;
    }
    MTL::RenderPipelineDescriptor *realDescriptor(descriptor);
    MTL::AutoreleasedRenderPipelineReflection reflection = NULL;
    NS::Error *error = NULL;
    MTL::PipelineOption replayOptions = (MTL::PipelineOption)(
        (uint64_t)MTL::PipelineOptionArgumentInfo | (optionsValue & 4));
    MTL::RenderPipelineState *real = Unwrap(this)->newRenderPipelineState(
        realDescriptor, replayOptions, &reflection, &error);
    realDescriptor->release();
    if(!real)
    {
      RDCERR("Failed to recreate Metal render pipeline with options");
      return false;
    }
    WrappedMTLRenderPipelineState *wrapped = NULL;
    GetResourceManager()->WrapResource(RenderPipelineState, real, wrapped, true);
    AddResource(RenderPipelineState, ResourceType::PipelineState, "Pipeline State");
    GetReplay()->AddRenderPipeline(RenderPipelineState, descriptor, reflection);
    DerivedResource(descriptor.vertexFunction, RenderPipelineState);
    if(descriptor.fragmentFunction)
      DerivedResource(descriptor.fragmentFunction, RenderPipelineState);
    for(WrappedMTLDynamicLibrary *library : descriptor.vertexPreloadedLibraries)
      DerivedResource(library, RenderPipelineState);
    for(WrappedMTLDynamicLibrary *library : descriptor.fragmentPreloadedLibraries)
      DerivedResource(library, RenderPipelineState);
    for(WrappedMTLBinaryArchive *archive : descriptor.binaryArchives)
      DerivedResource(archive, RenderPipelineState);
    for(WrappedMTLFunction *function : descriptor.vertexLinkedFunctions.functions)
      DerivedResource(function, RenderPipelineState);
    for(WrappedMTLFunction *function : descriptor.fragmentLinkedFunctions.functions)
      DerivedResource(function, RenderPipelineState);
  }
  return true;
}

WrappedMTLRenderPipelineState *WrappedMTLDevice::newRenderPipelineStateWithDescriptorOptions(
    MTL::RenderPipelineDescriptor *descriptor, MTL::PipelineOption options,
    MTL::AutoreleasedRenderPipelineReflection *reflection, NS::Error **error)
{
  RDMTL::RenderPipelineDescriptor captured(descriptor);
  // Copy the original descriptor so native creation retains properties not yet serialised.
  MTL::RenderPipelineDescriptor *realDescriptor = descriptor->copy();
  realDescriptor->setVertexFunction(Unwrap(captured.vertexFunction));
  realDescriptor->setFragmentFunction(Unwrap(captured.fragmentFunction));
  NS::Array *archives = descriptor->binaryArchives();
  if(archives && archives->count())
  {
    rdcarray<MTL::BinaryArchive *> native;
    for(NS::UInteger i = 0; i < archives->count(); i++)
    {
      MTL::BinaryArchive *archive = archives->object<MTL::BinaryArchive>(i);
      native.push_back(MetalBinaryArchiveIsWrapped(archive) ? Unwrap(GetWrapped(archive)) : archive);
    }
    realDescriptor->setBinaryArchives(NS::Array::array(
        (const NS::Object *const *)native.data(), native.size()));
  }
  NS::Array *vertexPreloaded = descriptor->vertexPreloadedLibraries();
  if(vertexPreloaded && vertexPreloaded->count())
  {
    rdcarray<MTL::DynamicLibrary *> native;
    for(NS::UInteger i = 0; i < vertexPreloaded->count(); i++)
    {
      MTL::DynamicLibrary *library = vertexPreloaded->object<MTL::DynamicLibrary>(i);
      native.push_back(MetalDynamicLibraryIsWrapped(library) ? Unwrap(GetWrapped(library)) : library);
    }
    realDescriptor->setVertexPreloadedLibraries(NS::Array::array(
        (const NS::Object *const *)native.data(),native.size()));
  }
  NS::Array *fragmentPreloaded = descriptor->fragmentPreloadedLibraries();
  if(fragmentPreloaded && fragmentPreloaded->count())
  {
    rdcarray<MTL::DynamicLibrary *> native;
    for(NS::UInteger i = 0; i < fragmentPreloaded->count(); i++)
    {
      MTL::DynamicLibrary *library = fragmentPreloaded->object<MTL::DynamicLibrary>(i);
      native.push_back(MetalDynamicLibraryIsWrapped(library) ? Unwrap(GetWrapped(library)) : library);
    }
    realDescriptor->setFragmentPreloadedLibraries(NS::Array::array(
        (const NS::Object *const *)native.data(),native.size()));
  }
  MTL::LinkedFunctions *vertexLinks =
      RDMTL::MetalNativeLinkedFunctions(descriptor->vertexLinkedFunctions());
  MTL::LinkedFunctions *fragmentLinks =
      RDMTL::MetalNativeLinkedFunctions(descriptor->fragmentLinkedFunctions());
  realDescriptor->setVertexLinkedFunctions(vertexLinks);
  realDescriptor->setFragmentLinkedFunctions(fragmentLinks);
  vertexLinks->release();
  fragmentLinks->release();
  MTL::RenderPipelineState *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newRenderPipelineState(
                          realDescriptor, options, reflection, error));
  realDescriptor->release();
  if(!real)
    return NULL;
  WrappedMTLRenderPipelineState *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    const bool supported = captured.binaryArchives.size() == descriptor->binaryArchives()->count() &&
                           ValidArchives(captured.binaryArchives) &&
                           captured.vertexPreloadedLibraries.size() ==
                               descriptor->vertexPreloadedLibraries()->count() &&
                           captured.fragmentPreloadedLibraries.size() ==
                               descriptor->fragmentPreloadedLibraries()->count() &&
                           captured.vertexPreloadedLibraries.size() <= 8 &&
                           captured.fragmentPreloadedLibraries.size() <= 8;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newRenderPipelineStateWithDescriptor_options);
    Serialise_newRenderPipelineStateWithDescriptorOptions(ser, wrapped, captured, options, supported);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    if(captured.vertexFunction)
      record->AddParent(GetRecord(captured.vertexFunction));
    if(captured.fragmentFunction)
      record->AddParent(GetRecord(captured.fragmentFunction));
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

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newComputePipelineStateWithFunctionOptions(
    SerialiserType &ser, WrappedMTLComputePipelineState *pipeline,
    WrappedMTLFunction *computeFunction, MTL::PipelineOption options,
    MTL::AutoreleasedComputePipelineReflection *reflection, NS::Error **error)
{
  SERIALISE_ELEMENT_LOCAL(optionsValue, (uint64_t)options).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading() && !ValidOptions((MTL::PipelineOption)optionsValue))
  {
    RDCERR("Invalid or unsupported Metal compute pipeline options");
    return false;
  }
  return Serialise_newComputePipelineStateWithFunction(ser, pipeline, computeFunction, error);
}

WrappedMTLComputePipelineState *WrappedMTLDevice::newComputePipelineStateWithFunctionOptions(
    WrappedMTLFunction *computeFunction, MTL::PipelineOption options,
    MTL::AutoreleasedComputePipelineReflection *reflection, NS::Error **error)
{
  MTL::ComputePipelineState *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newComputePipelineState(
                          Unwrap(computeFunction), options, reflection, error));
  if(!real)
    return NULL;
  WrappedMTLComputePipelineState *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newComputePipelineStateWithFunction_options);
    Serialise_newComputePipelineStateWithFunctionOptions(ser, wrapped, computeFunction, options,
                                                        reflection, error);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(computeFunction));
  }
  return wrapped;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newComputePipelineStateWithDescriptor(
    SerialiserType &ser, WrappedMTLComputePipelineState *pipeline,
    RDMTL::ComputePipelineDescriptor &descriptor, MTL::PipelineOption options, bool supported)
{
  SERIALISE_ELEMENT_LOCAL(ComputePipelineState, GetResID(pipeline))
      .TypedAs("MTLComputePipelineState"_lit);
  SERIALISE_ELEMENT(descriptor);
  SERIALISE_ELEMENT_LOCAL(optionsValue, (uint64_t)options).Important();
  SERIALISE_ELEMENT(supported);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!supported || !ValidOptions((MTL::PipelineOption)optionsValue) ||
       ComputePipelineState == ResourceId() ||
       GetResourceManager()->HasResource(ComputePipelineState) ||
       !ValidateMetalPipelineFunction(descriptor.computeFunction, MTL::FunctionTypeKernel) ||
       !ValidIntersectionLinks(descriptor.linkedFunctions) || !ValidBuffers(descriptor.buffers) ||
       !ValidPreloadedLibraries(descriptor.preloadedLibraries) ||
       !ValidArchives(descriptor.binaryArchives) ||
       descriptor.maxTotalThreadsPerThreadgroup > Unwrap(this)->maxThreadsPerThreadgroup().width ||
       descriptor.maxCallStackDepth != 1 || descriptor.supportAddingBinaryFunctions ||
       descriptor.supportIndirectCommandBuffers ||
       !descriptor.stageInputDescriptor.attributes.empty() ||
       !descriptor.stageInputDescriptor.layouts.empty() ||
       descriptor.stageInputDescriptor.indexBufferIndex != 0 ||
       descriptor.stageInputDescriptor.indexType != MTL::IndexTypeUInt16)
    {
      RDCERR("Invalid or unsupported Metal compute pipeline descriptor/options");
      return false;
    }
    MTL::ComputePipelineDescriptor *realDescriptor(descriptor);
    MTL::AutoreleasedComputePipelineReflection reflection = NULL;
    NS::Error *error = NULL;
    MTL::PipelineOption replayOptions = (MTL::PipelineOption)(
        (uint64_t)MTL::PipelineOptionArgumentInfo | (optionsValue & 4));
    MTL::ComputePipelineState *real = Unwrap(this)->newComputePipelineState(
        realDescriptor, replayOptions, &reflection, &error);
    realDescriptor->release();
    if(!real)
    {
      RDCERR("Failed to recreate Metal compute pipeline descriptor");
      return false;
    }
    WrappedMTLComputePipelineState *wrapped = NULL;
    GetResourceManager()->WrapResource(ComputePipelineState, real, wrapped, true);
    AddResource(ComputePipelineState, ResourceType::PipelineState, "Compute Pipeline State");
    DerivedResource(descriptor.computeFunction, ComputePipelineState);
    for(WrappedMTLDynamicLibrary *library : descriptor.preloadedLibraries)
      DerivedResource(library, ComputePipelineState);
    for(WrappedMTLBinaryArchive *archive : descriptor.binaryArchives)
      DerivedResource(archive, ComputePipelineState);
    for(WrappedMTLFunction *function : descriptor.linkedFunctions.functions)
      DerivedResource(function, ComputePipelineState);
    GetReplay()->AddComputePipeline(ComputePipelineState, GetResID(descriptor.computeFunction),
                                   reflection, real,
                                   descriptor.threadGroupSizeIsMultipleOfThreadExecution);
  }
  return true;
}

WrappedMTLComputePipelineState *WrappedMTLDevice::newComputePipelineStateWithDescriptor(
    MTL::ComputePipelineDescriptor *descriptor, MTL::PipelineOption options,
    MTL::AutoreleasedComputePipelineReflection *reflection, NS::Error **error)
{
  RDMTL::ComputePipelineDescriptor captured(descriptor);
  MTL::ComputePipelineDescriptor *realDescriptor = descriptor->copy();
  realDescriptor->setComputeFunction(Unwrap(captured.computeFunction));
  if(descriptor->binaryArchives() && descriptor->binaryArchives()->count())
  {
    rdcarray<MTL::BinaryArchive *> native;
    NS::Array *archives = descriptor->binaryArchives();
    for(NS::UInteger i = 0; i < archives->count(); i++)
    {
      MTL::BinaryArchive *archive = archives->object<MTL::BinaryArchive>(i);
      native.push_back(MetalBinaryArchiveIsWrapped(archive) ? Unwrap(GetWrapped(archive)) : archive);
    }
    realDescriptor->setBinaryArchives(NS::Array::array(
        (const NS::Object *const *)native.data(), native.size()));
  }
  if(descriptor->preloadedLibraries() && descriptor->preloadedLibraries()->count())
  {
    rdcarray<MTL::DynamicLibrary *> native;
    NS::Array *libraries = descriptor->preloadedLibraries();
    for(NS::UInteger i = 0; i < libraries->count(); i++)
    {
      MTL::DynamicLibrary *library = libraries->object<MTL::DynamicLibrary>(i);
      native.push_back(MetalDynamicLibraryIsWrapped(library) ? Unwrap(GetWrapped(library)) : library);
    }
    realDescriptor->setPreloadedLibraries(NS::Array::array(
        (const NS::Object *const *)native.data(),native.size()));
  }
  MTL::LinkedFunctions *links = RDMTL::MetalNativeLinkedFunctions(descriptor->linkedFunctions());
  realDescriptor->setLinkedFunctions(links);
  links->release();
  MTL::ComputePipelineState *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newComputePipelineState(
                          realDescriptor, options, reflection, error));
  realDescriptor->release();
  if(!real)
    return NULL;
  WrappedMTLComputePipelineState *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    const bool supported = captured.binaryArchives.size() == descriptor->binaryArchives()->count() &&
                           ValidArchives(captured.binaryArchives) &&
                           captured.preloadedLibraries.size() ==
                               descriptor->preloadedLibraries()->count() &&
                           captured.preloadedLibraries.size() <= 8;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newComputePipelineStateWithDescriptor);
    Serialise_newComputePipelineStateWithDescriptor(ser, wrapped, captured, options, supported);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(captured.computeFunction));
    for(WrappedMTLFunction *function : captured.linkedFunctions.functions)
      if(function) record->AddParent(GetRecord(function));
    for(WrappedMTLDynamicLibrary *library : captured.preloadedLibraries)
      record->AddParent(GetRecord(library));
    for(WrappedMTLBinaryArchive *archive : captured.binaryArchives)
      if(archive) record->AddParent(GetRecord(archive));
  }
  return wrapped;
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(
    WrappedMTLDevice, WrappedMTLRenderPipelineState *, newRenderPipelineStateWithDescriptorOptions,
    RDMTL::RenderPipelineDescriptor &, MTL::PipelineOption, bool);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(
    WrappedMTLDevice, WrappedMTLComputePipelineState *, newComputePipelineStateWithFunctionOptions,
    WrappedMTLFunction *, MTL::PipelineOption, MTL::AutoreleasedComputePipelineReflection *, NS::Error **);
INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(
    WrappedMTLDevice, WrappedMTLComputePipelineState *, newComputePipelineStateWithDescriptor,
    RDMTL::ComputePipelineDescriptor &, MTL::PipelineOption, bool);
