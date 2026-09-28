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

#include "metal_library.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_replay.h"
#include <unistd.h>

WrappedMTLLibrary::~WrappedMTLLibrary()
{
  if(!m_DynamicInstallPath.empty()) unlink(m_DynamicInstallPath.c_str());
  if(!m_DynamicInstallDirectory.empty()) rmdir(m_DynamicInstallDirectory.c_str());
}

WrappedMTLLibrary::WrappedMTLLibrary(MTL::Library *realMTLLibrary, ResourceId objId,
                                     WrappedMTLDevice *wrappedMTLDevice)
    : WrappedMTLObject(realMTLLibrary, objId, wrappedMTLDevice, wrappedMTLDevice->GetStateRef())
{
  if(realMTLLibrary && objId != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLLibrary::Serialise_newFunctionWithName(SerialiserType &ser,
                                                      WrappedMTLFunction *function,
                                                      NS::String *FunctionName)
{
  SERIALISE_ELEMENT_LOCAL(Library, this);
  SERIALISE_ELEMENT_LOCAL(Function, GetResID(function)).TypedAs("MTLFunction"_lit);
  SERIALISE_ELEMENT(FunctionName).Important();

  SERIALISE_CHECK_READ_ERRORS();

  // TODO: implement RD MTL replay
  if(IsReplayingAndReading())
  {
    if(!Library || Library->m_Type != eResLibrary || !Library->m_Real || !FunctionName ||
       FunctionName->length() == 0 || Function == ResourceId() ||
       GetResourceManager()->HasResource(Function))
    {
      RDCERR("Invalid Metal library/function identity or name");
      return false;
    }
    MTL::Function *realMTLFunction = Unwrap(Library)->newFunction(FunctionName);
    if(!realMTLFunction)
    {
      RDCERR("Failed to recreate Metal function '%s'", FunctionName->utf8String());
      return false;
    }
    WrappedMTLFunction *wrappedMTLFunction;
    GetResourceManager()->WrapResource(Function, realMTLFunction, wrappedMTLFunction, true);
    m_Device->AddResource(Function, ResourceType::Shader, "Function");
    m_Device->GetReplay()->AddShader(Function, GetResID(Library), realMTLFunction,
                                     FunctionName ? FunctionName->utf8String() : "");
    m_Device->DerivedResource(Library, Function);
  }
  return true;
}

WrappedMTLFunction *WrappedMTLLibrary::newFunctionWithName(NS::String *functionName)
{
  MTL::Function *realMTLFunction;
  SERIALISE_TIME_CALL(realMTLFunction = Unwrap(this)->newFunction(functionName));
  if(!realMTLFunction)
    return NULL;

  WrappedMTLFunction *wrappedMTLFunction;
  ResourceId id =
      GetResourceManager()->WrapResource(ResourceId(), realMTLFunction, wrappedMTLFunction);

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;
    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(MetalChunk::MTLLibrary_newFunctionWithName);
      Serialise_newFunctionWithName(ser, wrappedMTLFunction, functionName);
      chunk = scope.Get();
    }
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrappedMTLFunction);
    record->AddChunk(chunk);
    record->AddParent(GetRecord(this));
  }
  else
  {
    // TODO: implement RD MTL replay
  }
  return wrappedMTLFunction;
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLLibrary, WrappedMTLFunction *function,
                                            newFunctionWithName, NS::String *functionName);

