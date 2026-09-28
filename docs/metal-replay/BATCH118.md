# BATCH118 — One-level nested argument encoder

The `MTLArgumentEncoder::newArgumentEncoderForBufferAtIndex:` bridge now
creates a wrapped child encoder and records a new replay chunk. Reflection
reconstructs its parent index, size/alignment and a read-only child layout
containing up to eight direct texture/sampler members. The parent pointer
member accepts a correctly sized/aligned nested buffer; the child encoder
selects that buffer and encodes its members. Replay restores both packets
after CPU snapshots, preserves the parent→child resource relation and
records fragment texture usage one level down. Unsupported nested layouts
fail closed instead of reinterpreting arbitrary bytes or GPU addresses.

An independent native Metal Validation probe compiled `Outer`→`Inner` and
produced compute `(1,0,0,1)`. The T118 captured fragment fixture samples
the four RGBA8 quadrants through a child buffer and matches the original
T12 pixels. Three native five-frame runs, capture/XML identity, three-loop
CLI replay, Replay API pixels/usage/rewind, 18 malformed inputs and T12/T56/
T57/T60/T102 plus placement-heap sentinels passed. The Pipeline State public
structure still exposes only *direct* outer members, not a nested member
tree; do not interpret that UI limitation as missing GPU linkage. Further
nested levels, arrays, writable child members and non-texture/sampler child
resources remain separate gaps.

```sh
bash util/buildscripts/scripts/test_metal_capture_batch118_macos.sh
RENDERDOC_METAL_LAST_TEST=118 bash util/buildscripts/scripts/test_metal_replay_batch35_38_macos.sh
```

Capture SHA `e3fc631efe28…`; replay library and qrenderdoc bundle library
`84df606e5379…`. GUI/Computer Use has not run. GUI QA should inspect the
parent/child creation calls, nested buffer and texture resource links in the
API Inspector, four pixels and event seek; do not require a nested Pipeline
State tree. No commit or push.

Full terminal regression passed on 2026-09-27: 118 captures, 2625 malformed
cases and 1180 lifecycle opens, resident growth 6,668,288 bytes. Log:
`/tmp/metal-batch118-full.log`.
