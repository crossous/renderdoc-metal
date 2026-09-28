// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_binary_archive.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_library.h"
#include "metal_manager.h"
#include "metal_stitched_library.h"
#include "os/os_specific.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
bool ValidArchiveFunction(WrappedMTLFunction *function, MTL::FunctionType type)
{
  return function && function->m_Type == eResFunction && function->m_Real &&
         Unwrap(function)->functionType() == type;
}

bool ValidArchiveBuffers(const rdcarray<RDMTL::PipelineBufferDescriptor> &buffers)
{
  if(buffers.size() > 31) return false;
  for(const auto &buffer : buffers)
    if((uint64_t)buffer.mutability > 2) return false;
  return true;
}

bool ValidArchiveComputeDescriptor(WrappedMTLDevice *device,
                                   const RDMTL::ComputePipelineDescriptor &descriptor)
{
  return ValidArchiveFunction(descriptor.computeFunction, MTL::FunctionTypeKernel) &&
         descriptor.binaryArchives.empty() && descriptor.preloadedLibraries.empty() &&
         descriptor.linkedFunctions.functions.empty() &&
         descriptor.linkedFunctions.binaryFunctions.empty() &&
         descriptor.linkedFunctions.groups.empty() &&
         descriptor.linkedFunctions.privateFunctions.empty() &&
         !descriptor.supportAddingBinaryFunctions &&
         !descriptor.supportIndirectCommandBuffers &&
         descriptor.maxCallStackDepth == 1 &&
         descriptor.maxTotalThreadsPerThreadgroup <=
             Unwrap(device)->maxThreadsPerThreadgroup().width &&
         descriptor.stageInputDescriptor.attributes.empty() &&
         descriptor.stageInputDescriptor.layouts.empty() &&
         descriptor.stageInputDescriptor.indexBufferIndex == 0 &&
         descriptor.stageInputDescriptor.indexType == MTL::IndexTypeUInt16 &&
         ValidArchiveBuffers(descriptor.buffers);
}

bool ValidArchiveRenderDescriptor(WrappedMTLDevice *device,
                                  const RDMTL::RenderPipelineDescriptor &descriptor)
{
  return ValidArchiveFunction(descriptor.vertexFunction, MTL::FunctionTypeVertex) &&
         (!descriptor.fragmentFunction ||
          ValidArchiveFunction(descriptor.fragmentFunction, MTL::FunctionTypeFragment)) &&
         descriptor.binaryArchives.empty() &&
         descriptor.vertexPreloadedLibraries.empty() &&
         descriptor.fragmentPreloadedLibraries.empty() &&
         descriptor.vertexLinkedFunctions.functions.empty() &&
         descriptor.vertexLinkedFunctions.binaryFunctions.empty() &&
         descriptor.vertexLinkedFunctions.groups.empty() &&
         descriptor.vertexLinkedFunctions.privateFunctions.empty() &&
         descriptor.fragmentLinkedFunctions.functions.empty() &&
         descriptor.fragmentLinkedFunctions.binaryFunctions.empty() &&
         descriptor.fragmentLinkedFunctions.groups.empty() &&
         descriptor.fragmentLinkedFunctions.privateFunctions.empty() &&
         !descriptor.supportAddingVertexBinaryFunctions &&
         !descriptor.supportAddingFragmentBinaryFunctions &&
         descriptor.maxVertexAmplificationCount == 1 &&
         descriptor.vertexDescriptor.attributes.size() <= 31 &&
         descriptor.vertexDescriptor.layouts.size() <= 31 &&
         descriptor.colorAttachments.size() <= 8 &&
         ValidArchiveBuffers(descriptor.vertexBuffers) &&
         ValidArchiveBuffers(descriptor.fragmentBuffers) &&
         Unwrap(device)->supportsTextureSampleCount(descriptor.sampleCount) &&
         Unwrap(device)->supportsTextureSampleCount(descriptor.rasterSampleCount);
}
}

