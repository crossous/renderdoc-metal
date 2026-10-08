# B509：IR 帧内 Private 间接 TLAS 重建

接续 PHASE59/B508，补齐显式转换 TraceRay 的当前 AS 配方和帧内构建预检。最终固定库串行验收已完成。生产 compute/render RT 仍 false，未运行 UE。构建、native/capture/replay 串行，未提交或推送。

## 实现与对照

先查 DX12 `d3d12_manager.cpp:2316` 的 `CopyBuildInputs` 与 Vulkan `vk_acceleration_structure.cpp:453–484` 的初态输入复制/重建；VK 的 `arrayOfPointers` 分支明确不支持，Metal 不扩大到任意间接指针遍历。沿用已有 Private 输入在执行点冻结、72-byte 间接描述转换至 typed live child BLAS/UserID 的实现。没有读取 AS 内部，也没有从任意整数猜资源。

新增 `m_RayIRASCurrentContents`，与初始配方分开。预检和实际 replay 的 typed `buildIndirectInstances` 校验输入存储模式、buffer 范围/stride/count/flags、冻结字节、子 BLAS 种类和 GPU identity、AS/scratch 尺寸，再更新当前配方；TraceRay 的 direct/heap AS header、贡献索引、几何/IFT 闭包读取当前配方。EID0/每次初态恢复重置当前配方。预检 scope 结束也恢复初始配方，不能把扫描到的最终 AS 当作第一次调度的状态。

帧预检记录 command queue、command buffer、AS/compute/blit encoder 与 commit，要求同队列、encoder 结束和构建先于消费提交；也允许结束 AS encoder 后在同一 command buffer 调度。仍拒绝未验证的其他 AS 操作、函数表/IR 布局修改、render/ICB 路径。普通 buffer blit copy/fill 使用序列化的 typed 目的资源与范围；累积整个帧的 GPU 写入，拒绝与任何 TraceRay 不可变输入相交，包括没有 descriptor field 的贡献标量。IR 路径不再用 blit payload 整数扫描判定资源。

未变化的 Shared 创建数据可以提供 blit-only 上传/读回 buffer 的 CPU shadow，严格限定已知创建内容、非 heap、非 alias、<=64KiB，同值 coherent snapshot 才接受；不是允许动态 ABI 修改。IR 参数/GRS/SBT/资源和 sampler heap 仍是显式 immutable PSO/function 关联的 Shared 数据。

## 实际 sample

七个新增场景，均两次真实 TraceRay：首次消费初始 AS，GPU 上传 Private 间接描述，再重建同一 TLAS（UserID73→74），随后清空输入，最后消费重建的 AS。两次调度有独立输出/packet/GRS，重放时早期结果必须一直保留；最终输出在末次调度前保持7/7，EID0两份输出都恢复7/7。另检查 AS-ID/VA/texture 分配顺序变化、usage 与参数绑定。

实际前/后输出（均保留独立早期结果）如下：

| 场景 | 初始调度 | 帧内重建后 | 三方向事件选择 |
| --- | --- | --- | --- |
| 普通 | 146/11 | 147/11 | 150 |
| 初始空 | 11/11 | 147/11 | 150 |
| 初始全屏蔽 | 11/11 | 147/11 | 150 |
| heap-only AS | 34/183 | 34/184 | 171 |
| heap-only 初始空 | 34/48 | 34/184 | 171 |
| heap+local 六 sampler | 266/241 | 266/242 | 189 |
| global+heap+local 六 sampler | 1477/1452 | 1477/1453 | 207 |

场景覆盖普通、初始空/全屏蔽、heap-only AS、heap-only 初始空、heap+local 六 sampler、global+heap+local 六 sampler。只验证既有目标 AS 的帧内非空 Private 间接重建；不计新增 AS allocation、帧内 BLAS/refit/compact、Shared 帧内 AS、任意 release 或通用 UE producer 为通过。

## 反例与合法控制

新增 `metal_ir_ray_frame_invalid.py`：错误 encoder/目标/输入/scratch、参数范围/类型/flags、子 BLAS 列表与 GPU identity、IFT、截断冻结数据、未结束 encoder/错误提交顺序/跨队列、贡献索引越界，以及 GPU fill/copy 改写贡献、header、packet。要求语义拒绝、正常 exit1，无断言/overrun/signal/timeout。

