#include "CoreMinimal.h"
#include "Misc/EngineVersion.h"
#include "Runtime/Launch/Resources/Version.h"
#include "RHICommandList.h"
#include "renderdoc_app.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 8 && ENGINE_PATCH_VERSION == 3
#include "MetalRHIContext.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogRenderDocMetalLayouts, Log, All);

void AnnotateUE58MetalDescriptorLayouts(RENDERDOC_API_1_7_0 *API,
                                         FRHICommandListImmediate &RHICmdList)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 8 && ENGINE_PATCH_VERSION == 3
  if(!API || !API->SetObjectAnnotation || !GDynamicRHI ||
     FCString::Strcmp(GDynamicRHI->GetName(), TEXT("Metal")) || !GRHIGlobals.bSupportsBindless)
    return;
  // Get the actual native heaps through UE's context/device APIs. Do not identify buffers
  // by labels, resource numbers, sizes or an arbitrary scan of their contents.
  RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
  auto *Context = static_cast<FMetalRHICommandContext *>(GDynamicRHI->RHIGetDefaultContext());
  if(!Context)
    return;
  FMetalDevice &Device = Context->GetDevice();
  FMetalBindlessDescriptorManager *Manager = Device.GetBindlessDescriptorManager();
  if(!Manager || !Manager->IsSupported())
    return;
  FMetalRHIBuffer *Heaps[] = {Manager->GetStandardResourceHeap(), Manager->GetSamplerResourceHeap()};
  for(uint32 Kind = 0; Kind < 2; Kind++)
  {
    const FMetalBufferPtr Buffer = Heaps[Kind]->GetCurrentBufferOrNull();
    if(!Buffer || !Buffer->GetMTLBuffer() || Buffer->GetLength() % 24)
      continue;
    RENDERDOC_AnnotationValue Layout = {};
    Layout.vector.uint64[0] = Kind ? 2 : 1;
    Layout.vector.uint64[1] = Buffer->GetOffset();
    Layout.vector.uint64[2] = Buffer->GetLength() / 24;
    Layout.vector.uint64[3] = 24;
    const uint32 Result = API->SetObjectAnnotation(Device.GetDevice(), Buffer->GetMTLBuffer(),
        "metal.descriptorTable", eRENDERDOC_UInt64, 4, &Layout);
    UE_LOG(LogRenderDocMetalLayouts, Display,
        TEXT("UE58 native %s table layout: offset=%u entries=%u stride=24 annotation=%u"),
        Kind ? TEXT("sampler") : TEXT("resource"), Buffer->GetOffset(), Buffer->GetLength()/24, Result);
  }
  // Layouts alone do not establish complete descriptor coverage or CPU-write provenance.
  // Temporary update heaps, suballocation lifetimes and stale/freed slots still need hooks.
  // Deliberately do not declare metal.descriptorCoverage or permit full UE GPU replay.
  UE_LOG(LogRenderDocMetalLayouts, Display,
      TEXT("UE58 primary layouts recorded; descriptor coverage remains incomplete"));
#else
  UE_LOG(LogRenderDocMetalLayouts, Warning,
      TEXT("Native descriptor layout probe requires the matching UE 5.8.3 Metal headers"));
#endif
}