WrappedMTLBinaryArchive::WrappedMTLBinaryArchive(MTL::BinaryArchive *real, ResourceId id,
                                                 WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

WrappedMTLBinaryArchive::~WrappedMTLBinaryArchive()
{
  if(!m_TemporaryPath.empty()) unlink(m_TemporaryPath.c_str());
  if(!m_TemporaryDirectory.empty()) rmdir(m_TemporaryDirectory.c_str());
}

template <typename SerialiserType>
bool WrappedMTLBinaryArchive::Serialise_addComputePipelineFunctions(
    SerialiserType &ser, RDMTL::ComputePipelineDescriptor &descriptor)
{
  SERIALISE_ELEMENT_LOCAL(BinaryArchive, this).Important();
  SERIALISE_ELEMENT(descriptor);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!BinaryArchive || BinaryArchive->m_Type != eResBinaryArchive ||
       !BinaryArchive->m_Real || !ValidArchiveComputeDescriptor(m_Device, descriptor))
    {
      RDCERR("Invalid Metal binary archive compute function descriptor");
      return false;
    }
    MTL::ComputePipelineDescriptor *realDescriptor(descriptor);
    NS::Error *error = NULL;
    bool added = Unwrap(BinaryArchive)->addComputePipelineFunctions(
        realDescriptor, &error);
    realDescriptor->release();
    if(!added)
    {
      RDCERR("Failed to add captured compute function to Metal binary archive: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    m_Device->DerivedResource(descriptor.computeFunction, GetResID(BinaryArchive));
  }
  return true;
}

bool WrappedMTLBinaryArchive::addComputePipelineFunctions(
    MTL::ComputePipelineDescriptor *descriptor, NS::Error **error)
{
  if(!descriptor || !m_Real) return false;
  RDMTL::ComputePipelineDescriptor captured(descriptor);
  if(!ValidArchiveComputeDescriptor(m_Device, captured)) return false;
  MTL::ComputePipelineDescriptor *realDescriptor(captured);
  bool added = false;
  SERIALISE_TIME_CALL(added = Unwrap(this)->addComputePipelineFunctions(
      realDescriptor, error));
  realDescriptor->release();
  if(added && IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBinaryArchive_addComputePipelineFunctionsWithDescriptor);
    Serialise_addComputePipelineFunctions(ser, captured);
    MetalResourceRecord *record = GetRecord(this);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(captured.computeFunction));
  }
  return added;
}

template <typename SerialiserType>
bool WrappedMTLBinaryArchive::Serialise_addRenderPipelineFunctions(
    SerialiserType &ser, RDMTL::RenderPipelineDescriptor &descriptor)
{
  SERIALISE_ELEMENT_LOCAL(BinaryArchive, this).Important();
  SERIALISE_ELEMENT(descriptor);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!BinaryArchive || BinaryArchive->m_Type != eResBinaryArchive ||
       !BinaryArchive->m_Real || !ValidArchiveRenderDescriptor(m_Device, descriptor))
    {
      RDCERR("Invalid Metal binary archive render function descriptor");
      return false;
    }
    MTL::RenderPipelineDescriptor *realDescriptor(descriptor);
    NS::Error *error = NULL;
    bool added = Unwrap(BinaryArchive)->addRenderPipelineFunctions(
        realDescriptor, &error);
    realDescriptor->release();
    if(!added)
    {
      RDCERR("Failed to add captured render functions to Metal binary archive: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    m_Device->DerivedResource(descriptor.vertexFunction, GetResID(BinaryArchive));
    if(descriptor.fragmentFunction)
      m_Device->DerivedResource(descriptor.fragmentFunction, GetResID(BinaryArchive));
  }
  return true;
}

bool WrappedMTLBinaryArchive::addRenderPipelineFunctions(
    MTL::RenderPipelineDescriptor *descriptor, NS::Error **error)
{
  if(!descriptor || !m_Real) return false;
  RDMTL::RenderPipelineDescriptor captured(descriptor);
  if(!ValidArchiveRenderDescriptor(m_Device, captured)) return false;
  MTL::RenderPipelineDescriptor *realDescriptor(captured);
  bool added = false;
  SERIALISE_TIME_CALL(added = Unwrap(this)->addRenderPipelineFunctions(
      realDescriptor, error));
  realDescriptor->release();
  if(added && IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBinaryArchive_addRenderPipelineFunctionsWithDescriptor);
    Serialise_addRenderPipelineFunctions(ser, captured);
    MetalResourceRecord *record = GetRecord(this);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(captured.vertexFunction));
    if(captured.fragmentFunction) record->AddParent(GetRecord(captured.fragmentFunction));
  }
  return added;
}

