// SPDX-License-Identifier: MIT
#include "CoreMinimal.h"
#include "RenderDocMetalSettings.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>
#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <limits.h>
#include <cstdlib>

DEFINE_LOG_CATEGORY_STATIC(LogRenderDocMetalBootstrap, Log, All);

class FRenderDocMetalBootstrap final : public IModuleInterface
{
public:
  virtual void StartupModule() override
  {
    // Re-exec before RHI initialization; never restart a running editor with unsaved work.
    // Commandlets stay untouched unless explicitly used as a CPU startup-injection probe.
    const bool Probe = FParse::Param(FCommandLine::Get(), TEXT("RenderDocMetalStartupProbe"));
    if((IsRunningCommandlet() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) && !Probe)
      return;
    // Per-use GPU indirect arguments are execution inputs, not optional UI analysis.
    // These capture options must be active before Metal encoders are created.
    const auto EnableIndirectCapture = []() {
      for(const char *Key : {"RENDERDOC_METAL_CAPTURE_INDIRECT_ARGUMENTS",
                             "RENDERDOC_METAL_CAPTURE_RENDER_INDIRECT_ARGUMENTS"})
      {
        const char *ExistingValue = getenv(Key);
        if((!ExistingValue || !*ExistingValue) && setenv(Key, "1", 1) != 0)
          return false;
      }
      return true;
    };
    if(dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI"))
    {
      if(!EnableIndirectCapture())
      {
        UE_LOG(LogRenderDocMetalBootstrap, Error, TEXT("Could not enable required indirect argument capture inputs"));
        return;
      }
      UE_LOG(LogRenderDocMetalBootstrap, Display, TEXT("RenderDoc Metal attached before RHI initialization"));
      return;
    }
    // PostConfigInit precedes UObject initialization. Read the same per-project user
    // config as the later Project Settings page, without constructing its CDO here.
    const TCHAR *Section = TEXT("/Script/RenderDocMetalBootstrap.RenderDocMetalSettings");
    bool AutoAttach = false;
    if(!GConfig || !GConfig->GetBool(Section, TEXT("bAutoAttach"), AutoAttach, GEditorPerProjectIni) || !AutoAttach)
      return;
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("RENDERDOC_METAL_AUTO_ATTACH_ATTEMPT")) == TEXT("1"))
    {
      UE_LOG(LogRenderDocMetalBootstrap, Error, TEXT("Automatic attachment failed after one restart; dyld did not load RenderDoc. No further restart attempted."));
      return;
    }
    const auto ReadPath = [&](const TCHAR *Key, const TCHAR *Member) {
      FString Serialized, Path;
      if(GConfig->GetString(Section, Key, Serialized, GEditorPerProjectIni))
        FParse::Value(*Serialized, Member, Path);
      return Path;
    };
    FString Library = ReadPath(TEXT("LibraryPath"), TEXT("FilePath="));
    if(Library.IsEmpty())
    {
      FString App = ReadPath(TEXT("ApplicationPath"), TEXT("Path="));
      FPaths::NormalizeDirectoryName(App);
      if(App.IsEmpty() || FPaths::IsRelative(App) || !App.EndsWith(TEXT(".app")))
      {
        UE_LOG(LogRenderDocMetalBootstrap, Error, TEXT("Set an absolute .app or .dylib path in Project Settings > Plugins > RenderDoc Metal, then restart."));
        return;
      }
      Library = FPaths::Combine(App, TEXT("Contents/lib/librenderdoc.dylib"));
    }
    char PhysicalLibrary[PATH_MAX] = {};
    if(FPaths::IsRelative(Library) || !Library.EndsWith(TEXT(".dylib")) ||
       !IFileManager::Get().FileExists(*Library) || !realpath(TCHAR_TO_UTF8(*Library), PhysicalLibrary))
    {
      UE_LOG(LogRenderDocMetalBootstrap, Error, TEXT("RenderDoc library must be an existing absolute dylib path: %s"), *Library);
      return;
    }
    Library = UTF8_TO_TCHAR(PhysicalLibrary);
    if(Library.Contains(TEXT(":")))
    {
      UE_LOG(LogRenderDocMetalBootstrap, Error, TEXT("The library path cannot contain a colon (dyld library separator)."));
      return;
    }
    // Preserve any existing injections, and run the RenderDoc constructor first.
    FString Inject = Library;
    const FString Provider = ReadPath(TEXT("MetalRHIProvider"), TEXT("FilePath="));
    if(!Provider.IsEmpty())
    {
      if(FPaths::IsRelative(Provider) || !IFileManager::Get().FileExists(*Provider) ||
         !FPaths::GetCleanFilename(Provider).Equals(TEXT("libUnrealEditor-MetalRHI.dylib")) || Provider.Contains(TEXT(":")))
      {
        UE_LOG(LogRenderDocMetalBootstrap, Error, TEXT("MetalRHI provider must be an existing absolute exact-version libUnrealEditor-MetalRHI.dylib path."));
        return;
      }
      Inject += TEXT(":") + Provider;
    }
    const FString Existing = FPlatformMisc::GetEnvironmentVariable(TEXT("DYLD_INSERT_LIBRARIES"));
    if(!Existing.IsEmpty())
      Inject += TEXT(":") + Existing;
    char **Args = *_NSGetArgv();
    char Executable[PATH_MAX] = {};
    uint32_t ExecutableSize = sizeof(Executable);
    if(!Args || !Args[0] || _NSGetExecutablePath(Executable, &ExecutableSize) != 0)
    {
      UE_LOG(LogRenderDocMetalBootstrap, Error, TEXT("Cannot resolve editor executable for automatic attachment; use the command-line launcher."));
      return;
    }
    UE_LOG(LogRenderDocMetalBootstrap, Display, TEXT("Restarting this editor once with RenderDoc Metal: %s"), *Library);
    const FString OldLibrary = FPlatformMisc::GetEnvironmentVariable(TEXT("RENDERDOC_METAL_LIBRARY"));
    const FString OldComputeIndirect = FPlatformMisc::GetEnvironmentVariable(TEXT("RENDERDOC_METAL_CAPTURE_INDIRECT_ARGUMENTS"));
    const FString OldRenderIndirect = FPlatformMisc::GetEnvironmentVariable(TEXT("RENDERDOC_METAL_CAPTURE_RENDER_INDIRECT_ARGUMENTS"));
    const auto RestoreEnvironment = [&]() {
      unsetenv("RENDERDOC_METAL_AUTO_ATTACH_ATTEMPT");
      if(Existing.IsEmpty()) unsetenv("DYLD_INSERT_LIBRARIES");
      else setenv("DYLD_INSERT_LIBRARIES", TCHAR_TO_UTF8(*Existing), 1);
      if(OldLibrary.IsEmpty()) unsetenv("RENDERDOC_METAL_LIBRARY");
      else setenv("RENDERDOC_METAL_LIBRARY", TCHAR_TO_UTF8(*OldLibrary), 1);
      if(OldComputeIndirect.IsEmpty()) unsetenv("RENDERDOC_METAL_CAPTURE_INDIRECT_ARGUMENTS");
      else setenv("RENDERDOC_METAL_CAPTURE_INDIRECT_ARGUMENTS", TCHAR_TO_UTF8(*OldComputeIndirect), 1);
      if(OldRenderIndirect.IsEmpty()) unsetenv("RENDERDOC_METAL_CAPTURE_RENDER_INDIRECT_ARGUMENTS");
      else setenv("RENDERDOC_METAL_CAPTURE_RENDER_INDIRECT_ARGUMENTS", TCHAR_TO_UTF8(*OldRenderIndirect), 1);
    };
    // execv preserves argv exactly (including paths with spaces) and replaces this process.
    // No shell, detached duplicate editor, engine modification, or library unload is used.
    if(!EnableIndirectCapture() || setenv("DYLD_INSERT_LIBRARIES", TCHAR_TO_UTF8(*Inject), 1) ||
       setenv("RENDERDOC_METAL_LIBRARY", TCHAR_TO_UTF8(*Library), 1) ||
       setenv("RENDERDOC_METAL_AUTO_ATTACH_ATTEMPT", "1", 1))
    {
      const int Error = errno;
      RestoreEnvironment();
      UE_LOG(LogRenderDocMetalBootstrap, Error, TEXT("Could not prepare automatic attachment environment: %s"), UTF8_TO_TCHAR(strerror(Error)));
      return;
    }
    execv(Executable, Args);
    const int Error = errno;
    RestoreEnvironment();
    UE_LOG(LogRenderDocMetalBootstrap, Error, TEXT("Automatic attachment restart failed: %s"), UTF8_TO_TCHAR(strerror(Error)));
  }
};

IMPLEMENT_MODULE(FRenderDocMetalBootstrap, RenderDocMetalBootstrap)
