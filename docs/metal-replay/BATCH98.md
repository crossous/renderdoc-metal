# BATCH98 — Two-layer rate map bound to an array render target

T97 proved two-layer descriptor and parameter-buffer reconstruction but did not bind
the map to a render pass. T98 closes that specific validation gap: a 2-layer map
(physical widths 400 and 208 on this M2 Pro) is attached to a 2-slice
`Texture2DArray` render pass. A mesh draw writes slice 0, then blit copies that
slice to the drawable. Native Metal Validation checks the pass and the copied
BGRA pixel; Replay API checks map identity, render-target array length, array
texture, mesh output, blit source/destination and final pixel. Both CLI and API
replay pass. The shader does not emit a primitive to slice 1, so slice-1
rasterization and pixel output remain unverified.

`bash util/buildscripts/scripts/test_metal_capture_batch98_macos.sh` runs five
native trials, capture/XML, CLI3, API, 26 malformed variants and targeted
v1/v2/v3 compatibility. A replay guard now rejects a rate-map layer count that
does not match the pass's effective render-target array length. T95/T96 gain
one additional malformed pass case each; total malformed counts for T95–T98
are 18/18/22/26. No bridge/old-chunk marker is removed by T98; remaining
raw counts are **51/41**, plus T70's separate unsupported ICB range.

Full regression command:

```sh
RENDERDOC_METAL_LAST_TEST=98 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

Full log: `/tmp/metal-batch98-full.log`. No UI/Computer Use, commit or push.

Later GUI QA: open `t98_capture.rdc`; inspect a 2-layer map in the render pass,
array target with two slices, mesh draw on slice 0, following blit to the
drawable, green center pixel and event seeking. Do not infer slice-1 raster
output from this capture.
