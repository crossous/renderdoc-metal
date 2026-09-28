# BATCH95–97 — Rasterization rate map

## Implemented

- T95: capture/replay `newRasterizationRateMapWithDescriptor` (old chunk 1030),
  `copyParameterDataToBuffer` (new chunk 1315), and render-pass map identity.
  One-layer all-1 samples preserve the 400×300 physical target; parameter bytes are
  captured and available in Replay API. Capture schema v2 adds render-pass map fields
  while v1 files remain readable.
- T96: horizontal samples 0.5/0.5 on one layer. Native M2 Pro reports physical
  208×300; replay pixels at x100 and x175 verify the triangle contracts within the
  physical target. The Replay API's texture description still reports the drawable's
  400×300 logical size, so tests do not pick beyond physical x208.
- T97: descriptor snapshot/reconstruction now accepts up to four layers when the
  replay device supports that layer count. Schema v3 appends extra-layer sample arrays
  without changing v1/v2 decoding. The native two-layer map reports 400×300 for layer
  0 and 208×300 for layer 1. Its parameter data is copied, captured and replayed.
  T97 does **not** bind this two-layer map to an array render target; that execution
  path remains unverified.

Raw markers fall from 52/42 to **51 bridge / 41 old chunk**; T70's unsupported
GPU-generated ICB range remains an additional functional gap (52/42 inclusive).
Max MetalChunk is 1316. No UI/Computer Use was run, and nothing was committed/pushed.

## Terminal QA

`bash util/buildscripts/scripts/test_metal_capture_batch95_macos.sh`,
`bash util/buildscripts/scripts/test_metal_capture_batch96_macos.sh`, and
`bash util/buildscripts/scripts/test_metal_capture_batch97_macos.sh` each build,
run native Metal Validation, capture, inspect XML, replay CLI/API and exercise
malformed input. The malformed sets contain 17, 17 and 22 cases respectively.
T97 targeted compatibility checked old v1/v2 captures T01/T78/T88/T94/T95/T96.
Full regression command:

```sh
RENDERDOC_METAL_LAST_TEST=97 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

Full log: `/tmp/metal-batch97-full.log`.
The final run passed **97 captures, 2303 malformed cases and 970 lifecycle
opens**, with 1,589,248 bytes resident growth. Capture SHA-256 prefixes are
T95 `b4aa8a2f99ef…`, T96 `9bfcb4bddea0…`, T97 `1cab41975a47…`;
replay library and app-bundled library `d93f1c816b54…`. Fresh schema-v3
single-layer T95/T96 captures were also replayed via CLI/API, while the saved
T95/T96 captures remain v2 compatibility cases.

## Consolidated UI QA later

Open T95/T96/T97 alongside the existing Metal captures. T95 should expose map
creation, parameter copy, render-pass map resource link and the ordinary green
triangle. T96 should show the half-rate descriptor and a horizontally contracted
physical triangle. T97 should expose two distinct layer sample arrays and parameter
buffer data, but its pass deliberately has **no** map binding; do not claim two-layer
array-target rendering from this capture. Check capture switching and event seek.
