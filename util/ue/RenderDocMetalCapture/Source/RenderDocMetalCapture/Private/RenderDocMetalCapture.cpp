#include "CoreMinimal.h"
#include "CoreGlobals.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/ThreadSafeCounter.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "Styling/AppStyle.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SWindow.h"
#include "ToolMenus.h"
#include "UnrealClient.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "renderdoc_app.h"

#include <dlfcn.h>

DEFINE_LOG_CATEGORY_STATIC(LogRenderDocMetalCapture, Log, All);

#define LOCTEXT_NAMESPACE "RenderDocMetalCapture"

void AnnotateUE58MetalDescriptorLayouts(RENDERDOC_API_1_7_0 *API, FRHICommandListImmediate &RHICmdList);
bool StartMetalNativeFrameBaseline();

struct FRenderDocMetalCaptureEndState
{
  FThreadSafeCounter Phase; // 0=waiting, 1=render command pending, 2=finished
  void *Device = nullptr;  // Accessed only by ordered render-thread commands.
  double PresentDeadline = 0.0; // Start only after capture metadata preparation completes.
  FTextureRHIRef NativeViewportTexture;
  FIntPoint NativeViewportSize = FIntPoint::ZeroValue;
  FString NativeViewportAfterCapture;
};

class FRenderDocMetalCaptureModule final : public IModuleInterface
{
public:
  virtual void StartupModule() override
  {
    if(StartMetalNativeFrameBaseline()) return;
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FRenderDocMetalCaptureModule::RegisterMenus));
    ResolveAPI();
    const FString AutoCaptureDelay =
        FPlatformMisc::GetEnvironmentVariable(TEXT("UE_METAL_AUTO_CAPTURE_DELAY_SECONDS"));
    if(!AutoCaptureDelay.IsEmpty())
    {
      const float Delay = FCString::Atof(*AutoCaptureDelay);
      if(Delay >= 1.0f && Delay <= 120.0f)
        AutoCaptureHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([this](float) {
              CaptureFrame();
              AutoCaptureHandle.Reset();
              return false;
            }), Delay);
    }
  }

  virtual void ShutdownModule() override
  {
    if(PollHandle.IsValid())
      FTSTicker::RemoveTicker(PollHandle);
    if(AutoCaptureHandle.IsValid())
      FTSTicker::RemoveTicker(AutoCaptureHandle);
    if(EndCaptureHandle.IsValid())
      FTSTicker::RemoveTicker(EndCaptureHandle);
    if(CaptureWarmupHandle.IsValid())
      FTSTicker::RemoveTicker(CaptureWarmupHandle);
    RestoreCaptureSize();
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
  }

