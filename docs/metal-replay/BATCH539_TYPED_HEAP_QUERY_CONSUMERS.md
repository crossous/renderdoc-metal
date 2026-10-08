# B539 — typed kind3 descriptor heap 的 RayQuery 消费路径

2026-10-06，PHASE59。本批完成**初态 AS、CPU 发布的 kind3 槽位、已声明 query PSO** 的真实 converted query 重放。实际 UE 的帧内新 Header、帧内 AS build 与具体 Lumen PSO/root/indirect dispatch 仍未闭合；两项生产 RT capability 保持 false，持续任务 active。未提交、推送、重置仓库或改安装引擎/原用户工程。

## 接口与实现

先核对本仓库 DX12 `D3D12RTManager::PatchRayDispatch`（`d3d12_manager.cpp`）、Vulkan `Serialise_vkCreateAccelerationStructureKHR` / `Serialise_vkCmdBuildAccelerationStructuresKHR` 的已知对象、descriptor/address patch、build 配方与资源依赖模式。遵循相同捕获/初态/重放边界；Metal Shader Converter 的 AS header 是必要的 runtime ABI 适配，不读取 Metal AS 内部，不提供 RT shader 单步或 RT Pixel History。

- 新增显式背景 annotation `metal.rayQueryHeapDispatch`：对象为 compute PSO，UInt64×4 值为 `[known descriptor buffer object, slotOffset, known output buffer object, 0]`。仅 coverage65，唯一 PSO，最多64声明；heap/output 为非 alias 的 standalone Shared buffer、整个 backing≤16KiB，24-byte 槽位对齐且有完整初态。序列化追加 chunk1438，Max1439；既有 Header chunk1437和旧 chunk 布局不变。
- kind3 保留 AS-header 来源类型，限定 type4/单一来源；验证已知 Header 对象、offset、64-byte snapshot、已知 AS ID、已知贡献源 GPUVA。Descriptor 第一个字段重定位到重放 Header GPUVA+offset，Header 两字段再重定位到已知重放 AS ID/贡献地址；第二 descriptor word 必须0，第三 word 保留。不将 kind3 转成普通 kind0 raw 地址。
- coverage65 新 query namespace 可使用初态 Header，支持既有 Shared placement Header 和不可变 Shared/Private contribution、整个 backing≤16KiB。完整初态字节和 snapshot 必须匹配，重置/EID0 恢复 raw 初态并重新 patch。
- query contract 对应真实 DXIL cs6_6 `ResourceDescriptorHeap[sceneSlot]`，inline root index2 为16字节 `[output GPUVA, uint sceneSlot, uint reserved=0]`，descriptor heap buffer0 offset0。直接一维非零 dispatch，工作量≤4096；校验实际当前 CPU slot、PSO、输出范围、TLAS及子 BLAS typed 初态配方、贡献范围。资源 usage 包含 descriptor/Header/contribution/AS/子AS与输出。
- kind3 consumer 必须为声明的 query PSO；有效但未声明的 PSO 同样拒绝。Header/contribution 的 GPU 写入（query前或后）、frame AS encoder/build、GPU expected slot、间接 query dispatch均不属于当前 contract，提交前拒绝。当前扫描对 live kind3 槽位保守约束全部消费者；尚不是 UE 全局大 descriptor heap 的精确 slot footprint。
- 抽取共用 `ValidateRayQueryStructure`，旧 coverage3 的动态 Header/current AS recipe 路径继续使用同一已知对象验证；保留旧路径，未全局放宽 coverage65 guard。

本 sample 的 query AIR 读取 Header 的 AS ID。贡献指针重定位、范围、整 backing 和保护字验证通过，**未声称 shader 实际读取贡献数组**。

## 最终库的真实 sample 验收

产物根 `build-macos-debug/metal-ray-b539/final/`，汇总 `final-manifest.json`；可重跑入口 `variants.py`、`old-dynamic.py`、`regressions.py`、`lifecycle.py`、`preflight.py`、`freeze.py`。构建/GPU均使用公共 IR 测试锁，有限帧数/超时、进程组清理，确认无 UnrealEditor/qrenderdoc 并发。Backend/bundle最终一致为 `1e13bd74…`，每份 manifest 保存 DXC/converter/runtime header、DXIL/metallib、源码/helper/capture/backend 哈希和 build 方法。

| 新 heap-query 场景 | Header / contribution offset | 实际初态 TLAS | native / capture / API / CLI |
|---|---:|---|---|
| shared32 | 32 / 32 | direct，1实例，Shared源 | PASS，1000/1000/0/0 |
| placement-private | 4096 / 4096 | direct，1实例，Private贡献 | PASS，1000/1000/0/0 |
| private64-end | 16304 / 4096 | indirect Private输入，64实例 | PASS，3312/3312/0/0 |
| empty64 | 32 / 32 | indirect Private输入，空TLAS | PASS，0/0/0/0 |

四份 capture、64次事件选择/EID0。实际 cs6_6 reflection 为 UAV/Constant 两个根字段，UsesRayQuery=true，AIR query call及捕获原 metallib一致；每份都有实际非零 query dispatch。强 oracle 比较四 uint 输出、AS ID/贡献VA/HeaderVA重新分配、Header padding、贡献全部字节、descriptor与usage，并跨末事件、0、dispatch前、dispatch往返四轮。64实例验证最后 UserID136，空 TLAS 真实 native/query未命中；不是空调度。64实例/空TLAS场景的 native TLAS由 Private/GPU输入构建并清零原源，离线依赖冻结配方。

