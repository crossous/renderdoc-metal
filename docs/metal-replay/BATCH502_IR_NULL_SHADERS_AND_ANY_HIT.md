# BATCH502：UE shader identifier、空 shader 与转换 any-hit

2026-10-06，接续 PHASE59/B501。真实 fixture 暴露并修复两个 IR record 语义错误：UE 的 `pad0=~0ull` 是未使用标量，不能按零保留位拒绝；VisibleFunction 模式的 `intersectionShaderHandle` 是 VFT 的 any-hit/intersection 索引，不能当作几何 IFT 索引累加。补空 miss/closest-hit 的原生行为与输出验收。生产 `supportsRaytracing`/render 能力仍 false。

## 对照依据及实现

仓库 DX12 `renderdoc/data/hlsl/raytracing.hlsl` 的 shader identifier/export lookup 与空 identifier 分支（raygen 无效，其他阶段允许空记录），以及 `D3D12RTManager::PatchRayDispatch` 的显式依赖关联；VK `Serialise_vkCmdTraceRaysKHR` 的 typed SBT/native forwarding 保持原有参考原则。没有扩大 AS 内部查看、RT shader 单步或 Pixel History。

本机 UE5.8.3 `MetalRayTracing.cpp` 的 `FMetalShaderIdentifier` 构造写 `pad0=~0ull`，Null identifier 使用零 shader/intersection/sampler，默认 miss/hit record 可为空；any-hit-only hit group 允许无 closest-hit。`AllShaderFunctions` 按 `intersectionShaderHandle` 填 **VFT**。Apple `ir_raytracing.h` 明确区分 VisibleFunction、IntersectionFunction 和函数地址三种模式，并将 `pad0` 标为 Unused。上游 UE/converter/runtime 未修改；转换器与测试 shader 沿用已审查来源及许可。

`metal_ray_ir.cpp` 保留 pad0 任意标量 bytes，允许 miss/closest-hit shader=0，raygen 仍必须非零。hit 的第一 word 非零时验证 VFT slot；其他 region 仍要求该字段为零。几何交叉函数只验证 `instance.IFTOffset + geometry.IFTOffset`，不再加 record 的 VFT 索引，避免拒绝正确 any-hit 或访问错误函数表。

新增 `metal.rayIRShaderRole`：coverage3 的背景捕获客户端对 immutable compute PSO function handle 声明 UInt32 标量角色，0=raygen、1=miss、2=closest-hit、3=any-hit/intersection。native `FunctionTypeVisible` 本身无法区分 DXR 角色。handle 必须属于已声明 IR PSO、stage0、Visible 函数；最多256条。相同声明幂等，冲突拒绝，capture 只写4条。追加 `MTLFunctionHandle::DeclareRayIRShaderRole` chunk（旧编号不变，Max1430），扫描阶段拒绝 frame/重复/空/非法角色，加载阶段检查类型/owner/出生顺序。声明了角色的 PSO 每个非零 SBT shader 都必须匹配；非零 any-hit 始终需要角色3证据，不能把已绑定的 raygen/closest-hit 当 any-hit 执行。旧 B501 无角色且无 custom any-hit 的 capture 兼容，并在当前库实际复验。

范围仍是 B501 的 static Shared schema1、两个直接根、32-byte records、stack2、无 local root/static sampler/heap/callable。role3 的自定义 procedural intersection 转换路径本批未测试；不把 any-hit 通过计为所有 intersection 通过。

## 最终库与实际结果

backend 与 app bundle SHA256：`28d2a162410bd507abf20f5f66f73ebda5e66d0edbebf245a76eb1d142ba3e3b`。GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。串行 cmd/app 构建、Python/Bash 语法和 diff 检查通过。所有 GPU/native/capture/replay 串行，Metal validation 开启，无 UE/Qt 进程参与。

| 模式 | native/capture/replay 的两个 uint32 输出 |
| --- | --- |
| default / ue-pad / pattern-pad | 73 / 11 |
| null-hit | 7 / 11 |
| null-miss | 73 / 7 |
| ue-null-both | 7 / 7 |
| ue-any-hit / ue-any-hit-no-closest | 11 / 11 |
| ue-any-hit-null-miss | 7 / 7 |

