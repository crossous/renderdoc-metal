// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#pragma once

#include "metal_resources.h"
#include "metal_types.h"

class WrappedMTLBinaryArchive : public WrappedMTLObject
{
public:
  WrappedMTLBinaryArchive(MTL::BinaryArchive *real, ResourceId id, WrappedMTLDevice *device);
  ~WrappedMTLBinaryArchive();
  enum { TypeEnum = eResBinaryArchive };

  rdcstr m_TemporaryPath;
  rdcstr m_TemporaryDirectory;

  bool addComputePipelineFunctions(MTL::ComputePipelineDescriptor *descriptor,
                                   NS::Error **error);
  bool addRenderPipelineFunctions(MTL::RenderPipelineDescriptor *descriptor,
                                  NS::Error **error);
  bool addFunction(MTL::FunctionDescriptor *descriptor, WrappedMTLLibrary *library,
                   NS::Error **error);
  bool addLibrary(WrappedMTLFunction *function, rdcstr graphName,
                  rdcstr functionName, NS::Error **error);
  bool addTilePipelineFunctions(MTL::TileRenderPipelineDescriptor *descriptor,
                                WrappedMTLFunction *function, NS::Error **error);
  bool addMeshPipelineFunctions(MTL::MeshRenderPipelineDescriptor *descriptor,
                                WrappedMTLFunction *mesh, WrappedMTLFunction *fragment,
                                NS::Error **error);
  template <typename SerialiserType>
  bool Serialise_addComputePipelineFunctions(SerialiserType &ser,
                                             RDMTL::ComputePipelineDescriptor &descriptor);
  template <typename SerialiserType>
  bool Serialise_addRenderPipelineFunctions(SerialiserType &ser,
                                            RDMTL::RenderPipelineDescriptor &descriptor);
  template <typename SerialiserType>
  bool Serialise_addFunction(SerialiserType &ser, WrappedMTLLibrary *library,
                            rdcstr functionName);
  template <typename SerialiserType>
  bool Serialise_addLibrary(SerialiserType &ser, WrappedMTLFunction *function,
                           rdcstr graphName, rdcstr functionName);
  template <typename SerialiserType>
  bool Serialise_addTilePipelineFunctions(SerialiserType &ser, WrappedMTLFunction *function,
      rdcarray<uint32_t> colors, uint64_t sampleCount, uint64_t maxThreads, bool matchesTileSize);
  template <typename SerialiserType>
  bool Serialise_addMeshPipelineFunctions(SerialiserType &ser, WrappedMTLFunction *mesh,
      WrappedMTLFunction *fragment, rdcarray<uint32_t> colors, uint64_t sampleCount,
      uint64_t maxMeshThreads, uint64_t maxMeshGrid);
};

bool MetalBinaryArchiveIsWrapped(MTL::BinaryArchive *archive);
