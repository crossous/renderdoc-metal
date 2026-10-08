# B516：UE 帧内 R8Unorm 二维数组

B515后的真实Lumen CPU65预检拒绝12051/heap4063 R8Unorm 320×240、2DArray、arrayLength1、mip1、usage3。DX12 Texture2DArray和Vulkan arrayLayers均支持R8_UNORM，capture和initial contents按layer/subresource恢复；Metal已有slice数据与数组shader绑定，本批补等价帧内资源形态。

coverage65只接受Private/Tracked、≤512×512、depth/sample/mip1、1–8 array layers、RW usage3，保持options/swizzle/optimized及native placement footprint/heap alignment/birth/alias/lifetime证明。旧2D R8和R32Float数组分支不变。19组损坏文件覆盖零/超array层、mip/shape/format/usage/storage/coverage/heap及重复birth，API+CLI共38拒绝，无GPU wait开始或assertion/overrun/streamseek。

最终backend及bundle SHA256 `e6ecb2239cba0eba4459b3c820294855fb31e2df1a76288c8e2c3c8a55e6d389`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。证据 `build-macos-debug/metal-ray-b516/manifest.json`、`final-binary/librenderdoc.dylib`，详细各步日志和源码hash保留，无提交/推送。

运行 `python3 util/test/metal/metal_descriptor_frame_mip_gate.py --family r8array --work-dir build-macos-debug/metal-ray-b516/final-array`。确定性native/capture共享确切metallib，UE尺寸320×240×1、尺寸/层数边界512×512×8、非方形3×5×3，各连续两份不同heap offset捕获。每层先写byte64+8×slice，再覆盖192−8×slice；读取最后层输出64/120/80。逐层所有字节、首/中/尾及首/末slice PickPixel、两个写/中间只读Usage定位、GPU ID强制变化、四周期末事件→读事件→EID0共72选择、table归零、CLI3全部通过。

| 范围 | 实际结果 |
| --- | --- |
| 新3种R8数组 | 3native/6capture/API/CLI PASS；所有层独立值、72事件/EID0 |
| 19坏capture | API+CLI38拒绝 PASS |
| B514 R16六份及B515 packed二维六份 | 当前库API/CLI3 PASS，未重复其native/capture |
| 旧R8单层2D及R32Float数组 | 2native/4capture/API/CLI3 PASS |
| IR multi64 | 原B512capture当前库API/CLI3 PASS，非78组全量 |
| 官方MIT sample | pin/license核验、两scene native/capture/API270事件/CLI3及10尺寸查询 PASS；13官方坏capture未跑 |
| 能力/语法/diff | 6能力查询默认false/native判断 PASS，Python语法/diff PASS；B514的32CPU/AIR本批未重复 |
| UE | 原capture仅CPU65；越过12051，下一12052 RG11B10Float 2DArray320×240 array1 usage3拒绝，无GPU回放 |
| 78IR/75RT/308/旧84间接反例/Qt/ARC/UE最终图像事件 | NOT RUN / NOT VALIDATED，不继承旧库结果 |

日志严格扫描与hash起止一致记录manifest，构建/GPU串行、统一互斥，进程已终止。系统重启根因未修，公开RT能力仍false，AS内部/RT单步/RT Pixel History不扩展。下一真实packed浮点数组，再typed inline-query/AS header producer及实际UE输出验收。