template bool WrappedMTLBinaryArchive::Serialise_addComputePipelineFunctions(
    ReadSerialiser &, RDMTL::ComputePipelineDescriptor &);
template bool WrappedMTLBinaryArchive::Serialise_addComputePipelineFunctions(
    WriteSerialiser &, RDMTL::ComputePipelineDescriptor &);
template bool WrappedMTLBinaryArchive::Serialise_addRenderPipelineFunctions(
    ReadSerialiser &, RDMTL::RenderPipelineDescriptor &);
template bool WrappedMTLBinaryArchive::Serialise_addRenderPipelineFunctions(
    WriteSerialiser &, RDMTL::RenderPipelineDescriptor &);

namespace
{
bool ValidArchiveSourceFunction(WrappedMTLLibrary *library, const rdcstr &name,
                                WrappedMTLDevice *device)
{
  if(!library || library->m_Type != eResLibrary || !library->m_Real ||
     library->m_Device != device || name.empty() || name.size() > 128)
    return false;
  MTL::Function *function = Unwrap(library)->newFunction(
      NS::String::string(name.c_str(), NS::UTF8StringEncoding));
  if(!function) return false;
  MTL::FunctionType type = function->functionType();
  function->release();
  // Intersection descriptors are a distinct native class; keep that path gated
  // until its capture/replay reconstruction has a native positive fixture.
  return type == MTL::FunctionTypeVisible;
}
}

template <typename SerialiserType>
bool WrappedMTLBinaryArchive::Serialise_addFunction(SerialiserType &ser,
                                                     WrappedMTLLibrary *library,
                                                     rdcstr functionName)
{
  SERIALISE_ELEMENT_LOCAL(BinaryArchive, this).Important();
  SERIALISE_ELEMENT(library).Important();
  SERIALISE_ELEMENT(functionName).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!BinaryArchive || BinaryArchive->m_Type != eResBinaryArchive ||
       !BinaryArchive->m_Real || BinaryArchive->m_Device != m_Device ||
       !ValidArchiveSourceFunction(library, functionName, m_Device))
    {
      RDCERR("Invalid Metal binary archive source function");
      return false;
    }
    MTL::FunctionDescriptor *descriptor = MTL::FunctionDescriptor::alloc()->init();
    descriptor->setName(NS::String::string(functionName.c_str(), NS::UTF8StringEncoding));
    NS::Error *error = NULL;
    bool added = Unwrap(BinaryArchive)->addFunction(descriptor, Unwrap(library), &error);
    descriptor->release();
    if(!added)
    {
      RDCERR("Failed to add captured function to Metal binary archive: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    m_Device->DerivedResource(library, GetResID(BinaryArchive));
  }
  return true;
}

bool WrappedMTLBinaryArchive::addFunction(MTL::FunctionDescriptor *descriptor,
                                           WrappedMTLLibrary *library, NS::Error **error)
{
  NS::String *name = descriptor ? descriptor->name() : NULL;
  const char *utf8 = name ? name->utf8String() : NULL;
  rdcstr functionName = utf8 ? utf8 : "";
  if(!m_Real || !descriptor || descriptor->specializedName() ||
     descriptor->constantValues() || descriptor->options() != 0 ||
     (descriptor->binaryArchives() && descriptor->binaryArchives()->count()) ||
     !ValidArchiveSourceFunction(library, functionName, m_Device))
    return false;
  MTL::FunctionDescriptor *native = MTL::FunctionDescriptor::alloc()->init();
  native->setName(NS::String::string(functionName.c_str(), NS::UTF8StringEncoding));
  bool added = false;
  SERIALISE_TIME_CALL(added = Unwrap(this)->addFunction(native, Unwrap(library), error));
  native->release();
  if(added && IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBinaryArchive_addFunctionWithDescriptor);
    Serialise_addFunction(ser, library, functionName);
    MetalResourceRecord *record = GetRecord(this);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(library));
  }
  return added;
}

