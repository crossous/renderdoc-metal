# 通用 API 恢复整改（2026-10-07，覆盖旧场景驱动策略）

下一阶段先完成一组通用资源恢复与重定位机制，再追逐 Lumen 新案例。UE/Lumen 只作实际光追验收负载；不以功能名、pass/shader 名、PSO hash 或固定 EID 决定生产支持。名称/EID 仅用于测试定位。根 AGENTS.md 与持续任务 metal 已同步此要求；旧文档逐项增加 coverage/格式/尺寸许可的下一步被覆盖，历史结果保留。

## 持续规则

实际审计并修改 `ValidDescriptorFrameTexture`、`ProjectDescriptorFrameTextureView`、`m_DescriptorCoverage` 和 runtime AIR preflight，不能止于注释或清单。按已支持 API 类型与格式族恢复，不积累格式/尺寸/mip/view/usage 组合许可。coverage 用于捕获协议字段兼容，不作为不断增长的场景许可等级。保持 API/设备合法性、溢出、资源生命期、真实初态、缺失资源/生产者和统一预算；缺机制就补机制，不仅提高上限。

分离资源恢复、地址重定位、提交依赖与 shader 访问展示。恢复完整时 GPU 动态计算由原生 shader 执行，展示不足报告 partial/unknown；不能仅因此永久拒绝，也不能假装整个驻留 heap 都已被实际访问。未恢复 GPU 指针、失效初态、缺资源或生产者必须修复或明确拒绝，不能用旧字节或 CPU 计算的 shader 输出替代。

每个实质修改先读本地 DX12/Vulkan 具体函数，记录捕获、初态、重定位、提交依赖，说明实际复用及 Metal 必要差异。两者都不支持的调试能力保持不支持。独立 native sample 在声明范围内改变尺寸、格式、view、偏移与动态索引，合法同类变化无需再次修改后端。以较大能力批次开发：必要定向验证，稳定候选集中相关回归，再 UE。报告移除的限制、剩余依据与实际 PASS/FAIL/未运行，不以案例数或全量重跑次数作为进度。

光追开启门槛不变：官方 sample 和实际 UE 必须确实执行光追，输出、重放、事件往返/EID0、资源绑定、相关回归与原生设备能力通过后才开启对应能力并复验。RT 保持黑盒。构建/GPU 串行，共享锁和既有资源监督；中间产物继续在已挂载外盘，保留最终哈希与失败证据，不覆盖用户改动、不重置、不自动提交推送。

## 本组修改与约束分类

| 检查 | 分类及本组处理 |
|---|---|
| ValidDescriptorFrameTexture 的 R16Float/R16Uint/packed/array/volume 等按 coverage、尺寸、usage 分支 | 历史场景限制；删除组合分支，统一 `MetalTextureReplayLayout`，依照已有 block/plane 格式族与类型计算全部 mip/slice 的逻辑大小 |
| ProjectDescriptorFrameTextureView 的仅 R16Float、多 mip parent 特批、只允许单 mip 2D | 历史场景限制；改为共享 `ProjectMetalTextureView`，投影合法 mip/slice 区间、类型、尺寸、arrayLength/depth，工厂真实 Native 调用使用同一检查 |
| 帧 buffer/table/plain-copy 按 coverage 扩增容量 | 历史许可等级；改为固定、统一的 staging/table/copy 预算；不改变真实对象身份或 copy 当前字节恢复检查 |
| runtime dispatch 4/128 与 command 2/256 数量许可 | 历史场景上限；改为整个 frame 的 encoded bytes + 每 chunk 4096-byte 记账预留，共 128MiB，保留已有 GPU 总工作量/每 dispatch 工作量及线程组检查。此记账预留是保守策略估算，不冒称精确 RSS 上界 |
| mip/slice 范围、合法类型、sampleCount、known usage、Native heap alignment/extent、device sample support | API/设备约束及当前实现范围，继续保留；Native 工厂失败不能算支持 |
| GPU identity 不匹配、typed source 缺失、未出生/已 aliasable、初态或提交生产者缺失 | 资源/地址/生命周期/提交恢复约束，继续拒绝；不借展示 partial 绕过 |
| 8192×8192、depth256、array128、logical128MiB、Private/Tracked placement | 初态上传/读回与当前资源恢复的统一实现/预算边界，覆盖所有格式组合；不是设备最大能力声明。aggregate native allocation 继续由 MetalReplayAllocationBudget 按真实 heap/backing 计算一次 |
| AIR unknown 数值/分支与访问区间 | 仅展示不足时已有独立 partial 状态，可 Native 重放；未知地址 namespace/外部调用的资源副作用仍需独立恢复，不直接把 unresolvedCalls/Resources 全部删除 |
| 其他 m_DescriptorCoverage 历史族分支、standalone frame texture、不同格式 reinterpretation、nested view、MSAA descriptor 路径 | 本组未全部整改。下一组按协议字段实际需要与恢复机制逐项替换，不能宣称 coverage 已完全退出资格；缺机制的路径继续明确拒绝 |

首轮 native 输出/往返已通过，但坏 capture 删除首个真实 texture producer 后仍被普通 Native descriptor 路径接受（历史失败证据）；这是本次实际发现的恢复缺口，不归类为展示不足。当前补齐 Native argument-buffer dispatch 的实际 inline/root/typed slot 快照，按与 runtime 路径相同的提交顺序做资源/producer 恢复验证，不将 write residency 当成实际写入。首轮 FAIL 和后续修复结果分别保存，未修复验收前不标整组 PASS。当前独立 Native 变体已补齐并重新验收，稳定候选相关回归另列，不覆盖历史 FAIL。实际发现 Native stride=16 声明未进入仅 stride=8 的恢复快照；现按声明 stride 保存。Native argument-buffer 第三字段可以是用户 metadata，不能借 converted IR byte-count 规则误判；保留 Native allocation/source/access 的真实范围、只读与身份校验。Native texture read 需要实际 shader/clear producer 或初态，useResource(Write) 只表示驻留许可，不再制造像素初态。

本组共享布局在 descriptor preflight、single-sample placement factory 和 same-format Native subset view factory 实际使用。移除了 factory 对 allowGPUOptimizedContents=true 和 identity swizzle 的历史强制要求，保留已知 swizzle enum；非 identity swizzle 的完整 native/capture 验收尚未运行，不宣称已验。原有 depth/stencil aspect 与 BGRA sRGB→linear Native 分支保留。新的 generic projection 首批声明并验收重点是单采样颜色 2D、浮点格式族、多 mip 同格式 views；其他类型需要独立 native 变体证据，不能只凭函数接受就声明通过。

## 实际阅读的 DX12/Vulkan 对应实现

- 捕获/创建：`WrappedVulkan::Serialise_vkCreateImage`（vk_resource_funcs.cpp）序列化完整 CreateInfo、ResourceId 和 memoryRequirements，重放增加调试读/拷贝 usage、重映射 queue families，并按 Native API 创建；`Serialise_vkCreateImageView` 序列化 view CreateInfo，unwrap 已恢复 image，Native CreateImageView 后记录创建元数据。`WrappedID3D12Device::CreateShaderResourceView`（d3d12_device_wrap.cpp）先调用 Native 创建，再将 volatile DynamicDescriptorWrite/ResourceId 捕获入帧，并标记帧引用。Metal 保留完整描述/原 API chunk 与 Native placement/view 创建，资格来自格式族和子资源范围，不来自样例许可。
- 初态：`WrappedVulkan::Prepare_InitialState(eResImage)` / `Apply_InitialState(eResImage)`（vk_initstate.cpp）依据 bound memory、subresource layouts/queue ownership 与 init policy 捕获、恢复；全 undefined 且非 external 不伪造数据。`D3D12ResourceManager::Apply_InitialState`（d3d12_initstate.cpp）按 mip、array 和 plane 数，GetCopyableFootprints 后逐 subresource CopyTextureRegion，旧 capture 版本只影响确实序列化的 plane。Metal 复用“逻辑 mip/slice/plane 恢复，Native footprint 单独分配”思路，现有 initial uploader 深度/模板分 plane，通用布局按相同 block 与边界计算预算；帧出生资源由真实 producer 生成，未写区域保持原 Native 未定义语义。
- 身份/重定位：`D3D12Descriptor::GetRefIDs`（d3d12_manager.cpp）CBV 经 `WrappedID3D12Resource::GetResIDFromAddr`（d3d12_resources.h）确定对象+offset，SRV/UAV 保存 ResourceId；`Serialise_vkCreateImageView` unwrap 实际 image handle，Vulkan buffer device address 捕获（vk_resource_funcs.cpp vkCreateBuffer）保存 opaque capture address 以 Native 重建。Metal 纹理句柄不是可平移的 buffer VA：保留 captured GPU identity、typed factory 来源，出生后 `PatchDescriptorSlotField` 用真实 gpuResourceID 重建，buffer/root 由已恢复对象+offset 重定位；本组没有删除这些身份检查或猜测 GPU 地址。
- 依赖：`WrappedID3D12CommandQueue::Serialise_ExecuteCommandLists`（d3d12_command_queue_wrap.cpp）先 DataUploadSync，队列切换同步；`WrappedVulkan::Serialise_vkQueueSubmit`（vk_queue_funcs.cpp）按提交与等待关系重放，多队列/等待 semaphore 时同步。Metal 保留原 command/encoder/commit 顺序与 preflight live/producer/committed 集合，view 依赖关联 backing parent，alias/初态和 EID0 重置继续独立；新统一记账预算替代特定 command/dispatch 数量许可，没有移除 producer 或提交校验。
- 展示：`D3D12Replay::GetDescriptorAccess`（d3d12_replay.cpp）、`VulkanReplay::GetDescriptorAccess`（vk_replay.cpp）与 shader feedback 将展示与 Native Dispatch 分离。Metal 保留 `m_IRRuntimeDescriptorAccessComplete` 与 restored binding 独立；本组修改 runtime preflight 的预算许可，未将 AIR 未知资源/调用一概放行。

## 证据

开发中与稳定候选验收结果追加到 BATCH544、PHASE59、TEST_MATRIX、STATUS、HANDOFF。外盘目录 `build-macos-debug/metal-ray-b544/development/generic-resource-*` 保留 native/capture/replay 输出、坏 capture 和 manifest；结果未出来之前不标 PASS。最新之前 f6ebc5ee Native GPU 初态 descriptor、合法同表非重叠 GPU copy 与动态 Native sampler heap 集成已通过，UE 只完成禁用 initial/frame GPU 的加载预检，仍不代表整帧光追验收。


## Native 运行时编译与 AIR 缺失的独立路径

相关回归实际发现 `GetComputeAIR` 从公共 disassembler 得到捕获 MSL 文本，它并非 AIR 模块，不能以 AIR validModule=false 否决已经恢复的 Native shader。现在 helper 根据捕获的 library binary 来源决定 AIR 是否存在；Native source-compiled PSO 无 AIR 时使用独立的恢复/依赖快照，保留原生 dispatch，不执行 CPU shader 求值。converted IR 仍需其已捕获 ABI/指针来源及 AS 闭包，不借 Native 路径绕过。

这一修改先读 `D3D12Replay::GetDescriptorAccess` 的 ProcessDescriptorAccess 与 `VulkanReplay::GetDescriptorAccess` 的 staticDescriptorAccess；两者独立于实际 Native 调度、创建/初态恢复和 ExecuteCommandLists/QueueSubmit。Metal 复用“可选展示数据不决定 API 调度资格”，额外需要 argument-buffer typed field 与 gpuResourceID/VA 重定位，因为 Metal 纹理句柄不能当普通 buffer 地址平移。`GetComputeAIR` 与 Native 快照重用此前真实 capture/library/source 身份，未按 shader 名、EID 或 hash 分类。

对实际非零 Native dispatch，检查独立恢复的 inline/root/table、typed slot 来源及当前生存期。显式 read residency 仍要求真实初态或同 command/已提交 producer；useHeap 的 argument-buffer 资源按本次真实绑定 table 的 typed fields 收集可能效果，不遍历所有驻留 heap。可写 slot 加实际 dispatch 仅表示可能的 opaque producer，不冒称每像素已覆盖，不产生 CPU 结果；仅 useResource/useHeap 不建立生产者。AIR 不存在时清空动态纹理 access 列表并标 partial/unknown，已知直接 API buffer 绑定仍可显示，不假称整个公开列表必须为空。独立 JIT sample 改尺寸/格式/mip/view/placement/layer/dispatch，并保持完整像素、事件往返及 EID0 oracle；坏 capture 删除真实 writer 仍要求 GPU 前拒绝。

这不承诺所有非空 AIR 的 unresolved pointer/call 均已解耦；未知资源 namespace、未恢复指针、副作用或生产者仍需独立机制。相关回归首次失败和 JIT 第一次测试把“动态纹理 unknown”误要求为“所有直接 API 绑定列表为空”的 oracle 失败均保留，修复 oracle 后重新完整运行。

## Generic resource recovery first group (2026-10-07)

本组机制整改和所列相关回归已完成；B544/PHASE59整批及光追启用仍未完成。优先通用 API 机制的任务调整已写入根 AGENTS.md、持续任务 metal 和 [GENERIC_API_RECOVERY.md](GENERIC_API_RECOVERY.md)，覆盖历史按场景逐项增加许可策略。UE/Lumen只是验收负载。

实际修改：`ValidDescriptorFrameTexture` 依已支持 block/plane 格式族、类型与所有 mip/slice 计算恢复布局；`ProjectDescriptorFrameTextureView` 和 Native subset-view factory 使用同一范围投影；single-sample placement factory 共用布局。移除旧 R16/packed/array/volume/尺寸/usage 组合与 mip-view 特批，以及 allowGPUOptimizedContents=true/identity swizzle 强制条件。统一 staging/table/plain-copy 和 frame preflight 记账预算替代 coverage 容量/2或256 command/4或128 dispatch 许可；Native footprint、总 GPU 工作量、API范围/溢出/生存期/初态/alias/真实生产者检查保留。128MiB记账+每chunk4096预留是保守策略估算，不是精确RSS界限，不仅扩大数字。

