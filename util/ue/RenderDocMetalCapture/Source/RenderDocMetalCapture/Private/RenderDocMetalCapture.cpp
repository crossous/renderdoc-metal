#include "CoreMinimal.h"
#include "CoreGlobals.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UnrealClient.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "renderdoc_app.h"

#include <dlfcn.h>

DEFINE_LOG_CATEGORY_STATIC(LogRenderDocMetalCapture, Log, All);

#define LOCTEXT_NAMESPACE "RenderDocMetalCapture"

class FRenderDocMetalCaptureModule final : public IModuleInterface
{
public:
  virtual void StartupModule() override
  {
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
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
  }

private:
  RENDERDOC_API_1_0_0 *API = nullptr;
  FTSTicker::FDelegateHandle PollHandle;
  FTSTicker::FDelegateHandle AutoCaptureHandle;
  FTSTicker::FDelegateHandle EndCaptureHandle;
  bool bOldEmitDrawEvents = false;
  uint32 CapturesBefore = 0;
  double CaptureDeadline = 0.0;

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
    if(GetAPI(eRENDERDOC_API_Version_1_0_0, &RawAPI) != 1 || RawAPI == nullptr)
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
    if(PollHandle.IsValid() || EndCaptureHandle.IsValid())
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

    const FString CaptureDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RenderDocMetalCaptures"));
    if(!IFileManager::Get().MakeDirectory(*CaptureDir, true))
    {
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
    // Follow UE's official RenderDoc plugin: start on the render thread, render the selected
    // viewport once, then wait for its commands before closing this one controlled capture.
    ENQUEUE_RENDER_COMMAND(StartRenderDocMetalCapture)(
        [CaptureAPI = API](FRHICommandListImmediate &RHICmdList) {
          CaptureAPI->StartFrameCapture(nullptr, nullptr);
          UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Controlled Metal capture started on render thread"));
        });
    UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Drawing target viewport %p (%dx%d)"),
           Viewport, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);
    Viewport->Draw(true);
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
    ENQUEUE_RENDER_COMMAND(EndRenderDocMetalCapture)(
        [CaptureAPI = API, PreviousEmitDrawEvents = bOldEmitDrawEvents](FRHICommandListImmediate &RHICmdList) {
          RHICmdList.SubmitAndBlockUntilGPUIdle();
          const uint32 Result = CaptureAPI->EndFrameCapture(nullptr, nullptr);
          SetEmitDrawEvents(PreviousEmitDrawEvents);
          UE_LOG(LogRenderDocMetalCapture, Display, TEXT("Controlled Metal capture end result: %u"), Result);
        });
    EndCaptureHandle.Reset();
    return false;
  }

  bool PollCapture(float)
  {
    const uint32 Count = API->GetNumCaptures();
    if(Count > CapturesBefore)
    {
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
