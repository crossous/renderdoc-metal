# B538 — 动态 AS header 版本、Private 贡献源与截帧初态刷新

2026-10-06，PHASE59。本批实现和有界 sample 验收完成；**实际 UE 帧内新建 header / kind3 descriptor / query consumer 闭包未完成，两项生产 RT capability 仍 false**。持续任务继续；没有提交、推送、重置仓库或修改安装引擎/原用户工程。

## 实现与边界

先核对仓库 Vulkan `Serialise_vkCreateAccelerationStructureKHR`、`Serialise_vkCmdBuildAccelerationStructuresKHR` 的资源依赖、已知 AS 对象和 build/事件处理，以及 DX12 `D3D12RTManager::PatchRayDispatch` 的 descriptor/SBT 原生地址重定位。Metal 公开 64-byte header 用 ResourceId、offset、捕获身份和重放对象重建；不读取 AS 内部，也不提供 RT shader 单步或 RT Pixel History。

- Background header 声明保存最新事实；每次 StartFrameCapture 将其冻结到现有 descriptor-history snapshot，记录 Header/AS/contribution 读依赖，清理已释放来源。停止将背景声明累积到不可变资源创建 record。同一 header 在背景改变后再截一帧，获得新初态；总 live key 上限128，旧 chunk 布局不改。
- Coverage3 允许已知初态 header 的帧内显式写入。metadata 按原顺序记录有限版本；预检校验 AS/贡献对象、snapshot、offset 和当前版本，拒绝未声明改写、alias、未提交 reader 前发布以及 Header 的 GPU 改写。运行时等待已提交 reader 完成，在原写入点重新 patch AS ID 和贡献 GPUVA；帧内 write 事件可读回。
- EID0 恢复初始 header map/raw bytes/cursor；帧内版本与 current AS recipe 分开恢复。预检 scope 在所有返回路径恢复初始 raw/header 状态，不让预检的最终版本污染 loading。
- Header 可以是 Shared placement buffer；Header 与 contribution 的整个物理 backing 均 ≤16KiB、Header 8-byte 对齐/64-byte 可读，贡献4-byte 对齐。Contribution 可以是 Shared 或 Private，但仍要求完整已保存初态、已知 captured GPUVA、范围覆盖实际实例数。帧内 GPU 写入这些贡献源明确拒绝；不声称已支持 UE 的任意动态 contribution producer。
- 帧内最多128条显式 header write。只允许有已知初态 key 的 coverage3 query，不能借此接受实际 UE 的帧内新建 Header/kind3 consumer 或一般 heap-query dispatch。

Query AIR 只读取 header 中的 AS ID；本批验证贡献地址重定位、整 backing 字节及来源，不声称 shader 实际消费贡献数组。

## 最终库上的 sample 验收

产物根 `build-macos-debug/metal-ray-b538/final/`，总结果 `final-manifest.json`，串行入口 `variants.py`、`accept.py`、`regressions.py`、`preflight.py`、`lifecycle.py`。构建和所有 GPU 工作使用共同 IR 测试锁；有限超时、进程组清理、日志保留，不与 UE/qrenderdoc 测试并行。

| 场景 | Header / contribution offset | 实例 | 实际结果 |
|---|---:|---:|---|
| dynamic-shared | 32 / 32→48 | 1 | native/capture/API/CLI PASS |
| private-contribution | 4096 / 4096 | 1 | native/capture/API/CLI PASS |
| placement-header | 32 / 32 | 1 | native/capture/API/CLI PASS |
| dynamic-private | 4096 / 4096→4112 | 1 | native/capture/API/CLI PASS |
| dynamic-placement-private64 | 16304 / 4096→4112 | 64 | native/capture/API/CLI PASS |
| reused 同 Header 两次截帧 | 4096 / 4096→4112 | 1 | 两份 capture 的 API/CLI PASS |
| annotation-controls | 4096 / 4096 | 1 | native/capture/API/CLI PASS，12错误声明拒绝 |

七场景、八份 capture，**208次事件选择/EID0**。四 uint 精确 hit/hit/miss/short 结果、实际 query AIR/非零 dispatch、AS ID/VA 变化、贡献指针 +16、整贡献 backing、保留字/边界 padding、前后 AS/输出 usage 都验证通过。动态64实例最后命中3329；单实例帧前2241→帧后2258。旧64多几何由相关旧回归覆盖，不把本动态64实例场景称为64几何。

同 Header 连续 capture 使用第二个新 drawable；背景先刷新回旧 AS，再帧内发布新 AS。第二 capture 的新 AS 已有前一帧构建初态，强 oracle 同时验证两份 capture 的独立输出与32次事件选择。各 manifest 保存 UE DXC/converter 版本、runtime header、DXIL/metallib、sample/helper 源码、capture/binaries/backend 哈希。

| 损坏 capture 集 | 坏组 | API4 / CLI1 拒绝 | 合法控制 |
|---|---:|---:|---:|
| static-bad | 67 | 134 | 0 |
| dynamic64-bad | 128 | 256 | 3 |
| 合计 | **195** | **390** | **3** |

