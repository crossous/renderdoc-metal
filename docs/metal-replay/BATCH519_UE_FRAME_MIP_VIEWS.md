# B519：UE 帧内 R16 单 mip views

参照仓库 DX12 SRV/UAV 的 MostDetailedMip/MipSlice 和 Vulkan image view subresourceRange，复用 Metal native view 和既有父资源 usage 折叠。实际 UE XML 的 source11841（256×128、8mip R16Float）有10个单mip view，base0–7。本批只允许 coverage65、已验证帧内R16二维父资源、同format/type、identity swizzle、单mip/slice0–1，无嵌套；保存base并投影width/height/mip count。预检同时验证父资源已出生和投影metadata一致；不创建额外heap分配。

最终backend/bundle SHA256 `3d2452ecd9295c1e8770281d34d2def0a5f51b26c68960c5e5925415bbd79407`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。最终库和manifest在 `build-macos-debug/metal-ray-b519/`；无提交推送。

运行 `python3 util/test/metal/metal_descriptor_frame_mip_gate.py --family mipviews --work-dir build-macos-debug/metal-ray-b519/verified-views`。256×128父8mip的base0/1/3/7、512²父10mip的base9，总5native/10capture/API/CLI3 PASS。每份四周期末写→读→EID0，共120选择；view逐像素rawhalf、四分量PickPixel与父物理mip逐字节一致；逻辑尺寸、父usage两写/中间只读、不同GPU identity、table归零通过。没有初始化其他物理mip，因此没有邻mip独立性或纹理零值结论。

| 范围 | 实际结果 |
| --- | --- |
| 五组新单mip views | 5native/10capture/API/CLI3，120选择 PASS |
| 29损坏组 | 58 API/CLI拒绝；base溢出/范围/count、parent出生/ID、type/format/swizzle、duplicate、自引用及18旧父texture反例 |
| B514–518 texture | 原6+6+6+8+10=36 capture在新库API/CLI3 PASS，未重跑native |
| 旧RGBA8/R16单mip view | 2native/4capture/API/CLI3 PASS |
| IR multi64 | B512原capture当前库API/CLI3 PASS，非78组全量 |
| 官方Apple MIT sample | pin/license核验，两scene native/capture/API270事件/CLI3、10size queries PASS；官方13坏capture未跑 |
| 原生/默认/探针六能力 | PASS，两公开RT开关仍false |
| 原UE CPU65诊断 | 拒绝静态分配预算；没有加载或GPU重放，不计UE PASS |
| 完整78IR/75RT/308、旧84坏间接、32CPU/AIR、Qt/ARC、UE输出/seek | 本新库 NOT RUN / NOT VALIDATED |

`final-views/`保留第一次helper编译失败：rdcfixedarray不能直接memcmp，已改四分量逐项比较；生产库未受影响。`verified-views/`与`regressions/`所有完成日志严格扫描计数见manifest，无断言/overrun/非法seek/资源表诊断，构建和GPU串行互斥，所有子进程终止。

UE同一原capture CPU诊断已越过view metadata，下一offset827776 MTLDevice::newHeapWithDescriptor触发分配guard。累计native4919197952、initial4362319907（预检还按两份初态副本计算）、snapshots24864323；推荐12713115648。当前库保留GPU/总内存预算，不启动该大帧GPU回放，避免重复危险运行。下一核验实际heap footprint和低成本可用UE场景/设置，并补typed inline-query AS header/root/producer。不能通过增大预算或修改capture coverage启用RT。用户系统重启及原Qt/AS崩溃根因未闭环，持续任务未完成。
