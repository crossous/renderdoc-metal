# B453 — Vulkan Clear Before Draw / Pass

2026-10-03. Worktree only, no commit or push. Persistent Texture Viewer parity goal remains active.

## Reference and implementation

The reference is the existing official implementation in `renderdoc/driver/vulkan/vk_overlay.cpp`, `VulkanReplay::RenderOverlay`, ClearBeforeDraw / ClearBeforePass branch. It clears all active MRTs, retains the original shaders and pipeline, executes the selected draw or the real pass prefix, and returns a transparent overlay while the common viewer displays the modified original texture. The common `ReplayOutput::SetTextureDisplay` / `DisplayTex` path restores the original replay when changing back to None. Metal now follows that contract.

Metal has no in-pass `vkCmdClearAttachments` equivalent. Clear Before Draw completes and stores the actual source pass prefix, restarts the same attachments with Clear colour / Load depth and stencil, and executes the actual selected captured draw with its original PSO, shaders, bindings and arguments. Clear Before Pass changes the native load operation of the actual owning encoder (parallel children share their parent), then follows normal replay to the selected EID, preserving intervening work and submission order. The target pass stores attached MRTs before partial completion even when captured terminal stores discard them. Captured pass metadata is unchanged.

When viewing depth/stencil, Less/LessEqual clear depth to 1; Greater/GreaterEqual/Never clear to 0 and stencil to 0. Equal/NotEqual/Always retain depth/stencil, matching Vulkan. Pass depth policy uses the first actual draw's pipeline. The original fragment shader retains discard, texture sampling, blend output and exported depth. This is different from the fixed-colour Depth Test overlay: its fragment-depth-export mask path remains unfinished.

The common viewer calls OnlyDraw after generating the overlay. Since this helper already executed that draw, an exact selected-EID one-shot completion marker consumes that call. Other replay requests reset the marker. This avoids a second blend or a full replay erasing the clear. Existing fixed-colour overlays retain their original restore behavior. No common ReplayOutput code was changed.

Current support is ordinary single-sample, single-layer 2D, base mip/slice targets. MSAA, memoryless, array/layered, non-base attachment subresources and rasterization-rate maps are rejected before resource mutation. Clear Before Draw uses the eight ordinary captured direct/indirect draw forms; mesh/tessellation/ICB support is not claimed.

## Directed terminal validation

Final candidate library SHA256: `a086f3e5060bec3f74b0f0c9aadc1612873f755b4d1b983a28e683f8f70de810`.

`build-macos-debug/metal-clear-before.bD5XD2/`: PASS serial, parallel, Greater, Equal, Always, original fragment-depth-export, captured discarded depth-store, and combined D32S8 stencil test fixtures. Each fixture checks two MRTs against four independently executed Native Metal reference programs, twice. All output pixels compare byte-for-byte. The overlay itself is transparent, and common viewer None restoration returns both original MRTs exactly, with no manual seek. Native and capture/replay use the Metal validation layer. Library before/after hashes match.

`build-macos-debug/metal-test-overlay.aZMnTS/`: PASS all seven existing Native cases and all six existing real draw overlays, including viewport/scissor and multi-state rejection/reset. Same UE all six overlays twice and original SceneColor/GBuffer/depth byte restoration also PASS. Thus the original-PSO replay flag did not alter the verified replacement-PSO path.

The earlier `798b6ec7` run in `metal-clear-before.zZO1yF/` passed seven cases and UE before adding the unsupported-attachment guard and stencil fixture. It is historical evidence, not the final library.

## Same real UE validation

Same capture: `build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`, SHA256 `c506da2e07223f1027db3ddab42b5929f0f9ad64ff48ea4d8d5b10a6b08fed6b`.

Light EID3300, actual pass begin3226, SceneColor12320, 320x240 RGBA16F. `metal-clear-before.bD5XD2/ue.log`: PASS Draw/Pass twice. Both execute the captured light shader, change SceneColor, leave GBufferA12323 / GBufferC12331 / depth12397 unchanged, and restore all original bytes through None. This pass contains only this light draw, so both clear modes produce byte-identical results. Both cycles also match. Modified SceneColor SHA256 `09c675ce5c43d5164e53b641d982368e121a65a656245cdd55b1439f95907af2` with clear colour (0.2,0.3,0.4,1). Of 76800 pixels, 35197 differ from the quantized clear colour; this is nonzero light contribution, not a geometry-coverage claim.

After full regression, `build-macos-debug/metal-clear-before-after-full-a086f3e5/ue.log` repeats both overlays twice: PASS, all four output hashes equal the pre-full `09c675ce...` result. `images/terminal.log` also passes normal OpenCapture, full EID9839 and two EID0 resets. All three 900x640 Native presented readbacks have SHA256 `fc5f3afe33b3ec7a523d447d59d0b517337d4c8f8867f6d863f99f0ab5eb4f61`, matching the accepted baseline.

The known input hashes remain `33d636f6...`, `bb4bd6b0...`, `2a174a5c...`.

## Full regression and actual UI

Current frozen full regression PASS under `build-macos-debug/frozen-validation-a086f3e5/`: exit 0, 308 captures, 7786 malformed inputs, 3080 lifecycle opens; resident growth 6127616B. Start/end SHA256 both equal the candidate hash above. It is justified by the original-pipeline branch, partial pass stores and the OnlyDraw completion/restore contract. Previous ac2a9748 full PASS is historical only.

Actual UI validation on the new embedded candidate is pending: CUA app launch returned cgWindowNotFound; inventory then explicitly reported that the Mac is locked and requires manual unlock. The user has been asked once to unlock. This is the first blocking observation in this resumed phase; keep the goal active. Terminal readback does not replace UI rendering, and assistant UI verification does not constitute user visual acceptance. Embedded library equals a086f3e5; executable SHA256 `bea8625c719b602d27e66a4c0dd5540859abba5eadac644f050898ed4851838b`.

## Commands

Close qrenderdoc and UnrealEditor before serial GPU checks.

```sh
bash util/buildscripts/scripts/test_metal_clear_before_macos.sh \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc 3300
bash util/buildscripts/scripts/test_metal_test_overlay_macos.sh \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc 3300
```

## Remaining parity work

Depth Test with exported fragment depth should follow both Vulkan `vk_overlay.cpp` and D3D12 `d3d12_overlay.cpp`: preserve the original fragment shader, disable colour writes, record depth-passing fragments in a private stencil mask, then resolve that mask to green over real red coverage. Current rejection must remain until real fragment output is tested; interpolated vertex depth is not a replacement. Metal needs a bounded native depth-to-D32S8 copy/mask/resolve path while preserving captured bindings and resource restoration.

Quad Overdraw and Triangle Size need actual GPU invocation/primitive data, following Vulkan callbacks and post-VS data respectively. They remain unsupported; pass-event metadata or a coverage image alone is insufficient. MSAA/memoryless/layered overlays and compressed/MSAA numeric texture statistics remain pending.

## Next depth-mask component proof

`util/test/metal/metal_depth_mask_native.mm` is an independent 64x32 Native component fixture, not a RenderDoc overlay implementation. MSL offline compilation and Metal validation GPU execution PASS (`build-macos-debug/metal-depth-mask-native.log`). It copies a private D32 depth source into owned D32S8, runs the original fragment shader with colour writes disabled and depth-passing stencil replacement, then resolves an X32_Stencil8 view to red/green. VS depth .8 would fail everywhere; the actual original FS writes .1 on the left, .9 on the right, and discards a strip. All 2048 pixels match the expected mask, and original colour/depth remain unchanged. Formal replay integration, captured bindings and same UE validation are still pending.
