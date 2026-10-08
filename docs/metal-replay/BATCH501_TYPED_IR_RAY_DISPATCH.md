# B501：完整 typed IR 参数与首次转换 TraceRay 重放

2026-10-06，PHASE59。B500 的转换 DXR 样例原先只有 native/capture 73/11，离线被 raw 地址 guard 拒绝。本批补齐明确声明、完整 ABI 依赖与恢复，首次通过真实转换 RaygenIndirection 的离线输出与事件验收。**生产能力仍关闭，UE 实际调度尚未验收。**

## 接口与实现

先查仓库 DX12 `d3d12_command_list4_wrap.cpp::Serialise_DispatchRays` / `d3d12_manager.cpp::PatchRayDispatch` 和 Vulkan `wrappers/vk_draw_funcs.cpp::Serialise_vkCmdTraceRaysKHR`。沿用明确的 SBT 区域、根参数/资源依赖和 shader identifier 关联；不尝试 AS 内部解码或 shader 调试。Metal converted ABI 和分别重建的原生函数表 ID 是必要的 Metal 语义。

新增附加 chunk `MTLComputePipelineState::DeclareRayIRDispatch`，旧 chunk ID 不变（Max1429）。`metal.rayIRDispatch` 注解为 UInt64[packet对象,offset,schema1,rootCount2]，仅帧前、coverage3、Shared源；schema1 根0是 AS header SRV，根1是 uint32 UAV。已有显式 descriptor layout 新增8-byte schema4=AS、5=IFT、6=VFT，只有完整 IR 声明才允许。旧 raw capture 仍拒绝，没有按字节扫描或全局放开能力。

`metal_ray_ir.cpp` 核验全部152-byte dispatch字段，SBT地址/size/stride，GRS两个根，64-byte AS header与instance contribution、AS初始配方、primitive子AS、geometry/instance/record的 IFT索引，以及函数表 owner PSO、已填充索引与 null/范围。32-byte identifier 的函数索引保持不变，所有明确 GPU字段重定位到正确类型的 live对象。当前无local root/callable/descriptor heaps/非空static sampler；record/header保留位仅验证本样例的零值。最多4096 rays、64 instances、每区域64 records，Shared独立源≤64KiB，hit stride0或32。越界、未声明字段、缺失初态和输出覆盖 ABI/构建输入均拒绝。

dispatch前完整验证，声明无效不能退回一般compute路径。函数表重放保留已填充handle影子；声明IR管线仅接受maxCallStackDepth2，普通管线仍1。帧内AS/table变更和实际ABI CPU变更拒绝；commit自动生成的原样 coherent snapshots允许。EID0重置恢复初始字节并重新重定位。登记AS/BLAS、函数表、packet/SBT/GRS/header/contributions的CS read与结果UAV的CS write。

样例暴露第二个真实缺口：compute `useResources(TLAS)`只引用根AS，遗漏BLAS初态，导致重建失败。共用资源引用 helper现在复用直接AS绑定的 `MarkASInitialReferences`，保留primitive依赖与构建输入；capture已包含BLAS和TLAS两份初态。另将初态恢复错误分为CPU/Private/texture/AS/view，便于定位，顺序不变。

## 最终库实测

最终backend与bundle：`10c28bf1b00f9deeb6e87564f49aa3c4980185c44849d0a307df837ff4266916`。GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。