合法控制分别改变 UserID75、子 BLAS GPU identity，并将捕获的构建、清空与末次调度合并进同一 command buffer，再检查全部 API 事件/输出/EID0。最后一项是重放导入控制，不是新生成的 native 同 CB sample。空初态反例强制校验帧内新实例的贡献/IFT，避免初态 count0 掩盖验证缺口。脚本与现有测试共用 TMPDIR 下的 exclusive IR 测试锁。

## 最终结果

最终 backend/bundle SHA256 `d8d0b308d2099d60495be64d86a7966f4d094a809a4153574df26d91890881d9`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。cmd/app先后构建，起止hash一致；全部结果来自此库，不继承中间库的PASS。

- 58个真实转换 TraceRay 场景 native/capture/CPU ABI/API4335次事件选择/三方向/EID0/CLI各3loops通过。其中七个新帧内场景1188次选择；保持独立早期结果、最终7/7直到第二调度、EID0两输出复位，并强制 AS-ID/VA/texture 分配顺序变化。
- 19组坏IR：862正常拒绝、65合法控制；本批新增帧内四组156/12。三组旧 coherent header 控制在修正chunk长度并放到实际commit前后通过，另保存 `coherent-header-control-evidence.json`。
- 1坏初始列表/1合法往返控制；5次CPU结构导出，9项WrittenRecord保持7true/2false，最大数组计数在分配前拒绝、正常exit1。外层错误分类差异保留说明，不依赖其通用文案作为PASS依据。
- 18旧targeted API/CLI、41旧argument反例、6旧B500 packet、旧B501/B502 IR、无注解raw IR预期拒绝、6能力查询通过。
- 官方Apple MIT sample两个scene：seed1、4帧、64×64；native重复/capture/逐字节输出/API270事件/CLI各3loops、10尺寸查询、13坏官方capture通过。ZIP SHA256 `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`，来源/固定版本/构建和输出manifest在 `apple-sample/gate-results/`。41是旧argument数量，与官方13分开。
- 普通t35、global+heap+local帧内Private重建及官方两scene，4×10正常重开，resident growth3588096bytes、exit0。另同一进程30失败打开（超大列表、未知初态ASkind、新GPU改写贡献反例）+10成功打开/10CPU结构导出/30次EID0→dispatch→EID0，growth737280bytes、输出146/11与reset7/7，exit0。仅有限关闭验证，不当作任意ARC或UE验收。
- 最终2770份日志无 `Assertion failed`、`OVERRUNNING CHUNK`、`Unexpected Metal resource type`、`m_ResourceMap.empty` 或旧not-handled标记。严格排除本文件下列所有保留的中间失败和旧库产物，不把排除范围计为PASS。
- Python/Bash语法、diff检查、frame gate的CPU互斥检查通过；持锁时--help阻塞，释放后exit0、无GPU。所有GPU/构建串行。测试子步骤已整合进58场景包装器，包装器整体本批未直接运行。

产物：`captures/metal-ray-b509/final/<mode>/ir-runtime_capture.rdc`；`build-macos-debug/metal-ray-b509/final/manifest.json`、`validated-modes.json`、`modes-manifest.json`、`remaining-manifest.json`、19组反例manifest、`initial-list-close/manifest.json`、两份生命周期日志、`shutdown-log-audit.json`、`source-binary-hashes.json`、`coherent-header-control-evidence.json`、官方sample目录。B509根目录另保存构建/runner日志、`frame-chunk-metadata-evidence.json`、CPU锁检查、系统报告清单与UE scratch-offset源码证据。

未运行：完整75旧RT套件、集中308套件、实际UE RT/离线、原Qt sizeHint复现、任意ARC提前释放、独立旧descriptor-table十反例；不能继承其他库PASS。未启用生产RT。

## 保留的失败和修正

