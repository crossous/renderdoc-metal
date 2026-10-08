# B505：直接索引 IR 资源与 sampler heap

接续PHASE59/B504，实际实现PSO明确描述的descriptor heap资源闭包与重定位，真实转换TraceRay直接使用两个nonzero heap。生产compute/render RT仍false，实际UE未运行。

## 对照和接口

参照DX12 `Serialise_SetDescriptorHeaps`（`d3d12_command_list_wrap.cpp:1000`）、`Serialise_SetComputeRootDescriptorTable`（`:1701`）与 `PatchRayDispatch`；`renderdoc/data/hlsl/raytracing.hlsl:170–212`按已知sampler/resource heap范围、stride和root paramOffsets重定位。Vulkan `Serialise_vkCmdTraceRaysKHR`继续约束明确SBT区域。Metal IR特有24-byte entry用来源已知的类型关联，第三word metadata按标量保留，不当第三个GPU ID，也不通过值大小推断资源类型。

新增 `metal.rayIRHeapEntry`，对已声明IR dispatch的compute PSO调用UInt64 `[heap,index,kind,bytes]`：heap0资源/heap1 sampler；kind0只读raw buffer SRV、kind1 texture、kind2 sampler。资源heap拒绝sampler，sampler heap仅接受sampler；index<2730（64KiB backing内），每PSO≤1024声明；raw view bytes1–64KiB，texture/sampler bytes必须0。合法重复幂等且不多写chunk；冲突/duplicate/frame/错误对象/未知类型拒绝。追加chunk1432 `MTLComputePipelineState::DeclareRayIRHeapEntry`，Max1433。

调度前packet ResDescHeap/SmpDescHeap分别按明确声明解释；有地址无声明、有声明无地址、错heap范围均拒绝。raw entry `[bufferVA,0,byteSize]`，地址4对齐、metadata严格等于声明view bytes、实际backing范围足够；不支持typedBuffer flag、textureViewOffset或texture-buffer ID。texture `[0,textureID,0]`，sampler `[samplerID,0,0]`，typed身份对象必须可用。每种entry必须有相应schema1或2 GPU字段声明。

覆盖整个静态heap backing的非零内容：零洞/分配padding保持0，最高已声明slot后的额外live数据同样拒绝，不能删掉末端声明后隐藏仍会被shader访问的descriptor。全部heap backing/目标buffer/texture/sampler加入CS read usage，UAV不能覆盖它们。EID0恢复初始字节后再typed重定位；帧内实际CPU改写静态IR输入拒绝。旧无heap的IR样例继续兼容。

当前只支持Shared静态独立backing≤64KiB；heap AS/CBV/UAV、typed/texture-buffer view、nested UB、Private/GPU来源、动态heap和大heap尚未实现。不是完整UE资源heap支持。

## 真实样例

使用此前审查的UE5.8.3 DXC→Apple converter/runtime与自产MIT HLSL，转换profile lib_6_6，root flags **0x400|0x800=3072**。资源heap四个24-byte槽：slot0 raw SRV（buffer+4，值17，view bytes4）、slot1 R32Float texture（像素3/9/15/21，线性采样得到6）、slot2零洞、slot3 raw SRV（同buffer+12，值31，view bytes4）。sampler heap两个槽，slot0零洞、slot1线性clamp sampler。shader用DispatchRaysIndex动态选择resource slot0/3，texture slot1与sampler slot1实际影响输出：第一条ray heap贡献23，第二条37。

| 新模式 | native/capture/replay两个uint32输出 | 事件/方向 |
| --- | --- | --- |
| descriptor-heaps | 96 / 48 | 20 |
| ue-descriptor-heaps-any-hit | 34 / 48 | 20 |
| ue-descriptor-heaps-null-hit | 30 / 48 | 20 |
| ue-descriptor-heaps-null-miss | 96 / 44 | 20 |
| ue-descriptor-heaps-local-root-six-samplers | 154 / 280 | 25 |
| ue-descriptor-heaps-local-root-six-samplers-any-hit | 266 / 280 | 25 |
| ue-global-descriptor-heaps-local-root-six-samplers | 1365 / 1491 | 30 |

新增7模式480次事件选择，B504原20模式全部重跑1155次，最终 **27组/1635次** API事件三方向/EID0、slot3 packet绑定、完整record/scalar/pad/资源usage和每组CLI3loops通过。保留额外原生1MiB buffer、未建AS与texture，AS ID2→3、SBT VA及各texture ID变化均实际观察；heap texture、global/local texture分别按ResourceId核验，组合不是误拿最后一个texture ID。

