# B448: descriptor CPU write range and submission-owned metadata

2026-10-03. Local changes only; no commit or push. Current library and embedded application
library SHA256 `ff444e680c0786a3b682ecee4ad696f9db9df35980b7fb56cd9a6915ca7f8b3b`.

The new real UE capture now opens and completes full GPU reset replays. The original is
`build-macos-debug/local-m2-descriptor-replay/testproj-20261003-085846-931499/original.rdc`,
SHA256 `02919da2a0494c79dd27b8a126086fbf3db85036fdd85560970611b70ed321d3`.
The audited candidate is `testproj-20261003-092235-138208/replay.rdc` in the same parent.
Only the audited coverage declaration differs; original command chunks, binary data and
thumbnail are preserved. This is a bounded supported capture, not general UE support.

## First reset failure

Normal initial GPU open already produced the exact original thumbnail. Reset replay failed
when command13872 restored buffer24 bytes26376..26400. The old whole-buffer relocation
also visited unrelated slot26568, generation5390, type4: its24-byte value was recorded but
its source binding annotation had not yet appeared. The selected write's slot was complete.
Native command buffers had completed normally; this was a CPU restoration rejection.

At coverage65, CPU updates now relocate only entries overlapping the actual write range.
Aliased table restoration maps this same physical range into each live destination. Complete
entries are still patched; malformed identities in the modified range still fail. Initial
restoration and older coverage retain their previous whole-table behavior.

The subsequent global live-slot byte assertion had the same overreach. Preflight now records
each draw/dispatch's descriptor table closure by command buffer. Submission validation only
checks its consumed keys, retaining existing generation/lifetime/Native byte checks. It does
not waive missing bindings for an actual consumer or synthesize GPU results.

Local official RenderDoc `origin/v1.x` was consulted: D3D12
`Serialise_DynamicDescriptorCopies` applies each modified descriptor index; Vulkan
`Serialise_vkUpdateDescriptorSets` applies the specified descriptor writes. Metal retains
its Native GPU-address/resource-ID relocation while adopting the same update scope.

## Partial replay failure from earlier CPU initialization

The new query fixture encodes GPU work first, then commits a separate earlier empty command
buffer carrying the query buffer's CPU snapshot. Seeking the first dispatch extends the
encoding scan to that earlier commit. The scanner correctly skipped later GPU producers,
but still processed their unowned GPU descriptor expected-value/binding metadata, failing
`DescriptorSlotEvent` at EID21. CacheReplaySubmissionChunks now associates this metadata
with its explicit DescriptorSlotProducer command, so the same selected-submission filter
handles both. CPU allocation/value annotations remain CPU events. Native execution,
submission order and capture stream format are unchanged.

## Separate verification

| Category | Evidence and result |
| --- | --- |
| Directed delayed binding | Original Native17/34/51; prior DLL reproduced failure. Fixed first/middle/future seeks across4 EID0 cycles retain exact values and markers. `metal-frame-float-20261003/split-replay-consumers.log`. Final ff444e68 retained/unretained split scenario and missing/null consumed-source API+CLI refusal PASS in `metal-event-navigation.0efHYn/`. |
| Directed frame queries | Final ff444e68 standalone/placement/parallel/earlier-snapshot Native count4, untouched sentinels and reset seeks PASS; malformed source/snapshot/range/options cases refused before waits. `metal-frame-visibility.NHbdd5/`. |
| Immediate real UE | `new-ue-consumer-slots/`: normal open and2 full reset replays PASS. All three Native BGRA8 images900×640 SHA256 `fc5f3afe33b3ec7a523d447d59d0b517337d4c8f8867f6d863f99f0ab5eb4f61`; recompressed JPEG is byte-identical to capture thumbnail. |
| Final real UE selected events | ff444e68 `new-ue-selected-fix/`:3300 BatchedLights has6 proven texture inputs and AIR;4057 actual five-target MRT draw has stable Native bytes;50 blit selects normally. Two EID0 resets each PASS. Resource12323 retains the actual UE name GBufferA and sorted unique3300 PS_Resource /3808 Clear /4057 ColorTarget. Numeric EID is CPU encoding order; Native submission order is independently retained. |
| Full regression | Fixed ff444e68 PASS:308 captures,7786 malformed cases,3080 lifecycle opens, resident growth5488640B; start/end library hashes identical. `frozen-validation-ff444e68/full-regression.log`. |
| Assistant UI | Prior unlocked B445 checks recorded separately. Final application library matches ff444e68. Desktop re-locked before new capture/source-MSL/popup direct checks; user notified while terminal work continues. |

