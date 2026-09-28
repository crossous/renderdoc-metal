// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_device.h"
#include "metal_library.h"

namespace
{
bytebuf ReadLibraryFile(NS::String *path)
{
  bytebuf bytes;
  NS::Data *file = path ? NS::Data::dataWithContentsOfFile(path) : NULL;
  if(file && file->length())
    bytes.assign((const byte *)file->bytes(), file->length());
  return bytes;
}
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newLibraryBinary(SerialiserType &ser, WrappedMTLLibrary *library,
                                                 rdcstr origin, bytebuf &data)
{
  SERIALISE_ELEMENT_LOCAL(Device, this);
  SERIALISE_ELEMENT_LOCAL(Library, GetResID(library)).TypedAs("MTLLibrary"_lit);
  SERIALISE_ELEMENT(origin);
  SERIALISE_ELEMENT(data);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    // The origin is diagnostic metadata only. Never reopen the captured path during replay.
    if(!Device || Device != this || Library == ResourceId() ||
       GetResourceManager()->HasResource(Library) || data.size() < 4 ||
       memcmp(data.data(), "MTLB", 4) != 0)
    {
      RDCERR("Invalid Metal binary library identity or payload");
      return false;
    }
    dispatch_data_t mapped = dispatch_data_create(
        data.data(), data.size(), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    NS::Error *error = NULL;
    MTL::Library *real = Unwrap(this)->newLibrary(mapped, &error);
    dispatch_release(mapped);
    if(!real)
    {
      RDCERR("Failed to recreate captured Metal binary library: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    WrappedMTLLibrary *wrapped = NULL;
    GetResourceManager()->WrapResource(Library, real, wrapped, true);
    AddResource(Library, ResourceType::Pool, "Binary Library");
    DerivedResource(this, Library);
  }
  return true;
}

WrappedMTLLibrary *WrappedMTLDevice::CaptureLibraryBinary(MTL::Library *real, MetalChunk chunk,
                                                         const rdcstr &origin, bytebuf &data)
{
  if(!real)
    return NULL;
  WrappedMTLLibrary *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    if(data.empty())
      RDCERR("Could not capture the bytes of a successfully loaded Metal library");
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(chunk);
    Serialise_newLibraryBinary(ser, wrapped, origin, data);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

WrappedMTLLibrary *WrappedMTLDevice::newLibraryWithFile(NS::String *path, NS::Error **error)
{
  bytebuf data = ReadLibraryFile(path);
  MTL::Library *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newLibrary(path, error));
  return CaptureLibraryBinary(real, MetalChunk::MTLDevice_newLibraryWithFile,
                              path ? path->utf8String() : "", data);
}

WrappedMTLLibrary *WrappedMTLDevice::newLibraryWithURL(NS::URL *url, NS::Error **error)
{
  const bool fileURL = url && ((BOOL (*)(id, SEL))objc_msgSend)((id)url, sel_registerName("isFileURL"));
  const char *path = fileURL ? url->fileSystemRepresentation() : NULL;
  bytebuf data = ReadLibraryFile(path ? NS::String::string(path, NS::UTF8StringEncoding) : NULL);
  MTL::Library *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newLibrary(url, error));
  return CaptureLibraryBinary(real, MetalChunk::MTLDevice_newLibraryWithURL,
                              path ? path : "", data);
}

WrappedMTLLibrary *WrappedMTLDevice::newLibraryWithData(void *inputData, NS::Error **error)
{
  dispatch_data_t input = (dispatch_data_t)inputData;
  bytebuf data;
  if(input)
  {
    const void *bytes = NULL;
    size_t size = 0;
    dispatch_data_t mapped = dispatch_data_create_map(input, &bytes, &size);
    if(mapped)
    {
      if(size)
        data.assign((const byte *)bytes, size);
      dispatch_release(mapped);
    }
  }
  MTL::Library *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newLibrary(input, error));
  return CaptureLibraryBinary(real, MetalChunk::MTLDevice_newLibraryWithData, "dispatch_data", data);
}

WrappedMTLLibrary *WrappedMTLDevice::newDefaultLibraryWithBundle(NS::Bundle *bundle, NS::Error **error)
{
  NS::String *path = bundle ? bundle->pathForResource(
      NS::String::string("default", NS::UTF8StringEncoding),
      NS::String::string("metallib", NS::UTF8StringEncoding)) : NULL;
  bytebuf data = ReadLibraryFile(path);
  MTL::Library *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newDefaultLibrary(bundle, error));
  return CaptureLibraryBinary(real, MetalChunk::MTLDevice_newDefaultLibraryWithBundle,
                              path ? path->utf8String() : "", data);
}

INSTANTIATE_FUNCTION_WITH_RETURN_SERIALISED(WrappedMTLDevice, WrappedMTLLibrary *,
                                            newLibraryBinary, rdcstr, bytebuf &);
