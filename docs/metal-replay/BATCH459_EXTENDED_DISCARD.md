# B459: MSAA, untracked, transient and subresource DontCare

2026-10-03. Shares B458 candidate671c90b4 / GUIc3a0e3b6. Local, no commit/push.

## Implementation and reference

Retain common `GetDiscardPattern(RenderPassLoad/RenderPassStore)` and Vulkan's Fastest
bypass. Single-sample attachments use in-owner-buffer blits. MSAA and D24S8 use the
Vulkan/D3D12 fullscreen draw approach: typed colour, all samples, depth and stencil
handled independently, two stencil-reference passes. Debug pipelines/library/depth
states are cached and released after replay completion. No added queue, commit, CPU
wait, capture event or per-sample averaging substitutes for MSAA coverage.

Standalone replay textures opt into hazard tracking; heap replay allocations do so at
the heap boundary because their children inherit tracking. Preserve captured fences,
placement offsets, range/overlap/retirement checks and descriptor coverage gates.
Allow legal Default/Untracked/Tracked heap modes and supported MSAA heap texture
shapes; reject invalid mode3. Negative tests formerly rejecting legal untracked/default
values now use invalid enum values. This is debug allocation normalization, not a
claim that arbitrary unfenced native untracked transfers are safe.

Memoryless textures get Private replay backing, like Vulkan debug allocations for
transient resources. Captured load/store/resolve metadata stays intact. Load diagnostics
fill backing before the application pass and change its native load to Load. Store
diagnostics fill the source after real endEncoding, including resolve-only MSAA stores;
the resolved output remains untouched. Add selected3D depthPlane support, retaining
array/cube mip/slice handling. Rate-map passes fill physical attachment storage before
their mapped pass; the debug fill does not inherit that map.

## Partial replay correction

Native StoreDontCare at our artificial partial-pass end discarded the selected draw's
pixels even though future Store diagnostics had not run. Directed fixture reproduced
EID49 pixel0,0 red0 instead of64. Follow Vulkan render-pass normalization and partial
dynamic rendering STORE preservation (`vk_misc_funcs.cpp`, `vk_state.cpp`): captured
pass native stores start Unknown, allowing legal encoder setters. Real endEncoding
finalizes captured stores and applies diagnostics; debugger-created partial boundaries
store the selected contents (and preserve an existing resolve). Captured UI/API metadata
is unchanged. Selected draw now preserves clipped pixels plus Load pattern, and advancing
to actual pass end replaces them with Store pattern. Fastest preserves defined partial
draws while bypassing undefined-pixel pattern assertions. Serial and parallel parents
share this policy; children do not independently end the parent attachment stores.

## Resolve-source usage

Vulkan dynamic rendering records both ResolveSrc and Discard at endRendering when
the source store is DontCare, plus ResolveDst on its destination. Metal's
MultisampleResolve-only store has the same discarded-source semantics. B457/B459
pattern filling already handled this, but its usage list omitted Discard. Add that
usage at the same real end event; StoreAndMultisampleResolve remains preserved.
No replay/store operation or EID changes. The independent public-usage check reproduces
missing source Discard on87a024fe (`usage-before.log`, exit13).671c90b4 Balanced/Fastest
passes (`usage-after.log`, `usage-fastest.log`): case4 event39 source discard +resolve,
case5 event47 resolve without discard; destination ResolveDst matches each event.

## Directed validation

`metal-discard.IG5b1R/`: native validation, capture and Balanced/Fastest replay pass18
single-sample cases, including original Clear, typed colour, separate depth/stencil,
parallel/deferred store, unretained CBs, array mip/slice, cube face/mip,3D plane with
defined neighbour preservation, rate map, standalone untracked, automatic Untracked
heap, Default placement heap and selected draw before future StoreDontCare.204 reset
cycles pass, resident growth2,048,000B. MSAA15 cases also pass there; growth2,457,600B.

`metal-msaa-discard-directed/final8/`: final independent finite-value/glyph oracle
reads every sample via captured compute consumers; stencil is consumed by real per-sample
stencil-test draws. Covers2x/4x colour/float/uint/depth, depth/stencil plane preservation,
MSAA array layer, deferred parallel Store, resolve-only source discard, StoreAndResolve,
memoryless resolve, standalone untracked fence and automatic/placement heap MSAA.
Fastest asserts only defined contents.204 cycles pass, growth2,359,296B.

