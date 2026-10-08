# B518：实际 UE half 浮点数组尺寸

同一Lumen capture的CPU65预检在12058/heap4063 RGBA16Float 320×240二维数组array1、RW3处拒绝。DX12/Vulkan已经支持对应RGBA16_FLOAT二维数组、初态subresource及重放。Metal此前已经支持half数组，但帧内数组硬限64，本批只将coverage65且usage恰RW3的RGBA16Float二维数组尺寸上界扩至512。array1–8、单mip/sample、Private/Tracked/native heap footprint/alias/lifecycle等守卫保持。旧coverage64和RT7仍走64尺寸原范围，未扩展大型RT用途。

最终backend/bundle SHA256 `5f1a2f18e78e4f625d7345f6e59b167237fd9994f16e7a3cb598c8c25653189c`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。证据 `build-macos-debug/metal-ray-b518/manifest.json`，最终库`final-binary/librenderdoc.dylib`，无提交推送。

运行 `python3 util/test/metal/metal_descriptor_frame_mip_gate.py --family halfarray --work-dir build-macos-debug/metal-ray-b518/final-array`。新shape为UE320×240×1、512×512×2、256×128×8、3×5×3，各连续两capture；另小数组RT7、coverage64的真实native/capture两份控制。总5native、10capture/API/CLI3 PASS。不同slice red从(slice+1)/8覆盖为(8−slice)/8，G/B先0.5/0.75再0.25/0.5，alpha1；所有half raw bits、PickPixel首/中/尾和首/末slice、native最后层值、两写/中间只读usage、不同GPU ID、四周期末事件→读事件→EID0共120选择通过，table归零。通过label明确选择新fixture颜色模式，旧half颜色oracle保持。19坏capture共38次API/CLI拒绝，包含large RT7拒绝。

| 范围 | 实际结果 |
| --- | --- |
| 新half数组及legacy控制 | 5native/10capture/API/CLI3 PASS，120事件/EID0；旧coverage64+RT7两份通过 |
| 19坏capture | API+CLI38正确拒绝，無assertion/overrun/streamseek/GPU wait开始 |
| B514–B517原capture | R16 mip6、packed二维6、R8数组6、R11数组8，当前库API/CLI3 PASS；未重跑其native/capture |
| IR multi64 | 原B512capture当前库API/CLI3 PASS，非78组全量 |
| 官方MIT sample | pin/license核验、两scene native/capture/API270事件/CLI3、10尺寸查询 PASS；官方坏capture未跑 |
| 六能力/语法/diff | PASS；32CPU/AIR为B514旧库结果，本批未重复 |
| 真实UE | 只CPU65，不修改原capture；下一错误为frame texture view，未做UE GPU回放 |
| 完整78IR/75RT/308、旧84间接反例、Qt/ARC、UE图像/事件 | NOT RUN / NOT VALIDATED，不继承旧库证据 |

`UE-views.json`从原XML流读取，10个后续view确切source11841 R16Float256×128 mip8，level0两份、level1–7单mip、level3两份，slice0/1、identity swizzle。它说明实际需要的view范围；CPU错误目前仍是generic消息，不能仅凭此列表称该branch的具体失败ID已证明。下一用精确诊断和原生→捕获→重放view fixture补逻辑mip范围、父子生命周期/usage与EID0，之后typed inline-query AS header/root/producer及实际UE GPU输出验收。

日志严格扫描计数、hash起止/源码hash在manifest；构建/GPU串行互斥，进程已终止。公开supportsRaytracing和supportsRaytracingFromRender仍false，RT黑盒边界保持；系统重启、原Qt崩溃根因未修。持续任务未完成。
