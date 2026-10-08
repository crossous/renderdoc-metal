# B507：转换 TraceRay 的间接 TLAS 与 UserID

接续 PHASE59/B506。补齐 IR AS header 对已捕获的间接、空和全屏蔽 TLAS 初态配方的消费；使用真实转换 TraceRay、InstanceID 和 Private/GPU 构建输入证明。生产 compute/render RT 仍 false，实际 UE 本批未运行。

## 实现依据与边界

先查 DX12 `D3D12RTManager::CopyBuildInputs`（`d3d12_manager.cpp:2316`）对 TLAS 实例输入的 GPU readback，以及 Vulkan `vk_acceleration_structure.cpp:453–484` 的明确 instance-buffer 地址、范围和输入复制。Metal 沿用已实现的 typed AS 初态与冻结实例，另行解释其特有 72-byte Indirect 实例中的 BLAS GPU ID；不扫描任意 GPU 整数。

`ValidateRayIRDispatch` 的 global/heap 公共 AS-header 闭包现在接受 kind5 direct、kind9 indirect、kind10 empty、kind11 all-masked。kind9 的 `children` 和 `childGPUIdentities` 必须对应，复用 `ConvertMetalASIndirectInstances` 把冻结72-byte数据转成保留UserID的68-byte语义，再逐实例核验子 BLAS/geometry/IFT。kind10 必须零count、无实例字节与子AS；kind11 用 `ValidMetalASInactiveInstances` 核验全部 mask0/AS ID0，不伪造子 BLAS。原初态注册仍校验 source 范围、类型、schema 和冻结字节，IR 独立保留最多64实例边界；恢复后的 TLAS `m_LastBuildKind` 为5，与原有初态重建一致。

非空实例贡献地址必须有效、4对齐、范围至少 count×4，并受 SBT hit-record 边界限制。空 TLAS 不读取贡献值：可以是 null pointer；若有非零地址，仍必须是明确且有效的至少4-byte backing。header/非零贡献来源、TLAS 与实际子 BLAS 仍加入 CS read usage，保留别名保护和 EID0 恢复。empty/all-masked 没有子 BLAS，不要求读取仅供对照使用的旧 TLAS/BLAS。

这批只消费帧开始时冻结的 AS 配方。`metal_descriptor_tables.cpp:4906` 的 IR 静态契约仍提前拒绝所有帧内 AS encoder 操作，不能计为 UE 实际帧内 build/dispatch 支持，也不能仅删掉该 guard。下一批须增加按事件更新的当前 AS 配方和 preflight 闭包，再证明帧内 GPU 构建输入/子AS变化及 EID0 往返；不得用旧初态代替帧内状态。

## 真实样例

自产 MIT HLSL 增加间接模式的 `payload.value += InstanceID()`。活跃实例的 UserID 为73，命中基础输出由73变成146；局部根模式131变成204。VFT/IFT、152-byte packet、32/96-byte SBT 与 direct/heap-AS 绑定继续使用本机审查过的 UE5.8.3 DXC→Apple converter/runtime。

实例来源是88-byte backing，offset8、stride80，实际冻结72字节；活跃AS ID来自已知 BLAS，empty零count，all-masked 为 mask0/AS ID0但保留UserID73。heap 场景沿用不同位置的第二个 TLAS（x=100），区分全局根与 heap 绑定。Private 场景先 GPU blit 上传，再建 AS，随后 GPU 清零整个88字节并读回核验全零；真实 TraceRay 与重放继续使用此前已构建/冻结的实例，不能从后来清零的源 buffer 重新猜配方。

| 新模式 | 两个 uint32 输出 |
| --- | --- |
| ue-indirect-tlas | 146 / 11 |
| ue-indirect-tlas-empty | 11 / 11 |
| ue-indirect-tlas-masked | 11 / 11 |
| ue-indirect-tlas-private | 146 / 11 |
| ue-indirect-tlas-private-masked | 11 / 11 |
| ue-descriptor-heaps-heap-as-only-indirect-tlas | 34 / 183 |
| ue-descriptor-heaps-heap-as-only-indirect-tlas-empty | 34 / 48 |
| ue-descriptor-heaps-heap-as-only-indirect-tlas-masked | 34 / 48 |
| ue-descriptor-heaps-heap-as-only-indirect-tlas-private | 34 / 183 |
| ue-global-descriptor-heaps-heap-as-local-root-six-samplers-indirect-tlas-private | 1477 / 1452 |
| ue-descriptor-heaps-heap-as-only-local-root-six-samplers-indirect-tlas | 266 / 241 |
| ue-descriptor-heaps-heap-as-only-indirect-tlas-any-hit | 34 / 48 |
| ue-indirect-tlas-any-hit | 11 / 11 |
| ue-indirect-tlas-null-hit | 7 / 11 |
| ue-indirect-tlas-null-miss | 146 / 7 |

最终组合 **51场景/3147次事件选择** 已通过 native/capture/ABI/API 三方向/EID0/CLI3loops；新增15场景870次，B506原36场景2277次重新运行。60份新增 SDK reflection/metallib hashes、GRS 布局、root flags、冻结实例 offset/stride/child GPU ID/UserID/transform/空/屏蔽配方，以及 Private 输入清零日志，CPU核验通过。

## 最终回归与哈希

backend/bundle SHA256：`ee222fe1b6c053f8ef0283d6032fd4509bd754de57939402170821449d9be408`；GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。cmd/app依次构建，最终所有构建及GPU步骤串行，起止库hash一致；没有UE进程参与。