template bool WrappedMTLBinaryArchive::Serialise_addFunction(
    ReadSerialiser &, WrappedMTLLibrary *, rdcstr);
template bool WrappedMTLBinaryArchive::Serialise_addFunction(
    WriteSerialiser &, WrappedMTLLibrary *, rdcstr);

namespace
{
bool ValidArchiveStitchedLibrary(WrappedMTLFunction *function, const rdcstr &graphName,
                                 const rdcstr &functionName, WrappedMTLDevice *device)
{
  return function && function->m_Type == eResFunction && function->m_Real &&
         function->m_Device == device &&
         Unwrap(function)->functionType() == MTL::FunctionTypeVisible &&
         !graphName.empty() && graphName.size() <= 128 &&
         !functionName.empty() && functionName.size() <= 128 &&
         functionName == Unwrap(function)->name()->utf8String();
}

bool NativeArchiveAddLibrary(MTL::BinaryArchive *archive,
                             MTL::StitchedLibraryDescriptor *descriptor, NS::Error **error)
{
  SEL method = sel_registerName("addLibraryWithDescriptor:error:");
  if(!((BOOL (*)(id, SEL, SEL))objc_msgSend)(
         (id)archive, sel_registerName("respondsToSelector:"), method))
    return false;
  return ((BOOL (*)(id, SEL, id, NS::Error **))objc_msgSend)(
      (id)archive, method, (id)descriptor, error) != NO;
}
}

template <typename SerialiserType>
bool WrappedMTLBinaryArchive::Serialise_addLibrary(SerialiserType &ser,
                                                    WrappedMTLFunction *function,
                                                    rdcstr graphName, rdcstr functionName)
{
  SERIALISE_ELEMENT_LOCAL(BinaryArchive, this).Important();
  SERIALISE_ELEMENT(function).Important();
  SERIALISE_ELEMENT(graphName).Important();
  SERIALISE_ELEMENT(functionName).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!BinaryArchive || BinaryArchive->m_Type != eResBinaryArchive ||
       !BinaryArchive->m_Real || BinaryArchive->m_Device != m_Device ||
       !ValidArchiveStitchedLibrary(function, graphName, functionName, m_Device))
    {
      RDCERR("Invalid Metal binary archive stitched library descriptor");
      return false;
    }
    MTL::StitchedLibraryDescriptor *descriptor = MetalMakeSingleStitchedDescriptor(
        Unwrap(function), graphName, functionName, 0);
    NS::Error *error = NULL;
    bool added = NativeArchiveAddLibrary(Unwrap(BinaryArchive), descriptor, &error);
    descriptor->release();
    if(!added)
    {
      RDCERR("Failed to add captured stitched library to Metal binary archive: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    m_Device->DerivedResource(function, GetResID(BinaryArchive));
  }
  return true;
}

bool WrappedMTLBinaryArchive::addLibrary(WrappedMTLFunction *function, rdcstr graphName,
                                          rdcstr functionName, NS::Error **error)
{
  if(!m_Real || !ValidArchiveStitchedLibrary(function, graphName, functionName, m_Device))
    return false;
  MTL::StitchedLibraryDescriptor *descriptor = MetalMakeSingleStitchedDescriptor(
      Unwrap(function), graphName, functionName, 0);
  bool added = false;
  SERIALISE_TIME_CALL(added = NativeArchiveAddLibrary(Unwrap(this), descriptor, error));
  descriptor->release();
  if(added && IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBinaryArchive_addLibraryWithDescriptor);
    Serialise_addLibrary(ser, function, graphName, functionName);
    MetalResourceRecord *record = GetRecord(this);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(function));
  }
  return added;
}

template bool WrappedMTLBinaryArchive::Serialise_addLibrary(
    ReadSerialiser &, WrappedMTLFunction *, rdcstr, rdcstr);
template bool WrappedMTLBinaryArchive::Serialise_addLibrary(
    WriteSerialiser &, WrappedMTLFunction *, rdcstr, rdcstr);

