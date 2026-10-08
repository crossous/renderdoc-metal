2026-10-08 当前执行策略采用 [复盘](EXECUTION_REVIEW_2026-10-08.md)；当前M1结果只见 [heap backing闭环记录](HEAP_BACKING_RECOVERY_2026-10-08.md)。冲突的历史场景准入、CPU表达式/producer证明义务和旧负例oracle已替代；历史测试不算当前候选验收，最终RT开启门槛保持。

2026-10-07 最新任务整改（覆盖历史场景许可策略）：先完成一组通用资源恢复与重定位机制，再继续 UE/Lumen 新案例。实际修改纹理/frame-view/coverage/runtime AIR preflight，区分 API/设备、恢复缺口、统一预算、历史场景限制与展示分析；coverage 不积累场景等级，Native 动态计算与 partial 展示独立。具体执行规则、逐函数 DX12/Vulkan 对照及剩余边界见 [GENERIC_API_RECOVERY.md](GENERIC_API_RECOVERY.md) 与根 AGENTS.md；光追开启门槛不变。持续任务 metal 已同步。

# Metal 光追通用 API 集成推进规则

2026-10-07，按用户最新要求调整。完整终点继续是官方 sample 与实际 UE 光追 capture/replay/输出/事件/EID0/资源绑定及相关最终回归/原生能力判断通过，然后启用对应能力并复验；没有缩小为某个 fixture 通过。

## 功能批次与验收

以一段底层捕获→重放链为一个集成批次，连续补齐相关资源、绑定、AS、调度和生命周期缺口。开发中仅对新行为/失败做必要定向检查；相关功能集成后，在固定最终库集中跑与变化相关的回归。已通过且代码/库未变化的检查不重复执行。新的失败、后续修改或确有未解决风险时再做必要复验。PHASE/BATCH记录开发中与最终验收的区别，不为每个字段、geometry或小接口单独完成一个批次。

Lumen 是当前实际应用验收场景；实现按照 Metal API 对象、PSO/函数布局、GPU身份、descriptor/root类型、字节范围、producer/consumer、资源生命期和执行事件组织。保留捕获原有 API 顺序与原生调用，不按引擎、shader名、Lumen pass或效果提供条件分支。converted Metal bindless ABI需要的布局/类型事实由已知对象和实际创建/写入/绑定来源记录，适用于使用同一 ABI 的图形应用；禁止猜测原始地址位型或把普通光栅当光追。

继续先查仓库 Vulkan/DX12 的捕获、初态、descriptor/root、AS及间接调度实现，再完成 Metal 对应语义。两者不支持的调试功能保持边界，必要的 Metal独有语义单独实现。Lumen暴露的40/48字节是覆盖实例，完整ABI还包括不同类型根参数、静态sampler表、Private/帧内输入与buffer/texture输出，不能把40/48字节等同于固定数量的纯CBV。

## 外盘产物

外盘 `/Volumes/CauseUseMac/RenderDocMetalArchives` 为测试产物存储根，源码/主构建核心与用户原始工程保留。新批次先运行：

```sh
python3 util/buildscripts/scripts/metal_artifact_storage_macos.py --name metal-ray-b544
```

首次创建外盘目录及 `build-macos-debug/<name>` 链接；已有合法外盘链接复用；已有内盘目录必须先迁移/校验，命令不覆盖。其他外盘可用 `--archive-root` 明确指定。缺外盘时不得默默把大量产物积累到内盘；继续不产生大量文件的开发并提出具体问题。

本次迁移根 `/Volumes/CauseUseMac/RenderDocMetalArchives/20261007-015524`，`migration-manifest.json`及各root inventory记录源/目标、逐文件SHA256、字节数、链接和结果。219个root、240184个文件、18575982436 bytes（约17.3GiB）先复制/校验，再删除本机副本并建立原路径链接；全部链接核验通过。删除58个可重建的官方sample Xcode Index.noindex/ModuleCache.noindex，保留来源、构建方法、原始capture、最终库/helper、manifest和实际失败日志。原始项目captures仍在；清理时backend/bundle未变化，仍为B543的2d7439ae。归档后主build目录约2.3GiB，内盘可用约33GiB（之前14GiB）。活动B544目录链接到外盘，后续开发产物继续写在那里。

