# B456 — Vulkan Triangle Size semantics on Metal

2026-10-03. Continue the same UE capture and Texture Viewer parity task. No commit or push.

## Reference and implementation

Follow local official Vulkan `vk_overlay.cpp` TriangleSize branch, `trisize.geom`, `trisize.frag` and `texdisplay.frag`. The quantity is original projected primitive area, including primitives clipped by the screen or near plane; it is not the number of covered pixels. Raw output is RGBA16F `(area,area,area,1)`, with transparent untouched pixels. The display uses the shared 22-entry colour ramp and Vulkan's `2 + floor(20 - 20.1*(1-exp(-0.4*area)))` bucket formula, clamped to0..21. Empty alpha is discarded. The shared UI legend and Pixel Context follow the existing Triangle Size path.

Metal has no geometry shader. On the local M2, `barycentric_coord,center_no_perspective` and the determinant of its screen derivatives yield original projected triangle area: `0.5/abs(dfdx(b).x*dfdy(b).y-dfdx(b).y*dfdy(b).x)`, clamped to0.001 like Vulkan. Native GPU results are checked against a CPU projected-area oracle, including clipping and unequal clip-space W. The selected captured vertex shader, bindings and original geometry execute on GPU. This does not add a post-VS mesh extraction claim.

`RenderGeometryOverlay` shares the bounded ordinary-prefix/state/copy machinery with Quad Overdraw. Triangle Size preserves original depth/stencil tests and writes on private copies, plus cull, viewport/scissor and captured draw parameters. Pass mode accumulates the actual earlier draws of the same native pass. Ordinary original prefixes preserve previous original writes/side effects, as Vulkan's inter-draw replay does. Scratch resources remain owned through completion and captured targets are restored by the existing overlay restoration path. Public restore, submission and lifetime protocols are unchanged.

Current support is local M2 triangle list/strip, 2D single-sample/base mip/slice/single layer, ordinary stored depth/stencil and a128MiB scratch cap. Function tables, mesh/ICB, layered/memoryless/MSAA and non-triangle topologies remain unsupported. The UI enables Draw/Pass, labels MSAA unsupported, and uses shared enum ordering and display controls.

## Directed and same UE results

Backend/main/embedded library SHA256 `8e3328441d034c815fe4ee5b390b0f1927deec5e07543f0418f640d330e43c44`. GUI SHA256 `48b2791c21f0191367d9bef8b9adde90a2742fce01b5b6d7b5dd3cbb739717ba`.

`metal-triangle-directed/replay0.log` PASS initial captured area/heatmap/restoration, immediately followed by `ue-first.log` PASS the same UE EID3300 twice. `metal-triangle-size.n9Gauw/` PASS eight geometry cases and UE. Expanded `metal-triangle-size.Oc8T7A/` PASS eleven cases: ordinary128px², tiny2px², screen-clipped256px², unequal W, near-plane clipping, offset viewport64px², scissor, two draws (Draw1 covered pixel, Pass121), parallel encoder, Triangle Strip, and overlapping near/far primitives within one draw with depth writes (areas128/512px²). All public Draw/Pass overlays are checked twice against original Native area pixels, GPU heatmap bytes and original resource bytes/None restoration. Native depth case proves the farther primitive cannot overwrite nearer coverage. An initial extended Native CPU oracle used the wrong top-left edge threshold and failed; the corrected pixel-centre edge rule passed. That failed oracle attempt is not recorded as product PASS.

Same unchanged capture SHA256 `c506da2e07223f1027db3ddab42b5929f0f9ad64ff48ea4d8d5b10a6b08fed6b`, path `local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`. Expanded `ue.log` PASS EID3300 Draw/Pass twice:35840 area pixels, maximum32px², finite area/RGB/alpha validity and Pass covers Draw. SceneColor, GBufferA/C and main depth bytes remain unchanged after overlays and None. No new capture was needed.

`metal-quad-overdraw.Bp4STH/` PASS all eleven prior Quad cases after helper refactor, followed immediately by UE3300 twice (35840 positive pixels/max2, per-quad counts and restoration unchanged). `metal-texture-view.ts5e3Q/` PASS statistics/Range/gamma/alpha/subresources/NaN/Clipping directed checks, followed immediately by same UE3300 GBufferA12323 twice (histogram76800/minR0/maxR0.987292), original hash33d636f6 unchanged. Whole-frame comparison and combined acceptance are recorded below when completed.

## Full and actual UI — separate status

Current frozen8e332844 full regression PASS:308 captures,7786 malformed cases,3080 lifecycle opens, resident growth5816320B, exit0; start/end hashes identical. Evidence `frozen-validation-8e332844/full-regression.log`. B454's bb793d8d full remains historical. The first frozen-script preparation accidentally retained the automatic build step; that attempt was interrupted (exit130), retained as `interrupted-build-attempt.log`, and is not a full PASS. The candidate library/bin were recopied from the original8e332844 paths and the actual frozen script has no build step, plus start/end hash checks. Unused intermediate build objects from that attempt were removed after the full run, keeping candidate library/bin and logs. The public regression script now accepts `RENDERDOC_METAL_SKIP_BUILD=1` and checks library hashes; the Mac build script defaults to2 jobs (override `RENDERDOC_METAL_BUILD_JOBS`).

Post-full `metal-triangle-directed/post-full/` PASS same UE Quad/Triangle3300, original-FS Depth3112 (shader10697/depth12363/19200 green), six light overlays3300, Clear Before Draw/Pass3300 and numeric GBufferA checks, each two cycles with original bytes/None restoration. GBufferA/C/depth dumps retain hashes33d636f6/bb4bd6b0/2a174a5c. `images/terminal.log` PASS normal OpenCapture and two EID0 full resets; all three900x640 BGRA images retain accepted SHA256 `fc5f3afe33b3ec7a523d447d59d0b517337d4c8f8867f6d863f99f0ab5eb4f61`. Library and same capture SHA remain identical before/after this serial sequence. Full success is followed by actual UE progress checks, not used as their replacement.

Current actual Quad/Triangle rendered UI observation is pending. The current8e332844/48b2791c application opened the same UE normally and showed the original SceneColor sky/orange scene at3300, correct outputs/markers/indirect parameters, with no API error. Automated combo/keyboard selection did not change the overlay. An independent standard Qt5 `qt-combo-probe` window reproduced the same automated selection issue; no RenderDoc-specific dropdown defect or rendered overlay PASS is inferred. Both agent-owned windows exited before serial terminal GPU work. B455's earlier attempt was locked; B454's successful Depth/Clear UI belongs to bb793d8d. A terminal texture readback does not count as actual UI, and an assistant UI check does not replace user acceptance.

After post-full terminal checks completed, the current application reopened the same capture normally and is left at3300 SceneColor/None for UI confirmation. A fresh actual screenshot verifies restored sky/orange scene and correct light draw. No terminal GPU probe remains running. The optional question asks whether the user previously could select Overlay on this Mac, to distinguish automated input behavior from a real common Qt UI problem. Current Quad/Triangle rendered UI still remains unverified.

```sh
bash util/buildscripts/scripts/test_metal_triangle_size_macos.sh \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc 3300
```

GPU tests are serial with qrenderdoc and UnrealEditor closed. This script checks an existing unchanged library and does not run full, build the application, capture UE, commit or push. When preparing a combined acceptance candidate, the existing full runner can skip rebuilding with `RENDERDOC_METAL_SKIP_BUILD=1 RENDERDOC_METAL_LAST_TEST=312 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh`; this is not the default after each local fix.