namespace
{
bool ValidArchiveTile(WrappedMTLFunction *function, const rdcarray<uint32_t> &colors,
                      uint64_t sampleCount, uint64_t maxThreads, WrappedMTLDevice *device)
{
  if(!ValidArchiveFunction(function, MTL::FunctionTypeKernel) ||
     function->m_Device != device || colors.size() != 8 || sampleCount != 1 ||
     maxThreads > 1024 ||
     (colors[0] != MTL::PixelFormatBGRA8Unorm &&
      colors[0] != MTL::PixelFormatRGBA8Unorm))
    return false;
  for(size_t i = 1; i < colors.size(); ++i)
    if(colors[i] != MTL::PixelFormatInvalid) return false;
  return true;
}

MTL::TileRenderPipelineDescriptor *MakeArchiveTileDescriptor(
    WrappedMTLFunction *function, const rdcarray<uint32_t> &colors,
    uint64_t sampleCount, uint64_t maxThreads, bool matchesTileSize)
{
  MTL::TileRenderPipelineDescriptor *descriptor =
      MTL::TileRenderPipelineDescriptor::alloc()->init();
  descriptor->setTileFunction(Unwrap(function));
  descriptor->setRasterSampleCount(sampleCount);
  descriptor->setMaxTotalThreadsPerThreadgroup(maxThreads);
  descriptor->setThreadgroupSizeMatchesTileSize(matchesTileSize);
  for(size_t i = 0; i < colors.size(); ++i)
    descriptor->colorAttachments()->object(i)->setPixelFormat((MTL::PixelFormat)colors[i]);
  return descriptor;
}
}

template <typename SerialiserType>
bool WrappedMTLBinaryArchive::Serialise_addTilePipelineFunctions(
    SerialiserType &ser, WrappedMTLFunction *function, rdcarray<uint32_t> colors,
    uint64_t sampleCount, uint64_t maxThreads, bool matchesTileSize)
{
  SERIALISE_ELEMENT_LOCAL(BinaryArchive, this).Important();
  SERIALISE_ELEMENT(function).Important();
  SERIALISE_ELEMENT(colors).Important();
  SERIALISE_ELEMENT(sampleCount);
  SERIALISE_ELEMENT(maxThreads);
  SERIALISE_ELEMENT(matchesTileSize);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!BinaryArchive || BinaryArchive->m_Type != eResBinaryArchive ||
       !BinaryArchive->m_Real || BinaryArchive->m_Device != m_Device ||
       !ValidArchiveTile(function, colors, sampleCount, maxThreads, m_Device))
    {
      RDCERR("Invalid Metal binary archive tile pipeline descriptor");
      return false;
    }
    MTL::TileRenderPipelineDescriptor *descriptor = MakeArchiveTileDescriptor(
        function, colors, sampleCount, maxThreads, matchesTileSize);
    NS::Error *error = NULL;
    bool added = Unwrap(BinaryArchive)->addTileRenderPipelineFunctions(descriptor, &error);
    descriptor->release();
    if(!added)
    {
      RDCERR("Failed to add captured tile pipeline functions to Metal binary archive: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    m_Device->DerivedResource(function, GetResID(BinaryArchive));
  }
  return true;
}

bool WrappedMTLBinaryArchive::addTilePipelineFunctions(
    MTL::TileRenderPipelineDescriptor *descriptor, WrappedMTLFunction *function,
    NS::Error **error)
{
  if(!m_Real || !descriptor || !function) return false;
  rdcarray<uint32_t> colors;
  for(size_t i = 0; i < 8; ++i)
    colors.push_back((uint32_t)descriptor->colorAttachments()->object(i)->pixelFormat());
  uint64_t sampleCount = descriptor->rasterSampleCount();
  uint64_t maxThreads = descriptor->maxTotalThreadsPerThreadgroup();
  bool matchesTileSize = descriptor->threadgroupSizeMatchesTileSize();
  if(!ValidArchiveTile(function, colors, sampleCount, maxThreads, m_Device)) return false;
  MTL::TileRenderPipelineDescriptor *native = MakeArchiveTileDescriptor(
      function, colors, sampleCount, maxThreads, matchesTileSize);
  bool added = false;
  SERIALISE_TIME_CALL(added = Unwrap(this)->addTileRenderPipelineFunctions(native, error));
  native->release();
  if(added && IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBinaryArchive_addTileRenderPipelineFunctionsWithDescriptor);
    Serialise_addTilePipelineFunctions(ser, function, colors, sampleCount, maxThreads, matchesTileSize);
    MetalResourceRecord *record = GetRecord(this);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(function));
  }
  return added;
}