Native argument-buffer 实际调度保存声明 stride（含16-byte）、真正 inline/root/table/typed slot 快照；Native metadata不误当 converted IR byte-count。生产者来自实际 shader/clear/提交关系，useResource(Write)不单独制造初态。Native source-compiled library 无捕获 AIR 时资格走独立恢复/提交检查，原 shader 运行，动态纹理 access 标 unknown；直接API buffer绑定仍可展示。只按本次绑定table收集可能 opaque效果，不把驻留heap所有资源伪造为实际访问。shader CPU数值未被模拟。

逐函数 DX12/Vulkan 证据见通用文档：vkCreateImage/vkCreateImageView完整CreateInfo/Native创建；Prepare/Apply_InitialState bound-memory/subresource/undefined策略；D3D12 Apply_InitialState footprints/mip/array/plane；GetRefIDs/GetResIDFromAddr的resource+offset；ExecuteCommandLists DataUploadSync及跨queue同步、vkQueueSubmit等待；GetDescriptorAccess与NativeDispatch独立。Metal采用相同对象/子资源/提交闭包，额外处理不可平移的texture gpuResourceID和typed argument-buffer字段。无引擎/pass/shader名称、PSOhash或固定EID生产准入。

| 结果 / 范围 | 真实证据 |
|---|---|
| PASS 通用 native 预编译 library | `generic-resource-final-native-accepted/manifest.json`，六变体：R16/R32Float，769×17/513×129/1025×9，4/5mip及多mip view，非零placement偏移，19层parent取base3/count9子视图，132实际dispatch；各native/两capture/API四次完整像素与mip/layer/PickPixel/nativeID往返/EID0、CLI loops3。 |
| PASS Native JIT / 展示未知 | `generic-resource-native-JIT-validated/manifest.json`，同六变体以newLibraryWithSource真实运行，无捕获AIR；完整输出和往返oracle不放宽，动态texture公开access不伪造。每组16种损坏capture来源/producer/format/type/usage/mip/view/offset/range均API4/CLI1且无GPUwait。源/MSL/metallib provenance附加核验不改变已运行checks。 |
| PASS GPU 初态 / 动态 Native sampler | `generic-resource-final-dynamic-sampler/manifest.json`，真实initial/frame GPU descriptor、合法同表copy、动态sampler heap、Native SIMD/纹理atomics与独立提交；两capture/72事件/EID0、输出/padding、28坏组GPU前拒绝。普通Native负载，不以其结果替代RT验收。 |
| PASS 部分 texel 初态 / 共用 root backing | `generic-resource-final-partial-texel/manifest.json`，真实16-byte初始像素前缀，其余先未定义后真实GPU生产，合法不重叠root/view共用backing；两capture/48事件/EID0，26坏组GPU前拒绝。 |
| PASS fresh converted RayQuery | `generic-resource-final-RT/manifest.json`，Native/capture/API/CLI真实4253/4253/0/0和528uint，48事件/EID0/24typed查询，Private placement64几何/TLAS/六CBV/sampler/间接1→132→0；复用既有转换shader/reflection并记录hash，不是新官方sample或整套RT。 |
| PASS related regression | `generic-resource-final-related/development-regression-manifest.json`，29旧capture/58 API与CLI：Private/placement texel、multiUAV、TraceRay、frame compute→Load、linked、depth/D16/D32/D32S8、deferred/parallel与6/8MRT。不是全量回归。 |
| UE REJECTED / INCOMPLETE | `generic-resource-UE-pre-submit/manifest.json`，原capture403f90ea，45s/3GiB监督下禁用initial/frameGPU；peak1448443904bytes，未触发监督停止，无GPUwait/严格诊断。越过旧调度数量限制，后续PSO3906/offset297152有4 unresolvedCalls，原commit4502144 API4。ID仅用于证据定位；下一先判明调用的资源/地址/生产者恢复与可选展示缺口，不按该名称或ID放行。 |

固定backend/bundle SHA256 `d907eec86fdbddf3c646a708cf890d5a045379b958c01d9d1b43e1177141ad45`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。`generic-resource-evidence.json`保存七manifest及697个所列完成日志SHA，strict诊断0，不把UE supervised driver exit0当UE成功。

开发失败保留：首轮missing-producer错误接受；stride16被漏记、Native metadata误用IR长度和write residency伪造初态的定向正/负失败，修正后producer控制GPU前拒绝；array19 Native fixture物理budget不足退出3，改sample真实footprint预算后fresh完成；旧related首次Native source-compiled AIR/validModule拒绝；Native JIT第一次oracle错误要求全部公开绑定为空退出14，改为动态texture未知且直接API绑定仍允许，重新完整验收。中间编译FAIL/旧候选e46证据各保留自身hash，不覆盖为当前PASS。

剩余分类：API/设备及范围、Native heap alignment/extent/sample支持持续校验；8192²/depth256/array128/logical128MiB是当前初态/读回统一实现/预算范围，不是设备最大值。其他coverage族分支、standalone frame texture、不同格式reinterpretation、nested view、MSAA descriptor恢复未全部改造，不继续增添许可等级。2DArray→Cube投影尚未实现；非identity swizzle本组仅删历史限制，完整fresh native验收未跑。非空AIR的未知资源namespace/call仍需独立恢复，不能直接全部删除拒绝。

未运行：UE整帧真实RT输出/事件往返/绑定验收、当前库最终官方sample、full IR/full RT/lifecycle/Qt-ARC/最终native capability集中验收、三个历史数值展示坏控制。生产两supportsRaytracing保持false，目标active。历史Qt/ForceCrash/死机重启不因本组scope声称全部修复。

