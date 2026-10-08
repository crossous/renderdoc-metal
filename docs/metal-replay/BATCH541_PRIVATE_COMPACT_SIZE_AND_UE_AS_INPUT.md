# B541 — Private placement 输入、压缩尺寸与真实 UE BLAS 引用

2026-10-07，PHASE59。完成 Private compact-size encoder-end 快照及独立 query submission 的构建来源校验；修复 capture 压缩拷贝校验失败时直接跳过原生调用的问题。生产 `supportsRaytracing` / `supportsRaytracingFromRender` 均 false；持续任务 active。未提交/推送/重置，保留用户 RDHeaderView 改动、安装引擎及原工程。

## 对照与实际根因

先读本仓库 Vulkan `VulkanAccelerationStructureManager::CopyInputBuffers`（vk_acceleration_structure.cpp:186）、DX12 `BuildRaytracingAccelerationStructure`（d3d12_command_list4_wrap.cpp:1131）及 Metal 初态重建。沿用已知对象、冻结 build/query 输入与本机容量核验；保持 AS 黑盒边界，没有 AS 内部查看、RT shader 单步或 RT Pixel History。

B540 旧 UE build 的 descriptorBytes=0 并不证明 placement copy 未实现。当前源码已允许 tracked Private placement；本批先用相同 589824-byte 输入、1376256-byte scratch、options544、offset0/stride72、3实例验证，原 bfeca02b 库即通过。加入诊断后的 UE 首轮 f160792b 真正冻结了216字节，但 kind11/no-child 候选实际引用 GPU ID67/71、mask37/options6，不能合法当作全 masked/null packet。日志同时发现 native compact copy 多次因不能读取 Private size output 而提前返回，目标 BLAS 没有有效 m_LastBuildKind。首次 UE 与最终 UE 均有限运行并 owned cleanup -9；这不是 UE 正常退出。

## 实现

- 间接 AS encoder-end 快照现在补充已知捕获对象、native owner 一致和显式退休检查；placement 从创建起的 native isAliasable 不等于退休。保留 tracked heap、范围、scratch/AS 写入排斥及原64MiB快照边界。
- typed UInt/ULong compact-size 写入 Private standalone/placement 后，在同一 AS encoder 结束点插入4/8-byte Shared staging copy。持有 source/staging 到 native submission 完成；不等待任意应用回调，不读 Private CPU contents。相同 buffer/heap 中不同 AS 的重叠 query 范围不授予冻结值；新 query 清旧证据，copy metadata 清目标旧证据。
- 捕获 compact copy 只消费已完成、无error的冻结 query value，核验源 build、query build 来源、目标容量及原限制。独立 query submission 合法，记录 query 观察到的 build command，替代此前强制 query 与 build 来自同一 command 的限制。
- 捕获校验无法形成 typed recipe 时仍转发已包装原生 compact 操作，并记录 expectedSize=0 的显式无效 replay chunk；不再静默丢掉应用命令，也不把未知源伪造为有效 build metadata。重放继续拒绝无证据的记录。新增 Private size 快照是 capture 支持；本批未宣称任意帧内 Private size-query/compact chunk 重放支持，原 Runtime Private size read 仍有边界。
- fixture/gate 新增3实例、与UE相同的 placement 输入/scratch及72-byte布局，真实GPU上传、build后擦零；另测空初态→3实例、帧内真正零实例、Private placement size-offset256 compact BLAS→3/64实例 query。新 helper 明确区分 count0/空bytes 合法配方与缺快照，不用空bytes充当非零 build。
- 新增 placement options/type/range反例；捕获 heap hazard 改为 untracked 后，已有 replay allocation 会增强为 tracked，因此列为合法控制并用强API/CLI复验，没有为了让反例通过新增拒绝。

## 最终固定库验收

产物 `build-macos-debug/metal-ray-b541/final/`，入口 variants.py、negative.py、acceptance.py、fresh-compact.py、UE-proof.py、freeze.py；所有 build/GPU job 串行共用 IR lock，finite timeouts、固定输入/帧数、无UE/qrenderdoc并行。