Evidence roots above are under `build-macos-debug/`; UE evidence roots are under
`build-macos-debug/local-m2-descriptor-replay/ue-user-comparison/`. Existing stream-reader,
InitialContentsList and shutdown diagnostic logs are retained; not claimed fixed.
After-full ff444e68: new UE3300 and4349 GBufferA/C match byte-for-byte in both resets, and match the old baseline hashes33d636f6/bb4bd6b0. GBufferA contains1310 distinct packed values and35672 pixels differing from the dominant background. New complete Native images all matchfc5f3afe and the original JPEG exactly. Old UE3371/3551/188 and two full resets PASS; its GBuffer hashes and presented95a7af4f remain unchanged. Evidence: `ff444e68-after-full/`. Main/app/frozen libraries all remainff444e68. Only final UI is pending: CUA again reports Mac locked after terminal work completed. All GPU replay processes exited. The task remains active after this first fresh blocked goal turn; no commit or push.

Automatic continuation audit2: previous goal turn made concrete source/data progress and completed the terminal verification. This turn CUA again reports Mac locked; process inventory finds no live replay/test handle. No repeated GPU tests or speculative UI edits were started. Remaining direct popup/Escape/item selection, source-MSL and new-UE label UI checks require manual unlock. Goal stays active until the blocked threshold is met.

Automatic continuation audit3: previous turn made no progress because UI remained locked. CUA confirms the same condition again; authoritative process inventory is empty. Terminal and real-UE evidence is complete, but direct final UI requirements remain unproven. The blocked threshold is now met and update_goal returned blocked. No further replay, recapture or speculative source change was performed. Manual unlock is required to resume the remaining UI verification. No commit or push.

2026-10-03 final unlocked UI acceptance (supersedes the locked-Mac pending status above):
actual qrenderdoc DLL remains ff444e68 and executable8936b361. New UE normal open passes;
EID3300 Fragment Shader displays GBufferA/B/C, ScreenSpaceAO, SceneDepthZ and FWhiteTexture.
GBufferA displays scene normals and the follow tab reads `Cur Input Descriptor[1934] - GBufferA`.
The timeline has read/clear/write markers. Native Escape closes the actual usage popup;
End+Return selects its last ColorTarget entry and the actual event changes to4057 without
replay error. A physical mouse click in another panel dismisses the menu; process samples
confirm RDDialog::show is on-stack before and absent afterwards. AX tab activation alone
bypasses normal mouse handling and is not an outside-click test. CUA's main-window capture
omits the separate popup, so no new direct popup screenshot is claimed.

T01 EID5 Function14 opens the common Shader Viewer with `Captured MSL` and actual
vs_main/fs_main source. The same UE is then normally reopened and its Specialized Function4722
shows the real DeferredLightPixelMain AIR under `Metal AIR (Apple toolchain)`. Current UI is
left on EID3300 Fragment Shader for review, with the AIR tab available. No GPU CLI tests were
repeated and no source change invalidates the directed/full/after-full evidence. These are
assistant UI checks, not user visual acceptance. Evidence:
`build-macos-debug/local-m2-descriptor-replay/ue-user-comparison/ui-final-20261003/`.
The current supported capture and inspection workflow passes; this does not claim arbitrary
Metal feature parity, exhaustive dynamic bindless GPU feedback, or recovery of names/source
absent from older captures. No commit or push.
