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

2026-10-07 [B544 实际 sampler 消费与公共查询集成](BATCH544_API_RT_INTEGRATION.md)：继续同一大批次，参考DX12/VK sampler descriptor access/query，用实际AIR sample/gather调用、typed根指针及当前sampler表来源核验消费；公共GetDescriptorAccess/GetDescriptors/GetSamplerDescriptors返回当前sampler身份/参数，支持非零表偏移和EID0清空。当前backend/bundle `aca98095`、GUI `3ba30e36`、provider `2496f103`：两已有45c2真实capture当前扩展API/CLI PASS，各72 sampler参数/身份查询、48事件/EID0、144纹理身份及48 texture/AS公共查询、五纹理528texels/全部CBV padding/六sampler重定位；三执行点均解析6 sampler且无unknown。41坏组82无GPUwait拒绝+3合法控制，10旧capture20 API/CLI通过；真实编译七sampler程序在六项表之外的消费明确报sampler7/unknown1并拒绝，没有运行该坏程序的native调度。318完成日志无严格诊断、28源码/四产品与复现输入检查点外盘。中间工具SD生命周期错误及纯CBV第二调度误拒绝已修复复验并保留FAIL；未证明历史Qt/UE崩溃/重启修复。B544未关闭，当前无fresh Native/capture、官方/full/lifecycle/Qt-ARC/UE整帧GPU验收，不继承旧库PASS，两flagsfalse；下一完整buffer/其他AIR依赖及真实runtime PSO闭包→固定库集中验收→UE整帧。内盘约32GiB，持续active，无提交推送/重置。

2026-10-07 [B544 执行点 uniform 值与实际 AIR 消费集成](BATCH544_API_RT_INTEGRATION.md)：同一大批次继续开发，按提交/dispatch fileOffset 保存已证明 CBV 字节及当前 typed heap/Header 快照，用真实 AIR 核验 AS、SRV/UAV 角色和纹理数值类型；Private 帧内 CBV 支持既有预算内的 2MiB backing/offset58624，未知 GPU writer 仍拒绝。当前 backend/bundle `45c2a584`、GUI `3ba30e36`、provider `2496f103`：两 fresh Native/capture/API/CLI PASS（大 Private CBV+Private placement 纹理、小 Private placement CBV+Shared 纹理）；每项48事件/EID0、144身份、48公共descriptor/AS检查、五纹理各528texels、全部CBV padding/六sampler。35坏组70无GPUwait拒绝+3合法控制，10旧capture20 API/CLI通过；真实消费值追踪分别54/48 scalar读取，三执行点均解析1 AS/8 texture调用且无unknown，两compiler/AIR审计及CPU numeric测试通过。316完成日志无严格诊断，28源码/四产品检查点及所有产物外盘，内盘约32GiB。B544未关闭；实际UE自动PSO消费完整依赖/动态Header及整帧GPU输出/事件/EID0/预算未验，当前库官方/full/lifecycle/Qt-ARC未跑，两flagsfalse。保留测试夹具/日志留存与早期构建失败，不声称历史崩溃/重启修复；无提交推送/重置，持续active。

2026-10-07 [B544 通用 AS descriptor 与运行时绑定集成](BATCH544_API_RT_INTEGRATION.md)：同一大批次继续开发，参照DX12当前root table/AS descriptor和VK执行点参数/usage；AIR uniform解析支持匿名AS Header及实际AS调用，公共GetDescriptorAccess/GetDescriptors返回所绑定AS而非Header buffer。严格解析runtime binding ABI，核验根布局/绑定点/线程组与实际dispatch；runtime事实不能声明shader访问或开启消费资格，setBuffer替代setBytes会清空旧inline数据。当前backend/bundle `8caaecc0`、GUI `3ba30e36`、provider `2496f103`：两已有复合capture当前API/CLI PASS，每项48事件/EID0、144身份、48公共descriptor检查（含12 AS），五纹理全部528texels/CBV padding/六sampler；10 ABI坏组20无GPUwait拒绝+合法控制，10旧capture20 API/CLI PASS。中间71c9库两fresh Native/capture/API/CLI、21坏组42拒绝/两控制/旧20检查分别保留其hash，不冒充当前库fresh capture或最终验收。CPU解析119实际UE runtime payload+1 compiler payload及六真实query AIR形状通过；AIR使用合成loader，仅证明解析覆盖8 AS reset/93纹理调用，不证明真实资源依赖/UE输出。当前110完成日志无严格诊断，27源码/四产品校验备份及产物全外盘，内盘约32GiB。B544仍开发中，实际UE动态AS Header/自动PSO-heap消费闭包和整帧GPU/事件/EID0/预算未验，未重复已知失败UE；当前库官方/full/lifecycle/Qt-ARC未跑，两flagsfalse，历史崩溃/重启修复未证明。无提交推送/重置，持续active。

2026-10-07 [B544 通用多 UAV 与执行点依赖集成](BATCH544_API_RT_INTEGRATION.md)：继续同一大批次，PSO输出角色与资源身份解耦，多个texture UAV按当前heap slot解析；已验证槽位换绑、query纹理GPU输出→后续SRV消费的提交依赖及零调度。未证明的GPU buffer writer保持opaque，不能把旧初态用作后续消费证明。实现参照VK/DX12，无UE/Lumen/shader名分支。当前开发backend/bundle `105986c1`、GUI `3ba30e36`、provider `2496f103`；同一当前库两fresh Native/capture/API/CLI PASS：Private placement/五帧内2MiB CBV与Shared UAV/四帧内Private CBV，每项五纹理各528texels、48事件选择/EID0、144身份检查、36公共descriptor查询、完整CBV/padding/六sampler身份及只读usage，结果4248→4560和2375→2687。12坏组24无GPUwait拒绝+合法副本、10旧capture20 API/CLI定向检查通过，两真实compiler/AIR证明各两非零query；158完成日志无严格诊断。新19源码/四产品检查点及全部产物外盘，内盘约32GiB。B544未关闭，实际UE的通用动态AS Header/heap消费闭包及整帧GPU输出/定位/EID0/预算仍未验，本轮未重复启动已知失败UE；当前库官方/full/lifecycle/Qt-ARC未跑，两生产flagsfalse，历史崩溃/重启修复未证明。无提交推送/重置，持续active。

2026-10-07 [B544 通用纹理与 PSO 绑定 ABI 集成开发](BATCH544_API_RT_INTEGRATION.md)：同一大批次补齐类型化 texture SRV/UAV、Private/Tracked placement、原始创建 usage 核验及原生小堆范围；新增不可变 PSO 反射/运行时 ABI 捕获，保持 compiler JSON 与 runtime binding facts 的来源区别。当前开发 backend/bundle `96cf2591`、GUI `3ba30e36`、provider `2496f103`：新复合 Native/capture/API/CLI PASS（528 texels、56事件/EID0、48-byte帧内根/二维调度）；16坏组32无GPUwait拒绝+合法控制，10旧capture20 API/CLI定向回归PASS。实际新UE `403f90ea`/75,234,193bytes：119 PSO ABI/AIR核验、6光追PSO/5非零调度、40–72-byte根及原生线程组/绑定payload长度逐次一致，未依赖Lumen分组筛选；旧Lumen范围60 AIR/两非零query也通过。UE CPU65仍因动态AS Header/heap消费契约拒绝、无GPUwait/初态上传/frameGPU；UE输出/事件/EID0及总预算未验。B544未关闭，下一合并当前执行点动态/多输出和GPU生产依赖闭包，再固定库集中相关回归与短UE完整验收。两生产flagsfalse，无本库官方/full/lifecycle/Qt-ARC验收，不继承旧库；未证明历史Qt/UE崩溃或重启已修复。新产物继续外盘；追加34个frozen-validation目录1.18GiB校验迁移，保留路径链接（此前219目录17.3GiB迁移仍有效），未提交/推送/重置，持续active。

2026-10-07 [B544 通用 API 集成开发检查点](BATCH544_API_RT_INTEGRATION.md)：同一大批次继续补齐帧内 CBV 创建/CPU 与已知 Shared→Private blit 发布/提交顺序、2MiB backing 内的 CBV 子范围，以及 Private standalone/Tracked placement UAV 输出。实现参照 VK/DX12 的资源+offset、初态与提交依赖，不按 Lumen/shader/pass 特判。当前开发 backend/bundle `466a6485`、GUI `3ba30e36`：两新 Private 输出场景各 Native/capture/API/CLI PASS（528uint、56事件/EID0、二维[2,66,1]与零调度），其中一项组合五个帧内2MiB WriteCombined CBV、offset58624；四输出坏组八无GPUwait拒绝+合法对照，七旧capture十四API/CLI定向检查PASS。早期86f帧内三场景/34坏组与479d大初态/c9aa大帧内检查分别保留自己的库哈希，不计当前库最终验收。B544未关闭；texture UAV、多输出/更广GPU生产及真实PSO元数据闭包尚需继续，随后固定库集中相关回归→短UE真实光追截帧/输出/事件/预算。当前库未跑官方/full/lifecycle/UE/Qt-ARC，两生产flags仍false；此前用户崩溃/重启修复未证明。产物和检查点直接写外盘，219目录约17.3GiB迁移/58缓存清理保持有效，内盘约32GiB可用；本轮删三个过时patch脚本，保留失败证据。无提交推送/重置，持续active。

2026-10-07 [B544通用API集成继续开发](BATCH544_API_RT_INTEGRATION.md)：保持大批次合并/开发定向验证/固定最终库集中验收，Lumen仅作实际应用验收，接口与初态/重放继续参照VK/DX12。新增共用IR compute混合根类型与texture SRV依赖，static sampler逐slot来源/身份重定位，Private（含Tracked placement）初态CBV范围读取；不按shader/pass/效果特判。当前开发backend/bundle e8f709bf、GUI3ba30e36：40-byte四Private CBV+六sampler、48-byte五Private placement CBV+六sampler/二维[2,66,1]/64几何实例/unretained，两fresh Native/capture/API/CLI PASS（528uint各、48/56事件及EID0），六真实texture sample AIR调用/所有sampler ID变化/完整padding与read-only usage；27坏组54无GPUwait拒绝+合法对照、五旧capture API/CLI十检查PASS。mixed5早期初态拒绝/验收工具悬空SD引用崩溃已保留并修正，不等同此前UE/Qt崩溃修复。B544未关闭、未集中验收；下一帧内CBV producer-consumer与texture/Private输出通用闭包，再一次集中相关回归→短UE实际光追截帧/输出/事件/预算。两flagsfalse，无新UE或本库官方/full/lifecycle/Qt-ARC验收，不继承旧库。219目录17.3GiB外盘校验迁移与58 cache清理已完成，原路径链接、活动B544产物继续外盘；内盘约32GiB可用。未提交推送/重置，持续active。

2026-10-07 [PHASE59](PHASE59.md) / [B543](BATCH543_TYPED_HEAP_QUERY_INDIRECT.md)：参照VK FetchIndirectData/DX12 SaveExecuteIndirectParameters，补typed heap-query间接调度，逐encoder/ordinal/epoch核验原生GPU参数，保留原间接调用、零工作量与预算/闭包；修正typed只读资源误标CS_RW。最终backend/bundle2d7439ae、GUI3ba30e36、provideraba8b52b；八新间接query416事件（同encoder0/1/2、GPU1/132/0、末端offset/unretained/64几何/空TLAS）+14旧direct448事件+4旧heap64事件均Native/capture/API/CLI PASS。新72坏组144无GPUwait拒绝、6执行值不符组12次GPU核对后拒绝、2合法；普通计算间接八新capture56reset/seek、176坏组352无GPUwait/2执行不符组4拒绝PASS。56回归含官方两scene270事件10query/六能力、fresh四Sharedcompact18检查、20controller lifecycle（growth3063808）PASS，1798完成日志无严格诊断。复用B542实际UE9359c9e2，当前库normal/CPU65/pre-submit65仍API4无GPUwait/初态上传/frameGPU，UE完整输出与预算未验；40/48-byte CBV-root/二维调度/GPU producer-consumer仍缺。中间失败与502a结果保留不计最终PASS；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两flagsfalse、未提交推送。下一B544 typed CBV query闭包→实际Lumen，再短UE重放；持续active。

2026-10-07 [PHASE59](PHASE59.md) / [B542](BATCH542_PRIVATE_PLACEMENT_GEOMETRY.md)：补Private/Tracked placement BLAS顶点/索引encoder-end冻结、kind1/2/8初态及帧内重建，复用VK/DX12 typed build-input路径与原预算/退休/范围/compact容量核验。backend/bundlea138d70a、GUI3ba30e36、provideraba8b52b；14新query448事件+4旧heap64事件、714坏组1428无GPUwait拒绝/26合法、56旧回归（含官方两scene270事件10query/六能力）、fresh四Sharedcompact18检查与20controller lifecycle PASS（growth11190272），4994完成日志无严格诊断。新UE9359c9e2/69862532bytes，主TLAS216bytes+两compact kind2 child6859/6863配方齐全（648/288与48/12bytes），另两空TLAS；60AIR全审查/两非零HW query[2,66,1]/[132,1,1]，实际root2为40/48byte CBV表。normal身份guard、CPU65/pre-submit65动态heap-query闭包均API4无GPUwait/初态上传/frameGPU；64heap1491815424/初态1127834757预算未验。保留fixture/审计脚本失败；UE输出/事件/EID0、full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两flagsfalse、无提交推送。下一B543实际Lumen PSO/CBV-root/间接query和GPU producer-consumer声明，再UE整帧；持续active。

2026-10-07 [PHASE59](PHASE59.md) / [B541](BATCH541_PRIVATE_COMPACT_SIZE_AND_UE_AS_INPUT.md)：定位UE空packet真正前置缺口为Private compact-size读取/独立query submission及native compact被跳过；补encoder-end4/8-byte冻结、query build来源、保留native转发且未知replay拒绝。最终backend/bundlef6a8029d、GUI3ba30e36、provider复用aba8b52b。六新placement/compact/空frame sample192事件，四旧heap64事件，168坏组336无GPUwait拒绝/六合法、56回归检查（含官方两scene270事件10query/六能力）、fresh四Sharedcompact18检查及20controller lifecycle PASS（growth3506176），1436完成日志无严格诊断。新UE716106d8，主TLAS216bytes+known两child7097/7101（GPU67/71），两个空TLASbytes0；60AIR全部审查/两非零HW query[2,7,1]/[132,1,1]，nativecompact错误消失，但两BLAS初态recipe缺失。normal/CPU65/pre-submit65仍动态heap-query闭包API4提交前拒绝，UE输出/事件/EID0/frameGPU未验；61heap1419217920/初态1110926467不宣称总预算通过。保留中间失败；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两flagsfalse、无提交推送。下一B542实际堆内BLAS几何/索引冻结与compact子配方，再Lumen PSO/root/indirect/GPU producer；持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B540](BATCH540_FRAME_HEADER_HEAP_QUERY.md)：补coverage65新Shared64-byte Header创建/发布、CPU kind3槽位与当前Private typed TLAS build/query提交顺序及EID0；复用VK/DX12已知对象/冻结输入，未放宽未声明namespace。最终backend/bundlebfeca02b、GUI3ba30e36、provider复用aba8b52b。四新sample native/capture/API/CLI、128事件；四旧heap-query/64事件、六旧动态场景七capture/192事件，422坏组844无GPUwait拒绝/七合法控制、47旧回归（官方两scene270事件10query/六能力）及20 controller生命周期PASS（growth458752bytes），2985日志无严格诊断。复用B538UE c41abb06，最终normal/CPU65/pre-submit65仍提交前拒绝，无新UE/frameGPU/输出验收。新提取实际三TLAS输入options544 Private placement589824bytes、scratch1376256，构建snapshot皆空/children0；不能用standalone sample通过覆盖UE。保留首build/空initialshadow/gate单query计数失败与完整修复复验；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两生产能力false、未提交推送。下一B541实际Private placement每build快照/known-child闭包、GPU producer与sample→新UE，再Lumen PSO/root/indirect契约；持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B539](BATCH539_TYPED_HEAP_QUERY_CONSUMERS.md)：完成coverage65初态AS/CPU kind3槽位/typed query PSO重放，双字段Header及descriptor VA重定位、依赖与EID0；限制frame AS build、GPU Header/贡献改写、未声明消费者，未全局放宽。最终backend/bundle1e13bd74、GUI3ba30e36、provider复用aba8b52b。四新sample native/capture/API/CLI、64事件；六旧动态场景七新capture/192事件，347坏组694无GPUwait拒绝/七合法控制、47旧回归（官方两scene270事件10query/六能力）及20 controller生命周期PASS（growth442368bytes），3762日志无严格诊断。复用B538UE c41abb06，仅最终库normal/CPU65/pre-submit65提交前拒绝frame新Header，无初态上传/frameGPU；无新UE capture/UE输出事件EID0验收。保留helper编译/空TLAS反例/遗漏oracle失败与完整复验；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两生产能力false、未提交推送。下一B540 Header帧内创建/发布+当前AS build/CPU slot消费时序，再实际Lumen PSO/root/indirect/GPU producer与UE验收；持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B538](BATCH538_DYNAMIC_AS_HEADER_SNAPSHOTS.md)：补 StartCapture 最新 header 初态冻结、coverage3 同 Header 帧内显式版本/双字段重定位与提交顺序、Shared placement header和不可变 Private 贡献源，EID0恢复版本/raw。最终backend/bundle b71d6e4a、GUI3ba30e36、复用provider aba8b52b。七场景八capture native/capture/API/CLI、208事件选择/EID0、12 annotation拒绝、195坏组390无GPUwait拒绝/三合法控制、47旧回归（官方两scene270事件10查询/六能力）及20 controller生命周期PASS（growth425984bytes），1469日志无严格诊断。新UE c41abb06/66,705,869bytes、3TLAS/93direct+56indirect/60AIR全审查/两非零HW query；6header身份匹配、三背景snapshot与初态逐字节一致、26kind3记录，三frame header确为帧内新建，owned cleanup -9。65heap1,488,096,256/初态1,122,325,471，未宣称总预算通过；normal/CPU65/pre-submit65仍提交前拒绝UE新header/query闭包，无初态上传/frameGPU，UE输出/事件/EID0未验。保留build/fixture/oracle失败及修复复验；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两能力false、未提交推送。下一B539帧内新header、kind3当前slot/query consumer/依赖闭包，再实际UE验收；持续任务未完成、继续active。

