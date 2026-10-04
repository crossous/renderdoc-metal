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

#pragma once

#include "core/resource_manager.h"
#include "metal_resources.h"

struct MetalInitialContents
{
  MetalInitialContents() : type(eResUnknown) {}

  MetalInitialContents(MetalResourceType t) : type(t) {}

  MetalInitialContents(MetalResourceType t, bytebuf data)
      : resourceContents(data), type(t) {}

  template <typename Configuration>
  void Free(ResourceManager<Configuration> *rm)
  {
    // Initial buffer bytes are owned by bytebuf. ResourceManager calls Free when
    // replacing or discarding a snapshot; there is no Metal object to release.
    resourceContents.clear();
  }
  bytebuf resourceContents;

  // for plain resources, we store the resource type
  MetalResourceType type;
};

struct MetalResourceManagerConfiguration
{
  typedef WrappedMTLObject *WrappedResourceType;
  typedef void *RealResourceType;
  typedef MetalResourceRecord RecordType;
  typedef MetalInitialContents InitialContentData;
};

class MetalResourceManager : public ResourceManager<MetalResourceManagerConfiguration>
{
public:
  MetalResourceManager(CaptureState &state, WrappedMTLDevice *device)
      : ResourceManager(state), m_Device(device)
  {
  }
  void SetState(CaptureState state) { m_State = state; }
  CaptureState GetState() { return m_State; }
  WrappedMTLObject *FindAnnotationObject(void *object)
  {
    SCOPED_LOCK(m_Lock);
    for(const auto &entry : m_ResourceMap)
      if((void *)entry.second == object)
        return entry.second;
    return NULL;
  }
  void RefGPUIdentityResources()
  {
    // Native-address descriptor tables do not expose their resource dependencies through
    // encoder setters. Like D3D12 RefBuffers, conservatively retain every live resource
    // whose identity the application queried, including samplers (which have no heap).
    // Explicitly retired heap allocations are excluded even when a completed native command
    // buffer still retains their proxies; historical descriptor bindings need no live allocation.
    SCOPED_LOCK(m_Lock);
    for(const auto &entry : m_ResourceMap)
      if(entry.second && !Atomic::CmpExch32(&entry.second->m_CapturedAliasable, 0, 0) &&
         Atomic::CmpExch32(&entry.second->m_CapturedGPUIdentity, 0, 0))
        MarkResourceFrameReferenced(entry.first, eFrameRef_Read);
  }
  void AddSharedHeapBufferReferences(std::unordered_set<ResourceId> &references)
  {
    // useHeap(s) permits indirect access to every allocation on the heap. As with
    // D3D12's bindless buffer references, explicit setBuffer/useResource calls are
    // insufficient to discover the mapped CPU bytes consumed by this submission.
    SCOPED_LOCK(m_Lock);
    std::set<MTL::Heap *> heaps;
    for(ResourceId id : references)
    {
      auto found = m_ResourceMap.find(id);
      if(found != m_ResourceMap.end() && found->second && found->second->m_Type == eResHeap &&
         found->second->m_Real)
        heaps.insert((MTL::Heap *)found->second->m_Real);
    }
    if(heaps.empty()) return;
    for(const auto &entry : m_ResourceMap)
    {
      WrappedMTLObject *object = entry.second;
      if(object && object->m_Type == eResBuffer && object->m_Real &&
         !Atomic::CmpExch32(&object->m_CapturedAliasable, 0, 0))
      {
        MTL::Buffer *buffer = (MTL::Buffer *)object->m_Real;
        if(buffer->storageMode() == MTL::StorageModeShared && heaps.count(buffer->heap()))
          references.insert(entry.first);
      }
    }
  }
  void MarkCapturedTextureViewsRetired(WrappedMTLObject *source)
  {
    if(!source || source->m_Type != eResTexture || !source->m_Real) return;
    SCOPED_LOCK(m_Lock);
    MTL::Texture *native = (MTL::Texture *)source->m_Real;
    for(const auto &entry : m_ResourceMap)
    {
      WrappedMTLObject *object = entry.second;
      if(!object || object->m_Type != eResTexture || !object->m_Real || object == source) continue;
      for(MTL::Texture *parent = ((MTL::Texture *)object->m_Real)->parentTexture();
          parent; parent = parent->parentTexture())
        if(parent == native)
        {
          // Views cannot keep a logically retired placement allocation alive.
          // Keep native retention for submitted GPU work; only conservative
          // next-capture initial references inherit the parent's retirement.
          Atomic::CmpExch32(&object->m_CapturedAliasable, 0, 1);
          break;
        }
    }
  }
  ~MetalResourceManager() {}
  // Replay-only editor objects need the same typed wrapper/native release as shutdown.
  bool ReleaseReplayResource(WrappedMTLObject *resource) { return ResourceTypeRelease(resource); }
  void ClearWithoutReleasing()
  {
    // if any objects leaked past, it's no longer safe to delete them as we would
    // be calling Shutdown() after the device that owns them is destroyed. Instead
    // we just have to leak ourselves.
    RDCASSERT(m_InitialContents.empty());
    RDCASSERT(m_ResourceRecords.empty());
    RDCASSERT(m_ResourceMap.empty());
    RDCASSERT(m_WrapperMap.empty());

    m_InitialContents.clear();
    m_ResourceRecords.clear();
    m_ResourceMap.clear();
    m_WrapperMap.clear();
  }