新间接反例验证五份源capture：root Private、heap-only Private、global+local+heap Private各33坏输入/3合法控制；empty 27/3、Private masked 30/2。包括 schema/kind/source/type/offset/stride/count/flags、冻结实例截断/未知或null AS/错TLAS ID/缺或重复child、geometry IFT越界、贡献越hit records、错误header与贡献地址。原typed IR550/39重新通过，总计 **706坏IR正常拒绝/53合法控制**；signal/timeout不算拒绝。first-private先行33/3不重复计入。

18旧targeted API/CLI、41旧argument反例、6旧B500 packet、旧B501/B502 IR API/CLI、无声明raw IR预期拒绝、6项native/default/probe等能力查询通过。官方Apple MIT两个scene重新native重复/capture/逐字节输出/API270事件/CLI各3loops、10尺寸查询与13坏官方capture通过。固定ZIP SHA `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`。t35、新global+local+heap Private indirect及官方两scene **4×10重开**通过，resident growth **1589248bytes**、exit0；见下文已有device关闭诊断，不能计完整生命周期清理通过。

Python/Bash语法、diff和新增indirect gate共享进程锁CPU验证通过，锁持有时--help阻塞，释放后exit0，未启动GPU。没有重复运行整个包装器，实际子步骤逐项执行，包装器已纳入51场景与5个间接反例gate。

最终产物：`captures/metal-ray-b507/final/<mode>/ir-runtime_capture.rdc`，每模式ABI/compile/SDK/DXIL/metallib/native/capture/API/CLI证据；`build-macos-debug/metal-ray-b507/final/modes-manifest.json`、`remaining-manifest.json`、15个invalid manifest、`indirect-SDK-evidence.json`（60份）、`indirect-lock-check.json`、官方`apple-sample/gate-results/`、生命周期日志、`source-binary-hashes.json`与最终`manifest.json`。旧B506库保留`baseline/librenderdoc-b506.dylib`。

## 已保留的失败与控制

首个 empty API oracle 的输出11/11正确，但 capture 未使用的 BLAS 没有进入初态重建，导致只预留一个 replay AS 时发生 ID2→2，未达到强制重定位条件。修正测试为 empty/masked 预留第二个未建AS；不是后端放宽身份检查。

第一轮组合在 heap-only empty 的读取usage检查失败：仅供直接根对照的 TLAS/BLAS 仍在 capture，但实际 heap empty TLAS 没有子 BLAS。原 oracle 按应用 AS label 找对象，AS label 目前没有进入 GetResources 的自定义名称；改为从选中 header 的真实 AS GPU identity 找 ResourceId，并保留所有实际被使用 AS/子AS 的读取要求。首轮部分验收和 captures 已另存 `partial-before-usage-oracle-fix/`，不重复计入最终51组。

新反例脚本第一次缺初始化 ZIP blobs 字典，`first-private/indirect-invalid/manifest.json` 已明确改为 FAIL TEST SCRIPT，补齐初始化后 `indirect-invalid-corrected/` 33坏输入/3控制通过，不重复算最终反例。诊断 helper 曾误把 opaque ResourceId 转为整数而编译失败，已改按 ResourceDescription 打印；`debug-empty.log`、后续诊断与修正构建日志保留。CPU SDK审计首次把 SDK 输出的 `IRRootSignatureFlagNone` 枚举字符串当整数0，修正为真实序列化表示后60份验证通过，失败记录 `SDK-audit-first-failure.json` 保留。

合法 UserID74 控制实际输出147/11，证明冻结UserID重新构建并交给shader；合法 child/TLAS GPU ID 重编号同步修改其typed引用，CLI/API完整输出与事件仍正确。空场景另验证任意未读贡献值和null贡献指针，屏蔽场景允许改变未参与光交的UserID。

## 未闭环事项与接续

成功合法控制的 RenderDoc 日志存在 `metal_manager.cpp:146` 的 `Unexpected Metal resource type 4 during replay shutdown`（eResDevice）。核查 B506 `final/invalid/legal-pattern-unused-pad-oracle.log` 等也有同样诊断，属于已有问题，不是本批间接 TLAS 引入；但不能因此忽略。`WrappedMTLDevice::~WrappedMTLDevice` 拥有 manager 并在其 Shutdown 后释放 native device，`ResourceTypeRelease` 却把无 bridge 的 eResDevice 归到错误分支；constructor也把this加入资源管理器。下一批应区分 structured-export dummy 与 native replay owner 的实际关闭路径，明确 root device 所有权，验证成功/损坏 capture 关闭和重复生命周期，无递归删除或重复释放；当前有限重开结果不能等同于完整清理证明。

仍缺 AS label 的捕获名称、帧内 AS/IR 状态、可靠 UE producer shader/header/library/export/function/PSO 与 descriptor factory 关联、heap CBV/UAV/typed view、嵌套 UB、大 heap/更大工作量、Private/GPU IR 与动态 heap、callable/render RT。AS 内部查看、RT shader 单步与 RT Pixel History 不在承诺内。实际 UE RT/离线、75旧RT全覆盖、集中308、原Qt sizeHint复现、ARC任意提前释放与独立旧descriptor-table十反例本批未跑。B495系统冻结根因未修，不重跑旧长UE/full followup；生产能力false，持续目标active、未提交/推送。
