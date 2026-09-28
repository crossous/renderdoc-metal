# BATCH99 — Rasterize and replay the second rate-map layer

T98 bound a two-layer map but drew only into slice 0. T99 adds a mesh shader
whose per-primitive `render_target_array_index` routes two mesh threadgroups
into slices 0 and 1 of the same array render pass. Layer 1 uses horizontal
samples 0.5/0.5, giving 208×300 physical pixels on this M2 Pro. Blit copies
slice 1 to the drawable. Native Metal Validation checks green at x100 and
black at x175; Replay API checks the mesh event's slice-1 pixels, the blit
source slice and the final drawable pixels. CLI replay succeeds.

The shader needs `maxTotalThreadgroupsPerMeshGrid=2`. Older mesh-pipeline
capture only preserved the default, so capture schema v4 appends `maxMeshGrid`
to mesh-only pipeline creation and replays non-default values. v1/v2/v3 captures
remain readable. The serializer's conditional element is braced: its macro
expands to multiple statements, so an unbraced `if` would consume bytes from
old captures. Targeted compatibility checks include T01/T78/T82/T94/T95/T98.

`bash util/buildscripts/scripts/test_metal_capture_batch99_macos.sh` runs five
native trials, capture/XML, CLI3, API, 19 mesh and 26 rate-map malformed
variants, and targeted compatibility. The full command is:

```sh
RENDERDOC_METAL_LAST_TEST=99 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

Full log: `/tmp/metal-batch99-full.log`. T99 removes no bridge/old-chunk
marker: raw remaining **51/41**, plus T70's separate unsupported GPU-generated
ICB range. No UI/Computer Use, commit or push.
The run passed **99 captures, 2376 malformed cases and 990 lifecycle opens**,
with 3,014,656 bytes resident growth. T99 capture SHA-256 begins
`161abbbbaeb2…`; replay library and app-bundled library begin
`d349808b9692…`.

Later GUI QA: open `t99_capture.rdc`; expect `maxMeshGrid=2`, a two-group
MeshDispatch, two-layer map on an array render target, blit source slice 1,
green at x100/black at x175 on the final target, and stable event seeking.
