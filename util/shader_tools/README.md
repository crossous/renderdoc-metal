# Metal shader tools included in this fork

The macOS app automatically exposes its bundled processors in the standard RenderDoc
Shader Viewer and Pipeline State Edit menu. No Preferences setup or absolute checkout
paths are required. Moving the app does not change this. Existing user tools remain available.

| Entry | Purpose | Runtime dependency |
| --- | --- | --- |
| Apple AIR (bundled) | MetalLib → editable LLVM AIR text | Xcode metal-objdump |
| Apple Metal compiler (AIR) | Edited AIR → MetalLib | Xcode metal and metallib |
| Apple Metal compiler (MSL) | Captured/edited MSL → MetalLib | Xcode metal and metallib |
| Metal reconstructed MSL/HLSL/GLSL (preview) | AIR → Vulkan SPIR-V → high-level source | Bundled native metal2vulkan, spirv-val, SPIRV-Cross |
| Metal reconstructed MSL (editable subset) | Verified simple fragment-source editing | Same bundled tools, then Apple compiler |

The editor's **Compiler** dropdown also retains **Builtin**. With captured debug source,
Edit opens that source first. Without it, Edit offers AIR, the guarded reconstructed-MSL
entry, and the existing MSL replacement template. Preview tools appear in View, not Edit:
HLSL/GLSL have no native Metal compiler here, and translated MSL can have a different ABI.
Apple AIR is intermediate code, not Apple GPU machine ISA.

## High-level conversion limits

The pinned alpha converter is useful for inspecting supported modules, but is not a general
AIR-to-MSL replacement compiler. SPIRV-Cross consumes SPIR-V, not AIR. Translation can change
bindings, argument-buffer representation, function constants, vertex/fragment interfaces,
and compute dispatch payloads. Preview output carries this warning.

Reconstructed-MSL editing currently requires a resource-free fragment shader without
function constants, input varyings or imageblock attachments. Unsupported shaders fail with
an explanation and no compiled replacement. A function-constant fixture demonstrates why:
the translator produces default-valued high-level source, so a successful SPIR-V validation
would not preserve its captured specialization. No silent binding or specialization repair
is attempted.

The current UE capture's lighting EID 3928 and Nanite EID 3612 **do not yet decompile** with this
pinned converter: its typed emitter rejects LLVM `fptoui` and `fptosi`, respectively. AIR editing
and Apple AIR compilation work for both, with native replacement output checked byte-for-byte.
These are converter limitations, not evidence that native UE replay is failing. See
`docs/metal-replay/BATCH466_BUNDLED_SHADER_TOOLS.md` for separate validation results.

## Building and packaging

Build dependencies: Xcode with the Metal toolchain, CMake, a C++ compiler, and Rust >= 1.87.
These are developer dependencies. End users do not need Cargo, Rust, Homebrew, or a separate
LLVM installation. `/usr/bin/python3` and Apple's Metal tools are supplied by Xcode/CLT.
Build caches can live on an external disk.

```sh
bash util/buildscripts/scripts/build_metal_shader_tools_macos.sh /path/to/tool-cache
RENDERDOC_METAL_SHADER_TOOLS_ROOT=/path/to/tool-cache \
  bash util/buildscripts/scripts/build_metal_dev_macos.sh
```

Subsequent development builds reuse `METAL_SHADER_TOOLS_ROOT` recorded in CMakeCache.txt.
For CMake release builds, configure `-DMETAL_SHADER_TOOLS_ROOT=/path/to/tool-cache`.
A Metal release configure fails if the tool distribution is absent. `build-qrenderdoc`
packages the tools even when the app does not relink; the package step verifies required
executables, checksums, architecture, licenses/source, and dynamic library dependencies.
No non-system Homebrew dylib is allowed. All three native tools are built from pinned official
sources; source archive checksums and the Rust dependency lockfile are in this fork.

App location: `Contents/Resources/shader-tools/`, containing `bin/`, `licenses/`, `source/`,
Python adapters and `manifest.json`. Metal2vulkan is a separate LGPL executable. Corresponding
unmodified source, Cargo.lock, full licenses and rebuild/replacement instructions accompany it;
see [THIRD_PARTY.md](THIRD_PARTY.md). No user shaders or captures enter these archives.

Tools run in private scratch directories, with direct process invocation, a 24-second
total deadline, at most 20 seconds per translation step and a sampled process-group RSS
guard at 500 MiB. This completes before the common UI's 30-second external-tool wait.
Failed conversions leave no source/binary output. Scratch and child
processes are cleaned up on failure as well as success.

## Directed verification

Close qrenderdoc/UE before GPU replacement verification:

```sh
bash util/buildscripts/scripts/test_metal_shader_processors_macos.sh
```

The CPU tool test checks AIR, MSL/HLSL/GLSL output, MSL/AIR compilation, function-constant rejection,
stale-output cleanup, deadline enforcement, package hashes, and relocation to a path containing
spaces with Homebrew absent from PATH. The native replay probe checks actual application and
exact output/restoration on two cycles. Run the UE arguments shown in B466 when validating the
same capture; all GPU work remains serial.