`build-macos-debug/metal-ray-b544/artifact-migration.json` 为本机快捷索引。活动批次允许继续写入，历史目录保留时点证据；不能把迁移时的旧源码/库哈希当成新代码验收。仅删除可重建且未使用的缓存/临时文件，不删除无其他已校验副本的capture、原始系统崩溃证据或用户改动。

持续任务 `metal` 保留原时间表和完整目标，已写入大批次、通用API实现和外盘存储规则。

## 2026-10-07 用户执行准则：重放恢复与访问展示分离（优先于历史 guard 策略）

不能把不断扩充 CPU 上的 shader 表达式分析当作通用 API 适配的全部。按仓库 DX12/Vulkan 的实现，分别推进以下四层：

| 层次 | 应完成的机制 | 是否可以阻止重放 |
|---|---|---|
| 资源恢复 | 捕获真实初态/创建数据；恢复 AS recipe、Private/GPU 输入、帧内创建及生命周期；按原生 API 重做生产者 | 缺资源、失效/遗漏初态或 AS 恢复失败必须修复并拒绝错误重放 |
| 地址重定位 | 通过已知对象和真实 ABI/typed factory 来源，重建 root、descriptor、AS Header、GPU 生成指针及复制的地址字段 | 未恢复的 GPU 指针、错误来源/类型/范围必须修复；禁止猜测地址位型或简单忽略错误 |
| 提交依赖 | 保留原生队列、encoder、生产者→消费者和物理别名顺序，正确恢复事件前后及 EID0 | 缺生产者、未提交依赖或生命周期错误必须修复 |
| shader 访问展示 | 静态访问和可获得的动态反馈只用于报告真实/部分访问；声明的绑定/驻留资源不是实际访问列表 | 动态索引/条件/循环/数值表达式没有被 CPU 分析完整，不能仅因此永久拒绝已可正确重放的 API 调用 |

资源恢复证明与访问展示结果必须使用独立状态，不能继续用同一个 `UniformResourceAccess` 报告同时决定“能否重放”和“能否精确展示”。CPU 分析可以作为可选诊断/已知访问证据，不是必须执行所有 shader 表达式的 CPU 模拟器。不存在完整展示证据时，明确报告部分/未知展示，不把整个 heap 伪装成 shader 实际访问。GPU 生产者仍由真实 Native shader/API 重做；未恢复的地址字段要补真实重定位机制，不能用旧初态或 CPU 算出的输出替代。

本地参照：`renderdoc/driver/d3d12/d3d12_command_list_wrap.cpp::Serialise_Dispatch`、`renderdoc/driver/vulkan/wrappers/vk_draw_funcs.cpp::Serialise_vkCmdDispatch` 重放原生调度；两者 `GetDescriptorAccess` 使用静态访问与可用的 shader feedback 展示。DX12 在 `d3d12_replay.cpp:1995` 仅追加 valid 的动态反馈，不把 CPU 数值分析作为普通 Dispatch 的必要条件；Vulkan 对应 `vk_replay.cpp:2990` 和 `vk_shader_feedback.cpp::FetchShaderFeedback`。资源、地址及 descriptor 支持范围继续优先参考 VK/DX12；Metal 独有且必要的地址语义才补机制。RT 保持黑盒，Lumen 只作实际应用验收。

历史文档中“unknownBuffer/unknownCall 一律拒绝”及只以扩充表达式语法解决所有新 feature 的下一步，均被本准则取代。历史失败/通过日志仍保留，不更改已经发生的结果。每个新拒绝必须区分资源恢复、地址重定位、提交依赖、非法 API/capture 和纯展示不足，记录具体依据；纯展示不足不作为新支持范围的永久否决条件。坏 capture 验收继续覆盖缺失资源/来源、错误身份和地址字段、生命周期与提交破坏；只修改普通 shader 索引/数值的旧“拒绝 oracle”要审查其真实语义，不能为了维持旧预期重新绑定展示与重放。