中间文件与可恢复源码/文档/最终四产品检查点均在外盘 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`；内盘31GiB、外盘1.6TiB。无reset/commit/push，未编辑用户RDHeaderView。本组稳定后回到UE暴露的通用调用/namespace/提交恢复，不继续按Lumen功能新增许可。


Native调度具体对照：`WrappedID3D12GraphicsCommandList::Serialise_Dispatch`（d3d12_command_list_wrap.cpp:3530）保存原grid，在rerecord范围调用Native Dispatch及event callbacks；`WrappedVulkan::Serialise_vkCmdDispatch`（wrappers/vk_draw_funcs.cpp:1201）同样保留CmdDispatch和原维度，不要求CPU表达式分析成功。Metal保留原Native dispatch和事件，AIR仅提供可选的已识别访问；恢复资格另查typed来源/范围/生命期/初态/producer/提交。Vulkan上述image/view/queue函数的文件位于`renderdoc/driver/vulkan/wrappers/`。

稳定首组完成后的下一CPU审计：`development/generic-resource-next-effects-audit/manifest.json` 只提取实际捕获library并disassemble，无GPU/native执行/重放验收。4个unresolvedCalls对应4次air.clz.i32，声明attributes仅nounwind；现有AIR call-effects无法从缺失readnone推断资源效果。其余SIMD操作已按Native寄存器效果分类，结果仍unknown。下一通用批次处理已知Native数值API效果与未知外部资源调用的边界，以独立sample验证；不模拟clz/位运算数值、不按该shader/PSO/EID准入。

## Native API effects and resource-specific dependency recovery (2026-10-07)

仍属B544/PHASE59较大通用能力批次；未完成光追启用。接续首组纹理/view/预算整改，本轮修改两处真实机制，不按UE功能、pass/shader名、PSOhash或固定EID决定资格。

- Native register-only bit API的完整声明signature可确认资源效果，即使旧AIR省略readnone。`RegisterBitIntrinsicDeclaration` 按return/input类型、operation family和可选控制参数分类；clz/ctz/popcount/reverse_bits的scalar/vector prototype与未知外部调用、pointer参数和错类型区分。结果不计算、不产生CPU索引/地址事实。CPU测试以已知数值输入仍确认动态offset/返回未知，以及真实source identity保留；错误signature/外部call仍unknown。Native实际验证重点是uint32 clz/popcount驱动GPU索引，其他宽度/vector仅做signature CPU检查，不宣称全部Native验收。
- 通过actual typed slots、对象/offset、初态、真实producer和提交闭包恢复资源；AIR效果分类不替代这些检查。Native effect classification/prototype表为固定缓存，非bit声明快速排除；不缓存shader数值结果或生存期状态。
- CPU-only UE采样日志实际显示普通resource依赖记账在`noteResource`反复扫描全m_DescriptorSlotShadow。按其已有resource+offset有序key使用lower_bound，仅遍历该resource字段；保留live/data、borrowed/consumer、真实source及backing/alias/command引用行为。未跳过本resource字段，没有把全heap当实际shader访问。没有扩监督时限或放松预算。

### 具体DX12/Vulkan对照及Metal差异

`WrappedID3D12Device::Serialise_CreateComputePipelineState`（d3d12_device_wrap.cpp:853）保存完整PSO/shader bytes/root signature，安排Native编译及派生资源；`WrappedVulkan::Serialise_vkCreateComputePipelines`（wrappers/vk_shader_funcs.cpp:1072）保存CreateInfo、layout/module与Native deferred handle。`WrappedID3D12GraphicsCommandList::Serialise_Dispatch`（d3d12_command_list_wrap.cpp:3530）和`WrappedVulkan::Serialise_vkCmdDispatch`（wrappers/vk_draw_funcs.cpp:1201）保留原Native调度和维度；不要求CPU计算shader bit值。Metal原PSO/library/Native dispatch继续执行，只补缺AIR effect metadata的API分类，GPU结果不被CPU替代。

`D3D12Replay::GetDescriptorAccess`（d3d12_replay.cpp:1995）结合static和valid dynamic feedback，并依据本次bound heaps定位；`VulkanReplay::GetDescriptorAccess`（vk_replay.cpp:2990）从实际compute/graphics descriptor sets映射storage，独立展示。Metal partial/unknown和恢复资格继续独立，不因bit数值未知产生伪造访问。

`D3D12Descriptor::GetRefIDs`（d3d12_manager.cpp:555）按type取ResourceId，CBV由`GetResIDFromAddr`（d3d12_resources.h:1536）解析对象+offset。`D3D12ResourceManager::Apply_InitialState`（d3d12_initstate.cpp:1914/texture分支2130+）按真实mip/array/plane与CopyableFootprints恢复；Vulkan`Prepare_InitialState`/`Apply_InitialState`按bound memory/subresource/undefined策略。Metal保留typed GPU identity和真正初态，texture句柄需额外gpuResourceID Patch而不能当可平移VA；本轮不修改或绕过这些来源/初态检查。

`WrappedID3D12CommandQueue::Serialise_ExecuteCommandLists`（d3d12_command_queue_wrap.cpp:440/904–958）对动态写及本command的bound descriptor ranges获取对象引用，然后AddResourceReferences；先DataUploadSync及跨queue同步。`WrappedVulkan::Serialise_vkQueueSubmit`（wrappers/vk_queue_funcs.cpp:1283/1300+）处理原command与等待/跨queue关系。Metal复用按实际对象/表范围记录引用，保留原encoder/command/committed/producer集合以及view parent/alias检查；范围索引替代无效的跨resource全表扫描，不改变所需依赖。

### 实际验证与限制

| 结果 / 范围 | 证据与实际行为 |
|---|---|
| Baseline modern PASS / legacy FAIL | `register-effects-baseline`在旧d907库modern声明Native/capture/API/CLI通过；`register-effects-legacy-baseline`由相同独立Native fixture重组AIR，仅修改register declarations的effect属性、保持输入module指令/资源metadata，Native/capture完整输出通过，旧库API4/unknownCall2且GPU前拒绝。不是仅合成CPU报告。 |
| PASS 最终Native API effect集成 | `resource-dependency-final-dynamic-sampler-legacy`、`resource-dependency-final-partial-texel-legacy`，新鲜legacy library/Native/capture/API/CLI；Native GPU生成索引clz、SIMD、实际texture atomics、动态sampler、GPU-owned descriptor初态/合法同表copy与部分texel初态/共用root backing。两capture各72/48事件/EID0、20公开access与40location、完整buffer/pixel/padding；28/26坏组各API4/CLI1且GPU前拒绝，包括真实来源/namespace、captured VA、初态、producer、只读、alias边界。 |
| PASS 最终通用恢复变体 | `resource-dependency-final-generic`、`resource-dependency-final-JIT`各六Native变体、两capture/完整mip/layer像素/PickPixel/nativeID/四次往返/EID0/CLI；R16/R32浮点多尺寸/offset/mip/view、19层subview、132实际dispatch。JIT无AIR动态纹理展示unknown；两组16坏capture各GPU前拒绝。不将前一d907结果继承为当前库，实际重新跑。 |
| PASS fresh RT / related regression | `resource-dependency-final-RT`真RayQuery4253/4253/0/0与528uint、48事件/EID0，复用有provenance的converted shader/reflection；`resource-dependency-final-related`29旧capture58API/CLI含TraceRay、texel/multiUAV/NativeCompute→Load/linked/depth/deferred/parallel/MRT。不是full IR/full RT或官方最终验收。 |
| UE INCOMPLETE, bounded preflight completed | `resource-dependency-final-UE-pre-submit`仍用原403f90ea capture，禁用initial/frameGPU、45s/3GiB监督。约25s到API4而非超时，无GPUwait/strict，peak1623212032bytes；越过PSO3906/offset297152原4未知调用、后续graphics及更多提交。当前offset1584704/PSO3993是同一buffer2511读0…5044与写64…5108交叠，commit4981696拒绝。ID/名称仅定位证据；不因此按名称放行。 |

`register-effects-validated-UE-pre-submit`（2f3396）和`register-effects-stable-UE-pre-submit`（295be53）均timeout/-9、不计PASS。声明prototype缓存单独并未让UE完成；`register-effects-CPU-profile`短时CPU采样揭示资源依赖全表扫描，峰值1471643648bytes/无GPUwait，仍监督停止。最终范围索引后的约25s API4才是当前结果，未冒称整个性能问题或历史死机已经修复。CPU新增测试首次名称冲突编译FAIL保留，改测试变量后重跑，未覆盖旧报告。

最终backend/bundle SHA256 `9e44a610f61f82d98f1f2aedfacf21f73ddc18b088f17e6a35405c8ee2f1d1ee`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。`resource-dependency-evidence.json`保存七manifest和711所列完成日志SHA、strict0。`resource-dependency-final-related-checks-manifest.json`的UE driver exit0仅代表监督诊断运行正常，不是整帧重放通过。当前库两生产supportsRaytracing保持false。

下一批：`resource-dependency-next-inout-audit`CPU-only提取实际library2576/entry、buffer birth，保存库/AIR哈希，无GPU/native/replay验收。ScalarRead的CPU值当前用于可选数值区间/namespace推断，重放应允许已恢复的合法读写资源；但同dispatch写入可能使旧事实失效。先独立Native sample验证失效事实、地址字段/对象namespace及真实producer范围，重新建立保守恢复证明/partial展示；不能直接删除overlap检查或借旧initial数值来选GPU资源。剩余coverage族、standalone/更多view/更广pointer恢复仍需通用机制整改。

未运行/未完成：UE整帧真实RT输出/重放/事件往返/绑定，当前库最终官方sample、full IR/full RT/lifecycle/Qt-ARC/native capability集中门槛，三历史数值坏控制，非identity swizzle/更广view独立组。目标active、B544/PHASE59未关闭，无commit/push/reset；未修改RDHeaderView用户代码。源码/四最终产品/执行文档/CPU profile/失败与下一CPU审计检查点全部外盘，内盘31GiB、外盘1.6TiB。所有构建与GPU测试串行共享锁。

## B544 可变输入事实与 Native 本地内存效果（2026-10-07）

继续同一“通用资源恢复与重定位”能力批次，不新增微批次/场景许可。当前稳定backend与bundle SHA256 `7bb1d5605c788d7d08e77ce2e40dd88681fd81c5e2ec784f1d75ec66dc4b5765`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。构建0、diff-check0；生产supportsRaytracing及FromRender仍false，完整启用目标active。

实际修改 `ValidateDescriptorFrame` 的runtime dispatch机制：从已发布字节得到的CPU scalar事实在同dispatch有实际buffer/texture writer重叠时失效，删除对应提交proof并重新分析；typed slot、对象+offset及地址重定位独立保留。未知GPU相对偏移允许partial展示；重新分析失去descriptor/AS/pointer来源时仍拒绝，不沿用旧数值选择对象。known writer范围按逻辑buffer/texel区间与物理heap alias相交，保留同allocation内互不重叠的root/像素；unknown writer范围保守覆盖allocation。单调失效迭代的8轮是统一CPU分析预算，不按scene/PSO/EID许可；本组没有增大buffer/texture/work预算。Native indexed draw的真实index输入同时被写仍需额外恢复/顺序证明，保留拒绝。

`UniformResourceAccess` 现在把实际LLVM atomicrmw/cmpxchg及已识别Native local atomic API的addrspace(3)操作与device资源效果分开；出现其他/未知指针时不借此放行。原shader负责本地分配、原子、barrier和数值结果，CPU结果保持unknown，不推导shader输出。独立CPU检查确认本地atomic结果不能证明动态目标offset；device atomic缺来源继续拒绝。没有用UE功能、pass/entry名、PSO hash或固定EID决定生产支持，文中IDs仅审计定位。

每项修改的仓库对照：DX12 `WrappedID3D12GraphicsCommandList::Serialise_Dispatch`（d3d12_command_list_wrap.cpp:3530）及VK `WrappedVulkan::Serialise_vkCmdDispatch`（vk_draw_funcs.cpp:1201）捕获原维度并调用Native Dispatch，不在CPU执行线程组分配或shader数值。DX12 `D3D12Replay::GetDescriptorAccess`（d3d12_replay.cpp:1995）当前PSO/static/有效动态反馈与VK `VulkanReplay::GetDescriptorAccess`（vk_replay.cpp:2990）当前pipeline/descriptorSet展示独立；Metal采用同一分离方式，额外需typed argument-buffer字段及不可平移gpuResourceID来源恢复。`Serialise_CreateComputePipelineState`（d3d12_device_wrap.cpp:853）/`Serialise_vkCreateComputePipelines`（vk_shader_funcs.cpp:1072）捕获完整PSO/shader与原生重建，本组不修改捕获shader指令以“适配UE”。初态参考 `D3D12ResourceManager::Apply_InitialState`（d3d12_initstate.cpp:1914）资源/descriptor heap恢复与 `WrappedVulkan::Apply_InitialState`（vk_initstate.cpp:1835）descriptor写入及bound资源初态；Metal仍使用真实初态/已提交producer，不把失效数值当新GPU内容。地址参考 `D3D12Descriptor::GetRefIDs`（d3d12_manager.cpp:555）及 `WrappedID3D12Resource::GetResIDFromAddr`（d3d12_resources.h:1536）的resource+offset，VK恢复实际descriptor对象/handle；Metal仍需要实物typed来源，不猜整数地址。提交参考 `Serialise_ExecuteCommandLists`（d3d12_command_queue_wrap.cpp:440及904）DataUploadSync/跨队列同步、当前绑定descriptor引用，与 `Serialise_vkQueueSubmit`（vk_queue_funcs.cpp:1283）原command/等待和跨queue同步；Metal继续同command/committed producer、opaque写版本及alias闭包，驻留write不是producer。

| 当前库实际证据（外盘development） | 结果及真实范围 |
|---|---|
| `mutable-local-final-zero` / `mutable-local-final-offset32` | 自有MIT独立Native MSL，实际同invocation读改写、dynamic atomic目标及threadgroup allocator原子。fresh Native/capture/API/3-loop CLI；A[3]=124/B[7]=457与偏移+32的A[35]=124/B[39]=457，完整64uint/未写padding、两次Private CBV上传、每capture48事件往返/EID0、20公开access/descriptor/location通过。输入初态移除、缺source/root/ABI、原始/派生captured VA、AS-as-buffer、只读等各13坏组API4/CLI1且GPU前拒绝。Native调用结果不做CPU模拟；不是官方sample/UE/RT dispatch。 |
| `mutable-local-final-partial-texel` | fresh GPU-owned descriptor初态、实际合法同表非重叠copy、Private texel view与共享root backing、只有16-byte像素初态且其余未定义到真正GPU writer、Native SIMD及legacy register声明。完整buffer/pixel/padding、48事件/EID0和20公开access/40descriptor-location通过；26坏组GPU前拒绝。故意把view移到root区间使根事实失效，重分析后unknown descriptor namespace明确拒绝；不是删除overlap检查后GPU运行。 |
| `mutable-local-final-RT` | 当前native源码重建，复用有provenance的converted shader/reflection；fresh Native/capture/API/CLI，真实RayQuery4253/4253/0/0、528uint、64几何/Private TLAS、Private/帧GPU输入、48事件/EID0及typed公共查询通过。不是新官方下载/full RT或UE runtime AS消费支持。 |
| `mutable-local-final-related` | 29已有capture58 API/CLI通过：texel/multi-UAV、TraceRay、NativeCompute→Load、linked/depth/deferred/parallel/六八MRT。只计当前定向旧capture重放，不计fresh/全量启用验收。 |
| `mutable-local-final-UE-pre-submit` | 原403f90ea真实UEcapture；禁用initial上传/frameGPU，45s/3GiB监督，约28秒到API4、无GPUwait/strict，peak1406369792bytes。旧3993 scalar读写及4007本地atomic拒绝已越过；当前3246400/4065真实AIR含allocate/reset/next/commit/get intersection query，AS1/unknownAS1/unknownBuffer2/unknownCall10，commit4981696仍拒绝。监督driver0只表示诊断正常完成，不代表replay或RT输出PASS。 |

失败和中间证据保留：旧9e44 Native合法读改写可捕获但API3/旧scalar overlap拒绝（CLI未跑）；初版只在unknown offset目标读改写的baseline本来就PASS，不能当复现FAIL。6914中间库positive只归其hash。d7de候选三positive/freshRT/29旧capture通过，UE到4007 addrspace(3)误归外部buffer；本地atomic独立fixtureNative/capture成功而API3/unknownBuffer2拒绝。f9aa第一次local分类仍失败，定位为把getelementptr中的“ptr ”误当opaque pointer，修正字边界并新增inline GEP检查后当前候选通过。第一partial-texel跑到正确GPU前拒绝，但旧oracle只认旧overlap文字而FAIL；更新为核验“失效+unknown namespace”，保留旧FAIL并在独立目录完整复验。不是覆盖失败结果或继承旧hash PASS。

`mutable-local-evidence.json`保存六manifest和503完成日志SHA，strict0；`checkpoint-mutable-local-7bb1d560/manifest.json`保存当前源码/四产品/执行文档与复现脚本、CPU审计。外盘根 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，内部B544保持symlink。内盘约31GiB/外盘1.6TiB，所有构建及GPU/pre-submit串行共享锁，没有启动重叠UE或qrenderdoc。没有提交、推送、重置、改写用户RDHeaderView代码。

下一继续实际通用AS/Header的typed source/Native句柄重定位与已提交构建依赖，并按Native intersection-query API效果分类保持黑盒；不能通过删除queryReset/unknownAS/未知pointer检查提交。`mutable-local-next-RT-namespace-audit`保留实际library/entry/hash/10个Native query调用的CPU审计，未执行replay GPU或验RT输出。官方/full IR/full RT/lifecycle/Qt-ARC/native设备集中门槛与实际UE整帧RT输出、重放、事件/EID0、绑定仍未完成；其余coverage族、standalone frame texture/更广view与真实GPU指针发布机制仍需补。AS内部、RT shader单步和RT Pixel History不支持。

## B544 Native query 与 AS/Header 恢复（2026-10-07，候选 25632fab）

仍属于通用资源恢复与重定位能力批次。最终 backend/bundle `25632fabc7d45be667f48871f5ca14c4572ce45d728470d096bdeb706916b4c8`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。生产 supportsRaytracing 和 FromRender 保持 false；目标 active。本次确实修改后端与 Native sample，不仅整理清单；没有增加 coverage 等级、按 UE entry/PSO/EID 放行或提高资源预算。

`validateRuntimeDispatch` 增加 AS/Header 专用 typed 来源链：已恢复 kind3 descriptor 槽位产生 Header pointer，公开字段0的 Native AS handle、字段8的贡献缓冲 pointer 均从显式对象+offset恢复。dispatch 编码点保存 Header、验证过的 AS recipe 图和 build owner 快照；消费时验证 live AS/primitive、初态或真正 build、同 command/已提交 producer，并登记实际资源依赖。后来同对象的构建不能补早期缺失 producer。没有照搬旧 `ValidateRayQueryStructure` 的64实例场景限制；使用已验证 typed recipe/冻结输入及既有预算。贡献缓冲仍要有真实恢复的初态/producer，不能把 GPU 计算的 instance id 当 CPU 结果。

`UniformResourceAccess` 区分私有 query handle 和捕获资源。已知 instancing/triangle Native allocate/reset/next/commit/get/deallocate 方法保留私有句柄来源，返回数值 unknown；reset 仍记录并验证真实 AS 来源。getter 的 nocapture/readonly 属性不改变资源语义。store 按目标最外层存储空间分类：device pointer 存入 private slot不是 device buffer write，真正 device 目标/missing AS/missing query handle/未知 Native 外部指针调用仍保守拒绝。独立 CPU tests验证 private store、真实设备指针目标、未知 AS/handle以及 query 返回值不能证明动态写偏移。

Default direct instance构建族不再要求 child 与 parent 分属不同 command；child 的 build kind/owner 必须由真实在前 build 发布，额外验证 Native device一致。`CaptureASInitialBuilds` 顺序访问 candidates，只允许同 native submission的已冻结子 recipe，或之前已完成 submission；未来/缺失子构建不能提供配方。子 recipe被保留并在 TLAS之前恢复。本组 Native sample将不相关的初始化 blit 放在独立提交，AS build仍同一个提交；未取消 mixed-command/Shared-input未知 writer 的保守恢复检查。UserID/copy/refit的旧独立提交限制及 broader mixed-command snapshot机制未全部整改，不能宣称所有AS构建路径支持。

具体仓库参照（实改前读取）：DX12 `WrappedID3D12GraphicsCommandList::Serialise_BuildRaytracingAccelerationStructure`（d3d12_command_list4_wrap.cpp:990）保存完整 build descriptor、目的AS对象+offset，TLAS通过 `PatchAccStructBlasAddress` 重定位，再执行原生 Build；VK `WrappedVulkan::Serialise_vkCmdBuildAccelerationStructuresKHR`（vk_cmd_funcs.cpp:8688）保存 pInfos/rangeInfos、UnwrapStructAndChain实际handle，再原生 CmdBuild与事件/rerecord。Metal复用 typed对象、冻结输入、顺序构建图；必要差异是公开Header同时含不可平移 AS gpuResourceID和buffer VA，需要双字段重定位。DX12 `D3D12RTManager::PatchRayDispatch`（d3d12_manager.cpp:1630）恢复实际 shader-table/root descriptor区域，不CPU执行query；VK `Serialise_vkCmdTraceRaysKHR`（vk_draw_funcs.cpp:5018）恢复实际SBT区域/调度，Metal query无SBT时仍直接执行Native shader。初态参照 `D3D12ResourceManager::Apply_InitialState`（d3d12_initstate.cpp:1914）及 `WrappedVulkan::Apply_InitialState`（vk_initstate.cpp:1835）；Metal现有 `RecordReplayASContents` 保存验证的recipe、先原生恢复primitive再TLAS，本组不将allocation/identity当作已构建。提交参照 `Serialise_ExecuteCommandLists`（d3d12_command_queue_wrap.cpp:440/904）DataUploadSync/跨queue依赖，以及 `Serialise_vkQueueSubmit`（vk_queue_funcs.cpp:1283）原command/waits；Metal使用dispatch点的build owner、同command与committed条件，驻留不等于producer。展示参照两后端 `GetDescriptorAccess` 与 Native Dispatch 分离；Metal partial保留实际typed访问和unknown范围，未模拟交点。

限制分类：已移除 queryReset全拒绝、direct child不同提交要求、指针值内层addrspace造成私有store误报（历史/展示错误）。API/device仍检查真实对象、支持的TLAS/primitive recipe种类、设备及格式/指针类型；恢复限制仍拒绝缺Header/AS/child/贡献初态、未提交producer、opaque Header、未恢复GPU pointer。统一资源、初态、snapshot、Native工作量预算保持。本组新 runtime AS closure不以实例64为门槛；仍有其他coverage族、legacy query64、UserID/copy/refit、standalone frame texture/更广view等旧限制待按协议和真实机制整改。剩余私有pointer spill是地址来源传递缺口，不能永久当展示问题拒绝，也不能直接删unknownBuffer检查。

外盘development实际结果：

| 证据 | 真实范围 |
|---|---|
| `runtime-query-final-native-zero` / `runtime-query-native-variation-fixed` | 自有MIT独立MSL真Native intersection query：命中+distance+instance贡献读取、未命中参与完整输出oracle。1→3 TLAS实例；Header0→64、贡献0→16、输出index0→32，无后端二次修改。fresh Native/capture/API/3-loop CLI、两个完整64uint输出/CBV padding、每份48事件/EID0和20公共访问点（AS+UAV各descriptor/location）通过；各12缺namespace/Header/AS/child/贡献初态等坏组24 API4/CLI1在GPU前拒绝。无显式DeclareRayQuery旁路。 |
| `runtime-query-final-zero` / `runtime-query-final-partial-texel` | 当前库fresh可变输入、threadgroup allocator/local原子及动态GPU descriptor/部分texel初态通过，每份48事件/EID0和typed公共查询；13+26坏组78次GPU前拒绝。 |
| `runtime-query-final-RT` | 当前native源码构建，复用有provenance的converted shader/reflection；fresh真RayQuery4253/4253/0/0、528uint、64几何Private TLAS/帧GPU输入、48事件/EID0及typed公共查询。不是官方新下载或full RT验收。 |
| `runtime-query-final-related` | 29已有capture58 API/CLI（texel/multi-UAV/TraceRay/Compute→Load/linked/depth/deferred/parallel/六八MRT）通过；只计定向旧capture重放。 |
| `runtime-query-final-UE-pre-submit` | 403f90ea实际UEcapture，禁初态上传/frameGPU，45s/3GiB监督。API4/无GPUwait/strict，peak1284096000bytes；3246400/4065变为AS1/unknownAS0/unknownCall0，剩unknownBuffer1。AIR贡献buffer pointer经private alloca存取丢来源；不是UE重放/RT输出PASS。 |

失败证据完整保留：`runtime-query-native-baseline` Native0但capture7，实查TLAS同提交被拒绝；`runtime-query-ordered-build-baseline` 修复后Native/capture0、API3旧AS/calls闭包；首query build因局部变量shadow编译FAIL并保留日志，修名后构建通过；`runtime-query-native-closure-first` 剩五getter属性/deallocate未知calls，补实际Native API签名；`closure-second`/`closure-diagnostics`实际输出无差异，但test以未序列化AS label查身份失败，改用捕获声明真实structure；`runtime-query-native-variation`漏改descriptor source Header偏移而初态拒绝，修fixture后`variation-fixed`全通过。没有把这些失败计通过。CPU首次缺-I导致编译失败不是后端FAIL，随后正确构建和当前gate CPU通过；所有中间产物留外盘。

原始/派生shader捕获地址、只读资源、缺初态、未知producer拒绝保留；动态数值越界/division坏程序不执行、不冒充恢复拒绝。完整官方/IR/RT/frame-family、生命周期/Qt-ARC、原生设备集中门槛与UE整帧RT输出/重放/事件/EID0/绑定仍未完成。历史崩溃/系统重启未证明已修复。新产物外盘、内盘31GiB/外盘1.6TiB；构建/GPU串行共享锁，没有提交/推送/重置或改用户RDHeaderView。

当前证据已核验：`development/runtime-query-evidence.json` 保存7结果manifest及585完成日志SHA，严格诊断0；`checkpoint-runtime-query-25632fab/manifest.json` 保存83源码/执行文档、3产品SHA和不可变快照。最终构建0、diff-check0。Native query两份sample和同库相关回归范围见本节；不把UE诊断driver0当作UE replay PASS。目标active，下一是贡献缓冲pointer经过private alloca的typed来源传递，先独立Native复现，不能删除unknownBuffer检查。

## B544 私有指针来源与 Native query 值 ABI（2026-10-07，稳定候选 ec370d18）

延续通用资源恢复与重定位大批次；覆盖旧按getter/scene逐个许可方式，没有新coverage等级、UE/pass/shader/PSO/EID生产特判。实际修改 `metal_air_access.h` 及独立Native sample/gate/CPU tests。backend与bundle SHA256 `ec370d187de006bf9e64cfada5cf378c8b26d3c70819411301d648e9788780fc`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。构建与diff-check0，生产supportsRaytracing及FromRender保持false；目标active，B544/PHASE59不宣称启用完成。

### 实际机制与限制分类

`PrivatePointerSpillOrigins` 对非逃逸的私有pointer alloca构建CFG、可达边与reaching writer集合，load只使用全部在前writer的typed来源。分支/循环不计算条件，多个来源合并为PointerSet/partial；unknown writer和未初始化路径不能消失，未来store不能补早期load。LLVM private byte-pointer lifetime alias仅在只用于lifetime.start/end时不算外部逃逸；allocation和lifetime变化使旧来源失效。volatile保留原生语义，不在CPU执行shader。通过既有Loader/对象+offset、真正来源/初态/producer/alias闭包继续恢复最终device资源。128 private slots/256 CFG blocks是统一CPU工作预算，没按场景放大。未支持private aggregate/memcpy/逃逸call的来源恢复；这些仍是机制缺口，不永久归咎展示未知，也不直接删unknownBuffer。

Native triangle/instancing query的私有handle与捕获资源独立。按本机Metal SDK `metal_raytracing` 查询API及实际AIR返回ABI分类完整的候选/已提交scalar IDs、distance、ray vector、barycentric、front-facing、object/world transform getter族；allocate/reset/next/commit/deallocate/abort保持Native执行。fast/reassoc等编译标记不改变资源效果，完整返回类型和单一已知private handle仍需吻合。CPU数值、矩阵、GPU索引结果始终unknown。reset仍必须有真实已恢复AS/Header/build消费闭包。未知get_*名称、错ABI、额外resource/标量operands、缺handle仍unknown call；primitive_data返回device pointer、get_intersection_params多private输出、curves/multilevel尚需各自来源/配方/API证明，不能继承register-only资格。abort等仅CPU signature检查，未单独Native运行，不宣称Native全方法验收。UserID getter不等于所有UserID AS构建路径已支持。

已移除的限制是private store/load丢typed来源与按少数query getter许可导致的分析误拒绝。API/设备、真实资源/地址恢复、初态、生存期、producer/提交以及统一预算均继续检查；没有提高数字上限。独立sample实际GPU真交点、未命中、距离、贡献buffer读取与候选/已提交实例/primitive/geometry/user ID、barycentric、front-facing、ray和transform一致性参与完整输出oracle。fixture两offset分支从相同恢复贡献buffer的不同offset取值，GPU选择；不是CPU模拟shader后产生相同结果。

### 逐函数DX12/Vulkan对照与Metal差异

实改前读取DX12 `WrappedID3D12GraphicsCommandList::Serialise_Dispatch`（d3d12_command_list_wrap.cpp:3530）/VK `WrappedVulkan::Serialise_vkCmdDispatch`（vk_draw_funcs.cpp:1201），捕获原调度维度、原生执行与event/rerecord独立CPU数值。DX12 `D3D12Replay::GetDescriptorAccess`（d3d12_replay.cpp:1995）结合当前PSO/static/valid dynamic feedback，VK `VulkanReplay::GetDescriptorAccess`（vk_replay.cpp:2990）映射当前pipeline/sets；访问展示不代替Native调度。本组Metal采用相同分离，CFG仅保存来源集合而不执行交点/矩阵或补CPU shader算法。SPIRV debug的Op::Load/Store（spirv_debug.cpp:973/991）属于debugger值执行，不拿来作为重放资格模型。

初态读取 `D3D12ResourceManager::Apply_InitialState`（d3d12_initstate.cpp:1914）的descriptor heap复制/真实resource初态与 `WrappedVulkan::Apply_InitialState`（vk_initstate.cpp:1835）的descriptor写入、AS对象及binding state恢复。地址读取 `WrappedID3D12Resource::GetResIDFromAddr`（d3d12_resources.h:1536）的ResourceId+offset；Metal Loader保持显式kind3 Header的AS gpuResourceID与贡献buffer VA双字段重定位，private spill只是这条真实恢复来源的Native暂存，不把私有栈地址当捕获device allocation。AS初态仍由已验证recipe、primitive→TLAS顺序Native重建，不把query handle当构建资源。

提交读取 `Serialise_ExecuteCommandLists`（d3d12_command_queue_wrap.cpp:440/904）的实际descriptor引用、DataUploadSync/跨queue顺序与 `Serialise_vkQueueSubmit`（vk_queue_funcs.cpp:1283）的原command/waits/跨queue同步。Metal继续dispatch点冻结Header/AS graph/build owner和当前slot namespace、同command或已提交producer；重放资格不是私有变量名、query返回值或write residency。此处Metal必要额外机制是argument-buffer GPU pointer/不可平移handle及private暂存来源恢复；VK/DX12的descriptor API对象无需同样的AIR字段跟踪。

### 固定候选实际验收（development，全部外盘）

| 证据 | 实际结果 / 范围 |
|---|---|
| `private-query-stable-native-zero` / `private-query-stable-native-variation` | 两fresh自有MIT独立Native MSL真实triangle query，无DeclareRayQuery旁路；1→3 TLAS实例、Header0→64、贡献0→16、输出0→32，第二份两private offset writer分支，无后端二次修改。Native/capture/API/3-loop CLI、完整64uint/未写padding、每份48事件往返/EID0、20公开AS+UAV查询与40descriptor/location通过；各12坏组24 API4/CLI1在GPU前拒绝，含缺AS/child/贡献初态、namespace、错误ABI/source、原始/派生captured VA及只读。ShaderSHA分别c7b5fd93…/ee26e489…。 |
| `private-query-stable-mutable` / `private-query-stable-partial-texel` | fresh Native同dispatch读改写/threadgroup allocator原子与GPU-owned descriptor初态/实际同表copy/共享root部分texel，通过完整输出、各48事件/EID0及公共查询；13+26坏组78 API/CLI拒绝保留真实恢复边界。 |
| `private-query-stable-RT` | 当前native源码构建、复用有provenance converted shader/reflection；fresh Native/capture/API/CLI真RayQuery4253/4253/0/0、528uint、64几何Private TLAS/帧GPU输入及48事件/EID0。不是新官方sample/full RT或UE验收。 |
| `private-query-stable-related` | 29已有capture58 API/CLI（texel/multi-UAV/TraceRay/NativeCompute→Load/linked/depth/deferred/parallel/六八MRT）通过，只计本库定向回归。 |
| `private-query-stable-UE-pre-submit` | 原403f90ea实际UEcapture，45s/3GiB限制，禁initial上传/frameGPU。监督driver0但实际API4，peak1523056640bytes、无GPUwait/strict；原3246400/4065已AS1/unknownAS0/unknownBuffer0/unknownCall0，资源恢复partial接受。下一1591744/6399 AS2/unknownAS0/read237/write4/unknownBuffer56/texture9/sampler7/unknownCall0，commit4981760拒绝。不是UE整帧RT输出、重放或EID0 PASS。 |

CPU tests覆盖branch双方来源、未知overwrite/缺初始化分支/future store、循环在前writer、lifetime reset和外部alias逃逸，以及Native getter scalar/vector/aggregate正确ABI、错类型/指针返回/额外参数/缺handle、getter数值不能证明输出offset。Native实际4种transform/多个ID/vector/barycentric/front-facing参与输出；CPU method分类不能替代未运行的Native方法验收。

失败证据保留各自库哈希与原始报告：`private-pointer-native-baseline`（25632fab）Native/capture0、API3 unknownBuffer1；`private-pointer-native-first`（6eac7903）仍因LLVM lifetime byte alias丢来源拒绝；1098119a的`private-pointer-native-lifetime`和`private-pointer-native-branch-variation`实际PASS，不继承为ec370d18最终验收。`private-pointer-first-UE-pre-submit`（1098）越过原入口，后续query unknownCall24；`private-query-api-native-baseline`（1098）Native/capture0而API3 unknownCall20；首完整编译queryMethod shadow FAIL日志保留，修名后f4c8a010的`private-query-final-native-zero`Native/capture0、API3 unknownCall11，实查fast数学标记。最后本候选完整重跑上述stable组，无覆盖或将FAIL计PASS。

CPU-only下一审计 `private-pointer-next-RT-consumer-audit` 保存真实library/entry/factory链及SHA；实际library3062dd05…、AIR172594f2…。121 CFG block，4个真实private metadata load均有唯一在前typed writer；不是增加block预算解决。剩unknownBuffer56必须按真正descriptor/设备buffer来源恢复诊断，不能继续扩大CPU数值表达式许可或删除unknownBuffer。测试IDs/名称仅定位证据。未运行/未完成：UE整帧真RT输出/重放/往返/EID0/绑定，最终官方/full IR/RT/frame-family/lifecycle/Qt-ARC/原生设备集中门槛；更广private pointer/typed动态namespace、其余coverage/standalone texture/view/AS legacy族机制仍需推进。历史Qt/UE崩溃及系统重启未证明已修复。

新构建/GPU测试共享锁串行，所有新产物在 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，仓库路径保留symlink。内盘31GiB、外盘1.6TiB。根AGENTS及metal持续提示已验证覆盖旧场景策略；无提交/推送/reset或改用户RDHeaderView。证据 `private-query-evidence.json` 和 `checkpoint-private-query-ec370d18/manifest.json` 保存完成日志、源码/产品/文档SHA；后续读取本节最新边界，不继承旧库结果。

## B544 动态缓冲 namespace / 部分初态机制（5673f351，2026-10-07）

本组接续同一较大能力批次，不新增场景许可或 coverage 等级。先用独立 MIT Native sample 复现：真正 intersection-query 的实例 ID 经 GPU metadata 索引选择 descriptor buffer；Native/capture 正确，原 ec370d18 重放 unknownBuffer1 拒绝。恢复机制不计算 shader 行号/交点，不引入功能名、pass/shader 名、PSO hash 或 EID 资格。

实际修改 `metal_air_access.h` 的 typed pointer provenance 与 `metal_descriptor_tables.cpp::ValidateDescriptorSlotFrame` 内 runtime Loader/`validateRuntimeDispatch`：为已有声明的 24-byte table buffer 字段保留 `DescriptorPointer` namespace，typed struct row GEP 和 compiler byte GEP 的 ABI projection 可指向同一对象。Unknown × ABI stride 只保存布局，数值仍 Unknown；并非 index、溢出或 GPU 内存安全的 CPU 证明。未知相对索引由原 Native shader 执行。真实 metadata table 身份、声明范围、字段/stride 和来源须匹配；任意 GPU 整数 inttoptr、错误字段、缺表/来源仍拒绝。

恢复候选按消费点冻结的 publication/lifetime/source/offset/extent 构造，验证 GPU 初态或实际 typed copy 发布、真实资源生存期、alias、范围及提交依赖。一次 namespace 展开复用实际表的统一预算与每 dispatch cache，不将旧64值展示 cap 扩为场景数字。可能的缓冲只建立 resourceCommands 依赖，不计入实际 descriptor access，展示 partial；AS/UAV 的已知真实公共查询仍正常。动态 texture/AS 字段、动态目标写入、缺发布或 opaque 未恢复来源尚未实现，本组不会直接放行。全 heap 的动态读闭包尚可能因无关 retired/null row 过于保守，见 UE frontier；这不能升级为永久 API 许可名单。

`PatchDescriptorSource` 将已验证的帧内 standalone Private/Tracked allocation + Native 长度/身份/生命周期，与 CBV 数值读取分类分开，普通缓冲不再必须先被 AIR 识别为 CBV 才能重定位。背景初态、真实资源、当前工厂、alias、offset/overflow 与预算检查保留，未扩大数字上限。部分初态保留实际 upload/copy 区间：动态绑定是可能依赖，不要求整分配或 padding 已定义；每候选仍需真实恢复区间能覆盖一个该宽度合法读取，完全缺失的初态/生产者仍拒绝。Native 合法程序负责只读取其定义字节；不伪造 undefined padding，不将这一事实提升为全 allocation 初始化，也不声称 CPU 已证明所有动态执行路径。

### 修改前实际读取的 DX12/Vulkan 函数及对应方式

- `WrappedID3D12CommandQueue::Serialise_ExecuteCommandLists`（d3d12_command_queue_wrap.cpp:440；904–958）：动态 descriptor 写通过 GetRefIDs 捕获帧引用，boundDescs 按真实 heap 范围加入当前资源，随后合并 baked command references。`DataUploadSync`/原提交执行负责上传与队列依赖。Metal 复用实际当前 binding namespace 的保守资源依赖，而不是用未知 index 的 CPU 结果决定资格；依赖来自 frozen typed source，不能用未来 publication/build 修早期缺失。
- `WrappedVulkan::Serialise_vkQueueSubmit`（vk_queue_funcs.cpp:1283；捕获入口约1018–1052）：descriptorSets 的 layout bindings/variableDescriptorCount 中每实际 descriptor 调用 `AccumulateBindRefs`，提交保留队列/wait。Metal 用已有 typed table 声明和当前槽位 sources 替代 VkDescriptorSetLayout，在原 command/encoder/commit 顺序中检查实际依赖；未知访问展示不是提交许可。Metal encoded descriptor memory 可被 GPU 写，额外要求 actual initialRestored 或完整 typed GPU copy source。
- `D3D12ResourceManager::Apply_InitialState`（d3d12_initstate.cpp:1914）Native CopyDescriptorsSimple 恢复 heap，普通 Resource/Heap 根据 Copy/ForceCopy/SparseOnly 和真实 backing 上传；`WrappedVulkan::Apply_InitialState`（vk_initstate.cpp:1835；device-memory 分支约2424–2460）按 MemRefs rangeRefs/InitPolicy 构造 copy/clear intervals，buffer 的 bound memory 与 sparse 绑定分别处理。Metal 复用真实对象和区间恢复，而非 shader 数值已知度；帧出生 Private 缓冲只有4-byte合法 upload 时保存4-byte定义区间，EID0及未写 padding 不制造内容。
- `WrappedID3D12Resource::GetResIDFromAddr`（d3d12_resources.h，资源+offset 查找）和 `D3D12RTManager::PatchRayDispatch`（d3d12_manager.cpp:1630；1660–1730 heap base/size/stride 与 GPU VA lookup）保留原 GPU shader 的资源选择，按对象身份重定位地址。Metal 地址字段按 `PatchDescriptorSlot`/`PatchDescriptorSlotField`/`PatchDescriptorSource` 逐字段从真实 ResourceId+offset 重建；typed AS Header 的 gpuResourceID 与 contribution pointer 继续各自恢复，不扫捕获 VA 猜资源，也不要求普通 frame buffer 属于 CBV。
- `D3D12Replay::GetDescriptorAccess`（d3d12_replay.cpp:1995）仅合并 staticAccess 和 valid dynamic feedback；`VulkanReplay::GetDescriptorAccess`（vk_replay.cpp:2990）依据真实 stage/binding/有效访问反馈生成展示。Metal 没有该 GPU feedback 时保留 partial；本组 namespace 候选不进入公开实际访问，不把 whole heap 居留列表伪装成 shader 访问。

### 固定候选实际验证与保留的失败

最终 dylib 和 app bundled dylib 均 `5673f351b18da36b3d6b0171a9d77f696c718f9b1e1efe64df594644ff849434`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。共享锁下构建/GPU 串行。产物：`/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544/development/dynamic-namespace-final-*`，总验收 manifest 为 `dynamic-namespace-final-checks-manifest.json`。该总脚本 exit0 只表示定向检查和预期拒绝完成，UE capture 的 API4不算 PASS。

- 两 fresh Native 部分初始化 query：实际命中 instance2 动态选第3 Private 缓冲，1024-byte allocation 的 offset64只上传4字节3033；另一64KiB/offset65532变体使用所有支持的 register getter，实际instance0选独立输入，部分缓冲仍是恢复候选。两者 Native/capture/API/CLI0，完整A/B oracle不同输入1011/2022、定义4字节在copy前后/EID0往返（undefined不比对）、各56事件选择、20 AS/UAV公共access/40 descriptor-location；各16坏组32无GPUwait拒绝。两变体 backend 无修改，Header/贡献/输出偏移亦不同。
- fresh Native GPU descriptor initial+frame republish：3实例/实际instance1/Header64/贡献16/输出32/private pointer双来源，Native/capture/API/CLI0；48事件/EID0、20公共access/40 location，20坏组40拒绝，真实GPU copy source/value 删除或不匹配仍拒绝。
- fresh可变数值/线程组atomic：48事件/EID0、13坏组26拒绝；fresh部分texel/shared-root/GPU descriptor：48事件/EID0、26坏组52拒绝；不继承旧库结果。五项 fresh 普通 gate 共91坏组182 API/CLI拒绝。
- fresh converted RayQuery：真实4253/4253/0/0、528uint、64几何 Private TLAS/帧构建输入/48事件与EID0；Native/capture/API/CLI0。29旧capture58 API/CLI（IR TraceRay、多UAV/texel、图形linked、MRT/depth等）当前库通过。属于相关定向回归，不是官方/full/最终集中RT验收。
- 当前库 UE 原403f90ea capture：45s/3GiB 禁initial/frame GPU预检，API4，peak1207173120 bytes，未触发限额、无GPUwait/strict。原1591744/PSO6399 consumer 的 unknownBuffer56→0、AS2/unknownAS0/unknownCall0；64KiB buffer14859真实4-byte上传恢复越过，下一 table25/slot2760 publication检查拒绝。只测试定位使用这些ID/offset。原始 initial tuple 是(0,963,0)，槽位曾为纹理并退休；不是已证实缺缓冲，不能据此宣称UE output/replay通过。审计在 `dynamic-namespace-partial-resource-audit`，下一按当前零字段、退休generation、opaque GPU writer/真实发布证明补机制，非零失效地址不得跳过。

保留旧ec基线 unknownBuffer1、2093/84b9各中间PASS和first build undeclared helper FAIL；84b9 active1 GPU组合 Native7是样例对GPU重发布时刻期待B而实际指A的oracle错误，修样例而非修改资格；511d first部分缓冲 Native/capture0、预检accepted但真实replay DescriptorSlotBinding失败，促成普通frame Private地址修复；7fd5相关partial-texel中，坏capture仍API4，但旧oracle仅认可unknownBuffer、忽略新的dynamic writer缺机制诊断，保留失败后按真实拒绝原因修oracle；5673新定义字测试helper首次用错误SDObject accessor导致编译失败，修成AsUInt64并fresh复验，原失败保留。不同hash/未完成/FAIL不冒充最终PASS。

### 尚未完成与下一机制

支持范围限已声明typed table的buffer字段动态只读namespace与普通frame Private部分copy初态；没有实现动态texture/AS返回、动态写入producer覆盖、nested任意GPU pointer或所有unpublished/null/retired sparse行。下一先用独立 Native sample验证退役纹理/零缓冲字段、GPU table发布与非零失效来源的边界，再整改sparse namespace闭包；不能直接skip所有missing/retired，不能从旧initial字节推测GPU后的当前pointer。其后 UE预检只有新机制变化才重跑。UE整帧GPU输出/事件/EID0/绑定、官方/full IR/RT/lifecycle/Qt-ARC/native capabilities当前候选完整验收未运行；两生产supportsRaytracing flags false，B544/PHASE59和持续目标继续active。


## B544 物理零地址字段、GPU 发布与统一复制预算（c52ba022，2026-10-07）

本组延续同一“通用资源恢复与重定位”能力批次。移除动态缓冲 namespace 必须每行都有 live logical descriptor 的历史限制：physical buffer 字段确实为零的稀疏行、纹理行和退休行不需要虚构 buffer 依赖；退休只改变逻辑 generation，不清除或重写 Native 字节。非零地址仍需真实当前 publication、来源、生命周期、已定义内容与提交依赖。没有按 UE 名称、shader、PSO hash 或 EID 判断资格，也不模拟 GPU 行号、ray query 或 shader 输出。

实际实现：`ValidateDescriptorSlotFrame` 为声明 stride24 表的第0个地址字段保存消费点不可变零位图，只从真实 captured initial/typed CPU publication/已匹配 full24 GPU copy 发布；unknown GPU writer 和物理 alias 写使证明失效，typed producer 的目标字段先失效再由匹配 publication 重建。位图按真实范围/COW 保存，并计入既有 `MetalReplayPreflightBudget`，与 encoded chunks、复制恢复成本共享 128MiB/record reserve，不提高上限。commit-owned CPU update 与消费字段重叠时独立校验完整零字节，或要求本提交实际在后匹配的 GPU copy 恢复；不能从旧初态借零事实。

`DescriptorSlotShadow::gpuCopyPublished` 是派生状态、不新增 wire 字段或 coverage 等级；`ReadDescriptorSlotEvent` 仅在实际 expected full-copy 数据匹配后建立，CPU value publication 清除它。`TrackDescriptorGPUCopy` 不再用引用对象数量非零作为 producer 的替代条件：合法 all-null API descriptor 可没有引用资源，但每个 API 地址字段必须确实为零，full24 typed source/destination/type/生命周期/expected-copy 校验保持。runtime Loader 根据实际复制证明及完整 source map 相等判定发布，不伪造 CPU GPU destination 上传。完整 typed CPU publication 可以独立恢复24字节，缺冗余 raw 初态副本是合法控制，缺 publication 或真正 producer 仍拒绝。

复制预算整改实际删除 buffer ordinary copy 的 coverage 数量许可、typed copy 最多2次以及两个线性 buffer↔texture copy 分支的256次/16MiB场景计数。现逐次恢复成本按共享整帧预算记账；`DescriptorPlainCopyLimit` 的既有单次1MiB边界仍保留，不能把它误当整帧累计限额。真实 buffer extent、overflow、出生、alias、source/producer、typed全24及 Native linear-copy layout 校验继续。其他纹理-copy工厂范围或coverage协议族未全移除，本组不宣称全部API已支持。累计copy bytes只是诊断，不授予资格。

### 修改前实际阅读的本地 DX12/Vulkan 对照

- `D3D12Descriptor::GetRefIDs`（d3d12_manager.cpp:555）Undefined/Sampler 不制造资源引用，CBV由实际VA找resource，SRV/UAV保存object/counter；`WrappedID3D12Device::CopyDescriptorsSimple`（d3d12_device_wrap.cpp:2311）捕获源/目标heap并按每个descriptor真实非零RefIDs记录依赖，保存 DynamicDescriptorCopy 和 Native copy。Metal 复用“descriptor内容/复制事实与引用对象数量分离”，不能因空引用图拒绝真正全零字段。
- `D3D12ResourceManager::Apply_InitialState`（d3d12_initstate.cpp:1914）通过 Native CopyDescriptorsSimple 恢复原heap；`WrappedVulkan::Apply_InitialState`（vk_initstate.cpp:1835，descriptor分支1850–1920）原 vkUpdateDescriptorSets 恢复typed内容并同步SetBuffer/SetImage/SetAccelerationStructure，真正NULL_HANDLE字段不虚构对象。Metal 差异是24-byte用户可写表含GPU VA/resource ID/metadata，因此必须逐地址字段重定位或验证当前零字节，物理零位图不能代表整个descriptor为空，也不能借此绕过其他texture/AS字段恢复。
- Vulkan submit捕获 descriptorSets 迭代（wrappers/vk_queue_funcs.cpp:1018–1052）按 layout/variableDescriptorCount 及 AccumulateBindRefs 捕获真实资源；`Serialise_vkQueueSubmit`（:1283）按真实提交/队列切换/等待恢复 Native 执行顺序。Metal 复用消费点冻结来源、实际command/commit所有权；GPU publication须对应真正copy/producer，CPU上传与GPU恢复顺序额外验证。
- `WrappedID3D12GraphicsCommandList::Serialise_CopyBufferRegion`（d3d12_command_list_wrap.cpp:5193）和 `WrappedVulkan::Serialise_vkCmdCopyBuffer`（wrappers/vk_draw_funcs.cpp:1992）序列化实际对象/offset/size或region数组，调用原Native复制，并按实际command范围记录copy事件及usage。Metal继续按实际对象与原复制执行，数量不是scene许可证；现有CPU快照/区间恢复额外成本进入统一预算。
- DX12 `GetDescriptorAccess`（d3d12_replay.cpp:1995）与 Vulkan（vk_replay.cpp:2990）的访问展示与Native恢复分离；Metal 的完整恢复候选不冒充已访问descriptor，动态行号由原shader选择，展示保持partial。

### 实际验证与限制

最终 dylib 与 app bundled dylib 均 `c52ba022e71eb402f6b3e7ebeb165060441019a62d117d3540091b4d2f5c230e`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。共享锁下构建/GPU串行，产物均在 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544/development/null-namespace-*`，最终主要结果 `null-namespace-stable-checks-manifest.json` 和 `null-namespace-stable-GPU-checks-manifest.json`。主脚本exit0包含预期UE拒绝，不代表UE整帧PASS。

