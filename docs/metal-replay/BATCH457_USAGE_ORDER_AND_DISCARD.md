# B457: GBuffer usage order and DontCare debugging

2026-10-03. Local changes only; no commit/push. Same UE capture
`local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`, SHA256
`c506da2e07223f1027db3ddab42b5929f0f9ad64ff48ea4d8d5b10a6b08fed6b`.
Library `1437b5cb89b8bb600a6d22d72bf4aae0e10c16371d5c7f1a81e2582adf97f8e5`;
GUI `459b09eb45490fbe8aea1fa6b2e6f5fc3ac9ce26ba6e6348dceb9b826e580a41`.

## The observed order is CPU encoding order

Structured chunks and public GetUsage agree on the same GBufferA resource12323:

| Capture EID | Operation | Encoder | Command buffer | Queue |
| --- | --- | --- | --- | --- |
| 3300 | Fragment shader input | 14162 | 14146 | 11 |
| 3808 | Attachment load Clear | 14176 | 14143 | 11 |
| 4057 | Attachment write, indexed draw | 14183, parallel child | 14143 | 11 |
| 6790 | Commit | — | 14143 | 11 |
| 6791 | Commit | — | 14146 | 11 |

The producer and consumer are encoded on different CPU threads. No explicit enqueue precedes
these selected submissions. On the same Metal queue, producer14143 is committed before
consumer14146. Thus the actual execution relationship is Clear3808 → FB Color4057 → input3300,
although their displayed EIDs run in the opposite order. These are the same texture identity;
there is no heap-alias or GBuffer-name inference.

Official D3D12 `Serialise_ExecuteCommandLists` and Vulkan queue-submit loading splice baked
command-list/buffer events into the root event space at submission, incrementing the root
EID by baked eventCount. The present Metal backend keeps original CPU capture EIDs. The
B442 partial-replay submission closure already handles the late-encoded, earlier-submitted
producer; sorting GetUsage differently would break the common ascending EID contract and
would not fix the event tree. This change explains the encoding/submission distinction in
the actual Texture Viewer usage menu. It does **not** renumber the Metal frame into baked
submission order. That remains an architectural UX difference from D3D12/Vulkan.

`metal_submission_usage_audit.py` resolves parallel child owners without GPU execution.
Evidence: `build-macos-debug/metal-discard-directed/ue-submission-order.json`.
Bindless read discovery still occurs at inspected proven accesses; the usage list is not an
exhaustive GPU per-invocation access trace.

## DontCare patterns were missing; captured pass replay now uses the shared patterns

Previously Metal recorded ResourceUsage::Discard, but passed native DontCare loads/stores
through without diagnostic pixels. Vulkan ApplyRPLoadDiscards/ApplyRPStoreDiscards use
`GetDiscardPattern(RenderPassLoad/RenderPassStore)` and preserve a filled load by changing
its native operation to Load. Metal now does the same for supported attachments:

- Blit the common LOAD DONT CARE pattern before the native captured pass; only successfully
  filled attachment loads become Load. Actual application clears remain Clear.
- Blit STORE DONT CARE after a captured pass ends, including deferred parallel store actions.
- Write the selected mip/slice and layers; depth and stencil are filled independently.
- Encode debug copies into the original owner command buffer. No side queue, extra commit,
  added capture event or CPU wait is introduced. Copied completion blocks retain upload staging
  even for unretained application command buffers, and release on completion/cancellation.
- Match Vulkan's Fastest optimisation bypass. Overlay/output-owned scratch passes do not
  traverse these captured-pass hooks. Clear Before overrides apply before load diagnostics.
- Support single-sample 2D/array/cube uncompressed supported color formats, D16/D32 and D32S8
  planes, with a128MiB upload bound. MSAA, untracked resources (need explicit fences), memoryless, rasterization-rate-map passes, D24S8,
  3D and unsupported formats retain native behavior; these are not claimed implemented.
- A partial replay that stops inside the pass does not apply a future store discard. The
  store hook runs at the captured end boundary, preserving selected-draw inspection.

## Staging ownership correction before acceptance

The project compiles Metal bridges without ARC. The first156d8b0c implementation did not
balance the +1 from newBuffer: a copied completion block retained its staging capture, but
the creator reference also survived completion. Explicit release after installing the block,
and on the blit-creation failure path, now balances that reference. Enabling ARC directly
was rejected by the bundled metal-cpp header, so the final code follows the existing manual
reference-counting convention rather than changing official headers.