2026-10-06 [PHASE59](PHASE59.md) / [B537](BATCH537_TYPED_AS_HEADER_PROVENANCE.md)：新增显式64-byte AS header对象/贡献来源与静态coverage3双字段重定位，UE factory kind3及实际header write捕获；Shared placement只授予捕获事实。最终backend/bundle2b363cb3、GUI3ba30e36、provider aba8b52b/98exports。八native/capture/API/CLI、152事件/EID0、12 annotation拒绝/同值重复接受、343坏组686无GPUwait拒绝/五合法控制、47旧回归（官方两scene270事件10查询/六能力）通过，2378日志无严格诊断。新UE b35de06d/65,098,798bytes、6header（三frame）/23kind3来源全核对、3TLAS/99direct+56indirect/60AIR全审查/两非零HW query；owned cleanup -9。68heap1,558,989,824/初态1,123,342,185，未宣称总预算通过；CPU65/pre-submit65动态header契约拒绝、无初态上传/frameGPU，UE输出/事件/EID0未验。保留两build/空fixture/首UE placement捕获拒绝；full78IR/75RT/308/frame-family/Qt-ARC/官方坏13未跑，两能力false、未提交推送。下一B538 typed动态header版本/Private贡献/descriptor producer-consumer及UE query PSO/root闭包，持续active。

2026-10-06 [B536](BATCH536_UE_UINT16_BUFFER_TEXTURE.md)：补RGBA16Uint TextureBuffer8byte创建/读回/PickPixel及authoritative reflection的纯write typedUAV路径；backend/bundle5bbd0636、GUI3ba30e36。3shape六native/capture/API/CLI、408raw/pixel、24seek cycles/88事件选择、真实高位RGBA与RO/RW、零/非零offset、usage及EID0输出清零通过。72格式/范围/初态坏组144上传前拒绝，3writer组6加载期拒绝（发生初态上传，不计GPU前）。旧16格式两份/旧large R32三份API/CLI、新163初态纹理两capture12696subresource、八query/IR64、官方两scene270事件10查询及六能力通过，630完成日志无严格诊断。实际B535UE仅pre-submit65已越过buffer-view创建，下一slot25:27432/gen5508/type4无typedsource，API4且无GPUwait/初态上传；未验UE frameGPU/output/EID0。首三失败/oracle误判保留，生产flagsfalse、guard不放宽；下一B537显式AS-header factory/producer与typed动态header闭包，持续active、未提交推送。

2026-10-06 [B535](BATCH535_UE_SMALL_PLACEMENT_BLOCKS.md)：隔离provider placement block64→16MiB，唯一源码变更MetalBuffer.cpp、98exports完整，新模块ee945869；--rhi明确选择并记录hash。f4f8b745 backend/bundle不变，新UE capture fbe1b7ac/73,738,545bytes、3间接TLAS/96direct+56indirectcompute、60 AIR全部审查/2shader两非零HW query，owned cleanup -9。80heap共1,731,791,872bytes（旧3.557GB），初态1,175,476,527反略增；CPU65首次接受metadata/预算(native1,765,703,936/snapshot12,835,615)，未执行GPU重放。pre-submit65 API4于RGBA16Uint TextureBuffer8192/row65536创建，无GPUwait/初态上传；10保存日志无严格诊断。生产flagsfalse，UE输出/事件/EID0/typed动态header未验，原引擎/用户工程及guard保持；下一B536 typed RGBA16Uint view真实读写sample与旧回归，再当前UE提交前预检。持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B534](BATCH534_UE_UINT16_TEXTURE.md)：coverage65 Private/Tracked R16Uint2D单mip RW≤4096x512，unsigned16 raw/PickPixel与每row32768+y→65535-y；backend/bundlef4f8b745、GUI3ba30e36。4096x16/4096x512/3x5六native/capture/API/CLI、72事件/EID0、ID/usage/全部像素通过；八像素/线程守262144预算，20坏组40拒绝。旧78texture/view/heap/volume、八query/四IR、fresh整数array/atomic四capture、官方两scene/270事件/10查询/六能力/CPU预算通过；616日志无严格诊断。B533实际新UE CPU65已越过frame创建，native3,593,011,456/initial1,170,276,095/snapshots12,957,904，GPU静态总预算拒绝，无frame GPU。两flagsfalse，下一B535独立provider16MiB block及实际低heap新UE，保留安装引擎/原用户工程和guard；旧B528 sRGB仍未支持，typed动态header/producer待补。持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B533](BATCH533_UE_SMALL_LUMEN_CACHES.md)：新增隔离--small-lumen-caches及运行cvar数值/来源检查，surface512²/grid4/probe8/atlas32实际生效；dba15ac3库，newUEcapture9238d0c3/70,728,712bytes、3间接TLAS/99direct+56indirectcompute，60 AIR全审查0unvalidated、2shader/2非零HW query [2,64,1]/[132,1,1]，owned cleanup -9不计正常退出，输出/重放未验。52heap3,556,769,792bytes比B528增加，初态blob1,170,276,095减少，heap alone超过3GiB-128MiB硬上限，不宣称预算达标。normal/CPU65 API4且无GPUwait/严格诊断，下一R16Uint2D4096x16 usage3；源码找到isolated provider block64MiB（原installed512MiB），可独立减小粒度、guard保持。两能力false，下一B534实际R16Uint二维样例，再有界provider heap粒度与typed动态header/sRGB，未改原用户工程/installed engine、持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B532](BATCH532_UE_PACKED_RENDER_TEXTURE.md)：coverage65 RGB10A2Unorm2D≤512单mip读写可带RT7；backend/bundledba15ac3、GUI3ba30e36。三shape六native/capture/API/CLI、96事件/EID0、native clear读32/96/160、每像素packedbits/PickPixel及独立clear EID往返通过；20坏组40拒绝。旧72texture/view/heap/volume、八query/四IR、fresh整数array/atomic四capture、官方两scene/270事件/10查询/六能力/CPU预算通过；592日志无严格诊断。实际B528UE仅CPU65越过RGB10A2 usage7，下一BGRA8Unorm_sRGB2D320x240 usage7拒绝；无新UE/GPU/full/Qt-ARC，两能力false。下一B533实际小Lumen缓存新UE截帧并核验保留HW query及成本，再补sRGB与typed动态header/producer，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B531](BATCH531_UE_LARGE_PACKED_TEXTURE.md)：补coverage65 Private/Tracked RG11B10Float二维单mip RW≤4096²，wholeheap/总预算保持；backend/bundle3f7e0854、GUI3ba30e36。4096²/2048x1376/3x5六native/capture/API/CLI、72事件/EID0、全部packedbits/PickPixel/usage/ID通过；每线程≤8x8像素守262144调度预算，20坏组40拒绝。旧66texture/view/heap/volume、八query/四IR、fresh整数array/atomic四capture、官方两scene/270事件/10查询/六能力/CPU预算通过；568最终日志无严格诊断。实际B528UE仅CPU65越过大packed2D，下一RGB10A2Unorm二维320x240 usage7拒绝，无新UE/GPU/full/Qt-ARC；两能力false，下一B532 packed二维RT用途与clear/sample，随后小Lumen cache/typed动态header/producer及预算闭包，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B530](BATCH530_UE_PACKED_FLOAT_VOLUME.md)：补coverage65 Private/Tracked RG11B10Float单mip RW3D≤64³，wholeheap/预算保持；backend/bundle99df52e4、GUI3ba30e36。三shape六native/capture/API/CLI、72事件/EID0、每z独立精确11-bit值/全部packedbits/PickPixel/usage/ID通过；20坏组40 GPU前拒绝。旧60texture/view/heap/volume、八query/四IR API/CLI、fresh整数array/atomic四capture、官方两scene/270事件/10查询/六能力/CPU预算通过；544完成日志无严格诊断。首轮wrong_format=92与原RG11B10Float值相同，合法OpenCapture成功，保留FAIL，不声称GPU前拒绝；换Depth32Float3D完整复验。实际B528UE仅CPU65越过packed3D，下一RG11B10Float二维4096x4096 RW拒绝，无新UE/GPU/full/Qt-ARC；两能力false，下一B531有界大packed二维sample，再动态header/producer/预算闭包，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B529](BATCH529_UE_UINT_VOLUME.md)：补coverage65 Private/Tracked RW单mip R32Uint3D≤256x64x64、array1，复用typed placement/lifetime/预算。backend/bundleaee8c20d、GUI3ba30e36；3shape六native/capture/API/CLI、72选择/EID0、各z不同uint/全体素/PickPixel/两write+read usage/不同ID通过；20坏组40拒绝；fresh整数array/2Datomic四capture、旧50texture/view/heap+四floatvolume API/CLI、八query/四IR、官方2scene/270事件/10查询/六能力/CPU预算通过，524完成日志无严格诊断。保留首次sample每dispatch超262144预算拒绝（改每线程≤4相邻体素、guard不变）及fresh raw-source无AIR usage缺失失败（改对应编译metallib后通过）；此前已通过54旧纹理没有重复跑，不将失败计通过。原B528UE仅CPU65越过R32Uint，下一RG11B10Float3D10x8x26拒绝；没有新UE GPU/full/Qt-ARC。两能力false，下一B530 packed-float volume实际sample，再动态header/producer/预算闭包，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B528](BATCH528_UE_BOUNDED_SHADOW_SCENE.md)：补UE isolated small-shadow命令配置，实际cvar256/256/1与Depth16 frame256x256核验，越过B527 8192x2048拒绝；新capture9fa35417/62,399,341bytes、6间接TLAS/186direct+112indirectcompute，60 AIR条目全部审查、2shader/4非零RayQuery；owned capture cleanup -9不计正常退出，输出与GPU重放未验。43heap2,962,489,344bytes，初态blob1,608,781,321，未证明总预算通过（比B527初态增加，不宣称单调优化）。CPU65下一拒绝R32Uint3D192x48x48/usage3，normal frame-born identity拒绝，两API4无GPUwait/严格诊断。backend/bundlee231e2bd、GUI3ba30e36，两能力false，无driver guard放宽、原用户工程/installed engine未改。下一B529实际R32Uint3D frame extent与native/sample反例，再typed动态AS header/producer及预算闭包；持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B527](BATCH527_UE_SMALL_RAY_SCENE.md)：新增隔离3 mesh/2 light Blueprint工程与有限NullRHI生成器（两关卡保存通过），禁用无关默认插件并配置必需SkinCache；小UE有限capture成功、owned cleanup -9，不计正常退出。e231e2bd库，capture1b19090e/56,406,168bytes，3间接TLAS/99direct+56indirectcompute/163编译完成；60原metallib AIR全部审查、2真实非零RayQuery [2,72,1]/[786,1,1]，输出和重放未验。45heap共3,029,598,208bytes（旧5,266,571,264），初态blob1,227,522,505（旧4,362,319,907）；未证明总预算通过。normal-open API4因frame-born identity，CPU65 API4因Depth16 8192x2048 frame texture，均无GPU wait/断言；保留首轮缺SkinCache Fatal及第二轮startup timeout，未计通过。原用户工程/installed engine未改，两能力false。下一B528小场景CSM分辨率/成本验证及实际typed producer闭包，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B526](BATCH526_INLINE_QUERY_HEADER_RANGES.md)：query解析声明buffer+8对齐offset中的64-byte Shared AS header，保留whole backing≤16KiB/不可变来源/typed字段/地址唯一性；原chunk格式保持。backend/bundlee231e2bd、GUI3ba30e36；八native/capture/API/CLI、176事件/EID0、offset32/4096/16304与最后16字节保护区、frame/newTarget/multi64组合通过；236坏组472无GPU wait拒绝、五合法控制。旧query28（B524八份API，其余20 API/CLI）、TraceRay四份API/CLI、官方2scene/270事件/10查询/六能力通过，1734完成日志无严格诊断。旧texture/full/32CPU-AIR/Qt-ARC/官方坏13未重跑，无UE GPU。动态header、heap/nested绑定、producer及UE预算仍待补；下一先隔离低工作集UE场景和新截帧再补实际暴露缺口，两能力false、持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B525](BATCH525_INLINE_QUERY_NEW_TLAS.md)：query允许顺序验证的帧内新TLAS，无初态目标仅在typed冻结build提交后可用；独立前后Shared header/roots。backend/bundle99e80e2d、GUI3ba30e36；四native/capture/API/CLI、96事件/EID0、不同ASID/VA/两输出及前后usage通过；176坏组352无GPU wait拒绝含缺build/错target/提前query，四合法控制；旧query24 API（此前B524八份仅API）及旧16 API/CLI、TraceRay四份API/CLI、官方2scene/270事件/10查询/六能力通过，1280严格日志无诊断。保留首次MTLResourceID类型编译失败，._impl修复后全验；旧texture/full/Qt-ARC未重跑，无UE GPU。源码确认UE TLAS header为Shared动态子分配（纠正此前Private header猜测），下一补有界Shared header偏移、typed producer及低工作集UE；两能力false、持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B524](BATCH524_INLINE_QUERY_FRAME_GEOMETRY.md)：query消费Private帧内BLAS当前kind1/2/8配方，复用冻结验证及几何顺序/索引/finite约束。backend/bundle4d350b89、GUI3ba30e36；八native→capture→API/CLI、192事件/EID0、64几何+64实例最后命中4398、源清零、前后VA/ASID/usage通过；499坏组998无GPU wait拒绝、十合法控制通过；合法65实例native/capture成功而重放按上限拒绝，未声明控制拒绝。旧query16/TraceRay4、官方2scene/270事件/10查询/六能力通过，3358完成日志无严格诊断。保留首次读回buffer缺初态拒绝，明确零初态后完整复验；未放宽预检。旧50texture/full78IR/75RT/308/旧84/32CPU-AIR/Qt-ARC/官方13反例未重跑，无UE GPU，不继承旧hash。下一B525新TLAS目标，再Private/GPU header/typed producer与低工作集UE；两能力false、持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B523](BATCH523_INLINE_QUERY_FRAME_TLAS.md)：query消费同一已初始化TLAS的Private帧内typed冻结当前配方，子BLAS仍初态；Shared roots/header immutable。backend/bundlef71629a0、GUI3ba30e36；六native→capture→API/CLI、144事件/EID0、UserID73→74(2241→2258)、64实例3312→3329及空/屏蔽0→命中、两输出usage/两roots VA/保护字/AS-ID/BLAS前后usage/Private source清零与EID0通过；239坏组478 API/CLI无GPU wait拒绝、六合法UserID75/child-ID重编号控制通过。旧query十份/TraceRay四份、官方2scene/270事件/10查询/六能力及未声明控制通过，1744完成日志无严格诊断。保留oracle SDObject编译失败及合法控制漏CPU快照两次拒绝，修构造并完整复验。旧50texture/full78IR/75RT/308/旧84/Qt/ARC未重跑，UE预算阻塞未变化、本轮无UE GPU。下一B524 query消费Private帧内BLAS几何配方，再Private/GPU header/typed producer与小工作集UE；两能力false、持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B522](BATCH522_INLINE_QUERY_INDIRECT_TLAS.md)：inline query复用既有kind9/10/11间接/空/全屏蔽TLAStyped初態、72→68-byte UserID转换；保持Shared静态roots/header/output，子BLAS初态不可变。backend/bundle6f8bab50、GUI3ba30e36；八份native→capture→API/CLI、128事件/EID0/不同VA-ASID/保护字、UserID73→2241与64实例最后UserID136→3312、Private输入上传后88/5128bytes清零、空/屏蔽全0通过；149坏组298 API/CLI预提交拒绝、无GPU wait；旧query两份/TraceRay四份、官方2scene/270事件/10查询/六能力和未声明拒绝控制通过，1108完成日志无严格诊断。旧50纹理/full78IR/75RT/308/旧84/Qt/ARC未重跑、不继承旧hash；UE预算阻塞未变化未无效重跑，本轮无UE GPU。Private/GPU header/heap query/typed producer和帧内TLAS更新仍缺，下一B523复用已捕获的帧内冻结配方与事件状态；两能力false，持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B521](BATCH521_CONVERTED_INLINE_QUERY.md)：补独立converted inline RayQuery的不可变PSO→roots/offset/header/output声明、typed AS-ID/VA闭包、slot2调度与usage/初态/EID0；没有伪造IR TraceRay packet/SBT。backend/bundlee72514c6、GUI3ba30e36；root0/8两份native→capture→API/CLI、四结果1000/1000/0/0、32事件/EID0/强制VA和AS-ID变化/保护字通过；39坏组78 API/CLI拒绝且无GPU wait，未声明控制拒绝。旧50纹理/view/heap API/CLI、旧view四份native capture replay、四个Private/frame/multi/local/heap IR、fresh default TraceRay、官方两scene/270事件/10查询/六能力/CPU预算通过；588最终日志无严格诊断。初始fixture崩溃及编译/检查器失败保留，AS标签未捕获、同CB BLAS→TLAS仍拒绝。当前仅Shared静态普通TLAS≤64、1D直接query，UE Private/GPU header/typed producer/indirect TLAS/frame更新/低工作集仍待补；原UE CPU总预算拒绝，未跑UE GPU/完整78IR/75RT/308/旧84反例/Qt/ARC，两能力false。下一先复用现有typed间接TLAS初态供query，再补实际UE header和producer；持续active、未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B520](BATCH520_UE_BOUNDED_HEAPS.md)：使coverage65描述表预检单heap范围与现有原生重放576MiB一致，新增完整heap字段预检，保留整个backing收费和总GPU/CPU预算。backend/bundleda2234f1、GUI3ba30e36；129/192MiB四份native→capture→API/CLI、48选择通过，31坏组62拒绝（含六heap超总预算、整数溢出和出生/descriptor错误）；旧46纹理/view API/CLI、旧view四份native capture replay、IR64、官方两scene/270事件/10查询/六能力、CPU预算组件通过。原UE70heap共5.267GB，CPU估计native5.322GB/initial4.362GB，仍超总预算拒绝，无UE GPU运行；完整78IR/75RT/308/旧84反例/Qt/ARC未跑、两能力false。继续小规模converted inline-query AS header/root闭包及低成本UE截帧设置，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B519](BATCH519_UE_FRAME_MIP_VIEWS.md)：补coverage65 R16Float帧内单mip view投影、父资源usage与物理mip一致性。backend/bundle3d2452ec、GUI3ba30e36；5shape十份native→capture→API/CLI、120事件/EID0、view全部rawbits/PickPixel与parent对应通过；29坏组58拒绝，旧36纹理capture API/CLI、旧RGBA8/R16 view四份native/capture/API/CLI、IR64、官方两scene/270事件/10查询/六能力通过。原UE只CPU65越过frame views，下一静态分配guard拒绝（native约4.92GB、initial约4.36GB），未启动UE GPU、未放宽总预算；完整78IR/75RT/308/旧84反例/Qt/ARC未跑、两能力false。下一核实真实heap/初态预算并补typed inline-query闭包，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B518](BATCH518_UE_HALF_ARRAY_EXTENT.md)：补实际RGBA16Float二维数组RW尺寸64→512（仅coverage65，≤8层），保留旧RT/旧coverage尺寸。backend/bundle5f1a2f18、GUI3ba30e36；4新shape八份native→capture→API/CLI及旧coverage64+RT7两份、120事件/EID0、每层half bits/PickPixel/usage/GPU ID通过；19坏组38拒绝，旧R16/packed二维/R8各六份/R11八份API/CLI、IR64、官方两scene/270事件/10查询/六能力通过。真实UE仅CPU65越过所有此前texture创建拒绝，下一frame texture view拒绝；原XML有11841的0–7 mip单层views，具体失败分支待精确诊断。原capture不变，未跑UE GPU/输出、78IR/75RT/308/旧84间接反例/Qt/ARC；两能力false，继续mip view语义和typed inline-query闭包，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B517](BATCH517_UE_PACKED_FLOAT_ARRAY.md)：补实际Lumen RG11B10Float二维数组RW（coverage65、≤512、1–8层、Tracked/单mip），每层独立red及11/11/10原始位值/PickPixel验证。backend/bundle3d8bf082、GUI3ba30e36；4场景八份native→capture→API/CLI、96事件/EID0/usage/GPU ID，19坏组38拒绝通过；旧R16/packed二维/R8数组各六份、旧RG11二维两份native capture replay、IR64、官方两scene/270事件/10查询/六能力通过。原UE CPU65越过12052–57，下一12058 RGBA16Float二维数组320x240 array1 usage3仍拒绝；未修改原capture、未重跑UE GPU。未跑78IR/75RT/308/旧84间接反例/Qt/ARC和UE输出，两能力false；继续真实half数组及inline-query闭包，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B516](BATCH516_UE_R8_ARRAY_TEXTURE.md)：补实际Lumen R8Unorm二维数组RW placement（coverage65、≤512、1–8层、单mip/Tracked），每层不同值核验。backend/bundlee6ecb223、GUI3ba30e36；320x240×1/512x512×8/3x5×3六份native→capture→API/CLI、72事件/EID0、全部层字节/PickPixel/读写usage/不同GPU ID通过；19坏组38拒绝，旧R16六份/packed二维六份API/CLI、旧单层R8和float数组四份native capture replay、IR64、官方两scene/270事件/10查询/六能力通过。UE仅CPU65越过12051，下一12052 RG11B10Float二维数组320x240 array1 usage3拒绝，原capture不变。未跑78IR/75RT/308/旧84反例/Qt/ARC和UE输出；两能力false，继续实际数组格式与inline-query闭包，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B515](BATCH515_UE_PACKED_2D_TEXTURE.md)：补实际Lumen RGB10A2Unorm二维RW placement（coverage65、≤512、单mip、tracked）；旧3D/旧coverage保持。backend/bundled8f74eec、GUI3ba30e36；320x240/512x512/3x5六份native→capture→API/CLI、72事件/EID0、packed全部bits/PickPixel/正确读写usage/不同GPU ID通过；18坏组36拒绝、旧R16六份/旧packed3D两份native capture replay、IR64、官方两scene/270事件/10查询/六能力通过。实际UE仅CPU65，越过12050，下一12051 R8Unorm二维数组320x240 array1 usage3仍拒绝，未修改原capture、未重跑UE GPU。未跑78IR/75RT/308/Qt/ARC及旧84间接反例，B514结果仅旧库；两能力false，继续数组形态与inline-query闭包，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B514](BATCH514_UE_FRAME_R16_MIPS.md)：补实际UE的Private/Tracked placement R16Float RW二维多mip（≤512、完整mip预算）；修默认compute encoder误读dispatchType越界，AIR补typed ulong→pointer/texture句柄与air.buffer入口。backend/bundlecc91bb09、GUI3ba30e36；8/9/10mip六份及旧array/volume/float32/二维八份native→capture→replay通过，最终14份核验两写/读事件usage、逐像素/PickPixel/168选择-EID0及不同GPU ID；18坏mip组36拒绝、84坏间接证据168拒绝/2执行错配4拒绝/2控制、旧16间接112seek、IR multi64、官方2scene/270事件/10查询/6能力、32CPU+AIR通过。失败和oracle纠正保留；原UE CPU65越过11841，下一12050 RGB10A2二维320x240 usage3仍拒绝。UE输出/完整重放、78IR/75RT/308/Qt/ARC未验，冻结根因未修、两能力false；继续下一实际packed二维资源与typed inline-query闭包，未提交推送。