包括旧 query/root/header/grid、typed AS/build/Private 输入反例；新增 Header write 缺失、提前/推后、错误资源/offset/snapshot/保留字、alias、缺首 commit、贡献区 GPU 改写。全部拒绝无 `Metal replay wait begin`，无严格断言/stream seek/overrun/map 诊断。三合法控制包括重复的相同 frame header write，API 强事件 oracle 与 CLI loops3 通过。

47项相关旧回归 PASS：B536六个17-format buffer-view API、旧16-format两 capture API/CLI、旧large R32三个 API/CLI、fresh 163初态纹理 native/两 capture/API/CLI（12696 subresource）、八份 B526旧 query API/CLI、B512 multi64 TraceRay API/CLI、六项 capability、官方 Apple 两 scene/270事件/10尺寸查询/CLI loops3。

官方 MIT sample 沿用已审查 pin：
https://docs-assets.developer.apple.com/published/ade36d76f1bb/AcceleratingRayTracingUsingMetal.zip ，ZIP SHA256 `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`。本轮构建方法及实际 metallib/output/artifact hashes 见 `regressions/official/gate-results/manifest.json`；不把过去的库结果当成本库证据。

新增同进程两份动态 query capture ×10轮 controller open/seek-reset/readback/close 生命周期测试 PASS，resident growth **425984 bytes**，保留64MiB检查上限； dedicated strong query oracle 负责精确值，生命周期工具负责重复重放、读回和释放。最终范围 **1469份日志** 无严格诊断。完整78IR、75RT、308、全量旧 frame-family、Qt/ARC、官方13坏 capture **未运行**。

## 新的实际 UE Lumen capture

`final/UE-capture/original.rdc`：**66,705,869 bytes**，SHA256 `c41abb060d280b3c197d58faa0a8eb4665b04156b6a2a38fe1e36e070513e574`。隔离 UE5.8.3、16MiB provider、小场景/阴影/Lumen缓存，实际 cvar 与命令见 `UE-capture/manifest.json`；安装引擎、原用户工程保持不变。保存 capture 后 owned editor cleanup -9，launcher0；不计正常UE自行退出。

- 3 indirect TLAS build、93 direct+56 indirect compute、7 heap AS/identity；60原metallib AIR全部审查、0 unvalidated。实际两个绑定 ray-query AIR 的非零 indirect 调度：Main_0000a274_46403057 `[2,8,1]`（chunk30918）与 Main_0000c358_ed865861 `[132,1,1]`（chunk32802）。未证明各 shader 的实际分支命中或最终 ray 输出。
- 6条 Header facts、26条 kind3 factory bindings。六 snapshot 的 AS ID、贡献 GPUVA+offset及保留字匹配；三条背景 Header 的64字节声明还逐一匹配实际 Initial Contents，验证新 StartCapture snapshot 路径。所有 Header 为 Shared placement 64-byte buffer，offset0。
- 三条 frame Header **在帧内新建**（13230/13233/13234），对应 AS2559/2563/2566、contribution3091/3097/3104。它们不是本批 coverage3 的同一初态 Header 改写；证据见 `UE-header-proof.json`。kind3 当前 slot 及具体 query consumer 的完整重放链接仍未验收。
- 65 heaps 共1,488,096,256 bytes，初态blob1,122,325,471 bytes；由于 header guard 先拒绝，不宣称总预算通过。
- normal-open、CPU65、pre-submit65均按预期 API4 且无 GPU wait/初态上传/frame GPU。65诊断明确拒绝：`Dynamic Metal AS header needs typed initial query coverage3; UE heap-query producer/consumer closure is not yet supported`。UE 完整输出、重放、事件/EID0与绑定闭包仍未通过。

## 失败保留与最终哈希

保留并修正：background map 裁剪的 dangling-else 编译失败；placement fixture heap 小于4096导致原生重放创建拒绝（fixture改为≥4096/Tracked）；重复截帧 fixture 将 void StartFrameCapture 当bool的编译错误，以及重用已 present drawable 导致第二 capture 丢弃（改用新drawable）。动态64旧 `new-target-before-build` mutation 在共用 Header 时变成无改动的合法 capture，保留该 oracle FAIL，改为真正提前发布新版本；placement backing 反例只查 standalone creation 导致 StopIteration，增加 heap creation 分支后完整复验。新生命周期工具首编译失败来自 ARC 不适用 metal-cpp及误用 BufferDescription.name；改用既有非ARC构建和声明中的 output ResourceId 后复验通过。以上失败不计通过；最终表格仅使用一致库的新日志。

- Backend / bundle：`b71d6e4afd7e096f927ddeea09d1cdb8d106efd7d02dffb6d0ad02d5df5a24c3`；冻结副本 `final/librenderdoc.dylib`。
- GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。
- 隔离 provider（本批复用）：`aba8b52b92e43ffd7b64e146874db4d1de3b2a7d1ebd66b113dff089f507b1d1`；没有重新编译 provider，不重复声称新模块验收。

历史 Qt viewItemSize、AS bridge ForceCrash 和系统重启根因本批未确证，不声称已修复。下一 B539 以新 UE 三个帧内新建 Header 为输入，先用真实 converted query 小 sample 补 typed header 创建、kind3 descriptor当前slot来源及 query consumer 链接、AS/贡献依赖和事件重置；然后再实际 UE 验收。不能只删除 coverage65 guard、将 kind3 当普通 raw 地址或提前开启全局 RT。
