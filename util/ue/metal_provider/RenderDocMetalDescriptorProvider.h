// SPDX-License-Identifier: MIT
// Included only by an isolated, matching UE 5.8.3 MetalRHI source copy.
// Records diagnostic facts; never declares complete replay coverage.
#pragma once
#include "renderdoc_app.h"
#include <dlfcn.h>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace RenderDocMetalDescriptorProvider
{
struct Slot
{
  uint64 Generation = 0;
  uint32 Type = 0;
  bool Live = false;
};
struct Heap
{
  MTL::Device *Device = nullptr;
  MTL::Buffer *Buffer = nullptr;
  uint64 Offset = 0;
  std::map<uint32, Slot> Slots;
};
inline std::mutex Lock;
inline std::map<const void *, Heap> Heaps;
inline uint64 NextGeneration = 0;
struct Source
{
  void *Object = nullptr;
  uint32 Kind = 0;
  uint64 Offset = 0;
};
struct CreatedDescriptor
{
  IRDescriptorTableEntry Value{};
  Source Fields[2];
  // The RHICmdList lambda/context queue outlives the factory call. Keep its known
  // objects alive only until the diagnostic payload/value bindings are recorded,
  // so an allocator cannot reuse the pointer for a different native resource.
  std::shared_ptr<NS::Object> Retained[2];
  bool Known = false;
};
struct QueuedDescriptor
{
  uint32 Index = 0;
  uint32 Type = 0;
  CreatedDescriptor Created;
};
inline thread_local CreatedDescriptor CurrentCreated;
inline thread_local void *LastComputeEncoder = nullptr;
inline std::map<const void *, std::vector<QueuedDescriptor>> Queued;

inline RENDERDOC_API_1_7_0 *API()
{
  static RENDERDOC_API_1_7_0 *Result = []()
  {
    RENDERDOC_API_1_7_0 *Found = nullptr;
    auto GetAPI = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI"));
    if(GetAPI)
      GetAPI(eRENDERDOC_API_Version_1_7_0, reinterpret_cast<void **>(&Found));
    return Found;
  }();
  return Result;
}

inline void Created(const IRDescriptorTableEntry &Value, void *Object, uint32 Kind,
                    uint64 Offset = 0, void *Texture = nullptr)
{
  if(!API())
    return;
  CurrentCreated = {};
  CurrentCreated.Value = Value;
  if(Object && Kind == 0)
  {
    uint64 Address = 0;
    FMemory::Memcpy(&Address, &Value, sizeof(Address));
    const uint64 Base = static_cast<MTL::Buffer *>(Object)->gpuAddress();
    // Metal IR typed buffer views can encode an aligned base plus ordinary element
    // metadata. Record the actual encoded member offset against this known object.
    if(Address && Address >= Base && Address - Base < static_cast<MTL::Buffer *>(Object)->length())
      Offset = Address - Base;
  }
  CurrentCreated.Fields[0] = {Object, Kind, Offset};
  CurrentCreated.Fields[1] = {Texture, 1, 0};
  for(uint32 Index = 0; Index < 2; Index++)
    if(void *Field = CurrentCreated.Fields[Index].Object)
    {
      NS::Object *Known = static_cast<NS::Object *>(Field);
      Known->retain();
      CurrentCreated.Retained[Index] = std::shared_ptr<NS::Object>(Known,
          [](NS::Object *Retired) { Retired->release(); });
    }
  CurrentCreated.Known = true;
}

inline CreatedDescriptor TakeCreated(const IRDescriptorTableEntry &Value)
{
  CreatedDescriptor Result = CurrentCreated;
  CurrentCreated = {};
  if(!Result.Known || FMemory::Memcmp(&Value, &Result.Value, 24))
    Result = {};
  return Result;
}

struct ScopeCreated
{
  CreatedDescriptor Previous;
  ScopeCreated(const CreatedDescriptor &Value) : Previous(CurrentCreated) { CurrentCreated = Value; }
  ~ScopeCreated() { CurrentCreated = Previous; }
};

inline void Bindings(MTL::Device *Device, MTL::Buffer *Buffer, uint64 Offset,
                     const CreatedDescriptor &Created)
{
  if(!Created.Known)
    return;
  for(const Source &Field : Created.Fields)
  {
    if(!Field.Object)
      continue;
    RENDERDOC_AnnotationValue Value = {};
    Value.vector.uint64[0] = Offset;
    Value.vector.uint64[1] = Field.Kind;
    Value.vector.uint64[2] = reinterpret_cast<uintptr_t>(Field.Object);
    Value.vector.uint64[3] = Field.Offset;
    uint32 Result = API()->SetObjectAnnotation(Device, Buffer, "metal.descriptorSlotBinding",
        eRENDERDOC_UInt64, 4, &Value);
    if(Result)
      UE_LOG(LogMetal, Warning, TEXT("RenderDoc descriptor source binding rejected (%u); coverage remains incomplete"), Result);
  }
}

inline void Queue(const void *Context, FRHIDescriptorHandle Handle,
                  const IRDescriptorTableEntry &Value)
{
  if(!API())
    return;
  CreatedDescriptor Created = TakeCreated(Value);
  std::lock_guard<std::mutex> Guard(Lock);
  Queued[Context].push_back({Handle.GetIndex(), uint32(Handle.GetType()), Created});
}

inline std::vector<QueuedDescriptor> TakeQueue(const void *Context,
                                              const FMetalPendingDescriptorUpdates &Updates)
{
  if(!API())
    return {};
  std::lock_guard<std::mutex> Guard(Lock);
  std::vector<QueuedDescriptor> Result;
  Result.swap(Queued[Context]);
  Queued.erase(Context);
  if(Result.size() != size_t(Updates.Num()))
  {
    UE_LOG(LogMetal, Warning, TEXT("RenderDoc descriptor source queue size mismatch; coverage remains incomplete"));
    return {};
  }
  for(int32 Index = 0; Index < Updates.Num(); Index++)
    if(Result[Index].Index != Updates.Indices[Index] ||
       (Result[Index].Created.Known && FMemory::Memcmp(&Result[Index].Created.Value, &Updates.Descriptors[Index], 24)))
    {
      UE_LOG(LogMetal, Warning, TEXT("RenderDoc descriptor source queue order/value mismatch; coverage remains incomplete"));
      return {};
    }
  return Result;
}

inline void Event(const Heap &H, uint32 Index, const Slot &S, uint32 Kind)
{
  RENDERDOC_AnnotationValue Value = {};
  Value.vector.uint64[0] = H.Offset + uint64(Index) * 24;
  Value.vector.uint64[1] = S.Generation;
  Value.vector.uint64[2] = Kind;
  Value.vector.uint64[3] = S.Type;
  const uint32 Result = API()->SetObjectAnnotation(H.Device, H.Buffer,
      "metal.descriptorSlotEvent", eRENDERDOC_UInt64, 4, &Value);
  ensureMsgf(Result == 0, TEXT("RenderDoc Metal descriptor slot event rejected (%u)"), Result);
}

// FlushPendingDescriptorUpdates allocates exactly three handles in its 256-byte
// temporary heap. The remaining allocator capacity is uninitialized, not a typed
// table. Scope the annotation to those three entries without changing UE memory.
inline thread_local uint64 HeapTableCountOverride = 0;
struct HeapTableCountScope
{
  uint64 Previous;
  explicit HeapTableCountScope(uint64 Count) : Previous(HeapTableCountOverride)
  {
    HeapTableCountOverride = Count;
  }
  ~HeapTableCountScope() { HeapTableCountOverride = Previous; }
};

inline void Init(const void *Owner, FMetalDevice &Device, const FMetalBufferPtr &Buffer,
                 bool Sampler, uint64 Count = 0)
{
  if(!API())
    return;
  std::lock_guard<std::mutex> Guard(Lock);
  Heap &H = Heaps[Owner];
  H = {};
  H.Device = Device.GetDevice();
  H.Buffer = Buffer->GetMTLBuffer();
  H.Offset = Buffer->GetOffset();
  RENDERDOC_AnnotationValue Value = {};
  Value.vector.uint64[0] = Sampler ? 2 : 1;
  Value.vector.uint64[1] = H.Offset;
  Value.vector.uint64[2] = HeapTableCountOverride ? HeapTableCountOverride : Count ? Count : Buffer->GetLength() / 24;
  Value.vector.uint64[3] = 24;
  // Temp heaps can be born during capture. The current background-only layout API
  // rejects those; slot events still identify their actual native buffer and offsets.
  const uint32 Result = API()->SetObjectAnnotation(H.Device, H.Buffer,
      "metal.descriptorTable", eRENDERDOC_UInt64, 4, &Value);
  UE_LOG(LogMetal, Display, TEXT("RenderDoc descriptor provider heap: entries=%llu sampler=%u layout=%u"),
         Value.vector.uint64[2], Sampler ? 1 : 0, Result);
}

inline void Destroy(const void *Owner)
{
  if(!API())
    return;
  std::lock_guard<std::mutex> Guard(Lock);
  auto Found = Heaps.find(Owner);
  if(Found != Heaps.end())
    for(const auto &Entry : Found->second.Slots)
      if(Entry.second.Live)
        Event(Found->second, Entry.first, Entry.second, 1);
  Heaps.erase(Owner);
}

inline void Allocate(const void *Owner, FRHIDescriptorHandle Handle)
{
  if(!API() || !Handle.IsValid())
    return;
  std::lock_guard<std::mutex> Guard(Lock);
  auto Found = Heaps.find(Owner);
  if(Found == Heaps.end())
    return;
  Slot &S = Found->second.Slots[Handle.GetIndex()];
  checkf(!S.Live, TEXT("RenderDoc descriptor generation was allocated twice"));
  S.Generation = ++NextGeneration;
  S.Type = uint32(Handle.GetType());
  S.Live = true;
  Event(Found->second, Handle.GetIndex(), S, 0);
}

inline void Free(const void *Owner, FRHIDescriptorHandle Handle)
{
  if(!API())
    return;
  std::lock_guard<std::mutex> Guard(Lock);
  auto Found = Heaps.find(Owner);
  if(Found == Heaps.end())
    return;
  Slot &S = Found->second.Slots.at(Handle.GetIndex());
  checkf(S.Live, TEXT("RenderDoc descriptor generation was freed twice"));
  Event(Found->second, Handle.GetIndex(), S, 1);
  S.Live = false;
}

inline void CPUWrite(const void *Owner, FRHIDescriptorHandle Handle,
                     const IRDescriptorTableEntry &Value)
{
  if(!API())
    return;
  std::lock_guard<std::mutex> Guard(Lock);
  auto Found = Heaps.find(Owner);
  if(Found == Heaps.end())
    return;
  const Slot &S = Found->second.Slots.at(Handle.GetIndex());
  checkf(S.Live, TEXT("RenderDoc CPU descriptor write used a freed generation"));
  Event(Found->second, Handle.GetIndex(), S, 2);
  Bindings(Found->second.Device, Found->second.Buffer,
      Found->second.Offset + uint64(Handle.GetIndex()) * 24, TakeCreated(Value));
}

inline void MarkGPUWrites(FMetalDevice &Device, const FMetalBufferPtr &Buffer)
{
  if(!API()) return;
  RENDERDOC_AnnotationValue Value = {}; Value.uint32 = 1;
  const uint32 Result = API()->SetObjectAnnotation(Device.GetDevice(), Buffer->GetMTLBuffer(),
      "metal.descriptorGPUWrites", eRENDERDOC_UInt32, 0, &Value);
  ensureMsgf(Result == 0, TEXT("RenderDoc diagnostic GPU destination rejected (%u)"), Result);
}

inline void GPUValues(FMetalDevice &Device, const FMetalBufferPtr &Buffer,
                      const FMetalPendingDescriptorUpdates &Updates,
                      const std::vector<QueuedDescriptor> &Sources,
                      const FMetalBufferPtr &PayloadBuffer)
{
  if(!API())
    return;
  static_assert(sizeof(IRDescriptorTableEntry) == 24);
  for(int32 Index = 0; Index < Updates.Num(); Index++)
  {
    RENDERDOC_AnnotationValue Value = {};
    Value.vector.uint64[0] = Buffer->GetOffset() + uint64(Updates.Indices[Index]) * 24;
    Value.vector.uint64[1] = reinterpret_cast<uintptr_t>(LastComputeEncoder);
    Value.vector.uint64[2] = reinterpret_cast<uintptr_t>(PayloadBuffer->GetMTLBuffer());
    Value.vector.uint64[3] = PayloadBuffer->GetOffset() + uint64(Index) * 24;
    // Only active capture accepts producer records. Background GPU values below
    // still seed the next completed initial-state snapshot without claiming coverage.
    API()->SetObjectAnnotation(Device.GetDevice(), Buffer->GetMTLBuffer(),
        "metal.descriptorSlotProducer", eRENDERDOC_UInt64, 4, &Value);
    FMemory::Memcpy(&Value.vector.uint64[1], &Updates.Descriptors[Index], 24);
    const uint32 Result = API()->SetObjectAnnotation(Device.GetDevice(), Buffer->GetMTLBuffer(),
        "metal.descriptorSlotGPUValue", eRENDERDOC_UInt64, 4, &Value);
    ensureMsgf(Result == 0, TEXT("RenderDoc GPU descriptor value rejected (%u)"), Result);
    if(Sources.size() == size_t(Updates.Num()))
      Bindings(Device.GetDevice(), Buffer->GetMTLBuffer(), Value.vector.uint64[0], Sources[Index].Created);
  }
}

// Exact suballocation leases. The surrounding upload allocator also contains
// ordinary uniforms and integer indices, which are deliberately not declared.
inline void Payload(const void *Owner, FMetalDevice &Device, const FMetalBufferPtr &Buffer,
                    const std::vector<QueuedDescriptor> &Sources)
{
  if(!API())
    return;
  std::lock_guard<std::mutex> Guard(Lock);
  Heap &H = Heaps[Owner];
  checkf(!H.Buffer, TEXT("RenderDoc descriptor payload lease overlaps a live owner"));
  H.Device = Device.GetDevice();
  H.Buffer = Buffer->GetMTLBuffer();
  H.Offset = Buffer->GetOffset();
  for(uint32 Index = 0; Index < Sources.size(); Index++)
  {
    Slot &S = H.Slots[Index];
    S = {++NextGeneration, Sources[Index].Type, true};
    Event(H, Index, S, 0);
    Event(H, Index, S, 2);
    Bindings(H.Device, H.Buffer, H.Offset + uint64(Index) * 24, Sources[Index].Created);
  }
}

inline std::map<const void *, Source> StaticSources;
inline std::map<const void *, std::map<uint64, Source>> InlineSources;
inline void StaticTable(const void *Owner, FMetalDevice &Device, const FMetalBufferPtr &Buffer,
                        const TArray<MTL::SamplerState *> &Samplers)
{
  if(!API())
    return;
  Init(Owner, Device, Buffer, true, Samplers.Num());
  std::lock_guard<std::mutex> Guard(Lock);
  StaticSources[Owner] = {Buffer->GetMTLBuffer(), 0, Buffer->GetOffset()};
  Heap &H = Heaps.at(Owner);
  for(uint32 Index = 0; Index < uint32(Samplers.Num()); Index++)
  {
    Slot &S = H.Slots[Index];
    S = {++NextGeneration, 7, true};
    Event(H, Index, S, 0);
    Event(H, Index, S, 2);
    CreatedDescriptor D;
    D.Known = true;
    D.Fields[0] = {Samplers[Index], 2, 0};
    Bindings(H.Device, H.Buffer, H.Offset + uint64(Index) * 24, D);
  }
}

inline void InlineSource(const void *Owner, uint32 Group, uint32 Index,
                         const FMetalBufferPtr &Buffer, uint64 ExtraOffset = 0)
{
  if(!API())
    return;
  std::lock_guard<std::mutex> Guard(Lock);
  InlineSources[Owner][uint64(Group) << 32 | Index] =
      {Buffer->GetMTLBuffer(), 0, Buffer->GetOffset() + ExtraOffset};
}

inline void InlineReset(const void *Owner)
{
  if(!API())
    return;
  std::lock_guard<std::mutex> Guard(Lock);
  InlineSources.erase(Owner);
}

inline uint32 Stage(MTL::FunctionType Type)
{
  switch(Type)
  {
    case MTL::FunctionTypeKernel: return 0;
    case MTL::FunctionTypeVertex: return 1;
    case MTL::FunctionTypeFragment: return 2;
    case MTL::FunctionTypeObject: return 3;
    case MTL::FunctionTypeMesh: return 4;
    default: return 5;
  }
}

inline void DrawConstants(FMetalDevice &Device, void *Encoder, uint32 Index, uint64 Length)
{
  if(!API()) return;
  RENDERDOC_AnnotationValue Value = {};
  Value.vector.uint64[0] = 1; // vertex
  Value.vector.uint64[1] = Index;
  Value.vector.uint64[2] = Length;
  API()->SetObjectAnnotation(Device.GetDevice(), Encoder, "metal.inlineDrawConstants",
                            eRENDERDOC_UInt64, 4, &Value);
}

// UE's bindless residency flush registers every writable buffer/UAV texture with
// useResource(s); only read-only heap resources can skip those declarations.
// Call after all vertex/pixel IR resource commits, before the native draw.
inline void RenderWritesDeclared(FMetalDevice &Device, void *Encoder)
{
  if(!API() || !Encoder) return;
  RENDERDOC_AnnotationValue Value = {};
  Value.uint32 = 1;
  const uint32 Result = API()->SetObjectAnnotation(Device.GetDevice(), Encoder,
      "metal.renderWritesDeclared", eRENDERDOC_UInt32, 0, &Value);
  if(Result)
    UE_LOG(LogMetal, Warning, TEXT("RenderDoc render write declaration rejected (%u)"), Result);
}

inline void Inline(FMetalDevice &Device, const void *Owner, uint32 Group, void *Encoder,
                   uint32 Stage, uint32 Index, const void *Bytes, uint64 Count, uint64 Stride,
                   const void *StaticOwner = nullptr)
{
  if(Stage == 0) LastComputeEncoder = Encoder;
  if(!API())
    return;
  RENDERDOC_AnnotationValue Value = {};
  Value.vector.uint64[0] = Stage;
  Value.vector.uint64[1] = Index;
  Value.vector.uint64[2] = Count;
  Value.vector.uint64[3] = Stride;
  if(API()->SetObjectAnnotation(Device.GetDevice(), Encoder, "metal.descriptorInlineLayout",
                               eRENDERDOC_UInt64, 4, &Value))
    return; // background, unsupported encoder or incomplete layout: no coverage claim
  std::lock_guard<std::mutex> Guard(Lock);
  for(uint64 Entry = 0; Entry < Count; Entry++)
  {
    uint64 Raw = 0;
    FMemory::Memcpy(&Raw, static_cast<const uint8 *>(Bytes) + Entry * Stride, 8);
    if(!Raw)
      continue;
    Source S;
    if(StaticOwner && Entry + 1 == Count)
    {
      auto Found = StaticSources.find(StaticOwner);
      if(Found != StaticSources.end()) S = Found->second;
    }
    else
    {
      auto Found = InlineSources[Owner].find(uint64(Group) << 32 | Entry);
      if(Found != InlineSources[Owner].end()) S = Found->second;
    }
    Value.vector.uint64[0] = uint64(Stage) << 32 | Index;
    Value.vector.uint64[1] = Entry;
    Value.vector.uint64[2] = reinterpret_cast<uintptr_t>(S.Object);
    Value.vector.uint64[3] = S.Offset;
    // RenderDoc resolves the known wrapper without dereferencing a stale raw pointer.
    const uint32 Result = API()->SetObjectAnnotation(Device.GetDevice(), Encoder,
        "metal.descriptorInlineBinding", eRENDERDOC_UInt64, 4, &Value);
    if(Result)
      UE_LOG(LogMetal, Warning, TEXT("RenderDoc inline source binding rejected (%u); coverage remains incomplete"), Result);
  }
}
} // namespace RenderDocMetalDescriptorProvider