2026-10-06 [PHASE59](PHASE59.md) / [B513](BATCH513_UE_LUMEN_QUERY_AND_STREAM_SCAN.md)：B512验收后已回UE5.8.3有限Lumen HWRT截帧，原库ad0a保存新帧并owned cleanup（editor -9，不计自行正常关闭），6间接TLAS/643compute，280native编译完成。原metallib 58个Lumen模块全部CPU反汇编，PSO/function/library+原per-use组确认2shader/4非零硬件ray query调度；输出及完整重放未验。修ScanDescriptorMetadata对前向压缩流的两处非法回退，保持typed证据/guard；修旧三反例生成器零chunk预算，保留失败及两拒绝fixture的oracle纠正。最终backend/bundle77a7fa99、GUI3ba30e36；8compute+8render/112seek-reset、84组坏证据API+CLI168拒绝/2执行点错配4拒绝/2拒绝控制、64geometry IR定向、官方2scene/270事件与6能力通过，32CPU tests通过。原UE正常打开仍拒绝未声明资源，CPU65进一步拒绝R16Float256x128/mip8；77未重新UE截帧、完整78IR/75RT/308/原Qt/ARC/UE图像事件未验，不能继承ad0a全量。报告库存变化0仅stat，不证明旧冻结根因。两能力false、持续active、未提交推送；下一先实际UE多mip纹理与typed inline-query资源包/AS header producer闭包，再实际输出/事件/EID0。

2026-10-06 [PHASE59](PHASE59.md) / [B512](BATCH512_IR_FRAME_MULTI_GEOMETRY.md)：参照DX12/Vulkan多几何BLAS，补Private整buffer encoder-end冻结、新append chunk/current recipe/IR预检，保留几何顺序、Float3/4和UInt16/32原offset/stride及IFT槽；仅完整Private/Tracked/standalone usageNone使用新路，其他接口保持。修GeometryIndex所需SM6.6与64几何补写chunk参数预算，强化逐步日志检查，中间失败保留。最终backend/bundlead0a0248、GUI3ba30e36；串行78 native/capture/API8268事件（新multi6/1446）、2088坏IR/135合法控制（multi579/30）、1坏列表/1往返、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13反例/6能力通过。40正常重开growth1785856B，另30失败+10成功/10CPU导出/30seek-reset growth638976B；6504最终日志无断言/overrun/资源表/旧关闭诊断，CPU互斥/语法/diff通过、起止hash一致。2及64几何最后命中GeometryIndex贡献，逆序变成0，未引用NaN允许、引用NaN拒绝；新目标前后事件usage/EID0独立。UE/75RT/308/原Qt/ARC/独立旧table未跑、系统冻结根因未修、能力false、持续active、未提交/推送。下一按用户最新要求先有限UE Lumen HWRT新截帧/实际调度，按真实缺口补producer/接口；refit/copy/异buffer仍未支持。

2026-10-06 [PHASE59](PHASE59.md) / [B511](BATCH511_IR_FRAME_TRIANGLE_GEOMETRY.md)：参照DX12/Vulkan动态BLAS，补Private顶点/索引encoder-end冻结、新append chunk/current recipe/IR预检/新AS目标及staging生命周期；完整已验证输入与usageNone才走新路，其他入口保留。修44有效bytes需48最后stride填充，并修新目标事件usage oracle及SDObject访问器编译错误，失败完整保留。最终backend/bundled15e1e52、GUI3ba30e36；串行72 native/capture/API6822事件（新geometry6/1401）、1509坏IR/105合法控制（geometry471/24）、1坏列表/1往返、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13反例/6能力通过。40正常重开growth1130496B，另30失败+10成功/10CPU导出/30seek-reset growth344064B；4387最终日志无断言/overrun/资源表/旧关闭诊断，CPU gate互斥/语法/diff通过、起止hash一致。新目标无build明确拒绝，前后BLAS读usage互不混淆。UE/75RT/308/原Qt/ARC/独立旧table未跑、旧冻结根因未修、能力false、持续active、未提交/推送。下一IR多几何/refit/copy及可靠UE producer布局接入。

2026-10-06 [PHASE59](PHASE59.md) / [B510](BATCH510_INDIRECT_AS_SCRATCH_OFFSETS.md)：参照DX12 Buffer+Offset/VK alignment及Metal三角形路径，补间接TLAS非零scratchOffset，append新chunk/冻结补写/core/IR预检、256对齐与length-offset校验，旧零offset格式保持；8新场景GPU保护区及事件/EID0验证。修条件序列化宏缺完整块误读旧padding，失败完整保留。最终backend/bundlee11c809e、GUI3ba30e36；串行66 native/capture/API5421事件（offset8/1086）、1038坏IR/81合法控制（offset frame176/16）、1坏列表/1往返、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13反例/6能力通过。40正常重开growth1064960B，另30失败+10成功/10CPU导出/30seek-reset growth393216B；3283最终日志无断言/overrun/资源表/旧关闭诊断，CPU gate互斥/语法/diff通过，起止hash一致。用户ForceCrash具体offset未证实；UE/75RT/308/原Qt/ARC/独立旧table未跑、系统冻结根因未修、能力false、持续active、未提交/推送。下一帧内AS目标/几何及可靠UE producer接入。

2026-10-06 [PHASE59](PHASE59.md) / [B509](BATCH509_IR_FRAME_INDIRECT_BUILDS.md)：补IR帧内Private间接TLAS当前配方、typed冻结子BLAS/UserID、提交/encoder顺序和typed blit不可变输入保护；两次TraceRay旧/新输出及EID0独立恢复，覆盖初始空/屏蔽、heap/local/global。修帧AS补写文件流metadata flags断言，修旧合法header控制长度及实际commit位置；失败完整保留。最终backend/bundle d8d0b308、GUI3ba30e36；串行58native/capture/API4335事件（新7/1188）、862坏IR/65控制（新156/12）、1坏列表/1往返、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13反例/6能力通过。40正常重开growth3588096B，另30失败+10成功/10CPU导出/30seek-reset growth737280B；2770最终日志无断言/overrun/资源表/旧关闭诊断，CPU新gate互斥/语法/diff通过，起止hash一致。初始列表外层错误分类差异未修改。下一先补UE非零scratchOffset（同一bridge入口仍拒绝，仅源码缺口、未证明用户崩溃触发条件），再补目标/几何/producer；UE/75RT/308/原Qt/ARC/独立旧table未跑、冻结根因未修、能力false，目标active、未提交/推送。

2026-10-06 [PHASE59](PHASE59.md) / [B508](BATCH508_INITIAL_LIST_AND_DEVICE_CLOSE.md)：修owner device在manager Shutdown前解除注册，并覆盖CPU structured-export dummy；按DX12/Vulkan WrittenRecord格式解码初始资源列表，保留ID/written而不调用未实现的generic初态。backend/bundle5f4db1c7、GUI3ba30e36；最终串行51组native/capture/API3147事件/EID0/CLI3、706坏IR/53控制、1坏初始列表/1往返控制、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13反例/6能力通过。40重开growth409600bytes，另30失败+10成功/10CPU导出/30seek/reset growth1081344bytes；有效2326日志无旧device/资源表/断言诊断，CPU/native LLDB owner-release断点均未命中且exit0。新测试日志锁及零长度坏chunk构造失败保留；B507已存在的空TLAS反例chunk长度错误已修正，5间接组156/14重跑无断言，两个CPU互斥检查与语法/diff通过。IR帧内AS仍被提前拒绝，下一当前配方/preflight/EID0与可靠UE producer；UE/75 RT/集中/原Qt/ARC/独立旧table未跑，死机根因未修、能力false，持续active、未提交/推送。

2026-10-06 [PHASE59](PHASE59.md) / [B507](BATCH507_IR_INDIRECT_TLAS.md)：补IR global/heap header消费kind9间接、kind10空、kind11全屏蔽TLAS，复用72→68-byte typed子BLAS/UserID转换与IFT校验；InstanceID73真实输出146/11，Private/GPU输入建AS后清零仍正确，heap-only34/183、局部266/241、全局+局部1477/1452。backend/bundleee222fe1、GUI3ba30e36；最终串行51组native/capture/API3147事件/EID0/CLI3、706坏IR/53控制（新间接156/14）、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13坏capture/6能力及40有限重开(growth1589248bytes) PASS，hash一致。60份新增SDK反射/冻结recipe/UserID/Private清零及CPU语法/diff/新gate锁通过；合法UserID74输出147/11。测试ID强制变化、unused-AS usage、脚本blobs初始化、诊断helper编译和SDK枚举期望修正，失败保留。当前仍IR静态契约，所有帧内AS encoder提前拒绝；成功日志已有eResDevice关闭错误（B506亦存在），须修owner清理且不把有限重开计完整生命周期。UE/75 RT/集中/原Qt/ARC/旧独立table未跑，系统冻结根因未修、能力false。下一owner device关闭与帧内AS配方/preflight/EID0，随后可靠UE producer/typed view等；持续active、未提交/推送。

2026-10-06 [PHASE59](PHASE59.md) / [B506](BATCH506_IR_HEAP_ACCELERATION_STRUCTURES.md)：补IR资源heap AS-header kind3(bytes64、metadata0)，共用TLAS/BLAS/贡献/IFT typed闭包；支持无直接AS根的真实TraceRay，第二TLAS平移交换命中，输出34/110、局部266/168、全局+局部1477/1379。backend/bundle b0094506、GUI3ba30e36；最终串行36组native/capture/API2277事件/EID0/CLI3、550坏IR/39合法控制（新AS115/12）、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/**13坏官方capture**/6能力、40生命周期(growth933888bytes) PASS，起止hash一致。64份SDK反射(新AS36份)/heap-only真实UAV+常量根、CPU语法/diff/新gate互斥PASS。首个帧内坏header测试构造无效已修正为实际chunk注入，失败保留；B505官方反例误写41已按原日志纠正13。IR仍只接受direct TLAS kind5，实际UE用Indirect；heap CBV/UAV/typed view、nested UB/大heap/Private或GPU IR/动态heap及可靠UE producer关联仍缺。UE/75 RT/集中/原Qt/ARC/独立旧table未跑，死机根因未修、能力false，下一补IR间接TLAS及typed view；未提交/推送、持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B505](BATCH505_IR_DESCRIPTOR_HEAPS.md)：补immutable PSO→IR资源/sampler heap entry声明，直接索引raw SRV view/texture/sampler、标量buffer-size metadata、动态slot及零洞，full typed闭包/usage/重定位。backend/bundle1b40caee、GUI3ba30e36；27组native/capture/API1635事件/EID0/CLI3，heap96/48、local154/280、global+local1365/1491，强制AS-ID/VA/texture变化通过；435坏IR/27合法控制、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/13坏sample（B506按原日志纠正）/6能力/40生命周期(growth1769472bytes) PASS，最终全串行。28份SDK reflection/root flags3072和新gate互斥CPU检查PASS；SDK reflection不提供直接索引heap成员，须producer已知类型，不推断位型。首次编译Max断言未更新已修，失败日志保留。heap AS/CBV/UAV/typed texture-buffer view、nested UB、大heap/Private或GPU IR/动态heap及可靠UE shader自动关联仍缺；UE/75 RT/集中/原Qt/ARC/独立旧table未跑，死机根因未修，能力false。下一真实heap AS/view与隔离UE producer接入；持续active、未提交/推送。

