# BATCH503：转换光追的 local root 与 static sampler

2026-10-06，接续 PHASE59/B502。补齐 shader record 的局部 CBV、buffer SRV、32-bit constants、texture descriptor table 与 static sampler table，并以真实转换 TraceRay 输出完成 native→capture→replay。当前仍需要客户端明确声明 IR ABI；没有把微样例注解当成通用 UE 支持。生产两项 RT 能力仍 false。

## 对照及边界

DX12 `d3d12_manager.cpp::D3D12RTManager::PatchRayDispatch` 与 `renderdoc/data/hlsl/raytracing.hlsl` 的 export→local root association 保留 scalar gaps，并逐个 patch root VA/descriptor handles；VK `Serialise_vkCmdTraceRaysKHR` 保留显式 SBT 范围/stride/native dispatch。Metal converter 的公共 `IRCompilerSetLocalRootSignature`、`IRShaderIdentifier` 和 `IRDescriptorTableEntry` ABI 是本批依据，不猜任意整数。

本机 UE `MetalRayTracing.cpp::SetUniformBindings` 把 uniform buffer VA 写到 identifier/系统参数之后的 record；`MetalStaticSamplers.cpp` 建六种 nearest/linear、repeat/clamp、nearest/linear mip 组合，并使用 `IRDescriptorTableSetSampler`。该 runtime 将 sampler ID 放在 24-byte entry 的 **第一个 word**，第二个 word 为零，第三个 word 是 scalar LOD bias；texture entry 为零 buffer VA、texture ID、scalar metadata。不能把它们当作普通 pointer/texture/sampler 三资源 packet。

本批复用 schema0 的 VA、schema1 的 buffer/texture 两 word、schema2 的 sampler ID，分别以 stride24 声明 IR 表项，保持第三 word 为 scalar，不增加猜测型 schema。当前验证的是 **texture metadata0 / sampler bias0**，正好覆盖此 UE static sampler 构造；非零 metadata/bias 明确拒绝，未承诺通用支持。

## 实现

追加 `MTLFunctionHandle::DeclareRayIRLocalRoot`（旧 chunk 编号不变，Max1431）。背景 capture 的 `metal.rayIRLocalRoot` UInt64[4] 是 record offset/kind/count/target bytes，挂到 B502 已声明 DXR role 的 immutable compute function handle。kind0=read buffer SRV VA、1=32-bit constants、2=texture IR table、3=static sampler IR table、4=CBV VA。identifier 中 static sampler 指针固定 offset16，其他局部参数从32开始；每函数最多16参数、每参数最多64项，record field≤4096、source Shared独立 buffer≤64KiB。指针字段8-byte对齐；CBV VA按256对齐、buffer SRV按4对齐；常量按4对齐。合法重复注解幂等且不多写 chunk，冲突/重叠/非法大小/未知角色拒绝。

scan 在资源加载前校验 shape/role/重复/frame；加载验证 immutable handle/PSO/function；调度前以原始 initial snapshot 核验每个 source、view extent、GPU字段声明、texture/sampler typed身份及实际 live对象。所有非零局部 payload bytes 必须由声明的常量/地址字段覆盖；未关联 root 的非零 GPU字段拒绝。常量和 unused pad 不重定位，零 alignment padding 保留。追加所有 local buffer/table/texture/sampler 的 CS read usage，并禁止 UAV 覆盖 local CBV/SRV/table/ABI。帧内实际 CPU改写继续在静态 contract 中拒绝。

SBT record stride 扩为32–4096的32-byte倍数，每 region≤64 records、完整范围必须落在已冻结 backing。raygen 仍单 record；hit stride0表示唯一完整 record（本批96-byte控制通过），不按32-byte步进误解析 local payload。与 B502 empty/null identifier 兼容。global GRS仍只有两直接根，heap/callable/多根、Private/GPU写入 IR参数与 nested UB资源表尚未实现。

## 真实 fixture 与最终库

复用已审查 UE5.8.3 DXC→Apple converter/runtime，MIT自产 HLSL。增加局部根签名：CBV b0/space1、四个32-bit常量 b1/space1、buffer SRV t1/space1、texture table t2/space1、static sampler。96-byte record 中各根从32/40/56/64开始；两份 CBV 位于同一512-byte buffer 的0/256，SRV为32-byte buffer的12/8偏移；4×1 R32Float texture像素3/9/15/21，线性采样得到6（nearest会得到9），实际 sampler/filter 影响输出。

普通局部场景命中 **100+20+5+6=131**，未命中 **200+30+7+6=243**。any-hit 调用同一组局部资源并 `IgnoreHit()`，得到243/243。六 sampler场景按 UE 同样六种 filter/wrap组合建表，转换 shader 读取 **s3/space1，即slot3**，表144bytes，仍得到131/243；不是仅创建六个无用 sampler。

backend/bundle SHA256：`285f8768647e292957c5e7129ec7887a90f86510d0d4e11603d7407c8f9e2ce6`；GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。构建和 GPU测试串行，无 UE/Qt进程参与，native/capture/replay均开启Metal validation、固定两条ray和一次dispatch，进程 timeout有上限。

| 新增模式 | native/capture/replay 的两个 uint32 输出 |
| --- | --- |
| local-root | 131 / 243 |
| ue-local-root-any-hit | 243 / 243 |
| ue-local-root-null-hit | 7 / 243 |
| ue-local-root-null-miss | 131 / 7 |
| ue-local-root-any-hit-no-closest | 243 / 243 |
| ue-local-root-six-samplers | 131 / 243 |

