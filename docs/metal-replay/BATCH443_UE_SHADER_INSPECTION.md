# B443: UE bindless texture inputs and captured AIR inspection

2026-10-03, local changes after B440–B442; no commit/push and no new UE capture.
Candidate `70773cef260d2fa597dfa25bb30fff4a55fbde600ff3f83c425771c207080037`.
Same replay capture SHA256 `8b1c0a81d6557c14b69591cfb774cc7c748752d05914df37e52114ef131fa701`.

## Reference behavior and actual gap

[D3D12 GetDescriptorAccess](../../renderdoc/driver/d3d12/d3d12_replay.cpp) merges static
accesses and valid dynamic bindless feedback. [Vulkan GetDescriptorAccess](../../renderdoc/driver/vulkan/vk_replay.cpp)
does the same. Texture Viewer's Inputs are built from these descriptors, not a list of every
allocation in a heap. Bindless does not inherently mean that Inputs should be empty.

Metal had no descriptor access path for UE's IR-converter resource heap. Its shader
`DisassembleShader` was also an unconditional "not implemented" stub. The capture already
contained the original compiled metallib, so this was not a reason to ask for recapture.

## Implemented

- Preserve binary library bytes for inspection across file/URL/data/bundle/default loading
  paths, associate shaders with their captured library, and expose the original bytes through
  reflection. Compiled bytes remain encoding Unknown, not fabricated MSL source.
- Add `Metal AIR (Apple toolchain)` disassembly via the installed Apple's
  `xcrun metal-objdump --metallib --disassemble`. It uses a unique temporary file, no shell,
  and no capture-supplied executable/path; cleanup and per-library caching are included.
  Captured source libraries continue to expose their actual MSL. Missing tools/source receive
  explicit diagnostics. AIR is intermediate code, not Apple GPU ISA or original HLSL.
- Evaluate a bounded subset of uniform address dependencies in AIR: pointer bitcasts,
  scalar GEPs, integer loads/arithmetic/extensions and texture handle loads feeding actual
  AIR sample/read instructions. Bindings come from AIR metadata, not fixed shader names.
  Preserve captured inline bytes plus validated pointer ResourceId/offset provenance in the
  encoder/event state; do not follow captured process addresses.
- Resolve current uniform values from replay buffers. Each texture result must have a live
  typed descriptor source and an actual native table handle matching that texture. Unknown
  expressions, missing provenance and ambiguous multi-function modules remain unresolved.
- Feed these accesses through the common DescriptorAccess/GetDescriptors path, with unique
  heap indices for texture following, logical `Heap[N] (uniform AIR)` labels, and matching
  descriptor/sampler result array lengths. No texture-viewer-only injected thumbnails.

This is **uniform-resolved static access**, including conditional shader paths, as opposed to
per-pixel/per-invocation GPU feedback. It does not implement arbitrary varying/instance-driven
indices, general AIR execution, compute bindless feedback, shader debugging or original HLSL
reconstruction. No claim of complete Metal/D3D12/Vulkan feature parity is made.

## Same UE EID3371 evidence

Fragment function4712 `Main_0000aee4_f632e2ef`, library4709, contains
`air.sample_texture_2d` operations. Six texture inputs resolve through heap buffer24:

| Heap index | Byte offset | Resource |
| --- | --- | --- |
| 1998 | 47952 | 49924 — GBufferA |
| 2009 | 48216 | 49938 — GBufferB |
| 2004 | 48096 | 49933 — GBufferC |
| 2013 | 48312 | 49948 |
| 2056 | 49344 | 50009 — depth |
| 0 | 0 | 36 |

The same six resources resolve after each of two EID0 resets. GBufferC native bytes retain
SHA256 `bb4bd6b0785f8b5282bdf02d7e13f7f295d2efa46ce192eb36bb0aef4e0ca1b8`.
The AIR text is approximately105KB and includes the actual descriptor-index dependency chain.
These checks prove the specified static bindings and native handle identity, not which
conditional samples ran for every pixel. No GBuffer IDs are hardcoded in replay implementation.

## Separate verification results

| Category | Result |
| --- | --- |
| Directed terminal | PASS CPU dependency evaluator (changed uniform index, unresolved varying index, missing bindings/function, ambiguous modules); T01/T12/T40/T47 compatibility. T48 exposed missing default-library bytes; fixed and all five binary paths, native output, AIR and missing-MSL diagnostics pass. |
| Real UE immediately after changes | PASS first implementation at3371 twice; descriptor-following fixes at188/3371/3551 twice; default-library fix returns to3371 twice, same six inputs and GBuffer hash. After full regression,188/3371 pass again, six inputs and GBuffer hash unchanged; all three full900×640 native images retain SHA25695a7af4f… exactly. |
| Full regression | PASS frozen70773cef: 308 captures, 7786 malformed cases, 3080 lifecycle opens; resident growth5783552 bytes; start/end library hash unchanged. |
| Assistant-operated UI | PASS final70773cef on the same capture: EID3371 shows all six Inputs; selecting GBufferA and GBufferC changes the viewed texture independently, with scene geometry visible in GBufferC. Fragment pipeline resources show the corresponding heap indices. Double-clicking Function4712 opens actual AIR instructions. Left the AIR shader tab open; this is assistant verification, not user acceptance. |

Evidence: `build-macos-debug/local-m2-descriptor-replay/ue-user-comparison/`.
`shader-inputs/` has exact library extraction and Apple's disassembly; `air-directed/` has
local checks; `air-first-ue/`, `air-final-ue/`, `air-default-fixed-ue/` retain the immediate
same-capture checks. `air-after-full/` contains the final image/event checks and exported AIR;
`air-ui-renderdoc.log` retains the final UI log. `shader-inspection-results.json` tracks current verification status.
Existing stream-reader/InitialContentsList diagnostics described in B440–B442 remain;
PASS does not claim those logs are free of diagnostics.

## Reproduce the directed checks

Close qrenderdoc and UE before running GPU probes. This script does not run the full suite
or rebuild the library; use the already-built candidate:

```sh
bash util/buildscripts/scripts/test_metal_shader_inspection_macos.sh \
  "$PWD/build-macos-debug/local-m2-descriptor-replay/testproj-20261002-211846-505594/replay.rdc" 3371
```

It records parser checks, source/direct binding and all five binary library routes, then
returns to the specified capture/event twice after EID0 and exports fragment AIR. The log
shows resolved resources; the same-capture result above additionally checks the exact six
resource identities and native GBuffer hash.

## View in the current UI

The updated qrenderdoc is open on the same capture, EID3371, with the Function4712 AIR tab visible.
Use **Texture Viewer → Inputs** to inspect the six textures. To reopen the shader, select
**Pipeline State → Fragment Shader** and double-click **Function4712**; choose
**Metal AIR (Apple toolchain)**. The common current-input tab can still display the
`65535` no-static-binding sentinel; heap thumbnail labels and independent texture selection
were verified. This cosmetic label is not claimed fixed.