2026-10-06 [PHASE59](PHASE59.md) / [B504](BATCH504_IR_GLOBAL_ROOTS_AND_REFLECTION.md)：补多根GRS及immutable PSO→global CBV/SRV/constants/texture/static sampler/AS-header/UAV布局，8-word真实GRS与global+local组合。backend/bundle5566f91b、GUI3ba30e36；最终串行20组native/capture/API1155事件/EID0/CLI3，输出1284/1222、组合1342/1454及any-hit1454/1454；强制AS2→3/VA/global texture1→2/local2→3，325坏IR+21合法控制、18旧targeted/41旧argument/6旧packet/旧IR、官方2scene/270事件/10查询/41坏sample/6能力/40生命周期(growth1441792bytes) PASS。20份SDK真实reflection布局/entry/metallib hash与IR gate互斥CPU检查PASS。首轮短暂GPU测试重叠已保留、不计验收，新增互斥后完整串行重跑。UE普通shader离线转换→运行时metallib，仅compiler hook不能可靠关联；heap/嵌套UB/Private或GPU IR/callable仍缺、UE/75 RT/集中/原Qt/ARC/独立旧table未跑，死机根因未修、能力false。下一真实IR资源heap及可靠UE producer shader布局接入；未提交/推送、持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B503](BATCH503_IR_LOCAL_ROOTS_AND_STATIC_SAMPLERS.md)：补immutable shader→local CBV/SRV/constants/texture table/static sampler布局，96-byte SBT及typed资源链。backend/bundle285f8768、GUI3ba30e36；15组native/capture/API810事件/EID0/CLI3，局部输出131/243与any-hit243/243，UE同六sampler(slot3)输出通过，texture1→2及合法sampler285→29/288→32通过。185坏capture+15合法控制、18旧targeted/41旧argument/6旧packet/旧B501与B502 IR、官方2scene/270事件/10查询/41坏sample/6能力查询、40生命周期(growth1769472bytes) PASS。中间转换/编译/测试构造失败保留；未跑UE/75 RT/集中/原Qt/ARC/独立旧table。global仍2根，heap/callable/Private或GPU IR/嵌套UB/非零texture-sampler metadata未支持；死机根因未修、能力false。下一多根GRS/IR资源表与UE自动关联接入；显式fixture注解不冒充通用UE。未提交/推送、持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B502](BATCH502_IR_NULL_SHADERS_AND_ANY_HIT.md)：修正 VisibleFunction any-hit 的 VFT/几何IFT关联、保留UE pad0标量、允许空miss/closest-hit，并补immutable shader role声明及错角色拒绝。backend/bundle28d2a162、GUI3ba30e36；9组native/capture/API432事件/EID0/强制AS-ID/VA变化/CLI3，69坏capture+7合法控制、18旧targeted/41旧argument/6旧packet/B501旧IR、官方2scene/270事件/10查询/41坏sample/6能力查询、40生命周期(growth1900544bytes) PASS。原测试构造失败保留。未跑UE/75 RT/集中/原Qt/ARC/独立旧table；local root/static sampler/heap/GPU IR未支持，死机根因未修、能力false。下一扩真实fixture的local root/static sampler，禁止旧长UE/full；未提交/推送、持续active。

2026-10-06 [PHASE59](PHASE59.md) / [B501](BATCH501_TYPED_IR_RAY_DISPATCH.md)：完整typed IR packet/SBT/GRS/AS-header重定位与useResources(TLAS)的BLAS初态依赖已补，首次转换TraceRay native/capture/replay73/11、16事件三方向/EID0/资源usage/强制AS ID2→3及VA变化PASS。最终backend/bundle10c28bf1、GUI3ba30e36；54坏capture+合法stride32、官方2scene/270事件/10查询/41坏sample/6能力查询、18旧targeted/41旧argument反例/6旧packet、旧typed table10反例及4×10生命周期(growth1343488bytes) PASS。首个schema仅Shared静态、两直接根、32-byte records/无static sampler或local root；UE未验，75 RT/集中/原Qt/ARC未跑，死机根因未修、能力false。下一用真实fixture补UE pad0=~0ull/Null shader identifier，再扩local root/static sampler/GRS，禁止旧长UE/full。失败日志和最终hash清单保留，未提交/推送、目标active。

2026-10-06 [B500](BATCH500_TLAS_PACKETS_AND_CONVERTED_DXR_SAMPLE.md)：补TLAS参数GPU路径；12例BLAS/TLAS native/capture/API792事件/EID0/CLI、294坏输入、4帧内明确拒绝、18旧targeted/41旧反例/130生命周期(growth327680bytes) PASS。新增UE5.8.3 DXC→Apple IR真实TraceRay样例，RaygenIndirection+VFT/IFT、152-byte参数/32-byteSBT ID/64-byteAS header，native与精确probe capture命中73/未命中11；CPU完整ABI/来源及1次native编译审计PASS。IR离线明确拒绝无声明raw地址，未修重定位、不计replay成功。backend/bundle仍03fc7b32、GUI3ba30e36，B499同hash官方证据有效，本批未重跑。UE/75 RT/集中/原Qt/ARC未验，死机根因未修、能力false。下一直接用converted fixture补完整typed IR packet/SBT/GRS/AS header声明重定位，禁止绕过guard/直接旧长UE/full。失败日志保留，未提交/推送，目标active。

2026-10-06 [B499](BATCH499_TYPED_RAY_ARGUMENT_RESOURCES.md)：补MTLArgumentEncoder AS/IFT typed编码和AS/IFT/VFT初态重新编码，compute调度前验证成员、已构建AS种类与函数表PSO，补CS usage。六例Shared/Managed、function/device、标量/函数表数组native/capture/API396事件/EID0/CLI PASS，120坏输入及2帧内明确拒绝PASS；18旧targeted/41旧反例/70生命周期(growth409600bytes) PASS。新backend/bundle03fc7b32、GUI3ba30e36；当前库官方2scene/270事件/10查询/41坏sample/6能力查询PASS。UE raw IR/SBT尚未实现，instance参数GPU/嵌套/render/帧内重编码未验或不支持；UE/75 RT/集中/原Qt/ARC未跑，死机根因未修，能力false。下一typed TLAS packet和缺失/错误AS种类反例→完整UE IR声明重定位，禁止直接旧长UE/full。未提交/推送，持续任务未完成。

2026-10-06 [B498](BATCH498_SIX_NATIVE_RAY_COMPILE_APIS.md)：六种sync/async compute创建入口均用真实opaque triangle ray query验证1/0；exact1 trace有6begin/6end/3submitted、callback native线程可关联，unset/0/true/空关闭。enabled capture32事件×3/EID0/PSO+AS绑定、23坏creation、19旧targeted、30生命周期(growth589824bytes) PASS。修正生命周期检查漏认帧前AS无IFT inline ray；失败日志保留。UE IR实际header CPU布局152/104/32bytes，记录完整SBT/GRS/函数表/嵌套record依赖，未实现IR重定位。backend/bundle未改仍eea2539f，GUI3ba30e36，B497同hash官方证据有效；本批UE/75 RT/集中/原Qt/ARC未跑，死机根因未修，能力false。下一完整typed IR声明与重定位，禁止直接旧长UE/full；未提交/推送。

2026-10-06 [B497](BATCH497_FUNCTION_TABLE_GPU_IDENTITY.md)：按DX12 shader identifier关联/VK group handle原生转发补VFT/IFT gpuResourceID及typed依赖，查询未绑定表也捕获；没有实现UE raw IR packet重定位。最终backend/bundle eea2539fb069dae9d59d2c32ad962c562ee83aecb09d57be195e50538f123c57，GUI3ba30e36。四例native/capture/API714次事件/EID0/CLI PASS，77坏capture/8合法副本、3份旧RT408事件、18份旧函数表targeted、50生命周期(growth589824bytes) PASS；官方2scene/270事件/10尺寸/41坏sample/6能力查询PASS。四次真实descriptor-sync取证完整，新增系统native thread ID和日志shared-lock保留；10+21项CPU、preflight/语法/diff PASS。早期编译/日志/测试oracle失败均保留。UE/75 RT/集中/原Qt/ARC未验，死机根因未修，两能力false。下一UE IR typed ABI及六入口有限诊断，继续禁止直接旧长UE/full followup；未提交/推送，持续任务未完成。

2026-10-06 [B496](BATCH496_NATIVE_COMPUTE_COMPILE_TRACES.md)：新增仅capture且精确env=1的native compute编译取证，覆盖function/function-options/descriptor三类同步及三类异步。begin/submitted/native end按process/token关联，记录线程、函数名/label bytes、linked表数量、options/stackDepth、native结果与耗时；async可识别调用未返回/等待callback，不改变native调用次数、ABI、回放格式或支持范围。监督器退出时保存compile-trace.json；9项协议审计+20项监督/INI CPU测试PASS，串行renderdoccmd/app构建及preflight/语法/diff PASS。最终backend/bundle acd0c0d697b41c83390419b972e0a508358226371f4ced65e111123519369c14，GUI3ba30e36；111c4787已保留baseline。旧UE日志没有新trace，结果NOT OBSERVED，不能定位旧待编译函数。未运行GPU/UE/sample/75 RT/集中，新库不能继承111或08028旧库验收。两能力false；死机根因未修，先CPU推进UE IR/函数表typed ABI与编译证据，不直接重跑旧长UE/全量。持续任务未完成、未提交/推送。

2026-10-06 [B495](BATCH495_UE_RT_FREEZE_DIAGNOSTICS.md)：系统重启取证与UE诊断监督。23:50:44启动的111c4787 UE session在frame127停滞；WindowServer连续40s未checkin，UE线程等待高CPU的MTLCompilerService，forceReset为btn_rst/force_off。未证实GPU内核panic或具体根因。新增12s帧进展检测（同帧后台日志不续期）、1s栈取证/2s采样上限、owned group清理及outer timeout收尾，原子状态记录；修复RT隔离启动INI沿用1728×1020最大化窗口，固定640×480且原设置hash不变。18项CPU失败注入/INI测试、preflight/语法/diff PASS；重启后未启动UE/GPU、未改后端二进制。B494官方2scene/10查询/41坏sample/6能力查询已PASS；UE截帧未完成，75份RT/本库集中未运行，manifest已纠正INTERRUPTED。backend/bundle111c4787c2a7fddf59bdd7503e2ea89f8227c23d72962f584942cb7e2e65a14d，GUI3ba30e36。公开能力false。接续先CPU定位待编译管线/RT ABI及补取证；不要直接重启旧run_followup.py、全量或长UE运行。监督器不能保证阻止系统级死锁，根因未修复，持续任务未完成。

B492 ae81e24a接续结果：官方两scene/10尺寸查询/41坏sample/6能力查询、63份B482–492 RT7197事件、AS身份旧反例/schema2/旧Indirect46反例PASS，冻结/工作库起止hash一致。UE session20261005-222357已越过zero count，下一FAIL为private间接primitive候选count0（limit1024），instanceCount尚需补诊断，不能推定为null AS。当前库集中未跑；B491 e6集中308/7784/3080/growth0 PASS仅旧库。下一Native inactive instance探针及UE精确候选/参数诊断；生产能力false。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B492](BATCH492_RAY_EMPTY_INDIRECT_TLAS.md)：补Indirect零count尺寸/heap查询及kind10/schema8空TLAS配方，不读输入、不猜子AS，严格offset<length。backend/bundle ae81e24a，GUI3ba30e36。五例Shared帧前/帧内、Private/placement帧前及同CB alias：native/capture尺寸一致、ray miss0/0/0/0、API588事件/EID0/CLI、169坏输入、20旧检查/T12416反例与60生命周期PASS（growth589824bytes/hash一致）。官方/UE/63份RT接续中，本库集中未跑，e6固定集中308/7784/3080/growth0 PASS为旧库。ARC/原Qt crash/GUI未闭环，两能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B491](BATCH491_RAY_MANY_INDIRECT_CHILDREN.md)：间接TLAS typed子AS预算1024（GPU实际最多128已验），直接仍4；固定native数组越界已修。backend/bundle e6f18f11，GUI3ba30e36。五例native/capture/API588事件/CLI、226坏输入、20旧检查/T12416及60生命周期PASS（growth327680bytes）；官方两scene/10查询/41坏sample/6能力查询、58份RT6609事件、身份旧反例/schema2和集中308/7784/3080 PASS，集中growth0bytes/exit0/起止hash一致。UE session20261005-215022已越过多候选guard，下一FAIL为Private placement间接count=0；B492源码已编辑，等待native基线及新构建验证，不计本证据。ARC/原Qt crash/GUI未闭环，两能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B490](BATCH490_RAY_PLACEMENT_INSTANCE_INPUTS.md)：帧前Tracked placement heap间接实例输入已冻结并按kind9独立packed初态恢复；区分原生birth aliasability与显式退休，scratch/AS输出同heap重叠拒绝。backend/bundle0495cfde，GUI3ba30e36。七例native/capture/API714事件/CLI、322坏输入、20旧检查/T12416反例及8×10生命周期PASS（growth311296bytes）；distinct alias同CB/后来CB擦零通过。官方两scene/10查询/41坏sample/6能力查询PASS；UE越过heap guard后FAIL于1–4候选检查（session20261005-210821），固定库集中308/7784/3080 PASS，growth13074432bytes/hash一致。仍≤4候选、帧内Private/Untracked/ARC/原Qt crash/GUI未验，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B489](BATCH489_RAY_PRIVATE_INDIRECT_SNAPSHOTS.md)：Tracked独立Private间接实例已按AS encoder执行点冻结并按真实packet筛选typed子AS，帧前schema7恢复。backend/bundle349df416，GUI3ba30e36；八例native/capture/API816事件/CLI、368坏输入、20旧检查/T12416反例及9×10生命周期PASS（growth327680bytes）。官方两scene/10查询/41坏sample/6能力查询PASS，UE新诊断FAIL于间接实例输入guard（session20261005-203243），下一补精确诊断；本库集中全量未跑，c924730b集中308/7784/3080 PASS仅为旧库。帧内Private/heap/untracked/超过4候选仍未支持；ARC/原Qt crash/GUI未验，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B488](BATCH488_RAY_TYPED_INDIRECT_INSTANCES.md)：Shared Indirect实例按查询过的typed AS ID关联并规范化为UserID+live child handles，schema7初态/帧内/compact/copy已补；backend/bundle c924730b，GUI3ba30e36。七例native/capture/732事件/CLI、318坏输入、20旧检查/T12416反例、8×10生命周期PASS（growth720896bytes）。官方/38份RT/4491事件/身份旧反例/schema2及集中308/7784/3080 PASS，growth4653056bytes/hash一致；B486 a2e87831集中308/7784/3080 PASS/growth0为旧库。下一Private/GPU实例，UE同一guard不重跑，ARC/原Qt crash/GUI未验，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B487](BATCH487_RAY_USER_ID_INSTANCES.md)：补UserID直接TLAS、source偏移/填充步长及schema6初态，保留完整uint32编号；backend/bundle26303b3c，GUI3ba30e36。七例native/capture/732事件/CLI、284坏输入、20旧检查/T12416反例、8×10生命周期PASS（growth458752bytes）；官方两scene/10查询/41坏sample/6能力查询、24份旧RT/3027事件、7份Managed/flags及schema2兼容/24坏TLAS复验PASS。本库全量/UE/GUI未跑，B486 a2e87831固定308/7784/3080 PASS、growth0/hash一致。下一Shared间接实例typed关联；Private/Indirect/ARC/原Qt crash仍未闭环，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B486](BATCH486_RAY_ENCODER_INPUT_SNAPSHOTS.md)：补齐Tracked独立Private多indexed输入在AS encoder结束位置冻结，支持同CB GPU上传及构建后覆盖；backend/bundle a2e87831、GUI3ba30e36。四例408事件/CLI、120坏输入、20旧检查/T12416反例、5×10生命周期PASS（growth409600bytes）；Shared同CB边界native/capture正确、离线预期干净拒绝，未扩大支持。官方两scene/10查询/41坏输入/6查询PASS，固定库24份RT/3027事件及集中308/7784/3080 PASS、growth0/hash一致。Indirect TLAS原生四例0/73/0/73通过、旧库capture SIGTRAP失败，UE/ARC/原Qt crash/GUI未验，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B485](BATCH485_RAY_COMPUTE_VFT_CAPACITY.md)：修复UE触发的compute VFT count>32限制，按65536项重放分配预算保留原生创建结果；不扩大IFT offset/render能力。backend/bundle80c2d706，GUI3ba30e36。四例真实ray调用33/256/65536表末槽native/capture/705事件/CLI、32坏输入、20旧检查及T12416反例、5×10生命周期PASS（growth196608bytes）；官方两scene/10查询/41坏输入/6能力查询PASS。UE越过VFT后失败于Indirect TLAS构建；固定库20份RT/2619事件复验PASS，本80c2d706集中308/7784/3080 PASS、growth0bytes、起止hash一致。ARC/原用户crash/UE RT调度与离线未验，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B484](BATCH484_RAY_AS_GPU_IDENTITY.md)：AS gpuResourceID精确native查询、ResourceId关联/去重和查询依赖已补，backend/bundle cde95930，GUI3ba30e36。六例native/capture/738事件/API/CLI、36坏输入+6合法副本、18旧检查和7×10生命周期PASS（growth409600bytes）；官方两scene/10尺寸查询/41坏输入/6能力查询PASS。真实UE越过getter，随后compute VFT容量>32被拒绝并在管线断言，B485继续。本库全量未跑，32333bb6上波308/7784/3080已PASS、growth0/hash一致；ARC和用户原crash未验，RT dispatch/离线未证明，能力false，持续推进、未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B483](BATCH483_RAY_UNBOUND_SIZE_AND_UI_LIFETIME.md)：无输入/indirect TLAS尺寸及heap查询已与原生10组一致；backend/bundle32333bb6、GUI3ba30e36。官方两scene新截/离线像素/44与46事件×3方向/41坏输入及6能力查询通过。Qt header旧模型回调问题修复前FAIL、修复后PASS，当前独立CPU模型/销毁/滚动测试PASS；用户提供的原崩溃未复现，完整操作场景待回复。真实UE越过无输入查询，下一阻塞AS gpuResourceID；RT dispatch/离线未验，ARC未修，能力false。固定32333bb6全量308/7784/3080 PASS、growth0bytes、起止哈希一致（frozen-validation-32333bb6），持续任务继续，未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B482](BATCH482_RAY_MULTIPLE_INDEXED_GEOMETRIES.md)：已补2–64段共享VB/IB indexed geometry捕获/重放、schema5帧前输入冻结、Shared TLAS子快照materialise及compact/copy。最终backend/bundle cad38602一致；十例native/capture/1,176事件选择/CLI、300坏输入、18旧例及11×10生命周期通过（growth688128bytes），官方两scene新截/离线像素/事件/41坏输入及6能力查询通过。真实UE已越过两geometry构建，当前失败于无instance buffer/children的TLAS尺寸查询，B483继续补。UI崩溃线索为尺寸测量/疑似栈耗尽，header旧模型回调生命周期修复验证中；原报告未复现。ARC、UE dispatch/离线、本库全量及GUI未验；公开能力false，持续任务继续，未提交/推送。