下一项仍为同一 B544/PHASE59 大批次：先拆 `metal_descriptor_tables.cpp::validateRuntimeDispatch` 的恢复/重定位/提交资格与 `metal_shader_inspect.cpp`/`GetDescriptorAccess` 展示路径。用 Native 正确但静态分析不完整的 GPU 索引/分支 sample 验证“重放通过、展示部分”，再验证未恢复 GPU 指针/缺生产者等确实失败的控制。恢复机制完成并独立验证后，移除仅由展示不足造成的 hard gate；不能直接把所有 unknown 放行。随后回当前 UE 实际 capture，检查整体输出、真正 RT 调度、事件、EID0 和绑定，再集中验收生产能力开关。

2026-10-07 实现进展：B544 已验证首段“已知对象/恢复指针 + 动态偏移或分支候选范围 → Native 重放通过、展示部分”。下一处理独立 pointer provenance/namespace资格；当前 UE pointer select 丢失身份不能自动归类成实际地址缺失，不以继续扩充 CPU 分支/数值表达式作为主要修复。完整证据见 BATCH544 当前 continuation。

2026-10-07 B544当前进展：指针分支、结构投影与Native原子/调用资源效果保留已有恢复来源，数值展示不完整可Native重放/展示部分；嵌入原捕获VA独立拒绝并要求实际重定位。真实UE预检已越过旧首commit557952与六MRT；下一核实draw3330304的fragment阶段可选性/身份，不继续按数值表达式或引擎名字逐个准入。scope、失败和当前2133a76f验证见BATCH544；完整UE/最终能力开关仍未验，目标active。

2026-10-07 B544推进到f9718c71：可选Native阶段与barrier/fence资源效果独立，空shader阶段不产生访问列表，GPU共享内容保持未知但原生执行。UE越过fragment旧guard，下一1624的known-buffer/null选择后动态GEP来源资格；当前四unknown不证明实际地址恢复失败，先核实来源/字段语义，不将模拟共享数组或数值表达式作为主修复。当前scope/FAIL/未验在BATCH544，任务active、生产flagsfalse。

2026-10-07 B544当前529d0a7b：分支来源/相对索引、固定shader地址重定位与对象metadata/像素初态独立；GPU执行原shader，CPU展示partial不永久拒绝。已知predicate也不省略literal捕获VA/span校验，pixel读保留初态/producer。下一实际3836是Native SIMD效果与texture atomic读写/初态/提交闭包，再runtime AS/Header，不模拟lane算法、不只删unknownCall。standalone frame texture仍未实现，不能以placement fixture替代。PASS/FAIL/未跑在BATCH544，目标active/flags false。

2026-10-07 B544当前8d6ec636：Native SIMD/atomic返回保持unknown；恢复source/null phi与动态offset可原GPU重放/展示partial，texture store/load/RMW、初态/producer和提交独立。Native与帧factory共用texture→buffer布局/overflow/生命周期/alias/预算，真实写不恢复CPU旧事实。两同/跨提交fresh全输出/各72事件通过，UE越过3836，下一3853 frame buffer-texture对象/写资格拒绝待诊断，不以扩CPU数值表达式代替恢复。PASS/FAIL/未跑见B544/PHASE59，flagsfalse、目标active，用户四层规则持续有效。

2026-10-07 B544当前4163d391：frame buffer-texture对象/逻辑范围、当前copy字节有效性、像素restoration intervals独立CPU数值展示；root与texel backing合法不重叠可Native重放，缺像素恢复/真实overlap仍GPU前拒绝。UE越过3853，下一3854 background GPU descriptor初态/namespace与Private producer链；已有gpuExpected/opaque历史标记不能直接等于当前恢复失败，不以新增CPU数值表达式代替资源/地址/提交恢复。PASS/FAIL/未跑见B544/PHASE59，flagsfalse/目标active，用户四层准则持续有效。

2026-10-07 B544通用资源恢复首组：已实际移除frame纹理/view组合许可、coverage容量与command/dispatch数量许可，统一布局/投影/预算；Native stride/metadata/真实producer恢复和JIT无AIR展示unknown已由独立sample变体及相关回归验证。后续按通用资源/地址/提交机制继续清理剩余coverage族与未知namespace/call，不按UE feature/pass/name/hash/EID准入；不将supervised预检完成当UE成功。当前UE仍API4无GPU，整帧/最终启用未验，flagsfalse。详见GENERIC_API_RECOVERY/B544最新段与根AGENTS，覆盖历史场景逐项许可策略。