每模式保存SDK实际root signature与四export reflection JSON、DXIL/六metallib/helper hash。28份heap模式entry/stage/GRS布局及root flags3072 CPU核验通过。SDK `ResourceCount=0xffffffff`仍是sentinel；reflection UsedResources/TopLevelArgumentBuffer没有直接索引heap的实际成员，只有GRS参数，因此不能靠SDK reflection自动发现这些heap slot。该证据没有赋予额外replay权限，生产接入须从已知factory/descriptor写入过程传递类型。

## 最终验收

backend/bundle SHA256：`1b40caee6ba6cbef4b7417d33928ca4e0dae36a6b42659794ef4f8ef9b64d682`；GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。cmd与app构建依次通过，所有最终GPU/构建测试串行，无UE/Qt参与，起止hash一致。

新heap反例在heap-only和global+local+heap两份capture各 **55拒绝/3合法控制**：PSO身份/heap命名空间/kind/index/view shape、重复/frame/缺slot（包括末端slot）/缺GPU字段、null/swap/unaligned/未知heap、buffer view范围/flags/metadata、texture/sampler未知ID或metadata、零洞污染、UAV与所有heap输入别名、commit改写。合法控制分别一致sampler ID、texture ID与buffer VA重编号，CLI/API完整输出与事件oracle通过；重编号同步修订所有typed引用，保留metadata。

原default69/7、两local各58/4、两global各70/3重新通过，最终typed IR **435正常拒绝/27合法控制**。正常非零exit且匹配预期理由才算拒绝，signal/timeout不计通过。当前库18旧targeted API/CLI、41旧argument反例、6份旧B500 packet、旧B501与B502 any-hit IR API/CLI、无声明raw IR预期拒绝、6项能力/精确probe查询通过。

官方Apple MIT triangle/procedural重新native重复/capture/逐字节输出/API270事件/EID0/CLI各3、10尺寸查询/13坏sample通过；固定ZIP SHA `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94`。t35、新global+local+heap及官方两scene **4×10重开**通过，resident growth **1769472bytes**、exit0、最终库hash不变；不等同于ARC父对象任意提前释放测试。

Python/Bash语法和diff检查通过。新的heap gate使用与其他IR gates同一进程生命周期锁；CPU --help探针持锁时阻塞，释放后退出0，未执行GPU，防止这些IR gates独立运行时重叠。没有重复执行整轮shell包装器，仅其子步骤实际串行运行；不重复计验收。

## 失败、产物与下一步

首次cmd编译只漏更新MetalChunk::Max名称断言（1432→1433），`build-cmd.log` FAIL保留；更新断言后的 `build-cmd-corrected.log` 和 `build-app.log` PASS。新增chunk本身已有完整name/core分发。`native-proof/`三组原生proof与 `first/`首次完整闭环/反例通过保留，不加到最终27组计数。此前B504重叠问题本批未再出现。

最终产物：`captures/metal-ray-b505/final/<mode>/ir-runtime_capture.rdc`；`build-macos-debug/metal-ray-b505/final/<mode>/manifest.json`及DXIL/metallib/root/reflection、ABI/compile/API/CLI；`modes-manifest.json`；default/两local/两global/两heap共7个invalid目录；`heap-SDK-evidence.json`（28项）；`remaining-manifest.json`、官方 `apple-sample/gate-results/`、生命周期日志；`final/manifest.json`、`source-binary-hashes.json`。`heap-lock-check.json`记录CPU互斥。旧B504 backend保留于 `baseline/librenderdoc-b504.dylib`。

下一用实际heap AS和有格式的buffer view样例扩大闭包，再在隔离MetalRHI producer建立已知shader/header/library/export/function/PSO自动关联；SDK反射无法填补runtime descriptor类型。继续补nested UB、大heap范围、Private/GPU执行点与动态descriptor，然后实际UE调度/输出/重放/事件/资源绑定及最终回归。UE普通shader离线转换的限制见B504审查，不按最近调用顺序猜关联。

本批未运行实际UE RT/离线、75份旧RT完整覆盖、集中308、原Qt sizeHint复现、ARC提前释放、独立旧descriptor-table十反例。B495系统冻结根因未修，不直接旧长UE/full；能力false、持续active、未提交/推送。AS内部查看/RT shader单步/RT Pixel History不在承诺内。

B506按原始 `final/official-invalid.log` 纠正官方反例计数为13；41属于另行运行的旧argument反例，未重复计入官方sample。
