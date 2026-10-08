// SPDX-License-Identifier: MIT
// Test-only full renderer observations. No RenderDoc injection or replay is allowed.
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/SceneViewport.h"
#include "UnrealClient.h"
#include <dlfcn.h>

bool StartMetalNativeFrameBaseline()
{
  const FString Directory = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_METAL_NATIVE_BASELINE_DIR"));
  if(Directory.IsEmpty()) return false;
  // This mode must never silently turn a replay experiment into a Native baseline.
  if(dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI") || FPaths::IsRelative(Directory))
  {
    UE_LOG(LogTemp, Error, TEXT("NATIVE_BASELINE refused: injected RenderDoc or relative output path"));
    FPlatformMisc::RequestExit(false);
    return true;
  }
  struct FState { double ReadyAt = 0; uint32 Frame = 0; FSceneViewport *Viewport = nullptr; };
  auto State = MakeShared<FState>();
  FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
    [Directory, State](float) {
      FViewport *Viewport = GEditor ? GEditor->GetActiveViewport() : nullptr;
      if(!Viewport || Viewport->GetSizeXY().GetMin() <= 0) return true;
      if(!State->Viewport)
      {
        const int32 Width = FCString::Atoi(*FPlatformMisc::GetEnvironmentVariable(TEXT("UE_METAL_CAPTURE_VIEWPORT_WIDTH")));
        const int32 Height = FCString::Atoi(*FPlatformMisc::GetEnvironmentVariable(TEXT("UE_METAL_CAPTURE_VIEWPORT_HEIGHT")));
        if(Width < 64 || Height < 64 || Width > 512 || Height > 512 ||
           !IFileManager::Get().MakeDirectory(*Directory, true))
        {
          UE_LOG(LogTemp, Error, TEXT("NATIVE_BASELINE invalid bounded dimensions or output directory"));
          FPlatformMisc::RequestExit(false); return false;
        }
        State->Viewport = static_cast<FSceneViewport *>(Viewport);
        State->Viewport->SetFixedViewportSize(Width, Height);
        State->ReadyAt = FPlatformTime::Seconds() + 2.0;
        Viewport->Draw(true);
        return true;
      }
      if(State->Viewport != Viewport)
      {
        UE_LOG(LogTemp, Error, TEXT("NATIVE_BASELINE active viewport changed"));
        FPlatformMisc::RequestExit(false); return false;
      }
      if(FPlatformTime::Seconds() < State->ReadyAt) return true;
      if(!State->Frame)
      {
        for(const TCHAR *Name : {TEXT("r.Test.FreezeTemporalHistories"), TEXT("r.Test.FreezeTemporalSequences")})
        {
          IConsoleVariable *Variable = IConsoleManager::Get().FindConsoleVariable(Name);
          if(!Variable)
          {
            UE_LOG(LogTemp, Error, TEXT("NATIVE_BASELINE missing required cvar %s"), Name);
            FPlatformMisc::RequestExit(false); return false;
          }
          Variable->Set(1.0f, ECVF_SetByConsole);
          UE_LOG(LogTemp, Display, TEXT("NATIVE_BASELINE %s=%f"), Name, Variable->GetFloat());
        }
      }
      // Wait only at the end of each complete viewport frame, never between passes.
      // This may still affect inter-frame scheduling. These observations alone do
      // not prove all UE inputs fixed, actual RT dispatch, or a tolerance for replay.
      Viewport->Draw(true);
      FlushRenderingCommands();
      FReadSurfaceDataFlags Flags(RCM_UNorm);
      Flags.SetLinearToGamma(false);
      TArray<FColor> Pixels;
      const FString File = FPaths::Combine(Directory, FString::Printf(TEXT("native-frame-%u.bgra"), State->Frame));
      const FIntPoint Size = Viewport->GetSizeXY();
      if(IFileManager::Get().FileExists(*File) || !Viewport->ReadPixels(Pixels, Flags) ||
         Pixels.Num() != Size.X * Size.Y ||
         !FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8 *>(Pixels.GetData()), Pixels.Num() * sizeof(FColor)), *File))
      {
        UE_LOG(LogTemp, Error, TEXT("NATIVE_BASELINE output failed (existing file, readback or write)"));
        FPlatformMisc::RequestExit(false); return false;
      }
      UE_LOG(LogTemp, Display, TEXT("NATIVE_BASELINE frame=%u size=%dx%d bytes=%d linearToGamma=0 path=%s"),
             State->Frame, Size.X, Size.Y, Pixels.Num() * int32(sizeof(FColor)), *File);
      if(++State->Frame == 3)
      {
        UE_LOG(LogTemp, Display, TEXT("NATIVE_BASELINE EXECUTION COMPLETE; comparisons and overall acceptance unvalidated"));
        FPlatformMisc::RequestExit(false); return false;
      }
      return true;
    }), 6.0f);
  return true;
}
