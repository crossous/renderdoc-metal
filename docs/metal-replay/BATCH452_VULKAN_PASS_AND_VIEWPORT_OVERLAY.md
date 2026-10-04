# B452 — Vulkan pass events and Viewport/Scissor overlay

2026-10-03. Worktree changes only; no commit or push. Continue the active Texture Viewer parity goal.

## Vulkan reference and implementation

`VulkanReplay::GetPassEvents` (`renderdoc/driver/vulkan/vk_replay.cpp`) returns earlier draws/mesh draws and pass boundaries, excluding the selected EID. The old Metal implementation returned only the selected EID. Metal now records action ownership by the actual render encoder; parallel children share the parent render pass. It does not walk global `previousAction`: command buffers can be CPU-encoded in interleaved order. End boundaries and compute/blit/unknown events return no render-pass draws. This is metadata only, with no change to native submission or resource ownership.

The independent pass oracle reads serialized encoder IDs and parallel-child factory relationships from the public Structured File. It does not read the new ownership maps. The new 16×16 Native fixture encodes two command buffers sharing the same target in interleaved order, commits B before A, and opens a second encoder on A with the same target. Expected Native readback is red on the left and green on the right. Existing parallel and interleaved blit captures provide separate cases.

`DebugOverlay::ViewportScissor` follows Vulkan's `vk_overlay.cpp` and `OverlayRendering::CreateTempViewportPipe` in `vk_debug.cpp`: real selected draw with original vertex shader and captured arguments, red with scissoring disabled, green with original scissor, no cull/depth/stencil test. Then GPU annotation draws use the same 3px border, 16px checks, grey viewport border, translucent blue viewport interior and black/white scissor border with transparent interior. RGB and alpha both use SrcAlpha/OneMinusSrcAlpha, as Vulkan does. Annotation rectangles do not substitute for actual geometry coverage.

The ordinary single-sample/single-layer graphics restrictions from B450 remain. Multiple viewport/scissor arrays are detected and conservatively rejected before replay changes. A later single-state setter resets the count, so it is supported again. Depth-export and remaining pass/quad/triangle overlays are still unfinished; this batch does not claim complete Vulkan parity.

## Directed and same UE verification

Pass metadata candidate `7be262c49cb2ea207b5373b3dfdd53d84bd67d520c12a9c794b4d3a68e1ca7e8`: `build-macos-debug/metal-pass-events.EhKmmB/`. PASS 18 interleaved, 14 parallel, 18 blit action queries against serialized ownership. Same UE EID3300 returns `[3226]`, excluding 3300. Two EID0 reset cycles retain GBufferA/C/depth bytes. Capture remains `build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`.

Viewport candidate `0776deb7e34773ac4465b927e074c9a72f3b69053ec0b09bb8ecd11f36ba2374`: `build-macos-debug/metal-test-overlay.7S6vRT/`. PASS existing serial/parallel/cull/ephemeral cases with all six supported draw overlays, two cycles. The new offset viewport/scissor fixture checks transparent outside, grey viewport border, white scissor border, actual red/green coverage under blue annotation, and the Vulkan alpha blend result. PASS both cycles. Same UE EID3300 also passes all six overlays twice; SceneColor and GBufferA/C/depth raw bytes are unchanged after common replay restoration. The three input hashes still match the prior accepted baseline.

Final directed run on the same `0776deb7` library: `build-macos-debug/metal-test-overlay.as8INS/`, PASS seven Native fixtures (serial, parallel, cull, ephemeral, offset viewport, multi-state rejection, array→single reset) and same UE all six overlays twice. Multi-state rejection returns no overlay and leaves original bytes unchanged; resetting to a single viewport/scissor produces the verified correct overlay again. Both candidate hash files match. The three UE input hashes are `33d636f6…` / `bb4bd6b0…` / `2a174a5c…`, unchanged from B450–451.

Final pass oracle on `0776deb7`: `build-macos-debug/metal-pass-events.Ot0GPV/`, PASS all 18/14/18 small action queries, Native-equivalent final red/green shared target readback, and same UE `[3226]` list and two reset cycles. Thus the pass metadata result above is also verified on the current overlay candidate, not only the intermediate `7be262c4` library.

## Full regression and UI scope

No new full regression yet: this local batch changes pass metadata and adds a bounded overlay on the already validated prefix/restore path. The latest full PASS belongs to previous library `ac2a9748`, not this candidate: 308 captures / 7786 malformed / 3080 lifecycle opens, growth 8716288B, unchanged frozen hash. Run a new full suite when common restoration/submission/lifetime changes cannot be bounded locally, or when preparing the overall acceptance candidate.

Assistant UI validation is distinct from terminal validation and user visual acceptance. PASS actual CUA UI on the current embedded `0776deb7` library / executable `3325475454f09383e29af30f1b88817c8ff8cbb1334ebaa6c4d0bcea0ad114b0`: loaded the same UE from Recent Captures (normal load, no errors), navigated to EID3300, selected Viewport/Scissor Region, and observed the black/white border, translucent viewport and real lower geometry coverage. Switched to Depth Test and observed the normal green geometry coverage, then selected None. Original SceneColor view restored. Other unfinished overlays remain N/A; user visual acceptance is still pending.

The first screenshot immediately after changing the combo can show the prior image while the asynchronous replay is still running. A subsequent screenshot confirmed the actual overlay; the AX combo label alone was not treated as rendering success.

## Commands

GPU serial; close qrenderdoc/UnrealEditor before terminal probes.

```sh
bash util/buildscripts/scripts/test_metal_pass_events_macos.sh \
  build-macos-debug/metal-test-overlay.okKgzy/parallel_capture.rdc \
  build-macos-debug/metal-event-navigation.qCBnCu/marker_capture.rdc \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc 3300

bash util/buildscripts/scripts/test_metal_test_overlay_macos.sh \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc 3300
```