2026-10-07 B544 Native register API effect compatibility与按resource索引的依赖恢复：不计算CPU bit值、不按shader/EID/hash准入；完整prototype只提供资源效果，typed来源/初态/生命周期/提交校验独立。短CPU sample定位全局descriptor逐resource扫描，lower_bound保留本resource全部live字段/来源/依赖。UE同45s/3GiB限制内约25s到真实API4，下一同dispatch scalar事实/读写资源交叠；不拿监督exit0当UE PASS，不直接删除检查或用失效CPU数据选GPU资源。详见B544/PHASE59/GENERIC最新段，flagsfalse/目标active。

2026-10-07 [B544 动态 typed namespace 与部分初态通用整改](GENERIC_API_RECOVERY.md)：固定 backend/bundle `5673f351`。真实 GPU query/metadata 动态选择 24-byte typed table 的缓冲字段时，保留已恢复 namespace/ABI 字段而非要求 CPU 求出行号；按消费点冻结的实际槽位建立恢复候选和提交依赖，候选不冒充实际访问，展示 partial。普通帧出生 Private/Tracked 缓冲的地址重定位不再依赖 CBV 分类；真实部分上传区间独立于 CPU 数值，动态绑定不要求整个 allocation 初始化，缺来源/初态/生产者仍拒绝。两 fresh 部分缓冲 query 变体（实例2选第3缓冲/4-byte 上传、1KiB→64KiB、offset64→65532、Header/贡献/输出偏移）各56事件/EID0/完整输出及真实定义字校验通过；fresh GPU初态+帧重发布、读改写/local atomic/部分texel、converted RayQuery与29旧capture58 API/CLI通过。91坏组182 API/CLI均GPU前拒绝。UE禁GPU预检消除56未知缓冲并越过64KiB/4-byte恢复，下一table25/slot2760已退休纹理的null缓冲字段缺当前namespace证明，仍API4，peak1207173120bytes，无wait/strict；非UE整帧PASS。下一补零字段/退休槽与GPU发布的通用证明，非零失效来源不得跳过；不扩CPU shader表达式许可。官方/full/UE整帧最终验收未跑，两flagsfalse，目标active，失败和外盘检查点保留，无提交推送/重置。

动态 namespace 的 typed 地址恢复与部分初态区间按 [GENERIC_API_RECOVERY.md](GENERIC_API_RECOVERY.md) 实现；当前仍需通用 null/retired sparse字段与真实GPU发布证明，不再逐shader数值扩许可，不将可能依赖或旧initial字节当实际访问/当前GPU指针。

2026-10-07 B544当前c52ba022：sparse/retired/null仅按当前物理地址字段证明恢复，不按logical live行许可；匹配full24 typed GPU copy可以没有引用对象，空ref图不能代替producer校验。完整typed CPU publication可恢复对应字节，不要求冗余raw初态；真正未恢复地址/内容/producer仍拒绝。复制恢复与字段证明计入已有统一frame预算，单次staging边界不能误作整帧累计限额；动态候选不是实际access，未证明shader读过的候选不当作已访问证据。 下一physical backing/定义区间与动态候选恢复资格；原UE预检已越过退休零字段，非零候选初态/producer未证明仍拒绝。具体机制/对照/实际PASS与FAIL见GENERIC_API_RECOVERY.md最新节，不按名称/EID或逐CPU表达式扩许可。

2026-10-07 B544 af26d32b新机制：Native GPU写入的定义区间与CPU知道值独立：完整恢复资格后的精确、必执行非零compute写可发布对应区间/提交依赖，数值保持Unknown，不造padding；conditional/ranged/atomic/raster及未知GPU地址不能据此保证初始化，未来producer不能补早期消费。dispatch使用真实PSO要求的API绑定槽，不强制slot0或2D-copy场景；保留缺绑定/readonly/extent/生命周期/initial/producer/Native设备和RT闭包。 UE未知非零候选不能伪造生产，真实结果/剩余范围见GENERIC_API_RECOVERY.md最新节；继续较大通用能力批次。