六组各21事件三方向/EID0=378次；B502九组在当前库全部重新native/capture/API/CLI=432次，合计 **15组/810次事件选择**。每组CLI3 loops，slot3 binding、role/record/scalar/pad、完整资源usage通过。每组API保留额外1MiB buffer及原生AS，观察AS ID2→3、SBT VA变化；local场景另保留原生 texture，观察 **texture ID1→2** 且输出一致。实际 ordinary sampler ID是29→29，不能声称原生分配已改变 sampler ID；两份合法一致捕获ID重编号控制分别 **285→29**、六 sampler slot3 **288→32**，shader采样结果仍正确，验证 sampler重定位不是整数巧合。六 sampler capture原始 IDs为29/30/31/32/33/34。compile trace 每组各1 begin/native end，RaygenIndirection linked5/stack2；仅证明这些小样例，无旧 UE冻结函数定位结论。

## 拒绝及回归

新 local-root/static-sampler 反例在单 sampler与六 sampler两份capture上各 **58拒绝+4合法控制**，合计116拒绝/8控制；B502 default反例重新 **69拒绝+7控制**，本批共 **185拒绝+15合法控制**。正常非零 exit且理由匹配才算拒绝，signal/timeout不算通过。

新反例含shape/kind/count/bytes/alignment/overlap/重复/frame、缺 root参数/缺GPU字段声明、CBV/SRV view不足或未知/零地址、texture/sampler表错类型/未知或零ID/非零第二word/未支持metadata、未声明payload padding、UAV覆盖local输入、CPU commit快照变更及stride/range不整除。合法控制含一致sampler ID重编号、扩大但仍在backing内的CBV view、96-byte hit stride0、清零null hit record；CLI与API输出/事件oracle均通过。

当前库18份旧 targeted API/CLI、41旧argument反例、6份B500 packet API、旧B501 IR与旧B502 any-hit-only IR的API/CLI3、旧无声明raw IR预期拒绝、6项原生能力/精确probe查询通过。官方Apple MIT triangle/procedural重新 native重复/capture/replay逐字节输出、44/46事件三方向=270次/CLI各3loops、10尺寸查询与41坏sample通过。t35、新六sampler IR、官方两scene **4×10 生命周期**通过，resident growth **1769472 bytes**、exit0、backend起止hash一致；不是 ARC父对象任意提前释放证明。cmd/app构建、Python/Bash语法和diff通过。

## 失败、来源及产物

中间失败均保留，不计最终通过：

- `native-proof/`：raygen转换时只设global root，converter校验整个DXIL资源集合而报未关联space1 SRV。为所有编译入口设置局部签名，raygen本身不读取局部参数，32-byte record保持合法。
- `native-proof-corrected/`：测试试图写只读 sampler.label，改为创建前写 descriptor.label；`native-proof-v2/`五组纯native通过。
- `build-cmd.log` 与 `build-cmd-corrected.log`：同一行多个SERIALISE宏使用行号导致重定义、随后两个局部变量shadow触发Werror；`build-final.log`通过。
- `first-local-invalid.log`曾把 CBV source末端（+512）当作未知地址，实际正是另一个合法buffer的起点。修正为真正未知VA，并增加独立CBV/SRV bounded view不足反例；该失败和RDC在 `first/invalid/` 保留。新测试生成时一次Python缩进语法错误已修，未执行GPU。

上游UE/converter/runtime未修改，工具/头文件版本与完整hash沿用已审查来源并由每mode manifest核验。官方Apple ZIP继续固定SHA `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94` 及MIT许可。

产物：`captures/metal-ray-b503/final/<mode>/ir-runtime_capture.rdc`；`build-macos-debug/metal-ray-b503/final/<mode>/manifest.json` 与 DXIL/六metallib/helper、ABI/compile/API/CLI日志；`modes-manifest.json`；`final/invalid/`（69/7）；`final/local-invalid-local-root/`、`final/local-invalid-ue-local-root-six-samplers/`（各58/4）；`final/remaining-manifest.json`、官方 `apple-sample/gate-results/`、生命周期/旧回归日志。`baseline/librenderdoc-b502.dylib`保留旧库；`final/manifest.json`、`source-binary-hashes.json`记录最终批次范围/库/源码/文档/产物hash。

可复现入口 `util/buildscripts/scripts/test_metal_ir_ray_macos.sh` 扩为15个模式和三个反例gate；本批逐项实际串行运行其子命令，没有再次整轮运行shell包装器，也不计作额外通过。

## 下一缺口与未运行

接着补完整多根 GRS/IR资源表、callable/SBT geometry contribution 等真实 UE必要布局，并核对 UE根签名/shader export→Metal function/record 的可观测关联与自动捕获接入；显式 fixture注解不能替代生产 UE。继续以真实小样例输出、typed范围和坏capture证据开发，再进入现有 UE实际调度验收。

本批未运行UE实际RT/离线、75份旧RT完整覆盖、集中308、旧独立descriptor-table十反例、原Qt sizeHint场景、ARC父对象提前释放。Private/GPU执行点IR来源、嵌套UB资源字段、一般texture/sampler metadata、RT render、AS内部查看/RT单步/Pixel History未支持或不在承诺范围。B495系统冻结根因仍未修，不直接旧长UE/full followup。能力false、持续任务active、未提交/推送。
