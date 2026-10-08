# B537 — 类型化 AS header 来源、重定位与 UE 捕获

2026-10-06，PHASE59。批次完成；**UE 动态 header/query 重放尚未完成，两项生产 RT capability 仍 false**。没有提交、推送、修改安装引擎或原用户工程。

## 实现与参照

先核对仓库 Vulkan `Serialise_vkCreateAccelerationStructureKHR` / `Serialise_vkCmdBuildAccelerationStructuresKHR` 的显式资源与 build 参数，DX12 `D3D12RTManager::PatchRayDispatch` 的 typed descriptor/地址重定位及 AS 初态管理。延续其 ResourceId、偏移、捕获身份和重放对象的验证方式。Apple IR 的公开 64-byte header 是必要平台语义；不查看 AS 内部、不提供 RT shader 单步或 RT Pixel History。

- 新增尾部 chunk `MTLBuffer::DeclareRayASHeader`，旧 chunk 数值和布局不改。`metal.rayASHeader` 接收 `[headerOffset, known AS object pointer, known contribution buffer pointer, contributionOffset]`，捕获端仅通过已注册 wrapper 查询对象，序列化 ResourceId、offset、64-byte snapshot。校验 Shared 可读范围、AS ID、贡献地址与六个保留字。Shared placement header 可以记录捕获事实；这不授予重放 coverage。
- 完整重放仍限定 coverage3、独立 Shared backing ≤16KiB、background 不可变 header/contribution、已声明 query。验证 snapshot 与初态、明确 AS/贡献对象及 captured identity 后重定位两字段；保留普通 descriptor 布局和旧 capture 路径，事件回退与 EID0 每次恢复并重新 patch。frame declaration、错误对象/范围/字节或缺 query 契约明确拒绝。
- UE provider 在 `IRDescriptorTableSetAccelerationStructure` 记录 kind3 的已知 header 对象/子分配 offset；在真正写完 header 后记录 AS 与贡献来源。kind3 明确是 AS header，不映射为普通 kind0 地址；动态 descriptor/query consumer 完整支持前继续拒绝其重放。
- 隔离 UE5.8.3 MetalRHI 编译成功，98 exports、无缺失；对 B535 只改 `MetalBindlessDescriptors.cpp`、`MetalRayTracing.cpp`、provider header。16MiB placement block 与其余 source 保持 B535。安装引擎三个对应源文件哈希见 `provider-proof.json`。

## 最终验收（同一最终 backend）

产物根：`build-macos-debug/metal-ray-b537/final/`；总结果 `final-manifest.json`，顺序执行记录 `accept-manifest.json`。所有构建与 GPU 测试使用现有共同锁串行执行，子进程有有限 timeout、独立进程组和保存日志；无其他 UE/qrenderdoc GPU 作业并行。

| Query 场景 | Header / contribution offset | 实例 | native/capture/API/CLI |
|---|---:|---:|---|
| static32 | 32 / 32 | 1 | PASS |
| static0 | 0 / 0 | 1 | PASS |
| Private 输入、header 到 backing 尾部 | 16304 / 4096 | 1 | PASS |
| Private 帧内 TLAS 重建 | 32 / 32 | 1 | PASS |
| 帧内新 TLAS target | 4096 / 4096 | 1 | PASS |
| 64 indexed 几何、64 实例、最后命中 | 16304 / 4096 | 64 | PASS |
| 空 TLAS | 32 / 32 | 1 | PASS |
| annotation controls | 4096 / 4096 | 1 | PASS |

Query AIR 只读取 header 中的 AS ID；贡献地址的来源、snapshot和patch/读回已检查，不声称 query shader 实际消费了贡献数组。八场景实际 query AIR/非零 dispatch、四 uint 结果、AS ID/VA 改变、贡献 prefix/有效区/保留字、resource usage、前后事件往返及 EID0 共 **152 事件选择**通过。Private/GPU 输入 build 后清零，frame/newTarget/64几何命中验证保持；64 几何/64 实例最后命中4398。API controls 实际拒绝12种错误对象/指针/对齐/范围/保留字，并接受重复的相同不可变声明。每个场景 manifest 保存来源、引擎版本、converter、DXC、runtime headers、sample 源码、DXIL/metallib/capture/binaries 哈希。

| 损坏 capture 集 | 组数 | API4 / CLI1 拒绝 | 合法控制 |
|---|---:|---:|---:|
| static32-bad | 67 | 134 | 0 |
| new4096-bad | 116 | 232 | 2 |
| multi64-end-bad | 160 | 320 | 3 |
| 合计 | **343** | **686** | **5** |

