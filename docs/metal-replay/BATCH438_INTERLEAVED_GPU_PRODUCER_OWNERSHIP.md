# B438 — Interleaved GPU producer annotation ownership

2026-10-02. No commit or push. Original UE capture remains immutable:
`faa8540e474dbeddc579d48d3e35918e2b112f03803687042ef8cfb0ffc6b83f`.

The first rejection was command51382's commit at streamOffset201344. Encoder51572
belongs to a different command and has an explicit descriptor producer pending
for table24/offset30744. Its matching GPU-value annotation is the next chunk.
The unrelated command was already fully encoded. The preflight incorrectly used
the global pending-producer map as a submission gate.

The preflight now tracks explicit producer encoder ownership per pending slot.
At coverage65, another known, unsubmitted, live compute encoder may have a pending
annotation when an independent command commits. Own, ended, unknown or submitted
ownership still rejects. Ordinary blit expectations have no exemption; dispatch,
draw and end-of-frame completion checks remain unchanged. Native submission order,
captured arguments and descriptor values are unchanged.

Reference: D3D12 `Serialise_SetComputeRootDescriptorTable` stores descriptor state
under `m_BakedCmdListInfo[m_LastCmdListID]`. This is command ownership bookkeeping,
not a Metal synchronization workaround.

## Validation

- Baseline ae206881: tiny Native execution/capture passes; the SAME capture is
  rejected at commit/streamOffset2880 before frame GPU work.
- Fixed b69d9b82: retained/unretained, two captures each, actual producer/consumer
  values41→80 and80→41, independent Native clear and EID0 resets pass. Five
  ownership/completion/older-coverage negative groups pass via API and CLI.
- Immediately SAME real UE: original commit rejection is gone; preflight reaches
  `MTLHeap::newBuffer(offset)` at11732928, texture44759→buffer51866. No frame GPU
  execution or full-image success claimed for this capture.
- Full regression and UI were not rerun for this local patch. Previous ae full/UI
  evidence remains tied to ae, not b69 or a later library.

Evidence: `build-macos-debug/local-m2-descriptor-replay/interleaved-producer-fix/`,
`build-macos-debug/metal-interleaved-producer.cqrVjc/`, and UE session
`testproj-20261002-205658-806111`.

Targeted entry:
`bash util/buildscripts/scripts/test_metal_interleaved_producer_macos.sh`.

## Disk recovery

Archive: `/Volumes/CauseUseMac/RenderDocMetalArchives/20261002-2050`.
763 historical targets (684 test directories plus older module recovery, frozen
build and UE exports), 233184 files, 13128821296 bytes were copied and verified
with SHA256 before removing local copies and restoring original paths as links.
The local build directory fell from approximately16.2GiB to3.2GiB; available
space recovered from304MiB to approximately13GiB. Current library and capture
hashes were checked before further development. All34 original project captures
were retained; three exact duplicate groups were recorded without deleting them.

`plan.json`, `migration.jsonl`, `additional-plan.json`,
`additional-migration.jsonl`, `project-original-capture-inventory.json` and
`summary.json` contain the inventory and old/new paths. Current builds, newest
repair fixtures, the accepted ef audited candidate and current faa original stay
local. Historical linked artifacts require the external disk to remain mounted.
