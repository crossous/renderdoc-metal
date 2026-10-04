# B440–B442: UE event navigation and GPU submission prefix

Local changes after pushed checkpoint `95214a2777fdf63144fb342de0131f04729152aa`.
No new commit or push. User requests normal cross-pass scope duplication be preserved and
D3D12/Vulkan functional behavior be used as the reference.

## Evidence and scope

Same audited UE capture throughout:
`build-macos-debug/local-m2-descriptor-replay/testproj-20261002-211846-505594/replay.rdc`,
SHA256 `8b1c0a81d6557c14b69591cfb774cc7c748752d05914df37e52114ef131fa701`.
Original `faa8540e` and all project originals are unchanged. No recapture or UE rebuild needed.
Candidate library SHA256: `c7bb6da17b885a6a6a5e52268e79f223542a89eaa538ad9fe2905be35dc3644c`.

User exports are `/Users/crossous/Downloads/testrdc.txt` (D3D12) and
`/Users/crossous/Downloads/testrdc_metal.txt` (Metal). The D3D12 export is another
platform/resolution/project configuration, so counts and pixels are not an identical-workload golden.
Metal contains one present/end-of-capture, not nine concatenated frames.
All9 captured NaniteBasePass pushes belong to2 command buffers (51585/51636)
and9 different encoders; `nanite-scopes.json` lists the actual calls and EIDs.

UE's `MetalCommandEncoder.cpp` reapplies every active debug group when creating render,
compute and blit encoders. This legitimately repeats `NaniteBasePass` and lighting scopes.
D3D12 parallel recording also splits scopes. Do not merge distinct recorded scopes by name.
The supplied exports contain 2 vs 9 NaniteBasePass labels and 0 vs 59 descending real
EID transitions. The latter exposed an action-tree bug distinct from normal scope repetition.

## B440: marker ownership and UI representation

Compute/blit push, pop and signpost handling now selects the owning command buffer before
recording an event, matching the render encoder path. When an interleaved command buffer
resumes, its scope is continued at the chronological tail using the common fake-marker
representation, instead of appending later EIDs beneath an earlier root. Captured GPU calls,
original EIDs and recorded scope boundaries are retained. Synthetic IDs are unique and
outside the captured range; indirect argument resolution must not rename synthetic scopes.
The Event Browser now finds the first captured descendant through nested synthetic scopes.

The same UE action tree has no descending real EIDs, last real EID 9924, with 262 synthetic
scope continuations. These are navigation nodes, not additional GPU passes or frames.
The display still retains capture CPU EIDs; it does not renumber them into D3D12 baked-command
submission order. B442 makes the selected GPU state respect submission order.

Metal setBytes/setVertexBytes/setFragmentBytes bindings have bytes without a buffer ID.
The pipeline UI now shows their API name and supplied size, instead of dropping them or
marking them Empty. Reference: D3D12 InlineData/root constants and Vulkan push constants
in their pipeline viewers. This does not claim full bindless shader resource feedback,
constant decoding or an inline-byte inspector; resource-less rows are not fake buffers.

## B441: clicking Begin Metal Blit Pass EID188

Baseline b814 reproduced the user's fatal `Invalid Metal replay completion` at EID188.
The underlying failure was a submission snapshot of future table 51386, heap22 offset
19079168, with no overlapping predecessor allocation. The old check demanded an alias
proof even for a fresh, non-overlapping range. The fix defers its bytes until its real
birth, as already done for safe future snapshots, without inventing the resource or writing
captured GPU addresses into another allocation. Every actual overlap still requires the
exact validated alias pair, retired old slots and completed old GPU consumers.

Old b814 fails the tiny fresh-range capture; the candidate passes. Retained/unretained
existing alias positives and live-slot/unsubmitted-consumer rejection checks still pass.
Same UE EIDs99/144/188/190/194 passed two EID0/reset seeks with the local fix; 188 also
passed again after B442. The original RDC does not need modification.

## B442: first real lighting data deviation

The real discrepancy is at DirectLighting draw EID3371, not just its marker label:

- Nanite work belongs to command buffer51636, committed at original EID6057.
- Lighting draw3371 belongs to command buffer51638, committed at original EID6470.
- Nanite dispatches3472/3544 occur later in CPU encoding order than draw3371.
- Old partial replay stopped at the file offset for3371, submitted the incomplete earlier
  command buffer, and lost the later-encoded Nanite producer work.

Reference: [D3D12 ExecuteCommandLists](../../renderdoc/driver/d3d12/d3d12_command_queue_wrap.cpp)
and [Vulkan QueueSubmit](../../renderdoc/driver/vulkan/wrappers/vk_queue_funcs.cpp): a selected
command executes after the complete earlier submissions, independently of CPU recording order.
For the existing preflighted coverage65 single-queue contract, replay caches command/encoder
ownership and commit boundaries from the structured events. A partial seek includes complete
earlier submissions and only the selected command buffer's prefix; it skips later command
buffers and the selected buffer's later commands. CPU snapshots keep their validated submission
owners. Global resource/lifetime records retain their original ordering. No coverage extension,
wait bypass, GPU readback substitution or arbitrary multi-queue claim is introduced.