template bool WrappedMTLBinaryArchive::Serialise_addTilePipelineFunctions(
    ReadSerialiser &, WrappedMTLFunction *, rdcarray<uint32_t>, uint64_t, uint64_t, bool);
template bool WrappedMTLBinaryArchive::Serialise_addTilePipelineFunctions(
    WriteSerialiser &, WrappedMTLFunction *, rdcarray<uint32_t>, uint64_t, uint64_t, bool);

namespace
{
bool ValidArchiveMesh(WrappedMTLFunction *mesh, WrappedMTLFunction *fragment,
                      const rdcarray<uint32_t> &colors, uint64_t sampleCount,
                      uint64_t maxMeshThreads, uint64_t maxMeshGrid, WrappedMTLDevice *device)
{
  if(!ValidArchiveFunction(mesh, MTL::FunctionTypeMesh) || mesh->m_Device != device ||
     !ValidArchiveFunction(fragment, MTL::FunctionTypeFragment) ||
     fragment->m_Device != device || colors.size() != 8 || sampleCount != 1 ||
     !maxMeshThreads || maxMeshThreads > 1024 || maxMeshGrid > 1048575 ||
     (colors[0] != MTL::PixelFormatBGRA8Unorm &&
      colors[0] != MTL::PixelFormatRGBA8Unorm))
    return false;
  for(size_t i = 1; i < colors.size(); ++i)
    if(colors[i] != MTL::PixelFormatInvalid) return false;
  return true;
}

MTL::MeshRenderPipelineDescriptor *MakeArchiveMeshDescriptor(
    WrappedMTLFunction *mesh, WrappedMTLFunction *fragment,
    const rdcarray<uint32_t> &colors, uint64_t sampleCount,
    uint64_t maxMeshThreads, uint64_t maxMeshGrid)
{
  MTL::MeshRenderPipelineDescriptor *descriptor =
      MTL::MeshRenderPipelineDescriptor::alloc()->init();
  descriptor->setMeshFunction(Unwrap(mesh));
  descriptor->setFragmentFunction(Unwrap(fragment));
  descriptor->setRasterSampleCount(sampleCount);
  descriptor->setMaxTotalThreadsPerMeshThreadgroup(maxMeshThreads);
  if(maxMeshGrid) descriptor->setMaxTotalThreadgroupsPerMeshGrid(maxMeshGrid);
  for(size_t i = 0; i < colors.size(); ++i)
    descriptor->colorAttachments()->object(i)->setPixelFormat((MTL::PixelFormat)colors[i]);
  return descriptor;
}

bool NativeArchiveAddMesh(MTL::BinaryArchive *archive,
                          MTL::MeshRenderPipelineDescriptor *descriptor, NS::Error **error)
{
  SEL method = sel_registerName("addMeshRenderPipelineFunctionsWithDescriptor:error:");
  if(!((BOOL (*)(id, SEL, SEL))objc_msgSend)(
         (id)archive, sel_registerName("respondsToSelector:"), method))
    return false;
  return ((BOOL (*)(id, SEL, id, NS::Error **))objc_msgSend)(
      (id)archive, method, (id)descriptor, error) != NO;
}
}

