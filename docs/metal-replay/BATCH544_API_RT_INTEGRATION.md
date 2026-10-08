2026-10-08 UI 交付候选已备齐：[1280×720 UE Lumen/Nanite/VSM 唯一当前卡](UE_UI_CAPTURE_2026-10-08.md)。交付 c827cc7e 捕获在 backend/bundle1845eca8完成三整帧GPU重放与两次EID0；同帧Native像素比较FAIL（18133/23804/15578像素），整体INCOMPLETE，GUI手工验收待用户。独立direct/indirect/wide view/并发Drawable、五attachmentless与真实契约负例、官方Metal2均在同候选实跑通过；完整converted矩阵本次未跑。fresh1845的a877捕获Native成功但背景placement重叠replay拒绝，根因未闭合、不交付为正常文件。旧f1 compute RT完成证据独立，render RT不开启；文件、命令与manifest全外盘。

2026-10-08：对应 compute RT 开启验收已完成。当前 backend/bundle `f1ba1862`，`supportsRaytracing` 跟随 Native 设备，公开路径无需 probe；`supportsRaytracingFromRender` 保持false。开启后官方Metal2、真实UE/Lumen整帧三次EID0与同帧Native零差、query事件/绑定、独立合法变体、相关生命周期/frame及完整转换矩阵均在同候选实跑通过。GPU COMPLETED / 输出 MATCH（UE EXACT_MATCH）/ overall PASS仅对应记录的compute/API/预算及本机设备范围；Metal3扩展、render-stage、物理不支持设备实跑及原Qt GUI崩溃未认证。旧1dc FAIL不改写，旧hash不继承。[最终唯一记录](HEAP_BACKING_RECOVERY_2026-10-08.md#compute-rt-能力开启与最终验收完成f1ba18622026-10-08)。持续任务已按达标约定停止（metal automation已删除），不继续重跑或自动扩展验收范围。

2026-10-08 当前c88d10d4：按需控制输入/真实CPU恢复分离、独立Native、第二份UE无损整帧/reset/query绑定及scoped T2通过；M4生产捕获流程与固定最终验收仍开放，两个生产RT flagsfalse。[当前唯一证据](HEAP_BACKING_RECOVERY_2026-10-08.md#按需控制输入与跨捕获恢复候选-c88d10d4)，外盘scalar-interval-evidence.json；历史结果只属于其hash。

2026-10-08 当前c54b5fb4：通用间接恢复/实际GPU预算已整改，独立Native/T2通过，真实UE整帧原JPEG逐字节一致、两次EID0 raw输出一致及5个query事件绑定/往返通过。M3跨捕获/无损基准与M4最终启用/生产路径未完，两flagsfalse；[当前唯一证据](HEAP_BACKING_RECOVERY_2026-10-08.md#原生间接调度恢复与执行参数分离候选-c54b5fb4)。旧hash结果独立。

2026-10-08 候选 f3e2d595：来源/普通GPU数据通用整改及定向/相关集成通过；UE整帧GPU命令完成但单一间接数值差异，输出验收未完成。[唯一记录](HEAP_BACKING_RECOVERY_2026-10-08.md#指针条件来源与普通-gpu-贡献数据恢复候选-f3e2d595)；旧hash证据不算本库通过，两RT flagsfalse。

2026-10-08 当前候选 d152c324：Native索引真实恢复与可选CPU投影已分离，完整独立变体/T2通过，UE仍在后续指针來源阻塞；[唯一批次记录](HEAP_BACKING_RECOVERY_2026-10-08.md#native-索引输入恢复与可选数值投影候选-d152c324)。旧候选结果不算本库通过。

2026-10-08 当前M1候选 `340e330b`：Native深度/比较sampler与实际绑定恢复、完整独立Native/相关验证及UE索引分析阻塞见 [唯一批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#native-深度采样与实际绑定恢复候选-340e330b)。M2整帧GPU未完成，两RT flagsfalse，旧hash范围独立。

2026-10-08 当前M1候选 `7b46ae65`：同module实际函数/helper参数恢复、真实Native与UE未完成阶段见 [唯一批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#同模块函数身份与原生控制效果候选-7b46ae65)。两RT flagsfalse；旧hash范围独立。

2026-10-08 当前M1候选 `29c8aa53`：入口属性/typed table恢复、真实Native与UE未完成阶段见 [唯一批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#入口属性与-typed-table-实际绑定恢复候选-29c8aa53)。两RT flagsfalse；历史记录只属于其hash。

2026-10-08 当前M1候选 `9d987f9f`：shader自有数值常量/外部来源分离、独立Native及UE实际未完成阶段见 [唯一批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#shader-自带数值常量与外部缓冲来源分离当前候选-9d987f9f)。两RT flagsfalse，旧记录仅属于其hash。

2026-10-08 当前M1整数纹理ABI/规则格式恢复候选 `d22c3c1d`：实际机制、独立Native完整输出/事件、相关回归及UE尚未完成范围见 [唯一批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#整数纹理-abi-与规则格式字节恢复当前候选-d22c3c1d)。两RT flagsfalse，旧记录仅对应其hash。

2026-10-08 当前候选`6b35e12e`的原纹理同步、子资源生产链、真实Native与UE阶段见 [当前M1批次卡](HEAP_BACKING_RECOVERY_2026-10-08.md#原生纹理同步与投影子资源生产链当前候选-6b35e12e)。RT未开启，历史hash不计当前验收。

2026-10-08 当前执行策略采用 [复盘](EXECUTION_REVIEW_2026-10-08.md)；当前M1结果只见 [heap backing闭环记录](HEAP_BACKING_RECOVERY_2026-10-08.md)。冲突的历史场景准入、CPU表达式/producer证明义务和旧负例oracle已替代；历史测试不算当前候选验收，最终RT开启门槛保持。

2026-10-07 最新任务整改（覆盖历史场景许可策略）：先完成一组通用资源恢复与重定位机制，再继续 UE/Lumen 新案例。实际修改纹理/frame-view/coverage/runtime AIR preflight，区分 API/设备、恢复缺口、统一预算、历史场景限制与展示分析；coverage 不积累场景等级，Native 动态计算与 partial 展示独立。具体执行规则、逐函数 DX12/Vulkan 对照及剩余边界见 [GENERIC_API_RECOVERY.md](GENERIC_API_RECOVERY.md) 与根 AGENTS.md；光追开启门槛不变。持续任务 metal 已同步。

# B544 — 通用根参数、调度与资源闭包集成（开发中）

2026-10-07，PHASE59；本批未完成最终验收。遵循新的[集成推进规则](RT_API_INTEGRATION_WORKFLOW.md)，继续完整光追启用目标，不将 Lumen 专用效果或单个 sample 的通过定义为完成。

本批集成范围是 converted compute 的类型化根布局与资源范围、多维间接调度、descriptor/AS Header消费依赖和资源生命期，随后回到实际UE短capture/replay；需要的Private/帧内CBV、buffer/texture输出及静态sampler根语义要按底层API闭包补齐。开发中的必要定向检查与最终集中回归分开记录，不为每个小接口重复全部验收。

## 已有实质实现和开发证据

- 对照DX12根CBV按原地址解析已知资源+offset的做法，以及Vulkan按执行点记录间接参数的方法。新增PSO immutable CBV读范围声明（可配置1–16根、每根≤64KiB）；所有根必须完整、无重叠/重复，源由已知buffer身份及实际native VA解析，范围/对齐、初态、storage、写入和资源角色核验。复用inline descriptor来源与重定位，不按Lumen/UE shader名称分支。
- converted query可从类型化资源heap UAV slot写结果，AS Header/贡献/AS recipe保留原验证。CBV路径按所有grid/thread维度检查乘法与工作量上限，保留原生间接命令及逐次GPU参数核验；旧16-byte路径边界保留。
- 纯CBV阶段库/bundle SHA `21b6dec8481958a84b48f39b0412e78c13346ced864c7974e94e6591b73a3fb3` 构建通过，**是开发库，尚未做最终批次验收**。两生产光追flags仍false；已完成B543冻结库仍可在其外盘final目录恢复/检视。
- 真正DXC→IR converter的5/6 CBV shader，每个根实际影响ray或UAV数据，分别构成40/48-byte GRS。40-byte场景 Native/capture通过，修正测试oracle后独立API返回0：528 uint输出3387/3387/0/0、48次事件选择/EID0、所有CBV数据/padding/只读usage通过。首次完整gate因oracle误读结构字段及漏读creation contents失败，保留开发目录；不把其失败manifest改为PASS，CLI/完整gate尚需最终集成验收。
- 48-byte、多维GPU参数、Private placement64几何/64实例、unretained命令场景 `development/cbv6-2D` 的Native/capture/API/CLI完整gate通过，输出4426/4426/0/0，所有528 uint、56次事件选择/EID0、CBV全数据/padding/只读usage、GPU参数/AS身份及贡献地址核验。实际中间dispatch为 `[2,66,1]`，保留第一 `[1,1,1]` 和最后 `[0,66,1]`；没有以总132的一维调度代替原生二维命令。源码/helper/DXIL/metallib/capture/库/SDK哈希保存在manifest。
- 当时21b6开发库的三项旧行为定向回归（同encoder间接query、直接multi64 query、B512 multi64 TraceRay）各API/CLI通过，共六项；这是开发定向检查，不是官方sample/整套回归或本批最终验收。
- 开发中普通40/48-byte fixture不是实际UE完整布局。B542实际根表末项还含static sampler table，另有帧内CBV和texture/Private UAV资源；不声称已通过真实UE。接下来扩展公共类型化root/资源闭包，避免为应用硬编码slot或shader角色。

产物均在 `build-macos-debug/metal-ray-b544` 外盘链接下；当前开发入口 `build.py`、`development-regression.py`及 `util/test/metal/metal_ir_query_gate.py --cbv-roots/--query-grid-height`。build/GPU串行共享既有锁、有限帧数与超时，没有UE/qrenderdoc重叠。源码序列化宏同一行重复及enum Max未更新导致的两次构建失败已经修正；第二失败完整日志、第一次失败工具输出/诊断以及fixture失败均保留/明确区分。

## 混合根/Private初态CBV开发检查点（同批继续，未集中验收）

参照DX12 `d3d12_command_list_wrap.cpp:1979` 的 root CBV已知resource+offset及GPU VA重放、`d3d12_replay.cpp:1515` static sampler root-layout与descriptor呈现，Vulkan `vk_descriptor_funcs.cpp` immutable sampler资源parent，以及 `vk_cmd_funcs.cpp:3802` 执行点descriptor set/dynamic offset。Metal共用既有 `RayIRLocalRoot`/`RayIRHeapEntry` ABI类型，追加 `DeclareIRComputeRoot` / `DeclareIRComputeHeapEntry` 两chunk（Max1442、既有编号保留），公共annotation `metal.irComputeRoot` / `metal.irComputeHeapEntry`。目前consumer支持完整CBV/static sampler根、typed heap texture SRV；不根据Lumen/shader名称/slot用途猜测类型。各类型仍明确限定已经实现的范围。

- 根表必须完整、唯一、与每根读范围一致；sampler tail有声明schema2与每项type7/source-kind2的live slot，完整初态与当前shadow一致，已知对象匹配原生ID并重定位；scalar metadata和padding不作为resource ID，当前支持LOD bias0。纹理读slot必须known texture source/type4、完整初态、readonly Shared standalone RGBA8 2D单mip≤64²。CBV可读完整初态≤64KiB的Private standalone/Tracked placement，不把CBV数据当raw pointer扫描或要求CPU contents；继续核验地址唯一性/范围/别名/角色与写入边界。帧内CBV/GPU生产尚未开放。
- 当前 **开发库和bundle** SHA `e8f709bf05db8dc6e60b35b2c54ab7e77ecf6e3e641cba025e41e58329a00314`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。构建终结0，不是最终验收库。哈希清单 `development/checkpoint-e8f-hashes.json`，全部活动产物在既有B544外盘链接下。
- `development/mixed5-private-40`：四个Private standalone CBV offset64+末尾六sampler table构成40byte根，Private placement实例/贡献/Shared placement Header、132组间接query；Native/capture/API/CLI PASS，输出2375/2375/0/0、528uint、48事件/EID0，所有opaque CBV bytes/padding、sampler table scalar/padding、read-only usage与真实16 texture bytes核验。
- `development/mixed6-private-placement-2D`：五个Private/Tracked placement CBV offset64+六sampler table构成48byte根，Private placement64个几何和64实例、末端参数offset52、unretained，GPU参数1→[2,66,1]→[0,66,1]；Native/capture/API/CLI PASS，输出4498/4498/0/0、528uint、56事件/EID0。原生和捕获metallib byte一致，reflection精确4/5 CBV+Table，AIR实际六次texture sample和真实ray query，六个sampler实际ID均变化，336次table身份变化观察。
- `development/mixed6-closure-bad`：27坏组54 API/CLI拒绝，返回4/1且没有GPUwait marker；root缺项/重复/帧内/类型/范围/PSO、sampler count/range/source/type、heap namespace/kind/alias/outside、texture来源与inline sampler-output别名均覆盖。一个完整合法副本API/CLI通过。入口 `util/test/metal/metal_ir_compute_roots_invalid.py`；这是本批开发反例检查，不是最终整套反例覆盖。
- 当前e8f的旧行为定向验证：`development/legacy-e8f`（B543同encoder indirect、旧direct64 query、B512 TraceRay）与 `development/legacy-CBV-e8f`（B544原pure5 CBV/pure6二维）；五个旧capture API/CLI十检查PASS，证明legacy root格式仍可回放。没有重复做官方/整套/lifecycle验收。

更早 `mixed5`（64ad3d8c）原生/捕获成功，但缺静态slot来源，初态guard拒绝；修正sample按已有provider `StaticTable` 全部声明各sampler。`mixed5-slots` API -11/trace oracle6源于验收helper关闭file后仍访问SD表，关闭前缓存sampler offset后修复，失败/stack取证日志保留；LLDB在dyld启动未完成、40s终结124且旧UE formatter路径无效，sample记录保留，不计GPU/测试通过。`mixed5-closed`（9f2bb2ec）Shared混合根完整gate通过，但不是当前e8f的最终证据。构建变量遮蔽失败日志 `mixed-first-failed-build.log` 保留。此前用户UE/Qt崩溃和系统重启的完整修复尚未证明，不能以验收工具修复替代。

## 尚未完成

同一个B544继续帧内/Private CBV生产依赖及buffer/texture输出的通用图形API闭包，并接实际PSO类型元数据。需要的能力合并后，固定最终库一次集中损坏capture/相关官方sample/回归/生命周期验收，再跑短UE真实光追capture/replay，核验输出、事件前后/EID0、资源绑定与预算。本检查点没有新UE，没有当前库官方sample、full78IR/75RT/308/frame-family/Qt-ARC/官方坏13或controller lifecycle验收，不继承B543旧hash结果。生产supportsRaytracing及FromRender仍false；本批仍开发中，没有开启能力或重复启动已知闭包失败的UE整帧。

外盘迁移219目录/17.3GiB及58个可重建cache清理已经完成；所有本检查点capture/metallib/log/bad-case产物继续直接写外盘。内盘可用约32GiB。删除了已完成的patch生成临时脚本与本次生成的两个py_compile cache，保留源码、前一开发库备份及失败证据。持续任务规则已更新为大批通用API集成。没有提交/推送/重置，用户RDHeaderView改动保留。

## 帧内 CBV、较大 backing 与 Private UAV 开发检查点（同一 B544，未最终验收）

继续参照 DX12 root CBV 对应的已知资源+offset（`d3d12_command_list_wrap.cpp:1979` / `d3d12_replay.cpp:1195`）、VK descriptor/dynamic offset 与既有提交时输入冻结方式。资源存储方式使用普通 Metal buffer 初态、GPU VA重定位、实际提交及读回路径，不以效果或shader名称分支；没有新增本阶段chunk（Max仍1442）。

帧内CBV预检按实际原生提交顺序证明读范围：捕获的CPU snapshot属于其commit，已知Shared源blit才能发布Private/Tracked placement数据；拆分copy可合并覆盖，缺失/部分/未知源/迟提交不能推测完成。EID0撤销帧内对象，重新定位时重建；CBV仍为opaque值，不扫描内部指针。初态backing限制移到既有128MiB资源限额，单根声明read仍≤64KiB；帧内Shared支持既有8MiB backing与WriteCombined选项，总known-byte证明保持64MiB，Private standalone frame birth仍≤64KiB。大背景buffer的完整创建字节也可作为初态，加入初态预算且原资源/快照上限不变；独立InitialContents仍优先，缺失/截短数据拒绝。

输出允许完整初态的Private standalone或Tracked placement buffer（当前仍≤16KiB、typed UAV slot/已知VA/范围/角色/别名核验）。Capture annotation、序列化replay声明与dispatch consumer保持一致；输出经真实GPU Shared读回核验，API重放使用普通Private初态恢复。texture UAV和多输出尚未实现。

| 开发证据目录（均在外盘B544 development） | 实际库 | 结果与范围 |
| --- | --- | --- |
| `frame-private-CBV-closed`、`frame-placement-CBV-2D`、`frame-shared-CBV-published` | `86f20eec` | 三fresh Native/capture/API/CLI PASS，共152事件/EID0；Private四根/Tracked placement五根/Shared CPU发布，528uint各，最后项零工作量 |
| `frame-CBV-closure-bad` / `legacy-frame-86f` | `86f20eec` | 34坏组68无GPUwait拒绝+原样/两段copy两个合法对照；七旧capture十四API/CLI PASS |
| `large-initial-CBV-2MiB-creation/creation-fix-manifest.json` | `479d84f0` | 复用失败capture修复后API/CLI PASS，四2MiB CBV offset64、48事件/EID0；保留原FAIL manifest |
| `large-initial-CBV-offset58624` | `479d84f0` | fresh Native/capture/API/CLI PASS，五2MiB WriteCombined CBV offset58624、独立sampler offset16、二维[2,66,1]、528uint输出4248/4248/0/0、48事件/EID0 |
| `large-CBV-closure-bad` / `large-CBV-creation-bad-fixed` | `479d84f0` | 前者27坏组54无GPUwait拒绝后fixture XML导入失败，整体FAIL；修正blob编号后仅新增四坏组八无GPUwait拒绝+一个合法完整对照PASS，不将失败批次改为PASS |
| `large-frame-CBV-offset58624` / `large-frame-CBV-snapshot-bad` | `c9aa176d` | fresh Native/capture/API/CLI PASS，五帧内2MiB CBV offset58624、528uint输出4498/4498/0/0、56事件/EID0；缺失/截短/越界/Private伪CPU snapshot四坏组八无GPUwait拒绝+一个合法对照PASS |
| `private-output-frame-CBV-2D-closed` | `466a6485` 当前 | fresh Native/capture/API/CLI PASS：Private standalone输出+五Private placement frame CBV，64几何/实例、二维间接/offset52/unretained、528uint输出4498/4498/0/0、56事件/EID0 |
| `private-placement-output-large-frame-CBV` | `466a6485` 当前 | fresh Native/capture/API/CLI PASS：Tracked placement输出+五帧内2MiB Shared WriteCombined CBV offset58624，64几何/实例、二维/零工作量，528uint输出4498/4498/0/0、56事件/EID0 |
| `private-output-closure-bad` / `legacy-submission-output-current` | `466a6485` 当前 | 四输出坏组八无GPUwait拒绝（初态缺失/截短、输出与heap别名、身份缺失）+一个合法对照；七旧capture十四API/CLI PASS |

所有成功样例核验实际ray query/六texture sample AIR、GPU间接参数、sampler身份变化、声明资源只读usage、完整已发布字节和padding；Private输出经原生GPU拷贝后验证，API读回和事件/EID0分别验证。开发helper对帧内Shared只在首次query消费后检查数据，避免把尚未提交的CPU snapshot误当当前权威状态。合法拆分copy使用源/目标offset逐段重建oracle。没有将不同库结果拼接为最终验收。

本轮失败证据保留：`frame-private-CBV`早期frame Private创建guard拒绝；`frame-shared-CBV`早期API6为oracle提前检查未发布数据；`large-initial-CBV-2MiB`/`...-creation`先后预算/inline setBytes拒绝，完整创建payload预算与inline重定位先后依赖修复后通过；`large-CBV-closure-bad`新增空blob用了稀疏非法编号999999，导入工具-11，改用既有合法编号/一致blob大小后四新增反例通过，未宣称导入器自身修复。第一次Private输出`private-output-frame-CBV-2D` native通过，但capture注解仍限定Shared，应用早退出继而resource-manager teardown assertion/-11；统一capture/replay/consumer storage核验后新目录完整通过。构建均串行终结0，没有将失败或未跑范围计为PASS，也不据此认定此前UE/Qt/重启根因修复。

当前backend及bundle SHA `466a6485d46eab93478646347d29f443fb705931b555e545c14db3e60c3b132b`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。外盘源码/库校验备份 `checkpoint-submission-output-466a`、清单 `development/checkpoint-submission-output-hashes.json`；这是开发检查点，不是B544最终冻结验收。所有已启动build/GPU/helper均终结，未发现UE/qrenderdoc重叠。

B542实际UE root4318的2MiB options1背景buffer有完整InitialContents（creation字段为空）和五次部分CPU snapshot，本阶段大backing及WriteCombined样例覆盖相应基础API形状，但实际UE自动PSO布局/texture输出/生产消费闭包和整帧预算仍未验。下一仍在同一B544合并这些相关底层缺口，然后固定最终库一次集中官方sample、损坏capture、相关回归/生命周期，再短UE实际HWRT输出/事件/EID0/绑定与预算；两生产flags仍false。当前库未跑官方/full78IR/75RT/308/frame-family/controller lifecycle/UE/Qt-ARC，不能继承旧hash结果。

219目录17.3GiB外盘迁移与58 cache清理已完成；本轮新产物、源库备份与失败证据继续直接写外盘，删除三项过时patch生成脚本，记录 `development/checkpoint-cleanup.json`。内盘约32GiB可用。未提交/推送/重置，用户RDHeaderView改动保留，持续goal active。


## 2026-10-07 纹理 SRV/UAV 与实际 PSO 绑定元数据继续集成

仍属于 B544 开发过程，未冻结最终库或关闭批次；没有为单个 storage/字段新增 BATCH。对照 DX12 `d3d12_replay.cpp` UAV buffer/image 分类及 `d3d12_device_wrap.cpp:1182` RootSignature/`:853` compute PSO捕获、Vulkan `wrappers/vk_shader_funcs.cpp:377` shader字节/`:1072` compute PSO及既有texture初态/读回。生产支持边界及所有提交/预算/来源检查保持。

通用实现：typed heap SRV读取支持初态 Shared、Private standalone/Tracked placement 2D；创建 RW usage 不等于本次shader写角色，仍按实际声明与提交写依赖核验。UAV可为 buffer或≤64KiB普通color 2D texture，slot type5/source1、完整初态与明确write kind4；API GetTextureData/Private GPU upload/readback复用既有实现。kind1仍为texture read，sampler namespace和AS slot不复用为UAV。输出原始texture ID重定位、CS_RO/CS_RW、事件前后/回退/EID0纳入oracle。动态/多个输出、帧内新output、GPU生产后读取仍待完整消费契约，不能从这些样例推断支持。

允许原生256-byte小placement heap，继续64KiB-rounded总预算、native child size/alignment/容量/重叠检查。损坏反例发现重放debug补加ShaderRead被当成原始创建权限：metadata scan现在保存device/heap/buffer texture原始usage，view继承来源，query消费核验原始权限。另一个缩小heap请求的反例实际仍被原生取整容量合法容纳，修正为明确越界offset，未人为缩小原生heap容量。两失败manifest保留，不能将旧失败计PASS。

### 分库开发结果（不合并成当前库整套通过）

| 开发库 | 已跑范围 | 实际结果/产物 |
|---|---|---|
| `4c5c5a92` | Shared R32Uint、Private RGBA32Float UAV两fresh场景；placement大CBV修正helper | 两完整Native/capture/API/CLI PASS，48/56事件、528texels/2112float channels；placement原gate API6为birth识别错误，修正helper API/CLI及AIR证据通过，原FAIL仍保留 |
| `4c5c5a92` | `texture-UAV-closure-bad` | 12坏组24无GPUwait API/CLI拒绝+合法控制PASS |
| `1a47a379` | `texture-SRV-private-RW-UAV-float` | Private RW创建/SRV只读、Private Float UAV与48-byte frame roots完整PASS；256-byte heap旧最小值guard拒绝另存FAIL |
| `24483c28` | `texture-SRV-placement-small-heap-closed`、`legacy-texture-SRV-UAV-current` | fresh Private placement SRV/UAV+2MiB/offset58624帧内CBV Native/capture/API/CLI PASS，56事件；10旧capture20 API/CLI PASS。原始usage损坏capture被接受的失败不计PASS |
| `97f927d0` | `texture-SRV-UAV-original-usage-fixed-fixture`、`legacy-texture-SRV-UAV-original-usage` | 原始usage修复后的18坏组36无GPUwait拒绝+合法控制、10旧capture20 API/CLI PASS；初次heap-truncated fixture合法取整的失败保留 |
| 当前 `96cf2591` | `IR-reflection-private-placement-textures-width0` | fresh Native/capture/API/CLI PASS，64几何/实例、5帧内2MiB WriteCombined CBV offset58624、6 static samplers、Tracked placement R32Uint UAV/Private RW创建的SRV、[1,1,1]→[2,66,1]→[0,66,1]、528texels/56事件/EID0；PSO compiler JSON与原converter payload及AIR完全对应 |
| 当前 `96cf2591` | `IR-reflection-texture-closure-bad` | 16坏组32无GPUwait API/CLI拒绝+合法控制PASS，包含6不可变PSO metadata损坏与10纹理role/source/initial/usage/native heap范围反例 |
| 当前 `96cf2591` | `legacy-texture-SRV-UAV-reflection-96cf` | 10旧capture20 API/CLI PASS：旧direct/indirect heap、IR TraceRay、纯CBV/mixed Private初态、Private buffer output及Shared uint/Private float texture UAV |

以上产物均在外盘 `build-macos-debug/metal-ray-b544/development/` 各同名目录。正式完整官方/full78IR/75RT/308/frame-family/controller生命周期/Qt-ARC/整帧UE GPU验收在当前库**未跑**；不能继承B543或任何其他hash结果。

### 不可变 PSO 元数据与实际 UE

新增末尾 `CaptureIRComputeReflection`（Max1443，之前chunk ID不变）：以已知native compute PSO记录immutable字符串，单项≤64KiB/4096项/总16MiB，同值重复annotation幂等，capture同对象异值拒绝，重放duplicate/frame/零ID/错误对象/空/超长拒绝；它记录事实，**不授予GPU消费资格或coverage**。第一个构建Max断言遗漏、首次字符串注解用了vector width1导致API拒绝/应用早退出后的resource-manager断言均保留，已修正Max和字符串width0；本轮复验通过不能等同历史用户崩溃修复。

isolated UE shader GetPipeline在实际Kernel创建后发出metadata，未修改安装引擎或用户工程。首provider `d0e556ec`真实capture `336eed4a`/74,801,658bytes验证后发现compute JSON为空：安装编译器 `MetalShaderCompiler.cpp:536–546`只保存vertex反射，不能误报拿到完整compute compiler contract。后续provider `2496f103`保存编译header中真实RSNumCBVs、原生绑定点、6 sampler ABI、NumThreads；JSON的`Origin=MetalIRRuntimeBindings`明确区分来源且不填写UsesRayQuery/UsedResources来伪造shader语义。compiler完整JSON存在时原样保留。

新工具 `util/ue/audit_metal_ir_compute_reflection.py`按ResourceId连接metadata→实际PSO→function/library字节哈希→AIR，覆盖全compute范围；不以shader名、Lumen pass/debug group筛选。编译器UsesRayQuery声明与实际allocate/reset/next调用核对；runtime ABI不作编译器语义声明，独立识别AIR调用，逐dispatch核对metadata线程组与实际线程组、root长度与实际setBytes。sample 1 PSO/两非零query通过；首CPU audit漏URL library blob（UNVALIDATED）已留证，修正同样读取捕获URL/Data的data字段后通过；现有inventory修正computeFunction字段且八旧单测、三个AIR单测通过。

新 UE capture：`development/UE-IR-binding-ABI-capture/original.rdc` SHA `403f90ea8c23638b8c817701725e31885c7929c09fac169a99e7728a66ac092b`，75,234,193bytes。119 PSO全metadata/实际AIR通过，0 UNVALIDATED；6实际ray-query PSO的root长度40/48/56/64/72bytes，threadgroup均[32,1,1]，五次非零组[25,24,1]/[229,1,1]/[2,66,1]/[132,1,1]/[80,1,1]，另一query为零工作量。原Lumen范围60 AIR/两非零query也通过，不能再把那两项当作应用全部RT范围。抓到3间接TLAS build、96 direct+56 indirect compute，owned editor在capture保存后由supervisor关闭（不计正常Editor退出验收）。无新增strict assertion/ForceCrash/metadata拒绝日志。

同一新capture只跑一次CPU65诊断：API4因动态AS Header/UE heap-query producer-consumer闭包未支持而拒绝，未接受metadata总体/预算，无GPUwait/初态upload/frameGPU。**没有UE输出、事件选择、EID0或整帧绑定重放通过证据**；避免对同一未变阻塞再跑normal/pre-submit/GUI。后续必须用这些真实PSO/执行点事实补通用动态slot、多UAV与GPU生产依赖，不能按六个shader名字做适配、不能把runtime ABI解释为CBV的完整shader读范围。

当前backend/bundle `96cf259184b3450ee8b2d422135e88c7fdf82f2cb2ca1a9f86415e272c2be0aa`、GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`、provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`（98原exports全保留）。源码/两产品备份 `checkpoint-texture-reflection-96cf/manifest.json`；原reflection-only provider也另名保留。build/GPU串行，已启动进程均已终结；未提交、推送或重置，两生产能力仍false，持续goal active。


本轮追加迁移34个已关闭frozen-validation目录（1,021文件/1,265,807,006bytes，约1.18GiB），逐文件SHA及内部symlink完全核对后保留原路径链接；清单 `development/additional-frozen-storage-migration.json`。此前219目录17.3GiB/58可重建缓存清理不变；当前新产物全部外盘，主构建/工作dylib保留，内盘约32GiB可用。没有删capture或失败证据。


## 执行点多 UAV 与已知 GPU 生产消费集成（继续同一批次）

参照DX12 `d3d12_replay.cpp:1410` 的当前root table heap/offset及SRV/UAV类别、`d3d12_command_list_wrap.cpp:3668` 的执行点间接参数，以及VK `wrappers/vk_draw_funcs.cpp:1313` 的DispatchIndirect参数和usage、既有descriptor状态/提交依赖。Metal的不可变PSO声明描述角色，当前资源身份按每次dispatch的live slot、known source及native ID解析；没有新增效果、shader名称或Lumen分支。

compute heap kind8代表当前slot的texture UAV，和旧kind4的固定输出路径分开，旧chunk编号/Max1443保留。主输出和额外输出均检查source/type/完整原始身份packet、原创建ShaderWrite权限、Shared/Private/Tracked placement、ordinary color二维单mip、已知完整初态、非alias、尺寸/工作量与既有总预算。当前槽位输出单纹理上限128MiB、工作量262144，旧固定输出64KiB/4096边界保留；本次实际测试使用4×132小工作集，不宣称已验证新上限全范围或UE大帧。输出不能重复同一物理资源，也不能与读取、table、AS构建来源重叠。

所有当前UAV分别记录CS_RW usage，间接argument和只读依赖与全部输出检查角色冲突。query PSO的显式Write residency推迟到dispatch类型验证，只能属于已证明的输出集合；不把一个遗漏角色的GPU写悄悄当成受支持producer。GPU buffer输出仍记录modified/opaque，不能把旧buffer初态当作已产生的CBV或heap数据；只有非零、已验证query纹理writer可成为后续SRV的已知producer，并证明同命令次序或先前commit。未知/opaque writer仍拒绝，零调度不发布writer证明。纹理完整初态仍必需，帧内新建UAV尚未开放。

### 当前库的复合开发证据

backend及bundle SHA `105986c11ebee880822e3bbd3c4c79cbf441016b2383c0fce93b0226960ad47a`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`；provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`未修改。本库构建终结0。19份源码/四产品校验备份 `checkpoint-multi-UAV-105986/manifest.json`，统一开发证据 `development/multi-UAV-current-integration-manifest.json`，均位于B544外盘链接下，**不是最终冻结库或B544最终验收**。

| 外盘 development 证据 | 实际通过范围 |
| --- | --- |
| `multi-UAV-current-private` | Native/capture/API/CLI PASS；64几何/64实例、五帧内2MiB WriteCombined CBV offset58624、六静态sampler、48-byte根，所有五纹理Private/Tracked placement、unretained命令、GPU参数[1,1,1]→[2,66,1]→[0,66,1] offset52 |
| `multi-UAV-current-shared` | Native/capture/API/CLI PASS；四帧内Private CBV+sampler构成40-byte根，Shared五输出/输入纹理，三实例、相同二维/零调度，argument offset16 |
| 当前两复合目录的 `API.log` 及坏组合法副本 | 完整扩展helper API PASS；每capture48次事件前后往返/EID0、144当前descriptor身份检查、36公共GetDescriptorAccess/GetDescriptors核验，全部CBV bytes/padding、六sampler身份、CBV只读usage，五纹理每项528texels逐位核验 |
| 两复合目录各自 `compiler-AIR-proof` | 实际metallib compiler metadata与捕获完全一致，真实AIR query调用/六sample/两texture-write，1PSO/0UNVALIDATED/两非零query，原生threadgroup/根payload逐调度一致 |
| `multi-UAV-current-bad` | 12坏组24 API/CLI GPU前拒绝，无GPUwait；合法副本API/CLI通过。覆盖漏/错第二UAV角色、初态/ID缺失、创建readonly、重绑source/generation/alias、producer迟提交，以及旧读写角色/重复声明关键反例 |
| `legacy-multi-UAV-current` | 10旧capture20 API/CLI定向检查：旧direct/indirect query、TraceRay、pure CBV、混合CBV/sampler、Private buffer输出、Shared uint和Private float纹理输出均PASS |

同一PSO每轮主slot2按A→B→C换绑，次slot4按D→E→D，slot3按原始纹理→D→E。第一轮只写A/D各四texels，其余保持初态；第二轮读取D的真实GPU红色64（初态12）作为六次采样的输入，命中值增加312，写满B/E各528texels；第三轮零工作量，C及已有纹理保持不变。Private链结果4248→4560，Shared链2375→2687，命中/命中/未命中/短射线逐项核验。每轮单独commit/wait后再CPU改heap，防止多个编码器提交前CPU修改导致实际GPU都看到最终slot。原生/捕获输出一致，API多次事件回退保留各资源独立初态并核验EID0。

公共descriptor检查复用既有通用AIR uniform访问解析器：在三个调度事件上返回两ReadWriteImage和一Image，resource随当前slot变化，包含零调度的绑定语义；没有按测试label或Lumen名称构造descriptor。AS Header仍只核验外部AS ID/贡献VA与普通资源关系，不查看AS内部。

### 失败保留与实际未跑范围

`multi-UAV-first-build-FAIL.log` 为初次新代码局部变量shadow触发Werror，改名后构建通过。`dynamic-multi-UAV-private-chain` 原生/捕获成功，但API helper没有预分配AS/VA扰动对象，实际身份可能与捕获相等；补与旧helper一致的AS/VA/sampler扰动并在独立新目录通过，不改原FAIL manifest。其`helper-diagnostic`还包含一次临时诊断误传旧场景hit4498（本场景应4248）的oracle失败，保留原始输出，不计产品失败或PASS。7021早期开发库的36公共descriptor和完整CBV oracle扩展分别保留单独manifest，不覆盖前序gate；随后105986补保守buffer writer边界，并用 `development/multi-UAV-consolidated.py` 在同一库集中完成两fresh gate、12坏组和10旧capture复验。当前158日志/当前库PASS来自新目录，不拼接7021证据。

158个当前库完成日志检查无Assertion/ForceCrash/overrun/资源map残留等严格诊断；这不证明用户历史Qt/UE崩溃或系统重启原因已修复。所有build/GPU按共享锁串行，有限输入/帧数/超时；最新检查无UE/qrenderdoc重叠。产物直接外盘，仅清理本次两工具产生的pycache，保留capture、失败证据和工作build，内盘约32GiB可用。此前219目录17.3GiB及追加34目录1.18GiB迁移仍有效。

本轮没有新UE capture，也没有对未变化的旧CPU65失败重复执行：实际UE `403f90ea` 的119 PSO、六query PSO/五非零调度、40–72-byte运行时根事实仍保留自己的96cf开发证据。当前105986并未达到真实UE动态AS Header/自动PSO-heap消费闭包、全帧预算、GPU输出/前后定位/EID0/生命周期验收；当前显式typed query声明仍是支持边界，不能凭runtime ABI事实全局许可未知consumer。没有当前库官方sample/full78IR/75RT/308/frame-family/controller lifecycle/Qt-ARC验收，不继承旧库PASS。

下一保持同一B544，补**实际PSO/根ABI→当前heap索引及动态AS Header的通用消费契约**，利用已存在的AIR uniform地址/资源来源解析，保留未知依赖拒绝；合并更广GPU生产/帧内资源/生命周期后冻结库一次集中受影响回归，再回到短UE实际光追截帧与完整重放。两生产光追flags继续false；完整目标active，不提交/推送/重置。

当前新Private capture SHA `fb2888aba134171678991583a46a0e5815c90dbfa01cf6052b236c9f49113400`，Shared capture SHA `3d6d42657fca6b514d044219ca7d5db0a5ba678c045beb2326b605ad385462ad`；完整源码/SDK/converter/DXIL/metallib/helper/backend哈希分别保存于当前gate manifest。7021开发备份和早期失败目录继续保留，最终当前库/GUI/provider与汇总manifest在105986检查点逐项核验。


## 通用 AS descriptor 与实际 PSO 运行时 ABI（继续同一集成批次）

参考本仓库DX12 `d3d12_replay.cpp:861`把AS SRV的GPU地址关联到AS ResourceId、`:1410`当前root table heap/offset和类别，以及VK `wrappers/vk_draw_funcs.cpp:1313`对实际间接dispatch参数与usage的处理。实现按Metal PSO、AIR类型、已知descriptor来源、原生外部身份和当前绑定状态组织，没有UE、Lumen、shader名或pass分支。

`MetalAIR::UniformResourceAccess`在既有uniform texture解析上增加typed AS访问报告；支持实际converter的匿名两指针Header struct GEP，保存descriptor来源与Header来源的区别。调用报告记录有效module、AS reset/纹理调用以及unresolved数，无法解析的调用不能被当成没有消费。原texture inspection wrapper保留。公共AS解析必须来自当前kind3 Header source，核对当前Header配方/known AS对象，正常检查还读取Header外部handle并和原生AS gpuResourceID匹配。只读公共Header的普通字节，不读取AS内部。GetDescriptors返回AS ResourceId，GetDescriptorAccess按当前heap/24-byte slot标示AccelerationStructure。

新增 `metal_ir_compute_abi.h/.mm`，用Foundation JSON解析compiler和runtime来源区别。runtime ABI只接受明确Compute、不同原生绑定点0–30、连续8-byte CBV根/末尾Table、合法CBV Slot/Space、sampler count及三维原生线程组；bool、浮点、错类型、负数、越界、缺字段、错误根角色/重叠、线程组总数>1024均不能成为事实。runtime payload携带UsesRayQuery或UsedResources被拒绝，compiler原payload仍是compiler来源事实。capture注解拒绝非法JSON/runtime ABI；CPU预检保存typed ABI，与实际dispatch线程组、实际inline根长度核对。setBuffer替代setBytes清除inlineData与bytes，不能消费先前root残留。chunk Max1443和编号未改变。以上事实仍不自动生成资源读范围或放行未知consumer。

### 分库定向开发结果

| 库/外盘证据 | 实际运行与结果 |
| --- | --- |
| 806a34c3 / `development/AS-uniform-consumer/inspection-manifest.json` | 两105986旧复合capture各扩展API PASS：48事件/EID0、144当前纹理身份检查、48公共descriptor检查（36 texture+12 AS）。独立inspection开发结果，不能当当前库Native/UE通过 |
| 71c9ac71 / `development/AS-ABI-consolidated/manifest.json` | 两fresh Native/capture/API/CLI PASS（Private placement/五帧内2MiB CBV，Shared五纹理/四帧内Private CBV）；每项全部五纹理528texels，48事件/EID0、48公共descriptor（含12 AS）、144身份、全部CBV/padding及六sampler。21坏组42 API/CLI无GPUwait拒绝、legal-runtime-ABI和legal-original两控制；10旧capture20 API/CLI定向PASS。其完整hash `71c9ac7117bc951674622c70950f484693e0427175160f77efa91212246e59a9` |
| 当前 8caaecc0 / `development/AS-ABI-current-final-targeted/complete.json` | 最后补root绑定替换状态后，两上述capture各API/CLI通过；10 ABI坏组20 API/CLI无GPUwait拒绝（含root重新绑buffer）、合法runtime ABI API/CLI通过。当前10旧capture20 API/CLI通过，记录于 `development/AS-ABI-current-final-legacy/development-regression-manifest.json` |
| CPU / `development/AS-uniform-consumer/CPU-manifest.json`、`ABI-CPU-manifest.json` | 既有texture/原生raw-root/读写atomic和新AS literal Header、错字段、缺source/未知AS call等测试PASS；strict runtime解析含119真实UE payload和一个原compiler sample payload PASS，GPU commands0 |
| CPU / `development/AS-uniform-consumer/actual-AIR-shape-manifest.json` | 六真实UE query PSO的实际AIR/entry形状PASS，分别AS reset 1/2/1/1/1/2，texture calls39/14/21/2/12/5；合成loader无unresolved。这仅是parser-shape证据，**不是实际capture依赖或GPU输出验收** |

当前backend及bundle SHA `8caaecc0b07643c4a117ea49fc061d73b3f14c987ca8a9347870ce616b548112`；GUI SHA `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`；isolated provider SHA `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`未修改。当前27源码/四产品逐SHA校验备份 `checkpoint-AS-ABI-8caaecc0/manifest.json`，汇总 `development/AS-ABI-current-integration-manifest.json`。当前110完成日志无Assertion/ForceCrash/overrun/resource-map残留等严格诊断；最后后端/bundle build0，GUI的SDK版本警告仍存在。不将没有出现严格诊断当作Qt/UE历史崩溃或系统重启修复证明。

当前两个API输出仍是Private链4248→4560、Shared链2375→2687；零调度不写C及当前UAV，后续SRV读取已提交的真实纹理GPU输出。Native输出及fresh capture是在71c9库验过，当前8caaecc0只做最后状态修正后的必要定向重放，没有再刷两fresh Native/capture；保留各hash的证据范围，不拼接为当前完整验收。所有build/GPU沿共享锁串行，有限输入、帧数与超时；执行进程均已终结，未与UE/qrenderdoc重叠。

### 真实UE仍需完成的通用消费验证

继续使用 `development/UE-IR-binding-ABI-capture/original.rdc`（403f90ea、75,234,193bytes）的实际119 PSO/六query PSO/五非零调度事实。本轮CPU从原XML核验三个初态和三个frame-born Header，均64-byte Shared placement、同三AS及贡献来源，frame-born没有初态。未对相同动态Header失败重跑CPU65、normal/pre-submit或GPU整帧，没有新UE capture。严格runtime parser接受119真实绑定payload，并不使这些Header或shader自动获准。

下一在同一B544，把**实际PSO/运行时ABI→已发布的当前uniform字节→heap索引/typed source→AS Header及读写依赖→提交顺序**连接起来。既有computeCBVKnownBytes当前只存known mask，自动解析真实selector需要保留实际已证明字节并按commit发布；Private CBV只能由已知初态或Shared→Private typed copy获得CPU值，不能从GPU写过的旧初态取值。AIR报告的AS/texture已解析子集也不覆盖所有buffer读写、sampler或动态数据相关索引；自动consumer必须证明其所需完整依赖，不能因6份AIR的合成形状成功或runtime metadata非空就放宽Header guard。更广GPU producer、帧内输出资源及生命周期继续合并后固定库集中相关回归，再回到短UE真实光追截帧/完整重放。

当前库没有官方/full78IR/75RT/308/frame-family/controller生命周期/Qt-ARC/UE整帧GPU验收，UE输出、事件前后定位/EID0/真实绑定及总体预算仍未通过；不继承B543、105986或71c9库的PASS。B544和PHASE59未关闭，两生产能力false，目标active，无提交/推送/重置。

所有新产物和源码/产品备份直接外盘B544链接，内盘约32GiB、外盘约1.6TiB可用。此前219目录17.3GiB和34frozen目录1.18GiB校验迁移与58可重建cache清理仍有效；保留capture、失败、系统崩溃证据及用户改动，没有重复复制到内盘。

## 执行点已发布 uniform 值与实际 AIR 消费（继续 B544）

将旧 known mask 扩展为有界 data+known，CPU commit 快照与 typed Shared→Private copy 按提交顺序发布；每次 CBV 消费保存独立 fileOffset/range 字节。当前 heap slot/source、inline typed pointer 和 AS Header 在 dispatch 时冻结，后续 CPU 换绑不能改写先前消费证明。保留整体64MiB证明预算、单copy/累计copy与帧内资源/heap预算；standalone Private frame CBV 创建沿既有8MiB上限，不再被独立旧64KiB条件挡住。指针仍须 typed 来源，未知 GPU 写入不会被旧初态替代。

显式已知 query/CBV 契约现在使用 actual AIR 和实际已发布的 scalar，核验 AS slot、纹理 SRV/UAV角色及 unsigned/signed/float 数值类型；ResolveSubmissionBindlessUsage 用事件 fileOffset 查同一消费值，Private CBV 不需要额外 GPU 读回。参照 DX12 当前 root table/AS descriptor 与 VK 实际 dispatch usage/提交顺序，没有 UE/Lumen/shader/pass 特判。此处 AS/texture 消费报告仍不涵盖全部 buffer/sampler/数据相关索引，不能据此自动授权任意 runtime PSO 或真实 UE 整帧。

| 当前45c2a584证据（均在外盘 development/） | 实际结果 |
| --- | --- |
| `uniform-consumer-large-private/manifest.json` | fresh Native/capture/API/CLI；64几何/实例、五帧内2MiB Private CBV offset58624、Private/Tracked placement五纹理、六sampler、二维132组/间接offset52/unretained；结果4248→4560 |
| `uniform-consumer-small-placement/manifest.json` | fresh Native/capture/API/CLI；四80-byte Private/Tracked placement CBV offset16、40-byte roots、Shared五纹理；结果2375→2687 |
| 两上述API | 每项48事件往返/EID0、144身份、48公共descriptor（36 texture+12 AS）、全部五纹理528texels/完整CBV padding/六sampler身份和只读usage；零调度保持未写C和前序结果 |
| `uniform-consumer-current-bad-publication/manifest.json` | 35坏组70 API/CLI拒绝且无GPUwait；包含实际AS越界/无source、SRV/UAV错角色和unsigned shader→float UAV，3合法控制通过（完整split copy/runtime ABI/original） |
| `legacy-uniform-consumer-publication/development-regression-manifest.json` | 10旧capture20 API/CLI定向PASS，包含旧direct/indirect、TraceRay、纯/混合根、buffer与纹理输出 |
| `uniform-consumer-proof-persistent/manifest.json` | CPU AIR numeric/parser通过；两真实compiler/AIR审计均无unvalidated、各两非零query；实际捕获 scalar读取54/48次，三fileOffset均valid1/AS1/unknownAS0/texture8/unknownTexture0；非合成loader证据 |

Private capture SHA `9fbd6befd4125e46351709ce7ac0506e6c7f653854d432c1a144c7e129fbacc3`。完整第二capture及输入/SDK/converter/metallib/helper哈希在各 gate manifest。当前 backend/bundle SHA `45c2a58458c145dfc41afbcc27ef1b1b21d1b0dbf024031bf86443e21aaa6175`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。28源码/四产品逐SHA备份 `checkpoint-uniform-consumer-45c2a584/manifest.json`，汇总 `development/uniform-consumer-current-integration-manifest.json`；316完成日志无严格诊断，最后backend/bundle build0（SDK版本警告仍存在）。

开发失败均保留：旧Private frame创建64KiB guard拒绝后修复；placement CBV oracle遗漏heap capacity后修复；旧legal-split-copy把1MiB上传硬截成80字节后修复；selector坏例只修改creation blob被实际CPU publication覆盖，现同时修改权威快照，六新增selector均正确拒绝；首次proof工具未锁住日志导致退出删除日志，补留存后取得完整值追踪。首次局部变量shadow构建失败另存FAIL.log。前期d82f库的单项Private通过不计当前最终验收，所有FAIL不计PASS。

当前库未跑官方/full IR/RT/frame-family/controller生命周期/Qt-ARC/UE整帧GPU；不重复相同失败UE。实际UE403f90ea的119 runtime PSO/六query PSO事实保留，自动consumer仍需证明完整buffer/texture/sampler依赖、GPU producer与动态索引/当前Header提交闭包。下一继续在同一B544完成通用API路径，固定最终库集中回归后短UE实际光追截帧/输出/事件/EID0/预算验收。两生产flagsfalse，AS保持黑盒，历史崩溃/系统重启修复未证明，目标active。

所有新产物直接外盘B544链接；此前219目录17.3GiB+34frozen目录1.18GiB校验迁移/58可重建cache清理有效，内盘约32GiB、外盘约1.6TiB。保留用户未提交改动，无提交推送/重置。

## 实际 sampler 消费与公共查询（继续同一 B544）

参照本仓库DX12 `d3d12_replay.cpp:1912/1995`对静态/当前sampler heap身份与参数的查询，以及VK `vk_replay.cpp:2832/2990`按当前descriptor store返回sampler描述和访问。Metal沿typed来源→根指针→当前24-byte sampler slot实现，不按Lumen、UE、shader或pass分支。

AIR uniform报告新增Sampler类型、sample/gather的samplerCalls与unresolvedSamplers；支持typed sampler load和有typed来源的raw i64→sampler转换，未知/越界/缺operand不能当作没有消费。dispatch保存实际kind3 sampler根范围内当前slot/source；消费时须命中相同表与offset（支持root memberOffset16，不能假设全buffer的offset%24为零）。重放inspection核验source为真实sampler对象，普通公共查询额外核对当前外部native gpuResourceID，不读取AS内部。公共GetDescriptorAccess与GetDescriptors报告Sampler身份，GetSamplerDescriptors复用普通sampler参数转换，当前表不会误套直接绑定的LOD override；EID0不能留下前次调度sampler。

当前 backend/bundle SHA `aca9809595ff98b41cc3f9929105b8e5b6a9ddb20110c310b7e826b378cfc3be`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。28源码/四产品逐SHA备份 `checkpoint-sampler-consumer-aca98095/manifest.json`；真实编译七sampler的HLSL/converter源码、DXIL/metallib/JSON及复现脚本存于同检查点，编译输入哈希记录于 `development/sampler-consumer-current-integration-manifest.json`。

| 当前库定向开发证据（外盘development/） | 实际结果 |
| --- | --- |
| `sampler-consumer-final-targeted/manifest.json` | CPU sampler解析/缺source/静态越界/未知动态索引/gather及旧AIR测试PASS；两已有45c2真实capture各扩展API/CLI PASS，三实际消费点sampler6/unknown0 |
| 两当前扩展API | 每项72 sampler公共身份、min/mag/mip filter、三个寻址轴、LOD/normalized参数核验；48事件往返/EID0无stale sampler、144纹理ID/48 texture+AS公共查询、完整五纹理528texels/CBV padding/六sampler重定位，输出仍4248→4560与2375→2687 |
| `sampler-consumer-final-bad/manifest.json` | 41坏组82 API/CLI无GPUwait拒绝（含先前35组、5 sampler根/来源坏例、1 actual sampler consumer越界）；3合法控制API/CLI通过 |
| `sampler-consumer-outside-API/CLI-renderdoc.log`（在上述bad目录） | 真正编译的七sampler库替换capture的library数据，六项根表保持原事实；实际AIR报告valid1/AS1/unknownAS0/texture9/unknownTexture0/sampler7/unknownSampler1，API4/CLI1且无GPUwait。坏payload没有执行native帧/调度 |
| `legacy-sampler-consumer-final/development-regression-manifest.json` | 10旧capture20 API/CLI PASS，含纯CBV多dispatch、旧direct/indirect、TraceRay、混合根与buffer/纹理输出 |

当前318完成日志无Assertion/ForceCrash/overrun/resource-map残留等严格诊断，最后backend/bundle build0（GUI SDK版本警告仍在）。所有构建/GPU沿共享锁串行，执行已终结。

中间库 d2fe5642两已有capture/七sampler拒绝开发检查通过，不能冒充当前fresh native或最终验收。9720534d当前样例/41坏组通过，但旧pure5第二dispatch失败；原因是snapshot通过map[]读取未声明IRComputeRoots时插入空声明，下一dispatch被错误判成有typed mixed根。已改find只读、避免heap-entry读取副作用，未改拒绝标准；最终aca98095完整上述定向集合通过。首次验收工具在file->Shutdown之后读取SD引用，返回5；已把全部来源复制到关闭前，失败目录保留。两问题均为本轮开发发现，不作为历史UE/Qt/系统故障已修复的证据。

当前没有fresh Native/capture，也未跑官方/full IR/RT/frame-family/controller生命周期/Qt-ARC/UE整帧GPU；两个capture来自已验证45c2库，此处仅计当前必要扩展重放。sampler报告覆盖sample/gather typed消费，不能把它及现有AS/texture子集当作全部buffer/其他intrinsic/数据相关索引都已证明。实际119 UE PSO/六query事实仍保留，未重复相同动态Header拒绝的UE运行；下一完整buffer、其他实际AIR资源依赖和通用runtime PSO消费/producer顺序，固定最终库集中回归后再短UE真正光追截帧/整帧输出/定位/EID0/总预算。两生产flagsfalse，AS保持黑盒，B544和PHASE59开放。

所有新产物外盘，旧219目录17.3GiB+34 frozen1.18GiB校验迁移及58缓存清理保持；内盘约32GiB，未覆盖用户改动，无提交推送/重置。


## B544 实际 buffer 消费与 metadata intrinsic（继续同一大批次）

当前 backend/bundle SHA `ec73a2f18da53aa7d38bd31572869b02117fe854ae105b0b351581ef7975cad9`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。产品/28源码校验备份 `checkpoint-buffer-consumer-ec73a2f1/manifest.json`，汇总证据 `development/buffer-consumer-current-integration-manifest.json`；全部在外盘B544链接。产品构建0，保留首次private-member访问编译FAIL；GUI原有SDK警告仍存在。

实现参照本仓库DX12 `d3d12_replay.cpp:947` 的当前buffer UAV/byte range与 `GetDescriptorAccess`，VK `vk_replay.cpp:2468/2990` 的StorageBuffer/当前descriptor store；补齐Metal底层的实际global addrspace1/2 load/store与buffer atomic报告，未知资源/布局/地址不得成为“无消费”。thread_position_in_grid按实际groups×threads给出有界区间，CBV已发布scalar参与mul/add/and/shift/cast/GEP；溢出或未知索引保留资源来源但不允许地址证明。typed descriptor metadata与普通buffer数据分离，unknown/ranged地址不能读出CPU scalar或猜GPU身份。预提交要求CBV读取落在显式子范围，输出buffer实际descriptor源/ResourceId/当前写入角色及最大地址+访问字节落在实际allocation内；每dispatch冻结writer集合，后次writer不能解释前次消费。read-only/global atomic或任意GPU buffer producer尚未因此获准。

公共GetDescriptorAccess/GetDescriptors提供当前ReadWriteBuffer身份、offset及完整bound范围；纹理get_width/get_height等metadata调用计入资源依赖，尺寸查询结果仍unknown，不用作动态索引授权。支持范围仍以实际typed来源和既有显式query契约为界，runtime ABI不是通用PSO的完整消费许可。没有UE/Lumen、shader或pass名称分支，不读取AS内部。

| 当前库开发检查（外盘 development/） | 实际结果 |
| --- | --- |
| `buffer-consumer-corrected-checks/manifest.json` | 新CPU区间/缺thread extent/溢出/未知source与metadata查询通过；raw原生/capture已成功、修正helper后API/CLI通过；五纹理fresh native/capture/API/CLI通过。两capture均当前ec73库 |
| `buffer-consumer-large-private-closed` + `buffer-consumer-proof-corrected/*consumed.log` | 五帧内2MiB Private CBV offset58624、48-byte根/六sampler、64几何/实例、Private placement纹理输入和buffer输出、二维132组/间接offset52/unretained；4248/4248/0/0，528uint，48事件往返/EID0，12次当前buffer公共身份/offset/长度查询，全部CBV/padding/源clear及只读usage |
| `buffer-consumer-dimensions-multi-UAV-closed/manifest.json` | 同一大型输入组合与五Private placement纹理，4248→4560；全部五纹理528texels、48事件/EID0、144身份、48普通descriptor/AS、72sampler公共参数查询，zero dispatch/前序结果及六sampler重定位 |
| 两 `compiler-AIR-proof/manifest.json` 与当前汇总actual_proofs | 实际metallib disassembly各2尺寸查询、9/11实际texture调用；每三个fileOffset均valid1/AS1/unknownAS0/sampler6/unknownSampler0/bufferRead14/unknownBuffer0，raw另bufferWrite1、纹理0；scalar事实来自实际捕获/已发布CBV，含gridWidth8/count528、AS1/SRV3和raw stride4；两compiler/AIR审计0unvalidated，各2非零query，CPU事实不冒充GPU输出 |
| `buffer-consumer-current-bad/manifest.json` | 58坏组116 API4/CLI1拒绝且无GPUwait，3合法控制API/CLI通过；新增真实buffer stride8越界、UInt32max溢出、AS/SRV错角色均报告实际bufferWrite1后拒绝；覆盖root/sampler/CBVcopy/output/ABI/reflection受影响范围 |
| `legacy-buffer-consumer-current/development-regression-manifest.json` | 10旧capture20 API/CLI通过，含旧direct/indirect、TraceRay、纯/混合CBV、Private buffer、Shared Uint/Private Float纹理输出 |

456完成日志无Assertion/ForceCrash/overrun/resource-map残留等严格诊断。构建和GPU沿共享锁串行，已终结。raw gate第一次API返回5来自工具硬编码80-byte重建，而当前场景是分两次1MiB上传的2MiB CBV；工具现按真实capacity重建、受8MiB上限约束，最终12公共buffer查询和重放通过。原始gate FAIL保留，当前链由same-capture修正helper+CLI及最终实际证明日志组成，不把旧FAIL改写成PASS。此前工具调用不存在的GetBuffer导致编译FAIL也保留。证明脚本先沿用旧offset/texture-count假设失败，GPU helper实际退出0；汇总最终按两实际AIR调用数9/11和当前CBV各字段语义离线核验全部已完成日志，未无效重复GPU测试；两FAIL manifest仍在原目录。

当前库没有官方sample集中回归、full IR/RT/frame-family、controller lifecycle、Qt-ARC或UE整帧GPU验收。生产supportsRaytracing/FromRender仍false，B544和PHASE59未关闭；历史Qt/UE崩溃和系统重启修复未证明。下一优先推进实际UE119 runtime PSO对应的通用producer/consumer资源依赖与动态Header闭包，不扩展Lumen耦合条件；之后固定最终库集中相关回归→短UE真实光追截帧/输出/事件/EID0/资源绑定/总体预算验证。未跑项目不计通过，也不继承旧库验收。

外盘产物及检查点约束持续生效，内盘约32GiB/外盘1.6TiB可用。原219目录17.3GiB+34frozen1.18GiB校验迁移与58可重建cache清理保持，失败/capture/系统日志证据保留。无提交/推送/重置或覆盖用户改动。


## 通用 typed buffer SRV/UAV 集成（继续同一 B544）

本仓库DX12 `d3d12_replay.cpp:819/956` 的TypedBuffer/ReadWriteTypedBuffer和VK `vk_replay.cpp:2471/2473` 的UniformTexelBuffer/StorageTexelBuffer均支持对应能力。实际UE119 PSO AIR存在大量read/write/get_width texture_buffer调用，所以新增通用Metal buffer texture路径；Lumen仍仅是后续实际验收，不根据引擎、pass、shader名称选择权限。

沿已有buffer-backed texture的创建/parent initial/readback路径补齐RT依赖及公共查询。捕获侧用带锁view→parent查询，重放侧用实际注册关系；textureTypeTextureBuffer须Private parent、原生view的buffer关联/offset/range/未alias身份、合法格式/usage、完整parent初态及既有预算。保持其他2D纹理路径。初态恢复与读回使用parent buffer，避免把buffer texture当成可做2D texture blit的allocation。shader读取view也引用parent；写view将parent记为writer，阻止后续从旧CPU初态推断scalar；按实际parent核验读写重叠、descriptor backing、CBV与AS几何源别名。公共查询返回TypedBuffer/ReadWriteTypedBuffer、实际parent resource/view ResourceId、format/TextureType::Buffer、非零byteOffset和view的byteSize，以及正确只读/读写DescriptorLogicalLocation；不是把整个parent作为view范围。

当前 backend/bundle SHA `9fefb511e50d3510b8bdfb88d2cfdbcee0c3c7aa3f86c51e6b6de6029bf36814`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。30源码/四产品SHA备份 `checkpoint-texel-buffer-9fefb511/manifest.json`，汇总 `development/texel-buffer-current-integration-manifest.json`，均在外盘B544链接。构建0（GUI既有SDK warning仍在），生产RT flagsfalse。

| 当前库开发结果（外盘development/） | 实际证据 |
| --- | --- |
| `texel-buffer-consumer-alias-closed/manifest.json` / 两 `texel-buffer-*-alias-closed/manifest.json` | CPU旧/新consumer解析通过；Private standalone及Tracked placement parent两fresh Native/capture/API/CLI通过，五帧内2MiB Private CBV offset58624、六sampler、64几何/实例/Private GPU AS输入、二维132间接组offset52/unretained；真实Buffer<uint> SRV读取5并加入query命中、RWBuffer<uint>写入4253/4253/0/0，528uint |
| 两API的 `PASS 24 typed buffer API checks` | 各48事件前后往返/EID0、SRV/UAV parent/view身份与只读/读写usage、R32Uint格式、offset256、16-byte SRV和2112-byte UAV range、DescriptorLocations、全部parent padding/只读SRV parent完整内容、六sampler重定位/CBV与原始AS源clear；EID0不残留typed accesses |
| `texel-buffer-current-bad/manifest.json` | 75坏组150 API4/CLI1且无GPUwait，3合法split-copy/runtime-ABI/original控制API/CLI通过。新增10texel例覆盖parent初态missing/short、非对齐/越界offset、row/height、readonly/numeric-family、SRV及CBV alias；原root/sampler/CBVcopy/texture/ABI拒绝保留 |
| `texel-buffer-live-parent-alias/manifest.json` | 额外将SRV parent合法创建提前，输出view复用已存在parent；API4/CLI1拒绝，准确到通用parent读写重叠条件 `metal_ray_ir.cpp:825`，无buffer-view创建失败/无GPUwait；原始控制API/CLI通过。与主坏集合中forward-reference版本分开记录，不将两者混成同一覆盖结论 |
| `legacy-texel-buffer-current/development-regression-manifest.json` | 10旧capture20 API/CLI通过，涵盖旧direct/indirect、TraceRay、纯/混合根及buffer/2D Uint/Float输出 |
| `texel-buffer-existing-output-regression/development-regression-manifest.json` | 两已有ec73真实capture当前4 API/CLI通过，当前raw buffer公共查询、五纹理多UAV/公共sampler及事件/EID0覆盖；只计当前重放，不计当前fresh capture |
| `texel-buffer-proof-AIR-counts/manifest.json` / 两 `compiler-AIR-proof` | 两真实AIR各1 texel read、1 write、1 width查询，另2D width/height/num_mips及6sample，总12texture调用；三actual fileOffset均AS1/unknownAS0/texture12/unknownTexture0/sampler6/unknownSampler0/bufferRead13/bufferWrite0/unknownBuffer0，42捕获scalar读取各；0unvalidated PSO，各2非零query。compiler/AIR事实不替代GPU输出，但GPU输出已由上述native/API核验 |

576完成日志无Assertion/ForceCrash/overrun/resource-map等严格诊断，所有构建和GPU沿共享锁串行，已退出。开发FAIL保留：首次const getter使用非const GetResourceManager、局部object shadow构建；native通过而捕获typed annotation因捕获侧parent map未使用返回40，补带锁查询后通过；API/CLI先通过但工具按texture_buffer错误intrinsic拼写断言，已改实际`write_texture_buffer_1d`；后续proof工具沿用11texture/14buffer读计数而实际AIR新增num_mips/优化掉无用stride，现核验真实12/13及全部结果，原FAIL manifest保留。所有失败不计PASS，也不声称历史Qt/UE/系统重启已修复。

范围仍限现有合法typed query/当前完整parent与预算；任意GPU producer、Shared texture-buffer创建、未知动态heap/root消费以及实际UE自动runtime PSO/动态Header全闭包不因此获准。当前库官方sample集中回归/full IR/RT/frame-family/controller lifecycle/Qt-ARC/UE整帧GPU输出/事件/EID0/总体预算未跑，不继承旧库。B544/PHASE59开放，两生产flagsfalse。下一优先完成通用runtime PSO与真实heap/root/AS Header提交依赖，参考VK/DX12的当前descriptor范围与提交快照；避免只为增加夹具结果持续扩大定向回归，固定最终库后一次集中相关验收再回UE实际截帧。

所有新产物直接外盘，内盘约32GiB/外盘1.6TiB；此前219目录17.3GiB与34frozen1.18GiB校验迁移、58缓存清理保持。新生成可重建Python cache收尾清理，capture/失败/崩溃证据保留；无覆盖用户改动、提交推送或重置。


## 通用 AS Header 发布与实际 UE 无 GPU 加载（继续同一 B544）

按用户调整后的通用API大批次规则推进，未另开微批次。对照仓库DX12/VK的descriptor source identity、GPU VA/resource+offset和提交快照方式，将Metal公有64-byte AS Header的记录/重定位与shader消费资格分开：coverage65的初态/帧内Header、slot kind3的已验证Header source、AS build/fill前置与CPU提交快照都不再要求测试query PSO声明。完整tuple、保留字零值、Captured AS ID/贡献GPU VA、Shared Header storage/range、无alias身份、frame birth及提交先后仍验证；kind3 shader实际消费仍需原有完整支持闭包。保留compute而移除PSO声明的控制继续在GPU前拒绝。

独立重放实际发现提交CPU快照用旧PSO条件选择Header overlay，导致捕获的AS ID/VA覆盖已重定位Header；现按m_RayASHeaderCurrent中的真实Header buffer身份选择，不把普通descriptor backing当不可变Header。实际UE首次native无GPU加载另暴露贡献allocation为64KiB、旧publication校验硬限16KiB；公有Header只发布GPU pointer，现按贡献identity、storage、无alias、offset/至少4-byte范围与溢出验证，consumer另行证明实际读范围/完整初态，未放宽未知GPU producer或消费资格。

当前backend及bundle SHA256 `23c61b9ae4196577b9fee4ab72f9d2f9217adb09c1deed443cc2ad96346a8bb0`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。32源码/四产品SHA备份 `checkpoint-header-publication-23c61b9a/manifest.json`，汇总 `development/header-publication-current-integration-manifest.json`，B544链接均指向 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`。构建0（保留既有GUI SDK warning），没有提交/推送/重置/覆盖用户改动。

| 当前开发结果（外盘development） | 实际通过/拒绝/未运行范围 |
| --- | --- |
| `header-independent-large-contribution/manifest.json` | 两派生capture的Header-only API及3-loop CLI通过：从真实已捕获样本去掉query/root/heap角色声明及所有compute encoder工作，保留真实AS build/原输入清零/提交；另将贡献完整初态与allocation扩到64KiB、padding=0xcd。每项1 AS build、0 compute dispatch、28事件前后往返/EID0、Header ID/VA改变且稳定、保留字零、贡献完整内容及padding不变。不是当前fresh native/capture，也不冒充独立原生场景。 |
| 同一manifest 10坏组 | reserved-word、AS identity、贡献identity、Header offset/length/Private、贡献offset、source alias、birth-after-write、unqualified-consumer：20 API4/CLI1拒绝，均无GPUwait。最后一例保留实际query compute并移除声明，证明公有Header发布解耦没有授予未知shader消费权限。 |
| `header-publication-consumer-regression/development-regression-manifest.json` | 四已有capture当前8 API/CLI通过：两Private standalone/Tracked placement texel SRV/UAV（4253/4253、各48事件/EID0与24 typed公共API检查）、五texture multi-UAV（2375→2687、48事件/EID0/144身份及48descriptor查询）、已有B512 TraceRay。只计当前定向重放，不计当前fresh native/capture或完整旧回归。 |
| `typed-header-runtime-current-CPU/manifest.json` | c0ba中间库CPU-only coverage65接受真实UE `403f90ea8c23638b8c817701725e31885c7929c09fac169a99e7728a66ac092b`，无loading/GPU；不可当成当前23c库完整验收。 |
| `typed-header-runtime-pre-submit/manifest.json` | c0ba中间库native无GPU加载在初态DeclareRayASHeader因64KiB贡献allocation被旧16KiB条件拒绝；峰值RSS1,540,292,608bytes；失败保留。 |
| `typed-header-runtime-large-contribution-pre-submit/manifest.json` | 当前23c库metadata/加载已越过三初态Header及其64KiB贡献pointer/完整初态，ValidateDescriptorFrame在MTLHeap::newBuffer(offset)、streamOffset32128拒绝。exit4是诊断/拒绝退出，accepted=false；无GPUwait、无initial upload/frame GPU，峰值RSS1,574,125,568bytes、45秒/3GiB RSS上限未触发。不能计UE输出或replay通过。 |
| `typed-header-runtime-large-contribution-pre-submit/allocation-inventory.json` | CPU离线67 heap=1,521,585,152bytes、6 standalone buffer=23,068,928bytes，共1,544,654,080bytes物理backing；1142 heap buffer/7 heap AS不重复累加，buffer texture/view不当作独立allocation。zip原始payload1,138,534,078bytes。库存不含原生driver/compiler开销；本机16GiB，运行前memory_pressure -Q报告free69%。 |

92当前完成日志无Assertion/ForceCrash/overrun/resource-map残留等严格诊断；构建/GPU/诊断均沿共享锁串行且已退出。开发工具两个构建失败（使用旧OpenCapture签名、Message()链接缺DoStringise）分别保留first/api-corrected目录；link-corrected/diagnostics中的API6明确是上述提交快照遗漏重定位，后续修复通过，未改写旧FAIL。此前901c/c0ba中间manifest也保留各自hash，不冒充当前23c验收。

下一实际缺口是placement buffer frame closure。按当前序列化32-byte header/64对齐离线推算，offset32128对应chunk29781，Private Tracked heap3138/buffer14712、length229376、offset1662976，与背景buffer14617 length65536 offset1792000重叠，可能触及旧DescriptorPlacementAliasLimit=128KiB；这是待补原生定位诊断确认的候选，不把简单提高预算当作完成生命周期证明。此前口头Shared描述已更正，不把该拒绝认定为Shared。之后继续真实runtime PSO/当前heap/root/提交writer依赖；禁止引擎、Lumen、pass、shader名称权限分支。

当前库官方sample集中回归/full IR/RT/frame-family/controller lifecycle/Qt-ARC、UE initial GPU上传/整帧输出/事件定位/EID0/资源绑定/总体GPU预算均未跑，Qt/UE崩溃/系统重启修复未证明。两supportsRaytracing生产flags仍false，B544/PHASE59开放；固定功能集成后的最终库集中相关验收，再执行短UE真实光追截帧。所有新产物/检查点直接外盘，内盘约32GiB/外盘1.6TiB，保留capture/失败/系统证据，仅删可重建缓存。

## B544 通用 buffer alias / 无窗口 capture / child-device 生命周期开发证据（2026-10-07）

继续用户规定的大粒度B544/PHASE59；本节是开发集成检查，不新开批次，不关闭最终RT验收。Lumen仅是实际验收目标，代码没有UE、Lumen、pass或shader名称权限分支。参考本仓库DX12 `d3d12_device.cpp:2995/3052`、VK `vk_core.cpp:2887/3002`允许无swapchain/backbuffer的EndCaptureFrame及条件thumbnail；参考DX12 `d3d12_replay.cpp:1995`、VK `vk_replay.cpp:2990`区分当前descriptor store资源保持与实际shader descriptor access。

通用Metal实现：coverage65 placement buffer physical alias的旧/新allocation预算单独设为1MiB，不扩大texture的128KiB预算；仍验证Tracked heap、native footprint/alignment/range、birth与写入/提交顺序、retired descriptor来源及AS冻结证明。背景和帧内buffer的物理重叠GPU writer使旧内容/scalar事实失效，未知GPU producer不继承旧初态。本次仍按整个allocation保守失效，尚未实现partial known-copy到alias的scalar区间传播。真正无窗口capture可无drawable保存，显式窗口仍要求对应backbuffer；pending enqueue reservation仍拒绝。capture child ObjC代理保持owner device直到child清理结束，device销毁前释放内部frame capture record，支持空typed表中的普通资源操作；没有授予未证明shader/RT消费资格。

当前backend/bundle SHA256 `63ef3240d9c5df8ac8fba990da0daa1663febfb44f2d51afa6c75b9823ea5e7e`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建0、diff-check0；39源码/四产品SHA检查点 `checkpoint-placement-buffer-63ef3240/manifest.json`，汇总 `development/placement-buffer-current-integration-manifest.json`。根为 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，内部B544路径保留symlink；构建/GPU/pre-submit均共享锁串行执行且已退出。

| 当前库实际证据（外盘development） | 结果与范围 |
| --- | --- |
| `placement-buffer-current-63ef3240/manifest.json` | 两fresh Native/capture/API/CLI、229376与1048576-byte新Private Tracked alias叠在旧1MiB背景buffer；每项48事件前后往返/EID0、完整A/B/readback物理重叠值与未写padding、CopyDst usage通过。无drawable、无descriptor表、无RT dispatch，不能冒充RT输出验收。8坏组16 API4/CLI1拒绝且无GPUwait：frame/old超1MiB、unaligned、outside heap、duplicate birth、birth after writer、Untracked、copy越界。 |
| `placement-buffer-owner-current-63ef3240/manifest.json` | fresh 1MiB Native/capture/API/CLI/export通过；StartFrameCapture之前释放应用device引用，queue/heap/buffer的device identity相同，48事件/EID0和干净退出通过。 |
| `placement-buffer-lifecycle/manifest.json` | 同一process两坏capture拒绝后再完整打开有效headless capture、读取初态list、核验prefix/overlap输出及EID0/action/EID0、关闭controller，10轮（20拒绝+10正常打开）PASS，Resident growth9,027,584bytes（64MiB上限）。不是完整所有RT/controller生命周期矩阵。 |
| `placement-buffer-fresh-RT/manifest.json` | 当前native源码重建，复用已审查固定converted metallib/reflection；fresh Native/capture及API/3-loop CLI通过，真实RayQuery4253/4253/0/0、528uint、Private placement typed texel SRV/UAV、纹理尺寸/采样、六根/帧内Private CBV、64几何BLAS及Private TLAS、1/132/0间接query、48事件/EID0与24公共typed descriptor查询。不是重新下载/重新编译官方sample，也不是UE调度。 |
| `placement-buffer-consumer-regression/development-regression-manifest.json` | 四已有capture当前8 API/CLI：Private/placement texel、multi-UAV2375→2687、B512 TraceRay；只计当前定向重放，不能计fresh capture/全量旧回归。 |
| `placement-buffer-UE-submission-diagnostic/manifest.json` | 实际UEcapture403f90ea已越过旧229376-byte alias阻塞，在MTLComputeCommandEncoder::dispatchThreadgroups offset246400/encoder14984/pipeline3731/table25/slot27264拒绝；exit4 accepted=false，无initial uploads/frame GPU/GPUwait/严格诊断，峰值RSS1,575,649,280bytes，45秒/3GiB限制未触发。普通PSO AIR审计未发现ray调用，但含CBV动态descriptor索引与buffer store；绑定全局表中的AS slot并非实际AS消费。不能直接删除资格检查后执行未知GPU地址。 |

118当前完成日志无Assertion/ForceCrash/overrun/resource-map/record残留等严格诊断。保留新复现的capture结束后Buffer dealloc→ResourceManager释放→owner失效两份native .ips及SHA（`placement-buffer-crash-evidence/manifest.json`）；新fixture从capture首次discard、owner悬空、internal frame record残留、empty table误拒绝到修复的FAIL目录及首compile失败全部保留。旧876a开发通过仅归其hash，不计当前库验收。用户历史Qt/UE ForceCrash及系统重启尚未由本复现证明修复。

下一继续同一B544通用runtime PSO绑定与实际资源消费闭包：分开全局typed heap/AS Header发布与shader实际AS消费，按真实AIR、immutable runtime根ABI、当前descriptor来源、GPU writer/提交/资源生命周期证明普通动态buffer/texture读写及RT消费；仅删除AS guard或根据shader名称/无ray字符串放行均不接受。未知地址、GPU producer和不支持能力仍拒绝。相关功能集成后固定最终库集中官方sample/full IR/RT/frame-family/Qt-ARC/生命周期与损坏回归，再短UE真实光追截帧、输出/重放/事件/EID0/资源绑定/总体GPU预算。当前这些最终项目及UE initial GPU上传/frame执行均未运行，两supportsRaytracing生产flags仍false；AS内部、RT shader单步/RT Pixel History仍不承诺。

全部新增capture、失败、构建/回放日志、native/replay helper、系统证据、检查点直接外盘；内盘约32GiB，外盘约1.6TiB。先前迁移219目录17.3GiB及34 frozen目录1.18GiB/58 cache清理证据继续保留，本轮未产生需要再迁移的本地测试目录，未删除capture/故障证据或主构建。没有提交、推送、重置或覆盖用户RDHeaderView改动。

## B544 通用 runtime PSO 实际 descriptor/buffer 消费（2026-10-07）

继续同一大批次开发，没有新建微批次或关闭PHASE59。参考本仓库DX12 `d3d12_replay.cpp:1995`的当前PSO/static descriptor access/bindless feedback与 `d3d12_command_list_wrap.cpp` root CBV已知resource+offset、VK `vk_replay.cpp:2990`当前pipeline descriptor access及普通command提交/资源保持。Metal的runtime元数据只提供根布局/绑定点/线程组；shader访问由实际捕获AIR与当前typed slot/根来源、提交时CBV字节建立，不使用UE/Lumen/pass/shader名称权限分支，不假造测试query PSO声明。

新增普通runtime路径在完整frame preflight中冻结根ABI、inline pointer来源及当前slot快照，在各command commit顺序使用背景完整初态、捕获CPU提交快照与已证明GPU blit字节计算实际buffer地址依赖。实际读写需当前typed来源、native allocation/range和SRV/UAV角色；静态sampler来源受根table及count边界约束。对真实buffer/texture writer及其parent/Tracked物理alias使旧scalar事实失效；resource write residency是权限，不能把未消费/未写CBV当作GPU改写。未知GPU输入、地址、调用和不完整资源消费仍拒绝。AS slot可被全局表保持但不能因此授予shader消费权限；runtime RT消费仍需后续AS/Header/完整消费闭包，本路径对真实query reset继续拒绝。新增AIR unsigned icmp/known select只选择已知条件的typed分支，原始inttoptr sentinel/unknown条件不变成合法指针；修正struct GEP贪婪解析把动态索引错认作base的问题。没有运行或解释RT shader输出，没有增加AS内部查看/RT单步/RT Pixel History。

当前backend/bundle SHA256 `f68bb0be738f4081227cd9b8249143a4c0ea63d7d3a4df43b58d0fdde61da405`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建0、相关diff-check0；43源码/四产品校验检查点 `checkpoint-runtime-consumer-f68bb0be/manifest.json`，汇总 `development/runtime-consumer-current-integration-manifest.json`。所有产物位于 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，内部B544路径仍为symlink。构建及GPU/pre-submit验证共用独占锁、没有重叠启动UE/qrenderdoc。

| 当前库实际证据（development） | 实际结果与范围 |
| --- | --- |
| `runtime-consumer-RT-boundary/manifest.json` | 自有MIT compiled MSL场景，准确声明与实际native两个buffer参数/16-byte roots一致的runtime ABI（不是官方sample、不是IR converter输出、不是shader访问声明）。fresh Native/capture/API/CLI PASS，背景真实BLAS/TLAS及未消费公有AS Header/slot；两次Shared→Private CBV offset16上传，第一dispatch动态选A[3]=123，第二选B[7]=456，完整64uint/每buffer未写padding、CBV padding和48事件前后往返/EID0通过。write residency包含只读CBV和两输出，实际消费仍分别只有当前目标。无RT dispatch。 |
| 同一manifest + `runtime-public-query/manifest.json` | 原API20次GetDescriptorAccess检查；随后同一真实capture用扩展helper额外20次access/GetDescriptors/GetDescriptorLocations联合核验，输出A/B身份、ReadWriteBuffer类别、offset0/size256/RW location与48事件/EID0全部通过，AS未出现在实际access中。扩展helper不是新native capture，不把两组查询数量合并冒充更多事件。 |
| 同一manifest 9坏组 | 18 API4/CLI1拒绝、均无GPUwait：unknown-index、AS-as-buffer、write-outside、readonly-UAV、missing-ABI、missing-root、missing-source、unknown-GPU-input、unqualified-runtime-ray。最后一例替换为已验证真实RayQuery metallib/entry main，保留相同runtime布局元数据；日志valid=1、AS=1/unknownAS=1、unknownCall=8，完整frame在commit前拒绝，未执行错误根/原始GPU指针的RT shader。该库SHA20263b11a1dda2a42560ab772cc7e0bb2d44c0291bc16daeeb7f6aaafe648924。 |
| 同一manifest CPU checks | 既有AIR parser检查和新增known compare/select合法typed pointer、raw sentinel、unknown predicate、unknown external call检查通过；真实compiled MSL动态struct GEP由fresh捕获/重放覆盖。不是完整AIR/LLVM解释器。 |
| `runtime-consumer-fresh-RT/manifest.json` | 当前native源码重建，复用已有固定converted metallib/reflection；fresh Native/capture/API/CLI通过，真实RayQuery4253/4253/0/0、528uint、64几何BLAS/Private TLAS、GPU/Private输入冻结清零、typed texel SRV/UAV、CBV/六sampler、1/132/0间接query、48事件/EID0及24 typed公共查询。此场景仍使用已有完整显式query支持闭包，不代表实际UE runtime RT consumer已支持。 |
| `runtime-consumer-regression/development-regression-manifest.json` | 四已有capture当前8 API/CLI：Private/placement texel、multi-UAV2375→2687、B512 TraceRay；只计定向旧capture重放，不计fresh/full最终验收。 |
| `runtime-consumer-UE-pre-submit/manifest.json` | 实际UEcapture403f90ea当前通用验证在首个PSO437 dispatch offset32320/首提交commit42432拒绝：valid=1/read9/write3/unknownBuffer7、AS/texture/sampler/unknownCall均0。首shader的phi循环、三buffer按输入索引scatter/copy尚未有地址闭包；此前普通非AS dispatch没有完整runtime消费证明，不能宣称已越过旧3731阻塞。exit4、accepted=false，无initial uploads/frame GPU/GPUwait、无严格诊断；peak RSS1,580,875,776bytes，45秒/3GiB上限未触发。不是UE replay/output PASS。 |

108当前完成日志无Assertion/ForceCrash/overrun/resource-map/record残留等严格诊断。保留首次backend编译的ResourceId/bytebuf类型错误、源MSL而非AIR的控制拒绝、compiled MSL动态struct GEP错误、replay工具GetDescriptorAccess旧签名错误及其FAIL日志；后续对应修复/复验完成，未改写旧结果。之前63ef的alias/headless/lifecycle证据只归旧库，当前controller全量/lifecycle并未重跑；历史UE/Qt崩溃及系统重启仍未证明修复。

下一实际任务继续通用buffer输入/producer消费链：按immutable ABI和真实提交时输入，支持能证明边界的循环phi、data-indexed buffer/scatter访问、已知copy经物理alias后的区间事实传播；未知GPU生成索引/地址仍须来源/范围证明。Actual UE PSO437仅用于定位/验证，不在实现中按其ID/名称识别或放行。随后完整runtime texture/sampler/AS Header与真正RT消费闭包，固定最终库集中官方sample/full IR/RT/frame-family/Qt-ARC/lifecycle/损坏回归，再UE真实RT截帧/整帧输出/重放/事件/EID0/绑定/总体GPU预算。当前这些最终项目与UE initial uploads/frame GPU全部未完成，supportsRaytracing及FromRender生产flags仍false，目标active；没有缩小到普通runtime store作为完成目标。

新中间/测试/检查点文件全在外盘，内盘约32GiB、外盘约1.6TiB；无需新增本地大目录迁移，保留capture/故障/系统证据，不删主构建。没有提交/推送/重置或改写用户RDHeaderView改动。

## B544 通用 GPU 拷贝输入链集成开发（2026-10-07）

继续同一B544/PHASE59，开发必要的定向检查，未关闭批次或运行最终集中验收。参照DX12 `d3d12_command_list_wrap.cpp:5193`原生CopyBufferRegion/事件/资源+offset及VK当前descriptor资源与提交语义，Metal的普通buffer copy仍调用Native API；新增输入证明只传播实际已发布字节，不解释shader输出，不按UE/Lumen/pass/entry名授予权限。

Compute输入copy现在在commit按已捕获CPU更新、前序copy和实际shader writer顺序取源；Private/帧内源可通过多级copy获得逐字节known事实，未写区间保持unknown。目标小范围分段copy不覆盖其余padding。源不在CPU事实预算时仅允许已有Shared上传完整证明；已知GPU writer禁止回退静态旧字节。真实writer及物理alias加入失效集合，延迟创建的buffer事实也不能重新seed旧初态；没有扩大Native调度、heap或总内存预算，也没有将未知GPU计算结果当作CPU事实。物理alias已知区间传播与更广循环/RT consumer仍待实现。

当前backend和app内bundle SHA256 `f1988b677fd8001120fc941bf0d754e9db47404e79d6cb1705de4a119a4b312d`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`、provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建0、diff-check0。外盘根 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，内部B544保持symlink；`checkpoint-runtime-copy-chain-f1988b67/manifest.json`保存43源码/四产品SHA与复现脚本，汇总 `development/runtime-copy-chain-integration-manifest.json`。

| 当前库development证据 | 实际结果/限制 |
| --- | --- |
| `runtime-copy-chain-direct-final/manifest.json` | fresh普通runtime compiled MSL准确根ABI场景Native/capture/API/CLI通过，2实际stores A[3]=123/B[7]=456、全64uint输出/CBV padding、48事件往返/EID0、20当前typed公共access/descriptor/location身份检查。背景真实BLAS/TLAS+未消费AS slot；没有RT dispatch，非官方sample/非IR converter runtime shader。9坏组18 GPU前拒绝。 |
| `runtime-copy-chain-background-final/manifest.json` | fresh背景Private64-byte staging：Shared16字节→Privateoffset32→两段Private CBV offset16/24，各8字节，同command；完整Native/capture/API/CLI及48事件/EID0/20公共查询通过。12坏组24 GPU前拒绝，包括缺producer、未发布范围及源越界。 |
| `runtime-copy-chain-frame-final/manifest.json` | 上述链的Private staging在capture期间创建，无初态字节；只有真实copy发布范围可用于消费。fresh Native/capture/API/CLI、48事件/EID0/20查询、12坏组24 GPU前拒绝通过。 |
| `runtime-copy-chain-separate-final/manifest.json` | 帧内Private staging在前一个command提交，分段Private→Private CBV和consumer在后一个command；两轮CPU上传/不同值。fresh Native/capture/API/CLI、48事件/EID0/20查询、12坏组24 GPU前拒绝通过。 |
| `runtime-copy-chain-opaque-final/manifest.json` | fresh真实native/capture仍输出A[3]=123/B[7]=456；Shared上传→Private staging→实际runtime shader写该staging→Private CBV copy。shader写的字节没有CPU publication证明，API4/CLI1在完整frame GPU前拒绝，无GPUwait。即使原生写回同值，也不能由旧initial/CPU source证明新GPU字节；此项是预期拒绝控制，不计replay/output PASS。 |
| `runtime-copy-chain-fresh-RT/manifest.json` | native当前源码重建，复用已有固定converted metallib/reflection；fresh Native/capture/API/CLI通过，实际RayQuery4253/4253/0/0、528uint、64几何BLAS/Private TLAS、GPU构建输入冻结/清零、Private texel SRV/UAV、帧内CBV/六sampler、间接1/132/0、48事件/EID0和24 typed公共查询。仍属于已有完整显式query闭包，不证明UE runtime RT consumer可用。 |
| `runtime-copy-chain-regression/development-regression-manifest.json` | 四已有capture当前8 API/CLI通过：Private/placement texel、multi-UAV2375→2687、B512 TraceRay；只计当前定向重放，不计fresh/全量集中验收。 |
| `runtime-copy-chain-UE-input-audit.json` | 实际403f90ea UEcapture的CPU初态+首consumer owner14706提交前CPU snapshot检查：Shared WriteCombined buffer4311/2MiB、root17408初态是旧float字节，snapshot16898+526发布后真实参数[4,1,0,2]；临时heap14704 slot0→同buffer16896的96-byte descriptor payload，slot1→17152四索引[860,861,862,863]，slot2→table25目标。明确输入/当前来源，既有逐24-byte producer契约需保留；仅CPU审计，不是Native preflight/UE frame GPU/输出验收。ID/entry仅定位，不进入实现规则。 |

四positive合计192事件/EID0、80公共查询，45坏组90 API4/CLI1无GPUwait拒绝；另一opaque writer fresh控制两次预期GPU前拒绝。404当前完成日志无Assertion/ForceCrash/overrun/resource-map/record残留等严格诊断。早期d2525d6a frame/separate结果仅归旧hash，最终四positive/fresh opaque均用当前f1988b677fd8001120fc941bf0d754e9db47404e79d6cb1705de4a119a4b312d复验。保留 `runtime-copy-chain-regression-name-collision/artifact-note.json`：克隆复现脚本初次输出目录未完全替换，旧目录被当前结果覆盖；已保存混合append日志、从旧汇总保留原f68结果元数据，修正脚本在独立当前目录重跑8项通过，不能声称被覆盖的旧stdout恢复。checkpoint脚本过早读取缺失新目录的错误不计测试FAIL/PASS，最终正确目录与汇总已验证。

下一仍是同一B544通用有界loop phi、实际输入索引/typed descriptor GPU生产消费及动态Header/真正RT consumer闭包。实际首PSO的四次循环及四个24-byte散写是定位证据；按immutable根、当前源身份、已发布输入和准确producer source/destination验证，不能硬编码这个shader/UE效果，也不能跳过7 unknownBuffer后提交。当前未重复未变化的UE pre-submit loop失败；当前库UE初态上传/整帧GPU/输出/事件/EID0/绑定、官方/full IR/RT/frame-family/controller lifecycle/Qt-ARC最终集中验收均未运行。两supportsRaytracing生产flags仍false，历史UE/Qt崩溃和系统重启未证明修复，目标active。

持续任务已保存大批次、通用API、DX12/VK参照及外盘优先规则，当前ACTIVE。所有新产物/检查点直接外盘，检查无本地非symlink Metal测试产物目录，内盘约32GiB/外盘1.6TiB；仅删本轮生成19,066-byte可重建Python cache。此前219目录17.3GiB+34 frozen目录1.18GiB迁移/58 cache清理证据保留。不删除原始capture/故障/系统证据或主构建，不覆盖用户RDHeaderView改动，无提交、推送、重置。

## B544 通用有界循环、逻辑buffer view与真实消费（开发中，acc8b703）

仍在同一B544/PHASE59集成批次；以下是必要定向开发检查，不关闭批次、不替代最终固定版本集中验收。

- 延续DX12 `d3d12_command_list_wrap.cpp:2030` 的root资源+offset、`d3d12_replay.cpp:1195` 的buffer descriptor呈现及 `:1995` 的实际descriptor access，VK `wrappers/vk_cmd_funcs.cpp:3805` 的执行点dynamic offset/descriptor绑定。底层Metal继续原生执行shader和copy；当前增加的是资源来源、提交时输入与地址范围证明，没有模拟shader输出或使用引擎/pass/shader名称放行。
- AIR支持可证明的单block、零起点、step1、整数i32/i64循环（正界限≤65536），`ult/ne`回边或`eq`退出；bound必须独立于phi递归。限定三次分析传播，不执行迭代。实际已发布的readonly整数输入可给出保守min/max，逐字节known、range≤64KiB；默认旧loader不接受ranged load，runtime/public查询显式启用。零count、非单位step、recurrence bound、未知calls/地址和无法证明输入继续拒绝；未宣称一般循环/任意控制流已支持。
- buffer view使用已捕获24-byte descriptor低32位metadata长度与已知source offset；有长度时必须在native allocation内并约束实际消费，legacy unsized保留已有行为。统一wire tags0/1 BufferSRV/UAV、2/3 TypedBufferSRV/UAV、4/5 TextureSRV/UAV、6CBV、7Sampler；对象/source field仍独立验证，buffer写仅UAV角色。普通数据buffer即使另有descriptor lease，也不会按metadata slot误读。tag0/1/2/3有本轮fresh证据；不把tag6公共呈现或所有format/图形stage称为本轮已逐项验收。
- scalar/index输入参与实际writer重叠检查，包含buffer和texture parent/物理alias；shader能改写当前范围时，拒绝依靠旧输入证明后续loop地址。后续真实writer仍使旧字节失效，不重载旧初态。新增counterexample让索引源指向输出，第一次写覆盖后续索引；明确观测`Metal runtime scalar writer overlap`，在GPU前拒绝。另逻辑64-byte descriptor不足以覆盖native256-byte内的index55写，仍拒绝。

当前backend及bundle SHA256均 `acc8b703d59d48f80e5979ca4d56ff9ca5b25b29797c52a3049f605a57bdae56`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建terminal0；45源码/四产品检查点 `build-macos-debug/metal-ray-b544/checkpoint-runtime-loop-acc8b703/manifest.json`。全部新产物在该B544外盘链接下，合并证据 `development/runtime-loop-integration-manifest.json`。

| 范围 | 当前acc8实际结果与限制 |
|---|---|
| `runtime-loop-overlap-guards` / `runtime-loop-separate-types` | 两fresh ordinary Native/capture/API/CLI PASS，直接及Shared→Private→Private跨提交输入，type0/1/2/3；各48事件往返/EID0、20公共access和60 descriptor检查，完整64uint输出、输入/CBV/padding；14+15坏组共58 API4/CLI1无GPUwait拒绝。Native compiled MSL准确runtime ABI，**没有RT调用，不能称为光追sample验收**。 |
| `runtime-loop-fresh-RT` | fresh真实converted RayQuery Native/capture/API/CLI PASS，4253/4253/0/0、528uint、Private64几何/TLAS、typed texel/六sampler、六混合根及间接1→132→0、48事件/EID0及公共查询；复用已审查compiled shader，不是新官方sample或实际UE。 |
| `runtime-loop-regression` | 四已有capture的8 API/CLI PASS：Private/placement typed buffer、multi-UAV2375→2687及B512 TraceRay。是旧capture当前库复验，不称fresh。以上244完成日志严格诊断为空。 |
| `runtime-loop-UE-CPU/proof-final-manifest.json` | 原始捕获的submitted root `[4,1,0,2]`、索引860–863和96-byte payload，actual AIR报告1loop/9reads/3writes/0unknown/0calls；CPU PASS，未执行GPU、未核验输出。 |
| `runtime-loop-UE-buffer-types/manifest.json` | 当前真实capture预检越过first scatter及后续相同compute，停在draw streamOffset283456（encoder15002，render PSO3376；旧diagnostic误印compute pipeline0）。exit4，peak RSS1,601,683,456bytes，45s/3GiB限制未触发，无initial uploads/frame GPU/GPUwait/严格诊断；**REJECTED/INCOMPLETE**，不能计UE重放通过。 |
| `runtime-loop-UE-graphics-AIR/manifest.json` | 同capture的depth-only PSO、vertex main及linked stage-in原始AIR/函数/library hash留证。没有fragment，两函数无query调用，main有visible-function-reference、linked函数读取vertex/instance数据。CPU事实不授予draw执行资格；下一补通用graphics参数/linked function/实际descriptor消费闭包，不能仅凭无ray调用放行未知访问。 |

保留中间失败：`runtime-loop-first` helper enum编译失败；`runtime-loop-native-first` Native/capture正确但公共descriptor size断言失败；`runtime-loop-subrange` dd3f PASS属于旧库；`runtime-loop-UE-range-proof` 35e4在type0旧guard拒绝。`runtime-loop-buffer-types` 当前acc8早期direct12坏组PASS仅补充证据，不与上表重复累计。没有重写旧失败manifest为PASS。

当前生产supportsRaytracing及FromRender仍false；仍缺runtime RT真实AS/Header消费闭包、graphics linked/动态输入、实际UE完整上传/输出/重放/事件/EID0/绑定及最终official/full IR/RT/frame-family/lifecycle/Qt-ARC集中检查。历史Qt/UE崩溃/系统重启未证明修复，不继承旧hash的final PASS。持续目标active，无需用户决策，继续按通用API链集成后固定版本集中验收。

## 2026-10-07 通用 Native graphics linked function 集成检查点（39ff0128；B544继续）

参考仓库DX12 `d3d12_device_wrap.cpp::Serialise_CreateGraphicsPipelineState` 和 VK graphics pipeline/state capture，将实际Native函数及attached linkedFunctions和逐事件绑定身份作为来源；DX12 `d3d12_initstate.cpp` / VK `vk_initstate.cpp` 保留 AS portable build 输入与子资源依赖，Metal继续保存配方而不查看AS内部。AIR是当前保守消费/地址证明的Metal内部实现，不提供RT shader单步或PixelHistory。Native/converted入口只取直接argument列表，忽略嵌套member ordinal；converted追加的shader metadata不能替代argument列表。

- `GetGraphicsAIR`取实际PSO的vertex/fragment函数及attached linked函数（functions/binary/private/groups，受容量约束），拒绝未解析函数/递归/越界/未知地址。Native vertex_id由已发布index bytes及baseVertex推导，instance_id/base_instance来自真实draw；linked函数参数由caller SSA传递，返回aggregate保留整数索引及资源依赖。float数学只产生未知浮点值，不能授予地址许可。
- 复用提交有序typed root/heap、当前Shared创建数据、CPU更新和Private copy证明；创建数据只在未被真实writer/物理alias失效时保存，descriptor地址不作为普通scalar猜测。创建数据副本纳入既有64MiB证明预算。公共GetDescriptorAccess/GetDescriptors/GetDescriptorLocations使用该事件、阶段和管线的已通过实际消费记录，EID0清空，shader replacement不复用旧记录。
- Header snapshot原先在background状态调用manager帧引用，后者直接忽略，导致unused AS heap场景有TLAS配方却没有BLAS初态。快照移入active状态，并标记当前AS配方的primitive children与输入/索引/尺寸依赖；不扩大RT consumer资格。

全部产物根为外盘链接 `build-macos-debug/metal-ray-b544/development/`，构建/GPU使用共享串行锁，无UE/qrenderdoc并发；最终 backend/bundle SHA `39ff012838798c14ac69debb032abaf32235c01da8aa3837b6af5cfa05db0319`。GUI/provider沿用已有产品hash，不把未运行GUI/UE操作计为通过。

| 当前39ff结果 | 证据/范围 |
|---|---|
| CPU PASS | `graphics-integration-checks/CPU.log`：实际位置计算/linked函数返回索引、instance/base subtraction、缺失linked、嵌套ordinal与converted追加metadata。 |
| fresh Native/capture/API/CLI PASS | `graphics-linked-final/manifest.json`：Native linked vertex stage-in，两个indexed draw/baseInstance0与1、unused有效AS Header heap，32事件往返/EID0、每次完整256float depth（first .25/second .75/EID0 1）。公共16实际vertex access及descriptor身份/16-byte view。`graphics-public-locations/manifest.json`补GetDescriptorLocations与EID0无heap access，复用同capture，不能另计fresh/重复累计。普通graphics，不是RT/UE验收。 |
| 6坏组12 GPU前拒绝 PASS | `graphics-linked-final-invalid/manifest.json`：published instance selector越界、index vertex越界、typed inline source缺失、创建/CPU输入缺失、stride越界、detached linked function；API4/CLI1，严格诊断/GPUwait空。 |
| fresh converted RayQuery PASS | `graphics-linked-final-RT/manifest.json`：当前源码Native/capture/API/CLI，4253/4253/0/0、528uint、64几何/实例、Private placement typed buffer output、六sampler/CBV、二维间接1→132→0、48事件/EID0和公共查询。metallib/reflection复用既有转换产品并记hash，没有声称新转换。 |
| fresh普通runtime循环/copy PASS | `graphics-linked-final-loop/manifest.json`：Shared→Private→Private跨提交，48事件/EID0、20 access/60 descriptor、完整输出/只读/padding；17坏组34拒绝含writer overlap、未知input和view length。 |
| 四旧capture8 API/CLI PASS | `graphics-linked-final-regression/development-regression-manifest.json`：Private/placement texel buffer、多UAV、B512 TraceRay。不是fresh capture或最终全回归。 |
| UE REJECTED/INCOMPLETE | `graphics-linked-current-UE-pre-submit/manifest.json`：原始true Lumen HWRT capture403f90ea，45s/3GiB限额内peak1,612,087,296bytes，无initial/frameGPU/GPUwait。旧draw AS资格guard已越过，但实际graphics commit消费报告尚未出现，不能声称UE graphics闭包通过。当前frame MRT renderpass303616、三个新Private placement颜色target512²（RGBA8/RG11B10Float）、Load/UnknownStore及新depth/stencil Load初态/alias语义拒绝。下一核验通用frame attachment Load/DontCare/alias与已发布写入，再证明完整graphics和runtime RT消费闭包。 |

失败证据保留：first/second graphics build编译错误、测试shader保留字、incorrect draw annotation的helper abort、未支持captured verification readback（后移到EndCapture之外，原渲染帧仍完整）、遗漏BLAS初态、public query未实现、AIR converted附加metadata解析回归（`graphics-linked-fresh-RT`/`graphics-linked-regression` FAIL，后续39ff修复复验PASS）。首损坏脚本仅改creation被有效commit CPU snapshot覆盖，实际有效输入加载成功，已修正为同时修改published输入；该早期FAIL不计坏组通过。原失败manifest不重写为PASS。

本轮集成检查通过不等于B544关闭；production supportsRaytracing及FromRender仍false。当前固定库official sample/final full IR/RT/frame-family/lifecycle/Qt-ARC/UE整帧输出、事件/EID0与绑定未跑；不继承旧hash的最终验收。任务active，无需用户决策。历史用户Qt/UE ForceCrash/系统重启未证明已修复。源码/四产品/七文档检查点：`checkpoint-graphics-linked-39ff0128/manifest.json`。


## B544 continuation — native frame attachments and typed pointer publication (2026-10-07)

仍为 B544/PHASE59 大批次中的实际开发检查，固定本轮最后版本集中复验，不是最终 RT enablement。

本地参照：`d3d12_command_list4_wrap.cpp::Serialise_BeginRenderPass` 保留 Preserve/Discard，仅 Clear 执行清除；Vulkan `vk_cmd_funcs.cpp` 保留原 load op。Metal frame-born 新图像首次 Load 可以含未定义像素，不强制先 Clear，不把任意 draw 当成全覆盖初始化证明；原生操作、资源出生/extent/alias/usage、level/slice、PSO 格式、depth/stencil 配对、deferred Store 及预算仍核验。

`metal_descriptor_tables.cpp` 对已有完整初态的普通 uncompressed regular colour 使用格式/comp type/block shape 验证，涵盖实际规范化/浮点家族；保持旧 coverage、cube/volume/packed/int 专用范围。`metal_common.cpp` 补既有 R8/R16 Snorm 的 readback block shape。非法 Load 枚举和未支持 color store options 拒绝。新 MIT 源码 `metal_frame_attachment_load_{capture.mm,replay.cpp,gate.py,invalid.py,cases.h}` 与 `.metal` 以实际 fullscreen draw 完整覆盖 16² MRT 和 D32S8，独立固定 byte oracle。两个提交 first→second/first/last/0 往返，后一次 depth=.75/stencil=42，前一次 .25/7；帧内附件 EID0 无资源，background 首附件 EID0 恢复零初态。

DX12 `d3d12_command_list_wrap.cpp::Serialise_SetComputeRootShaderResourceView` 与 Vulkan `vk_descriptor_funcs.cpp` 的 descriptor address 用资源+offset 重定位，并不在发布地址时消费被指内存。Metal `DeclareRayASHeader` 同样仅发布 AS 身份及贡献指针；去掉 contribution 曾写入即拒绝的整资源门槛。CPU Header 自身 GPU writer、未提交 reader、真实身份/offset/64 bytes/padding/来源仍验证；实际 consumer 的 current contents、范围、别名/提交证明保持。AS 为黑盒，不承诺内部查看、shader debug 或 RT PixelHistory。

当前 backend/bundle SHA256 `08677fd7691535b85a10e6323ee725b87c705c4bc14b803f0ae1ea4d0bc02338`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。最后 build0、相关 diff-check0。根 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，所有以下路径相对该根；内部链接保留。

| 当前版本范围 | 证据与结果 |
|---|---|
| Fresh attachments PASS | `development/frame-native-final-attachments/manifest.json`：12 Native/capture/API/CLI，first Load 和 Discard→Load，以及 background R16Unorm/RG8Unorm/R16Float/RG16Float/R32Float/RG32Float/RGBA32Float/R8Snorm/R16Snorm/RG16Unorm；每场景32选择，共384，三个颜色及depth/stencil每plane完整256像素，EID0通过。 |
| Malformed attachments PASS | `frame-native-final-attachment-invalid` 和 `frame-native-final-initial-invalid`：分别19坏组38 API4/CLI1；共38组76，含枚举/options/level/slice/resolve/重复目标/出生/配对/PSO/缺失deferred store；无GPUwait/严格诊断。 |
| Header independent PASS | `frame-native-final-header-invalid/manifest.json`：派生 Header-only/64KiB contribution 两合法控制4 API/CLI；10坏组20 GPU前拒绝，实际 reserved/identity/offset/length/storage/source/birth/unqualified consumer。是派生检查，非 fresh Native Header-only。 |
| Fresh RayQuery PASS | `frame-native-final-RT/manifest.json`：真实 Native/capture/API/CLI，4253/4253/0/0、528uint，Private placement input/output/SRV、64 TLAS/geometry、6CBV/sampler、indirect1→132→0、原helper48事件及公共typed查询。shader/reflection复用并记录hash，不冒称重新conversion或official。 |
| Related old PASS | `frame-native-final-related-regression/development-regression-manifest.json`：21旧capture42 API/CLI；Private/placement texel、多UAV、B512 TraceRay、两compute→frame Load、linked graphics、7 depth/deferred/parallel族各两capture。MRT helper按 CommandBufferBoundary 与 encoder scope分别计数；第二capture独立oracle为186→122（第一122→186）。先前三 scope/oracle失败保留，不算backend通过或丢弃失败。 |
| UE REJECTED/INCOMPLETE | `frame-native-header-identity-UE-pre-submit/manifest.json`：实际 capture403f90ea，三Header发布已越过；PSO3731普通buffer writer已通过实际consumer报告，但PSO1707 dispatch247360 valid1/read8/write1/unknownBuffer3，于command commit557952拒绝。peak1,637,203,968bytes，45s/3GiB上限未触发，无initial/frameGPU/GPUwait/严格诊断。实际graphics commit报告仍未到达，不能计UE graphics/output/replay PASS。 |
| Extended contribution consumer FAIL/UNSUPPORTED | `frame-native-contribution-publication-RT/manifest.json`：新 Native GPU contribution copy→Header→query 原生4253/4253/0/0及capture成功，API4，未跑其CLI；`frame-contribution-copy-rejection-check/manifest.json` 当前 trace确认 dispatch(indirect)11089856 GPU前拒绝、无严格诊断。`frame-native-GPU-contribution-header-only/manifest.json` 派生扩展控制亦API4，未跑余下case，保留FAIL。这些不能因为新指针规则计为支持，下一补底层实际GPU贡献版本/consumer闭包。 |

其他保留的中间结果：`frame-native-load-first` helper编译错误RDCMAX；`frame-native-color-family` Snorm Native/capture成功而 API创建拒绝；diagnostic build失败的 `%u`/size_t 后改 `%zu`；cec5/f925/124f/75ae 中间测试和实际UE303616→387136→396736→412928阻塞定位各留证，其通过不继承到0867。最终汇总 `development/frame-integration-evidence-manifest.json`，校验检查点 `checkpoint-frame-integration-08677fd7/manifest.json`。

下一仍同一大B544：按实际 PSO1707 的3未解析buffer地址与当前输入/producer证明通用地址闭包，之后完整 graphics、真正RT consumer与GPU contribution版本；不能按UE/Lumen/shader名特判，不能用旧初态覆盖未知新GPU数据，不能忽略unknownBuffer提交。保持支持RT生产flags false。当前没有最终official两scene/full IR/full RT/全部frame/lifecycle/Qt-ARC/native能力复验，也没有UE整帧GPU输出、事件/EID0和绑定通过证据；历史ForceCrash/Qt崩溃/系统重启没有证明修复。任务active，无用户阻塞，无自动提交推送。

## B544 continuation — dispatch / scalar / sourced GPU descriptor development and direction correction (2026-10-07)

仍是同一大集成批次，未关闭 B544，未启用生产 RT。采用用户最新“恢复/重定位/提交/展示分离”准则，见 `RT_API_INTEGRATION_WORKFLOW.md`；本节历史展示 guard 的拒绝记录不代表未来必须永久拒绝对应动态访问。

本轮已实现真实 dispatch 的 threadgroup 三维坐标和 local linear index、保守无零除数的 unsigned div/rem 范围；精确无条件 compute 整数 store 的逐字节版本可作为可选当前值事实，CPU/copy/物理 alias/后续未知写入失效，不写回伪造 GPU 输出；raster coverage/条件/原子/未知 store 不发布已知值。稀疏状态计入节点预算。已验证的 GPU descriptor copy 元数据与 typed source 相等时，runtime 可读它的重定位来源；GPU-owned destination 仍由真实复制写入，绝不改为 CPU 上传。上述都是通用资源/地址语义，没有 UE/Lumen/shader名特判。

最后 backend/bundle SHA256 `aeec766b1fe34bb432b992ce4208c13624187f9d107d7ddf1f897e1cd40d5a8d`。产物根 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，以下路径相对根；完整检查点 `checkpoint-replay-display-direction-aeec766b/manifest.json`。

| 范围 / 对应版本 | 实际证据 |
|---|---|
| 当前 GPU descriptor Native/capture/replay PASS（aeec） | `development/GPU-descriptor-runtime-native/manifest.json`：初始 heap 两槽为互换对象，真实 GPU copy 后正确写 A[3]=123/B[7]=456，完整 256-byte outputs/padding；48事件/EID0及20公共访问/descriptor点，跨提交 frame Private CBV 拷贝；16派生组32 API4/CLI1于GPU前拒绝，含缺descriptor copy、GPU值与来源不匹配。普通数值变异类旧 guard 的政策仍需重新分类，不能把所有旧拒绝当最终API验收要求。 |
| 前一固定版本集中定向 PASS（ac0d） | `development/invocation-scalar-final-manifest.json`：counter/group/loop 三 fresh Native/capture/API/CLI、144事件/EID0/60公共access点（descriptor共100），41派生组82拒绝；三合法Native/capture但conditional/overwrite/opaque replay被旧guard保守拒绝的控制，非支持PASS；fresh converted RayQuery4253/4253/0/0、528uint、48事件/公共查询，21旧capture42 API/CLI通过。是ac0d版本证据，不自动继承为aeec的全部回归。 |
| 当前 UE REJECTED / INCOMPLETE（aeec） | `development/GPU-descriptor-runtime-UE-pre-submit/manifest.json`，实际403f90ea capture；越过first PSO1707 dispatch247360（unknownBuffer0），第二dispatch248128读取14823/14824时 unknown byte/unknownBuffer1，于commit557952仍拒绝。peak RSS1,635,532,800 bytes，未触发45s/3GiB限制，无initial/frame GPU/wait/严格诊断。真实捕获有4-byte输入copy，64线程中分支只消费有效范围；这是拆展示门槛的具体样本，不能据此宣称UE资源恢复或整帧输出验收完成。 |
| CPU/方向调整 | `development/group-invocation-CPU/post-direction-{build,test}.log` exit0。后续未构建的分支范围扩展已从backend源码撤下，归档 `development/guarded-range-experiment-not-built/manifest.json`。新 `metal_runtime_guarded_index.metal` 及Native fixture参数保留作未来“分析不完整但合法重放”验收，Native/capture/replay尚未运行，不能计通过。 |

保留实际失败：首group fixture tg_size元数据不准确；scalar proof test两次编译错误；首counter跨提交opaque旧门槛拒绝；增加合法槽后unknown-index反例使用旧索引3而得到API0/GPUwait，改为实际越界namespace4；这些失败及正确复验分别保留。诊断 ac0d 指出 GPUexpected 索引槽已有复制来源，aeec补后推进下一dispatch。临时编辑脚本归档后删除 `/tmp` 副本，清理本轮生成的 .pyc，内盘可用约32GiB；源码/用户工程/失败证据不删除，未提交推送/重置。

当前最终 official/full IR/full RT/全部frame/lifecycle/Qt-ARC/native能力判断和 UE整帧GPU输出、真正RT replay、事件/EID0/绑定均未完成，历史崩溃/重启未证明修复。两生产flags false，目标active，无用户阻塞。下一按新的分层准则完成真正的API恢复资格，再用Native sample及UE输出驱动修复，不继续把未知展示转换成新拒绝白名单。

## B544 continuation — sourced buffer restoration versus numerical access display (2026-10-07)

仍在 B544/PHASE59 大批次开发，不关闭批次、不启用光追。按用户最新准则与本地 DX12 `Serialise_Dispatch`、Vulkan `Serialise_vkCmdDispatch`/两者 `GetDescriptorAccess` 将已恢复绑定和可选数值展示开始分开。

`validateRuntimeDispatch` 对已确定资源身份的指针，独立验证已重定位 descriptor/source、逻辑 view、出生/别名、已有初态或 Native 生产者、提交与可写角色；GPU 动态偏移不能由 CPU 展示计算完整时，保留 Native 调度。分支内的静态候选 interval 越界不能直接等同实际执行越界，改为部分展示；descriptor view 本身的错误范围仍拒绝。未知实际写入仍整体使旧 CPU scalar/物理别名事实失效，不用残留 base 猜写入区间，不上传 CPU 模拟 shader 输出。新增独立 `m_IRRuntimeDescriptorAccessComplete`，公共 compute/graphics 查询只返回已识别 descriptor，按当前正确 pipeline 身份选取；不把全 heap 驻留声称为实际访问。现有未知对象/调用/RT-AS qualification 门槛尚未全部拆开，这是首段机制，不能宣称已完成所有独立恢复资格。

新 Native guarded-index fixture 在真实 64 local threads 中仅 q<1 消费 Private GPU-produced counter；候选 counter range/输出偏移的 CPU 分析不完整，Native 调度正确。此轮未再扩充 branch 数值解释器；AIR 仅记录静态候选含控制流，用于标识展示的不确定性及禁止发布条件 store 的已知 bytes。

当前 backend/bundle SHA256 `d17d244ec0a54e4b2190b8d965fb9bba7bc40ecd8ed4fee96f3c28117793186e`；GUI仍 `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`。两个构建均0，第二只修当前 compute/graphics pipeline 身份选择；日志分别保留。全部产物根 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，以下路径相对根，检查点 `checkpoint-restoration-display-d17d244e/manifest.json`。

| 当前固定版本实际范围 | 结果与证据 |
|---|---|
| Fresh guarded dynamic access PASS | `development/restoration-display-guarded-native/manifest.json`：Native/capture/API/CLI/full outputs与padding通过，48事件前后/往返/EID0、20公共access与descriptor/location点；实际 preflight 两次 `accessDisplay=partial`。9结构/来源/类型/ABI/未资格RT坏组18 API4/CLI1于GPU前拒绝。三旧数值/producer改动控制未跑，并记录到manifest；未将它们伪装为坏capture通过。 |
| Related fresh ordinary dispatch PASS | GPU-descriptor separate-copy 和 scalar-counter两 fresh Native/capture/API/CLI，分别48事件/EID0、20公共查询；15+11坏组52 API/CLI拒绝，含真实descriptor GPU拷贝缺失/来源/字节身份错误。三新场景共144事件/EID0、60公共access/descriptor点，35坏组70拒绝。 |
| Fresh converted RayQuery PASS | `development/restoration-display-RT/manifest.json`：复用并记录真正 converter 的 metallib/reflection，fresh Native/capture/API/CLI；4253/4253/0/0、528uint、48事件往返及typed公共查询，64几何/TLAS、Private placement、6CBV/sampler、原生2D indirect1→132→0。是开发 fixture，不冒称新 official/UE验收。 |
| Related existing capture PASS | `development/restoration-display-related/development-regression-manifest.json`：21旧capture42 API/CLI，texel Private/placement、多UAV、B512 TraceRay、两compute→frame Load、linked graphics及14 depth/deferred/parallel。 |
| Actual UE INCOMPLETE | `development/restoration-display-UE-pre-submit/manifest.json`：真实 capture403f90ea，无initial/frame GPU/GPUwait；仍于PSO1707 dispatch248128、commit557952 unknownBuffer1拒绝，peak RSS1,636,614,144bytes，没有45s/3GiB超限。实际AIR `%65 = select ... %62, inttoptr(i64 1024 ...)` 在输入值未知时丢失已有 pointer identity；该值未知首先来自14823/14824仅4-byte有效copy而CPU候选读256bytes。**这是当前分析的指针来源丢失，尚未证明捕获缺少实际GPU地址恢复，不能当成新feature永久不支持。** 下一补独立 pointer provenance/namespace恢复资格，Native保留原选择与合法实际访问；不通过继续解释分支计算/q范围解决全部API。 |

`development/restoration-display-evidence.json` 保存所有对应库manifest及361日志SHA256，严格诊断0；CPU测试同时核验 unknown offset 保留已知对象、缺对象仍未知、条件store不发布精确值。没有修改生产flags，两者仍false。最终 official/full IR/full RT/全部生命周期/Qt-ARC/native capability 和 UE整帧GPU输出/实际RT replay/events/EID0/绑定仍未完成；历史崩溃/系统重启未证明修复。构建、GPU及UE诊断共享锁且串行，全产物外盘，没有reset/commit/push。

## B544 continuation — pointer provenance, Native call effects and API color attachment extent (2026-10-07)

继续同一 B544/PHASE59 大批次。资源恢复、地址重定位、提交依赖与访问展示保持独立；本轮没有增加 fabs/max/dot/convert 等 shader 数值模拟，也没有 UE/Lumen/pass/函数名支持白名单。实现沿用仓库 DX12/Vulkan 的原生调度和静态/可选 feedback 展示方法。

分支指针保留所有已知资源来源，标量 GEP、结构字段投影、bitcast 与原子/声明的参数内存效果继续携带来源；数值偏移不完整时 Native 调度可执行，展示标记部分，未知写使原 buffer/物理 alias 的旧 scalar 事实失效。未知或 GPU 整数地址分支不被丢弃，不把整数1024当来源。只有 shader 字节码固定常量可作为常量指针分支；独立对照捕获原 GPU buffer VA/范围，命中则要求真正 shader 地址重定位并于 GPU 前拒绝。1024仅是 fixture 常量，**不是 Metal 官方保留地址、不是后端白名单，也没有承诺非法实际访问可执行**。常量经未知/非零 GEP 派生仍不合格。当前正例的真实 Native 分支始终选择有效已恢复对象。

数值调用按实际 module 的精确 `readnone`/`memory(none)` 声明判断无资源效果，仅接受无指针/opaque handle 的数值实参；数值返回值保持未知。`argmemonly` 通过所有显式 pointer 来源记录资源候选，无字节覆盖保证，不能误匹配 `inaccessiblemem_or_argmemonly`。AIR 原子支持实际下划线和点命名形式，只记录资源读写，GPU 执行其计算；不发布 CPU 原子输出。CPU 核验未知/缺来源、两对象分支、GPU整数、零字段投影/派生常量和声明边界。声明语义参照 [LLVM11 LangRef](https://releases.llvm.org/11.0.0/docs/LangRef.html) 与 [LLVM LangRef](https://llvm.org/docs/LangRef.html)。

通用颜色附件范围由 coverage65 的 API 8槽支持贯通 renderpass、普通 draw 和 mesh target/PSO 验证；旧 coverage 兼容范围保持，真实格式、初态/Native producer、出生、重叠、延后 Store 与提交检查不取消。参照 DX12 `Serialise_OMSetRenderTargets` 和 Vulkan `Serialise_vkCmdBeginRendering` 的 API 附件数量/当前资源处理，不按 Lumen GBuffer 数量特判。新 MRT fixture 验证6/8附件、deferred/parallel，每个颜色附件逐像素比较，包含往返与 EID0。

当前固定 backend/bundle SHA256 `2133a76f7d7c81ad6fd9e519802a1f20fcaeb720db567f90adbcea3eca6d2111`；GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`，provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建均串行0，修改后对当前固定库集中运行以下相关验证；本批未关闭，生产两个 RT flags 仍 false。

| 当前固定版本实际验证 | 结果/限制与产物（相对外盘根） |
|---|---|
| Fresh selected-pointer atomic Native/capture/API/CLI PASS | `development/native-pointer-effects-current-native/manifest.json`：Private GPU counter、64 threads/实际分支、数值函数与选中指针真实 atomic exchange，完整输出A[3]=123/B[7]=456及padding；48事件/EID0，20公共access/descriptor/location点，展示partial。10结构/来源/类型/ABI/身份/未资格RT坏组20 API4/CLI1 GPU前拒绝，包含嵌入已捕获VA反例。三旧数值/producer oracle未跑，manifest明确记录；非RT dispatch/非官方sample。 |
| Fresh API MRT extent PASS | `development/native-pointer-effects-MRT-extent-source/manifest.json`：6direct、6deferred、8deferred、8parallel四 Native/capture，各两capture，总8份API/CLI完整颜色/跨pass输出，每份25事件选择含4次EID0，共200。5坏组10 GPU前拒绝：缺第8附件、重复附件、格式交换、缺最后延后Store、越界Store index8。capture/helper SHA在同目录artifact-inventory.json；不是 UE/RT验收。 |
| Fresh converted RayQuery PASS | `development/native-pointer-effects-RT/manifest.json`：真实 converter metallib/reflection复用并记录hash，新 Native/capture/API/CLI输出4253/4253/0/0、528uint，48事件/EID0/24typed公共查询；64几何/TLAS、Private placement、6CBV/sampler、2D indirect1→132→0。既有显式query闭包，不证明UE runtime RT consumer已可用。 |
| Related existing captures PASS | `development/native-pointer-effects-related/development-regression-manifest.json`：21旧capture42 API/CLI，包括Private/placement texel、多UAV、B512 TraceRay、两compute→frame Load、linked graphics和14depth/deferred/parallel。固定当前库的相关集中验证，不冒称full旧回归。 |
| Actual UE INCOMPLETE | `development/native-pointer-effects-MRT-UE-pre-submit/manifest.json`：真实403f90ea capture，无initial/frame GPU/GPUwait，peak RSS1,180,876,800bytes，无45s/3GiB超限和strict诊断。越过1707/3776指针/调用与首commit557952阻塞，越过3233472六颜色附件创建；当前draw3330304在stage2 shader identity为空/可选阶段旧guard拒绝。下一核实 Native PSO/capture字段，区分合法无fragment阶段与捕获身份遗漏，再补相应API语义；不直接将所有缺shader/缺地址放行。UE整帧GPU输出、实际RT consumer、事件/EID0/绑定仍未到达。 |

中间版本有各自真实证据，不能继承为当前库全部PASS：8308指针分支正例通过而UE仍unknownCall15；da0f数值调用声明/原子命名正例通过，UE3776 unknownCall0但unknownBuffer1；472b结构投影+原子指针样例通过、UE无GPU推进至3233472六MRT。所有实际FAIL、CPU编译/旧oracle/stale binary、首Native原子拒绝均保留。首MRT测试使用非默认 `MTLCompileOptions.preprocessorMacros`，该捕获源码编译选项当前仍未支持，Native/capture正确而API5拒绝；改为shader源内define、默认选项后同库完整通过，没有宣称支持该编译选项。runner日志被helper重命名后读文件失败亦保留，修正日志读取，不把它算重放PASS。

`development/native-pointer-effects-evidence.json`记录当前5项manifest、243完成日志SHA与strict0（仅本固定版本列明范围）；检查点 `checkpoint-native-pointer-effects-2133a76f/manifest.json`保存源码/四产品/九文档。全产物仍在 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，内部B544路径为symlink；内盘32GiB/外盘1.6TiB，未产生内部测试产物、未删除capture/失败证据，无reset/commit/push。最终official/full IR/full RT/lifecycle/Qt-ARC/native能力与UE整帧验收未运行，历史Qt/UE崩溃和系统重启未证明修复，目标active。

## B544 continuation — optional Native stages and synchronization effects (2026-10-07)

继续同一 B544/PHASE59 大批次；按资源恢复、地址重定位、提交依赖与访问展示四层推进，没有新增 shader 数值/线程组算法模拟，没有引擎/pass/shader-name支持分支。

实际 UE capture `403f90ea8c23638b8c817701725e31885c7929c09fac169a99e7728a66ac092b` 的 PSO3376/3386/3470 明确记录 fragmentFunction=0、有效vertex function；3470同时有R8Unorm颜色和Depth32Float_Stencil8格式/实际pass附件。`development/optional-fragment-UE-records.json`是CPU capture字段证据，不冒称GPU输出。参照本地 DX12 `Serialise_CreateGraphicsPipelineState` 对零bytecode不伪造shader，以及 Vulkan `Serialise_vkCreateGraphicsPipelines` 的实际stage数组，coverage65普通vertex管线允许Native可选fragment阶段，不要求颜色附件为空或有效但未执行的fragment绑定为空。管线/对象/绑定范围与Native创建检查独立保留，缺失vertex/错误shader仍不自动放行。`GetGraphicsAIR`已按明确无fragment返回空入口，不为它生成Native消费任务。

公共 `GetDescriptorAccess`过滤当前没有Native shader的阶段，保留`GetPipelineState`中的有效绑定；驻留/残留binding不是已执行访问。新样例的首轮Native与重放颜色/深度正确，但public fragment access仍存在，API95暴露并修复此展示问题；不把仅Native正确当成公开访问验收通过。生产opaque MetalFX资源展示的既有独立分支保持。

实际compute PSO1624的AIR包含Native wg/simdgroup barrier和atomic fence，旧分类把fence当device-buffer原子、barrier当未知调用。现在按真正Native同步签名区分资源效果与同步效果：原shader/原GPU barrier/fence照常执行，threadgroup内容和数值返回仍未知，不发布CPU线程组输出或旧scalar事实。同步不能代替device-buffer读写/地址来源证明；真正buffer atomic仍记录其源对象的读写并使未知写入事实失效，未知外部调用仍需资源/恢复资格。CPU测试含三类同步及未知调用负例；匹配表达式在入口分析开始时构造，避免逐instruction重复创建。**Fresh Native样例实际执行wg/simdgroup barrier与threadgroup读写、Private GPU counter、真实atomic exchange；AIR atomic fence此轮只有CPU与真实UE无GPU预检证据，UE实际GPU fence尚未验。**

当前固定 backend/bundle SHA256 `f9718c71e8b29410ca5d5c9be05cd0fd79aea7f51010dbf5e2c9ee8103168ff9`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`、provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建/GPU/UE预检共享锁串行，最终库集中执行以下相关验证：

| 当前版本实际范围 | 结果与证据（相对外盘根） |
|---|---|
| Fresh Native同步与动态来源 PASS | `development/native-synchronization-native/manifest.json`：Native/capture/API/CLI完整A[3]=123/B[7]=456及padding；64线程共享数组实际GPU产生索引，wg/simd同步，unknown numerical offset保持来源/展示partial；48事件/EID0和20public access/descriptor/location点。10结构/来源/类型/身份/未资格RT坏组20 API4/CLI1 GPU前拒绝。CPU新同步/原子/条件来源/失效来源测试0；三旧数值oracle未运行并在manifest标明，不算拒绝PASS。实际AIR保存在shader-disassembly.ll。非RT dispatch/非官方sample。 |
| Fresh Native无fragment颜色+深度 PASS | `development/optional-fragment-current-native/manifest.json`：d32 direct、d32 deferred、d32s8 parallel三场景各两capture，总6 Native/capture/API/CLI；独立颜色附件全4像素保持80/64/128/255，深度/模板值、两普通MRT/cross-pass输出/padding、shader身份正确，零fragment public access；各29事件选择含4 EID0，共174选择/24重置。4坏组8 GPU前拒绝：缺颜色、颜色错用深度、缺deferredStore、未执行fragment绑定的非法范围。同目录artifact-inventory.json记录capture/helper SHA。 |
| Fresh converted RayQuery PASS | `development/optional-fragment-RT/manifest.json`：复用已记录真实converter metallib/reflection，新Native/capture/API/CLI4253/4253/0/0、528uint、48事件/EID0/24typed查询，64几何/TLAS、Private placement、六CBV/sampler、2D间接1→132→0。已有显式query闭包，不是UE runtime RT验收。 |
| Related existing captures PASS | `development/optional-fragment-related/development-regression-manifest.json`：29旧capture58 API/CLI，原21texel/multiUAV/B512 TraceRay/frame Load/linked/depth/deferred/parallel，加此前8份6/8 MRT direct/deferred/parallel。是当前固定库的相关集中回归，不冒称完整旧RT/全库。 |
| Actual UE INCOMPLETE | `development/native-synchronization-UE-pre-submit/manifest.json`：API4、无initial/frame GPU/GPUwait、peak RSS1,163,837,440bytes，未超45s/3GiB，无strict诊断。越过draw3330304的旧fragment guard，推进commit3517632；PSO1624/dispatch265216从unknownCall35/unknownBuffer19变为unknownCall0/unknownBuffer4（read12/write1）。实际AIR是已知descriptor来源/null选择后动态GEP的四个读；当前derived literal-null资格拒绝，尚未证明有缺失GPU指针。下一独立处理这些来源和真正地址字段的恢复需求，**不模拟GPU共享数组、prefix/max或输入数值来消除unknown**。shader输出、实际RT消费、UE整帧/事件/EID0/绑定仍未验。 |

保留首次public API `GetDescriptorAccess`签名编译错误与修正、首API95 ghost access失败、中间b417 Native正例/UE同步拒绝的独立哈希范围；不将中间通过或当前UE拒绝改写成PASS。`development/native-synchronization-next-pointer-source.json`记录下一实际机制缺口。未知派生null读的拒绝不是新的永久不支持feature结论；恢复需求要独立核实。

`development/optional-stages-synchronization-evidence.json`保存当前5项manifest、251日志SHA与strict0（仅列明范围），检查点 `checkpoint-optional-stages-synchronization-f9718c71/manifest.json`保存64源码、四产品、九文档及复现脚本。所有产物外盘 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`，内部B544保持symlink，内盘32GiB/外盘1.6TiB，无reset/commit/push、无用户文件覆盖。最终official/full IR/full RT/lifecycle/Qt-ARC/native能力与UE整帧未验，历史Qt/UE崩溃和系统重启未证明修复；两生产能力false，目标active，本大批次未关闭。

## B544 continuation — conditional sources, literal relocation and object metadata (2026-10-07)

同一B544/PHASE59大批次继续，按用户要求独立处理资源恢复、地址重定位、提交依赖与访问展示。参照本地DX12 `Serialise_Dispatch` 的原GPU调度/`GetDescriptorAccess` 的可选动态反馈和Vulkan静态/反馈访问展示，不能把CPU数值不完整等同资源丢失。没有CPU threadgroup/SIMD/prefix算法模拟，也没有引擎、pass、shader名或1024数值白名单。

已恢复buffer/null分支后的GPU相对索引保留资源身份；null本身不成为恢复对象。缺失真实source、opaque指针、typed GPU地址当数值索引的scalar/struct GEP仍拒绝。旧CPU range/loop断言改为检查来源与未知范围，不借数值模拟消除unknown。未知literal算术没有授予通用GPU地址恢复。

固定shader字面量及派生memory byte span独立匹配捕获GPU身份/真实资源范围，包含跨入资源起始位置的访问。真实捕获VA需要重定位；普通bytecode常量保持Native语义。literal alternatives在CPU已知predicate选择后仍保留并沿GEP投影，不能借CPU分支绕过旧VA检查。GPU指针数据没有变成数值启发式或伪造GPU输出。

Native纹理尺寸/level查询读取对象信息，不要求像素初态；preflight帧对象从已验证factory记录取尺寸/格式，Native对象仍在原事件出生。pixel读取仍需初态/producer。Native无参且实际声明readonly/inaccessiblememonly的内部调用没有捕获资源输入，执行原GPU调用，opaque返回仍unknown；外部/写入/带参数或可访问资源调用不按此授予来源，返回handle不能伪装buffer。真实pixel读取负例中的隐式read sampler验证该副作用分类，没有CPU构建sampler或模拟返回。

固定backend/bundle SHA256 `529d0a7b11e5afa462bb83eba0154e4387e999394b95a31e93e2d9cb73ac2183`，GUI `3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91`、provider `2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619`。构建/GPU/UE预检共享锁串行，当前库集中验证：

| 当前实际范围 | 结果/产物（相对外盘根） |
|---|---|
| Fresh Native三种机制集成 PASS | `development/texture-metadata-native/manifest.json`：Private counter→64线程共享数组/原barrier→nullable相对读→条件atomic/连续四写，A[3..6]=123..126、B[7..10]=456..459，全64uint/每buffer和CBV padding、48事件往返/EID0、20公开查询点/40descriptor-location检查。frame-born Private placement R32Uint 8x8无纹理初态/无pixel read，GPU width/height决定真实写入；非RT dispatch/非官方sample。15坏组30 API4/CLI1 GPU前拒绝，含base/derived/spanning捕获VA、缺dimension source/错CBV role、真实pixel读取缺初态；后者明确命中 `Metal runtime texture contents unavailable: resource=ResourceId::44 pixelRead=1`，无GPUwait。三旧数值controls未跑，不算拒绝PASS。实际两份AIR/capture/helper哈希见该目录。 |
| Fresh converted RayQuery PASS | `development/texture-metadata-RT/manifest.json`：复用已记录converter metallib/reflection，新Native/capture/API/CLI4253/4253/0/0、528uint、48事件/EID0/24typed查询，64几何/TLAS、Private placement、六CBV/sampler、2D间接1→132→0；已有显式query闭包，不是UE runtime RT验收。 |
| Fresh Native可选fragment PASS | `development/texture-metadata-fragment-native/manifest.json`：d32 direct/deferred、d32s8 parallel共6 fresh capture，174选择/24 EID0、颜色/深度/模板/MRT/padding与零不存在fragment阶段access；4坏组8 GPU前拒绝。 |
| Related existing captures PASS | `development/texture-metadata-related/development-regression-manifest.json`：29旧capture58 API/CLI，Private/placement texel、multi-UAV、B512 TraceRay、frame Load、linked graphics、depth/deferred/parallel及6/8 MRT；不是全库/full RT/full IR。 |
| Actual UE INCOMPLETE | `development/texture-metadata-UE-pre-submit/manifest.json`：复用实际capture `403f90ea8c23638b8c817701725e31885c7929c09fac169a99e7728a66ac092b`，API4、peak RSS1,419,739,136bytes，未超45s/3GiB，无initial/frame GPU/GPUwait/strict诊断。越过1624 nullable读、1625连续写及3830尺寸消费，commit3517632仍未通过；下一3836/dispatch274240 unknownBuffer0、unknownCall10。实际AIR为SIMD ballot4/broadcast-first2/is-first2和texture-buffer fetch-add2；需Native副作用、texture atomics真实读写/初态/提交依赖，不能模拟lane结果或只删unknownCall，再runtime AS/Header闭包。 |

`development/texture-metadata-records-next-effects.json`保存实际factory/AIR与下一调用统计，是CPU审计，不是UE GPU输出。`development/provenance-metadata-evidence.json`保存5项当前manifest、285完成日志SHA/strict0（仅列明范围）；`checkpoint-provenance-metadata-529d0a7b/manifest.json`保存64源码、四产品、九文档。全部产物外盘 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544`；内盘32GiB/外盘1.6TiB，无reset/commit/push、未覆盖用户改动。

中间CPU旧assert、Native7 oracle/fixture、MSL/API编译、standalone budget、future metadata与未命中pixel guard失败均保留，详见failures_preserved。standalone frame texture constructor未实现，不以placement成功覆盖。最终official/full IR/full RT/lifecycle/Qt-ARC/native能力及UE整帧输出/事件/EID0/绑定未跑；原Qt/UE崩溃和重启未证明修复。两生产flags false，目标active，B544/PHASE59尚未关闭。

## B544 continuation — Native SIMD, texture atomics and readback (2026-10-07)

同一B544/PHASE59大批次继续。资源恢复、地址重定位、提交依赖、shader访问展示独立；动态偏移和Native SIMD/原子数值由真实GPU执行，展示partial不等于恢复失败。没有CPU lane/atomic结果模拟或UE/Lumen/pass/PSO名称/ID支持分支。参照本地DX12 Serialise_Dispatch/Serialise_CopyTextureRegion（d3d12_command_list_wrap.cpp:5289、Native调用5320–5330）及Vulkan Serialise_vkCmdCopyImageToBuffer（vk_draw_funcs.cpp:1884、Native调用1908起）的原生执行、事件和资源角色，访问反馈展示仍独立。

实际声明convergent且无device-pointer的Native SIMD按寄存器效果执行，返回数值保持unknown；非convergent、外部及pointer参数不获来源。pointer phi与select保留全部来源候选，missing/opaque/cyclic来源不猜测，null不成为资源。buffer/texture原子区分store写、load读、RMW读写；load/RMW验证先前像素初态或真实producer和提交关系，store不要求旧像素。帧内Private texture writer通过已验证factory记录保留，不因Native对象尚未出生丢失提交依赖。

通用texture→buffer预检核验源初态或同提交/已提交producer、对象生命周期/alias、buffer范围、slice/mip/region/row/image pitch及16MiB聚合copy预算。Native与帧factory共用布局/overflow判断，真实transfer不变，不创建对象/像素或CPU shader输出；写回清除CPU旧字节/alias事实。已有BC1/BC5和普通颜色范围保持，depth/其它packed/compressed范围未扩大。更广buffer-backed texture producer/alias和同dispatch初始化尚未完整证明。

固定backend/bundle SHA256 8d6ec636db55ff213f58f774d118d70672627472a62c8634aeab89ee06d380a0，GUI 3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91，provider 2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619。构建0、CPU0、diff-check0。构建/GPU/UE共享锁串行；UE只45秒/3GiB预检，无initial uploads/frame GPU。

| 当前固定库范围 | 实际结果/产物（development下） |
|---|---|
| Fresh Native同/跨提交链 PASS | native-lanes-atomic-complete、native-lanes-atomic-separate-complete各manifest：Native/capture/API/CLI，frame-born Private placement R32Uint 8x8无像素初态，atomic_store产生0x12345+i，atomic_load驱动nullable相对buffer读，atomic_fetch_add最终0x12346+i；真实SIMD ballot/broadcast-first/is-first和threadgroup/simdgroup barrier、buffer atomic exchange/四连续写A[3..6]=123..126/B[7..10]=456..459。全64uint/CBV/counter padding、64纹理像素、texture→buffer offset32/row256/2112-byte全部padding。各72事件往返/EID0、20公共点/40descriptor-location。各19坏组、共76 API4/CLI1 GPU前拒绝，含缺producer/readonly atomic/readback越界/短row/缺submit、base/derived/spanning捕获VA。缺producer实际命中pixel contents guard，无GPUwait；非RT dispatch/非官方sample。 |
| Fresh metadata-only PASS | native-lanes-metadata-regression/manifest.json：width/height无像素初态仍Native/capture/API/CLI通过，48事件/EID0/20公共点/40descriptor-location；15坏组30 GPU前拒绝，真实pixel读缺初态仍命中contents guard。三旧数值controls未跑并显式记录，不计拒绝PASS。 |
| Fresh converted RayQuery PASS | native-lanes-atomic-RT/manifest.json：复用记录的converter metallib/reflection，新Native/capture/API/CLI4253/4253/0/0、528uint、48事件/EID0/24typed查询，64几何/TLAS/Private placement、六CBV/sampler、间接1→132→0；已有显式query闭包，不是UE runtime RT验收或新官方下载。 |
| Fresh可选fragment PASS | native-lanes-atomic-fragment-native/manifest.json：d32 direct/deferred、d32s8 parallel共6 fresh，174选择/24 EID0、颜色/深度/模板/MRT/padding、零不存在fragment access；4坏组8 GPU前拒绝。 |
| Related captures PASS | native-lanes-atomic-related/development-regression-manifest.json：29旧capture58 API/CLI，Private/placement texel、multiUAV、B512 TraceRay、frame Load、linked graphics、depth/deferred/parallel、6/8 MRT；不是全库/full RT/full IR。 |
| Actual UE INCOMPLETE | native-lanes-atomic-UE-pre-submit/manifest.json：actual capture403f90ea、API4、peak1,275,609,088bytes，45s/3GiB未触发，无initial/frame GPU/GPUwait/strict。越过3836/274240 SIMD和texture-buffer atomics，unknownBuffer/unknownCall0、accessDisplay partial；下一3853/277440是buffer-texture get_width+write，unknownBuffer0/unknownCall0，commit3517632仍拒绝。核实对象metadata/format/写角色/alias/lifetime，唯一拒绝分支尚待诊断，不能继续CPU数值模拟。 |

三fresh普通链合计192事件/EID0、60公共点/120descriptor-location，相关57坏组114 API4/CLI1 GPU前拒绝。native-lanes-atomic-artifact-inventory.json保存两capture/真实AIR/helper哈希。native-lanes-atomic-next-view-audit.json是CPU审计：owner提交前publication将4311/offset24064旧initial[2992368640,64,0,0]改为[1306,2160,4,16]；heap25 slot31344→14847，factory MTLBuffer::newTextureWithDescriptor。ID仅定位，不成为实现规则；不能用编码时旧initial证明提交时GPU输入，不是UE输出PASS。

native-lanes-atomic-evidence.json汇总7manifest、565scoped完成日志SHA/strict0，仅列明范围；中间CPU/MSL/API/layout/negative schema失败保留（failures_preserved），中间positive不计完整组PASS。checkpoint-native-lanes-atomic-8d6ec636/manifest.json保存65源码/四产品/九文档与复现脚本。产物外盘 /Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544，内部B544 symlink；内盘31GiB、外盘1.6TiB。仅清理精确可重建cache，不删原capture/故障、主构建、用户工程；无reset/commit/push/RDHeaderView覆盖。

下一继续通用frame buffer-texture view恢复、初态/producer/alias、runtime AS/Header/namespace和必要GPU地址恢复，再UE真正RT整帧/输出/事件/EID0/绑定及最终官方/full IR/full RT/lifecycle/Qt-ARC/native能力集中验收。最终项目未完成，standalone frame texture constructor仍不完整；两supportsRaytracing生产flagsfalse，历史Qt/UE崩溃/系统重启未证明修复。目标active，B544/PHASE59未关闭，持续任务四层准则仍有效。

## B544 continuation — buffer-texture views and current copy state (2026-10-07)

同一B544/PHASE59大批次继续，用户四层准则持续有效。参照仓库 Vulkan vk_resource_funcs.cpp::Serialise_vkCreateBufferView（2387起）与 DX12 d3d12_device_wrap.cpp::CreateUnorderedAccessView（1517起）的 Native 对象/格式/资源+offset描述，恢复 buffer-texture factory 元数据和真实视图范围；不是 UE/Lumen/shader/PSO名称或ID适配。Native动态数值保持GPU执行，partial访问展示不等于资源未恢复。

帧内MTLBuffer::newTextureWithDescriptor通过已有factory/格式/layout/资源出生/生命周期资格后，preflight可读其width/height/format而不提前创建Native对象。texture-buffer写与CPU地址输入只核验逻辑texel字节范围及物理heap alias；同一backing的根参数与像素范围不重叠可以正常重放，实际重叠仍拒绝。Native已有视图与帧factory共用逻辑范围判断。

当前scalar字节有效性按提交顺序维护：真实opaque writer清除dense/sparse事实，并阻止旧初态重新seed；后续已验证CPU上传或Native copy可重新建立当前精确字节。历史opaque标记不再永久排除后续正确拷贝，copy源也使用当前有效字节而不回退旧初态。未知GPU字节仍不变成CPU数值或捕获地址来源。独立restoration intervals记录完整初态、当前CPU上传与copy链的内容覆盖，不依赖CPU是否知道像素数值；copy先清除目标被替换范围再传播源覆盖，缺初始化的源不继承目标旧像素。仅有16-byte根参数上传不能证明offset256/256-byte texel像素已恢复。更广GPU buffer producer与动态heap namespace仍需补真实恢复，不能将本次保守拒绝永久作为纯展示失败的规则。

固定backend/bundle SHA256 4163d391ea5486af9f3d7e9ff9f8bb4a46b8c498662b07c8fd7be2823bb1594f，GUI 3ba30e36278fa0934013114f38ec2b974b2b1765b9286dcc9372764c32019a91，provider 2496f103cf253a44ce93b22177076ffa6701a0be0193d3dca7787b7b46f53619。构建0、CPU0、diff-check0。构建/GPU共享锁串行，UE只45秒/3GiB无initial/frameGPU预检。

| 固定当前库检查 | 实际证据（development下） |
|---|---|
| Two fresh view groups PASS | frame-texel-view-final-bounded、frame-texel-view-final-shared-root各manifest，Native/capture/API/CLI真实编译MSL，Private placement 1024-byte backing/R32Uint 64 texels、offset256/row256。两次完整64像素0x12346+i→0x12347+i和1024-byte padding/根参数，Native SIMD/条件来源及A/B四连续写保持GPU执行；各48事件往返/EID0、20 public点/40descriptor-location。Shared-root根16..32与像素256..512，真实逐thread先read(local)再write(local)，不制造CPU shader结果；移除1024-byte像素上传而保留16-byte根上传，实际命中pixel contents guard，无GPUwait。各19/20坏组，共78 API4/CLI1 GPU前拒绝，含readonly/格式/offset/alias/缺source、真实根与view重叠、捕获VA base/derived/spanning。frame view backing本身未直接在EID0查询，既有输出/根/counter重置已验证，不扩大该scope。 |
| Two fresh atomics groups PASS | frame-texel-view-final-atomic-complete、frame-texel-view-final-atomic-submit，原同/跨提交Native store→load/RMW、8x8纹理、SIMD及2112-byte texture→buffer像素/padding各72事件；各19坏组/共76 GPU前拒绝。与两view组共240事件/EID0、80公共点/160descriptor-location和77坏组154拒绝。 |
| Unknown writer control, not final supported scope | frame-texel-view-final-opaque/manifest.json：Native/capture正确，未知shader writer→copy后消费仍API4/CLI1 GPU前拒绝，未回退旧scalar初态。动态heap selector/指针namespace资格后续需独立补恢复，不能将CPU数值未知本身永久定为拒绝条件，也不计该控制为功能已支持。 |
| Fresh actual RayQuery PASS | frame-texel-view-final-RT，复用已记录converted shader/reflection，新Native/capture/API/CLI4253/4253/0/0、528uint、48事件/EID0/24typed查询、64几何/TLAS/六CBV/sampler/Private placement/间接1→132→0；不是新下载官方sample或UE runtime RT验收。 |
| Related capture regression PASS | frame-texel-view-final-related/development-regression-manifest.json，29旧capture/58 API/CLI，Private/placement texel、multiUAV、B512 TraceRay、frame Load、linked/depth/deferred/parallel、6/8 MRT。不是full IR/full RT/full库验收；旧8d库fresh可选fragment不继承为本库fresh结果。 |
| Actual UE remains INCOMPLETE | frame-texel-view-final-UE-pre-submit，actual403f90ea/API4，peak1,421,934,592bytes，45s/3GiB未触发，无initial/frameGPU/GPUwait/strict。已越过3853/277440 frame buffer-texture metadata/write资格，下一3854/278528，commit3517632仍拒绝；多个已有background GPU descriptor被当frame producer缺来源，另新Private14851/14852输入数值未知。下一先区分已验证/重定位的background GPU初态与frame实际producer/address namespace，不能增加CPU shader数值表达式来替代。 |

frame-texel-view-next-descriptor-audit.json是CPU provenance审计：heap25/slot31320最后background event3与捕获初态24bytes逐字节一致，typed source14156；初态校验/overlay已有PatchDescriptorSlot。ID仅定位证据，不成为支持分支；未证明frame GPU重放。需要核实当前slot身份/生命周期/namespace和Private producer链，不能将gpuExpected历史标记与缺真实GPU指针混为一谈。

frame-texel-view-evidence.json汇总8manifest、695scoped完成日志SHA/strict0，保留早期Native/MSL/API/oracle失败。新unknown-index从捕获实际bindings选择真正缺slot，不因新增合法buffer/texture槽而人为扩大拒绝；三旧数值控制仍未跑并显式记录，不计PASS。frame-texel-view-final-grouped-manifest.json保留首次atomic测试oracle失败；corrected continuation和两完整atomic目录才计whole-group PASS。ea10中间range组和早期005139e6/6dcd views各保留自己的库哈希，不计当前最终验收。

checkpoint-frame-texel-view-4163d391/manifest.json保存65源码/四产品/九文档与复现脚本，证据及产物全外盘 /Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524/build-macos-debug/metal-ray-b544；内盘31GiB、外盘1.6TiB，无reset/commit/push、未覆盖RDHeaderView用户改动。B544未关闭、目标active，两生产supportsRaytracing仍false。UE真正RT整帧输出、事件/EID0/绑定及最终官方/full IR/full RT/lifecycle/Qt-ARC/native capability集中验收尚未完成；standalone frame texture和更广GPU namespace仍未完整支持，历史崩溃/重启未证明修复。

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


2026-10-07 [B544 零字段/稀疏 namespace 与统一复制预算整改](GENERIC_API_RECOVERY.md)：backend/bundle `c52ba022`。物理buffer零字段按真实initial/CPU publication/匹配full24 GPU copy保存消费点证明，retired/纹理/未发布零行不虚构buffer依赖；opaque/alias writer与CPU提交更新使旧事实失效，非零缺来源/初态/producer仍拒绝。GPU全零descriptor的空引用图不再误判无producer。ordinary/typed/线性copy场景计数改已有整帧预算，保留单次1MiB真实恢复边界、不扩大上限。独立Native的3/17/257稀疏行、R32Uint2×3→RGBA8Unorm7×5、动态实例/偏移及GPU背景+帧重发布通过；fresh部分Private/可变/texel、converted RayQuery与29旧capture58API/CLI通过，111坏组222GPU前拒绝，缺冗余raw initial但完整typed publication合法控制通过；未声明地址writer仍恢复拒绝。UE无GPU预检越过退休零字段，下一非零placement候选14714缺内容/producer证明，仍API4，peak1357774848bytes，无wait/strict，不能假装候选被实际访问。下一physical backing/定义区间与动态候选恢复资格，非CPU shader数值扩许。完整UE/最终官方full与能力验收未跑，两flagsfalse、目标active；新产物全外盘、失败保留、无提交/推送/重置。

逐函数 DX12/Vulkan 对照、实际机制/边界、当前库验证与保留失败见 [GENERIC_API_RECOVERY.md](GENERIC_API_RECOVERY.md) 的“物理零地址字段、GPU 发布与统一复制预算”节。继续同一较大能力批次；UE预期拒绝不算验收通过。


2026-10-07 [B544 Native GPU定义区间与通用dispatch绑定](GENERIC_API_RECOVERY.md)：固定backend/bundle `af26d32b`。真实在前Native GPU store的定义区间与CPU数值分开；未知clz值仍Unknown，精确非零compute写按已恢复地址/范围/提交发布4byte区间、统一预算，不造padding/CPU值。conditional/ranged/atomic/raster/未知pointer不借此保证初始化。direct/indirect sourced dispatch以实际闭包和PSO反射绑定校验取代slot0/2D-copy形状限制。两fresh真query变体cross/slot2/offset64/1KiB→same/slot7/offset65532/64KiB、实例2/各56事件/真实GPUword完整输出通过，未来producer与缺producer仍GPU前拒绝；fresh旧恢复相关路径、converted RayQuery及29旧capture58API/CLI通过，128坏组256拒绝。UE禁GPU预检仍API4在非零候选14714内容/producer未证明处，peak1364377600bytes、无wait/strict；没证据实际访问该候选，不能伪造producer。下一动态候选/physical backing/定义区间恢复义务边界，非扩CPUshader数值。完整UE及最终集中门槛未验、两flagsfalse、目标active；新产物外盘，失败/当前产品源码文档哈希保留，不提交推送重置。

具体前置DX12/Vulkan逐函数对照、生产机制/边界、PASS/FAIL/未跑和固定产物见GENERIC_API_RECOVERY.md最新“Native GPU写入定义区间与通用dispatch绑定”节。继续同一通用能力大批次，UE预期拒绝不算整帧通过。

2026-10-07 [B544 typed namespace 行窗口与真实读取宽度](GENERIC_API_RECOVERY.md)：固定backend/bundle `fb456ca1`。typed地址保存真实API invocation界及ABI行窗口，动态读取不再被不可达冷行内容阻断；全部物理字段仍重定位，窗口内来源/initial/producer/生命周期/提交校验，范围未知仍恢复完整namespace，候选展示partial。缓存区分窗口，只复用已验证读取宽度，不把allocation容量当内容证明；Native volatile内存效果保留，不计算shader数值。三fresh真query变体1/17/257冷行、1KiB→64KiB/offset64→65532、Header/贡献/输出偏移、cross upload→same-submit slot7 GPU producer完整输出/各56事件通过；放宽调度触及冷行、越声明界、2byte上传不能证明4byte读取及缺/未来producer均GPU前拒绝。fresh旧范围相关恢复、converted RayQuery、29旧capture58API/CLI通过，115坏组230拒绝。UE禁GPU预检仍API4于非零候选14714内容/producer，peak1420476416bytes，无wait/strict；不是实际访问证据、不是UE整帧PASS。下一按真实物理backing/输入恢复义务补机制，不扩CPUshader许可或跳过缺行；完整UE/官方/full/最终启用未验，两flagsfalse、目标active。当前证据/失败/源码与产品检查点全外盘，无提交推送重置。


2026-10-07 [B544 placement buffer 物理字节恢复](GENERIC_API_RECOVERY.md)：固定 backend/bundle `c573bda0`。按真实 Native heap/offset/逻辑范围共享已恢复字节，重新创建的合法 buffer alias 不再丢失前一对象的 upload/GPU 定义区间；unknown-source copy 覆盖使物理证明失效，旧初态先统一播种，禁止后来 alias 复活旧内容。CPU 数值和 typed 地址来源仍独立，纹理/AS opaque footprint 不证明 buffer 内容，未来 producer 不能借给早期消费。三 fresh alias query 变体改变 size/heap offset/读取偏移/上传→Native GPU store/绑定槽，完整输出和事件往返通过；fresh 窗口 GPU、retired/部分 texel、converted RayQuery 与29旧 capture 的58 API/CLI通过，121坏组242 GPU前拒绝。UE 同捕获禁GPU预检仍 API4 于候选14714的内容/producer缺口，peak1296728064bytes、无GPUwait/strict，不算整帧PASS。实际Native纹理footprint审计没有重叠，下一补真实 creation input/生产者恢复机制，不跳过候选或伪造零初态。完整 UE/最终官方/full/设备启用未验，两RT flags false、目标 active。产物和失败证据均外盘，无提交推送重置。


2026-10-08 [B544 API 创建初态通用恢复](GENERIC_API_RECOVERY.md)：backend/bundle `568e50eb`。真实 MTLDevice::newBuffer(length) 的零初始化契约、newBuffer(bytes) 的完整原输入在实际出生点发布恢复区间，seek 仍执行原 Native factory；不要求多余 upload，不应用于背景帧初态或 heap buffer，不制造 CPU shader 数值。bytes factory 缺/短 payload 明确拒绝，不能退化为 length factory。三 fresh Private零/Shared零/Shared bytes sample 改变16KiB→64KiB、offset1024→65532、Header/贡献/动态query/输出偏移，完整创建字节、输出与事件/EID0通过；改为实际 heap 输入的 alias/窗口/producer负例与相关恢复、fresh converted RayQuery、29旧capture58 API/CLI通过。九 fresh runtime464事件，167坏组334 GPU前拒绝。修正旧 partial/cold fixture 把设备新建 buffer 当 undefined 的错误语义；旧拒绝记录保留但不继续作为合法 API 的拒绝依据。UE14714仍是heap factory，CPU审计证明该契约不适用；本候选未重跑无变化UE，不继承旧库UE结果，整帧仍未完成。继续真实heap输入恢复，全部最终RT门槛未完成、flags false，较大批次 active；产物/失败/检查点外盘，无提交推送重置。