- fresh retired/null、稀疏257行、GPU零publication17行：R32Uint2×3→RGBA8Unorm7×5真实Private纹理上传；3/17/257零行、1→3实例、实际GPU实例2→row5、Header0→64/贡献0→16/输出0→32、whole Native query getter API，原Native计算不改；各Native/capture/API/CLI0、48事件/EID0/完整输出，公开AS+UAV查询和partial动态展示通过。GPU变体首提交实际3次typed full24复制越过旧最多2次限制，背景发布+帧重发布同时覆盖。零资源引用不等于无producer。
- fresh partial-active2 的frame Private第三缓冲/真实4-byte上传、mutable读改写/local atomic、partial texel/shared root/GPU初态与重发布继续通过；分别56/48/48事件/EID0，定义字/像素与完整输出验证不造padding初态。上述六正常fresh场景共296事件检查，111坏组222 API/CLI拒绝均在GPU前；缺真实copy、source publication、AS/child/贡献初态、未知VA、retired非零来源继续拒绝。另GPU变体缺冗余raw source initial的合法控制完整API/CLI输出和事件往返通过；未跑的旧numeric controls不计PASS。
- fresh不声明typed producer的GPU地址writer Native/capture0，但API4/CLI1在真实runtime writer恢复闭包拒绝（unknownBuffer1），无GPUwait。不是writer支持PASS，也不冒称单独覆盖了后续null位图拒绝分支；严格test oracle现在要求到runtime检查，不接受旧无inline布局早拒绝作为同一验证。
- fresh converted RayQuery Native/capture/API/CLI0，4253/4253/0/0、528uint、64几何Private间接TLAS/帧目标/真实输入、48事件/EID0通过。29旧capture58 API/CLI通过，涵盖texel、multi-UAV、TraceRay、frame load、graphics linked、depth和6/8 MRT，属于相关回归。CPU allocation/preflight组件通过实际backing去重、overflow、CPU/headroom、多个1MiB复制+位图共享预算/aggregate拒绝/精确边界；不扩大预算。
- 当前原UE403f90ea capture的45s/3GiB禁initial/frameGPU预检：API4、peak1357774848bytes，没有触发限额、GPUwait或strict诊断。越过table25/slot2760退休纹理零缓冲字段；当前1591744/6399 consumer仍AS2/unknownAS0/unknownBuffer0/unknownCall0，下一slot20592的非零resource14714（393216-byte frame placement）未证明初态/在前producer，commit4981760拒绝。CPU审计 `null-namespace-next-producer-audit` 保存实际factory/publication/heap3138物理重叠链与原capture哈希；该候选没有找到此前直接copy/useResource，不能假装实际shader已访问它，也不能直接放行所有未定义候选。下一先审计physical backing/定义区间与动态依赖候选资格，补真实机制或明确有效拒绝。测试IDs只定位证据；尚不是UE输出/重放PASS。