template <typename SerialiserType>
bool WrappedMTLBinaryArchive::Serialise_addMeshPipelineFunctions(
    SerialiserType &ser, WrappedMTLFunction *mesh, WrappedMTLFunction *fragment,
    rdcarray<uint32_t> colors, uint64_t sampleCount, uint64_t maxMeshThreads,
    uint64_t maxMeshGrid)
{
  SERIALISE_ELEMENT_LOCAL(BinaryArchive, this).Important();
  SERIALISE_ELEMENT(mesh).Important();
  SERIALISE_ELEMENT(fragment).Important();
  SERIALISE_ELEMENT(colors).Important();
  SERIALISE_ELEMENT(sampleCount);
  SERIALISE_ELEMENT(maxMeshThreads);
  SERIALISE_ELEMENT(maxMeshGrid);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!BinaryArchive || BinaryArchive->m_Type != eResBinaryArchive ||
       !BinaryArchive->m_Real || BinaryArchive->m_Device != m_Device ||
       !ValidArchiveMesh(mesh, fragment, colors, sampleCount,
                         maxMeshThreads, maxMeshGrid, m_Device))
    {
      RDCERR("Invalid Metal binary archive mesh pipeline descriptor");
      return false;
    }
    MTL::MeshRenderPipelineDescriptor *descriptor = MakeArchiveMeshDescriptor(
        mesh, fragment, colors, sampleCount, maxMeshThreads, maxMeshGrid);
    NS::Error *error = NULL;
    bool added = NativeArchiveAddMesh(Unwrap(BinaryArchive), descriptor, &error);
    descriptor->release();
    if(!added)
    {
      RDCERR("Failed to add captured mesh pipeline functions to Metal binary archive: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    m_Device->DerivedResource(mesh, GetResID(BinaryArchive));
    m_Device->DerivedResource(fragment, GetResID(BinaryArchive));
  }
  return true;
}

bool WrappedMTLBinaryArchive::addMeshPipelineFunctions(
    MTL::MeshRenderPipelineDescriptor *descriptor, WrappedMTLFunction *mesh,
    WrappedMTLFunction *fragment, NS::Error **error)
{
  if(!m_Real || !descriptor) return false;
  rdcarray<uint32_t> colors;
  for(size_t i = 0; i < 8; ++i)
    colors.push_back((uint32_t)descriptor->colorAttachments()->object(i)->pixelFormat());
  uint64_t sampleCount = descriptor->rasterSampleCount();
  uint64_t maxMeshThreads = descriptor->maxTotalThreadsPerMeshThreadgroup();
  uint64_t maxMeshGrid = descriptor->maxTotalThreadgroupsPerMeshGrid();
  if(!ValidArchiveMesh(mesh, fragment, colors, sampleCount,
                       maxMeshThreads, maxMeshGrid, m_Device)) return false;
  MTL::MeshRenderPipelineDescriptor *native = MakeArchiveMeshDescriptor(
      mesh, fragment, colors, sampleCount, maxMeshThreads, maxMeshGrid);
  bool added = false;
  SERIALISE_TIME_CALL(added = NativeArchiveAddMesh(Unwrap(this), native, error));
  native->release();
  if(added && IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLBinaryArchive_addMeshRenderPipelineFunctionsWithDescriptor);
    Serialise_addMeshPipelineFunctions(ser, mesh, fragment, colors, sampleCount,
                                      maxMeshThreads, maxMeshGrid);
    MetalResourceRecord *record = GetRecord(this);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(mesh));
    record->AddParent(GetRecord(fragment));
  }
  return added;
}

template bool WrappedMTLBinaryArchive::Serialise_addMeshPipelineFunctions(
    ReadSerialiser &, WrappedMTLFunction *, WrappedMTLFunction *, rdcarray<uint32_t>,
    uint64_t, uint64_t, uint64_t);
