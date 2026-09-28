// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Baldur Karlsson
#include "metal_dynamic_library.h"
#include "metal_device.h"
#include "metal_library.h"
#include "metal_manager.h"
#include "os/os_specific.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

WrappedMTLDynamicLibrary::~WrappedMTLDynamicLibrary()
{
  if(!m_TemporaryPath.empty()) unlink(m_TemporaryPath.c_str());
  if(!m_TemporaryDirectory.empty()) rmdir(m_TemporaryDirectory.c_str());
}

WrappedMTLDynamicLibrary::WrappedMTLDynamicLibrary(MTL::DynamicLibrary *real, ResourceId id,
                                                   WrappedMTLDevice *device)
    : WrappedMTLObject(real, id, device, device->GetStateRef())
{
  if(real && id != ResourceId() && IsCaptureMode(m_State))
    AllocateObjCBridge(this);
}

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newDynamicLibrary(SerialiserType &ser,
    WrappedMTLDynamicLibrary *dynamic, WrappedMTLLibrary *library, bool supported)
{
  SERIALISE_ELEMENT_LOCAL(DynamicLibrary, GetResID(dynamic)).TypedAs("MTLDynamicLibrary"_lit).Important();
  SERIALISE_ELEMENT(library).Important();
  SERIALISE_ELEMENT(supported).Important();
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    if(!supported || DynamicLibrary == ResourceId() ||
       GetResourceManager()->HasResource(DynamicLibrary) || !library ||
       library->m_Type != eResLibrary || !library->m_Real ||
       Unwrap(library)->type() != MTL::LibraryTypeDynamic ||
       library->m_DynamicInstallPath.empty())
    {
      RDCERR("Invalid or unsupported Metal dynamic library identity or source");
      return false;
    }
    NS::Error *error = NULL;
    MTL::DynamicLibrary *real = Unwrap(this)->newDynamicLibrary(Unwrap(library),&error);
    if(!real)
    {
      RDCERR("Failed to recreate Metal dynamic library: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    NS::String *path = NS::String::string(library->m_DynamicInstallPath.c_str(),
                                         NS::UTF8StringEncoding);
    NS::URL *url = NS::URL::fileURLWithPath(path);
    error = NULL;
    if(!real->serializeToURL(url,&error))
    {
      RDCERR("Failed to materialize replay Metal dynamic library: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      real->release();
      return false;
    }
    WrappedMTLDynamicLibrary *wrapped = NULL;
    GetResourceManager()->WrapResource(DynamicLibrary,real,wrapped,true);
    AddResource(DynamicLibrary,ResourceType::Pool,"Dynamic Library");
    DerivedResource(library,DynamicLibrary);
  }
  return true;
}

WrappedMTLDynamicLibrary *WrappedMTLDevice::newDynamicLibrary(WrappedMTLLibrary *library,
                                                              NS::Error **error)
{
  if(!library) return NULL;
  MTL::DynamicLibrary *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newDynamicLibrary(Unwrap(library),error));
  if(!real) return NULL;
  WrappedMTLDynamicLibrary *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(),real,wrapped);
  if(IsCaptureMode(m_State))
  {
    const bool supported = Unwrap(library)->type() == MTL::LibraryTypeDynamic;
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newDynamicLibrary);
    Serialise_newDynamicLibrary(ser,wrapped,library,supported);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddParent(GetRecord(library));
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newDynamicLibrary(
    ReadSerialiser &, WrappedMTLDynamicLibrary *, WrappedMTLLibrary *, bool);
template bool WrappedMTLDevice::Serialise_newDynamicLibrary(
    WriteSerialiser &, WrappedMTLDynamicLibrary *, WrappedMTLLibrary *, bool);