### 保留的失败与未覆盖范围

旧5673后端独立 native sample 能正确运行/捕获，但 retired/null 行在实际动态namespace消费处 API3 拒绝，保存 `null-namespace-retired-baseline`。初次新零字段样例完整重放通过后，坏capture生成器局部变量 `copy` 遮蔽模块导致FAIL，已修复；初次GPU全零copy的“缺raw source initial必须拒绝”oracle错误，实际完整typed CPU publication足以恢复，改为完整重放输出/事件控制并保留原FAIL。首次build使用deprecated `shared_ptr::unique()` 在Werror下失败，修成use_count；预算初次错误把单次1MiB当整帧累计限制，fresh converted RT 在第二段copy拒绝（95d5），修为共享frame记账并fresh复验，保留全部FAIL。不把不同hash/未完成记录算当前PASS。

动态texture/AS字段、动态写目标完整producer覆盖、任意nestedGPU指针、所有legacy coverage及standalone/MSAA/view机制未全支持。未知GPU pointer writer即使Native恰好写0，也不能仅靠旧零初态放行；独立writer测试到真实runtime writer closure拒绝，不冒称已覆盖每个零位图失效分支。UE整帧GPU光追输出/重放/事件/EID0/绑定、官方/full IR/RT/lifecycle/Qt-ARC与最终原生设备集中验收未完成，两生产flagsfalse，B544/PHASE59/持续目标active。历史Qt/UE崩溃及系统重启未证明全部闭环。