- 新可复用串行脚本 `util/buildscripts/scripts/test_metal_ir_ray_macos.sh` 完整PASS。DXC→Apple converter、native/capture真实TraceRay **73/11**，CPU完整ABI与native编译审计PASS；RaygenIndirection linked4/stack2，1begin/1end，0.442ms、native thread301411。
- API：16事件×3方向/EID0，slot3绑定和完整资源usage/SBT索引PASS。保持1MiB buffer及一个未构建原生AS占位，**AS GPU ID实际2→3，SBT VA90194339072→90195584256**，输出仍73/11。证明不依赖原进程整数巧合。CLI3 loops PASS。
- **54坏capture**正常退出拒绝（无signal/timeout算通过）：声明、缺字段、coverage、stack1/3、区域/stride/grid、null/unknown函数表或索引、AS种类/初态、contribution、geometry IFT范围、CPU快照变更和UAV覆盖等。合法hit stride32控制的CLI及同样强制分配顺序/API48次选择PASS。坏capture保持导出可用。
- 官方Apple MIT sample当前库重新验收：triangle/procedural native重复、注入输出逐字节一致、44/46事件各三方向（270次）/CLI各3 loops；10尺寸查询、41坏sample（13+28）、6能力查询PASS。默认compute/render均0，精确probe=1只按原生值放行compute。
- 旧18份targeted API/CLI、41旧argument反例（27+7+7）、六份B500 BLAS/TLAS参数capture的API事件/输出PASS。旧无声明IR仍正常拒绝。旧schema0–3显式table的GPU122/161/12 seeks/像素、合法sampler alias与10反例API+CLI且无GPU提交PASS。
- t35、新IR、两份官方capture **4×10** reopen/close PASS，resident growth **1343488 bytes**，exit0、起止backend hash一致。不是任意ARC父对象提前销毁验收。
- 串行cmd/app构建、Python/Bash语法及diff检查PASS。无UE/Qt进程参与；没有全量压力测试。

## 来源、失败与产物

来源沿用B500已审查的本机UE5.8.3 DXC/Apple converter/runtime，原生HLSL fixture为MIT；外部工具、实际header、自产DXIL/metallib/helper与源码哈希见最终manifest/hash清单。官方ZIP继续核验MIT许可及固定SHA `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`，没有修改上游文件。

中间失败全部保留，不算最终通过：最初PSO被stack1限制拒绝；缺Shared creation初态快照；误拒绝原样commit快照；TLAS缺BLAS初态。补共用引用helper时一次不完整类型编译失败，随后一个gate错误地在旧库上运行、仍失败；两者日志保留。补BLAS后CPU审计按64-byte大小猜header，碰到同长instance buffer，改按GRS typed来源定位。坏capture脚本最初假定直接绑定packet查询过VA，KeyError发生于任何反例前，改用实际查询的SBT。旧table首次误传native日志地址而非capture日志，修正后旧oracle又不认schema4的新明确拒绝原因；扩展其精确错误分类，仍要求API/CLI拒绝且无GPU提交，最终十组通过。

`baseline/`保留03fc及bfe3库；本批旧中间证据仍在`build-macos-debug/metal-ray-b501/`，不能与最终10c28证据混用。最终产物：

- `captures/metal-ray-b501/final/ir-runtime_capture.rdc`
- `build-macos-debug/metal-ray-b501/final/ir/manifest.json`、完整ABI/compile/API/CLI日志与DXIL/五metallib/helper
- `final/invalid/manifest.json`（54拒绝+1合法控制）、所有变体RDC/XML/ZIP/日志
- `final/remaining-manifest.json`、官方`apple-sample/gate-results/`、旧targeted/packet/argument/table反例与生命周期日志
- `final/source-binary-hashes.json`与批次manifest，最终库/GUI/工具/测试及文档哈希

## 接续与未运行

下一实际缺口来自UE `MetalRayTracing.cpp`：shader identifier `pad0=~0ull`、Null identifier，然后局部根参数/static sampler、完整GRS/descriptor heaps和GPU来源。先扩固定转换fixture，以native/capture/replay输出驱动；本批schema1不能冒充完整UE布局。

UE实际RT输出/离线、75份旧RT全覆盖、集中308份、原Qt sizeHint crash、ARC父对象提前销毁、render/嵌套/Private或GPU写入IR参数未验或未支持。死机根因仍未修，B495取证与有限诊断约束有效；不要直接旧长UE/full followup。持续任务active，未提交/推送。
