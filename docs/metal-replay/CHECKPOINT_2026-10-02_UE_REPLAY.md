# UE 5.8.3 replay checkpoint — 2026-10-02

This checkpoint collects the local M2 work from B326 through B439 on top of
`c4be68bb7fce662e4dd8498408981fa2e824c3b6`, on `metal-replay-v1.46`.
The user requested this commit and push before providing further visual
differences. It is a reproducible investigation baseline, not final user visual
acceptance. Preserve the evidence below when investigating those differences.

## Included changes

- Explicit descriptor table identities, source provenance, generation retirement,
  GPU producer ownership and relocation, with bounded pre-submit validation.
- Initial resource contents and views, placement heap reuse, command submission
  ordering, completion waits and replay/reset lifetimes.
- Captured compute/render indirect evidence checked against actual replay GPU
  arguments; MRT, layered volume and texture display support.
- UE capture plugin annotations, an isolated matching MetalRHI provider, CPU
  audit tools and a workflow that preserves the original capture while preparing
  a separately audited replay copy.
- Native Metal fixtures, negative input checks and batch investigation records.

Generic resource and submission behavior was compared with the repository's
D3D12/Vulkan implementations and locally available matching UE source. The
repository contains provider integration code and preparation tools; installed
UE source/builds, captures, binaries and diagnostic archives are local artifacts.

## Evidence at this checkpoint

Host: Apple M2 Pro, 16 GiB RAM, macOS 26.1, Xcode 26.0.1, UE 5.8.3 CL58210709.
The verified main, frozen and qrenderdoc embedded library SHA256 is
`b814fdbbbd570ecd3643e94f33df768bcbb82577c2a0a5198d89d2a50def6ced`.
This identifies the pre-commit tested binary; a later rebuild embeds a different
Git revision and must be identified separately.

| Verification | Result and scope |
| --- | --- |
| Targeted | B438 interleaved producer ownership and B439 retired Private texture-to-buffer reuse reproduced in tiny native cases, then repaired with positive/reset and negative checks. Each fix immediately returned to the same `faa8540e` UE capture. See the batch records for the distinct tested library hashes. |
| Full regression | Frozen current `b814fdbb`: 308 normal captures, 7786 malformed cases and 3080 lifecycle opens passed; resident growth 7,602,176 bytes. Library hash unchanged throughout. |
| Real UE | Normal open and full EID9924; loaded plus two EID0/full replays produce identical 900×640 native bytes. EID2017 indirect arguments are actually read as 8,1,1. BasePass3310 five MRTs and volume3473/3533 four 64³ outputs are stable across resets. These partial checks also passed immediately after the full regression. |
| UI operation | Assistant operated qrenderdoc with the current library: normal load, actual fifth MRT selection, both volume outputs at Slice63, and the presented Texture51804 at EID9924. Editor UI, sky and yellow platform are visible; normal exit returned 0. |
| Previous UE control | `ef026832` on current `b814fdbb`: normal open and three full native images retain the previous exact hash `1e7f9e791cdabde129856f23e9a13e3463c0d311324dc95e8eb25b8045e74e34`. |
| Commit preparation | Python AST and shell syntax checks, staged whitespace/content checks; no new GPU run solely for committing. |

Current original capture SHA256:
`faa8540e474dbeddc579d48d3e35918e2b112f03803687042ef8cfb0ffc6b83f`.
Audited replay copy SHA256:
`8b1c0a81d6557c14b69591cfb774cc7c748752d05914df37e52114ef131fa701`.
All 71211 original chunks and 40195 binary members plus the thumbnail are
preserved; only the validated coverage65 declaration is added.

Native full image SHA256:
`95a7af4fecf053d22f5dc3dc48b263c6bc345a072b246804d75d0dfd32065af0`.
Re-encoding using the capture thumbnail's resize/JPEG procedure matches its bytes
exactly (`fa2bfb47fef200d7e8cfcbde0505e7d69c59d7ef37c66525c0e8fe96c647f056`).
This is not an independent captured full-precision image or per-pass golden.
Stable MRT readbacks and observed UI do not establish correctness of every pass.
User visual acceptance remains pending; investigate the user's next differences
against this exact capture and revision. Arbitrary UE captures are not covered.

## Local entry points and evidence

- [Run the verified capture or make a new one](../../util/ue/README.md).
- [B434: first actual indirect data mismatch](BATCH434_RETIRED_DESCRIPTOR_UNIFORM_REUSE.md).
- [B436: original-preserving candidate audit](BATCH436_AUDITED_UE_REPLAY_WORKFLOW.md).
- [B438: interleaved GPU producer ownership](BATCH438_INTERLEAVED_GPU_PRODUCER_OWNERSHIP.md).
- [B439: retired texture backing, current full/UE/UI results](BATCH439_RETIRED_PRIVATE_TEXTURE_BACKING.md).

Local current capture session:
`build-macos-debug/local-m2-descriptor-replay/testproj-20261002-211846-505594/`.
Central local results: `build-macos-debug/local-m2-descriptor-replay/latest-ue-replay-results.json`.
These generated files are excluded from Git. Historical artifacts were verified
and moved to `/Volumes/CauseUseMac/RenderDocMetalArchives/20261002-2050`, with
symlinks at their old local paths. All 34 project originals remain local. The
current library, provider and replay copy do not require the external disk.

Follow-up cadence: first locate the earliest real data/image discrepancy;
necessary targeted verification, then immediately the same UE capture. Run full
regression for unbounded shared restoration/submission/lifetime changes or the
next acceptance candidate. Record targeted, full, real UE and UI separately.