template <typename SerialiserType>
bool WrappedMTLLibrary::Serialise_newSpecializedFunction(SerialiserType &ser,
                                                         WrappedMTLFunction *function,
                                                         MetalFunctionSnapshot snapshot)
{
  SERIALISE_ELEMENT_LOCAL(Library, this);
  SERIALISE_ELEMENT_LOCAL(Function, GetResID(function)).TypedAs("MTLFunction"_lit);
  SERIALISE_ELEMENT_LOCAL(supported, snapshot.supported);
  SERIALISE_ELEMENT_LOCAL(functionName, snapshot.name).Important();
  SERIALISE_ELEMENT_LOCAL(specializedName, snapshot.specializedName);
  SERIALISE_ELEMENT_LOCAL(options, snapshot.options);
  SERIALISE_ELEMENT_LOCAL(constantNames, snapshot.constantNames);
  SERIALISE_ELEMENT_LOCAL(constantIndices, snapshot.constantIndices);
  SERIALISE_ELEMENT_LOCAL(constantTypes, snapshot.constantTypes);
  SERIALISE_ELEMENT_LOCAL(constantValues, snapshot.constantValues);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!supported || !Library || Library->m_Type != eResLibrary || !Library->m_Real ||
       Function == ResourceId() || GetResourceManager()->HasResource(Function) ||
       functionName.empty() || functionName.size() > 4096 || specializedName.size() > 4096 ||
       options != 0 || constantValues.size() > 4096 ||
       constantNames.size() != constantValues.size() ||
       constantIndices.size() != constantValues.size() ||
       constantTypes.size() != constantValues.size())
    {
      RDCERR("Invalid or unsupported Metal function identity, descriptor or constant snapshot");
      return false;
    }
    for(size_t i = 0; i < constantValues.size(); i++)
    {
      if(MetalFunctionConstantSize(constantTypes[i]) == 0 ||
         constantValues[i].size() != MetalFunctionConstantSize(constantTypes[i]) ||
         constantIndices[i] >= 65536 || constantNames[i].size() > 4096)
      {
        RDCERR("Invalid Metal function constant type, size, index or name");
        return false;
      }
    }
    MTL::FunctionConstantValues *values = MTL::FunctionConstantValues::alloc()->init();
    for(size_t i = 0; i < constantValues.size(); i++)
    {
      if(constantNames[i].empty())
        values->setConstantValue(constantValues[i].data(), (MTL::DataType)constantTypes[i],
                                  constantIndices[i]);
      else
        values->setConstantValue(constantValues[i].data(), (MTL::DataType)constantTypes[i],
                                  NS::String::string(constantNames[i].c_str(), NS::UTF8StringEncoding));
    }
    NS::Error *error = NULL;
    NS::String *name = NS::String::string(functionName.c_str(), NS::UTF8StringEncoding);
    MTL::Function *real = NULL;
    const MetalChunk chunk = (MetalChunk)ser.ChunkMetadata().chunkID;
    if(chunk == MetalChunk::MTLLibrary_newFunctionWithDescriptor ||
       chunk == MetalChunk::MTLLibrary_newFunctionWithDescriptor_async ||
       chunk == MetalChunk::MTLLibrary_newIntersectionFunctionWithDescriptor)
    {
      MTL::FunctionDescriptor *descriptor =
          chunk == MetalChunk::MTLLibrary_newIntersectionFunctionWithDescriptor ?
              (MTL::FunctionDescriptor *)MTL::IntersectionFunctionDescriptor::alloc()->init() :
              MTL::FunctionDescriptor::alloc()->init();
      descriptor->setName(name);
      descriptor->setConstantValues(values);
      if(!specializedName.empty())
        descriptor->setSpecializedName(NS::String::string(specializedName.c_str(), NS::UTF8StringEncoding));
      real = chunk == MetalChunk::MTLLibrary_newIntersectionFunctionWithDescriptor ?
                 Unwrap(Library)->newIntersectionFunction(
                     (MTL::IntersectionFunctionDescriptor *)descriptor, &error) :
                 Unwrap(Library)->newFunction(descriptor, &error);
      descriptor->release();
    }
    else
    {
      if(!specializedName.empty())
      {
        values->release();
        RDCERR("Specialized name requires a Metal function descriptor");
        return false;
      }
      real = Unwrap(Library)->newFunction(name, values, &error);
    }
    values->release();
    if(!real)
    {
      RDCERR("Failed to recreate Metal specialized function: %s",
             error ? error->localizedDescription()->utf8String() : functionName.c_str());
      return false;
    }
    WrappedMTLFunction *wrapped = NULL;
    GetResourceManager()->WrapResource(Function, real, wrapped, true);
    m_Device->AddResource(Function, ResourceType::Shader, "Specialized Function");
    m_Device->GetReplay()->AddShader(Function, GetResID(Library), real, real->name()->utf8String());
    m_Device->DerivedResource(Library, Function);
  }
  return true;
}

WrappedMTLFunction *WrappedMTLLibrary::CaptureFunction(MTL::Function *real,
                                                       const MetalFunctionSnapshot &snapshot,
                                                       MetalChunk chunk, bool borrowed)
{
  if(!real) return NULL;
  if(borrowed) real->retain();
  WrappedMTLFunction *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_newSpecializedFunction(ser, wrapped, snapshot);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
    record->AddParent(GetRecord(this));
  }
  return wrapped;
}

template bool WrappedMTLLibrary::Serialise_newSpecializedFunction(ReadSerialiser &, WrappedMTLFunction *, MetalFunctionSnapshot);
template bool WrappedMTLLibrary::Serialise_newSpecializedFunction(WriteSerialiser &, WrappedMTLFunction *, MetalFunctionSnapshot);