template bool WrappedMTLBinaryArchive::Serialise_addMeshPipelineFunctions(
    WriteSerialiser &, WrappedMTLFunction *, WrappedMTLFunction *, rdcarray<uint32_t>,
    uint64_t, uint64_t, uint64_t);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newBinaryArchive(SerialiserType &ser,
    WrappedMTLBinaryArchive *archive, bytebuf &data)
{
  SERIALISE_ELEMENT_LOCAL(Device, this);
  SERIALISE_ELEMENT_LOCAL(Archive, GetResID(archive))
      .TypedAs("MTLBinaryArchive"_lit).Important();
  SERIALISE_ELEMENT(data);
  bool emptyArchive = ser.VersionAtLeast(0xB) && data.empty();
  if(ser.VersionAtLeast(0xB))
  {
    SERIALISE_ELEMENT(emptyArchive);
  }
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(Device != this || Archive == ResourceId() ||
       GetResourceManager()->HasResource(Archive) ||
       (emptyArchive ? !data.empty() : data.size() < 4) ||
       data.size() > 64 * 1024 * 1024)
    {
      RDCERR("Invalid Metal binary archive identity or payload");
      return false;
    }
    rdcstr directory, path;
    if(!emptyArchive)
    {
      rdcstr pattern = FileIO::GetTempFolderFilename() + "renderdoc-metal-archive.XXXXXX";
      rdcarray<char> chars(pattern.c_str(), pattern.size());
      chars.push_back(0);
      char *created = mkdtemp(chars.data());
      if(!created) return false;
      directory = created;
      path = directory + "/archive.metallib";
      int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
      size_t written = 0;
      while(fd >= 0 && written < data.size())
      {
        ssize_t n = write(fd, data.data() + written, data.size() - written);
        if(n > 0) written += size_t(n);
        else if(n < 0 && errno == EINTR) continue;
        else break;
      }
      bool saved = fd >= 0 && written == data.size();
      if(fd >= 0 && close(fd) != 0) saved = false;
      if(!saved)
      {
        unlink(path.c_str());
        rmdir(directory.c_str());
        RDCERR("Could not materialize captured Metal binary archive");
        return false;
      }
    }
    MTL::BinaryArchiveDescriptor *descriptor = MTL::BinaryArchiveDescriptor::alloc()->init();
    if(!emptyArchive)
      descriptor->setUrl(NS::URL::fileURLWithPath(
          NS::String::string(path.c_str(), NS::UTF8StringEncoding)));
    NS::Error *error = NULL;
    MTL::BinaryArchive *real = Unwrap(this)->newBinaryArchive(descriptor, &error);
    descriptor->release();
    if(!real)
    {
      if(!emptyArchive)
      {
        unlink(path.c_str());
        rmdir(directory.c_str());
      }
      RDCERR("Failed to recreate captured Metal binary archive: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    WrappedMTLBinaryArchive *wrapped = NULL;
    GetResourceManager()->WrapResource(Archive, real, wrapped, true);
    wrapped->m_TemporaryPath = path;
    wrapped->m_TemporaryDirectory = directory;
    AddResource(Archive, ResourceType::Pool, "Binary Archive");
    DerivedResource(this, Archive);
  }
  return true;
}

WrappedMTLBinaryArchive *WrappedMTLDevice::newBinaryArchive(MTL::BinaryArchiveDescriptor *descriptor,
                                                             NS::Error **error)
{
  NS::URL *url = descriptor ? descriptor->url() : NULL;
  const bool fileURL = url &&
      ((BOOL (*)(id, SEL))objc_msgSend)((id)url, sel_registerName("isFileURL"));
  const char *path = fileURL ? url->fileSystemRepresentation() : NULL;
  bytebuf data;
  if(IsCaptureMode(m_State))
  {
    struct stat info = {};
    if(!descriptor || (url && (!path || strlen(path) > 4096 || stat(path, &info) != 0 ||
                       !S_ISREG(info.st_mode) || info.st_size < 4 ||
                       info.st_size > 64 * 1024 * 1024)))
    {
      RDCERR("Metal binary archive capture requires an empty descriptor or bounded file URL");
      return NULL;
    }
    if(url)
    {
      NS::Data *file = NS::Data::dataWithContentsOfFile(
          NS::String::string(path, NS::UTF8StringEncoding));
      if(!file || file->length() != (size_t)info.st_size)
      {
        RDCERR("Metal binary archive changed while reading capture payload");
        return NULL;
      }
      data.assign((const byte *)file->bytes(), file->length());
    }
  }
  MTL::BinaryArchive *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newBinaryArchive(descriptor, error));
  if(!real) return NULL;
  WrappedMTLBinaryArchive *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newBinaryArchiveWithDescriptor);
    Serialise_newBinaryArchive(ser, wrapped, data);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newBinaryArchive(
    ReadSerialiser &, WrappedMTLBinaryArchive *, bytebuf &);
template bool WrappedMTLDevice::Serialise_newBinaryArchive(
    WriteSerialiser &, WrappedMTLBinaryArchive *, bytebuf &);