1. fixture 初次误把 `MTLInstanceAccelerationStructureDescriptor` 类声明成协议，native 编译失败；修正类指针，记录 `first-frame-compile-failure/`。
2. 首份 native/capture/ABI 成功，但预检拒绝 capture 内临时创建的验证 buffer。将 sample 的读回验证 buffer 提前创建；不是新增帧内 allocation 支持。记录 `first-frame-replay-failure/`。
3. 随后 blit-only Shared upload/readback 的未变化 CPU coherent snapshot 没有独立初态，预检拒绝；增加创建内容 shadow，记录 `first-frame/`、`diagnostic-line.log`、`creation-shadow-API.log`。该中间捕获 manifest 保留 FAIL，单独150事件诊断 PASS 不算最终全部验收。
4. 新 replay oracle 的 `ResultDetails::Message()` inline fallback 引用未导出 `DoStringise<ResultCode>`，链接失败；改为数值 code，记录 `first-final-replay-compile-failure/`。
5. 扩展 local-root 后 audit 的局部 VA 字典覆盖了按 ResourceId 的 source 字典；修正局部变量，记录 `before-final-blit-guard/` 的 FAIL。普通/empty/masked/heap 的中间 PASS 也与最终库分开。
6. 合并 CB 控制最初仍保留 commit 前的 wait，重放干净拒绝；修正控制为 commit/wait 均置于 End of Capture 前。记录 `frame-invalid-first/`，不计同 CB 通过。新增 typed blit 预检第一次用无默认构造的 `NS::Range`，编译失败；显式 Make(0,0) 修正，记录两份 build 日志。`frame-invalid-typed-first/` 39反例/3控制通过仅属于中间2d86库。

7. 初始列表最大数组计数仍在分配前拒绝、正常exit1且无断言，但外层错误由原 B508 的 APIDataCorrupted 文本变为 APIReplayFailed/Failed to process Metal chunk，旧 gate 的通用字符串列表未识别。保留 `initial-list-error-classification-failure/` 和 `first-remaining-manifest.json`，改核验具体的 pre-allocation 数组边界诊断、最大计数、对应列表 chunk 和正常失败退出。后端错误分类传播没有在本批更改或宣称一致。当时已通过的19组IR反例保留、不重复计数；先重新运行初始列表及剩余步骤，随后第8/9项新库又完整重验。

8. 汇总 helper 首次误把 sample gate 的精确状态 `PASS NATIVE/CAPTURE/REPLAY` 当成 `PASS`，修正该格式匹配。随后最终日志审计真正发现七份新 capture 的 `SetChunkMetadataRecording` offset 断言：`WriteMetalASFrameBuild` 原本在已经写入初态/其他 chunk 的文件流上切换 flags。改用独立 scratch WriteSerialiser 生成完整 Chunk，保留原 build metadata/flags，写入主流后按 Chunk::Delete 生命周期释放；不在文件流上切换 flags。首轮误用 private destructor 的 delete 编译失败已修正，构建日志保留。`before-frame-writer-fix/` 保存全部 f151 库产物，不能作为最终无诊断 PASS。
9. 同次审计还发现 B506 原有 `legal-coherent-AS-header-commit` 控制把4-byte CPU差异替换成64-byte header仍沿用48-byte chunk，且追加在 End of Capture 之后；CLIexit0没有实际完整验证它。已为增长/对齐补足长度、插到首个实际 commit 前，并给 heap-AS gate 加断言/overrun/resource-map 诊断拒绝。三组旧控制必须按修正后新库重跑，不能忽略诊断。已通过的 f151 功能结果全部保留；本次新 capture writer 改变 backend，重新运行58场景/19组IR反例及所有相关验收，按 d8d0 起止hash独立计数。

## 后续与边界

下一步先补 UE 使用的非零 scratch offset：`MetalRayTracing.cpp:2459` 传入 ScratchBufferOffset，而当前 indirect bridge `:174` 仍直接拒绝非零 offset 并调用 METAL_NOT_HOOKED。这是源码缺口，与用户栈入口一致，尚未证明是该次崩溃的实际触发条件；记录 `UE-scratch-offset-source-evidence.json`。随后补实际帧内 AS 目标/几何/资源生命周期及必要动态输入，继续调查可可靠关联的 UE shader/header/library/export/function/PSO 与 typed descriptor factory；显式 fixture 注解不冒充自动 UE 支持。heap CBV/UAV/typed view、nested UB、大 heap/工作量、Private/GPU IR、动态 heap、必要 SBT/callable/render RT 仍未验收。

用户系统冻结只有此前 WindowServer watchdog/强制重启、UE compiler 等待证据；本次 filename/stat 清单未发现新的 panic/watchdog。没有运行旧长 UE/full followup，未定位冻结具体根因，也不声称修复用户 UE build-AS ForceCrash 或原 Qt sizeHint 崩溃。只有最终 sample、实际 UE RT、相关回归和 native 能力判断均达标后才能启用能力；目标保持 active。
