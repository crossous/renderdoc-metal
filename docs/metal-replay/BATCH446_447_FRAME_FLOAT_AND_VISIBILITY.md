# B446–B447: new UE frame-created float texture and visibility buffer

2026-10-03. User unlocked the Mac and resumed work. Changes remain local; no commit or push.

The original new Testproj capture is preserved at
`build-macos-debug/local-m2-descriptor-replay/testproj-20261003-085846-931499/original.rdc`,
SHA256 `02919da2a0494c79dd27b8a126086fbf3db85036fdd85560970611b70ed321d3`.
It contains actual UE GBufferA/B/C/D label chunks. The earlier accepted UE capture and its
native images remain the baseline, rather than being replaced by this new capture.

## First failure: 2D float creation bound

Chunk28270 creates texture14001 in placement heap2094: RGBA16Float160×120, single mip,
sample/depth/array1, Private/Tracked, ShaderRead|ShaderWrite|RenderTarget. The previous
frame float validation rejected dimensions over64. The other21 frame texture creations
passed this particular descriptor check. A native160×120 fixture reproduced the rejection.

At coverage65, 2D R32Float/RGBA16Float/RGBA32Float now use the existing512 dimension bound
of other frame colour targets. Older coverage, array/volume bounds, formats, usages,
native heap size/alignment/range/alias checks and allocation budget are unchanged.
Failure diagnostics now identify the texture/heap and descriptor fields.

Official RenderDoc sources in local `origin/v1.x` were inspected: D3D12
`d3d12_device_rescreate_wrap.cpp::Device_CreatePlacedResource` retains descriptor and
heap offset; Vulkan `vk_resource_funcs.cpp::Serialise_vkCreateImage` retains CreateInfo
and native memory requirements. Replay uses the original Metal texture allocation;
the fix does not resize textures or substitute pixels.

## Next failure: frame-created query result identity and CPU initialization

The next pre-submit rejection was Counting mode on encoder14134. Its pass refers to
frame-created Shared/Tracked heap22 buffer14133, length262144, offset23259392. Ordinary
pointer deserialization returned null before native frame creation and lost this reference
during preflight. RenderPassDescriptor now keeps the logical ResourceId using the same
existing serialized `visibilityResultBuffer` field, following texture attachment identity
handling. No new field is appended to the capture stream.

Frame query results require a live, unretired Shared/Tracked buffer8B..1MiB and a complete
captured CPU snapshot no later than the query command's commit. Initialization follows
captured commit order, since pass encoding can precede the separate earlier submission
that owns the CPU snapshot. For this UE buffer the full262144-byte snapshot belongs to
command14087, before query command14090 commits; its first seven64-bit values are zero.
Later CPU updates are excluded from this initialization proof. Background query buffer
initial contents checks remain. Typed tables/GPU descriptor writes and previously modified
buffers are excluded. Native result mode, offset alignment/range, encoder ownership,
write tracking and existing snapshot restoration are retained.

Official D3D12 BeginQuery/ResolveQueryData and Vulkan CmdBeginQuery/CmdCopyQueryPoolResults
were compared. They retain native query execution and result resource identity. Metal
reuses its existing native setVisibilityResultMode implementation; no CPU query simulation
or skipped query is introduced.

## Verification record

| Category | Result |
| --- | --- |
| Directed float | Original native fixture PASS; pre-fix replay reproduced creation rejection. Fixed160×120 RGBA16Float full texels/PickPixel/GPU ID/zero-at-EID0 PASS across4 reset cycles. Ten descriptor/legacy64/heap/identity mutations refused before GPU waits. |
| Immediate same UE after float | EID3371 twice PASS;6 proven inputs and3 sorted unique GBufferA usages remain; native output bytes identical. `ue-user-comparison/frame-float-fix/`. |
| Directed query | Final ff444e68 standalone/placement/parallel/earlier-submission initialization:8 captures,32 seeks, Native count4 and sentinels PASS;40 API+CLI negative groups reject before GPU wait. `metal-frame-visibility.NHbdd5/`. The earlier-submission case exposed a partial replay metadata ownership defect, fixed in B448. |
| Immediate same UE after query | EID3371 twice PASS; same inputs/usages/output. `ue-user-comparison/frame-query-fix/`. |
| New real UE | Pre-submit and first GPU open passed after these fixes. EID0 reset then exposed an unrelated-slot overlay defect; B448 fixes it. Three900×640 Native images are identical and regenerate the original JPEG exactly. Final ff444e68 EID3300 lighting/4057 five MRT/50 blit pass two resets each. |
| Full regression | Fixed ff444e68 PASS:308 captures,7786 malformed cases,3080 lifecycle opens, resident growth5488640B; start/end library hashes identical. Prior dafb37a9 full PASS is historical. |
| Assistant UI | B445 unlocked checks passed compact headers/pass captions, target list, native `.metallib` Save, resource folding, graphics↔compute and OM. Actual new UE named inputs, source MSL and final popup checks pending. |

Final combined candidate library `ff444e680c0786a3b682ecee4ad696f9db9df35980b7fb56cd9a6915ca7f8b3b`.
Directed and real UE results remain separate. Existing stream-reader/InitialContentsList
diagnostics are not claimed resolved. The task is still in progress.

2026-10-03 final unlocked UI acceptance (supersedes the locked-Mac pending status above):
actual qrenderdoc DLL remains ff444e68 and executable8936b361. New UE normal open passes;
EID3300 Fragment Shader displays GBufferA/B/C, ScreenSpaceAO, SceneDepthZ and FWhiteTexture.
GBufferA displays scene normals and the follow tab reads `Cur Input Descriptor[1934] - GBufferA`.
The timeline has read/clear/write markers. Native Escape closes the actual usage popup;
End+Return selects its last ColorTarget entry and the actual event changes to4057 without
replay error. A physical mouse click in another panel dismisses the menu; process samples
confirm RDDialog::show is on-stack before and absent afterwards. AX tab activation alone
bypasses normal mouse handling and is not an outside-click test. CUA's main-window capture
omits the separate popup, so no new direct popup screenshot is claimed.

T01 EID5 Function14 opens the common Shader Viewer with `Captured MSL` and actual
vs_main/fs_main source. The same UE is then normally reopened and its Specialized Function4722
shows the real DeferredLightPixelMain AIR under `Metal AIR (Apple toolchain)`. Current UI is
left on EID3300 Fragment Shader for review, with the AIR tab available. No GPU CLI tests were
repeated and no source change invalidates the directed/full/after-full evidence. These are
assistant UI checks, not user visual acceptance. Evidence:
`build-macos-debug/local-m2-descriptor-replay/ue-user-comparison/ui-final-20261003/`.
The current supported capture and inspection workflow passes; this does not claim arbitrary
Metal feature parity, exhaustive dynamic bindless GPU feedback, or recovery of names/source
absent from older captures. No commit or push.
