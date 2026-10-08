# B498：六类真实光追管线创建与诊断验收

2026-10-06，PHASE58。补齐B496/B497尚未实际执行的五个compute创建入口验证，新增六入口真实opaque triangle ray query捕获/重放gate，并修正生命周期检查器漏认帧前AS+无函数表inline query的问题。未启动UE，未改后端库；生产RT能力仍false。

## 实现与参照

Vulkan `wrappers/vk_shader_funcs.cpp` 的 compute/ray管线创建及 DX12 native pipeline/state object 创建均保留原生结果，再记录/重放资源。测试验证Metal对应已有同步/异步包装，不替换或再次调用原生创建API。每个入口使用不同的trace0–trace5 kernel，native只创建一次；三个async通过completion handler返回并保留结果，10s等待/30s进程上限，后续真实ray调度验证结果。

新增 `metal_ray_compile_capture.mm`、`metal_ray_compile_replay.cpp`、`metal_ray_compile_invalid.py` 与串行 `test_metal_ray_compile_macos.sh`。固定一个opaque triangle BLAS、两个ray（hit/miss）、6个PSO/6个dispatch，无循环渲染/UE。AS帧前构建；输出12个uint初始均7。每种创建方式输出1/0，构建与调度Metal验证无错误。所有输入与帧数固定。

## 最终实际结果

| 创建入口 | function | options | native begin/end thread | 耗时ms | query/capture/replay |
| --- | --- | --- | --- | --- | --- |
| function-sync | trace0 | 0 | 152931/152931 | 0.235 | PASS 1/0 |
| function-options-sync | trace1 | ArgumentInfo=1 | 152931/152931 | 0.069 | PASS 1/0 |
| descriptor-sync | trace2 | ArgumentInfo=1 | 152931/152931 | 0.082 | PASS 1/0 |
| function-async | trace3 | 0 | 152931/152933 | 0.149 | PASS 1/0 |
| function-options-async | trace4 | ArgumentInfo=1 | 152931/152933 | 0.126 | PASS 1/0 |
| descriptor-async | trace5 | ArgumentInfo=1 | 152931/152933 | 0.111 | PASS 1/0 |

`enabled-compile-trace.json`为COMPLETE TRACE：6 begin/6 native end、3个不同async token submitted、无pending/native failure。每条核验function、options、linked/binary/private=0和非零native thread；sync begin/end同线程，async实际在另一线程callback。记录完整本身不证明RT，独立ray输出/AS绑定/dispatch oracle才是调度证据。

trace开关5组：精确1记录；unset/0/true/空字符串均NOT OBSERVED。五组capture的六个query输出都与native一致，证明关闭诊断时原API仍工作。只有enabled capture作完整API/CLI/坏输入/生命周期验收，其余四份只计native与capture输出，不宣称离线验收。

离线enabled capture的6种creation chunk ID各一次、6个不同PSO，32个全部事件×正/反/再正=96次PASS；每次先EID0核验12个uint恢复为7，按GPU事件前缀/CPU原chunk fileOffset分别核验已执行与未执行结果，每次dispatch核验对应PSO与实际AS资源ID。两个历史同步API显示名相同，用chunkID区分，未更改旧名称/编号。CLI3 loops PASS。

23坏capture PASS：各API的零PSO、未知function、将已存在buffer冒充function，以及5份有optionsValue记录的invalid options16。均干净失败于被修改的具体creation chunk，拒绝信号/超时或无关失败。function-async的既有serializer也携带optionsValue=0，因此该API同样验选项反例。

旧基础/inline/sampler/transfer sentinel `--sentinel t36 t37 t38 t39 t40 t41`展开15份，另针对pipeline variants/binary library/async results/events追加t47/t48/t51/t52四份；合计19份API/CLI PASS。新增gate今后一次选齐19份，本轮15+4分两次串行执行。生命周期t35+B497帧内函数表+B498 capture，共3×10 PASS，growth589824bytes，exit0、起止backend hash一致。不是75份RT全套或集中308份验收。