2026-10-05 当前 [PHASE58](PHASE58.md) / [B481](BATCH481_RAY_HEAP_AS.md)：heap AS 自动/placement、帧前/帧内创建与 heap/offset 查询已补齐。backend/bundle 9122ac7a一致；六例native/capture/API/CLI通过、846事件选择、78坏捕获、18旧例和7×10生命周期通过（growth278528bytes）。官方两scene重新截帧/像素/事件/CLI、41坏输入及6能力查询通过。真实UE首个阻塞已定位并修复为未包装heap AS；当前进入usage0/motion1/两段Private indexed geometry，B482继续。ARC、UE RT dispatch/离线/GUI/本库全量未通过或未运行；生产能力仍false，目标active，未提交/推送。

2026-10-05 当前 [PHASE57](PHASE57.md) / [B480](BATCH480_RAY_UE_DIAGNOSTIC_AND_TABLE_IDS.md)：backend/bundle e0e4df24一致。compute VFT/IFT 单表/数组身份拒绝和合法 nil 恢复、6项能力查询、官方两种光追 sample 新截/像素/44与46事件三方向/41坏输入通过；basics定向2,070命名事件、20×10生命周期通过（growth540672bytes）。默认两项能力false，进程诊断开关只按原生值放行compute。UE已初始化并真实进入AS构建，primitive descriptor兼容检查拒绝；B481正定位usage/geometryCount。ARC、GUI和UE RT回放未通过；本库未重跑全量。B479固定a35028ba全量308/7784/3080已通过。目标active，未提交/推送。

2026-10-05 当前 [PHASE57](PHASE57.md) / [B479](BATCH479_RAY_BOX_INITIALS_AND_TABLE_ORDER.md)：官方 triangle/procedural 两场景 native→capture→offline 像素逐字节一致，44/46 事件各三方向和 CLI 各3 loops通过。补齐 boxes 初态、compact 依赖、先函数表后 pipeline 的合法顺序；backend/bundle a35028ba 一致。22 旧 RT 例 + pointer、2,574 命名事件选择、27 T60 +41 官方坏输入通过。固定库全量308/7784/3080通过、growth3784704bytes、起止哈希一致（两轮旧负例失败记录保留）；新 RT 生命周期14×10通过、growth0bytes。B480诊断开关/compute表ID构建验证中，ARC 提前释放、GUI/真实 UE RT 待验，两项能力仍 false，持续目标 active，未提交/推送。

2026-10-05 当前 [PHASE57](PHASE57.md) / [B478](BATCH478_RAY_ARGUMENTS_AND_DISPATCH.md)：官方 triangle 新捕获和离线像素 oracle、44 事件三轮前后定位通过；只读 pointer 参数描述、Managed packet 和 AS residency 已补齐，backend/bundle 5a5d1683 一致。22 旧 RT 例、独立 pointer fixture、T60/27 坏输入通过，总 2,436 事件选择。procedural 捕获输出仍与原生一致，离线卡在 boxes/TLAS 初态；B479 开发中。全量/GUI/UE RT/生命周期未验，两项能力仍 false，持续目标 active，未提交/推送。

2026-10-05 当前 [PHASE57](PHASE57.md) / [B477](BATCH477_RAY_GPU_INPUT_SNAPSHOTS.md)：官方两种光追场景捕获通过，与 native 输出逐字节一致；新增 GPU 输入快照、实例 flags/mask/IFT offset 和 Managed 初态 seek 恢复。backend 582c068b 下 10 新例 + 12 旧例 API/CLI、2,262 事件选择、24 损坏输入及 schema2 兼容通过。离线官方 sample 卡在 pointer argument encoder，B478 正在修复；程序化 boxes 初态、ARC 生命周期、全量/GUI/UE 光追未验。两项能力仍 false，持续目标 active，未提交/推送。

2026-10-05 当前 [PHASE57](PHASE57.md) / [B476](BATCH476_APPLE_RAY_SAMPLE_AND_SIZE_QUERIES.md)：已建立每小时接续的光追持续任务（id metal），规则与启用门槛见 [持续计划](RAYTRACING_ENABLEMENT.md)。接入固定版本 Apple 官方 sample：两种 scene 原生各两次、输出逐字节可重复；补齐 Managed/Private TLAS 尺寸与 heap 查询，5 组/两接口与原生一致。最终 backend/bundle 309d1058 一致；3 个 Shared TLAS 重截/API/CLI（306 事件选择）及12旧例通过。官方 capture-probe 仍失败于 dispatch 缺少有效 TLAS 绑定，离线 sample/UE 光追未验；设备/子资源 ARC 释放顺序断言待修。两项能力仍 false，下一步按 VK/DX12 执行点输入复制方式补 Managed/GPU 输入及真实构建链，保留未提交修改。

2026-10-05 当前 PHASE56：B474–475补齐refittable/refit/普通copy/compact AS初态、nil原地refit、每轮逻辑状态重置、帧内compact尺寸查询及提交前源版本校验。最终backend/bundle b5451e79一致：47新+171旧正例、5,199事件选择、281损坏输入、4语义边界、490生命周期通过；schema1/2/3兼容。受控Shared三角形AS基本生命周期链可用；Private/同CB GPU输入等未闭环，两项光追能力仍false。未跑全量/GUI/真实UE光追，未提交/推送。见 [PHASE56](PHASE56.md)、[B474](BATCH474_RAY_REFIT_AND_COPY_INITIALS.md)、[B475](BATCH475_RAY_COMPACT_INITIALS.md)。

2026-10-05 当前 PHASE55：B471–473 补齐帧前非索引/索引 BLAS、TLAS 初态；提交时冻结输入，按依赖重建并在 seek 重置恢复。最终backend/bundle 8594954e 一致：16新+30旧正例、1,785次事件选择、177损坏输入及3项语义边界、180生命周期（增长360,448 bytes）通过。最终定向结果见 [B473](BATCH473_RAY_INDEXED_INITIALS.md)，范围和接续优先级见 [PHASE55](PHASE55.md)。设备光追能力仍 false；未跑全量/GUI/真实 UE 光追。保留工作区修改，未提交/推送。

# Metal Replay 测试矩阵

2026-10-05 B469–B470：按用户Vulkan/DX12对照准则补齐AS marker三接口、AS fence两接口、IFT单槽/range清空恢复，修复visible/IFT未知非空handle变nil漏洞；使用公共事件树及既有fence epoch模型。最终backend/bundle ae2a2895一致：两种native/capture GPU ray0/1/0/1、54/65事件各3方向（357选择）、112异常拒绝、30旧帧API/CLI、两新帧各3CLI循环、40生命周期（growth720896B）通过。帧前build变体native/capture正确但离线明确拒绝AS绑定，证实AS初态恢复仍是通用光追阻塞；两项raytracing查询仍false。未跑全量/GUI/真实UE光追验收；未提交/推送。见[PHASE54](PHASE54.md)、[B469](BATCH469_RAY_MARKERS_AND_TABLE_CLEAR.md)、[B470](BATCH470_RAY_AS_FENCES_AND_BASELINE.md)。

2026-10-01 BATCH332–333：六种typed tiny各两次capture，GPU字节/像素/seek、
56组API+CLI负例通过；slot六条generation/payload顺序、四项非法annotation拒绝、
metadata gate API+CLI拒绝通过。独立MetalRHI编译/98导出、NullRHI exit0、
正常Metal NewMap启动通过；完整UE replay未验，全量未跑，新增Viewer未验。
当前库1ee8ba5e…，日志metal-descriptors.h6ILpf。见[BATCH333](BATCH333_UE_PARTIAL_PROVIDER.md)。

2026-10-01 BATCH331：五类typed小例各两次capture的GPU字节/像素/seek通过，
Shared/Private frame placement、Private buffer texture view VA/ID重定位与帧首
reset新增通过；48API+CLI负例、旧六帧终端通过。最终库/app5fab70bf…Private
view人工89/167/89通过；全量未跑，完整UE仍GPU前拒绝。见[BATCH331](BATCH331_FRAME_DESCRIPTOR_RESOURCES.md)。

2026-09-30 BATCH330：pre-existing资源首次getter在帧内的真实重复metadata
捕获/replay通过；冲突身份、冲突sampler别名拒绝，等价别名正例通过，合计15
负例API+CLI。两类小帧最终库人工Viewer输出/seek通过；UE仍GPU前拒绝，
全量未跑。见[BATCH330](BATCH330_DUPLICATE_IDENTITIES_AND_VIEWER.md)。

2026-09-30 BATCH328–329：显式Shared typed表的真实GPU relocation、offset/常量
保留、CPU partial diff、GPU copy+显式CPU完整entry、12/15次seek与两次capture
通过；13API+CLI负例、旧六帧API+CLI通过。六次drawable present/引用释放后
thumbnail保存通过；UE新捕获end=1/primary布局导出通过。**完整UE预期拒绝；
全量未跑；人工UI锁屏未验**。见[BATCH328](BATCH328_EXPLICIT_DESCRIPTOR_REPLAY.md)
和[BATCH329](BATCH329_UE_DESCRIPTOR_LAYOUTS_AND_DRAWABLE_LIFETIME.md)。

2026-09-30 BATCH327：查询过GPU身份的live间接资源保守capture引用、缓存
getter依赖保留、已销毁对象排除，三变体×两次capture与CPU/API+CLI拒绝通过；
旧六帧终端通过。两个独立原生进程的typed packet重编码（buffer+4 offset、
texture、sampler、常量保留）得到122。**重定位只到原生算法验证；没有UE
replay/GPU descriptor更新支持或人工UI验收。** 见
[BATCH327](BATCH327_GPU_IDENTITY_RESOURCE_CLOSURE.md)。

2026-09-30 BATCH326增量：buffer gpuAddress、texture/sampler gpuResourceID的
原生返回值可捕获为身份诊断metadata；pre-frame/active query、跨两次capture
持久性和CPU导出通过；含该族的capture在GPU前拒绝。**只支持诊断，未支持
Shader Converter descriptor重定位或任意raw GPU地址replay。** 旧六帧API/CLI
与20次lifecycle通过，具体清单见 [BATCH326](BATCH326_GPU_IDENTITY_DIAGNOSTICS.md)。

## 优先级

- P0：首条可用 replay 链路必须覆盖。
- P1：首版“常见图形接口”承诺的一部分。
- P2：首版后扩展；不能阻塞 P0/P1 交付。
- Out：当前阶段明确不做。

## 功能覆盖矩阵

| 类别 | P0 | P1 | P2 / Out |
| --- | --- | --- | --- |
| Device/Queue | default device、command queue、command buffer、commit/wait | 多 command buffer、debug label/group | 多 GPU、任意应用注入 |
| Buffer | length/bytes 创建、shared/private、初始内容、vertex/index/uniform | offset 更新、blit copy/fill、多 buffer | sparse/IO buffer |
| Texture | 2D、mip、sampled、render target、RGBA/BGRA/depth | array、cube、compressed、MSAA resolve、views | sparse texture（P2） |
| Render pass | clear/load/store、单 color、depth | MRT、stencil、resolve | tile/imageblock 高级路径（P2） |
| Pipeline | vertex/fragment、vertex descriptor、blend/depth/raster | function constants、多个 attachment | dynamic library/function stitching（P2） |
| Binding | vertex/fragment buffer、texture、sampler | 批量绑定、argument buffer 只读解析 | function table/ray resources（Out） |
| Draw | primitives、indexed | instanced、base vertex/base instance、常见 primitive | indirect/ICB（P2） |
| Compute | 不阻塞第一条三角形链路 | pipeline、buffer/texture binding、dispatch | advanced synchronization（P2） |
| Blit | texture/buffer copy | fill、mipmap、常见同步 | sparse mapping（Out） |
| UI | Event、Texture、Buffer、Pipeline、Shader、Mesh | resource links、markers、错误提示 | shader debug、pixel history（Out） |

## 必备测试场景

每个场景应当有源程序、固定输入、capture 生成说明、预期事件树和验证数据。测试程序优先保持小而
独立，不依赖大型引擎。

每个场景分别跟踪五个状态：Native、Capture、RDC inspect、Replay、UI。禁止仅因为生成了文件就把
场景标记为完成。

