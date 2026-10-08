# B506：从 IR descriptor heap 绑定加速结构

接续 PHASE59/B505。实际补齐资源 heap 中的 AS header 依赖、校验和重定位，并支持没有直接 AS 根参数的转换 TraceRay。生产 compute/render RT 仍 false；本批没有运行实际 UE，不能计为完整 UE 支持。

## 对照与实现

先查 DX12 descriptor 写入和 bindless 依赖（`d3d12_device_wrap.cpp:1300–1385`）、`Serialise_SetDescriptorHeaps` 与 `Serialise_SetComputeRootDescriptorTable`，及 Vulkan `VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR` 的 typed AS 更新与 buffer descriptor 路径。采用其明确类型、资源依赖和身份重定位方式，保持 RT 黑盒边界。

UE 的 `MetalBindlessDescriptors.cpp:84–96` 同样由已知 AS factory 调用这个 helper；`MetalRayTracing.cpp:2416–2459` 使用 Indirect instance descriptor 并明确填写 header 的 AS ID 与贡献地址。因此 heap AS 与实际 UE 所需的间接实例闭包是两个必须分别验证的范围。

Apple 本机 runtime `ir_raytracing.h` 的 `IRDescriptorTableSetAccelerationStructure` 写入 `[headerGPUVA,0,0]`。第三 word 是零，不能误填 header 大小64，也不能猜成 GPU ID。原 `metal.rayIRHeapEntry` 新增 kind3 AS header，仅允许资源 heap0，声明 bytes 必须64；仍是 immutable PSO 声明，不增加新的捕获 chunk 或改变 Max1433。

将原全局根 AS header 校验抽成公共闭包，heap 和 global 共用：header 的 schema4 AS 身份、schema0 实例贡献地址、64-byte reserved 字节、live TLAS/direct kind5 配方、已构建种类、最多64实例、贡献数组4对齐与 SBT hit-record 边界、子 BLAS kind1/2/8、实例 child 索引、每段 geometry 的 IFT slot 及函数关联均需成立。把 header、贡献 buffer、TLAS 与子 BLAS 纳入 CS read 依赖；输出 UAV 不能覆盖其任一来源。贡献数组不能误读成任意 uint32 buffer。

明确 global 布局现在可以不含 kind5 AS 根，但必须有输出 UAV，并至少验证一个 global 或 heap AS header。没有直接根且没有已证明的 heap AS 时拒绝；旧两根隐式布局不改变。仅有未知资源 heap 地址仍不接受。

## 真实 TraceRay 证据

沿用已审查 UE5.8.3 DXC→Apple converter/runtime 和自产 MIT HLSL。`ResourceDescriptorHeap[2]` 真正提供 TraceRay 使用的 TLAS：新建第二个 direct TLAS，共用 BLAS，但实例平移 x=100；原全局 TLAS 的几何仍在 x=0。命中与未命中随之交换，防止仅重定位字节却仍使用全局 TLAS 的假通过。

额外两个 heap-only 模式的实际 SDK root signature 只有 UAV 和两个32-bit常量，**没有 AS SRV 根参数**；GRS 为 `[outputVA,0]`。未使用的旧 TLAS/header 保留作负对照，不要求其 CS read usage。新增 AS header 独立64-byte backing、实例贡献8-byte backing（实际使用首4字节）；后4字节为零，亦允许真实测试 UAV 覆盖贡献 buffer 的拒绝。

重放保持额外原生 buffer/未建 AS/texture，实际全局 AS ID2→3、heap AS ID3→4、SBT VA 与 texture ID 改变。oracle 从 capture 的实际初态字节取得身份，按 ResourceId 对照，不根据多个 AS chunk 的顺序选对象。验证 AS、BLAS、header、贡献及 heap read usage、packet 绑定、事件前后往返与 EID0。

| 新模式 | 两个 uint32 输出 |
| --- | --- |
| descriptor-heaps-heap-as | 34 / 110 |
| ue-descriptor-heaps-heap-as-any-hit | 34 / 48 |
| ue-descriptor-heaps-heap-as-null-hit | 34 / 44 |
| ue-descriptor-heaps-heap-as-null-miss | 30 / 110 |
| ue-descriptor-heaps-heap-as-local-root-six-samplers | 266 / 168 |
| ue-descriptor-heaps-heap-as-local-root-six-samplers-any-hit | 266 / 280 |
| ue-global-descriptor-heaps-heap-as-local-root-six-samplers | 1477 / 1379 |
| descriptor-heaps-heap-as-only | 34 / 110 |
| ue-descriptor-heaps-heap-as-only-local-root-six-samplers | 266 / 168 |

## 最终验收

最终 backend 与 bundle SHA256：`b00945068a18317e36fd26463793e80f256ceeae64cb72b1dcff23c8b39d4186`；GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。构建和 GPU 测试全部依次执行，起止 backend hash 一致，无 UE 进程参与。9个新增场景642次事件选择加原27场景1635次，最终 **36组/2277次** API 三方向/EID0，全部 native/capture/ABI/绑定/usage/强制身份变化与每组 CLI3loops通过。