本组最终源码/产品/结果检查点：`/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544/checkpoint-null-namespace-c52ba022/manifest.json`；结果/完成日志哈希、实际未跑范围及保留失败记录见 `development/null-namespace-evidence.json`。旧5673检查点原样保留。


## B544 Native GPU 写入定义区间与通用 dispatch 绑定（af26d32b，2026-10-07）

本组实际整改 GPU 内容生产与CPU值展示的耦合，不模拟shader结果、不增coverage或CPU表达式许可。独立Native sample真实调用clz，GPU在frame-born Private缓冲写出3033，intersection-query动态选择第三descriptor读取它；旧c52 Native/capture0，但runtime已经确认真实写入，后续仍因未获得定义区间API3拒绝。新机制将“这个字节由真实在前GPU写入定义”与“CPU知道字节值”分开。

`metal_air_access.h::UniformAccessReport::BufferAccess`新增派生effect标记`definiteWrite`：现有合法device store有精确恢复地址时，只记录确定的写入效果，值可为Unknown。它不扩展数值运算；`definiteStore`依旧独立用于可选精确CPU数值事实。任何branch/switch/invoke/loop、ranged/未知地址、PointerSet候选和atomic不借此建立新精确区间。`ValidateDescriptorSlotFrame::validateRuntimeDispatch`在完整资源/地址/readonly/extent/提交证明之后、当前dispatch操作顺序中，为确实执行的非零compute写入保存其真实定义区间及command依赖，费用进入既有frame预算；不会提前使用未来writer。新区间不制造CPUknown bytes、不初始化padding、不适用于raster或typed descriptor指针字段；未知GPU指针还须原typed重定位机制，不能拿“字节已写”替代来源恢复。CPU定向检查覆盖未知clz值的确定写、未知/ranged地址和可跳过写的区别。

独立样例随后暴露真正Native replay的另一个历史限制：预检已接受producer，但`Serialise_dispatchThreadgroups`要求slot0或匹配的2D纹理copy pair，只有slot2的合法inline typed pointer被拒绝。实际修改direct与indirect dispatch的sourced路径，以已有whole-frame资源闭包及实际PSO反射所需绑定/对齐/大小校验为准，不限定slot0、output buffer或2D copy形状。保持真实encoder生命周期、RT闭包、函数表、Native设备/PSO线程组、非零工作量、间接参数证据/epoch/目标与预算检查。本组独立producer已从slot2变slot7无需再改后端；现有间接路径相关回归也通过，但不冒称已新增slot7间接producer专项验证。

### 修改前阅读的本地对应函数及复用

- Vulkan `WrappedVulkan::Apply_InitialState`（vk_initstate.cpp:1835、device-memory分支2424–2460）从原bound memory、rangeRefs和InitReq/InitPolicy决定copy/clear区间，记录真实initialization与Native操作，不要求CPU能算出shader输出；同文件1938–1996按bound offset与实际write refs决定后续重置。Metal复用定义区间与值分离，但其现有typed捕获恢复契约需要保守的Native写效果/原操作顺序证明，不能把useResource(Write)当producer。
- DX12 `D3D12ResourceManager::Apply_InitialState`（d3d12_initstate.cpp:1914、1952及2090–2138）按Copy/ForceCopy/SparseOnly/真实resource与初态copy source、barrier恢复；缺source仍报错，调用原CopyBufferRegion而不执行CPUshader。`WrappedID3D12GraphicsCommandList::CopyBufferRegion`捕获部分（:5275–5286）记录原对象/offset/bytes、源Read和目标PartialWrite。Metal复用真实Native执行、资源/offset和partial内容，写后的区间不等于全allocation初态，缺真正producer仍拒绝。
- DX12 `WrappedID3D12GraphicsCommandList::Serialise_Dispatch`（d3d12_command_list_wrap.cpp:3530）和 Vulkan `WrappedVulkan::Serialise_vkCmdDispatch`（wrappers/vk_draw_funcs.cpp:1201）记录真实command及三维groups，在原/重录command范围调用Native dispatch并建立事件；资格不要求shader呈某种2D copy或使用slot0。Metal按照实际反射绑定/来源/地址和原encoder/submission顺序执行，同时保留其typed inline恢复与设备/PSO线程组验证。
- 提交/地址仍沿前组已读DX12 GetResIDFromAddr/RTManager::PatchRayDispatch和Vulkan Serialise_vkQueueSubmit的原生对象/offset/提交依赖思路；本组没有添加RT专用CPU执行器、AS内部或shader单步。字节定义状态不能重定位GPU VA，真正未知指针仍拒绝。

### 固定候选真实验证