Native GBufferC resource49933 at draw3371 (320×240 BGRA8):

| Measurement | Before | After |
| --- | --- | --- |
| Nonzero pixels | 2361 | 35672 |
| Distinct packed pixels | 12 | 32 |
| SHA256 | `8da023e3b8c291508c28bc9a3e4644b650ed0656bd8d50a52d6ec66e01f9795e` | `bb4bd6b0785f8b5282bdf02d7e13f7f295d2efa46ce192eb36bb0aef4e0ca1b8` |

After bytes match post-Nanite EID3551 and the full-frame GBuffer, across two EID0 resets.
The lighting SceneColor itself changes from4076 to76325 nonzero RGB pixels.
This fixes missing platform/wall geometry at the lighting event. It is stronger evidence
than merely successful open or repeatable wrong bytes.

For inspection, EID3310 is an early conventional BasePass draw before Nanite shading;
EID3551 exposes all five populated MRT bindings after Nanite. GBufferC is target3/resource49933.
UE `GBufferInfo.cpp` maps target4 to GBufferD/CustomData for this five-target layout; all-zero
custom data is not itself evidence of missing geometry. Default-lit shading does not require
nonzero custom material data. The actual GBufferB alpha low4 bits contain only
background0 (41128 pixels) and DefaultLit1 (35672 pixels), matching UE
`DecodeShadingModelId`/`SHADINGMODELID_DEFAULT_LIT`. EID3251 is a depth/stencil-only light-volume draw with no color
attachment. EID3371 is the actual batched light draw with SceneColor49920 and fragment shader4712.
EID3182 is a debug scope on a compute encoder, not a graphics draw with missing color output.

## Verification — keep the four categories separate

| Category | Result |
| --- | --- |
| Directed terminal | PASS `metal-event-navigation.qCBnCu`: unique/chronological marker IDs and correct owners, synthetic selections, retained/unretained fresh/alias tables, entirely late producers and producer tails, EID0 native buffer checks; live old slots and unsubmitted alias consumers rejected through API/CLI. |
| Same real UE immediately after fixes | PASS marker-fix, boundary-fix and submission-fix probes on the same8b capture. Final post-regression188/3182/3251/3371/3551 each twice after EID0, with corrected GBuffer hashes at all lighting stages. Three 900×640 full-frame native images (loaded + two resets) retain SHA25695a7af4f… exactly; actual GBuffer correction measured above. |
| Full regression | PASS fixed `frozen-validation-c7bb6da1`: 308 normal captures, 7786 malformed cases, 3080 lifecycle opens; resident growth8716288 bytes, start/end library SHA unchanged. |
| UI operation | PASS, assistant-operated qrenderdoc on final c7bb6da1: double-click EID188 without fatal error; EID3182 compute pipeline5381 and 16-byte inline input; EID3371 graphics pipeline5287, fragment shader4712, heap24/25 and 48-byte inline input, visible SceneColor geometry; EID3551 target3/GBufferC visible geometry. Nested continuation range3366–3371 is correct. App left open at3371/SceneColor for user inspection. This is not user acceptance. |

Evidence root: `build-macos-debug/local-m2-descriptor-replay/ue-user-comparison/`.
`lighting-dependencies/` preserves the pre-B442 bytes; `submission-fixed-ue/` has corrected
bytes, `lighting-gbuffer-before.png`, `lighting-gbuffer-after.png` and comparison JSON.
Targeted runner: `bash util/buildscripts/scripts/test_metal_event_navigation_macos.sh`.
Full regression is an acceptance gate after these shared replay changes, not a per-edit default.

The earlier e543aad0 acceptance run was stopped after normal captures while completing
the same ownership correction for blit signposts. It is not a full-regression pass.
Final c7bb6da1 directed run includes both blit signposts; `candidate-ue/` immediately
rechecked188/3371/3551 and confirmed the same corrected GBuffer bytes.
The static ownership audit accounts for every encoder command in all77 submissions.

D3D12/Vulkan are source references and the user-supplied D3D12 event export; this Mac
validation does not claim a native D3D12/Vulkan GPU test or identical cross-platform images.

UI checks completed on 2026-10-03 Asia/Shanghai. `ui-renderdoc.log` preserves the current UI log.
The post-regression UE log still contains 19852 pre-existing `File and decompress stream readers do not support seeking` diagnostics and one `SystemChunk::InitialContentsList not handled` diagnostic. These also occur in baseline logs; they did not cause these replay checks to fail and are not fixed by this change. PASS here means the specified API/native-image/UI checks passed, not an error-free diagnostic log.
