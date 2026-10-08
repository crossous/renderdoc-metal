# RenderDoc Metal Capture for Unreal Editor

[English project guide](../../../README.md) · [简体中文项目说明](../../../README-zh.md)

A project plugin with **Project Settings → Plugins → RenderDoc Metal**, automatic startup attachment, and **Capture Metal Frame** in the viewport toolbar and Tools menu. Prefer the `.app` setting for the downloaded distribution; the optional `.dylib` setting takes precedence.

## Install and configure

1. Download the plugin ZIP from the [macOS release](https://github.com/crossous/renderdoc-metal/releases/tag/v1.46-metal.1). Extract `RenderDocMetalCapture` into `<Project>/Plugins/`. The source ZIP is also self-contained; it includes the RenderDoc application API header.
2. Enable **RenderDoc Metal Capture** in UE's Plugins dialog. The prebuilt package targets the installed **UE 5.8.3, CL 58210709, BuildId 55116800, macOS arm64** editor. Other builds must compile the source plugin against their own engine. The current descriptor-layout probe uses UE 5.8.3 private MetalRHI headers; adapting that probe is required if those layouts differ.
3. Open **Project Settings → Plugins → RenderDoc Metal**. Set **RenderDoc application (.app)** to `/Applications/RenderDocMetal.app`. Alternatively set **RenderDoc library (.dylib)** to `/Applications/RenderDocMetal.app/Contents/lib/librenderdoc.dylib`.
4. Enable **Automatically attach on editor startup** and restart the editor. Absolute existing paths are required. Startup errors are written under `LogRenderDocMetalBootstrap`; commandlets and Null RHI are skipped by default.
5. Click **Capture Metal Frame** with a ready scene viewport. The saved path appears in a notification and the Output Log, under `<Project>/Saved/RenderDocMetalCaptures`. Open the RDC in RenderDoc Metal. The plugin captures a viewport frame and does not automatically launch a replay alongside UE.

Settings are per user/project (`Saved/Config/MacEditor/EditorPerProjectUserSettings.ini` on the tested engine); local application paths need not be committed with the project. Equivalent configuration:

```ini
[/Script/RenderDocMetalBootstrap.RenderDocMetalSettings]
bAutoAttach=True
ApplicationPath=(Path="/Applications/RenderDocMetal.app")
LibraryPath=(FilePath="")
MetalRHIProvider=(FilePath="")
```

To disable attachment without starting UE, set `bAutoAttach=False` in this file. Use a compatible arm64 backend with its required dependencies installed; the app distribution provides those dependencies.

## How automatic attachment works

The built-in UE RenderDoc plugin's `LoadAndCheckRenderDocLibrary` loads a library and obtains `RENDERDOC_GetAPI` on its supported Windows/Linux platforms. This Metal backend instead implements `MTLCreateSystemDefaultDevice` through dyld `__DATA,__interpose` entries (`metal_hook_bridge.mm`, `apple_hook.cpp`). Native experiments found that late `dlopen` makes the API symbol available but leaves the device as `AGXG14SDevice`; the same library preloaded at process startup returns `ObjCBridgeMTLDevice`. Loading the Metal consumer after RenderDoc did not fix late loading.

The bootstrap module runs at **PostConfigInit**, before RHI initialization. It reads the configuration cache directly: UObject default objects are not ready at this phase. When enabled, it validates the paths, preserves the original argument vector and any existing `DYLD_INSERT_LIBRARIES`, then uses `execv` to replace **this same editor process once** with startup injection configured. There is no second editor, shell launcher or engine-file replacement. It also enables the backend’s existing per-use compute/render indirect-argument snapshots before encoders are created, preserving required GPU execution inputs. It does not alter draw/dispatch parameters or relax replay checks. The startup guard prevents another restart if injection is unsuccessful. This early startup step cannot discard work from an already running editing session.

Expected log messages:

```text
LogRenderDocMetalBootstrap: Restarting this editor once with RenderDoc Metal: ...
LogRenderDocMetalBootstrap: RenderDoc Metal attached before RHI initialization
LogRenderDocMetalCapture: RenderDoc API resolved from .../librenderdoc.dylib
```

If the OS refuses injection or the library cannot load, inspect the startup log and the compatibility of the chosen library. For command-line attachment to a compatible editor, the equivalent startup environment is:

```sh
DYLD_INSERT_LIBRARIES="/Applications/RenderDocMetal.app/Contents/lib/librenderdoc.dylib" \
RENDERDOC_METAL_LIBRARY="/Applications/RenderDocMetal.app/Contents/lib/librenderdoc.dylib" \
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
"/absolute/path/YourProject.uproject"
```

## UE bindless / converted RT metadata

Loading RenderDoc is distinct from recovering GPU pointers. UE bindless and converted inline-ray-query workloads may need descriptor identities, layouts and publication timing from the exact engine's MetalRHI module. The **Matching MetalRHI provider** advanced setting preloads such an instrumented module after RenderDoc. It does not build, certify or substitute metadata automatically.

The repository's [provider preparation](../prepare_ue_metal_provider.py) and [isolated module build](../build_ue_metal_module_macos.py) tools target the tested UE 5.8.3 source/ABI. Both refuse to patch the installed engine. The builder also requires the matching cached MetalRHI compile/link response files. An example developer workflow from the repository root:

```sh
python3 util/ue/prepare_ue_metal_provider.py \
  --engine "/Users/Shared/Epic Games/UE_5.8/Engine" \
  --source "/Volumes/External/RenderDocMetal/MetalRHI-source"
python3 util/ue/build_ue_metal_module_macos.py \
  --engine "/Users/Shared/Epic Games/UE_5.8/Engine" \
  --source "/Volumes/External/RenderDocMetal/MetalRHI-source" \
  --output "/Volumes/External/RenderDocMetal/MetalRHI-build"
```

Select the resulting `libUnrealEditor-MetalRHI.dylib` in the advanced setting. A module from a different engine revision is not interchangeable. **No UE engine module is distributed in the plugin ZIP.** Missing addresses/initial state/dependencies remain real recovery failures; the plugin does not disable backend checks to accept a capture.

## Build the source plugin

Copy this directory into your project's `Plugins/RenderDocMetalCapture`, then build its editor target using the matching engine and Xcode. For the tested engine:

```sh
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" \
  UnrealEditor Mac Development \
  -Project="/absolute/path/YourProject.uproject" \
  -Architecture=arm64 -MaxParallelActions=2 -NoHotReload -NoRemote
```

A C++ project's own editor target may be named `<Project>Editor`; use that target when appropriate. Do not build or run GPU tests concurrently with active captures/replays.

## Validation scope

The current modules compile and link on UE 5.8.3 arm64. An isolated **Null RHI** commandlet test passed configuration-driven `.app` startup attachment, one restart, both module loads and final RenderDoc API resolution; it exited normally. Native device-factory experiments separately verified effective preload interposition. The [release record](../../../docs/metal-replay/RELEASE_2026-10-08.md) records path-priority and startup-guard controls, exact binary hashes and preserved failures.

The new automatic-attachment path has **not** yet been certified by a GPU capture from the project-settings UI or a full Release replay/output matrix. Existing capture-button and UE frame evidence is tied to the earlier recorded candidates. The supplied 1280×720 demo's Native image comparison still fails; installing this plugin does not establish arbitrary Lumen/Nanite/VSM correctness. Render-stage RT, RT shader stepping, AS internals and Pixel History remain outside the supported scope.