| ID | 场景 | 主要覆盖 | 优先级 | 状态 |
| --- | --- | --- | --- | --- |
| T00 | 空帧 + present | 文件加载、device/queue、输出窗口 | P0 | Native/Capture/RDC inspect/CLI Replay/output 像素/lifecycle/UI 重开验证通过 |
| T01 | 彩色三角形 | render pass、pipeline、MSL、非索引 draw | P0 | Native/Capture/RDC inspect/CLI Replay/Texture UI/resize/event seek/capture texture readback/像素拾取/DDS 保存/Shader UI/最小 Pipeline State UI/lifecycle/UI 重开验证通过 |
| T02 | 索引立方体 | vertex/index buffer、depth、Mesh Viewer | P0 | Native/Capture/RDC inspect/CLI Replay/output 像素/index/vertex 数据/lifecycle/Event + Texture + Pipeline + Buffer + Mesh UI 全部通过 |
| T03 | 纹理四边形 | texture upload、sampler、fragment binding | P0 | Native/Capture/RDC inspect/CLI Replay/output 像素/texture data+pick/generic binding/shader reflection/used-unused 过滤/lifecycle/Pipeline+Texture+Resource UI 验证通过 |
| T04 | 动态 uniform | buffer offset/更新、多 draw | P1 | Native/Capture/RDC inspect/CLI Replay/event seek/buffer data/constant-block reflection+descriptor/Pipeline+Buffer UI/lifecycle 验证通过 |
| T05 | 实例化网格 | instance layout、base instance | P1 | Native/Capture/RDC inspect/CLI Replay/action+buffer+VS input+mesh+pixel 自动断言/lifecycle/Pipeline+Mesh+Buffer UI 验证通过 |
| T06 | MRT + blending | 多 attachment、blend state | P1 | Native/Capture/RDC inspect/CLI Replay/双 attachment readback+event seek/action outputs/blend state/lifecycle/Texture+Pipeline OM+export UI 验证通过 |
| T07 | depth/stencil | depth/stencil state 和 attachment | P1 | Native/Capture/RDC inspect/CLI Replay/五 draw event seek/depth output/front-back stencil state/lifecycle/Texture+Pipeline OM+export UI 验证通过 |
| T08 | MSAA resolve | multisample texture、resolve | P1 | Native/Capture/RDC inspect/CLI Replay/三 draw resolve event seek/sample+resolve state/lifecycle/Texture+Pipeline OM+export UI 验证通过 |
| T09 | mip/cube/array | 子资源枚举和查看 | P1 | Native/Capture/RDC inspect/CLI Replay/12 子资源 readback+display/pick/DDS 保存/lifecycle/Pipeline+Texture UI 验证通过 |
| T10 | buffer/texture blit | copy/fill/mipmap | P1 | Native/Capture/RDC inspect/CLI Replay/buffer+texture+mip readback/event seek+usage/DDS 保存/lifecycle/Event+Resource+Buffer+Texture UI 验证通过 |
| T11 | compute texture filter | compute pipeline、dispatch、读写纹理 | P1 | Native/Capture/RDC inspect/CLI Replay/dispatch 前后 readback+event seek+read-write descriptor/DDS 保存/lifecycle/最新 Event+CS Pipeline+Resource+Texture UI 验证通过 |
| T12 | argument buffer | 资源引用解析 | P2 | Native/Capture/RDC inspect/CLI Replay/argument resource+descriptor+reflection/event seek/readback/DDS/lifecycle/最新 Event+FS Pipeline+Buffer+Texture+Resource UI 验证通过 |
| T13 | indirect draw | 单次间接 draw 参数 | P2 | Native/Capture/RDC inspect/CLI Replay/Indirect action+usage/16-byte Buffer Viewer/seek/raw save/14×10 lifecycle/最新 Event+API+IA Pipeline+Resource UI 通过；ICB 留待独立 fixture |
| T14 | indexed instancing | 非零 index offset/base vertex/base instance | P1 | Native/Capture/RDC inspect/CLI Replay/action+usage/clear-draw seek/6-byte index range+save/Mesh instance 0/1/15×10 lifecycle/最新 Event+API+IA Pipeline+Resource+Buffer+Mesh UI 通过 |
| T15 | point/line topology | Point/Line/Line Strip 与 VS Input | P1 | Native/Capture/RDC inspect/CLI Replay/action+topology/vertex offset/seek/Mesh/Buffer/Resource/DDS/16×10 lifecycle/最新 qrenderdoc L4 均通过 |
| T16 | vertex texture/sampler binding | vertex stage texture/sampler 与 VS Pipeline/descriptor | P1 | Native/Capture/RDC inspect/CLI Replay/VS reflection+descriptor/usage/seek/Texture+Mesh+Buffer+Resource/DDS/HTML/17×10 lifecycle/最新 qrenderdoc L4 均通过 |
| T17 | texture/sampler batch binding | vertex/fragment range、空槽与 used/unused | P1 | Native/Capture/XML/Replay、4 份异常 RDC、descriptor/usage/seek、DDS/HTML、18×10 lifecycle 与最新 qrenderdoc L4 通过 |
| T18 | fragment storage buffer | 非零 slot、结构化只读 buffer、PS Resource | P1 | Native/Capture/XML/Replay、descriptor/usage/seek、raw/CSV/HTML、19×10 lifecycle 与最新 qrenderdoc L4 通过 |
| T19 | vertex storage buffer | 非零 slot、只读 buffer、VS Resource | P1 | Native/Capture/XML/Replay、descriptor/usage/seek、raw/CSV/HTML、本场景 lifecycle 与最新 qrenderdoc L4 通过；T18/T16/T02/T05 定向通过，L3 未触发 |
| T20 | indirect command buffer | 单命令 ICB、execute range、资源与 action | P2 | Native/Capture/XML/Replay/readback/action/state、6 类异常 RDC、联合 T01/T02/T05/T13/T14 定向、CLI/8×10 lifecycle 与最新 qrenderdoc Event/API/IA/Mesh/Buffer/Resource/HTML/CSV L4 通过；L3 未触发 |
| T21 | indexed indirect draw | 间接五字段、非零 index/参数 offset、IA/Mesh | P2 | Native/Capture/XML/Replay/readback/action/state、9 类异常 RDC、联合清单/CLI/8×10 lifecycle 与最新 qrenderdoc Event/API/IA/Mesh/Buffer/Resource/五字段 CSV/HTML L4 通过；T13 四字段 UI 复验通过，L3 未触发 |
| T22 | 多命令 ICB | 非零 execute range、多个展开 draw 与逐命令 state | P2 | Native/Capture/XML/Replay API/CLI、三包原始字节、逐事件 seek、11 类异常拒绝、联合 9×10 lifecycle 与最新 qrenderdoc Event/API/IA/Mesh/Buffer/Resource/HTML/CSV L4 通过；L3 未触发 |
| T23 | indexed ICB | ICB indexed command、index offset/base vertex/instance | P2 | Native/Capture/XML/Replay API/CLI、6-byte index raw、两实例 Mesh、18 类异常拒绝、联合 9×10 lifecycle 与最新 qrenderdoc Event/API/IA/Mesh/Buffer/Resource/HTML/CSV/DDS L4 通过；L3 未触发 |
| T24 | ICB reset 后重编码 | `resetWithRange` 失效旧命令、替换与邻居保留 | P2 | Native/Capture/XML/Replay API/CLI、4 包原始字节、逐事件 seek、旧资源无 draw usage、8 类异常拒绝、联合 11×10 lifecycle 与最新 qrenderdoc Event/API/IA/Mesh/Buffer/Resource/HTML/CSV L4 通过；L3 未触发 |
| T25 | 混合命令 ICB | 同一 descriptor/range 的非索引与 indexed draw | P2 | Native/Capture/XML/Replay API/CLI、252-byte 原始资源、逐事件 seek、15 类异常拒绝、联合清单/lifecycle 与最新 qrenderdoc 两类 action、IA/Mesh/Buffer/Resource、HTML/CSV/DDS L4 通过；L3 未触发 |
| T26 | ICB inherit pipeline | execute 时继承 render encoder pipeline | P2 | Native/Capture/XML/Replay API/CLI、24-byte 原始顶点、逐事件 state/seek、异常拒绝、联合 11×10 lifecycle 与同轮最新 qrenderdoc Pipeline 17/18、IA/Mesh/Buffer/Resource、HTML/CSV/DDS L4 通过；L3 未触发 |
| T27 | ICB inherit buffers | execute 时继承 vertex buffer 与动态 offset | P2 | Native/Capture/XML/Replay API/CLI、2×104-byte 原始 packet、逐事件 state/seek、异常拒绝、联合 11×10 lifecycle 与同轮最新 qrenderdoc Buffer 16/17 offset 16、IA/Mesh/Resource、HTML/CSV/DDS L4 通过；L3 未触发 |
| T28 | compute dispatchThreads | thread-level grid、非整除边界与 action | P2 | Native/Capture/XML/Replay API action/state/usage/readback/seek、10×7 局部更新/透明边界、DDS、异常拒绝、联合 CLI/lifecycle 与 GUI L4 通过；右侧缩略图已验 |
| T29 | compute buffer binding | 非零 offset 的 compute buffer 输入/输出 | P2 | Native/Capture/XML/Replay API buffer slot 2/4、offset 32/64、272-byte descriptor、原始字节/像素/seek、异常拒绝、联合 CLI/lifecycle 与 GUI L4 通过；`$action()` 和右侧缩略图已验 |
| T30 | compute sampler 直接绑定 | CS sampler slot、采样过滤/寻址、状态与输出 | P2 | 自动与 GUI L4 通过；公共事件树 EID 4/13 顶层及 `$action()` 最短复验已确认；阶段已关闭 |
| T31 | compute 批量资源绑定 | 非零 range、空槽/覆盖、texture/sampler/buffer 批量入口 | P2 | 自动与 GUI L4 通过；EID 7 分组、空槽、筛选、状态栏已验，GUI 导出入口由用户本轮免复验且自动 DDS/raw 已核对；阶段已关闭 |
| T32 | CPU 参数 compute indirect dispatch | 非零 offset、间接 threadgroup 数量与资源 usage | P2 | Native/Capture/XML/Replay API、5 类异常拒绝、联合 CLI/lifecycle 与 GUI L4 通过；新版实际执行数量摘要已验，阶段已关闭 |
| T33 | GPU 生成参数 compute indirect dispatch | compute 写入参数、跨 encoder 可见性与间接 dispatch | P2 | Native/Capture/XML/Replay API、6 类异常拒绝、写入前后/dispatch 后 seek、联合 CLI/lifecycle 与 GUI L4 通过；新版实际执行数量摘要已验，阶段已关闭 |
| T34 | render inline bytes 与 buffer batch binding | vertex/fragment bytes、buffer arrays、vertex offset update | P1 | Native/Capture/XML/Replay API、像素/state/usage、10 类异常拒绝、CLI/lifecycle 自动通过；GUI L4 待验 |
| T35 | command 创建变体 | limited queue、descriptor/unretained command buffer、compute dispatch type、waitUntilScheduled | P1 | Native/Capture/XML/Replay API、两 dispatch/output、同步 chunk、4 类异常拒绝、CLI/lifecycle 自动通过；GUI L4 待验 |
| T36 | render 动态状态与 debug markers | viewport/scissor arrays、depth clip/bias、fill、blend、visibility、store/options、barrier、markers、D32S8 API | P1 | Native/Capture/XML/Replay API、像素/counter/全图 depth-stencil、14 类异常拒绝、CLI/lifecycle 自动通过；GUI L4 待验 |
| T37 | pitched blit transfers | buffer↔texture、whole texture、slice/mip range copy、blit markers | P1 | Native/Capture/XML/Replay API、4096-byte padding/subresource/seek、43 类异常拒绝、CLI/lifecycle 自动通过；GUI L4 待验 |
| T38 | sampler LOD clamps | VS/FS/CS single/batch LOD、nil、覆盖、plain恢复、事件descriptor | P1 | Native/Capture/XML/API/CLI/lifecycle和异常输入通过；GUI L4 待验 |
| T39 | Private buffer readback | 同步staging、非对齐/截断、buffer-output compute、资源类型校验 | P1 | Native/Capture/XML/API/CLI、联合72类新异常、private staging生命周期通过；GUI L4 待验 |
| T40 | Compute inline/offset/threadgroup | 三种新入口、inline拷贝、真实buffer重绑/offset、两块共享内存与新encoder重置 | P1 | Validation-layer Native/Capture/XML/API/CLI、41类异常、42×10联合lifecycle通过；GUI L4 待验 |
| T41 | Blit descriptor/optimization | 创建入口、CPU/GPU whole与slice/mip hints、只由hint引用的texture | P1 | Validation-layer Native/Capture/XML/API/CLI、23类异常、旧copy断言与42×10联合lifecycle通过；GUI L4 待验 |
| T42 | Compute资源声明/barrier/marker | single/batch资源、scope/resource依赖、三dispatch、声明-only资源保留 | P1 | Validation-layer Native+Replay/Capture/XML/API/CLI、35类异常、精确buffer/descriptor/usage/seek、44×10联合lifecycle通过；GUI L4 待验 |
| T43 | Render分阶段声明/barrier | 两次VS buffer写入→FS读取、stage参数、RW descriptor/usage、失败pipeline语义 | P1 | Validation-layer Native+Replay/Capture/XML/API/CLI、68类异常、buffer/padding/pixel/seek、44×10联合lifecycle通过；VS Storage Buffers新条目及GUI L4待验 |
| T44 | Fence跨encoder同步 | 创建/包装、Blit/CS/Render update/wait、Untracked buffers依赖、fence复用 | P1 | Native+Replay验证层/Capture/XML/API/CLI、56类异常、精确GPU数据/usage/逐API seek、47×10联合lifecycle通过；GUI L4待验 |
| T45 | atTime present + buffer marker | Present资源登记、time metadata、marker添加/清空/范围、独立注释资源 | P1 | Native+Replay验证层/Capture/XML/API/CLI、21类异常、后台注释压缩与marker-only内容、47×10联合lifecycle通过；GUI L4待验 |
| T46 | minimum-duration present | Present变体、duration metadata，复用fence及marker路径 | P1 | Native+Replay验证层/Capture/XML/API/CLI、10类异常、精确buffer/pixel/seek、47×10联合lifecycle通过；GUI L4待验 |
| T47 | 同步pipeline创建变体 | render/compute options+reflection、compute descriptor、线程组限制、shader父依赖 | P1 | Native+Replay验证层/Capture/XML/API/CLI、84类异常+2合法options变体、六pipeline/532-byte数据/pixel/seek、48×10联合lifecycle通过；GUI L4待验 |
| T48 | 预编译library | file/URL/data/bundle/default、源对象提前释放、嵌入二进制脱离原路径回放 | P1 | Native+Replay验证层/Capture/XML/五payload比对/API/CLI、64类异常、5库7shader/676-byte数据/pixel/seek、49×10联合lifecycle与3份源码新录兼容通过；GUI L4待验 |
| T49 | Command buffer回调 | scheduled/completed、包装身份/闭包生命周期、CPU快照提交时机、shared更新部分replay/回退 | P1 | Native+Replay验证层/Capture/XML/API/CLI、87类异常、八回调/两dispatch/412-byte结果与padding/pixel/seek、50×10联合lifecycle与3份源码新录兼容通过；GUI L4待验 |
| T50 | Texture CPU读取与同步 | 两getBytes、Managed synchronizeTexture、mip/slice/行与图像间距、独立资源保留、CPU派生GPU输入 | P1 | Native+Replay验证层/Capture/XML/API/CLI、176类异常+2合法metadata变体、四CPU读/三同步/68-byte参数回退、51×10联合lifecycle通过；3D仅native/capture读回、不含viewer，GUI L4待验 |
| T51 | 异步Library/PSO创建 | 六completion入口、原生错误/反射、descriptor快照、包装身份/生命周期、五pipeline实际执行 | P1 | Native+Replay验证层/Capture/XML/API/CLI、104类异常+2合法options变体、七回调/三dispatch/两draw/428-byte数据与回退、53×10联合lifecycle及3份源码新录兼容通过；source options限nil，GUI L4待验 |
| T52 | Event同步 | newEvent、signal/wait、双队列三提交、先signal后wait、epoch重建与seek | P1 | Native+Replay验证层/Capture/XML/API/CLI、122类异常+2合法wait变体、两Event/六同步/444-byte结果与padding/逐事件回退、53×10联合lifecycle通过；外部/SharedEvent/future-signal wait未支持，GUI L4待验 |
| T53 | ICB GPU操作与单命令reset | Shared render ICB reset/copy/optimize、空命令、混合indexed状态复制、epoch初值恢复与未知初值拒绝 | P1 | Native+Replay验证层/Capture/XML/API/CLI、194新异常+3合法变体、旧70 ICB异常+2空命令正例、四阶段/9 draw/5 empty/7 GPU操作/资源及回退、54×10联合lifecycle通过；真实帧前GPU写入变体原生正确离线拒绝；GPU生成/Private/compute ICB未支持，GUI L4待验 |
| T54 | indexed短重载与legacy blit | 无base的instanced draw、旧无options双向copy布局 | P1 | Native/Capture/API/CLI、36异常+2合法变体、action/offset/index/pixels/seek、57×10联合lifecycle通过；GUI L4待验 |
| T55 | Function常量/descriptor同步异步 | 四创建入口、index/range/name、copy/reset、快照与callback所有权 | P1 | Native/Capture/API/CLI、74异常+1合法变体、四PSO/548-byte结果/像素/回退、57×10联合lifecycle通过；scalar常量/options0/无archives，GUI L4待验 |
| T56 | Argument texture/sampler数组 | batch range、nil/部分覆盖、带reflection创建、顶层资源数组反射 | P1 | Native/Capture/API/CLI、38异常、两个独立texture/sampler/descriptor/usage/像素/回退、57×10联合lifecycle通过；帧前Shared单packet，buffer/constants/多packet未扩展，GUI L4待验 |
| T57 | Argument buffer成员/constants/多packet | 单个/批量只读pointer、arrayElement、两提交CPU更新、地址重定位与Shared初值/seek | P1 | Native/Capture/API/CLI、161异常、地址清零正例、真实帧内重绑拒绝、四阶段像素/descriptor/成员数据/哨兵/回退、58×10联合lifecycle通过；FS顶层Shared目的buffer，GUI L4待验，详见BATCH57 |
| T58 | 三种纹理view | 同格式2D/2DArray Shared父纹理、mip/slice subset、swizzle、GPU双向别名与seek | P1 | Native/Capture/API/CLI、56异常、9draw逐事件像素与父纹理子资源数据、60×10联合lifecycle通过；GUI L4待验，详见BATCH58 |
| T59 | buffer-backed纹理 | Shared父buffer与2D子texture、offset/row pitch/alignment、CPU更新/GPU写入与seek | P1 | Native/Capture/API/CLI、35异常、3draw逐事件像素、父buffer153字节含padding核对、60×10联合lifecycle通过；GUI L4待验，详见BATCH58 |
| T60 | Device独立ArgumentEncoder | descriptor创建、只读2D纹理/sampler成员、两个packet与绘制 | P1 | 原生/capture/API/CLI、29异常、2draw逐事件成员/像素/资源/回退、61×10联合lifecycle通过；GUI L4待验，详见BATCH60 |
| T61 | no-copy Buffer | 页对齐Shared默认options、原生指针/deallocator、三提交CPU写入 | P1 | 原生/capture/API/CLI、13异常、3draw逐事件4096-byte数据/像素/回退、62×10联合lifecycle通过；抓取回调可延迟，GUI L4待验，详见BATCH61 |

T10 新 marker 另存 `t10_debug_capture.rdc`，仅新入口 GUI 待最短复验，不撤销原 T10 已验状态。

## 外部样例候选

外部样例用于补充验证，不直接替代仓库内的最小确定性测试。引入前必须记录固定 commit、许可证、
所需 SDK 和实际使用的 target。

1. Apple 官方 Metal Sample Code：
   `https://developer.apple.com/metal/sample-code/`
   - 优先场景：Using a Render Pipeline to Render Primitives、Creating and Sampling Textures、
     Customizing Render Pass Setup、Processing a Texture in a Compute Function、MSAA、argument buffer。
   - `LearnMetalCPP.zip` 适合验证 metal-cpp、基础绘制、buffer、texture 和 animation。
2. `metal-by-example/learn-metal-cpp-ios`
   - 地址：`https://github.com/metal-by-example/learn-metal-cpp-ios`
   - 初查 commit：`d966e516631f42bac8febdb44d955a545fac4661`
   - 许可证：Apache-2.0。
   - 主要用于 API 调用序列参考；它以 iOS 为目标，需筛选可移植到 macOS 的核心 renderer。
3. `LeeTeng2001/metal-cpp-cmake`
   - 地址：`https://github.com/LeeTeng2001/metal-cpp-cmake`
   - 初查 commit：`e22adb2d0e5fd8d34a96c9c7c6cb3ef6ec943285`
   - 许可证：Apache-2.0。
   - CMake 结构适合快速构建多个 metal-cpp 基础用例。
4. `dehesa/sample-metal`
   - 地址：`https://github.com/dehesa/sample-metal`
   - 初查 commit：`0003824a52516052f2d28503f576907e03425dd3`
   - 许可证：MIT。
   - 覆盖 Swift/macOS/iOS 的更广 Metal 示例，用于第二轮兼容验证。

外部样例应放在独立的 `tests/metal/external` 或工作区外缓存中，不能无说明地复制进 RenderDoc
源码。Apple 下载样例的再分发条款需要在 vendor 前单独确认。

## 验证方式

- 数据验证：已知 buffer 字节模式、已知纹理颜色/梯度、CPU 参考结果。
- 状态验证：测试程序写出 descriptor 摘要，与 qrenderdoc Pipeline State 对比。
- 图像验证：保存参考输出，允许明确的颜色空间/浮点误差阈值。
- 事件验证：对 action 名称、层级、draw 参数和 event ID 做结构化断言。
- 稳定性验证：重复打开 capture、切换事件和资源、关闭 capture，检查崩溃与资源增长。

当前自动回归还会把 Metal replay output 读回为 640x480 RGB PPM：T00 校验中心 clear 色，T01
校验背景色和中心非背景像素，以防 output window 或 texture display 退化为“只是不崩溃”。T01
还在同一 controller 中连续 10 轮依次选择 clear、draw、clear action，断言中心像素
`#14141a -> 非背景 -> #14141a`，覆盖前进与回退的 event-range replay。

T01 同时直接读取 capture backbuffer 的 400x300 BGRA 原始字节，在 clear EID 校验中心
`1a1414ff`，在 draw EID 校验中心发生变化，并通过 `PickPixel()` 校验背景 RGBA 约为
`(0.08, 0.08, 0.10, 1.0)`。最后经 `SaveTexture()` 写出 400x300 ARGB8888 DDS，覆盖 qrenderdoc
纹理保存所依赖的数据路径。qrenderdoc 实机在 EID 2 对 `(202, 128)` 的右键拾取返回
`(0.61176, 0.34118, 0.32157, 1.00)`，并成功从保存对话框写出同规格 DDS。

T01 的 replay smoke 还断言 shader 资源恰好包含 `vs_main`/Vertex 与 `fs_main`/Fragment，reflection
encoding 为 MSL，`captured.metal` 同时包含真实 vertex/fragment 入口。qrenderdoc 实机从 Resource
Inspector 的 Function 13/14 分别进入 Shader Viewer，两者均显示该 MSL，状态栏为
`No problems detected`。

T01 的 Pipeline State 自动回归会分别选择 clear 与 draw EID，确认 Metal capture 类型和 color
target；在 draw EID 进一步断言 render pipeline 为真实 `PipelineState` resource、vertex/fragment
shader 入口及 reflection、`TriangleList`、slot 0 的 96-byte vertex buffer 和 swapbuffer color target。
qrenderdoc 实机在 EID 2 的 Metal Pipeline State 页面显示 `Pipeline State 15`、`Function 13/14`、
`vs_main/fs_main`、`Triangle List`、`Buffer 16`（offset 0、size 96）和 `Texture 23`（mip/slice 0），
状态栏为 `No problems detected`。