2026-10-07 [B544 typed namespace 行窗口与真实读取宽度](GENERIC_API_RECOVERY.md)：固定backend/bundle `fb456ca1`。typed地址保存真实API invocation界及ABI行窗口，动态读取不再被不可达冷行内容阻断；全部物理字段仍重定位，窗口内来源/initial/producer/生命周期/提交校验，范围未知仍恢复完整namespace，候选展示partial。缓存区分窗口，只复用已验证读取宽度，不把allocation容量当内容证明；Native volatile内存效果保留，不计算shader数值。三fresh真query变体1/17/257冷行、1KiB→64KiB/offset64→65532、Header/贡献/输出偏移、cross upload→same-submit slot7 GPU producer完整输出/各56事件通过；放宽调度触及冷行、越声明界、2byte上传不能证明4byte读取及缺/未来producer均GPU前拒绝。fresh旧范围相关恢复、converted RayQuery、29旧capture58API/CLI通过，115坏组230拒绝。UE禁GPU预检仍API4于非零候选14714内容/producer，peak1420476416bytes，无wait/strict；不是实际访问证据、不是UE整帧PASS。下一按真实物理backing/输入恢复义务补机制，不扩CPUshader许可或跳过缺行；完整UE/官方/full/最终启用未验，两flagsfalse、目标active。当前证据/失败/源码与产品检查点全外盘，无提交推送重置。


2026-10-07 [B544 placement buffer 物理字节恢复](GENERIC_API_RECOVERY.md)：固定 backend/bundle `c573bda0`。按真实 Native heap/offset/逻辑范围共享已恢复字节，重新创建的合法 buffer alias 不再丢失前一对象的 upload/GPU 定义区间；unknown-source copy 覆盖使物理证明失效，旧初态先统一播种，禁止后来 alias 复活旧内容。CPU 数值和 typed 地址来源仍独立，纹理/AS opaque footprint 不证明 buffer 内容，未来 producer 不能借给早期消费。三 fresh alias query 变体改变 size/heap offset/读取偏移/上传→Native GPU store/绑定槽，完整输出和事件往返通过；fresh 窗口 GPU、retired/部分 texel、converted RayQuery 与29旧 capture 的58 API/CLI通过，121坏组242 GPU前拒绝。UE 同捕获禁GPU预检仍 API4 于候选14714的内容/producer缺口，peak1296728064bytes、无GPUwait/strict，不算整帧PASS。实际Native纹理footprint审计没有重叠，下一补真实 creation input/生产者恢复机制，不跳过候选或伪造零初态。完整 UE/最终官方/full/设备启用未验，两RT flags false、目标 active。产物和失败证据均外盘，无提交推送重置。


2026-10-08 [B544 API 创建初态通用恢复](GENERIC_API_RECOVERY.md)：backend/bundle `568e50eb`。真实 MTLDevice::newBuffer(length) 的零初始化契约、newBuffer(bytes) 的完整原输入在实际出生点发布恢复区间，seek 仍执行原 Native factory；不要求多余 upload，不应用于背景帧初态或 heap buffer，不制造 CPU shader 数值。bytes factory 缺/短 payload 明确拒绝，不能退化为 length factory。三 fresh Private零/Shared零/Shared bytes sample 改变16KiB→64KiB、offset1024→65532、Header/贡献/动态query/输出偏移，完整创建字节、输出与事件/EID0通过；改为实际 heap 输入的 alias/窗口/producer负例与相关恢复、fresh converted RayQuery、29旧capture58 API/CLI通过。九 fresh runtime464事件，167坏组334 GPU前拒绝。修正旧 partial/cold fixture 把设备新建 buffer 当 undefined 的错误语义；旧拒绝记录保留但不继续作为合法 API 的拒绝依据。UE14714仍是heap factory，CPU审计证明该契约不适用；本候选未重跑无变化UE，不继承旧库UE结果，整帧仍未完成。继续真实heap输入恢复，全部最终RT门槛未完成、flags false，较大批次 active；产物/失败/检查点外盘，无提交推送重置。