template <typename SerialiserType>
bool WrappedMTLDevice::Serialise_newDynamicLibraryWithURL(SerialiserType &ser,
    WrappedMTLDynamicLibrary *dynamic, rdcstr origin, rdcstr installName, bytebuf &data)
{
  SERIALISE_ELEMENT_LOCAL(Device, this);
  SERIALISE_ELEMENT_LOCAL(DynamicLibrary, GetResID(dynamic))
      .TypedAs("MTLDynamicLibrary"_lit).Important();
  SERIALISE_ELEMENT(origin);
  SERIALISE_ELEMENT(installName).Important();
  SERIALISE_ELEMENT(data);
  SERIALISE_CHECK_READ_ERRORS();
  if(IsReplayingAndReading())
  {
    // The original path is diagnostic only: replay needs the captured binary bytes.
    rdcstr base = FileIO::GetTempFolderFilename() + "renderdoc-metal-url.";
    const rdcstr suffix = "/library.metallib";
    if(!Device || Device != this || DynamicLibrary == ResourceId() ||
       GetResourceManager()->HasResource(DynamicLibrary) || origin.size() > 4096 ||
       installName.size() < base.size() + 6 + suffix.size() ||
       installName.size() > 200 || data.size() < 4 || data.size() > 64 * 1024 * 1024)
    {
      RDCERR("Invalid Metal URL dynamic library identity or payload");
      return false;
    }
    rdcstr pattern = base;
    const size_t padding = installName.size() - base.size() - 6 - suffix.size();
    for(size_t i = 0; i < padding; i++) pattern += 'p';
    pattern += "XXXXXX";
    rdcarray<char> chars(pattern.c_str(), pattern.size());
    chars.push_back(0);
    char *created = mkdtemp(chars.data());
    if(!created)
    {
      RDCERR("Could not create temporary Metal URL dynamic library directory");
      return false;
    }
    rdcstr directory = created;
    rdcstr path = directory + "/library.metallib";
    bytebuf relocated = data;
    size_t replacements = 0;
    for(size_t i = 0; i + installName.size() <= relocated.size(); i++)
    {
      if(memcmp(relocated.data() + i, installName.c_str(), installName.size()) == 0)
      {
        memcpy(relocated.data() + i, path.c_str(), path.size());
        replacements++;
        i += installName.size() - 1;
      }
    }
    if(!replacements)
    {
      rmdir(directory.c_str());
      RDCERR("Captured Metal URL dynamic library has no relocatable install name");
      return false;
    }
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    size_t written = 0;
    while(fd >= 0 && written < relocated.size())
    {
      ssize_t n = write(fd, relocated.data() + written, relocated.size() - written);
      if(n > 0) written += size_t(n);
      else if(n < 0 && errno == EINTR) continue;
      else break;
    }
    bool saved = fd >= 0 && written == relocated.size();
    if(fd >= 0 && close(fd) != 0) saved = false;
    if(!saved)
    {
      unlink(path.c_str());
      rmdir(directory.c_str());
      RDCERR("Could not materialize captured Metal URL dynamic library");
      return false;
    }
    NS::URL *url = NS::URL::fileURLWithPath(
        NS::String::string(path.c_str(), NS::UTF8StringEncoding));
    NS::Error *error = NULL;
    MTL::DynamicLibrary *real = Unwrap(this)->newDynamicLibrary(url, &error);
    if(!real)
    {
      unlink(path.c_str());
      rmdir(directory.c_str());
      RDCERR("Failed to recreate captured Metal URL dynamic library: %s",
             error ? error->localizedDescription()->utf8String() : "unknown error");
      return false;
    }
    if(!real->installName() ||
       strcmp(real->installName()->utf8String(), path.c_str()) != 0)
    {
      real->release();
      unlink(path.c_str());
      rmdir(directory.c_str());
      RDCERR("Relocated Metal URL dynamic library install name did not match materialized path");
      return false;
    }
    WrappedMTLDynamicLibrary *wrapped = NULL;
    GetResourceManager()->WrapResource(DynamicLibrary, real, wrapped, true);
    wrapped->m_TemporaryPath = path;
    wrapped->m_TemporaryDirectory = directory;
    AddResource(DynamicLibrary, ResourceType::Pool, "URL Dynamic Library");
    DerivedResource(this, DynamicLibrary);
  }
  return true;
}

WrappedMTLDynamicLibrary *WrappedMTLDevice::newDynamicLibraryWithURL(NS::URL *url,
                                                                      NS::Error **error)
{
  const bool fileURL = url &&
      ((BOOL (*)(id, SEL))objc_msgSend)((id)url, sel_registerName("isFileURL"));
  const char *path = fileURL ? url->fileSystemRepresentation() : NULL;
  bytebuf data;
  NS::Data *file = path ? NS::Data::dataWithContentsOfFile(
      NS::String::string(path, NS::UTF8StringEncoding)) : NULL;
  if(file && file->length())
    data.assign((const byte *)file->bytes(), file->length());
  MTL::DynamicLibrary *real = NULL;
  SERIALISE_TIME_CALL(real = Unwrap(this)->newDynamicLibrary(url, error));
  if(!real) return NULL;
  NS::String *name = real->installName();
  const char *install = name ? name->utf8String() : NULL;
  if(IsCaptureMode(m_State))
  {
    const size_t minimum = FileIO::GetTempFolderFilename().size() +
                           strlen("renderdoc-metal-url.") + 6 +
                           strlen("/library.metallib");
    const size_t nameLength = install ? strlen(install) : 0;
    bool found = false;
    for(size_t i = 0; install && i + nameLength <= data.size() && !found; i++)
      found = memcmp(data.data() + i, install, nameLength) == 0;
    if(!path || strlen(path) > 4096 || data.size() < 4 ||
       data.size() > 64 * 1024 * 1024 || nameLength < minimum ||
       nameLength > 200 || !found)
    {
      RDCERR("Metal URL dynamic library cannot be captured with relocatable install name");
      real->release();
      return NULL;
    }
  }
  WrappedMTLDynamicLibrary *wrapped = NULL;
  GetResourceManager()->WrapResource(ResourceId(), real, wrapped);
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(MetalChunk::MTLDevice_newDynamicLibraryWithURL);
    Serialise_newDynamicLibraryWithURL(ser, wrapped, path ? path : "",
                                       install ? install : "", data);
    MetalResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped);
    record->AddChunk(scope.Get());
  }
  return wrapped;
}

template bool WrappedMTLDevice::Serialise_newDynamicLibraryWithURL(
    ReadSerialiser &, WrappedMTLDynamicLibrary *, rdcstr, rdcstr, bytebuf &);
template bool WrappedMTLDevice::Serialise_newDynamicLibraryWithURL(
    WriteSerialiser &, WrappedMTLDynamicLibrary *, rdcstr, rdcstr, bytebuf &);
