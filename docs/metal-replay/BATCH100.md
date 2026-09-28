# BATCH100 — Async mesh pipeline with second rate-map layer

T99 proved the synchronous mesh-pipeline capture path for non-default
`maxTotalThreadgroupsPerMeshGrid=2`. T100 runs the same two-layer render-pass
and mesh primitive routing through asynchronous pipeline creation. The app
mutates the descriptor after the async call; capture preserves its BGRA8,
mesh function, options and grid-limit snapshot. Native Metal Validation,
capture/XML, CLI3, Replay API slice-1 pixels and final blit pixels pass.

`bash util/buildscripts/scripts/test_metal_capture_batch100_macos.sh` runs five
native trials, capture, replay, 10 async-mesh and 26 rate-map malformed
variants, plus old v1/v2/v3 targeted replays. Full command:

```sh
RENDERDOC_METAL_LAST_TEST=100 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

Full log: `/tmp/metal-batch100-full.log`. T100 changes no bridge/old-chunk
marker; raw remaining **51/41**, plus T70's separate unsupported GPU-generated
ICB range. No UI/Computer Use, commit or push.
The final run passed **100 captures, 2412 malformed cases and 1000 lifecycle
opens**, with 1,949,696 bytes resident growth. Capture SHA-256 begins
`6997b8195353…`; replay library and app-bundled library begin
`d349808b9692…`.

Later GUI QA: open `t100_capture.rdc`; inspect asynchronous mesh pipeline
creation with grid limit 2, two-group MeshDispatch, array slices 0/1, blit
from slice 1 and x100 green/x175 black after the copy. Check event seek and
capture switching. Application completion callbacks are not replayed.