The public replay stress fixture uses an autorelease pool each cycle to exclude transient
command objects from the measurement. With four warm-up cycles and200 measured cycles,
old156d8b0c grew58,900,480B and failed the16MiB limit; release-only634c8aac grew1,277,952B
and passed. Final1437b5cb, including the untracked-attachment guard, grew737,280B and passed
all ten pixel cases for all204 cycles. Evidence: stress-old-pooled.log,
stress-fixed-pooled.log and final-directed/stress.log. The earlier unpooled measurement
(360MB growth) includes transient autoreleased command objects and is not the isolated
staging comparison. The same UE3300/4057 was immediately checked after each local fix,
with unchanged3300 GBufferA/C/depth.

## Validation

Directed: Native Metal validation capture and public API replay PASS for ten cases and two
EID0→end resets: clipped GPU draw overwrites only its pixels; RGBA8/RGB10A2/R32F/R32UI load
patterns; D32; depth-DontCare/stencil-Clear and depth-Clear/stencil-DontCare; deferred parallel
store; unretained command buffer; array slice1/mip1; untouched Clear. Glyph oracle checks
repetition past64-pixel/8-row boundaries. Initial test-harness attempts incorrectly read
undefined EID0 state and expected127 instead of common UNorm S8 white255; both corrected,
not suppressed product failures. See `metal-discard-directed/{native,capture,replay}.log`.

Immediate real UE: PASS3300 and4057 twice; GetUsage exactly3300 PS_Resource /3808 Clear /
4057 ColorTarget, sorted/unique. At3300 GBufferA/C/depth hashes remain33d636f6/bb4bd6b0/
2a174a5c respectively, byte-for-byte with the accepted baseline. Evidence:
`metal-discard-directed/ue/events.log` and `eid-3300-cycle-*-target-*.bin`.

Historical156d8b0c full regression passed308/7786/3080, growth7,979,008B; its post-full
UE overlays/numeric checks and three presented frames also passed. These predate the
creator-reference fix and do not validate the final library. Final1437b5cb full regression PASS308 captures /7786 malformed cases /3080 lifecycle
opens, resident growth999,424B, exit0 and identical start/end library hashes. Evidence:
`build-macos-debug/frozen-validation-1437b5cb/full-regression.log`. Post-full UE PASS3300/4057 twice, six light overlays and Clear Before Draw/Pass with
None restoring original output/input bytes. Normal open and two full frame resets all
produce the originalfc5f3afe native presented hash; GBufferA/C/depth retain33d636f6/
bb4bd6b0/2a174a5c. Evidence: `metal-discard-directed/final-post-full/`, with identical
before/after library and capture hashes. The prior locked-screen UI check was resumed after the user unlocked the Mac.
The current app opened the same capture normally (status: No problems detected), selected
light EID3300 and displayed the named GBufferA input with scene-normal geometry. The
window is left at3300/GBufferA with RGB enabled and Overlay=None. CUA right-click attempts
via AX and native coordinates on the thumbnail/labels did not expose the menu in AX or
screenshots; left AX selection and RGB checkbox changes worked. This does not establish
whether the failure is native input automation or a product issue. The heading/entries and
outside-click dismissal therefore remain **unverified**, not a UI PASS. Older
8e332844 full/UI results validate the previous candidate only. User feedback at the start
of this turn says the prior texture-view functions appear functional; that is not detailed
acceptance of every overlay or this new discard implementation.

Additional final guard check: Native capture with an explicitly untracked color attachment,
Balanced and Fastest replay both PASS the defined clipped draw pixels for two resets,
without asserting any values in undefined DontCare pixels. All eleven final fixture cases
pass; `metal-discard-directed/untracked-check/{native,capture,replay,fastest}.log`.

## Remaining parity and UI work

The Mac is unlocked and current1437b5cb/459b09eb UI is open at3300/GBufferA. Menu annotation
and outside-click dismissal remain unverified as described above; repeating GPU regression
would not resolve this input/UI check. No code or library changed during this unlock audit.

Parity is partial. Common pattern generation and supported load/store diagnostic semantics
follow Vulkan/D3D12. Coverage still excludes the attachment classes listed above. The
submission audit proves producer-before-consumer execution for this GBuffer, but does not
make encoding EIDs into execution EIDs. Proper event-tree/usage parity requires the official
baked command-buffer submission model, including replay selection and usage mapping;
menu wording or usage-only sorting does not implement it. No commit/push.
