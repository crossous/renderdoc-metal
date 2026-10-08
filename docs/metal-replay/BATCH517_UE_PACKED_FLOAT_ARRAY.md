# B517：UE RG11B10Float 数组与 packed 浮点验收

同一真实Lumen capture CPU65预检的12052/heap4063 RG11B10Float 320×240 2DArray、array1、mip1、RW3是本批实际缺口。DX12 R11G11B10_FLOAT与Vulkan B10G11R11_UFLOAT_PACK32具备等价格式及layer/subresource capture/replay；Metal已有原生格式转换/读回，本批扩展经过证明的二维数组形态，没有实现新浮点转换器。

与B516数组共用Private/Tracked、宽高≤512、1–8层、depth/sample/mip1、RW3及原options/swizzle/native footprint/heap extent/alignment/birth/alias/lifetime守卫。只增加RG11B10Float，不扩大未知packed格式。公开两能力仍false。

最终backend和bundle SHA256 `3d8bf08244a81ff9808c9d20fcb95ffa6729155dc4a87c2f5af636a994c42cc1`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。证据 `build-macos-debug/metal-ray-b517/manifest.json`、最终库`final-binary/librenderdoc.dylib`。

`python3 util/test/metal/metal_descriptor_frame_mip_gate.py --family r11array --work-dir build-macos-debug/metal-ray-b517/final-array` 使用同一确切metallib完成native→capture→replay。UE320×240×1、512×512×2、256×128×8、3×5×3四种shape，各两份不同heap offset捕获。red先(slice+1)/8，再(8−slice)/8；G/B先0.5/0.75，再0.25/0.5。每slice的所有32-bit原始11/11/10编码与已知精确golden相等，PickPixel首/中/尾及首/末slice的float与隐含alpha1相等，最后层native输出32/64/256/96以及G128/B192一致。四周期末事件→读事件→EID0、两写与中间只读usage、table归零及不同native GPU ID均通过，不只核验非黑画面。

| 范围 | 实际结果 |
| --- | --- |
| 新packed float数组 | 4native/8capture/API/CLI3 PASS，96事件/EID0，全部packed bits与PickPixel |
| 19坏capture | API+CLI38拒绝 PASS，包括array0/9、shape/mip/format/usage/storage/heap/oldcoverage/重复birth |
| 原R16 mip/packed二维/R8数组 | 每类原6capture，当前库API/CLI3 PASS，未重复其native/capture |
| 旧RG11二维 | 1native/2capture/API/CLI3 PASS，packed helper兼容普通二维颜色 |
| IR multi64 | B512原capture当前库API/CLI3 PASS，非78组全量 |
| 官方MIT sample | pinned archive/license、两scene native/capture/API270事件/CLI3及10尺寸查询 PASS；官方反例未跑 |
| 能力/语法/diff | 6能力默认false/native判断及Python语法/diff PASS；B51432CPU/AIR没有转记本批 |
| 实际UE | 原capture只CPU65；越过12052–57，下一12058 RGBA16Float 320×240 2DArray array1 RW3拒绝；无GPU回放 |
| 完整78IR/75RT/308/旧84间接反例、Qt/ARC、UE最终输出和事件 | NOT RUN / NOT VALIDATED，不继承旧库证据 |

最终日志数、严格诊断扫描与完整source/binary hashes见manifest，构建/GPU串行互斥，所有进程已终止。`UE-next-textures.json`来自原XML的streaming CPU读取，只记录实际字段，不当成支持证明。没有新UE截帧；原capture哈希保持B513401db9a1…；系统重启根因仍未修。下一half二维数组的现有64尺寸边界与typed inline-query/AS header/producer闭包，最终仍需真实UE输出/重放/EID0及相关最终库回归才能启用能力。
