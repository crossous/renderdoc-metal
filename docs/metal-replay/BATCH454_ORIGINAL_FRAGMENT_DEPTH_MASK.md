# B454 — Original fragment depth and Vulkan stencil-mask overlay

2026-10-03. Continue the persistent Texture Viewer parity goal. Worktree only; no commit or push.

## Implementation and reference

Follow the fragment-depth-export branches in official `renderdoc/driver/vulkan/vk_overlay.cpp` and `renderdoc/driver/d3d12/d3d12_overlay.cpp`: real selected geometry provides red fail coverage, the original fragment shader records depth-passing fragments into a private stencil mask, and the mask resolves to green over the red coverage. The original shader's exported depth, discard, inputs and pipeline specialization remain active. Vertex depth does not substitute for fragment depth.

Metal `RenderDepthExportOverlay` now performs this path. It copies the completed prefix's actual depth into owned shader-readable storage, converts it to owned D32S8 with stencil clear0, replays the original FS with colour writes disabled and stencil replace1 on depth pass, then resolves an X32_Stencil8 view. All attached MRT formats are retained in private dummy MRTs. The original MRTs and depth are not used as writable overlay targets. Red and mask phases each restore their own prefix so vertex/storage side effects do not feed one phase into another. Common final OnlyDraw uses the existing full restoration flag; no new public completion/submit/restore behavior was added.

Temporary textures, shaders, pipelines and depth states are owned with Native SharedPtr until GPU completion. Prefix depth gets an explicit Native retain across source-pass completion. Total private storage, including the returned overlay, is capped at128MiB. Existing ordinary single-sample/base-mip/2D/single-layer restrictions remain; memoryless, mismatched attachment sizes and function-table remapping are not claimed.

## Real UE failure and local fix

The first real depth-event run on `28a4ae22` aborted the replay process (exit134) when a native copy received nil sourceTexture. Evidence: `metal-depth-export.xXlh7P/ue-depth-3112.log`. This was not a Mac reboot. The cause was missing source command-buffer context selection after WithoutDraw: the submission prefix can end on another encoder, making saved depth metadata empty. The other Metal overlays already explicitly select the captured source encoder before saving state. Apply that same step here, retain the source depth, and verify it before issuing the copy. No public ordering or resource-lifecycle mechanism was replaced.

## Directed terminal evidence

Current library SHA256 `bb793d8d7d69ef11ab4f789a4745cd5deca13e22bc6d7b12656c95baa92d16d0`; main and embedded libraries match. UI executable remains `bea8625c719b602d27e66a4c0dd5540859abba5eadac644f050898ed4851838b`.

`build-macos-debug/metal-depth-export.JcRT1K/` PASS seven Native fixtures twice, with Metal validation: source MSL, parallel encoding, precompiled metallib/AIR, D32 with two MRTs/sampling/blend/discard, conservative depth(less), conservative depth(greater), and interleaved consumer-first encoding / producer-first submission. Source/parallel/AIR/less require512 green pixels where original fragment depth.1 passes even though vertex depth.5 would fail on the left. Greater exports.9 and requires512 red pixels. MRT/discard requires384 green and128 red pixels. Every RGBA16F overlay pixel is checked; common replay restores original colour, and the MRT case also checks its second target and D32 bytes. Candidate start/end hashes match. These are actual GPU draws and stencil masks.

Earlier `87050951` / `28a4ae22` Native and light checks are historical evidence; the real depth-event failure above is not recorded as a PASS.

## Same real UE evidence

Same capture `local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`, SHA256 `c506da2e07223f1027db3ddab42b5929f0f9ad64ff48ea4d8d5b10a6b08fed6b`.

Final `metal-depth-export.JcRT1K/ue.log` PASS EID3112 twice: source FS ResourceId10697, depth output ResourceId12363 (160x120),19200 green pixels, original depth bytes unchanged. Trace explicitly confirms the original-fragment mask path; this is not just validation of the old non-exporting light shader.

`metal-depth-export.JcRT1K/ue-light-3300.log` PASS existing six overlays twice at light EID3300 on the same final bb793d8d library. SceneColor and GBufferA/C/depth bytes restore unchanged. The earlier light snapshot hashes on28a4ae22 still match the accepted three input baselines; final repeated light readback comparisons prove no changes across overlay refresh.

The intermediate bd4273e2 full-frame normal-open and two-reset image probe PASS under `metal-depth-export.FMCFMw/images/`: all three900x640 BGRA readbacks match baseline SHA256 `fc5f3afe33b3ec7a523d447d59d0b517337d4c8f8867f6d863f99f0ab5eb4f61`. Final bb793d8d post-full whole-frame recheck PASS under `metal-depth-export.JcRT1K/post-full/images/`: normal OpenCapture and two EID0 reset replays all match fc5f3afe exactly.

The same missing context selection was also present in Clear Before. Select the actual owner encoder (serial or parallel) before validating its attachments, and reselect it after restoring Draw prefix. Final `metal-clear-before.dSvhQI/` PASS eight Native MRT reference cases and same UE3112 Draw/Pass twice. This real event has no colour attachments and depth Compare Always; the probe explicitly asserts that captured policy. Both modes therefore skip depth clear and must reproduce the original depth exactly, matching Vulkan. `ue-light-3300.log` on the same final library also PASS both modes twice, with exact original restoration. All candidate hashes match.

Post-full `JcRT1K/post-full/` PASS original-FS Depth EID3112 twice, Clear Before EID3112/3300 both modes twice, six light overlays twice. GBufferA/C/depth SHA remain33d636f6/bb4bd6b0/2a174a5c; all restore checks pass.

## Full regression and UI status

The final combined Depth Export + Clear Before context candidate PASS one frozen full suite under `frozen-validation-bb793d8d/`: 308 captures,7786 malformed cases,3080 lifecycle opens, resident growth9502720B, exit0. Start/end SHA256 both equal current bb793d8d. Post-full same UE checks run separately below. Local fixes first completed directed Native and same UE validation; the new full is for the combined acceptance candidate, not the default after each local change. Previous a086f3e5 full PASS (308/7786/3080, growth6127616B) remains historical evidence.

Actual assistant UI PASS on bb793d8d after window access recovered: same UE EID3112 selected HalfResolutionDepthCheckerboardMinMax (160x120,D32S8), Depth Test visibly rendered green over the entire texture, None restored the original dark depth image. EID3300 SceneColor baseline showed sky and orange scene; Clear Before Draw and Pass each visibly removed the sky while retaining original light-shader scene draws; None restored the sky and original scene. Separate screenshots after selection verified rendering, not just the combo label. Capture stayed functional with no API error. GUI exited before the next GPU tests. Earlier locked attempts remain historical failures. This is assistant UI observation, not user manual acceptance.

## Commands and next work

```sh
bash util/buildscripts/scripts/test_metal_depth_export_overlay_macos.sh \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc ue-depth:3112
```

For all six light overlays, use the same script with3300, or the generated replay probe with3300. GPU serial; close qrenderdoc/UnrealEditor first.

Remaining: actual UI validation for Clear Before and depth export, Quad Overdraw, Triangle Size/post-VS support, MSAA/layered/memoryless overlays and compressed/MSAA numeric statistics. Shader depth-export classification still conservatively inspects library source/AIR where selected-entry reflection lacks output signatures; selected-entry metadata precision should be improved. This batch does not claim complete Vulkan parity.
