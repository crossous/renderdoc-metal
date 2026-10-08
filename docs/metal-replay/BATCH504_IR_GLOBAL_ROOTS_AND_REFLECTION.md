# B504：多根 IR 全局参数、实际 SDK reflection 与串行验收

接续 PHASE59/B503，完成实际功能缺口：GRS不再固定只有AS-header和UAV两根，新增不可变PSO全局布局及整个资源闭包验证/重定位。生产compute/render光追能力仍false，未运行UE。

## 对照与接口

先对照仓库DX12 `D3D12RTManager::PatchRayDispatch`（`d3d12_manager.cpp:1630`）的shader record/root-signature参数地址关联及typed依赖，Vulkan `Serialise_vkCmdTraceRaysKHR`（`vk_draw_funcs.cpp:5018`）的明确SBT区域语义。沿用既有Metal资源身份和初态重定位；IR特有的GRS字节布局单独声明，没有猜任意整数或shader名字。

新增 `metal.rayIRGlobalRoot`：对已声明IR dispatch的compute PSO调用UInt64四元组 `[offset,kind,count,bytes]`。kind0只读buffer SRV、1为32-bit constants、2为24-byte IR texture table、3为24-byte IR sampler table、4为CBV、5为64-byte AS header、6为uint32 UAV。kind5/6各唯一，布局≤64项、每source≤64KiB；CBV地址256对齐、SRV4对齐、指针field8对齐/constants4对齐，table count×24与bytes一致。参数shape、重叠、对象/PSO身份、帧内声明均拒绝；合法重复幂等且不多写chunk。

追加chunk1431 `MTLComputePipelineState::DeclareRayIRGlobalRoot`，Max1432。dispatch schema1的rootCount现在是2–256个64-bit GRS words；原有rootCount2且无显式global布局仍按AS-header/UAV两根解释，旧capture兼容。多根缺声明、GRS每个byte未覆盖（即使为0）、根指针缺typed field、未知资源/不足view、texture/sampler错类型或不支持metadata、UAV覆盖任一输入都在调度前拒绝。所有global buffer/table/texture/sampler加入CS read usage。帧内实际CPU变更仍拒绝，EID0恢复原始初态再重定位。覆盖范围仍为Shared静态独立buffer，无heap/callable/Private或GPU生成IR来源。

## 真实转换 sample

继续使用B500已审查UE5.8.3 DXC→Apple Metal IR converter/runtime与自产MIT HLSL，不修改上游。root signature包含六参数：AS SRV、output UAV、CBV b0/space2、四constants b1/space2、raw SRV t1/space2、texture table t2/space2；SDK在末槽追加六static samplers，合计64bytes/8words。偏移0/8/16/24(16bytes)/40/48/56；CBV取512-byte backing的256偏移值1000，raw buffer偏移4值5，constant200，texture线性采样6，global贡献 **1211**。sampler实际读取s3/space2，即slot3；不是仅创建闲置sampler。

converter保存SDK实际 `IRVersionedRootSignatureDescriptorCopyJSONString` 与四export的 `IRObjectGetReflection` JSON，并保存每份metallib/hash。五个新模式共20份entry/stage/TopLevelArgumentBuffer布局CPU核验通过；只读有界实际数组，不把ResourceCount=0xffffffff sentinel或空Name当有效资源列表。该证据属于本sample，不能替代UE自动关联，也不是 replay许可。

| 模式 | native/capture/replay两个uint32输出 | 每方向事件 |
| --- | --- | --- |
| global-root | 1284 / 1222 | 21 |
| ue-global-root-any-hit | 1222 / 1222 | 21 |
| ue-global-root-null-miss | 1284 / 1218 | 21 |
| ue-global-local-root-six-samplers | 1342 / 1454 | 26 |
| ue-global-local-root-six-samplers-any-hit | 1454 / 1454 | 26 |

新增五模式345次事件选择，旧15模式也全部重新native/capture/API/CLI运行810次，最终 **20组/1155次** 三方向选择、每次EID0重置、slot3绑定、完整record/pad/constants/资源usage和CLI每组3loops通过。AS ID2→3、SBT VA变化、global texture1→2、组合local texture2→3均实际观察。global selected sampler slot3为32、组合local为38；实际sampler未强制变化，另以合法一致捕获ID重编号控制验证重定位，未宣称自然分配顺序改变了sampler ID。

## 最终回归与二进制