包含新 typed header 的空/错误资源、BLAS冒充TLAS、贡献源互换、offset/范围溢出、snapshot短/错AS/错VA/保留字、重复、frame位置、缺query、coverage65，以及旧 root/header/grid、typed AS 配方、Private输入、frame build/geometry/lifetime 反例。全部拒绝无 `Metal replay wait begin`、无严格诊断；五合法控制 API 与 CLI loops3 通过。以实际 gate 记录为准，不将资源对象创建等同于 GPU 提交。

47项相关旧回归 PASS：B536六个17-format buffer-view strong API（408 raw/PickPixel）、旧16-format两capture API/CLI、旧large R32三个API/CLI、fresh 163初态纹理 native/two captures/API/CLI（12696 subresource）、八份旧 B526 query API/CLI、B512 multi64 TraceRay API/CLI、官方 Apple 两scene/270事件/10查询/CLI loops3、六项 device capability gate。官方 MIT sample 使用既有下载 pin：ZIP `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`，来源 https://docs-assets.developer.apple.com/published/ade36d76f1bb/AcceleratingRayTracingUsingMetal.zip；构建方法与本轮实际 library/artifact hashes 见 `regressions/official/gate-results/manifest.json`。

最终范围2378份保存日志无 OVERUNNING/断言/seek/资源类型/map或record泄漏诊断。完整78IR、75RT、308、旧全量frame-family、Qt/ARC、官方13坏capture **未运行**，不计本库通过。

## 实际 UE Lumen 截帧

最终 `UE-capture/original.rdc`：65,098,798 bytes，SHA256 `b35de06df8fde3ecb3c143ee44e91c04d8ff176118e3a015de2d5be2aa796a99`。使用隔离小场景/16MiB provider，命令及实际 cvar 来源/数值在 `UE-capture/manifest.json`。r.RayTracing / Lumen.HardwareRayTracing / Inline 实际为1；launcher成功保存capture后 owned cleanup -9，**不计正常UE退出**。

- 3 indirect TLAS build、99 direct+56 indirect compute、7 heap AS/identity；60原metallib AIR审查，0 unvalidated，两个实际非零绑定的 HW query shader：Main_0000a274_46403057 `[2,64,1]`（indirect chunk28849）与 Main_0000c358_ed865861 `[132,1,1]`（chunk30635）。这证明绑定 ray-query AIR 的实际调度；未证明每次 shader 分支执行或最终光追输出。
- **6 header facts，其中3条frame写入，23条kind3 descriptor bindings**。全部 snapshot AS ID、贡献VA+offset和保留字核对通过，见 `UE-header-proof.json`。frame header Buffer12507/12510/12511 分别绑定 AS2559/2563/2566 与 contribution3119/3131/3139；这些 header 为 Shared placement heap 64-byte buffer、offset0。
- 68 heaps共1,558,989,824 bytes，初态blob1,123,342,185 bytes；未对本capture宣称 CPU/GPU总预算已完整验收，因为 typed frame-header guard 先拒绝。
- normal-open API4：旧 frame-born identity 边界；CPU65与pre-submit65 API4：`Invalid or dynamic Metal typed AS header declaration; frame-authored headers require a later query contract`。三检查无GPU wait，未上传初态或执行 frame GPU。**UE 输出、完整重放、事件定位/EID0/绑定闭包仍未验收。**

## 保留失败与限制

首轮 build 的同一行 SERIALISE_ELEMENT 宏命名冲突与 chunk Max assertion 未递增，均修复并保留两份失败日志。第一空场景 fixture 给贡献有效区留下0xa5，而空 helper 不写该区，强 oracle 失败；显式初始化有效区0后复验通过。第一隔离UEcapture header被独立buffer捕获guard拒绝（仍有kind3来源），保留旧capture `4bdaed53…` / provisional库 `167543b2…`；允许Shared placement **捕获事实**后，在最终库重新跑完整八样例、343反例、47回归和UE新截帧。未把此前通过当作最终库证据。

用户报告过的 Qt viewItemSize、AS bridge ForceCrash、系统重启根因本批未确证；本轮无此类严格诊断，不声称已修复历史崩溃。

## 哈希与后续

- 最终 backend / bundle：`2b363cb36c313f13ecfd47c9676b1f78fbadd2eeb2c15b8af2f3e044509c1c79`；已保存 `final/librenderdoc.dylib`。
- GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。
- 隔离 MetalRHI：`aba8b52b92e43ffd7b64e146874db4d1de3b2a7d1ebd66b113dff089f507b1d1`。

下一 B538：以本轮三条实际 frame-header 与kind3当前slot为证据，补有界动态header写入版本、Private contribution 来源及 descriptor producer/consumer 链接；先构造真实帧内写入的小样例验证捕获、patch、事件/EID0、坏capture，再接实际UE query PSO/root/heap绑定。不能只删除 frame-header guard 或将kind3当raw地址。全局能力保持false，直到官方sample及UE实际输出/完整重放/事件/相关回归/原生设备判断均达标。持续任务继续 active。
