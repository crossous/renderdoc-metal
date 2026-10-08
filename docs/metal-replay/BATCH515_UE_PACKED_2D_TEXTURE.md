# B515：真实 UE RGB10A2 二维读写纹理

B514修后同一B513真实Lumen capture的CPU65预检推进到12050/heap4063 RGB10A2Unorm 320×240、2D、mip1、usage3。参照DX12 R10G10B10A2 resource和Vulkan A2B10G10R10 image正常capture/initial subresource/replay路径，Metal已有packed格式上传/读回/像素解码；本批只补帧内形态许可，不再造格式转换。

coverage65、Private/Tracked placement、宽高≤512、depth/array/sample/mip均1、usage恰RW3、资源options/swizzle/optimized/nonnzero shape及native extent/heap alignment/birth/alias/lifecycle均验证。旧3D packed分支和旧coverage不变；二维RT usage7本批未验，明确拒绝。

最终backend和bundle SHA256 `d8f74eec2a15a13afd3d5ddae0255570ac8babc1a5fb1599c0810115921a75cd`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。证据 `build-macos-debug/metal-ray-b515/manifest.json`、`final-binary/librenderdoc.dylib`，全程构建/GPU串行、统一互斥，进程已终止；无提交推送。

验证命令：`python3 util/test/metal/metal_descriptor_frame_mip_gate.py --family packed2d --work-dir build-macos-debug/metal-ray-b515/final-packed`。精确MSL→metallib用于native和capture，三种size为UE320×240、上界512×512、非方形3×5；同heap不同offset连续两capture，write/read/overwrite两个相反颜色，完整packed bits（256/512/767及767/256/512、alpha3），每像素与PickPixel边角/中点核验。六份API四周期末事件→读事件→EID0共72选择通过，usage两写和中间只读区分；descriptor EID0归零，GPU ID与原capture不同，CLI各3loops通过。18组坏capture由API和CLI共36次拒绝，包括单mip、形状、usage、范围、unknown格式、Shared/untracked、oldcoverage64、heap offset及重复birth。

| 范围 | 实际结果 |
| --- | --- |
| packed二维3种size | 3native/6capture/API/CLI PASS；72事件/EID0，全像素/PickPixel/usage/ID |
| 18坏capture | API4+CLI1共36次正确拒绝，无assertion/overrun/stream seek诊断 |
| B514 R16 mip8/9/10 | 原6capture在当前d8库API/CLI3 PASS，未重跑这六份native/capture |
| 旧packed3D | 1native/2capture/API/CLI3 PASS，coverage44保持 |
| IR多几何64 | 已有B512capture当前库API/CLI3 PASS，非78组全量 |
| 官方MIT Apple sample | pinned zip/license核验、两scene native/capture/API270事件/CLI3及10尺寸查询 PASS；官方坏capture未跑 |
| 能力/Python/diff | 6能力默认false/native判断 PASS，gate语法/diff PASS；32CPU/AIR为B514证据，本批未重复 |
| 实际UE | 原capture不变，CPU65越过12050；下一12051/heap4063 R8Unorm 2DArray320×240、arrayLength1、usage3拒绝；无GPU回放 |
| 旧84间接反例/完整78IR/75RT/308、UE最终图像、原Qt/ARC | NOT RUN / NOT VALIDATED；不继承B514 cc91的回归到当前库 |

最终日志扫描计数/源码和binary hashes保留manifest。新GPU捕获是独立确定性fixture，不称UE成功。未修系统冻结根因，不启用supportsRaytracing或supportsRaytracingFromRender。下一实际R8二维数组形态，随后typed inline-query AS header/root/producer闭包和UE输出/事件/EID0验收；RT黑盒边界保持。