backend/bundle SHA256：`5566f91b4ca1bc6183fbab177d4c1d7d2c17eeacadc69f218fb2164d2945bf89`；GUI：`3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。renderdoccmd与app构建串行通过，最终验收起止hash一致。

新global根反例在global-only和global+local两份capture各 **70拒绝/3合法控制**：shape/pipeline/count/bytes/overlap/重复/帧内、缺根/缺GPU字段、rootWords边界、CBV/SRV范围与对齐、texture/sampler元数据与身份、UAV输入别名、commit snapshot改写。合法控制含一致sampler ID重编号、扩大但仍在source内的CBV与SRV view；CLI/API输出均通过。组合重编号同步更新所有引用同一captured sampler ID的typed表，不破坏局部别名。

旧default69/7 + 两local各58/4重新通过，最终typed IR **325拒绝/21合法控制**。仅正常非零exit且匹配预期理由算拒绝，signal/timeout不算通过。当前库18份旧targeted API/CLI、41旧argument反例、6旧B500 packet、旧B501 IR/旧B502 any-hit IR、无声明raw IR预期拒绝、6项能力/精确probe查询通过。官方Apple MIT triangle/procedural两scene重新native重复/capture/逐字节API输出/270事件/EID0/CLI3、10尺寸查询/41坏sample通过。t35、新global+local IR及官方两scene **4×10重开**通过，resident growth **1441792bytes**；不是ARC父对象任意提前释放证明。

Python/Bash语法、diff检查通过。GPU/构建最终串行，无UE/Qt参加；native固定两条ray/一次dispatch、超时有限。独立IR gates新增同一进程生命周期互斥文件，CPU `--help`探针在另一gate持锁时阻塞，通过后才继续，防止这些gates独立调用时重叠。不能保证阻止系统级死锁。

## UE关联的实际限制

只读审查本机UE5.8.3源码并保存hash（`association-source-hashes.json`）：

- `MetalRayShader.cpp:35–46`从FMetalCodeHeader取得stage、entry与global/local root params；`:143–202`另建root signature。
- `MetalBaseShader.h:165–301`只将已编译shader bytecode加载为Metal library；header不在该dispatch_data中。`:458–516`之后可能延迟创建function。
- `MetalCompileShaderMSC.cpp:1285`普通shader转换/局部签名发生在离线构建；runtime `MetalRayTracing.cpp:261`cache并非普通shader重新转换，`:1103/1134`只合成dispatch/intersection wrapper。
- `MetalRayTracing.cpp:2767–2914`真实GRS包含static/dynamic UB，static sampler在最后slot；packet同时填写nonzero ResDescHeap/SmpDescHeap。

因此仅hook runtime converter或把最近一次IRRootSignatureCreate与最近加载metallib按顺序配对都不能可靠证明关联。下一需要在既有隔离MetalRHI producer按已知shader/header/library/export/function/PSO传递元数据，同时补实际heap/nested UB/typed buffer-view语义。本批sample使用显式声明且两heap为0，不冒充UE支持。

## 失败与产物

首次20模式运行与首个global反例gate发生短暂GPU重叠，已告知并保留 `pre-lock-attempt/` 与 `first/global-invalid/`，不计最终验收；新增互斥后完整20组及所有反例/回归串行重跑。首次附加reflection CPU审计目录过滤误选global-invalid目录而报缺文件，修正为五个精确mode后20项通过；没有额外GPU测试。早期只用native的 `native-proof/` 保留。

最终产物：`captures/metal-ray-b504/final/<mode>/ir-runtime_capture.rdc`；`build-macos-debug/metal-ray-b504/final/<mode>/manifest.json`、DXIL/六metallib/SDK root与reflection JSON、ABI/compile/API/CLI日志；`modes-manifest.json`；五组invalid目录；`remaining-manifest.json`；`apple-sample/gate-results/`；`SDK-resource-layout-evidence.json`；生命周期日志；`final/manifest.json`、`source-binary-hashes.json`。旧B503库保留于 `baseline/librenderdoc-b503.dylib`。

官方ZIP继续使用固定SHA `4ee961f8eac0b15232c0ff29066c1e39672e2dd29f855d0c062cdd686accaf94` 与MIT许可证。工具/runtime来源版本/hash由每mode manifest记录。可复现入口 `test_metal_ir_ray_macos.sh` 更新为20模式和default/两local/两global反例gates；本批实际串行运行各子命令，没有再执行整轮shell包装器，不重复计通过。

本批未运行实际UE RT/离线、75份RT全覆盖、集中308、原Qt sizeHint复现、ARC提前释放或独立旧descriptor十反例。heap/callable、nested UB、Private/GPU IR、一般texture/sampler metadata、render RT仍缺；AS内部查看/RT shader单步/RT Pixel History不承诺。B495死机根因仍未修，不直接旧长UE/full。能力false，持续任务active，未提交/推送。