Hardware query: Apple M2 Pro supports2x/4x, not8x or D24S8. Code allows supported8x
allocations and D24S8 raster fill; neither is locally GPU-validated. Do not count these
as PASS. Raw MSAA Texture Viewer readback is an existing separate limitation; this
coverage uses real GPU consumers and does not claim that viewer feature is implemented.

An additional native control found that no-draw combined depth/stencil Load passes can
leave the loaded plane's resolve undefined on this GPU, even after an explicit native
Clear→Load initialization. Preserve native behavior. Final plane coverage tests the
stored source samples with depth reads and stencil-test consumers, and verifies the
defined standalone depth/colour/memoryless resolves. Historical `native-ds-load.log`
and failed resolve checks are retained, not mislabeled a pattern-fill failure or PASS.
MSL depth-return declaration and nil depth-state setter failures in initial raster
attempts were corrected before final validation; current native Metal validation passes.

`metal-msaa-discard-directed/store-planes/` on671c90b4: native Metal validation,
capture, Balanced/Fastest and204 reset cycles PASS. Depth StoreDontCare glyph covers
all four source samples while stencil37 remains intact; stencil StoreDontCare is
consumed by real stencil-test draws on all samples while depth.25 remains intact.
Store-plane stress resident growth3,325,952B, within16MiB guard. Final33 base cases
and resolve-source usage checks also pass on671c90b4 in
`metal-discard-directed/b458-b459-final-671c90b4/`.

## UE and acceptance

Same c506da2e UE capture was immediately checked after each library change. Current
3928 lighting/3509 producer, EID0 resets and chronological3415/3509/3928 GBufferA usage
pass;3928 GBufferA/C/depth and SceneColor remain byte-identical to B457. Evidence:
`metal-discard-partial-directed/ue/`, `metal-discard.IG5b1R/ue/`, final8/ue/.

Full: fixed87a024fe `frozen-validation-87a024fe/` PASS, exit0,308 captures,
7786 malformed checks and3080 lifecycle opens. Resident growth0B; start/end library
hash identical. Earlier failed candidates above remain historical, not accepted.
87a024fe post-full33 Balanced/Fastest cases, same UE events/overlays/Clear Before
and three complete frame readbacks PASS, unchanged from baseline.
Current671c90b4 fixed full PASS:308 captures,7786 malformed checks,3080 lifecycle
opens, resident growth0B, exit0 and unchanged library hash.

Post-full671c90b4 same UE PASS:3928/3509 twice with EID0 resets, six light
overlays/None restore, Clear Before restore, normal open and two full reset readbacks.
Evidence `metal-discard-directed/b458-b459-post-full-671c90b4/` and
`b458-b459-final-671c90b4/`. Four lighting-resource hashes match B457; all three
900x640 presented readbacks equal fc5f3afe. Library/capture hashes unchanged.

See B458 for preserved failure logs and submission-model corrections.

UI: actual671c90b4 app opens the same UE with "No problems detected". Selected
3928 BatchedLights, displayed GBufferA scene normals and chronological timeline
clear/write/read markers; actual3928→3509→3928 and54 Begin Metal Blit Pass→3928
seeks succeed without completion errors. Left at3928 GBufferA, RGB, Overlay None,
Range[0,1]. Right clicks on caption/image and a coordinate attempt did not expose
a menu in the automation's AX/screenshot result. Menu contents, usage-item jump and
outside-dismiss remain unverified; terminal GetUsage PASS is not a menu/UI PASS.

New capture is not required for these replay changes.

## Reproduce

After building, close qrenderdoc/UE and run:

```sh
bash util/buildscripts/scripts/test_metal_discard_macos.sh \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc 3928
```

This compiles tiny native fixtures, checks captured replay in Balanced/Fastest, runs
204 reset cycles for each single-sample/MSAA family, then immediately checks the same
UE lighting event. It creates one evidence directory and does not run the full suite.
Use the existing full batch script only for broad shared-state changes or acceptance.
