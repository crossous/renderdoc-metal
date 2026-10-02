# B439 — Retired Private texture backing reused by a buffer

2026-10-02. Current candidate library b814fdbb. No commit or push.

After B438, the SAME faa8540e UE capture rejects buffer51866 at streamOffset11732928.
Its64KiB allocation in Private/tracked placement heap4267, offset41669632,
overlaps background RG11B10Float texture44759 (192×104). Descriptor slots
24/24432,24/24816 and24/33144 that named it are retired before this birth. The
existing placement path only permits a buffer predecessor.

The new coverage65 path requires bounded Private/tracked backing, no live logical
descriptor or view source naming the old texture, and every old consumer already
submitted. Preflight records exact texture→buffer pairs and consumer identities;
later captured uses of the retired backing reject. Runtime keeps the Native
texture retained and reuses `WaitReplayCommandBuffer` to finish precisely those
submitted consumers before permitting backing reuse. It never commits unfinished
work to make an alias possible. Initial contents and frame resource reset paths
remain the existing paths.

Generic references: D3D12/Vulkan separate logical descriptors and retained GPU
resource lifetime. Metal placement allocation uses the native heap footprint.
Apple's resource synchronization and memory heaps documentation are the platform
reference; this does not rely on creation alone synchronizing overlapping work.

## Validation

- Baseline b69d9b82: Native/capture17/34/51 pass; SAME tiny capture rejects the
  texture→buffer birth at1152.
- Fixed b814fdbb: retained/unretained/subset-view Native execution and capture,
  all12 event seeks per case after EID0 resets, exact17/34/51 and markers pass.
  Live descriptor, unsubmitted consumer and older coverage all reject before
  frame GPU work through API and CLI. A view fixture explicitly references its
  parent so the original parent initial pixels are captured.
- Immediately SAME real UE faa8540e: strict pre-submit and payload-preserving
  audit pass. Normal OpenCapture and full EID9924 replay pass (241 draws,
  114 dispatches). Loaded plus two EID0→full Native900×640 images are identical,
  SHA256 `95a7af4fecf053d22f5dc3dc48b263c6bc345a072b246804d75d0dfd32065af0`.
  Re-encoded JPEG exactly matches the original capture thumbnail,
  SHA256 `fa2bfb47fef200d7e8cfcbde0505e7d69c59d7ef37c66525c0e8fe96c647f056`.
  No independent original full-precision image golden is claimed.
  Candidate `8b1c0a81d6557c14b69591cfb774cc7c748752d05914df37e52114ef131fa701`
  preserves71211 original chunks and40195 binary members plus thumbnail; only
  coverage65 metadata is added. Session `testproj-20261002-211846-505594`.
- SAME real UE partial verification: EID2017 buffer47752/offset16 directly reads
  captured8,1,1 in both resets. BasePass3310's five Native MRTs, volume3473/3533's
  four64³ RGBA16Float outputs, and end9924 all pass two EID0 reset seeks. All nine
  MRT pairs are byte-identical; volume samples are finite and nonzero. These
  volume checks establish Native stability, not equality to an unavailable
  captured full-precision volume golden.
- Frozen b814 acceptance full regression passed21:46:39:308 normal captures,
  7786 malformed cases,3080 lifecycle opens; resident growth7602176B. Start/end
  library hashes match. Verify-only runner, no Qt/main rebuild. Immediately
  after full regression, SAME faa EID2017/3310/3473/3533/9924, actual8,1,1 and
  all nine Native MRT pairs passed again.
- Current b814 qrenderdoc UI: normal load reports no problems. Actual EID3310
  FB4/Texture49929 selection, EID3473 both64-cubed outputs at Slice63, and
  EID9924 actual presented Texture51804/900×640 were observed. Complete editor
  UI, blue sky and yellow platform are visible. Normal quit exits0. Assistant
  CUA observations are recorded separately; human manual acceptance is not claimed.
  Foreground log `ue-b814-ui-foreground.log`, results `ue-b814-ui-results.json`.

- Previous accepted ef026832 control on current b814: normal open, full EID9669,
  loaded and two EID0 resets all pass. All three Native900×640 images retain
  the previous exact SHA256 `1e7f9e791cdabde129856f23e9a13e3463c0d311324dc95e8eb25b8045e74e34`.
  Evidence `ue-ef-b814-control/results.json`.

Evidence: `build-macos-debug/local-m2-descriptor-replay/retired-texture-alias-fix/`
and `build-macos-debug/metal-retired-texture-alias.LA54CN/`.

Targeted entry:
`bash util/buildscripts/scripts/test_metal_retired_texture_alias_macos.sh`.
