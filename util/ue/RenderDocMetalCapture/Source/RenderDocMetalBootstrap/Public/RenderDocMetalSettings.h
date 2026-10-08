// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "RenderDocMetalSettings.generated.h"

/** Per-user startup settings; changing paths requires an editor restart. */
UCLASS(Config=EditorPerProjectUserSettings, meta=(DisplayName="RenderDoc Metal"))
class RENDERDOCMETALBOOTSTRAP_API URenderDocMetalSettings : public UDeveloperSettings
{
  GENERATED_BODY()
public:
  virtual FName GetContainerName() const override { return TEXT("Project"); }
  virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
  virtual FName GetSectionName() const override { return TEXT("RenderDocMetal"); }

  /** Attach before Metal device creation, by re-executing this editor once with dyld injection. */
  UPROPERTY(Config, EditAnywhere, Category="Startup", meta=(DisplayName="Automatically attach on editor startup"))
  bool bAutoAttach = false;

  /** Absolute path to the downloaded RenderDocMetal.app bundle. */
  UPROPERTY(Config, EditAnywhere, Category="Startup", meta=(DisplayName="RenderDoc application (.app)"))
  FDirectoryPath ApplicationPath;

  /** Optional absolute dylib path; when set, this takes precedence over ApplicationPath. */
  UPROPERTY(Config, EditAnywhere, Category="Startup", meta=(DisplayName="RenderDoc library (.dylib, optional)", FilePathFilter="dylib"))
  FFilePath LibraryPath;

  /** Optional exact-version instrumented MetalRHI module. Not needed for ordinary direct Metal captures. */
  UPROPERTY(Config, EditAnywhere, AdvancedDisplay, Category="Startup", meta=(DisplayName="Matching MetalRHI provider (optional)", FilePathFilter="dylib"))
  FFilePath MetalRHIProvider;
};