T02 自动回归会核对 stride 28 的 interleaved Float3/Float4 vertex descriptor、`Depth32Float` attachment、
less/write depth state、CCW/back-face raster state、两组 viewport/scissor，以及 36 个 UInt16 和 36 个
UInt32 index draw。Replay smoke 逐字节比较 72/144-byte index buffer，确认两个 action 都带
`Drawcall|Indexed`，逐 draw 断言 vertex attributes/layout、index binding、depth/raster 与左右
viewport/scissor，并检查 clear -> UInt16 draw -> UInt32 draw -> UInt16 draw 回退的像素结果。
同一 smoke 会断言通用 `GetVertexInputs()` 返回 `attr0` Float3/offset 0 与 `attr1` Float4/offset 12，
创建 Mesh replay output，实际执行 Float3 VS Input 的 indexed wireframe 绘制并检查非背景像素。
qrenderdoc 实机选择 EID 3 后，API Inspector 显示
`drawIndexedPrimitives(36, MTLIndexTypeUInt32, Buffer 20)`，Texture Viewer 的 Outputs 同时列出
`FB0 Texture 27` 和 `DS Texture 17`，左右两个彩色立方体正确。Pipeline State 在 EID 2/3 分别显示
`Buffer 19 / 72 / UInt16` 与 `Buffer 20 / 144 / UInt32`、左/右 viewport/scissor，并共同显示
Float3/Float4、stride 28、less/write 和 back/CCW；状态栏为 `No problems detected`。
从 vertex attribute 行激活可进入标准 Mesh Viewer：EID 2 的 VS Input 表格按 UInt16 index 展开
`attr0/attr1`，预览显示立方体线框，VS Output 表明确显示 Metal post-VS unsupported。从 Pipeline
State 激活 Buffer 18 会打开 224-byte、stride 28 的自动格式 Buffer Viewer；Buffer 19/20 分别以
`ushort index`/`uint index` 打开 72/144-byte 子范围并显示相同索引序列。

T03 自动回归会核对 4x4 RGBA8 descriptor、64-byte `replaceRegion` 内容、nearest/clamp sampler、
fragment texture/sampler slot 0、Float2 position/Float2 UV 与 `TriangleStrip` draw。Replay output 在
四个象限分别断言 `ff2010`、`10e030`、`1840ff`、`f0d020`，并通过 `GetTextureData()` 与
`PickPixel()` 精确读取同一组 texel；通用 `PipeState::GetReadOnlyResources(Fragment)` 和
`GetSamplers(Fragment)` 同时核对 Texture 17 与 Sampler 18。qrenderdoc 实机在 EID 2 显示正确四象限，
Pipeline 双击纹理进入标准 Texture Viewer，双击 sampler 进入 Resource Inspector，状态栏无错误。

P4.4 的首个 UI 收敛切片复用标准 `PipelineFlowChart`，把 Metal 状态分到 IA/VS/RS/FS/OM 五个阶段页。
T03 实机验证 IA 的 Float2/Float2 与空 index 提示、FS 的 Function 14/Texture 17/Sampler 18、shader
直接跳转和 OM 空 depth 提示；T02 实机验证 IA 的 UInt16 Buffer 19、RS 的左 viewport/scissor 与
Back/CCW、OM 的 Texture 27/Depth Texture 17/Less/Write。`Show Empty Items` 使用标准红色空槽；
`Show Unused Items` 在 shader binding reflection 到位前保持禁用并显示原因。

P4.4 第二个 UI 收敛切片将所有表切换为标准 `RDTreeWidget`/`RDHeaderView`，并将 Metal ResourceId
接入通用资源上下文、usage、thumbnail/preview 分发。T03 EID 2 的 Texture 17 双击仍打开标准
Texture Viewer；标准 Export 控件实际写出 `/tmp/metal-pipeline-t03.html`，内容核对 IA/VS/RS/FS/OM、
Triangle Strip、Texture 17 和 Sampler 18。macOS CUA 的 secondary-click 对 Event Browser 和资源表
均不产生 Qt context-menu 事件，因此右键菜单本轮以公共接线、构建和资源激活回归为证据，不把自动化
限制误记为功能失败。

P4.4 第三个 UI 收敛切片在创建 render pipeline 时请求 Metal argument reflection。T03 自动回归断言
fragment shader 的 `colourTexture`/`colourSampler` 均为 slot 0、直接 Image/Sampler binding、只读且
active；fixture 同时把 Texture 17/Sampler 18 绑定到 shader 未声明的 slot 1，断言通用 descriptor
查询将 slot 0 映射到 reflection index 0，将 slot 1 标记为 `NoShaderBinding + staticallyUnused`，且
`onlyUsed=true` 只返回 slot 0。最终 qrenderdoc 在 T03 EID 2 的 FS 页默认仅显示 slot 0；勾选
`Show Unused Items` 后 texture/sampler 表各增加真实物理 slot 1，状态栏保持 `No problems detected`。

T04 使用一个 512-byte uniform buffer，在 offset 0/256 保存两组固定 float4 颜色；两条 draw 通过
`setFragmentBufferOffset` 与左右 viewport 形成可独立验证的事件。XML 断言初始字节、slot、offset
和两条 draw；output smoke 断言 buffer 原始数据、`uniforms`/slot 0/16-byte active constant-block
reflection、通用 descriptor、EID 2/3 的 0/256 offset，以及 clear -> 左红 -> 左红右绿 -> 左红图像
往返。最终 qrenderdoc 的 FS Constant Buffers 表分别显示 `Buffer 16 / 0 / 512` 与
`Buffer 16 / 256 / 256`；双击 EID 3 行进入标准 Buffer Viewer 的 offset 256、length 256 子范围。

T05 使用独立的 24-byte position buffer 和 96-byte instance offset/colour buffer，vertex descriptor
分别设置 stride 8/per-vertex 与 stride 24/per-instance。第 0 个 instance 是不会被绘制的 sentinel，
`drawPrimitives(..., instanceCount=3, baseInstance=1)` 应只输出红、绿、蓝三个实例。XML 与 replay
smoke 断言两个 buffer 初始字节、slot/stride/step、`Drawcall|Instanced`、instance count/base instance、
通用 `attr0/attr1/attr2` 映射、clear/draw 往返和三处精确像素。qrenderdoc EID 2 的 IA 显示
`Vertex / 1` 与 `Instance / 1`；Mesh Viewer instance 0/1 分别读取 record 1/2，两个 Pipeline buffer
行都能进入标准自动格式 Buffer Viewer。

T06 使用 BGRA8 drawable 和 shared RGBA8 第二 color attachment。slot 0 开启
`SourceAlpha/OneMinusSourceAlpha` RGB blending，alpha 使用 `One/Zero`；slot 1 禁用 blending 并只写
RGB。两条 draw 分别覆盖全屏与中央三角形，240-byte vertex buffer 为 Float2 position、Float4 color0、
Float4 color1。XML 断言两个 format、blend factors 和 `RGBA/RGB_` write mask；replay smoke 断言 clear/
draw action 的两个 outputs、通用 blend state、buffer 字节，以及两张 texture 在 clear、draw 1、draw 2、
回退 draw 1 的精确 RGBA 值。qrenderdoc EID 3 的 Texture Viewer Outputs 可切换 FB0/FB1，OM 页显示
两张 Color Targets 与两行 Blend State；实际 HTML export 包含相同数据，状态栏无错误。

T07 使用一个 `Depth32Float_Stencil8` combined attachment 和 BGRA8 color target。mask/test 两个
depth-stencil state 覆盖 Always/Less、depth write、front/back Equal compare、read/write masks 与
Keep/Replace/IncSat/DecSat operations；fixture 先发出一次双 reference，再以单 reference 建立左右 mask。
五条 draw 依次形成 stencil mask、左绿、右蓝和一次 depth fail。XML 断言 format、operations/masks、
single/dual reference 和 draw 数量；replay smoke 断言五个 action 的 depth output、各事件 state 与图像
往返，最终左右像素为 `10df30/1840ff`。qrenderdoc EID 6 的 Texture Viewer 显示左绿右蓝；标准 OM
Depth/Stencil 表与实际 HTML export 显示 Texture 20、Less/Write Enabled、Front/Back reference 5、
`000000FF/00000000` masks、Equal 与 `Inc Sat/Dec Sat`，状态栏无错误。

T08 使用 4x BGRA8 `Texture2DMultisample` color attachment，并通过
`StoreActionMultisampleResolve` 显式 resolve 到单采样 drawable。三条 draw 形成左红、右蓝与中央绿色
叠加三角形。XML 断言 texture/pipeline sample count、resolveTexture、storeAction 和 216-byte vertex
buffer；replay smoke 断言 clear、三条 draw、rewind 的 resolve 像素、`Texture2DMS` color target、
单采样 resolve target、sample/alpha-to-coverage state 与 action output。qrenderdoc EID 4 的标准 OM
Multisample/Color/Resolve 分组显示 `4x -> 1x` 关系；从 resolve 行进入 Texture Viewer 后中心拾取为
`(0.06275, 0.87451, 0.18824, 1.00)`，实际 HTML export 与页面一致，状态栏无错误。

T09 使用 3-mip RGBA8 2D、3-slice 2D array 和六面 cube，每个子资源为不同固定色；12 条屏幕色带
定位采样结果。XML 断言所有 `replaceRegion` 的 mip/slice、3 个 fragment texture binding 与 sampler；
replay smoke 逐子资源检查原始字节、cube face pick、越界拒绝、标准输出显示和 cube DDS 导出。
qrenderdoc EID 2 FS 页显示 Texture 17/18/19（2D/2D Array/Cube）和 Sampler 20；Texture Viewer
可切换 mip、slice 与 face。macOS 26 上 Qt 5 combo 弹窗崩溃，子资源框以点击循环/方向键方式选择。

T10 使用 64-byte source/destination buffer、8x8 四象限 RGBA8 source/destination texture 与
4-mip RGBA8 texture；XML 检查 blit encoder、buffer offset/length、fill range/value、texture
区域和 mipgen。Replay smoke 断言 `CopySrc/CopyDst/Clear/GenMips` usage、EID 1-6 action 顺序与
资源链接、EID 2→3→2 buffer 数据往返、texture copy 四象限、mip1/mip3 原始字节、最终四条
色带和 468-byte 全 mip DDS。最新 qrenderdoc 的 Event Browser 显示 `Copy/Clear Pass #1` 和
逐操作事件；API Inspector 可跳到 Buffer 18/Texture 20，标准 Buffer Viewer、Texture Viewer
与 UI DDS 保存均和自动结果一致，状态栏为 `No problems detected`。

T11 使用独立的 8×8 RGBA8 source/destination texture，`filter_main` 以 `2×2×1` threadgroups、
`4×4×1` threads/group 将 source 的 B/R/G 分量写入 destination，再由 render draw 采样。destination
清零记录在 frame stream 内，保证 seek 到 dispatch 前/后/回退时读到全零、CPU 参考 swizzle、全零。
XML 断言 compute pipeline/encoder、两个 texture binding、dispatch 参数和 end；replay smoke 断言
`CS_Resource/CS_RWResource` usage、compute pipeline/shader/entry、标准只读/读写 descriptor、全部 256
字节 texel、最终绘制边界像素和 384-byte DDS。T03/T09/T10 定向 output smoke、完整 T00-T11 回归、
12×10 lifecycle 和逐份 CLI replay 已通过。最新 qrenderdoc EID 2 CS 页显示 `filter_main`、
Texture 19 只读、Texture 20 读写；标准 Texture Viewer 首像素拾取为 `(16,32,24,255)`，
Resource Inspector 与 EID 5 draw 绑定一致。UI/自动 DDS 逐字节相同，CS HTML export 完整，
状态栏为 `No problems detected`。

T12 使用 fragment function 创建 buffer slot 0 的 `MTLArgumentEncoder`，将 4×4 RGBA8 texture 写入
`id(0)`、Point/Clamp sampler 写入 `id(1)`，并把 16-byte argument buffer 绑定到 fragment buffer 0。
XML 断言 encoder 创建、target buffer、texture/sampler 写入、`setFragmentBuffer` 与 `useResource`；
replay smoke 断言 `MetalPipe::ArgumentBuffer`、`arguments.colourTexture`/
`arguments.colourSampler` reflection、通用 texture/sampler descriptor、`PS_Constants`/
`PS_Resource` usage、64-byte texel、四象限输出、clear→draw seek 和 192-byte DDS。T03/T04/T11 定向、
完整 T00-T12、13×10 lifecycle（resident growth 671,744 bytes）与逐份 CLI replay 已通过。最新
qrenderdoc 在 EID 2 验证 `Buffer 20`、`Texture 17`、`Sampler 18` 及三类标准资源跳转；四象限输出
正确，UI/自动 192-byte DDS 的 SHA-256 相同，Pipeline HTML export 包含 `fs_main` 与三项绑定，状态栏
为 `No problems detected`。

T13 使用 offset 16 的 16-byte `MTLDrawPrimitivesIndirectArguments`，四字段固定为 `3/2/1/1`；
capture/replay/Buffer Viewer/Raw `.bin` 和左右色块一致。未对齐 offset 17 与越界 offset 36 的
派生 RDC 均在 indirect chunk 明确失败。完整 T00-T13 回归、14×10 lifecycle（resident growth
1,294,336 bytes）和最新 qrenderdoc 的 Event/API/IA Pipeline/Resource/标准 Buffer Viewer 验收通过；
UI raw `.bin` 与自动产物逐字节一致，状态栏为 `No problems detected`。

T14 使用 UInt16 index bytes `{4,4,0,1,2,4}`、byte offset 4、baseVertex 1、baseInstance 1、
instanceCount 2；越界/错位哨兵令左红右蓝输出与 Mesh VS Input 可以判别三个 offset。自动验证
`Indexed|Instanced` action、Buffer 18 的精确 4/6 子范围、Buffer 16/17 的 Vertex Buffer usage 与
Buffer 18 的 Index Buffer usage、6-byte `{0,1,2}` raw save、clear/draw/回退、两实例 Mesh preview。
派生 RDC 的 byte offset 3/10 分别因未对齐/越界被拒绝。完整 T00-T14 回归、15×10 lifecycle
（resident growth 1,015,808 bytes）及最新 qrenderdoc Event/API/IA/Buffer/Resource/Mesh/Texture
验收通过；UI CSV 为 0/1/2，Pipeline HTML 与状态栏均正确。

T15 使用同一 264-byte interleaved Float2/Float4 buffer 中三段非零起点，Point `(1,1)`、
Line `(3,2)`、Line Strip `(6,3)` 分别输出红点、绿水平线、蓝折线；视口外哨兵隔离范围。
自动 smoke 验证 XML/action topology、VS Input、Buffer 数据、Vertex Buffer usage、三个 Mesh preview、
clear→draw→回退和 480128-byte DDS。非法 primitive 99 与零 vertexCount 的派生 capture 均在
`drawPrimitives` chunk 拒绝；T01/T02/T05/T14 定向以及 T00-T15 全量 L3、16×10 lifecycle
（resident growth 737280 bytes）均通过。最新 qrenderdoc L4 已看到三类 Event/API、拓扑、标准
Buffer/Resource、三个 Mesh VS Input、UI DDS/HTML export 和状态栏均已通过。

T16 的 2×2 RGBA8 texture 在 vertex shader `texture(0)`/`sampler(0)` 采样，24 个顶点构成
红、绿、蓝、黄四象限；未注入 BGRA readback、capture XML、原始 texture/vertex bytes、
VS reflection/descriptor、`VS_Resource` usage、clear→draw→回退与 DDS 通过自动检查。
texture/sampler slot 128 与空资源四份派生 capture 均在相应 chunk 拒绝；T03/T12 定向与
T00-T16 全量 L3、17×10 lifecycle（resident growth 114688 bytes）通过。最新 qrenderdoc L4
已核对 EID 2 Event/API、VS Pipeline、标准 Texture/Buffer/Mesh/Resource 跳转、UI DDS/HTML
及 `No problems detected` 状态栏。

T18 使用 640-byte shared buffer，在 byte offset 256 的 fragment `device const float4 *` 读取
四组确定性颜色，物理 slot 3，未绑定 slot 5。自动 smoke 核对 native BGRA、XML、storage
reflection/descriptor、`PS_Resource`、clear/draw/回退、640-byte raw export、哨兵与异常 offset。
T04/T12/T10 定向、T00-T18 全量 L3、逐份 CLI replay 和 19×10 lifecycle（resident growth
1,556,480 bytes）通过。最新 qrenderdoc L4 核对 EID 2 的 FS Storage Buffers 256+384 范围、
标准 Buffer Viewer、Resource Inspector `FS - Resource`、HTML/CSV 保存和状态栏；UI CSV 前四行
与 raw export 数据一致。

生命周期 smoke 会在同一进程中分别打开/关闭 T00-T18 各 10 次，检查 action、swapbuffer、draw、
texture readback，并首次调用 histogram、pixel history、post-VS、四种 shader debug 和 target/custom
shader build 的 unsupported 路径。完成 P4.4 第三个 UI 切片后的 2026-09-22 完整回归在两轮 warm-up 后
resident growth 为 540,672 字节；完成 T10 后十一份 capture 回归增长为 1,277,952 字节；T11 的
十二份 capture 全量回归增长为 999,424 字节。加入 T02
前的额外 50 轮压力检查增长 1,441,792 字节。qrenderdoc 还实机完成
`T01 -> Close -> T00 -> Close -> T01`，重开后 T01 的 EID 2 图像和 Pipeline State 正确；
History/Debug 按钮明确显示不支持且保持禁用。

## 切片与批次执行顺序

```text
每个 T：Native reference -> Capture -> RDC/chunk inspection -> Replay/readback/state 自动断言
批次末：最终构建 -> agent 完成去重的 L1/L2 + CLI/lifecycle -> 用户按 QA_GUIDE.md
在同一轮 qrenderdoc 完成 L4 并反馈 -> 两阶段关闭
```