固定dylib/bundle `af26d32b90d2f6c9ed48988186fde8e5d5007d76ae0dd8c828f169e0d9dda191`、GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。产物根 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544/development/GPU-defined-*`；主要manifest为`GPU-defined-stable-checks-manifest.json`与`GPU-defined-stable-GPU-checks-manifest.json`。总脚本的预期UE拒绝不算UE PASS。

- 两fresh MIT Native真query样例：cross-submit/slot2/1KiB allocation/offset64与same-submit/slot7/64KiB/offset65532、Header64/贡献16/输出32、3实例中真实实例2选第三缓冲及whole query getter变化，Native/capture/API/CLI全部0，完整256-byte输出、真实GPU-defined word3033、各56事件往返/EID0、20AS+UAV公共查询/40location检查通过。producer未绑定slot0且CPU不模拟clz，日志实际`scalarKnown=0`。每次只定义4bytes，padding/EID0不制造字节oracle。删producer、删真实source、缺AS/child/Header/贡献与原始VA都仍GPU前拒绝；将整个在前GPU producer command移动到首consumer之后时，在实际`contents unavailable`处拒绝，未来writer不能补早期消费。
- 同hash fresh retired/null、部分Private真实copy、可变/local atomic、部分texel/shared root/GPU初态+帧重发布与GPU全零descriptor再验通过；上述七正常fresh样例共360事件，128坏组256API/CLI均GPU前拒绝。缺冗余raw source initial但完整typed24 publication的合法控制完整重放继续通过。未声明GPU地址writer Native/capture0、真实runtime unknownBuffer1 API4/CLI1拒绝，无GPUwait，不计writer支持PASS。
- fresh converted RayQuery Native/capture/API/CLI0，4253/4253/0/0、528uint、64几何Private间接TLAS/帧目标、48事件/EID0通过。29旧capture58 API/CLI通过，覆盖texel、multi-UAV、TraceRay、frame load、graphics linked、depth及6/8MRT，属于定向相关回归。CPU AIR检查随每份fresh gate编译运行通过，新增Unknown stored value的definiteWrite、ranged/未知地址和conditional store反例保持数值Unknown。未扩大预算或借此启用能力。
- UE原403f90ea capture禁initial/frameGPU、45s/3GiB预检仍API4，peak1364377600bytes、没有触发限额/GPUwait/strict。原consumer1591744/6399仍AS2/unknownAS0/unknownBuffer0/unknownCall0，继续在table25 slot20592的14714未获内容/producer证明处拒绝。该对象是frame placement、393216bytes；没有找到在前直接copy/useResource，也没证明shader实际访问了此候选，因此不伪造producer或当作实际access。CPU审计追加`all-serialized-identities.json`枚举所有原始字段身份（区分同值blob index），与原factory/物理backing审计一起保留，IDs/EIDs仅定位测试。下一继续按通用机制解决动态依赖候选与真正恢复义务的边界，UE整帧验收未通过。


### 失败及尚未完成

`GPU-defined-range-baseline`记录c52真实Native/capture0、已识别private写但后续缺区间API3；`GPU-defined-range-private`记录a15d420b已补区间、真实replay dispatch slot0历史guard失败；`GPU-defined-range-closed`记录af库Native/capture/API/CLI输出与事件通过后，测试生成器仍错误要求两dispatch而非真实三dispatch导致FAIL，已修oracle并fresh复验。失败保留不计当前PASS。

不承诺从可能的conditional/ranged/atomic/raster写直接推得整个buffer已初始化，也不按未来producer补早期消费。新机制不解决所有动态namespace依赖候选的过度保守问题：当前UE非零候选未找到在前直接copy/useResource，未证明实际shader读过；不能借本组写区间去伪造它已有producer。下一继续physical backing/真实定义区间及动态候选恢复资格，保留真正缺来源/初态/producer的拒绝。最终官方/full IR/RT/frame-family/lifecycle/Qt-ARC/native能力与UE整帧光追输出/事件/EID0/绑定未运行或未完成，supportsRaytracing和FromRender仍false，目标active，历史用户崩溃/系统重启不冒称已闭环。所有新产物外盘，构建/GPU共享锁串行，不覆盖用户RDHeaderView/工程、不提交推送重置。

本组最终源码/产品/结果检查点：`/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544/checkpoint-GPU-defined-af26d32b/manifest.json`；结果/完成日志哈希、失败和未跑范围见`development/GPU-defined-evidence.json`。旧c52检查点原样保留。

## B544 typed namespace 行窗口与真实读取宽度（2026-10-07）

本组先复现独立 Native query：真实 group 坐标动态选择 descriptor 行，原生输出及捕获正确；旧 `af26d32b` 后端把合法 ranged metadata load 误判未知地址。已实际修改 `Value`/typed GEP 与 runtime loader：保留已知 API invocation 范围的 exclusive row-window，以及真实 ABI stride/field。原 shader 仍执行选择；不新增 CPU intrinsic/算术结果模拟，不将窗口内候选当实际访问。无法证明范围时延续完整当前 namespace 恢复，不能借这个窗口机制跳过任意未知行。

`validateRuntimeDispatch` 依据真实 table 声明验证窗口的起止、对齐、stride、field、溢出；所有物理 typed 地址仍按当前发布/来源和生存期重定位，窗口内可能读取的 buffer 才承担对应内容与生产者恢复义务。移除了“合法 ranged table load 必然 Unknown”及“可证明范围外冷 buffer 也必然要内容 producer”的过度限制。真实API/设备、全部字段身份、窗口内initial/producer、readonly、alias、提交与统一预算不变；不提高 count 或 size 限额，不按名字、hash或EID资格。

恢复缓存现在按 `(table, base, end)` 区分窗口，并仅复用已经验证的读取宽度；更宽读取重新验证实际内容区间，不借allocation容量升级内容证明。独立样例先窄后宽读取同一动态候选，真实4byte部分输入通过；损坏capture只留下2byte上传仍GPU前拒绝。新定向样例暴露 Native volatile i16 load 被错误当成类型字符串的问题，已识别可选 volatile 修饰并保留真实2byte内存效果，数值保持可选分析/Unknown；不忽略该读取。原始失败分别保存。

### 本轮实际逐函数阅读和复用

- 范围/展示：`D3D12Replay::SavePipelineState`（d3d12_replay.cpp:1246，root table ranges:1415–1463）保留 root signature 的 NumDescriptors、table offset/register/space；`VulkanReplay::GetDescriptorLocations`（vk_replay.cpp:3114，3175–3239）依据 layout binding 的 GetDescriptorCount(variableDescriptorCount) 定位，越界明确报告。`GetDescriptors`（DX12:1790、VK:2642）依据已恢复 descriptor store/ranges 返回对象，不执行shader索引。Metal复用声明范围/对象与可选展示分离；本组的 AIR 行窗口是 Metal typed原始VA ABI 所需的额外保守地址投影，并非声称DX12/VK实现相同AIR算法。
- 捕获/字段恢复：复读 `WrappedID3D12Device::CopyDescriptorsSimple`（d3d12_device_wrap.cpp:2311）Native复制后记录源/目标heap及source descriptor引用；`D3D12Descriptor::GetRefIDs`（d3d12_manager.cpp:555）CBV经GetResIDFromAddr、SRV/UAV保存真实ResourceId。`WrappedVulkan::Apply_InitialState`（vk_initstate.cpp:1850–1904）wrapper vkUpdateDescriptorSets unwrap已恢复资源，并重建current bindings、bufferInfo offset/range与AS数组。Metal延续此前typed publication/copy source证明及PatchDescriptorSlotField的ResourceId+offset/Native gpuResourceID；冷行也保持真实字段重定位，不借未知索引制造地址或仅恢复窗口内字段。
- 内容：`D3D12ResourceManager::Apply_InitialState`（d3d12_initstate.cpp:2090–2138）要求实际copy source，按Native barrier和CopyBufferRegion恢复；`WrappedVulkan::Apply_InitialState`（vk_initstate.cpp:2430–2460）按MemRefs逐范围计算InitReq Copy/Clear，没有信息时保守恢复完整memory。Metal复用“内容证明来自真实恢复区间，不来自allocation容量”的原则；frame-born真正4byte producer独立于CPU知道其值，窗口内缺内容继续拒绝。
- 提交/展示：`WrappedID3D12CommandQueue::Serialise_ExecuteCommandLists`（d3d12_command_queue_wrap.cpp:440–490）DataUploadSync及跨queue同步；`WrappedVulkan::Serialise_vkQueueSubmit`（wrappers/vk_queue_funcs.cpp:1283–1315）保存原提交/等待关系且多queue或等待semaphore时同步。Metal按消费点冻结的publication和同command/已提交producer恢复，未来producer仍不能补前消费。`D3D12Replay::GetDescriptorAccess`（1995）只追加valid feedback，`VulkanReplay::GetDescriptorAccess`（2990）返回静态访问数据；候选恢复集合不伪装成shader访问，展示partial，不以展示完整性新增场景许可。

### 固定候选实际结果

| 范围 | 真实结果 |
|---|---|
| 独立Native row-window query | `bounded-namespace-final-range-small/manifest.json`、`-range-offset/`、`-range-GPU/`；Native→capture→API→CLI各0，真实intersection query输出/完整padding、AS公共查询、各56事件往返/EID0。API group维度3，row1..3可能读取，分别增加1/17/257个frame-born非零已发布但无producer的冷buffer；原生不读取冷行，生产路径保持其字段重定位而不造内容。1KiB→64KiB/offset64→65532、Header0→64/贡献0→16/输出0→32、普通cross upload→same-submit slot7 Native GPU producer均无需再改后端。 |
| 恢复负例 | 声明窗口触及冷行、超过table extent、4byte读取只有2byte实际上传、缺typed来源/initial/upload、未来producer等都API4/CLI1且无GPUwait；前两个范围变体各19组、GPU变体18组。支持范围是buffer字段只读候选恢复；动态GPU writer、texture/AS字段选择需独立闭包，未宣称可用。 |
| 旧范围机制fresh | `bounded-namespace-final-producer-cross/`、`-retired/`、`-partial-texel/`；Native/capture/API/CLI均0，56/48/48事件；三者17/16/26坏组GPU前拒绝。合计本组6 fresh runtime capture 320事件、115坏组230 API/CLI拒绝；此是实际范围记录，不用案例数衡量进度。 |
| converted RayQuery / 相关回归 | `bounded-namespace-final-RT/manifest.json`，实际4253/4253/0/0、528uint、48事件、Private placement/64几何/TLAS/间接参数及绑定通过；复用原converted shader/reflection，非新官方或full RT。`bounded-namespace-final-related/development-regression-manifest.json`，29旧capture58 API/CLI通过（texel、多UAV、TraceRay、frame load、linked、depth及6/8MRT），非全量。 |
| UE拒绝 / 未完成 | `bounded-namespace-final-UE-pre-submit/manifest.json`，复用真实capture403f90ea、45s/3GiB监督，禁initial/frameGPU；API4，peak1420476416bytes，无监督停止、GPUwait、strict。offset1591744/PSO6399的动态范围仍未知，完整table25/count786432；slot20592非零candidate14714/range0..393216尚无内容/producer证明，commit4981760拒绝。与前一候选相同，没有声称突破UE；ID仅定位测试证据，尚无实际shader读取这个candidate的证据。 |

最终backend与bundle SHA256 `fb456ca18cd74b7dc8b788811cfc49437d9de4f9ac3f74f591f354460a48d194`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。证据索引 `development/bounded-namespace-evidence.json`、外盘可恢复检查点 `checkpoint-bounded-namespace-fb456ca1/manifest.json` 保存实际源码/产品/结果/日志哈希；不继承旧库PASS。持续任务metal已保留时间表同步最新规则，持久prompt逐字核验。

失败保留：`bounded-namespace-baseline` 的af26 Native/capture正确但API3 ranged metadata误拒；`bounded-namespace-closed` 的833候选CPU新测试断言失败（read路径未处理ranged namespace与test loader标记问题），非Native验收PASS；29cee4e1中间 `-validated` 修正后旧宽度sample已验，但不计最终PASS；c8候选 `bounded-namespace-stable-range-small` 原生/capture正确，volatile类型误识别API3，后续识别真实内存效果后fresh完整验收。测试只保证声明的候选读取范围/内容，不假称某候选已被真正shader访问。

完整UE GPU输出/事件/绑定、当前库最终官方sample、full IR/full RT/lifecycle/Qt-ARC/native设备能力集中验收仍未运行或未完成；历史崩溃/死机不因该scope声称解决。生产supportsRaytracing及FromRender保持false，B544/PHASE59与目标active。下一优先当前UE动态候选的真实物理backing/输入恢复义务与独立snapshot/生产者机制审计；不能仅添加CPU shader表达式、直接跳过未恢复行或把后来alias资源copy借给早期消费。重型UE GPU仍等待恢复闭包，避免同阻塞无效重跑。新产物全外盘，内盘31GiB、外盘1.6TiB；保留唯一失败证据，无用户RDHeaderView变更、reset/commit/push。


## B544 placement buffer 的物理字节恢复（固定 c573bda0）

本组实际修改 `metal_descriptor_tables.cpp` 的恢复区间，不新增 coverage 等级、场景格式/尺寸许可或 shader 数值模拟。已移除“换一个 ResourceId 就丢失同一 heap 已恢复内容”的逻辑对象限制；类型、设备 Native placement footprint、合法 offset/extent、生命周期、地址来源、初态/生产者、提交顺序与既有预算保持。此为资源/地址恢复机制整改；CPU 不知道实际值仅影响访问展示。动态 namespace 候选依旧不是实际 shader 访问。

### 实质修改前的本仓库对照

- DX12 `D3D12ResourceManager::Prepare_InitialState`（`d3d12_initstate.cpp`）：`Resource_Heap` 使用 `WrappedID3D12Heap::GetUnwrappedWholeMemBuffer`，placed buffer 不单独序列化初态。`D3D12ResourceManager::Apply_InitialState` 恢复 heap 的真实 whole-memory destination，经真实 copy source 与 barriers/`CopyBufferRegion` 恢复；缺 copy destination/source 不被当作成功。复用其“物理内存承载字节初态，逻辑资源只是投影”思路。
- Vulkan `WrappedVulkan::Prepare_InitialState`（`vk_initstate.cpp`）普通 buffer 的真实初态由 device memory 承担；`WrappedVulkan::Apply_InitialState(eResDeviceMemory)` 使用 `MemRefs::rangeRefs`、`InitReq`、`wholeMemBuf` 和实际 copy/fill 恢复物理区间，缺 memrefs 时保守完整恢复。复用其区间化内存恢复。该函数对 mid-frame memory 无 saved source 可采用 Clear，这是 API undefined 初态策略，**不是** Metal 将缺失已定义初态或 producer 猜为零的依据。
- 地址对应：DX12 `D3D12Descriptor::GetRefIDs` 由 CBV address 或已保存 descriptor 身份获取资源，`WrappedID3D12Device::CopyDescriptorsSimple` 传递实际 descriptor 引用。Vulkan `WrappedVulkan::vkCreateBuffer` 在捕获时记录真实 opaque capture/device address，并启用 capture/replay address flag；`WrappedVulkan::Serialise_vkBindBufferMemory2` 使用 `TrackReplayBufferAddress` 保存真实 memory/offset 关系。Metal 没有等价 opaque address 再分配保证，继续使用现有 typed 来源、当前发布和原始 VA→新 VA 重定位；不由物理区间猜指针。
- 提交对应：DX12 `WrappedID3D12CommandQueue::Serialise_ExecuteCommandLists` 的原提交及 `DataUploadSync`，Vulkan `WrappedVulkan::Serialise_vkQueueSubmit` 的真实队列依赖；Metal 沿用实际 command/submission、消费点冻结依赖与先后次序。未来 copy/compute 不能补之前读取。
- Metal 必要差异：Native Metal 没有上述可任意按 raw buffer 访问整个 texture/AS heap 的接口。使用既有 `indirectFootprint` 的实际 heap factory、`heapBufferSizeAndAlign`、逻辑 length 和 Native offset 校验，只在 buffer footprint 内投影已恢复字节；texture/AS 的 Native opaque footprint 不能被解释为 raw buffer 内容，也不新增 AS 内部查看。

### 新通用机制

`restoredPlacementBufferRanges` 按真实 `MTL::Heap*` 保存绝对区间；独立 allocation 保留按 ResourceId 保存。真实 captured buffer 初态在提交处理前统一播种，防止后来 alias 延迟读取旧 initial 复活已失效内容。upload、已恢复 source copy、完整资格后的 Native 精确必执行 GPU store 发布对应物理区间；读取按当前逻辑对象 offset/length 投影。实际 copy 在失效 destination 之前按值保存 source 区间，合法同 heap 不重叠复制保持证明；未知 source 覆盖清除目标物理范围。该机制不搬运 CPU scalar-known 或 typed 指针 provenance。旧对象退休后不能通过旧地址访问，新的合法对象可读取仍存在的真实 backing 字节。

保留单次 staging/整帧统一预算，不扩大数字上限；保持 producer 的资格、生命周期、Native 合法性、原始提交依赖及非零地址来源。未初始化 padding 不被升级为完整 allocation。conditional/ranged/atomic/raster 不能凭本机制伪造必须写入。

### 固定候选实际验证与限制

全部路径在共享锁下串行，产物位于外盘 `metal-ray-b544/development/physical-buffer-range-stable-*`，仓库 build 链接保留。

| 定向负载 | 实际改变 | 结果 |
|---|---|---|
| alias upload small | former heap offset0→新 buffer offset256，1KiB、定义4-byte offset64，真实 query 动态选择第三资源 | Native/capture/API/CLI PASS，56事件/EID0、完整定义输出 |
| alias upload offset | heap offset4096、64KiB、定义4-byte offset65532，Header64/贡献16/输出32，3线程窗口与17冷行 | PASS，56事件；未知 source 覆盖、disjoint alias、2-byte 不足4-byte读取等 GPU前拒绝 |
| alias GPU offset | 原 Native clz GPU store、slot7、heap offset4096、64KiB/offset65532、输出16 | PASS，56事件；CPU 值 Unknown，真实定义字3033；未来/缺 producer拒绝 |
| range GPU / retired / partial texel | 257冷行/同提交 GPU producer，以及相关实际部分初态/帧发布路径 | fresh PASS，56/48/48事件 |
| converted RayQuery | fresh native→capture→replay，复用已转换 shader，placement几何/TLAS/indirect | PASS，4253/4253/0/0、528uint、48事件 |
| 相关既有 capture | texel/multiUAV/TraceRay/frame-load/linked/depth(D16/D32/D32S8)/6与8MRT | 29 capture、58 API/CLI PASS；不当全套或所有 fresh |
| 原 UE capture bounded preflight | 45s/3GiB监督，initial upload/frame GPU禁用 | API4 / INCOMPLETE，peak1296728064bytes，无GPUwait/strict；内容/producer未恢复，非 UE PASS |

六 fresh runtime 合计320事件；121坏组242 API/CLI均实际 GPU前拒绝。位置/名称/EID仅定位测试，不决定后端行为。最终 backend 与 bundle dylib SHA256 `c573bda02044547798b55e05023faad52fb43b1f14cafd9386aba113fce1b04a`；GUI SHA256 `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。