另**重新编译并新截帧**验证六个旧动态 Header 场景、七份 capture：dynamic-shared / private-contribution / placement-header / dynamic-private / dynamic-placement-private64 / reused两份，全部 native/capture/API/CLI PASS，192次事件选择/EID0。包括帧内新TLAS前后 query、同Header版本与贡献地址+16、连续两次 capture 的背景初态刷新；未把它们算作新 coverage65 frame-build 支持。

## 损坏 capture 与旧回归

新 runner `util/test/metal/metal_ir_query_heap_invalid.py` 使用 `--encoder-template` 指向已核验 B538 导出 capture 的 blit/AS encoder chunk，记录其哈希；插入真实结构的 GPU fill/encoder与有效新PSO。坏 capture 均成功导入，API4/CLI1且无 `Metal replay wait begin`。

| 最终测试目录 | 坏组 | API/CLI无GPUwait拒绝 | 强API/CLI合法控制 |
|---|---:|---:|---:|
| private-bad-expanded | 50 | 100 | 1 |
| private64-bad | 63 | 126 | 1 |
| shared-bad | 50 | 100 | 1 |
| empty-bad-final | 56 | 112 | 1 |
| old-dynamic-bad-final | 128 | 256 | 3 |
| 合计 | **347** | **694** | **7** |

覆盖声明缺失/重复/帧内发布、错误 PSO/heap/output/root/inline、kind3来源/type/range、Header AS/贡献/snapshot、GPU前后改写、frame AS encoder、有效未声明消费PSO、BLAS冒充TLAS、缺初态AS配方、64实例上限/stride/type/child/NaN/options，以及旧动态Header/build/几何反例。重复早期42组/private-bad 和失败中间运行不计此总数。

47项相关旧回归重新 PASS：B536六个17格式buffer-view API、旧16格式两capture API/CLI、旧large R32三份API/CLI、新163初态纹理native/两capture/API/CLI（12696 subresource）、八份B526 query API/CLI、B512 multi64 TraceRay API/CLI、六项 capability、官方 Apple 两scene270事件/10尺寸查询/CLI loops3。

官方 MIT sample 复用已下载且已审查 pin：`https://docs-assets.developer.apple.com/published/ade36d76f1bb/AcceleratingRayTracingUsingMetal.zip`；ZIP SHA256 `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`。本库重新验收的输出/构建/artifact hashes见 `regressions/official/gate-results/manifest.json`。

两份新 heap-query capture ×10轮 controller open/seek-reset/readback/close PASS，resident growth **442368 bytes**，保留64MiB上限；精确输出由各专用强 oracle 验证。最终目录3762份日志（包含保留失败）无严格断言、stream seek、overrun、map诊断；失败记录不是PASS。完整78IR/75RT/308、全量旧frame-family、Qt/ARC、官方13损坏capture **未运行**。

## 实际 UE 与剩余工作

本批**没有重新启动 UE 或生成新 UE capture**：provider及UE捕获语义未改变，复用B538真实 Lumen capture `c41abb060d280b3c197d58faa0a8eb4665b04156b6a2a38fe1e36e070513e574`（66,705,869bytes）。已知两个实际非零 HW query 调度与三帧内新 Header 的证据沿用 B538，不能说本批重新核验全部AIR或UE输出。

在最终 `1e13bd74…` 上 normal-open / CPU65 / pre-submit65 三个检查均 API4，无GPUwait/初态上传/frameGPU。65仍明确拒绝：`Dynamic Metal AS header needs typed initial query coverage3; UE heap-query producer/consumer closure is not yet supported`。本 sample 的新 PSO annotation 尚未应用到 UE，不冒充 UE 的绑定契约；UE完整输出、事件/EID0、总预算和 GPU 重放均未验收。

下一 B540：从真实 UE frame Header13230/13233/13234与 AS2559/2563/2566事实出发，用有限 sample 补 Header帧内创建/发布、当前typed AS build配方及CPU slot版本/消费的时序；再扩实际 Lumen PSO/root/indirect调度与GPU descriptor producer的明确契约。每步做 native→capture→replay/事件/反例，再运行UE；禁止只删除 guard或提前开启生产能力。

## 保留失败与哈希

- 首 strong helper 使用公共API没有的 `RDCMAX` 编译失败，保存 `heap-query-first/failed-strong-strong-build.log`，改为普通比较后最终四份 helper 编译/强验收通过。
- 空TLAS反例构造错误读取空 children array，保留 `final/empty-bad` FAIL；改用明确的已知 primitive AS，完整56组及合法控制在 `empty-bad-final`复验通过。
- 旧动态反例首次调用遗漏 `--oracle`，到合法控制启动失败，保留 `old-dynamic-bad` FAIL；正确 oracle/mode/64实例完整128组/三控制在 `old-dynamic-bad-final`复验。runner新增缺oracle提前参数错误，避免到合法控制才失败。
- Backend / bundle：`1e13bd74f975bf50e918ed2a03234d83d7992bd85f87c2f134e39d056d16f85d`；冻结副本 `final/librenderdoc.dylib`。
- GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。
- provider复用：`aba8b52b92e43ffd7b64e146874db4d1de3b2a7d1ebd66b113dff089f507b1d1`，本批未重新编译。

历史 Qt viewItemSize、AS bridge ForceCrash、系统重启根因仍未确证，本批不声称修复。保留用户现有改动，包括 RDHeaderView.cpp。
