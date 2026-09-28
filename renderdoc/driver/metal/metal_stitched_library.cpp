// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_device.h"
#include "metal_function.h"
#include "metal_library.h"
#include "metal_manager.h"
#include "metal_replay.h"
#include "metal_stitched_library.h"

MTL::StitchedLibraryDescriptor *MetalMakeSingleStitchedDescriptor(MTL::Function *function,
                                                        const rdcstr &graphName,
                                                        const rdcstr &functionName,
                                                        uint32_t inputIndex)
{
  MTL::FunctionStitchingInputNode *input =
      MTL::FunctionStitchingInputNode::alloc()->init(inputIndex);
  const NS::Object *inputObject = input;
  MTL::FunctionStitchingFunctionNode *output = MTL::FunctionStitchingFunctionNode::alloc()->init(
      NS::String::string(functionName.c_str(), NS::UTF8StringEncoding),
      NS::Array::array(&inputObject, 1), NS::Array::array());
  const NS::Object *outputObject = output;
  MTL::FunctionStitchingGraph *graph = MTL::FunctionStitchingGraph::alloc()->init(
      NS::String::string(graphName.c_str(), NS::UTF8StringEncoding),
      NS::Array::array(&outputObject, 1), output, NS::Array::array());
  const NS::Object *functionObject = function;
  const NS::Object *graphObject = graph;
  MTL::StitchedLibraryDescriptor *descriptor = MTL::StitchedLibraryDescriptor::alloc()->init();
  descriptor->setFunctions(NS::Array::array(&functionObject, 1));
  descriptor->setFunctionGraphs(NS::Array::array(&graphObject, 1));
  graph->release();
  output->release();
  input->release();
  return descriptor;
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newStitchedLibrary(SerialiserType &ser,
    WrappedMTLLibrary *library, WrappedMTLFunction *function, rdcstr graphName,
    rdcstr functionName, uint32_t inputIndex)
{
  SERIALISE_ELEMENT_LOCAL(Device, this);
  SERIALISE_ELEMENT_LOCAL(Library, GetResID(library)).TypedAs("MTLLibrary"_lit).Important();
  SERIALISE_ELEMENT(function).Important();
  SERIALISE_ELEMENT(graphName).Important();
  SERIALISE_ELEMENT(functionName).Important();
  SERIALISE_ELEMENT(inputIndex);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(Device != this || Library == ResourceId() || GetResourceManager()->HasResource(Library) ||
       !function || function->m_Type != eResFunction || !function->m_Real ||
       Unwrap(function)->functionType() != MTL::FunctionTypeVisible ||
       graphName.empty() || graphName.size() > 128 ||
       functionName.empty() || functionName.size() > 128 || inputIndex != 0 ||
       functionName != Unwrap(function)->name()->utf8String())
    {
      RDCERR("Invalid or unsupported Metal stitched library graph");
      return false;
    }
    MTL::StitchedLibraryDescriptor *descriptor = MetalMakeSingleStitchedDescriptor(
        Unwrap(function), graphName, functionName, inputIndex);
    NS::Error *error = NULL;
    MTL::Library *real = Unwrap(this)->newLibrary(descriptor, &error);
    descriptor->release();
    if(!real)
    {
      RDCERR("Failed to recreate Metal stitched library: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    WrappedMTLLibrary *wrapped = NULL;
    GetResourceManager()->WrapResource(Library, real, wrapped, true);
    AddResource(Library, ResourceType::Pool, "Stitched Library");
    DerivedResource(this, Library);
    DerivedResource(function, Library);
  }
  return true;
}

WrappedMTLLibrary *WrappedMTLDevice::newStitchedLibrary(WrappedMTLFunction *function,
    rdcstr graphName, rdcstr functionName, uint32_t inputIndex, NS::Error **error)
{
  if(!function || function->m_Type != eResFunction || !function->m_Real ||
     Unwrap(function)->functionType() != MTL::FunctionTypeVisible ||
     graphName.empty() || graphName.size() > 128 ||
     functionName.empty() || functionName.size() > 128 || inputIndex != 0)
    return NULL;
  MTL::StitchedLibraryDescriptor *descriptor = MetalMakeSingleStitchedDescriptor(
      Unwrap(function), graphName, functionName, inputIndex);
  MTL::Library *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newLibrary(descriptor, error));
  descriptor->release();
  if(!real) return NULL;
  WrappedMTLLibrary *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newLibraryWithStitchedDescriptor);
    Serialise_newStitchedLibrary(ser, wrapped, function, graphName, functionName, inputIndex);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(function));
  }
  return wrapped;
}

WrappedMTLLibrary *WrappedMTLDevice::CaptureAsyncStitchedLibrary(MTL::Library *real,
    WrappedMTLFunction *function, rdcstr graphName, rdcstr functionName,
    uint32_t inputIndex)
{
  if(!real) return NULL;
  // Completion values are borrowed; the temporary bridge owns one native reference.
  real->retain();
  WrappedMTLLibrary *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newLibraryWithStitchedDescriptor_async);
    Serialise_newStitchedLibrary(ser, wrapped, function, graphName, functionName, inputIndex);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(function));
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newStitchedLibrary(
    ReadSerialiser &, WrappedMTLLibrary *, WrappedMTLFunction *, rdcstr, rdcstr, uint32_t);
template bool WrappedMTLDevice::Serialise_newStitchedLibrary(
    WriteSerialiser &, WrappedMTLLibrary *, WrappedMTLFunction *, rdcstr, rdcstr, uint32_t);