九组均执行真实转换 `TraceRay`：any-hit shader 调用 `IgnoreHit()`，命中光线转为 miss，验证的是实际函数调用及结果变化。每组16事件三方向/EID0，合计 **432 次事件选择**；slot3 参数绑定、SBT函数索引/nullable 字段/完整 pad bytes、AS/BLAS/ABI/table read 与输出 write usage 通过。每组保持额外原生 buffer/AS 分配，均观察 **AS ID2→3、SBT VA90194339072→90195584256** 且输出正确。每组 CLI3 loops 通过。编译取证为 RaygenIndirection linked5/stack2，9组各1 begin/1 native end；default 0.415ms，仅证明该微样例编译，未定位旧 UE 卡死函数。

**69 个损坏 capture 正常退出拒绝，7 个合法控制通过**。反例新增 wrong VFT role、缺 any-hit 角色、错误对象类型/未知/零 handle、角色越界/重复/帧内/出生前声明等；保留 B501 packet/GRS/SBT/AS header、几何偏移、CPU快照变更、区域越界和输出覆盖反例。signal/timeout 不算通过。合法控制包括 stride32、UE pad、pattern pad、空 miss/hit、any-hit、旧无角色 contract；CLI 和 API oracle 均通过。

当前库相关回归：18份旧 targeted API/CLI、41个旧 argument 反例、6份 B500 packet API、B501旧 IR API/CLI3、旧 raw IR 预期明确拒绝、6项 capability 查询通过。官方 Apple MIT triangle/procedural sample 重新 native重复/capture/replay逐字节输出一致；44/46事件三方向共270次、CLI各3 loops、10尺寸查询与41坏sample通过。t35、新 any-hit-only IR、官方两份capture **4×10 生命周期通过**，resident growth **1900544 bytes**，exit0，backend起止hash一致。这不是 arbitrary ARC 父对象提前销毁证明。

## 失败及可复现产物

第一轮 malformed 测试把“role 在 handle 出生前”错误放到 DriverInit 前，先由文件格式校验拒绝；该轮测试判定 FAIL，不计通过。修正为放在对应 handle creation 之前，完整69拒绝+7合法控制重跑通过。原失败日志、RDC/XML/ZIP 保留在 `final/invalid/`；最终证据是 `final/invalid-corrected/manifest.json`。未发生 GPU timeout/signal。

- `captures/metal-ray-b502/final/<mode>/ir-runtime_capture.rdc`：九组 capture。
- `build-macos-debug/metal-ray-b502/final/<mode>/manifest.json`：native/capture/ABI/compile/API/CLI、工具/源码/产物 hash；对应 DXIL、六 metallib 和 helpers。
- `metal-ray-b502/modes-manifest.json`、`native-proof/manifest.json`：九组结果及早期纯 native 语义证明。
- `final/invalid-corrected/manifest.json`：69坏输入+7合法控制及日志；`invalid-corrected-gate.log`。
- `final/remaining-manifest.json`：旧回归、官方sample、能力与生命周期；`final/apple-sample/gate-results/` 和各检查日志。
- `final/manifest.json`、`final/source-binary-hashes.json`：批次范围和最终源码/文档/二进制/hash。

复现入口 `util/buildscripts/scripts/test_metal_ir_ray_macos.sh` 已串行扩为九种模式，再用 default capture 验69反例和7合法控制；build/result/capture 目录和 skip-build 沿用已有环境变量。此脚本各子命令在本批单独串行实际运行；扩展后的整个 shell 包装器未再次整轮执行，不另计一次验收。

## 未运行与接续

本批没有运行 UE 实际 RT、75份旧 RT 完整覆盖、集中308份、旧显式descriptor table独立10反例、原 Qt sizeHint 操作或 ARC 提前释放。B501 上一库对应证据不能继承到新库。B495 WindowServer/MTLCompilerService 停滞及强制重启取证仍是未修复根因；不直接重跑旧长 UE/full followup。

下一实际开发点是扩转换 fixture 的 hit/miss local root/static sampler 与显式 typed record 数据，再补多根/descriptor heaps及 Private/GPU执行点 IR来源，以输出和损坏 capture 证据决定支持范围。UE 对象布局/生产接入和实际 dispatch/replay 尚未闭环，能力保持关闭。持续任务 active，未提交/推送。