T19 使用 768-byte shared buffer，在 byte offset 256 保存 24 个 `float4` 位置；vertex shader 从
slot 4 读取，slot 6 绑定同一 buffer 但静态未使用。自动 smoke 核对四象限、XML、reflection、
descriptor、`VS_Resource`、IA/storage 分类、clear/draw/回退、raw 范围和异常 slot/offset。
T19/T18/T16/T02/T05 定向、CLI replay 与本场景 10 轮 lifecycle 通过；最新 qrenderdoc L4 核对
VS Storage Buffers、used/unused、标准 Buffer/Resource、HTML/CSV/raw 与状态栏。未触发 T00-T19 L3；
最近完整基线仍为 T00-T18。

T00-T29 已完成整条链路；T26/T27 的联合自动与同轮 qrenderdoc 证据集中在
`BATCH27-28.md`。T28/T29 的自动链路、最终联合定向及用户同轮 L4
均已通过，两条及批次关闭；见 `BATCH29-30.md`。每条切片立即验证功能自动链路，
最终构建上由 agent 联合去重测试；用户按 `QA_GUIDE.md` 的一次性验收单，在同一轮
qrenderdoc 验收两份 capture 并反馈后一起关闭。

## T28/T29 自动与验收摘要

T28 用 7×5 `dispatchThreads`、4×3 threadgroup 更新 10×7 RGBA8 texture，右三列与下两行
保持零；T29 用 compute buffer slot 2/4、offset 32/64 读写 64 个 uint，前后哨兵保持
`0xa5`。两项的 action/state/usage/descriptor、逐事件 seek、原始字节、最终像素、DDS/raw
输出均自动断言；10 类异常 RDC 被拒绝。联合 T11/T10/T01/T18/T19/T12/T16/T17
通过，11×10 lifecycle resident growth 1,638,400 bytes。随后缩略图修复追加
T01/T03/T09/T11/T12/T16/T17/T22/T25/T28/T29 Replay API 与 CLI，12×10
lifecycle 通过；GUI L4 见 `QA_BATCH29-30.md`，L3 未触发。

2026-10-05 B493定向验收：全禁用Indirect槽位kind11/schema9，仅接受每packet ASID0/mask0，完整transform/flags/IFT offset/userID冻结。最终backend/bundle 08028e4c7e2048c3ac37d0de00d712de139c3026b4960d8c836d4cc12f3b255f，GUI3ba30e36。五例Shared帧前/帧内、Private/placement帧前、同CB distinct alias擦零：native/capture/API588事件/EID0/CLI及完整72-byte snapshot oracle PASS；4×43+36=208坏输入、20旧检查/T12416反例、6×10生命周期PASS（growth425984bytes/hash一致）。产物captures/metal-ray-b493、build-macos-debug/metal-ray-b493/indirect-manifest.json。官方/UE/68份RT与本库集中接续中；ARC/原Qt crash/GUI/UE RT未闭环，公开能力false，未提交/推送。

B493 08028e4c接续验收：官方两scene/10尺寸查询/41坏sample/6能力查询、68份B482–493 RT7785事件、AS身份旧反例/schema2/旧Indirect与空TLAS反例PASS；固定库集中308/7784/3080 PASS，growth0bytes/exit0/工作和冻结库起止hash一致。产物frozen-validation-08028e4c/full-regression.log与metal-ray-b493/followup-manifest.json。UE session20261005-225938进入截帧后Private/Tracked placement、54实例、background0被bridge主动ForceCrash拒绝；仍未证明RT dispatch/输出/离线。B494已编辑，待串行构建/GPU，不能计此证据；生产能力false。

2026-10-05 B494定向验收：帧内Private/Tracked（独立或placement）间接实例输入按AS encoder消费点冻结，提交完成后保持原API chunk位置/metadata写入；重放typed live child/UserID staging或全禁用Indirect。精确范围别名仅在已验证冻结build、native encoder关闭且无descriptor backing冲突时允许，EID0清空资格。最终backend/bundle 111c4787c2a7fddf59bdd7503e2ea89f8227c23d72962f584942cb7e2e65a14d，GUI3ba30e36。七例native/capture/API954事件/EID0/CLI3 PASS，含同TLAS两build原始userID73/74、两种inactive完整bytes oracle及两份合法CPU别名提前创建控制；5×43+2×37+6=295坏输入/alias反例、20旧检查/T12416及8×10生命周期PASS（growth1081344bytes/起止hash一致）。产物captures/metal-ray-b494、metal-ray-b494/indirect-manifest.json。两次实现/判定FAIL日志保留，不计通过。官方/UE/75份RT和本库集中尚未跑；08028e4c集中308/7784/3080/growth0属于旧库。公开能力false，ARC/原Qt crash/UE RT未闭环，未提交/推送。


2026-10-07 [B544 buffer 实际消费与纹理尺寸查询](BATCH544_API_RT_INTEGRATION.md)：保持同一大批次，新增 AIR 全局 buffer load/store/atomic 依赖报告、实际 thread-grid 与 CBV scalar 的保守地址范围、当前 typed buffer descriptor 查询及尺寸 intrinsic 资源依赖；参照DX12/VK资源+offset/当前descriptor，未加UE/Lumen特判。当前 backend/bundle `ec73a2f1`：两 fresh native/capture 后的当前 API/CLI 通过，Private placement buffer 528uint/12公共buffer查询、五纹理各528texels/144身份/48公共descriptor/72sampler查询，均48事件往返/EID0；真实三消费点各14buffer读，raw另1写，9/11texture和6sampler且无unknown。58坏组116无GPUwait拒绝+3合法控制、10旧capture20API/CLI通过；456日志无严格诊断，28源码/四产品外盘检查点。首次构建/工具编译/80-byte工具重建/证明脚本旧假设FAIL保留并修正，未计PASS。B544/PHASE59和RT启用验收仍开放，两flagsfalse；当前库官方/full/lifecycle/Qt-ARC/UE整帧GPU未跑，不能证明历史崩溃/重启修复。优先完成通用runtime PSO/实际资源依赖/动态Header闭包再固定库集中验收及UE；新产物外盘，内盘约32GiB，无提交推送/重置。

详见同一B544的“实际 buffer 消费与 metadata intrinsic”结果表和外盘 `development/buffer-consumer-current-integration-manifest.json`；保持大批次开发定向检查，最终集中验收尚未完成。


2026-10-07 [B544 typed buffer SRV/UAV 集成](BATCH544_API_RT_INTEGRATION.md)：继续同一大批次，参照DX12 typed buffer/VK texel buffer，支持底层Metal Private buffer texture当前parent/view、非零offset/format/range、完整parent初态及读写依赖；捕获侧parent查询加锁，view写使parent后续scalar证明失效，parent读写/CBV/AS输入别名拒绝，无UE/Lumen特判。当前 backend/bundle `9fefb511`：Private standalone与Tracked placement两fresh Native/capture/API/CLI PASS，各实际texel读/write/width查询、4253/4253/0/0、528uint、48事件往返/EID0、24公共typed buffer descriptor/location检查、完整父buffer padding/只读SRV/六sampler重定位。75坏组150无GPUwait拒绝+3合法控制，另已生存parent读写重叠反例在通用闭包line825拒绝/原始控制通过；10旧capture20与两已有ec73 capture4 API/CLI通过。两实际AIR/三消费点12texture/13buffer读/6sampler无unknown，各42scalar事实；576日志无严格诊断，30源码/四产品检查点与全部新产物外盘。构建/捕获parent映射/工具AIR旧假设FAIL保留，不计PASS。B544/PHASE59未关闭，官方/full/lifecycle/Qt-ARC/UE整帧当前库未验，两生产flagsfalse；下一通用runtime PSO/实际heap依赖和动态Header，固定版本集中回归再UE实际截帧验收。内盘约32GiB，无提交推送/重置。

详细范围、失败、当前hash及逐项结果见B544“通用 typed buffer SRV/UAV 集成”和外盘 `development/texel-buffer-current-integration-manifest.json`；尚未进入固定最终版集中启用验收。


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

2026-10-07 [B544 私有指针与 Native query API 通用整改](GENERIC_API_RECOVERY.md)：固定 backend/bundle `ec370d18`。非逃逸 private pointer slot按CFG所有在前writer保留typed对象/offset，分支/循环与LLVM生命周期独立数值；未初始化、未来writer、未知来源/外部逃逸仍拒绝。三角形实例query按Native返回ABI分类整个数值/向量/矩阵getter族，GPU计算、展示partial，设备pointer-return不借此放行。两fresh Native真query（1→3实例、Header0→64/贡献0→16/输出0→32、两private来源分支）完整输出/48事件/EID0/AS+UAV公共查询通过，缺AS/child/贡献初态仍GPU前拒绝；fresh读改写/local atomic/部分texel/converted RayQuery与29旧capture58API/CLI通过。UE无GPU预检越过3246400/4065；1591744/6399 AS2/unknownAS0/unknownCall0，剩unknownBuffer56，commit4981760仍API4，peak1523056640bytes。不是UE整帧PASS，flagsfalse、目标active；下一补真实动态descriptor/缓冲地址namespace闭包，不扩CPU shader表达式许可。新产物外盘，保留中间FAIL、源码/产品/文档哈希，无提交推送/重置。

| B544 private-pointer / Native-query stable | PASS 独立Native来源/生命周期/值ABI与相关回归；UE仍INCOMPLETE | private-query-stable-native-zero/variation + mutable/partial-texel/RT/related + UE-pre-submit；当前ec370d18；完整范围和保留失败见GENERIC_API_RECOVERY.md最新节。UE driver0不是replay通过。 |

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


## B544 c52ba022 通用零字段/复制预算稳定候选

| 范围 | 实际结果 | 证据（development/） |
|---|---|---|
| retired/native稀疏namespace尺寸/格式/索引变化 | PASS Native/capture/API/CLI、完整输出/事件/EID0，动态展示partial | null-namespace-stable-retired、sparse、GPU-null/manifest.json |
| GPU全零full24复制、背景初态和帧重发布 | PASS，真实三typed-copy/空refmap；缺真实copy/source publication仍拒绝，完整typed publication缺raw initial为合法控制 | null-namespace-stable-GPU-null/manifest.json |
| partial Private/可变/local atomic/partial texel | PASS fresh输出及56/48/48事件；定义区间与CPU数值分离 | null-namespace-stable-partial-active2、mutable、partial-texel/manifest.json |
| unqualified GPU pointer writer | Native/capture PASS；API4/CLI1恢复闭包预期拒绝，无GPUwait，非writer支持 | null-namespace-stable-opaque-null/manifest.json |
| fresh converted query / 29旧captures | PASS 4253/4253/0/0/528uint/48事件及58API/CLI；定向回归 | null-namespace-stable-RT/manifest.json、related/development-regression-manifest.json |
| 坏capture与合法控制 | 六fresh正常场景111坏组222GPU前拒绝；1额外合法恢复控制完整API/CLI；旧numeric未跑不计通过 | 上述六manifest + null-namespace-stable-GPU-checks-manifest.json |
| allocation/统一preflight预算 | PASS CPU边界、overflow、多copy+proof共享aggregate；无GPU | null-namespace-budget-manifest.json |
| 原UE无GPU45s/3GiB预检 | INCOMPLETE API4；退休零字段越过，非零placement候选内容/producer未证明；peak1357774848，无wait/strict | null-namespace-stable-UE-pre-submit/manifest.json、null-namespace-next-producer-audit/ |
| 官方/full IR/RT/lifecycle/Qt-ARC/native flags最终集中验收及UE整帧输出 | 当前候选未运行/未完成，不开启flags | RAYTRACING_ENABLEMENT.md |

预算初次误将单次边界作为累计限额、CPUPublication合法控制oracle错误、旧opaque fixture早拒绝、编译/坏capture生成器FAIL全部保留，详见GENERIC_API_RECOVERY.md。固定dylib/bundle c52ba022e71eb402f6b3e7ebeb165060441019a62d117d3540091b4d2f5c230e，GUI3ba30e36；全部新产物外盘。


## B544 af26d32b GPU定义区间与通用dispatch绑定

| 范围 | 真实结果 | development证据 |
|---|---|---|
| Native GPU未知数值→Private定义区间→真query | PASS cross/slot2/offset64/1KiB与same/slot7/offset65532/64KiB，各Native/capture/API/CLI0、56事件/EID0/4-byte GPUword/完整输出，CPU scalarKnown0 | GPU-defined-stable-producer-cross、producer-same/manifest.json |
| 缺/未来GPU producer及相关来源/AS/Header | GPU前拒绝，cross含完整producer移到消费之后的真实contents缺口定位 | 两producer manifest，cross/late-partial-producer-API.log |
| AIR定义效果与CPU数值分离 | PASS Unknown值精确store可定义字节；unknown/ranged地址和conditional不能保证定义，数值不模拟 | 两producer/CPU.log及同hashfresh gate CPU结果 |
| fresh旧恢复相关路径 | PASS retired/partial-copy/mutable/local-atomic/partial-texel/GPU-null与完整typed无raw initial合法控制；新producer合计七fresh360事件/128坏组256拒绝 | GPU-defined-stable-*及GPU-defined-stable-GPU-checks-manifest.json |
| 未声明GPU pointer writer | Native/capture0；API4/CLI1在真实runtime闭包拒绝，无GPUwait，非支持PASS | GPU-defined-stable-opaque-null/manifest.json |
| fresh converted RayQuery/29旧capture | PASS 4253/4253/0/0、528uint/48事件及58旧API/CLI；非全量最终RT验收 | GPU-defined-stable-RT/manifest.json、related/development-regression-manifest.json |
| UE无GPU45s/3GiB预检 | INCOMPLETE API4，14714候选未证明内容/producer；peak1364377600，无wait/strict，不能当作实际access | GPU-defined-stable-UE-pre-submit/manifest.json |
| 官方/full IR/RT/lifecycle/Qt-ARC/native flags及UE整帧输出/往返/绑定集中验收 | 当前候选未运行/未完成，flagsfalse | RAYTRACING_ENABLEMENT.md |

固定dylib/bundle af26d32b90d2f6c9ed48988186fde8e5d5007d76ae0dd8c828f169e0d9dda191，GUI3ba30e36；外盘checkpoint和失败保留见GENERIC_API_RECOVERY.md。旧c52/a15真实API拒绝与af测试dispatch-count oracle FAIL不计当前PASS。

2026-10-07 [B544 typed namespace 行窗口与真实读取宽度](GENERIC_API_RECOVERY.md)：固定backend/bundle `fb456ca1`。typed地址保存真实API invocation界及ABI行窗口，动态读取不再被不可达冷行内容阻断；全部物理字段仍重定位，窗口内来源/initial/producer/生命周期/提交校验，范围未知仍恢复完整namespace，候选展示partial。缓存区分窗口，只复用已验证读取宽度，不把allocation容量当内容证明；Native volatile内存效果保留，不计算shader数值。三fresh真query变体1/17/257冷行、1KiB→64KiB/offset64→65532、Header/贡献/输出偏移、cross upload→same-submit slot7 GPU producer完整输出/各56事件通过；放宽调度触及冷行、越声明界、2byte上传不能证明4byte读取及缺/未来producer均GPU前拒绝。fresh旧范围相关恢复、converted RayQuery、29旧capture58API/CLI通过，115坏组230拒绝。UE禁GPU预检仍API4于非零候选14714内容/producer，peak1420476416bytes，无wait/strict；不是实际访问证据、不是UE整帧PASS。下一按真实物理backing/输入恢复义务补机制，不扩CPUshader许可或跳过缺行；完整UE/官方/full/最终启用未验，两flagsfalse、目标active。当前证据/失败/源码与产品检查点全外盘，无提交推送重置。


2026-10-07 [B544 placement buffer 物理字节恢复](GENERIC_API_RECOVERY.md)：固定 backend/bundle `c573bda0`。按真实 Native heap/offset/逻辑范围共享已恢复字节，重新创建的合法 buffer alias 不再丢失前一对象的 upload/GPU 定义区间；unknown-source copy 覆盖使物理证明失效，旧初态先统一播种，禁止后来 alias 复活旧内容。CPU 数值和 typed 地址来源仍独立，纹理/AS opaque footprint 不证明 buffer 内容，未来 producer 不能借给早期消费。三 fresh alias query 变体改变 size/heap offset/读取偏移/上传→Native GPU store/绑定槽，完整输出和事件往返通过；fresh 窗口 GPU、retired/部分 texel、converted RayQuery 与29旧 capture 的58 API/CLI通过，121坏组242 GPU前拒绝。UE 同捕获禁GPU预检仍 API4 于候选14714的内容/producer缺口，peak1296728064bytes、无GPUwait/strict，不算整帧PASS。实际Native纹理footprint审计没有重叠，下一补真实 creation input/生产者恢复机制，不跳过候选或伪造零初态。完整 UE/最终官方/full/设备启用未验，两RT flags false、目标 active。产物和失败证据均外盘，无提交推送重置。


2026-10-08 [B544 API 创建初态通用恢复](GENERIC_API_RECOVERY.md)：backend/bundle `568e50eb`。真实 MTLDevice::newBuffer(length) 的零初始化契约、newBuffer(bytes) 的完整原输入在实际出生点发布恢复区间，seek 仍执行原 Native factory；不要求多余 upload，不应用于背景帧初态或 heap buffer，不制造 CPU shader 数值。bytes factory 缺/短 payload 明确拒绝，不能退化为 length factory。三 fresh Private零/Shared零/Shared bytes sample 改变16KiB→64KiB、offset1024→65532、Header/贡献/动态query/输出偏移，完整创建字节、输出与事件/EID0通过；改为实际 heap 输入的 alias/窗口/producer负例与相关恢复、fresh converted RayQuery、29旧capture58 API/CLI通过。九 fresh runtime464事件，167坏组334 GPU前拒绝。修正旧 partial/cold fixture 把设备新建 buffer 当 undefined 的错误语义；旧拒绝记录保留但不继续作为合法 API 的拒绝依据。UE14714仍是heap factory，CPU审计证明该契约不适用；本候选未重跑无变化UE，不继承旧库UE结果，整帧仍未完成。继续真实heap输入恢复，全部最终RT门槛未完成、flags false，较大批次 active；产物/失败/检查点外盘，无提交推送重置。