失败保留：`physical-buffer-range-baseline` 的旧 fb456ca1 Native/capture 正确但新的合法 alias 内容误拒；`physical-buffer-range-closed` 为中间通过，非最终全集；`physical-buffer-range-final-alias-upload-offset` 曾因负例只移动256仍覆盖真实4byte而错误预期拒绝，API0使 fixture FAIL。修正为 Native 合法、真正不相交的范围后用 fresh stable captures 完整验证，不篡改旧失败，也不称原来仍相交的变化非法。

UE 没有因为本组泛化自动通过。原 capture SHA256 `403f90ea8c23638b8c817701725e31885c7929c09fac169a99e7728a66ac092b`，1591744/PSO6399 的真实 query API 效果已识别，table25 的非零候选 slot20592→buffer14714（heap3138、offset2250752、393216bytes）仍缺内容/生产者证明；没有证据称该候选实际被 shader 读取。`physical-backing-next-audit` 用 Native `heapTextureSizeAndAlignWithDescriptor` 查询45个捕获 texture 实际参数，无资源分配/提交；目标区间无 texture footprint 相交。后来15262/15263 的 copy 在消费之后，不能借用。此审计排除一项 backing 来源假设，不提供重放输出或 GPU producer 证明。

下一继续同一较大“通用资源恢复与重定位”批次：补 frame-born buffer 的实际 Native creation input/原内存恢复及生产者义务，区分 API undefined 与丢失已定义状态。不能无证据清零、默认没观察 writer 就已初始化、直接跳过全 namespace 候选，也不能以观察后快照掩盖缺失 producer。新增机制先 Native sample 变化偏移/alias/生产者/动态索引，稳定后才重复实际 UE。真实 GPU 指针/内容/依赖恢复完整时 Native 执行，展示保持 partial；不回到 CPU shader 表达式分析扩许可。

未运行/未完成：当前库官方 sample 最终验收、full IR/full RT/lifecycle/Qt-ARC、最终 Native 设备能力、实际 UE 整帧 GPU光追输出/事件/EID0/绑定；GPU-null/opaque writer 旧 fresh 路径本候选未重复。既有崩溃/死机不因该局部 scope 宣称解决。生产 supportsRaytracing/FromRender false，PHASE59/B544 与目标 active；不得提前开启或忽略失败。持续提示同步物理区间准则，源码/产品/结果/日志/失败与可恢复检查点外盘保存，不覆盖用户 RDHeaderView，不 reset/commit/push。


## B544 API 创建初态与 heap 内容分离（2026-10-08，568e50eb）

本组实际修改 `metal_descriptor_tables.cpp` 和 `metal_device.cpp`，分类属于 **API 初态保证 + 资源恢复机制**，不是 shader 展示完整性许可。Apple 的 [MTLDevice makeBuffer(length:options:)](https://developer.apple.com/documentation/metal/mtldevice/makebuffer(length:options:)) 明确保证新 buffer 清零；[MTLHeap makeBuffer(length:options:offset:)](https://developer.apple.com/documentation/metal/mtlheap/makebuffer(length:options:offset:)) 保留实际 heap placement/implicit-alias 语义，没有前一 API 的同等清零保证。两篇官方 Markdown 与 SHA256 保存在外盘 `development/API-creation-input-audit`。不将未保存的 heap 物理输入猜零。

### 修改前实际本仓库对照与复用

- DX12 `WrappedID3D12Device::Serialise_CreateCommittedResource`、`Serialise_CreatePlacedResource`（`d3d12_device_rescreate_wrap.cpp:650/694`）：捕获真实 props/heap offset/desc/initial state/GPU address，再调用 `Serialise_CreateResource` 创建对应 Native 类型；committed allocation 与 placed backing 分开。Metal 复用创建接口及原输入决定出生状态、真实地址由 Native factory 产生的思路，不据场景/shader/PSO hash 判定。Metal 独有的设备 buffer 零保证由其 API 本身提供，不能假称所有 DX12/Vulkan allocation 都有相同契约。
- DX12 `D3D12ResourceManager::Prepare_InitialState`/`Apply_InitialState`：前述 heap whole-memory 初态与真实 copySource 恢复继续保留；帧前旧资源的初态不能从其更早 factory 默认值重构。
- Vulkan `WrappedVulkan::Serialise_vkAllocateMemory`（`vk_resource_funcs.cpp:286`）恢复真实内存类型与 Native allocation；`WrappedVulkan::Apply_InitialState(eResDeviceMemory)`（`vk_initstate.cpp:2420` 附近）按 MemRefs/range/InitReq 执行 copy/fill，mid-frame 无 saved source 可按其 undefined-memory 初态策略 Clear。Metal 本组只依赖更强的真实 MTLDevice 契约，**不**照搬该策略去掩盖 heap 原数据、失效初态或缺 producer。
- 地址：DX12 `D3D12Descriptor::GetRefIDs`/`CopyDescriptorsSimple`、Vulkan `vkCreateBuffer` opaque capture address 与 `Serialise_vkBindBufferMemory2`/`TrackReplayBufferAddress`；Metal 的原 ResourceId+offset→新 Native VA、当前 typed publication、field/layout/生命周期校验不变，初态区间不被当成 typed 指针来源。
- 提交：DX12 `Serialise_ExecuteCommandLists`/`DataUploadSync` 与 Vulkan `Serialise_vkQueueSubmit` 的原队列和恢复同步继续对应 Metal 既有真实提交依赖。新初态只在 factory 的实际流位置播种，不能供出生前访问；seek/EID0 仍重建原 Native 对象，而非在 CPU 执行 shader 或另造固定结果。

### 实质变化、旧负例纠正及边界

frame 扫描/实际 replay 中，`MTLDevice_newBufferWithLength` 必须没有 initial payload，`MTLDevice_newBufferWithBytes` 必须有完整 length bytes，后者缺失不能隐式改成前者。合法 frame factory 出生后将其真实 API 初态登记为完整恢复范围；不用补一个应用未调用的 upload，也不把数值写成 CPU scalar-known 或伪造 shader access。Shared bytes 使用原 Native data；Shared/Private length 使用 Native 零保证，每次 seek 创建重做。已有 offset/extent/Native 设备、地址发布、生命周期/readonly、背景帧初态、真实 heap producer、未知来源覆盖、预算检查保留，上限与 coverage 等级不变。

**历史测试语义修正**：此前 `queryPartialHeap` 的非 alias 分支和 cold rows 实际使用 `MTLDevice newBuffer(length)`，将 padding/冷行全部称为 undefined 或将缺多余 upload 当作缺初态，依据不准确。旧 Native 正例只读取定义 word，其输出事实仍成立；旧库的真实拒绝日志保留，但相应负例不能永久成为通用 API 的拒绝依据。当前 fixture 的 producer-required 输入及冷行改为真正 Private placement heap buffer：对其未知输入、不足4byte的2byte upload、缺/未来 producer、未知-source 覆盖继续验证 GPU前拒绝。新增独立设备 creation-only Native sample 完全没有对应 upload/compute producer，验证 API 自己保证的真实初态。未扩大场景许可，也没删掉真实未恢复内容检查。

稳定候选 backend/bundle SHA256 `568e50ebca3c2bc241501400b3d5bb6c1bcd2fcecd9d53931894533b7990e998`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。所有构建/GPU验证串行共享锁，产物在外盘 `development/API-creation-*`。

| 当前候选负载 | 实际结果 |
|---|---|
| zero-private 64KiB/offset65532/Header64/贡献16/输出32 | fresh Native/capture/API/CLI PASS；完整64KiB API初值与输出、48事件往返/EID0 |
| zero-shared 16KiB/offset1024 | fresh PASS；完整原生零字节与输出、48事件 |
| bytes-shared 64KiB/offset65532/输出16/真实query API getter | fresh PASS；全 buffer 的原 pattern/3033/输出、48事件；缺真实 creation payload GPU前拒绝 |
| 实际 heap alias upload small/offset/GPU及257冷行 same-submit slot7 | fresh PASS，原 Native query/GPU3033字、4×56事件；真实缺 producer/冷行可达/越 table/未知覆盖等拒绝 |
| retired/部分texel、背景GPU初态+帧重发布 | fresh PASS，2×48事件；范围内真实恢复与原动态计算 |
| fresh converted RayQuery | PASS，4253/4253/0/0、528uint、48事件；实际 capture shader/reflection复用，非最终full |
| 29相关旧capture | 58 API/CLI PASS；非所有fresh/全套 |
| 当前库 UE | **未运行**；该机制不改变原拒绝目标的 heap 输入契约，无同阻塞重复重跑，也不继承旧库UE结论为当前PASS |

九 fresh runtime464事件，167坏组334实际GPU前拒绝；这只是验证范围记录，不作为进度衡量标准。`API-creation-stable-checks-manifest.json` 保留中间 FAIL，最终成功的三个创建路径与余下相关检查由各不可覆盖 manifest 共同证明。

失败保留：`API-creation-zero-baseline` 在旧 c573 Native/capture 正确，但 API3 因设备新建 buffer 内容证明缺失；新候选 `API-creation-zero-closed` 定向通过，非最终矩阵。`API-creation-stable-bytes-shared` 正例Native/capture/API/CLI均通过，但缺 payload 负例只修改 XML byteLength、没有移除 ZIP 实际数据，实际 openerAPI0而 fixture FAIL。修正真实数据包缺失并用 fresh `API-creation-final-bytes-shared` 验证，不改旧记录、也不声称那个仍有真实 payload 的 capture 缺输入。

`API-creation-input-audit/UE-factory-contract.json` 从原完整 XML 证明 resource14714 的实际 factory 是 heap3138/offset2250752，不是设备 buffer。原捕获SHA256 `403f90ea8c23638b8c817701725e31885c7929c09fac169a99e7728a66ac092b` 的实际RT整帧验收仍未完成；先前 c573 API4/禁GPU结果仅属于旧固定库。本候选没有恢复这个 heap 输入，不能给它默认零或称这次修复推进了UE消费点。下一必须补真实 heap 原内存/creation input、实际GPU指针与生产者恢复义务。观察后的快照不能借给更早消费或掩盖GPU发布缺 producer，metadata residency不能当实际初始化；所有受声明支持的合法变化继续用独立 Native 执行验证。

当前库最终官方sample/full IR/full RT/lifecycle/Qt-ARC/Native设备能力、实际UE整帧光追输出/事件/EID0/绑定仍未验；旧GPU-null/opaque相关fresh本候选未重复。生产 supportsRaytracing/FromRender false，目标/PHASE59/B544 active。历史UE/Qt崩溃与死机未因局部测试宣称解决。AGENTS与持续提示覆盖旧场景策略及本次API初态纠正，检查点、失败和全部当前结果外盘保留，不改用户RDHeaderView，不reset/commit/push。
