# B521：转换 inline RayQuery 的独立 typed dispatch

实际UE Lumen AIR执行inline query，与已有TraceRay的152-byte packet、SBT/VFT/IFT不同。本批参考Vulkan `vk_descriptor_funcs.cpp:1524` 对AS descriptor的明确类型解包及DX12 `d3d12_replay.cpp:859` 的AS SRV地址→资源关联；沿普通compute调度捕获、保存类型依赖、初态重建与重放地址替换。没有承诺RT shader单步、AS内部查看或RT Pixel History。

新增append chunk `MTLComputePipelineState::DeclareRayQueryDispatch`（1436）及background-only `metal.rayQueryDispatch` 注解：PSO对应roots、byte offset、header、output。复用coverage3/schema0 GPU VA/schema4 AS-ID，query禁止同PSO IR声明，schema5/6继续只允许真正IR。Shared standalone非alias buffers≤16KiB、roots两word/8-byte alignment、header64字节且保留字为0、uint32输出按1D实际非零调度检查（≤4096线程），slot2绑定必须匹配。静态初态普通TLAS≤64实例，子BLAS与贡献pointer明确关联，输出禁止用作AS构建输入或descriptor table。预检唯一可采信creation bytes缓存后用于EID0；与已有IR同样保护参数不可变、CPU更新/encoder顺序和usage。未声明的query保持拒绝。

自有MIT HLSL使用`RayQuery::TraceRayInline`/`Proceed`/`CommittedRayT`，DXC cs_6_5→本机Apple converter的真实Compute metallib；新增converter inline-query入口保留旧TraceRay入口。四条ray覆盖两个distance1命中、空间miss、TMax=0.5未命中，native/capture/replay全为1000/1000/0/0。实际SDK reflection UsesRayQuery=true、SRV0/UAV8与typed声明一致；捕获newLibraryWithURL字节SHA匹配native metallib，唯一非零groups1/threads4调度与AIR allocate/reset/next_intersection_query证据保存在各manifest/LL。外部DXC/converter/runtime来源、版本及hash沿用本机UE固定依赖，新source和产物精确SHA在manifest内；自有源码不冒充官方sample。

最终backend/bundle SHA256 `e72514c67e5e05793f1bf4138570daf0d2a6b50a628c3b70fef223e4541043bd`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。最终库及manifest在`build-macos-debug/metal-ray-b521/`。

| 实际范围 | 结果及产物 |
| --- | --- |
| root offset0/8两场景 | 两份native/capture/API/CLI3 PASS；`final-verified-query`、`verified-query-offset8`，32事件/EID0、强制buffer VA与AS-ID变化、offset8两端保护字 |
| 39损坏组 | `query-invalid` 78 API/CLI预提交拒绝 PASS，无GPU wait；声明/同资源/PSO/布局/根pointer/AS null未知及BLAS/贡献/保留字/grid/slot/输出越界 |
| 未声明控制 | `untyped-control` native/capture PASS，API4/CLI1预期拒绝，未提前启用RT |
| 旧纹理/view/heap | `regressions` B514–520五十原capture API/CLI3 PASS；另旧RGBA8/R16 view两native/四capture/API/CLI3 PASS |
| 共享IR逻辑回归 | 原B512 multi64、global-local-sampler-anyhit、global-heap-Private-frame-multi及新目标heap-Private-frame-multi四capture API/CLI3 PASS；`TraceRay-default` fresh conversion/native/capture/API/CLI3 PASS |
| 官方Apple MIT sample | `regressions/official` 两scene、270事件/EID0、10查询、CLI3 PASS；固定archive SHA4ee961f8…，13坏官方sample NOT RUN |
| 六能力/CPU预算/语法/diff | PASS；supportsRaytracing和supportsRaytracingFromRender均false |
| UE | 原帧CPU65 mandatory-exit仍total预算拒绝；本轮未启动UE GPU/新capture/输出重放 |
| 完整78IR/75RT/308、旧84间接反例/32CPU-AIR/Qt/ARC | 当前库 NOT RUN，旧结果不继承 |

严格扫描588完成日志无断言、overrun、非法stream seek、未知Metal类型、资源表未空或缺geometry snapshot；所有构建/GPU使用同一互斥锁，有限超时和只终止自有进程。最终hash一致。

失败记录：`baseline` 注入capture -11，同CB BLAS→TLAS现有guard拒绝后无drawable导致丢capture/资源表断言；已将fixture两个AS build各自提交完成并取得drawable，未删除AS guard，也没有证明这是用户UE ForceCrash根因。后续macro同一行导致__LINE__冲突、chunk Max sentinel、fixture API命名、replayer OpenCapture/Result Message链接失败全部保留；首次输出无InitialContents的closure拒绝通过权威creation bytes缓存修复。AS标签未捕获导致oracle寻找错误对象，改为按header初态和AS GPU identity定位；生产AS label缺口未修。`verified-query` 强化验收误认库创建接口导致末尾哈希检查失败，API/CLI已通过，修为URL/data字节检查后完整重新验收。以上失败目录不纳入干净日志统计、不计为通过。

当前仅初始普通TLAS kind5，不支持query的间接TLAS/帧内更新、Private或GPU header/root、资源heap query、render混合或indirect/dispatchThreads。B513真实UE是间接TLAS且Private/suballocated header、typed AS producer缺失，并有5.267GB heap/4.362GB初态的预算问题；不能将小fixture通过计为UE能力。下一B522先复用既有kind9/10/11 typed间接AS初态供query，再扩GPU/header producer与实际UE小工作集场景。能力开关仍false，持续任务未完成；用户Qt/AS崩溃和系统冻结未闭环，未提交推送。
