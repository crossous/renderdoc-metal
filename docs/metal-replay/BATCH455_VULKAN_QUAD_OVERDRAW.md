# B455 — Vulkan quad overdraw semantics on Metal

2026-10-03. Persistent Texture Viewer parity work; no commit or push.

## Reference and implementation

Follow official local `renderdoc/data/glsl/quadwrite.frag`, `quadresolve.frag`, `texdisplay.frag` and Vulkan's QuadOverdraw callback. Each primitive increments a quad's bucket for its 1/2/3/4 live pixels; resolve sums bucket[i]/(i+1). This counts actual fragment execution in 2x2 groups, not geometry coverage painted to resemble a heatmap.

Metal uses early_fragment_tests, sample_mask/post_depth_coverage and quad_shuffle_xor on the single-sample path. These attributes are described by Apple's Metal Shading Language specification; the local M2 GPU proof verifies their real depth/stencil/helper behavior. Counter storage is private to the replay helper (Native SharedPtr, unregistered with capture resources), while the selected real VS and captured setters produce actual geometry. Original cull, depth compare, stencil compare/read mask/reference, viewport and scissor remain active. Original depth/stencil are copied to owned private textures and all depth/stencil writes are disabled. Captured fragment shaders are replaced, matching Vulkan Quad Overdraw; this is distinct from Depth Test's original-FS exported-depth mask.

Pass mode selects real earlier draws from the same native pass. Each count draw restores an independent completed ordinary prefix, preserving prior depth writes and vertex/storage side effects; counters accumulate separately. All native work waits for completion before scratch resources release. Common original restoration uses the existing overlay full-replay flag; no public resource restoration, submission order or lifetime protocol changed. Existing guards retain 2D/base mip/slice/single-layer/non-memoryless/single-sample support and128MiB private storage cap. Function-table pipeline remapping, mesh/ICB/multisample overlays remain unclaimed.

Raw output is RGBA16F count in every channel, like Vulkan's resolve. Texture display uses shared `colorRamp[22]`, floor(value+0.25), clamp bucket21 and discard bucket0. The sRGB attachment preserves the ramp's UI colour bytes. Pixel Context follows the same heatmap path. Metal UI enables Quad Draw/Pass; MSAA is disabled and labelled N/A on MSAA like Vulkan.

## Candidate and directed results

Library SHA256 `b2c8718b7fcfbfec28ee75aadf69285132d6eba7a9dd67fe780404b0fa2ed721`, main/embedded identical. GUI SHA after MSAA capability label build `6701c0cd9585dba7007c1832cfb13744582666f63b0559984652d0c698de98f2`.

Evidence `build-macos-debug/metal-quad-overdraw.13OLw3/`: original Native component9 cases PASS exact four buckets:1/2/3/4 live pixels, two repeated one-lane/four-lane draws, partial early depth rejection, stencil pass and complete stencil rejection. Public Draw/Pass replay of all9 captured cases PASS twice with raw counts, per-quad uniformity, original depth bytes and None restoration. Repeated-draw Pass is2 while Draw is1. Native fixture capture initially returned EndFrameCapture false without drawable/present; use the same tiny CAMetalLayer/present path as existing Metal fixtures. Failed attempts are not PASS.

`display-0..8.log` also PASS actual GPU heatmap readback: count1/2 use shared ramp RGB64,0,64 /64,0,192 (+/-1 byte); zero stencil rejection restores original background. `capture-9.log` /`display-9.log` PASS odd5x3 Native buckets1,6,0,8, resolve6 quads; all15 public overlay pixels count1 and correct ramp, twice. `capture-10.log` /`display-10.log` PASS parallel encoder Draw/Pass counts and heatmap twice; immediately followed by `ue-parallel-followup.log` PASS same UE3300 twice.

`metal-texture-view.HNwYRZ/` PASS prior numeric/display controls after shared display shader update: statistics, Range, gamma, alpha, subresources, NaN/Clipping; immediately followed by same UE3300 GBufferA resource12323 twice, original bytes unchanged.

## Same real UE and full-frame progress

Same c506da2e capture `local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc`. `ue.log`, `ue-display.log`, `ue-odd-followup.log` PASS EID3300 Draw/Pass twice:35840 positive pixels, maximum2, exact RGBA raw counts/per-quad uniformity, Pass >= Draw, original SceneColor/GBufferA/C/depth bytes and None restore unchanged. This is actual UE indirect drawing (6 vertices,560 instances), not a synthetic UI screenshot.

`images/terminal.log` PASS normal OpenCapture + two full EID0 reset replays; all three900x640 native presented readbacks retain accepted SHA256 `fc5f3afe33b3ec7a523d447d59d0b517337d4c8f8867f6d863f99f0ab5eb4f61`.

## Full and actual UI, recorded separately

No full suite was run on the isolated b2c8718b Quad/display candidate. The preceding bb793d8d full308/7786/3080 (growth9502720B/hash consistent/exit0) belongs to B454. After Triangle integration, current combined8e332844 candidate PASS one frozen full308/7786/3080 (growth5816320B/hash consistent/exit0); see [B456](BATCH456_VULKAN_TRIANGLE_SIZE.md) for current post-full UE and UI results. Directed checks returned to the same UE after each local step.

Actual assistant UI pending: current b2c8718b/6701c0cd GUI opened the same capture, but the subsequent actual window query reported Mac locked again before Quad Draw/Pass→None, legend and Pixel Context observations. The agent-owned GUI process95985 was stopped before serial Native/UE probes. No rendered Quad UI PASS is claimed. B454 Depth/Clear UI PASS is historical to bb793d8d. Do not count a combo label as rendered output.

```sh
bash util/buildscripts/scripts/test_metal_quad_overdraw_macos.sh \
  build-macos-debug/local-m2-descriptor-replay/testproj-20261003-092235-138208/replay.rdc 3300
```

GPU serial, qrenderdoc/UnrealEditor closed for terminal probes. Remaining includes Triangle Size/post-VS equivalent semantics, MSAA/layered/memoryless overlays and unsupported numeric texture cases. Quad support does not claim full Vulkan parity.