新 AS heap gate 运行三份源 capture：普通与global+local各38坏输入/4合法控制，heap-only 39/4。覆盖 AS slot 类型/命名空间/大小、duplicate/frame/缺声明/缺 GPU 字段、未知/错类型/错对齐 header、header AS ID零/未知/BLAS代替TLAS、贡献数组范围/对齐/错buffer/越hit records、metadata/保留字节、UAV覆盖header/贡献，以及真实帧内 CPU 变更。合法同字节frame snapshot、AS ID与两种 VA 重编号均通过 CLI/API。原 default69/7、两local各58/4、两global各70/3、两heap各55/3重跑通过；总计 **550坏 IR 正常拒绝/39合法控制**。first-only corrected gate的先行39/4不重复加到最终计数。

当前库18旧targeted API/CLI、41旧argument反例、6份旧B500 packet、旧B501/B502 IR API/CLI、无注解raw IR预期拒绝、6项native/default/probe等能力查询通过。官方Apple MIT triangle/procedural重新native重复/capture/逐字节输出/API270事件/CLI各3，10尺寸查询及 **13坏官方捕获** 通过；固定 ZIP SHA `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`。t35、新global+local+heap-AS与官方两scene **4×10重开**通过，resident growth **933888bytes**、exit0；不是任意ARC父对象提前释放测试。

16个heap场景64份SDK reflection/对应metallib hash、root flags3072和GRS布局通过CPU核验，其中9个新增AS场景36份反射。heap-only的UAV/常量布局按SDK结果明确核验，仍不从reflection推断heap成员。Python/Bash语法、diff检查及新增heap-AS gate进程锁CPU测试通过：锁持有期间--help阻塞，释放后exit0，未启动GPU。没有重复执行shell包装器，实际子步骤全部完成。

更正前批文字计数：B505 `final/official-invalid.log` 同样明确是13坏官方捕获，旧汇总把41旧argument数量写进官方项。本批按实际日志分别记录13与41；已经更正B505文档及索引，其他历史库的未核验数不沿用。

## 失败与控制

早期 `native-proof/` 和 `first/` 的原生/捕获闭环只用于定位，中间库不计最终验收。构建依次保存 `build-cmd.log`、`build-app.log`、`build-cmd-final.log`、`build-app-final.log`、`build-cmd-heap-only.log`、`build-app-heap-only.log`。

首个 `mutated-AS-header-commit` 构造 FAIL 保留于 `first-only/heap-AS-invalid/`：它试图修改不存在的帧内 header CPU chunk，实际没有改任何字节，却当成坏输入。未变化 header 已由捕获初态记录，`CaptureCmdBufCPUWrites` 不产生冗余修改 chunk。这是测试构造错误，不能宣称后端漏拒绝已修复。

修正 gate 通过拷贝真实 frame CPU chunk 结构、新 blob 和明确 header ID/size，注入实际修改；同时加入字节完全一致的合法 frame snapshot 控制。前者正常拒绝，后者完整 CLI/API 输出与事件通过。独立合法 AS ID、header VA、贡献 VA 重编号控制验证所有 typed 引用同步修改后仍可重放。拒绝只接受匹配原因的正常非零 exit，signal/timeout 不算通过。

## 产物与接续

最终 captures：`captures/metal-ray-b506/final/<mode>/ir-runtime_capture.rdc`。每模式的 DXIL/metallib/root/reflection、native/capture/API/CLI/ABI/compile evidence 位于 `build-macos-debug/metal-ray-b506/final/<mode>/`；最终 modes/remaining/SDK/source-binary/汇总 manifest 同目录。B505 后端保留于 `baseline/librenderdoc-b505.dylib`。包装脚本增加36模式和三个 heap-AS 反例 gate；验收只计实际运行子步骤，不重复跑整个包装器。

当前 AS 闭包只接受 direct TLAS kind5；还不能把之前已经支持捕获的间接 TLAS kind9/10/11 用于 IR header。下一优先用真实转换样例补间接 TLAS 的 typed 子 AS/冻结实例/贡献关联，以及有格式的 buffer view；kind9 使用已冻结72-byte实例与明确 childGPUIdentities，可复用现有 `ConvertMetalASIndirectInstances` 转成保留UserID的68-byte语义后检查IFT，不把72-byte输入当64-byte direct数组。kind10零count与kind11全masked无子AS需独立miss-only native证据、贡献读取边界及坏身份控制，不能简单放宽kind白名单；随后接入可靠 UE shader/header/library/export/function/PSO 和 descriptor factory 关联。heap CBV/UAV、typed texture-buffer view、嵌套 UB、大 heap、Private/GPU IR 与动态 heap、callable/render RT 仍缺，SDK reflection 也不提供直接索引 heap 的实际成员，不按整数或最近调用顺序推断。

实际 UE RT/离线、75份旧 RT 全覆盖、集中308、原 Qt sizeHint 复现、ARC 父对象提前释放与独立旧 descriptor-table 十反例本批未跑。B495系统冻结根因未修，不直接重跑旧长 UE/full followup。官方 sample 和必要回归不能替代实际 UE；未达到持久启用门槛，生产能力 false，持续目标 active，未提交/推送。AS 内部查看、RT shader 单步和 RT Pixel History 不在支持承诺内。