## 修正及失败证据

- 首次API oracle按AS显示label找资源，AS未在GetResources中提供该label，循环开始前失败。改为从实际setAccelerationStructure的typed ResourceId取得并检查每次绑定一致，未改backend label行为。`replay-before-AS-binding-oracle-fix.log`保留。
- 首次坏例脚本期待serializer读取chunk错误，实际creation验证正确返回“Failed to process Metal chunk <被修改API>”。改为核验该具体创建API，未扩大到任意错误。`invalid-before-creation-error-oracle-fix.log`保留。
- 原生命周期检查器仅按IFT创建/若干AS build/refit名字识别compute ray，误拒绝帧前AS、无IFT的opaque inline query（iteration0）。新增实际非零AS绑定且存在Dispatch的识别，专门输出仍由ray oracle证明；不把generic compute当ray。修前日志/manifest在`before-inline-ray-lifecycle-oracle-fix/`，修后3×10通过。未改产品支持范围。

## UE IR ABI 实际审查

新增CPU-only `metal_ir_ray_layout.mm`，直接包含本机UE_5.8 MetalShaderConverter自带Apache-2.0头文件，未复制第三方实现；实际编译运行，不以手算布局作实现依据。`ir_raytracing.h` SHA256 `2fc410d918442c074abce207ab94fd013a29bc0f8ec3d54592ef3a99a8839922`。layout日志/manifest记录：

- IRDispatchRaysArgument=152bytes，descriptor=104，IRShaderIdentifier=32。
- shader table ranges起点raygen0/miss16/hit40/callable64，Width88。
- GRS104、ResDescHeap112、SmpDescHeap120、VFT128、IFT136、IFTs144。
- shader record static sampler地址位于offset16。

实际UE `GetDispatchRaysDesc`包含四段SBT地址、size/stride；无callable时为空，HitGroup禁用索引时stride允许0。`DispatchRays`把参数绑定到IR bind point3。GRS还含UB与static sampler地址；shader record含local root/static sampler地址；AS GPU header含ASID与instance contributions地址。不得只补VFT/IFT字段便称为完整dispatch。

实际UE配置 `IRIntersectionFunctionCompilationVisibleFunction`，shaderIdentifier的前两个64bit字段是table index，不能把它们当gpuResourceID改写。进一步typed声明必须保留此语义以及record stride/范围/null状态。DX12 `d3d12_command_list4_wrap.cpp::Serialise_DispatchRays`经 `D3D12RTManager::PatchRayDispatch`复制、patch已声明shader records；VK `vk_draw_funcs.cpp::Serialise_vkCmdTraceRaysKHR`保留typed SBT区域，支持capture replay handles。Metal补这一对应边界有必要，但本批只有来源/布局证据，**尚未实现IR packet/SBT重定位**。

## 哈希、产物与接续

backend/bundle仍为 `eea2539fb069dae9d59d2c32ad962c562ee83aecb09d57be195e50538f123c57`；GUI仍为 `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。没有重建或替换库，source/binary manifest核验一致；B497同哈希官方两scene等证据仍有效，本批没有重复运行官方sample。

产物 `build-macos-debug/metal-ray-b498/manifest.json`、gate/native/capture/replay/CLI/invalid/old-pipelines/lifecycle日志、5组renderdoc.log和compile-trace.json、IR layout及source-binary-hashes.json；5份capture位于 `captures/metal-ray-b498/`。Python/shell语法与diff检查PASS。仅新增helper构建及targeted增量renderdoccmd（无库变化），不宣称新产品修改。

UE实际待编译函数仍未知，本fixture不能证明旧MTLCompilerService/WindowServer停滞根因修复。未运行UE、75份RT、新库集中、GUI原Qt crash、ARC父对象验收。继续实际IR typed packet/SBT/GRS/AS header依赖，然后具备具体输入/停止条件的UE诊断；不直接重跑旧长UE/full followup。两生产能力false，未提交/推送，持续任务未达终点。