| 范围 | 实际结果 |
| --- | --- |
| 六新 native/capture/强API/CLI | PASS，192事件选择/EID0；placement3=2292/2292/0/0，placement64=3329/3329/0/0，empty-initial→3=2292，empty-frame=0/0/0/0，Private compact3/64分别2292/3329；每份AS输入构建后589824bytes全零，Header/descriptor/贡献/usage验证 |
| 四新捕获旧 static heap-query | PASS，64事件/EID0；强API及CLI3 loops；与六新场景共10query captures/256事件 |
| 损坏捕获/合法控制 | PASS，两个源各84坏组/168API4+CLI1无GPUwait拒绝、各3合法强API/CLI；总168坏组336拒绝、6合法控制 |
| 旧回归 | PASS，56 checks（含 helper builds）：B536格式、旧16格式/large R32、fresh163初态纹理2capture12696subresources、八B526query、B512 multi64 TraceRay、四旧compact API/CLI、六能力入口及官方两scene270事件10query/CLI3 |
| Shared compact捕获回归 | PASS，另fresh四mode native/capture/API/CLI（background-compact、indexed-compact-frame、tlas-indexed-child-compact、compact-copy），18 checks含两build，未借旧capture代表新捕获 |
| controller生命周期 | PASS，compact64和空frame各10 controller open/seek-reset/readback/close，resident growth3506176bytes，低于64MiB界限 |
| 最终日志 | PASS，1436完成日志无严格assert/overrun/streamseek/未知类型/resource-map关闭诊断；中间失败独立保留不计通过 |
| UE重放预检 | normal/CPU65/pre-submit65均API4且无GPUwait/初态上传/frameGPU，仍缺实际heap-query producer/consumer namespace，拒绝符合当前边界 |
| full78IR/75RT/308/frame-family/Qt-ARC/官方坏13 | NOT RUN；不能继承旧库PASS；历史GUI崩溃/系统重启根因未由本批证明 |

官方 MIT Apple sample 来源： https://docs-assets.developer.apple.com/published/ade36d76f1bb/AcceleratingRayTracingUsingMetal.zip ，ZIP SHA256 `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`，复用已审查cache，当前库重新构建/运行两scene；版本、构建方法、实际结果见 regressions/official/gate-results/manifest.json。

## 最终 UE 证据与剩余缺口

使用 UE5.8.3CL58210709、B527隔离3mesh/2light工程、B537 provider，命令开启 Lumen HardwareRayTracing/Inline 并校验实际cvar来源。`UE-capture/manifest.json`、`UE-build-proof.json`、`UE-capture/AIR-proof/manifest.json` 保存来源及完整结果。

- 新 capture SHA256 `716106d898f57b7deb9bce4d0b0392638f96b0b4a70d3ddd0c0a3cd6686e715d`。3 typed TLAS builds、96direct+56indirectcompute；60 AIR条目均disassembled，两个实际绑定query shader的间接group为[2,7,1]与[132,1,1]，明确是硬件 ray-query 调用，未验证ray结果/输出。
- 主TLAS2560输入2995确实冻结216字节（3×72），两个已知child7097/7101、GPU ID67/71。两个空TLAS仍count0/bytes0/children0，符合实际无输入读取。三套输入Private/Tracked placement589824、scratch1376256，原native compact copy错误日志消失。
- 两个child的初态 build recipe均缺失：当前几何输入快照/初态路径对堆内 BLAS 仍有限制。捕获层已从空packet/no-child推进到真实引用，不能据此宣称UE离线重建/输出通过。
- 61heap合1419217920bytes，初态blob1110926467bytes；由于预检更早的动态header/query namespace拒绝，本批不声称总GPU预算门槛已通过。
- 两生产flag仍false；不为UE运行注入sample专用16-byte根参数契约。接下来B542补实际堆内BLAS几何/索引冻结、compact子配方与生命周期，再实际Lumen PSO/root、间接参数、descriptor GPU producer-consumer闭包及UEGPU重放/输出。

## 中间失败

`first-placement3`：fixture ARC heap引用所有权编译失败，明确__strong修复；`first-failed-backend-build.log`：-Werror逻辑括号，修复；final/first-failed-placement3：helper误用SDObject.children，改NumChildren/GetChild；final/first-failed-placement-compact3：独立query command被旧build/query同提交条件拒绝，native转发已有真实输出但capture没有配方，修后完整native/capture/API/CLI复验；final/first-failed-negative-template.log：错误模板路径，修正；final/first-failed-placement3-bad：错误把replay增强tracking后的合法heap当坏捕获，曾发生GPUwait，不计无GPUwait拒绝，改两个合法控制完整复验。before-private-size-fix与before-query-version-fix结果不是最终库PASS。

## 哈希

backend/bundle/final冻结库： `f6a8029d18b6e8c0e6dbe576c1faa8eb6b84d776ca830ffee90dd161a1c5c97c`。
GUI： `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。
provider复用未重建： `aba8b52b92e43ffd7b64e146874db4d1de3b2a7d1ebd66b113dff089f507b1d1`。
源码、工具、manifest及二进制路径/哈希见 final-manifest.json；起止backend一致。目标未完成，持续active。