  // ResourceManager interface
  ResourceId GetID(WrappedMTLObject *res)
  {
    if(res == NULL)
      return ResourceId();

    return res->m_ID;
  }
  // ResourceManager interface

  template <typename realtype>
  ResourceId WrapResource(ResourceId id, realtype obj,
                          typename UnwrapHelper<realtype>::Outer *&wrapped,
                          bool transferOwnership = false)
  {
    RDCASSERT(obj != NULL);
    RDCASSERT(m_Device != NULL);

    // on replay, we provide an ID for replayed versions of capture-time resources. For
    // replay-only/internal resources, we auto-gen a resource id.
    // during capture we should always be auto-gen'ing a resource id for obvious reasons.
    if(id == ResourceId())
      id = ResourceIDGen::GetNewUniqueID();
    else
      RDCASSERT(IsReplayMode(m_State));

    using WrappedType = typename UnwrapHelper<realtype>::Outer;
    wrapped = new WrappedType(obj, id, m_Device);
    wrapped->m_Real = obj;
    wrapped->m_Type = (MetalResourceType)WrappedType::TypeEnum;
    if(IsReplayMode(m_State))
    {
      if(!transferOwnership)
        ((NS::Object *)obj)->retain();
      wrapped->m_OwnsReal = true;
    }
    AddResource(id, wrapped);

    // TODO: implement RD MTL replay
    //    if(IsReplayMode(m_State))
    //     AddWrapper(wrapMetalResourceManager(obj));
    return id;
  }

  template <typename realtype>
  void ReplaceRealResource(WrappedMTLObject *wrapped, realtype obj, bool transferOwnership = false)
  {
    RDCASSERT(wrapped != NULL);

    if(IsReplayMode(m_State) && obj && !transferOwnership)
      ((NS::Object *)obj)->retain();

    if(wrapped->m_Real && wrapped->m_OwnsReal)
      ((NS::Object *)wrapped->m_Real)->release();

    wrapped->m_Real = obj;
    wrapped->m_OwnsReal = IsReplayMode(m_State) && obj != NULL;
  }

  template <typename wrappedtype>
  void ReleaseWrappedResource(wrappedtype *wrapped)
  {
    ResourceId id = GetResID(wrapped);

    // TODO: implement RD MTL replay

    ResourceManager::ReleaseResource(id);
    MetalResourceRecord *record = GetRecord(wrapped);
    if(record)
    {
      record->Delete(this);
    }
    delete wrapped;
  }

  using ResourceManager::AddResourceRecord;

  template <typename wrappedtype>
  MetalResourceRecord *AddResourceRecord(wrappedtype *wrapped)
  {
    MetalResourceRecord *ret = wrapped->m_Record = ResourceManager::AddResourceRecord(wrapped->m_ID);

    ret->m_Resource = (WrappedMTLObject *)wrapped;
    ret->m_Type = (MetalResourceType)wrappedtype::TypeEnum;
    return ret;
  }

  // ResourceRecordHandler interface implemented in ResourceManager
  //  void MarkDirtyResource(ResourceId id);
  //  void RemoveResourceRecord(ResourceId id);
  //  void MarkResourceFrameReferenced(ResourceId id, FrameRefType refType);
  //  void DestroyResourceRecord(ResourceRecord *record);
  // ResourceRecordHandler interface

private:
  // ResourceManager interface
  bool ResourceTypeRelease(WrappedMTLObject *res);
  bool Prepare_InitialState(WrappedMTLObject *res);
  uint64_t GetSize_InitialState(ResourceId id, const MetalInitialContents &initial);
  bool Serialise_InitialState(WriteSerialiser &ser, ResourceId id, MetalResourceRecord *record,
                              const MetalInitialContents *initial);
  void Create_InitialState(ResourceId id, WrappedMTLObject *live, bool hasData);
  void Apply_InitialState(WrappedMTLObject *live, MetalInitialContents &initial);
  // ResourceManager interface

  WrappedMTLDevice *m_Device;
};