private:
  RENDERDOC_API_1_0_0 *API = nullptr;
  FTSTicker::FDelegateHandle PollHandle;
  FTSTicker::FDelegateHandle AutoCaptureHandle;
  FTSTicker::FDelegateHandle EndCaptureHandle;
  FTSTicker::FDelegateHandle CaptureWarmupHandle;
  bool bOldEmitDrawEvents = false;
  uint32 CapturesBefore = 0;
  double CaptureDeadline = 0.0;
  TSharedPtr<FRenderDocMetalCaptureEndState, ESPMode::ThreadSafe> CaptureEndState;
  FSceneViewport *SizedViewport = nullptr;
  FIntPoint PreviousViewportSize = FIntPoint::ZeroValue;
  bool bPreviousFixedViewportSize = false;
  bool bCaptureSizeWarm = false;
  TWeakPtr<SWindow> SizedWindow;
  FVector2D PreviousWindowSize = FVector2D::ZeroVector;
  EWindowMode::Type PreviousWindowMode = EWindowMode::Windowed;
  bool bPreviousWindowMaximized = false;

  void RestoreCaptureSize()
  {
    FViewport *Current = GEditor ? GEditor->GetActiveViewport() : nullptr;
    if(GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport == SizedViewport)
      Current = GEngine->GameViewport->Viewport;
    if(SizedViewport && Current == SizedViewport)
      SizedViewport->SetFixedViewportSize(bPreviousFixedViewportSize ? PreviousViewportSize.X : 0,
                                         bPreviousFixedViewportSize ? PreviousViewportSize.Y : 0);
    SizedViewport = nullptr;
    bCaptureSizeWarm = false;
    if(TSharedPtr<SWindow> Window = SizedWindow.Pin())
    {
      Window->Resize(PreviousWindowSize);
      if(PreviousWindowMode != EWindowMode::Windowed)
        Window->SetWindowMode(PreviousWindowMode);
      else if(bPreviousWindowMaximized)
        Window->Maximize();
    }
    SizedWindow.Reset();
  }

  bool ConfigureCaptureSize(FViewport *Viewport)
  {
    const FString WidthText = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_METAL_CAPTURE_VIEWPORT_WIDTH"));
    const FString HeightText = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_METAL_CAPTURE_VIEWPORT_HEIGHT"));
    const FString WindowWidthText = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_METAL_CAPTURE_WINDOW_WIDTH"));
    const FString WindowHeightText = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_METAL_CAPTURE_WINDOW_HEIGHT"));
    if(WidthText.IsEmpty() && HeightText.IsEmpty() && WindowWidthText.IsEmpty() && WindowHeightText.IsEmpty())
      return true;
    FSceneViewport *Scene = Viewport->AsSceneViewport();
    const int32 Width = FCString::Atoi(*WidthText), Height = FCString::Atoi(*HeightText);
    const int32 WindowWidth = FCString::Atoi(*WindowWidthText), WindowHeight = FCString::Atoi(*WindowHeightText);
    if(!Scene || Width < 32 || Height < 32 || Width > 2048 || Height > 2048 ||
       ((!WindowWidthText.IsEmpty() || !WindowHeightText.IsEmpty()) &&
        (WindowWidth < 320 || WindowHeight < 240 || WindowWidth > 2048 || WindowHeight > 2048)))
    {
      Notify(LOCTEXT("InvalidCaptureSize", "Invalid controlled capture size; viewport must be 32..2048 and window 320x240..2048x2048."), true);
      return false;
    }
    SizedViewport = Scene;
    PreviousViewportSize = Scene->GetSizeXY();
    bPreviousFixedViewportSize = Scene->HasFixedSize();
    if(WindowWidth > 0)
      if(TSharedPtr<SWindow> Window = Scene->FindWindow())
      {
        SizedWindow = Window;
        PreviousWindowSize = Window->GetClientSizeInScreen();
        PreviousWindowMode = Window->GetWindowMode();
        bPreviousWindowMaximized = Window->IsWindowMaximized();
        if(PreviousWindowMode != EWindowMode::Windowed)
          Window->SetWindowMode(EWindowMode::Windowed);
        if(bPreviousWindowMaximized)
          Window->Restore();
        Window->Resize(FVector2D(WindowWidth, WindowHeight));
        const FVector2D AppliedSize = Window->GetClientSizeInScreen();
        UE_LOG(LogRenderDocMetalCapture, Display,
               TEXT("Controlled window requested %dx%d, applied %.0fx%.0f; previous %.0fx%.0f mode=%d maximized=%d"),
               WindowWidth, WindowHeight, AppliedSize.X, AppliedSize.Y,
               PreviousWindowSize.X, PreviousWindowSize.Y, int32(PreviousWindowMode), int32(bPreviousWindowMaximized));
      }
    Scene->SetFixedViewportSize(Width, Height);
    // Optional fixture camera; never used to decide API or replay support.
    const FString Camera = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_METAL_CAPTURE_CAMERA"));
    if(!Camera.IsEmpty())
    {
      TArray<FString> Parts;
      Camera.ParseIntoArray(Parts, TEXT(","));
      double Values[6] = {};
      bool Valid = Parts.Num() == 6;
      for(int32 I = 0; Valid && I < 6; ++I)
        Valid = LexTryParseString(Values[I], *Parts[I]) && FMath::IsFinite(Values[I]);
      bool Applied = false;
      if(Valid && GEditor)
        for(FEditorViewportClient *Client : GEditor->GetAllViewportClients())
          if(Client && Client->Viewport == Viewport)
          {
            Client->SetViewLocation(FVector(Values[0], Values[1], Values[2]));
            Client->SetViewRotation(FRotator(Values[3], Values[4], Values[5]));
            Client->Invalidate();
            Applied = true;
          }
      if(!Applied)
      {
        Notify(LOCTEXT("InvalidCaptureCamera", "Controlled camera requires six finite values and an editor viewport."), true);
        RestoreCaptureSize();
        return false;
      }
      UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Controlled editor camera applied: %s"), *Camera);
    }
    // Complete the resize before StartFrameCapture so frame resource births remain
    // ordinary background resources, and report the resulting size rather than flags.
    FlushRenderingCommands();
    UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Controlled viewport size applied: %dx%d (previous %dx%d)"),
           Scene->GetSizeXY().X, Scene->GetSizeXY().Y, PreviousViewportSize.X, PreviousViewportSize.Y);
    return true;
  }


  void Notify(const FText &Message, bool bError) const
  {
    UE_LOG(LogRenderDocMetalCapture, Log, TEXT("%s"), *Message.ToString());
    FNotificationInfo Info(Message);
    Info.ExpireDuration = 8.0f;
    if(TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
      Item->SetCompletionState(bError ? SNotificationItem::CS_Fail : SNotificationItem::CS_Success);
  }

  bool ResolveAPI()
  {
    if(API)
      return true;
    void *Symbol = dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    if(!Symbol)
    {
      UE_LOG(LogRenderDocMetalCapture, Warning,
             TEXT("RENDERDOC_GetAPI is absent; start the editor with DYLD_INSERT_LIBRARIES"));
      return false;
    }

    Dl_info LibraryInfo = {};
    if(dladdr(Symbol, &LibraryInfo) == 0 || !LibraryInfo.dli_fname)
      return false;
    const FString ActualLibrary = UTF8_TO_TCHAR(LibraryInfo.dli_fname);
    const FString ExpectedLibrary = FPlatformMisc::GetEnvironmentVariable(TEXT("RENDERDOC_METAL_LIBRARY"));
    if(!ExpectedLibrary.IsEmpty() && !ActualLibrary.Equals(ExpectedLibrary, ESearchCase::CaseSensitive))
    {
      UE_LOG(LogRenderDocMetalCapture, Error, TEXT("Wrong RenderDoc library: %s (expected %s)"),
             *ActualLibrary, *ExpectedLibrary);
      return false;
    }

    void *RawAPI = nullptr;
    const auto GetAPI = reinterpret_cast<pRENDERDOC_GetAPI>(Symbol);
    if(GetAPI(eRENDERDOC_API_Version_1_7_0, &RawAPI) != 1 || RawAPI == nullptr)
      return false;
    API = static_cast<RENDERDOC_API_1_0_0 *>(RawAPI);
    if(!API->StartFrameCapture || !API->EndFrameCapture || !API->GetNumCaptures || !API->GetCapture ||
       !API->SetCaptureFilePathTemplate)
    {
      API = nullptr;
      return false;
    }
    UE_LOG(LogRenderDocMetalCapture, Display, TEXT("RenderDoc API resolved from %s"), *ActualLibrary);
    return true;
  }

  void CaptureFrame()
  {
    if(!ResolveAPI())
    {
      Notify(LOCTEXT("NoAPI", "RenderDoc Metal is not loaded; use the launch script."), true);
      return;
    }
    if(PollHandle.IsValid() || EndCaptureHandle.IsValid() || CaptureWarmupHandle.IsValid())
    {
      Notify(LOCTEXT("Pending", "A RenderDoc Metal capture is already pending."), true);
      return;
    }

    FViewport *Viewport = nullptr;
    if(GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport &&
       GEngine->GameViewport->Viewport->HasFocus())
      Viewport = GEngine->GameViewport->Viewport;
    if(!Viewport && GEditor)
      Viewport = GEditor->GetActiveViewport();
    if(!Viewport || Viewport->GetSizeXY().X <= 0 || Viewport->GetSizeXY().Y <= 0)
    {
      Notify(LOCTEXT("NoViewport", "No active scene viewport is ready for capture."), true);
      return;
    }

    if(!SizedViewport && !ConfigureCaptureSize(Viewport))
      return;
    if(SizedViewport && !bCaptureSizeWarm)
    {
      // UE retires descriptors through DeferredDelete. A resize immediately followed
      // by capture can freeze obsolete slots just before their next-frame frees.
      // Draw normally first and allow editor ticks to retire the old allocations.
      Viewport->Draw(true);
      CaptureWarmupHandle = FTSTicker::GetCoreTicker().AddTicker(
          FTickerDelegate::CreateLambda([this](float) {
            CaptureWarmupHandle.Reset();
            bCaptureSizeWarm = true;
            CaptureFrame();
            return false;
          }), 2.0f);
      UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Controlled size warmup requested; capture starts after ordinary editor ticks"));
      return;
    }

    const FString CaptureDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RenderDocMetalCaptures"));
    if(!IFileManager::Get().MakeDirectory(*CaptureDir, true))
    {
      RestoreCaptureSize();
      Notify(FText::Format(LOCTEXT("CreateDirFailed", "Cannot create capture directory: {0}"),
                           FText::FromString(CaptureDir)), true);
      return;
    }

    const FString Prefix = FPaths::Combine(CaptureDir, TEXT("UE58"));
    API->SetCaptureFilePathTemplate(TCHAR_TO_UTF8(*Prefix));
    CapturesBefore = API->GetNumCaptures();
    CaptureDeadline = FPlatformTime::Seconds() + 60.0;
    bOldEmitDrawEvents = GetEmitDrawEvents();
    SetEmitDrawEvents(true);
    CaptureEndState = MakeShared<FRenderDocMetalCaptureEndState, ESPMode::ThreadSafe>();
    // Follow UE's official RenderDoc plugin: start on the render thread, render the selected
    // viewport once, then wait for its commands before closing this one controlled capture.
    ENQUEUE_RENDER_COMMAND(StartRenderDocMetalCapture)(
        [CaptureAPI = API, State = CaptureEndState](FRHICommandListImmediate &RHICmdList) {
          AnnotateUE58MetalDescriptorLayouts(CaptureAPI, RHICmdList);
          State->Device = GDynamicRHI ? GDynamicRHI->RHIGetNativeDevice() : nullptr;
          CaptureAPI->StartFrameCapture(nullptr, nullptr);
          State->PresentDeadline = FPlatformTime::Seconds() + 5.0;
          UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Controlled Metal capture started on render thread"));
        });
    UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Drawing target viewport %p (%dx%d)"),
           Viewport, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);
    Viewport->Draw(true);
    const FString NativeViewportOutput =
        FPlatformMisc::GetEnvironmentVariable(TEXT("UE_METAL_NATIVE_VIEWPORT_OUTPUT"));
    const bool NativeReadAfterCapture =
        FPlatformMisc::GetEnvironmentVariable(TEXT("UE_METAL_NATIVE_VIEWPORT_AFTER_CAPTURE")) == TEXT("1");
    if(!NativeViewportOutput.IsEmpty() && NativeReadAfterCapture)
    {
      // Retain the exact viewport backing. Read it in the ordered end command,
      // after EndFrameCapture, before another draw can overwrite this backing.
      CaptureEndState->NativeViewportTexture = Viewport->GetRenderTargetTexture();
      CaptureEndState->NativeViewportSize = Viewport->GetSizeXY();
      CaptureEndState->NativeViewportAfterCapture = NativeViewportOutput;
    }
    if(!NativeViewportOutput.IsEmpty() && !NativeReadAfterCapture)
    {
      // Optional diagnostic oracle from this exact full viewport draw, before
      // Slate presentation. Frame-end readback can affect scheduling; do not
      // count an output match with this observer as a production mechanism fix.
      FReadSurfaceDataFlags Flags(RCM_UNorm);
      Flags.SetLinearToGamma(false);
      TArray<FColor> Pixels;
      const FIntPoint Size = Viewport->GetSizeXY();
      if(FPaths::IsRelative(NativeViewportOutput) || Size.X > 2048 || Size.Y > 2048 ||
         IFileManager::Get().FileExists(*NativeViewportOutput) ||
         !Viewport->ReadPixels(Pixels, Flags) || Pixels.Num() != Size.X * Size.Y ||
         !FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8 *>(Pixels.GetData()), Pixels.Num() * sizeof(FColor)), *NativeViewportOutput))
      {
        UE_LOG(LogRenderDocMetalCapture, Error, TEXT("Same-frame Native viewport observation failed"));
        ENQUEUE_RENDER_COMMAND(DiscardNativeViewportObservation)(
            [CaptureAPI = API, PreviousEmitDrawEvents = bOldEmitDrawEvents](FRHICommandListImmediate &) {
              CaptureAPI->DiscardFrameCapture(nullptr, nullptr);
              SetEmitDrawEvents(PreviousEmitDrawEvents);
            });
        FlushRenderingCommands();
        RestoreCaptureSize();
        return;
      }
      UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Same-frame Native viewport saved: %dx%d BGRA, linearToGamma=0, %s"), Size.X, Size.Y, *NativeViewportOutput);
    }
    // A scene viewport can render offscreen; Slate presents its texture later in the editor
    // tick. Keep the controlled capture open through that present so Metal has a backbuffer.
    EndCaptureHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FRenderDocMetalCaptureModule::EndCapture), 0.0f);
    UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Requested one viewport draw; prefix: %s"), *Prefix);
    Notify(LOCTEXT("Triggered", "RenderDoc Metal: capturing the active viewport."), false);
    PollHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FRenderDocMetalCaptureModule::PollCapture), 0.25f);
  }

  bool EndCapture(float)
  {
    if(CaptureEndState->Phase.GetValue() == 2)
    {
      EndCaptureHandle.Reset();
      return false;
    }
    if(CaptureEndState->Phase.GetValue() == 1)
      return true;
    CaptureEndState->Phase.Set(1);
    ENQUEUE_RENDER_COMMAND(EndRenderDocMetalCapture)(
        [CaptureAPI = API, PreviousEmitDrawEvents = bOldEmitDrawEvents, State = CaptureEndState](FRHICommandListImmediate &RHICmdList) {
          const bool Presented = State->Device && CaptureAPI->SetObjectAnnotation &&
              CaptureAPI->SetObjectAnnotation(State->Device, State->Device,
                  "metal.capturePresented", eRENDERDOC_Empty, 0, nullptr) == 0;
          if(!Presented && FPlatformTime::Seconds() < State->PresentDeadline)
          {
            State->Phase.Set(0);
            return;
          }
          RHICmdList.SubmitAndBlockUntilGPUIdle();
          const uint32 Result = Presented ? CaptureAPI->EndFrameCapture(nullptr, nullptr) :
                                            CaptureAPI->DiscardFrameCapture(nullptr, nullptr);
          SetEmitDrawEvents(PreviousEmitDrawEvents);
          if(Presented && Result && !State->NativeViewportAfterCapture.IsEmpty())
          {
            const FIntPoint Size = State->NativeViewportSize;
            TArray<FColor> Pixels;
            FReadSurfaceDataFlags Flags(RCM_UNorm);
            Flags.SetLinearToGamma(false);
            const bool Valid = State->NativeViewportTexture.IsValid() && Size.X > 0 && Size.Y > 0 &&
                Size.X <= 2048 && Size.Y <= 2048 && !FPaths::IsRelative(State->NativeViewportAfterCapture) &&
                !IFileManager::Get().FileExists(*State->NativeViewportAfterCapture);
            if(Valid)
              RHICmdList.ReadSurfaceData(State->NativeViewportTexture, FIntRect(0, 0, Size.X, Size.Y), Pixels, Flags);
            const bool Saved = Valid && Pixels.Num() == Size.X * Size.Y &&
                FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8 *>(Pixels.GetData()), Pixels.Num() * sizeof(FColor)), *State->NativeViewportAfterCapture);
            UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Same-frame Native viewport AFTER capture: saved=%d %dx%d BGRA, linearToGamma=0, %s"), Saved, Size.X, Size.Y, *State->NativeViewportAfterCapture);
            State->NativeViewportTexture.SafeRelease();
          }
          State->Phase.Set(2);
          if(Presented)
          {
            UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Controlled Metal capture end result: %u"), Result);
          }
          else
          {
            UE_LOG(LogRenderDocMetalCapture, Warning,
                TEXT("No Metal presentation within 5 seconds after capture start; controlled capture discarded"));
          }
        });
    return true;
  }

  bool PollCapture(float)
  {
    const uint32 Count = API->GetNumCaptures();
    if(Count > CapturesBefore)
    {
      RestoreCaptureSize();
      uint32 PathLength = 0;
      API->GetCapture(Count - 1, nullptr, &PathLength, nullptr);
      if(PathLength > 0 && PathLength < 32768)
      {
        TArray<char> Path;
        Path.SetNumZeroed(PathLength);
        if(API->GetCapture(Count - 1, Path.GetData(), &PathLength, nullptr))
        {
          const FString CapturePath = UTF8_TO_TCHAR(Path.GetData());
          Notify(FText::Format(LOCTEXT("Captured", "Capture saved: {0}"),
                               FText::FromString(CapturePath)), false);
          UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Capture saved: %s"), *CapturePath);
        }
      }
      PollHandle.Reset();
      return false;
    }
    if(FPlatformTime::Seconds() >= CaptureDeadline)
    {
      RestoreCaptureSize();
      Notify(LOCTEXT("TimedOut", "No capture after 60 seconds; inspect RenderDoc and UE logs."), true);
      PollHandle.Reset();
      return false;
    }
    return true;
  }

  void RegisterMenus()
  {
    FToolMenuOwnerScoped Owner(this);
    const FUIAction Action(FExecuteAction::CreateRaw(this, &FRenderDocMetalCaptureModule::CaptureFrame));

    if(UToolMenu *Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.ViewportToolbar"))
    {
      FToolMenuSection &Section = Toolbar->FindOrAddSection("Right");
      Section.AddEntry(FToolMenuEntry::InitToolBarButton(
          "RenderDocMetalCaptureFrame", Action, LOCTEXT("Button", "Capture Metal Frame"),
          LOCTEXT("ButtonTip", "Draw and capture the active scene viewport with RenderDoc Metal"),
          FSlateIcon(FAppStyle::GetAppStyleSetName(), "DeveloperTools.MenuIcon")));
    }
    if(UToolMenu *Tools = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
    {
      FToolMenuSection &Section = Tools->FindOrAddSection("RenderDocMetal");
      Section.AddMenuEntry("RenderDocMetalCaptureFrame", LOCTEXT("Menu", "Capture Metal Frame"),
                           LOCTEXT("MenuTip", "Draw and capture the active scene viewport"),
                           FSlateIcon(FAppStyle::GetAppStyleSetName(), "DeveloperTools.MenuIcon"),
                           Action);
    }
  }
};

IMPLEMENT_MODULE(FRenderDocMetalCaptureModule, RenderDocMetalCapture)

#undef LOCTEXT_NAMESPACE
