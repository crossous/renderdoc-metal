# B458: submission EIDs, action tree and resource usage

2026-10-03. Local changes only; preserve B440–457 changes, no commit/push.
Current library `671c90b4baa25e06de078e85cbbf2b39115baa6e2d9b2d718aabea856ca33436`.
GUI `c3a0e3b6ca92b73ac4fa66516122c0fb710145510d9763b9e65a3963dabac1ba`;
its embedded library matches the terminal library. B459 shares this candidate.

## Fix

B457 proved that the apparent read-before-write sequence was CPU recording order:
producer CB14143 commits before consumer CB14146 on queue11. The menu explanation did
not fix the underlying event model. Follow official D3D12 `Serialise_ExecuteCommandLists`
and Vulkan `Serialise_vkQueueSubmit`: bake each command stream into the public event
space at submission, with a flat command-buffer boundary, followed by the baked stream at its original scope level. Retain serialized chunks
and physical offsets for native replay; no capture rewrite or new capture is needed.

The one-to-one mapping updates API events, actions, pipeline/inline/sampler snapshots,
pass queries, MultiAction ranges and resource usage together. Rebuild scope trees per
owner to avoid artificial cross-command-buffer continuation markers. Each API event
has exactly one inspector owner, including ICB execution slots. The old encoding-order
menu annotation is removed. EIDs describe logical submissions, as with D3D12/Vulkan;
they are not globally synchronized GPU timestamps across unrelated queues.

Partial selection resolves physical chunks from the selected baked prefix and completes
earlier submissions. It does not include later work from the selected command buffer
merely because another producer's physical tail extends past the selected CPU offset.
Parallel child streams are baked in creation order. Existing validation restrictions on
concurrently open parallel children remain; this change does not relax their encoder or
descriptor/lifetime guards or claim support for previously rejected captures.

Same UE capture:
`build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`,
SHA256 `c506da2e07223f1027db3ddab42b5929f0f9ad64ff48ea4d8d5b10a6b08fed6b`.

| Old encoding EID | Public submission EID | GBufferA operation |
| --- | --- | --- |
| 3808 | 3415 | Clear |
| 4057 | 3509 | FB Color |
| 3300 | 3928 | FS Resource, lighting |

Old Nanite4349 maps to3626; old depth-overlay draw3112 maps to4524. GBufferA is still
resource12323, not an inferred alias. The public usage list is ascending and unique.

## Directed and real UE validation

`metal-event-navigation.WsfAPq/`: retained/unretained native captures and replay pass
marker ownership, reversed CB commit order, future alias/nonoverlapping snapshots,
producer tails, delayed unrelated binding, and negative descriptor gates. Producer-tail
dispatch outputs now follow submission order:17/0/0,34/0/0,34/34/0,34/34/51.
Expectations were changed to these native dependencies, not weakened.

`metal-submission-directed/`: marker GPU readbacks pass in B-before-A submit order
(34 then17), no fake continuations. Same UE was checked immediately after each local
change, at3928 and3509 twice with EID0 resets. Event tree and usages are chronological;
lighting GBufferA/C/depth and SceneColor are byte-identical to the accepted baseline:
`33d636f6`, `bb4bd6b0`, `2a174a5c`, `51527fdf` respectively. Producer3509 is an earlier
write and is not expected to equal the later Nanite-completed lighting inputs.

First frozen full attempt `frozen-validation-0da61eed/` failed at T20, not a full PASS.
The failure was duplicate API inspector events: MultiAction child events were retained
in child actions and also accumulated into the next ordinary action. Bijection now
excludes those retained child events from the ordinary pending list. T20 directed
`t20-fixed.log` and immediate same UE `ue-icb/` passed. Later B459 partial-store fixes
are included in835ff48a; earlier successful directed checks alone do not accept it.

Second frozen full attempt `frozen-validation-835ff48a/` failed T31: the submission
parent moved compute begin/end scopes away from the root expected by the existing
fixture. Adding PassBoundary alone (b46aa2e3) did not fix this. Candidate2afefc59
layout uses flat submission boundaries like D3D12/Vulkan, preserving scope depth.
Unmodified T31 and T20 directed checks (`t31-flat.log`, `t20-flat.log`) pass.

Third frozen full attempt `frozen-validation-2afefc59/` failed T52 because
command-buffer signal/wait calls after the last encoder action had no API Inspector
owner after moving commit to the front. Candidate168a70e4 preserves those tail calls
on a flat command-buffer end boundary without adding or renumbering API events.
T52 directed `t52-tail.log` passes all original event/data/rewind checks.

Fourth frozen full attempt `frozen-validation-168a70e4/` failed T259.
Acceleration-structure encoder chunks use the field `Encoder`, which the ownership
cache missed. Include that field for both creation and lookup, retaining the same
native operations and guards. Candidate87a024fe T259 refit, T266 copied refit and T52
sync directed checks (`t259-as.log`, `t266-as.log`, `t52-as.log`) pass unchanged.

Full: fixed87a024fe `frozen-validation-87a024fe/` PASS, exit0,308 captures,
7786 malformed checks and3080 lifecycle opens. Resident growth0B; start/end library
hash identical. Earlier failed candidates above remain historical, not accepted.
87a024fe post-full:18 single-sample and15 MSAA Balanced/Fastest cases PASS;
same UE3928/3509, six light overlays/None, Clear Before and normal open +two full
resets PASS. SceneColor/GBuffer/depth and all presented frame hashes match baseline.

Current671c90b4 adds only MSAA resolve-source Discard usage (see B459); directed
resolve-only/StoreAndResolve source+destination usage and Balanced/Fastest pixels pass.
Final fixed671c90b4 `frozen-validation-671c90b4/` PASS, exit0:308 captures,
7786 malformed checks,3080 lifecycle opens, resident growth0B, hash unchanged.
Post-full671c90b4 same UE PASS:3928/3509 twice with EID0 resets, six light
overlays/None restore, Clear Before restore, normal open and two full reset readbacks.
Evidence `metal-discard-directed/b458-b459-post-full-671c90b4/` and
`b458-b459-final-671c90b4/`. Four lighting-resource hashes match B457; all three
900x640 presented readbacks equal fc5f3afe. Library/capture hashes unchanged.

UI: actual671c90b4 app opens the same UE with "No problems detected". Selected
3928 BatchedLights, displayed GBufferA scene normals and chronological timeline
clear/write/read markers; actual3928→3509→3928 and54 Begin Metal Blit Pass→3928
seeks succeed without completion errors. Left at3928 GBufferA, RGB, Overlay None,
Range[0,1]. Right clicks on caption/image and a coordinate attempt did not expose
a menu in the automation's AX/screenshot result. Menu contents, usage-item jump and
outside-dismiss remain unverified; terminal GetUsage PASS is not a menu/UI PASS.

## Reproduce

Build with `bash util/buildscripts/scripts/build_metal_dev_macos.sh` (default2 jobs).
Close qrenderdoc/UE before directed GPU checks:

```sh
bash util/buildscripts/scripts/test_metal_event_navigation_macos.sh
```

The same existing UE capture opens in the rebuilt app; no recapture is needed.
Its lighting draw is now3928, GBuffer producer3509 and Clear3415. Old3300/4057
bookmarks must be relocated by action/resource, since public EIDs now follow submission.
Capture file bytes and original captures are preserved.
